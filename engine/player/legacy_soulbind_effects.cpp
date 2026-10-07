// BracketSim legacy compatibility: Shadowlands soulbind traits, wired up.
// See legacy_soulbind_effects.hpp for why this lives on player_t.

#include "player/legacy_soulbind_effects.hpp"

#include "action/action_state.hpp"
#include "action/dbc_proc_callback.hpp"
#include "action/dot.hpp"
#include "item/special_effect.hpp"
#include "action/spell.hpp"
#include "buff/buff.hpp"
#include "dbc/dbc.hpp"
#include "item/item.hpp"
#include "player/player.hpp"
#include "sim/cooldown.hpp"
#include "sim/event.hpp"
#include "sim/proc_rng.hpp"
#include "sim/sim.hpp"

#include <algorithm>

namespace legacy_soulbind
{
namespace
{
// Indexed by player_e. Every row but warrior is read from that class's own
// /bsdeep on a level 60 character.
const class_values_t CLASS_VALUES[] = {
  //                     combat  lead   kevin  field  waste  measured
  /* NONE        */ { 10.0,  10.0, 20.0, 18.0, 10.0, false },
  /* WARRIOR     */ { 10.0,  20.0, 40.0, 18.0, 10.0, true  },
  /* PALADIN     */ { 10.0,   5.0, 10.0,  9.0, 40.0, true  },
  /* HUNTER      */ { 10.0,   7.5, 15.0, 24.0,  5.0, true  },
  /* ROGUE       */ {  7.5,   5.0, 10.0, 18.0, 15.0, true  },
  /* PRIEST      */ { 30.0,  10.0, 20.0, 18.0,  8.0, true  },
  /* DEATH_KNIGHT*/ { 10.0,  20.0, 40.0,  3.0, 10.0, true  },
  /* SHAMAN      */ { 10.0,   7.5, 15.0, 24.0, 14.0, true  },
  /* MAGE        */ { 10.0,  30.0, 60.0, 12.0, 15.0, true  },
  /* WARLOCK     */ { 10.0,   7.5, 15.0, 12.0, 10.0, true  },
  /* MONK        */ { 20.0,  10.0, 20.0,  6.0, 30.0, true  },
  /* DRUID       */ { 10.0,   4.0,  8.0, 24.0, 30.0, true  },
  /* DEMON_HUNTER*/ { 10.0,  20.0, 40.0, 18.0, 10.0, true  },
  /* EVOKER      */ { 10.0,  10.0, 20.0, 18.0, 10.0, false },
};
// Warrior is measured too, from a second dump: the first returned node names but
// no descriptions because the client's spell cache had not filled before the
// addon's second pass. Its values ride Spear of Bastion (Combat Meditation and
// Effusive Anima), Conqueror's Banner (Lead by Example and Kevin's Oozeling),
// Ancient Aftershock (Field of Blossoms) and Condemn (Wasteland Propriety).
//
// With warrior in, the two structural rules hold twelve for twelve: Kevin's
// Oozeling is exactly twice Lead by Example, and Effusive Anima is CD/15 per
// enemy capped at CD/3.
}  // namespace

const class_values_t& class_values( const player_t* p )
{
  switch ( p ? p->type : PLAYER_NONE )
  {
    case WARRIOR:      return CLASS_VALUES[ 1 ];
    case PALADIN:      return CLASS_VALUES[ 2 ];
    case HUNTER:       return CLASS_VALUES[ 3 ];
    case ROGUE:        return CLASS_VALUES[ 4 ];
    case PRIEST:       return CLASS_VALUES[ 5 ];
    case DEATH_KNIGHT: return CLASS_VALUES[ 6 ];
    case SHAMAN:       return CLASS_VALUES[ 7 ];
    case MAGE:         return CLASS_VALUES[ 8 ];
    case WARLOCK:      return CLASS_VALUES[ 9 ];
    case MONK:         return CLASS_VALUES[ 10 ];
    case DRUID:        return CLASS_VALUES[ 11 ];
    case DEMON_HUNTER: return CLASS_VALUES[ 12 ];
    case EVOKER:       return CLASS_VALUES[ 13 ];
    default:           return CLASS_VALUES[ 0 ];
  }
}

double effusive_anima_cdr_per_enemy( double host_cooldown_seconds )
{
  return host_cooldown_seconds / 15.0;
}

double effusive_anima_cdr_cap( double host_cooldown_seconds )
{
  return host_cooldown_seconds / 3.0;
}

namespace
{
// How many enemies are actually in the fight. Several traits scale off this.
int active_enemies( const player_t* p )
{
  if ( !p || !p->sim )
    return 1;
  int n = static_cast<int>( p->sim->target_non_sleeping_list.size() );
  return std::max( n, 1 );
}

// Armour slots carrying an enchant. Forgeborne Reveries reads this.
int enchanted_armor_pieces( const player_t* p )
{
  int n = 0;
  for ( const auto& i : p->items )
  {
    if ( !i.active() )
      continue;
    if ( i.slot == SLOT_MAIN_HAND || i.slot == SLOT_OFF_HAND )
      continue;
    if ( i.parsed.enchant_id != 0 || !i.parsed.encoded_enchant.empty() )
      n++;
  }
  return n;
}
}  // namespace

namespace
{
/*
 * PUSTULE ERUPTION (7 Oct 2026): "Fleshcraft covers you in 3 pustules per 1 sec channeled. Taking damage or being healed
 * pops a pustule, dealing 274 Nature damage to nearby enemies". Midnight's export dropped 351094, 352086 and 352095;
 * the Shadowlands client data gives Trembling Pustules 9 stacks / 2 min and the pop 8 yd, split between targets, and
 * Shadowlands SimulationCraft's tooltip-read 0.72 x max(spell power, attack power). Shadowlands popped them on a 1 sec
 * timer; here one pops per melee hit actually taken (BracketSim's rule for "when you take damage" effects - a tank
 * is hit by the boss, a damage dealer is not).
 */
struct pustule_eruption_t : public spell_t
{
  buff_t* pustules;

  pustule_eruption_t( player_t* p, buff_t* b ) : spell_t( "pustule_eruption", p ), pustules( b )
  {
    background = may_crit = true;
    school = SCHOOL_NATURE;
    aoe = -1;
    split_aoe_damage = true;
    radius = 8.0;
    spell_power_mod.direct = 0.72;
  }

  double composite_total_spell_power() const override
  {
    return std::max( spell_t::composite_total_spell_power(), spell_t::composite_total_attack_power() );
  }

  void execute() override
  {
    if ( !pustules || !pustules->check() )
      return;
    pustules->decrement();
    spell_t::execute();
  }
};

/*
 * NIYA'S TOOLS: BURRS (7 Oct 2026, a player). "Your damaging attacks and spells have a chance to toss Niya's Spiked Burrs
 * under your target ... inflicting N Nature damage over 6 sec." Every spell is gone from Midnight's export; the numbers are
 * the Shadowlands DBC on this machine (simc-shadowlands SpellDataDump): driver 320659 = 1.75 RPPM on yellow melee, yellow
 * ranged and hostile spells; projectile 321659 = 1 sec delay; DoT 333526 = 6 sec, a tick every 1 sec (hasted), each tick
 * $points = max(SP, AP) x 0.3312 x (1 + versatility). As Shadowlands SimulationCraft did, the DoT lands on the target after
 * the delay - a boss is standing on the burrs.
 */
struct spiked_burrs_t : public spell_t
{
  spiked_burrs_t( player_t* p ) : spell_t( "spiked_burrs", p )
  {
    id = 333526;   // the report's icon and tooltip
    school = SCHOOL_NATURE;
    background = true;
    may_crit = tick_may_crit = true;
    dot_duration = 6_s;
    base_tick_time = 1_s;
    hasted_ticks = true;
    spell_power_mod.tick = 0.3312;
    travel_delay = 1.0;
  }

  double composite_total_spell_power() const override
  {
    return std::max( spell_t::composite_total_spell_power(), spell_t::composite_total_attack_power() );
  }
};

stat_pct_buff_type primary_pct_buff_type( const player_t* p )
{
  switch ( p->convert_hybrid_stat( STAT_STR_AGI_INT ) )
  {
    case STAT_STRENGTH: return STAT_PCT_BUFF_STRENGTH;
    case STAT_AGILITY:  return STAT_PCT_BUFF_AGILITY;
    default:            return STAT_PCT_BUFF_INTELLECT;
  }
}
}  // namespace

void effects_t::create_buffs( player_t* p )
{
  /*
   * NEWFOUND RESOLVE (7 Oct 2026, a player): "your Doubt will manifest ... Directly facing your Doubt will overcome it,
   * granting 10% Intellect and Stamina for 15 sec" - 10% of the PRIMARY stat (352917: Modify Total Stat% 10, $pri). Built
   * for everyone, like volatile_solvent_humanoid, so `buff.newfound_resolve` resolves on any actor; only the trait triggers
   * it (combat_begin).
   */
  newfound_resolve = make_buff( p, "newfound_resolve" )
                         ->set_duration( 15_s )
                         ->set_default_value( 0.10 )
                         ->set_pct_buff_type( primary_pct_buff_type( p ) );

  enabled = !chosen.empty();
  if ( !enabled )
    return;

  if ( has( NEWFOUND_RESOLVE ) )
    newfound_doubt = p->get_shuffled_rng( "newfound_resolve", 1, 30 );

  if ( has( NIYAS_TOOLS_BURRS ) )
  {
    spiked_burrs = new spiked_burrs_t( p );
    auto burrs = new special_effect_t( p );
    burrs->name_str     = "niyas_tools_burrs";
    burrs->type         = SPECIAL_EFFECT_EQUIP;
    burrs->proc_flags_  = PF_MELEE_ABILITY | PF_RANGED_ABILITY | PF_NONE_HARMFUL | PF_MAGIC_SPELL;
    burrs->proc_flags2_ = PF2_ALL_HIT;
    burrs->ppm_         = -1.75;   // real PPM, as 320659 states; no haste scaling listed
    burrs->rppm_scale_  = RPPM_NONE;
    burrs->execute_action = spiked_burrs;
    new dbc_proc_callback_t( p, *burrs );
  }

  if ( has( PUSTULE_ERUPTION ) )
  {
    trembling_pustules = make_buff( p, "trembling_pustules" )->set_max_stack( 9 )->set_duration( 120_s );
    auto pop = new special_effect_t( p );
    pop->name_str     = "pustule_eruption";
    pop->type         = SPECIAL_EFFECT_EQUIP;
    pop->proc_flags_  = PF_MELEE_TAKEN | PF_MELEE_ABILITY_TAKEN;
    pop->proc_flags2_ = PF2_ALL_HIT;
    pop->proc_chance_ = 1.0;
    pop->execute_action = new pustule_eruption_t( p, trembling_pustules );
    new dbc_proc_callback_t( p, *pop );
  }

  /*
   * PARTY FAVORS (7 Oct 2026): Theotar's Mad Duke's Tea, one drunk before the fight - +3% Haste (354016), Critical
   * Strike (354017), primary stat (353266) or Versatility (354018). Shadowlands SimulationCraft's model and option, kept:
   * shadowlands.party_favor_type picks one, "random" (its default) draws one per fight.
   */
  if ( has( PARTY_FAVORS ) )
  {
    auto tea = [ p ]( std::string_view name, unsigned id, stat_pct_buff_type type ) {
      return make_buff( p, name, p->find_spell( id ) )
          ->set_default_value( p->find_spell( id )->effectN( 1 ).percent() )
          ->set_pct_buff_type( type );
    };
    buff_t* haste   = tea( "the_mad_dukes_tea_haste", 354016, STAT_PCT_BUFF_HASTE );
    buff_t* crit    = tea( "the_mad_dukes_tea_crit", 354017, STAT_PCT_BUFF_CRIT );
    buff_t* primary = tea( "the_mad_dukes_tea_primary", 353266, primary_pct_buff_type( p ) );
    buff_t* vers    = tea( "the_mad_dukes_tea_versatility", 354018, STAT_PCT_BUFF_VERSATILITY );
    std::string_view type = p->sim->shadowlands_opts.party_favor_type;
    if ( util::str_compare_ci( type, "haste" ) )
      party_favors[ party_favor_count++ ] = haste;
    else if ( util::str_compare_ci( type, "crit" ) )
      party_favors[ party_favor_count++ ] = crit;
    else if ( util::str_compare_ci( type, "primary" ) )
      party_favors[ party_favor_count++ ] = primary;
    else if ( util::str_compare_ci( type, "versatility" ) )
      party_favors[ party_favor_count++ ] = vers;
    else if ( util::str_compare_ci( type, "random" ) )
    {
      party_favors[ 0 ] = haste; party_favors[ 1 ] = crit; party_favors[ 2 ] = primary; party_favors[ 3 ] = vers;
      party_favor_count = 4;
    }
    else if ( !util::str_compare_ci( type, "none" ) )
      p->sim->error( "Warning: Invalid type '{}' for Party Favors, ignoring.", type );
  }

  // ---------------------------------------------------------------- Kyrian --
  // "Chance to critical strike is increased by 2% for every nearby enemy or
  // ally, up to 6%."
  pointed_courage = make_buff( p, "pointed_courage" )
                        ->set_max_stack( 3 )
                        ->set_duration( timespan_t::zero() )
                        ->set_default_value( 0.02 )
                        ->set_pct_buff_type( STAT_PCT_BUFF_CRIT );

  // "Gain 3% critical strike chance for 10 sec after damaging an enemy above
  // 90% health." On a patchwerk pull that is the opener and nothing after it.
  spear_of_the_archon = make_buff( p, "spear_of_the_archon" )
                            ->set_duration( 10_s )
                            ->set_default_value( 0.03 )
                            ->set_pct_buff_type( STAT_PCT_BUFF_CRIT );

  // "Damaging a new enemy grants you 3% Haste for 10 sec, up to 5 stacks."
  // Every enemy in a fixed target set is new at the pull.
  hammer_of_genesis = make_buff( p, "hammer_of_genesis" )
                          ->set_max_stack( 5 )
                          ->set_duration( 10_s )
                          ->set_default_value( 0.03 )
                          ->set_pct_buff_type( STAT_PCT_BUFF_HASTE );

  // ------------------------------------------------------------- Necrolord --
  // "Your Intellect and Armor are increased by 1% for each enchantment on your
  // armor, up to 3%." Only the Intellect half matters for damage.
  forgeborne_reveries = make_buff( p, "forgeborne_reveries" )
                            ->set_max_stack( 3 )
                            ->set_duration( timespan_t::zero() )
                            ->set_default_value( 0.01 )
                            ->set_pct_buff_type( STAT_PCT_BUFF_INTELLECT );

  // "After landing 10 critical strikes, you gain 18% increased chance to
  // critical strike for 10 sec. May only occur once per 60 sec." Any real
  // rotation clears ten crits well inside the cooldown, so the cooldown is the
  // binding constraint and this is driven off it rather than off a crit count.
  marrowed_gemstone = make_buff( p, "marrowed_gemstone" )
                          ->set_duration( 10_s )
                          ->set_default_value( 0.18 )
                          ->set_pct_buff_type( STAT_PCT_BUFF_CRIT );

  // A buff cannot drive its own repeat - its period is bounded by its duration -
  // so the 60s cadence comes from a separate quiet driver.
  marrowed_gemstone_driver = make_buff( p, "marrowed_gemstone_driver" )
                                 ->set_quiet( true )
                                 ->set_duration( timespan_t::zero() )
                                 ->set_period( 60_s )
                                 ->set_tick_callback( [ this ]( buff_t*, int, timespan_t ) {
                                   marrowed_gemstone->trigger();
                                 } );

  // "Damaging an enemy above 90% health grants you 34 Mastery for 5 sec, up to
  // 5 stacks."
  carvers_eye = make_buff<stat_buff_t>( p, "carvers_eye" )
                    ->add_stat( STAT_MASTERY_RATING, 34.0 )
                    ->set_max_stack( 5 )
                    ->set_duration( 5_s );

  // "Your critical strikes grant you stacks of Valiant Strikes, up to 20", and
  // Light the Path: "Your Valiant Strikes grant 0.25% critical strike chance
  // per stack." Both numbers are from the live client's own descriptions,
  // which is this file's stated source - all four of the spells behind them
  // (329791, 330943, 351491, 352981) are missing from Midnight's export.
  //
  // Valiant Strikes ALONE is pure healing and does nothing here; the crit only
  // exists when Light the Path is taken too, so this is gated on both.
  //
  // It sits at the cap rather than ramping. The stacks are spent when a nearby
  // party member drops below 50% health, and a solo sim has no party - so
  // nothing ever consumes them. That is a fact about the sim rather than a
  // number anyone chose. SimulationCraft made the same thing an OPTION,
  // shadowlands.valiant_strikes_heal_rate, because it cannot be derived.
  if ( has( VALIANT_STRIKES ) && has( LIGHT_THE_PATH ) )
  {
    valiant_strikes = make_buff( p, "legacy_valiant_strikes" )
                          ->set_max_stack( 20 )
                          ->set_duration( timespan_t::zero() )
                          ->set_default_value( 0.0025 )
                          ->set_pct_buff_type( STAT_PCT_BUFF_CRIT );
  }

  // --------------------------------------------------------------- Venthyr --
  // "While you are above 80% health you gain 1% Intellect every 2 sec, stacking
  // up to 5 times."
  // "Gain 3% Haste for 10 sec after defeating an enemy, up to 15%." Five
  // stacks of three. It can only do anything on a fight style where things
  // actually die - on Patchwerk nothing does, and it correctly measures zero.
  gnashing_chompers = make_buff( p, "gnashing_chompers" )
                          ->set_max_stack( 5 )
                          ->set_duration( 10_s )
                          ->set_default_value( 0.03 )
                          ->set_pct_buff_type( STAT_PCT_BUFF_HASTE );

  // "moving within 3 yd of a party or raid member ... increasing Mastery by
  // 10". A flat rating, and a small one: 10 mastery rating is close to nothing
  // at level 60 and does not scale, so this is honest rather than exciting.
  better_together = make_buff<stat_buff_t>( p, "better_together" )
                        ->add_stat( STAT_MASTERY_RATING, 10.0 )
                        ->set_duration( timespan_t::zero() );

  built_for_war = make_buff( p, "built_for_war" )
                      ->set_max_stack( 5 )
                      ->set_duration( timespan_t::zero() )
                      ->set_default_value( 0.01 )
                      ->set_pct_buff_type( STAT_PCT_BUFF_INTELLECT );

  // "While in combat, you gain a stack of Thrill Seeker every 2 sec ... At 40
  // stacks Thrill Seeker is consumed to grant you Euphoria."
  euphoria = make_buff( p, "euphoria" )
                 ->set_duration( 10_s )
                 ->set_default_value( 0.20 )
                 ->set_pct_buff_type( STAT_PCT_BUFF_HASTE );

  // "When the Haste effect of Euphoria ends, gain 10 sec of either 20%
  // increased Critical Strike chance or Versatility, whichever you currently
  // have more of."
  fatal_flaw_crit = make_buff( p, "fatal_flaw_crit" )
                        ->set_duration( 10_s )
                        ->set_default_value( 0.20 )
                        ->set_pct_buff_type( STAT_PCT_BUFF_CRIT );
  fatal_flaw_vers = make_buff( p, "fatal_flaw_vers" )
                        ->set_duration( 10_s )
                        ->set_default_value( 0.20 )
                        ->set_pct_buff_type( STAT_PCT_BUFF_VERSATILITY );

  if ( has( FATAL_FLAW ) )
  {
    euphoria->set_stack_change_callback( [ this, p ]( buff_t*, int, int cur ) {
      if ( cur != 0 )
        return;
      if ( p->cache.attack_crit_chance() >= p->cache.damage_versatility() )
        fatal_flaw_crit->trigger();
      else
        fatal_flaw_vers->trigger();
    } );
  }

  // Thrill Seeker is a pure timer: one stack every 2 seconds, Euphoria at 40.
  // That is 80 seconds exactly, with nothing random in it, so the cadence is
  // driven directly rather than by counting stacks - counting them through a
  // buff's own period double counted and fired Euphoria roughly twice as often
  // as the game does.
  thrill_seeker = make_buff( p, "thrill_seeker" )
                      ->set_quiet( true )
                      ->set_duration( timespan_t::zero() )
                      ->set_period( 80_s )
                      ->set_tick_callback( [ this ]( buff_t*, int, timespan_t ) { euphoria->trigger(); } );

  // ------------------------------------------------------------- Night Fae --
  // "When at least 2 allies are within 8 yd, your Versatility increases by 3%."
  social_butterfly = make_buff( p, "social_butterfly" )
                         ->set_duration( timespan_t::zero() )
                         ->set_default_value( 0.03 )
                         ->set_pct_buff_type( STAT_PCT_BUFF_VERSATILITY );

  // "Damaging an enemy before they damage you increases your chance to critical
  // strike by 25% for 5 sec." That is the pull, once.
  first_strike = make_buff( p, "first_strike" )
                     ->set_duration( 5_s )
                     ->set_default_value( 0.25 )
                     ->set_pct_buff_type( STAT_PCT_BUFF_CRIT );

  // "Redirected Anima increases your maximum health by 1% and your Mastery by 6
  // for 30 sec, and stacks overlap." Gained on a chance from damage, which at a
  // real cast rate settles near the cap, so it is ramped on a period instead of
  // rolled per cast.
  // Bonded Hearts: "If any of the affected allies are of another covenant,
  // Redirected Anima's Mastery and maximum health effects are increased by
  // 50%." A raid has other covenants in it, so the larger value is the one a
  // grouped player gets. The health half is not modelled; this port measures
  // damage.
  const double anima_mastery = has( BONDED_HEARTS ) ? 6.0 * 1.5 : 6.0;

  // BracketSim (27 Sep 2026): stacks last 30 s and overlap (each its own expiry). The old model held 20 stacks all
  // fight; Wowhead 322721: "Approximately 2 procs per minute", 30 sec, and the Night Fae class ability grants 8. At a
  // steady 2 a minute that is ~1 stack, plus 8 for 30 s after each class-ability cast. 60 Blood (Niya): the trait was
  // worth +16.5% under the old model.
  grove_invigoration = make_buff<stat_buff_t>( p, "redirected_anima" )
                           ->add_stat( STAT_MASTERY_RATING, anima_mastery )
                           ->set_max_stack( 20 )
                           ->set_duration( timespan_t::from_seconds( 30 ) )
                           ->set_stack_behavior( buff_stack_behavior::ASYNCHRONOUS );

  // Stacks are gained on a chance from damage and last 30s, so they settle at a
  // ceiling rather than climbing forever. Ramped on a quiet driver to that
  // ceiling and held, which is what a sustained rotation produces.
  // "Your spells and abilities have a chance to call Tubbins and Gubbins to
  // your side for 12 sec, parasol in hand." Their parasol is a flat stat gain
  // while it is up; the call is a proc, so it is driven on its average cadence.
  soothing_shade = make_buff<stat_buff_t>( p, "soothing_shade" )
                       ->add_stat( STAT_MASTERY_RATING, 138.0 )
                       ->set_duration( 12_s );

  soothing_shade_driver = make_buff( p, "soothing_shade_driver" )
                              ->set_quiet( true )
                              ->set_duration( timespan_t::zero() )
                              ->set_period( 30_s )
                              ->set_tick_callback( [ this ]( buff_t*, int, timespan_t ) {
                                soothing_shade->trigger();
                              } );

  // "Damaging or healing a target analyzes its merit over 15 sec, increasing
  // your damage or healing to it by 1%, increasing by an additional 1% every
  // 3 sec." Five steps of 1% over its 15s, then it restarts, so it averages 3%
  // against a target you never stop hitting.
  soulglow_spectrometer = make_buff( p, "soulglow_spectrometer" )
                              ->set_max_stack( 5 )
                              ->set_duration( timespan_t::zero() )
                              ->set_default_value( 0.01 )
                              ->set_period( 3_s )
                              ->set_tick_callback( []( buff_t* b, int, timespan_t ) {
                                if ( b->check() < 5 )
                                  b->trigger();
                              } );

  // "Dealing damage or healing a target grants you 1% increased damage or
  // healing to that target for 4 sec, up to 3%."
  dream_delver = make_buff( p, "dream_delver" )
                     ->set_max_stack( 3 )
                     ->set_duration( timespan_t::zero() )
                     ->set_default_value( 0.01 )
                     ->set_period( 1.5_s )
                     ->set_tick_callback( []( buff_t* b, int, timespan_t ) {
                       if ( b->check() < 3 )
                         b->trigger();
                     } );

  // "The next time you damage an enemy who is below 35% health ... increase
  // your damage and healing to such targets by 5% for 10 sec." Only live in the
  // execute window, and only alongside Wild Hunt Tactics, which is the trait
  // that arms it.
  wild_hunt_stratagem = make_buff( p, "wild_hunt_stratagem" )
                            ->set_duration( timespan_t::zero() )
                            ->set_default_value( 0.05 );

  grove_invigoration_driver = make_buff( p, "redirected_anima_driver" )
                                  ->set_quiet( true )
                                  ->set_duration( timespan_t::zero() )
                                  ->set_period( 30_s )  // ~2 procs per minute (Wowhead), as their average
                                  ->set_tick_callback( [ this ]( buff_t*, int, timespan_t ) {
                                    grove_invigoration->trigger();
                                  } );

  // ------------------------------------------------- The covenant-gated four --
  // These do nothing on their own: each fires from its own covenant's class
  // ability through covenant_ability_cast(). Their DURATIONS are per class and
  // come from the measured table, because each inherits the window of whatever
  // ability it rides - Combat Meditation is 30s on a priest and 10s on a druid.
  const class_values_t& cv = class_values( p );

  // "Boon of the Ascended increases your Mastery by 79.4 for 30.0 sec" on a
  // level 60 - and 53.2 on a level 40, which is what showed this SCALES. See
  // combat_meditation_mastery(). The DURATION is per class and measured, and
  // the author's monk reading of 20.0 sec matches the table exactly.
  combat_meditation = make_buff<stat_buff_t>( p, "combat_meditation" )
                          ->add_stat( STAT_MASTERY_RATING, combat_meditation_mastery( p ) )
                          ->set_duration( timespan_t::from_seconds( cv.combat_meditation ) );

  // "Unholy Nova increases your Intellect by 5% ... You gain 2% additional
  // Intellect for each ally affected", up to four. Percentages, so this one
  // carries no level assumption. The live description renders the caster's own
  // stat by class, so it is the PRIMARY stat rather than Intellect specifically -
  // a warrior reads Strength here.
  {
    stat_e prim = p->convert_hybrid_stat( STAT_STR_AGI_INT );
    stat_pct_buff_type pct = STAT_PCT_BUFF_INTELLECT;
    if ( prim == STAT_STRENGTH )
      pct = STAT_PCT_BUFF_STRENGTH;
    else if ( prim == STAT_AGILITY )
      pct = STAT_PCT_BUFF_AGILITY;

    lead_by_example = make_buff( p, "lead_by_example" )
                          ->set_duration( timespan_t::from_seconds( cv.lead_by_example ) )
                          ->set_default_value( 0.05 + 0.02 * std::min( 4, nearby_allies ) )
                          ->set_pct_buff_type( pct );
  }

  // "Fae Guardians puts flowers at your feet for 18.0 sec that increase your
  // Haste by 15% while you stand with them." A patchwerk actor never moves, so
  // full uptime inside the window is the right model here and would not be on a
  // profile with movement.
  field_of_blossoms = make_buff( p, "field_of_blossoms" )
                          ->set_duration( timespan_t::from_seconds( cv.field_of_blossoms ) )
                          ->set_default_value( 0.15 )
                          ->set_pct_buff_type( STAT_PCT_BUFF_HASTE );

  // "Mindgames signals the start of tea time, granting 6% Versatility to you
  // ... Lasts 8 sec."
  wasteland_propriety = make_buff( p, "wasteland_propriety" )
                            ->set_duration( timespan_t::from_seconds( cv.wasteland_propriety ) )
                            ->set_default_value( 0.06 )
                            ->set_pct_buff_type( STAT_PCT_BUFF_VERSATILITY );

  // "Volatile Solvent: Humanoid - Mastery increased by 30.2" at level 60, and
  // whatever the same coefficient gives at every other bracket. Fleshcraft
  // triggers it; see fleshcraft_t below and the note in the header.
  //
  // Built unconditionally rather than behind has(), because the action list is
  // parsed against buff names and `buff.volatile_solvent_humanoid.down` has to
  // resolve even on an actor that has not taken the trait. It is triggered only
  // where has() is true, so an actor without the trait simply never gains it.
  volatile_solvent_humanoid =
      make_buff<stat_buff_t>( p, "volatile_solvent_humanoid" )
          ->add_stat( STAT_MASTERY_RATING, volatile_solvent_humanoid_rating( p ) )
          ->set_duration( timespan_t::from_seconds( VOLATILE_SOLVENT_DURATION ) );
}

void effects_t::covenant_ability_cast( player_t* p, covenant_e cov, cooldown_t* host_cd )
{
  if ( !enabled || !p )
    return;

  switch ( cov )
  {
    case COVENANT_KYRIAN:
      if ( has( COMBAT_MEDITATION ) )
        combat_meditation->trigger();

      // "Reduce the cooldown of Boon of the Ascended by 12 sec per affected
      // enemy, to a maximum of 60 sec." That is CD/15 per enemy capped at CD/3,
      // a rule confirmed against all twelve classes, so it is derived from the
      // host's own cooldown rather than carried per class.
      if ( has( EFFUSIVE_ANIMA ) && host_cd && host_cd->duration > timespan_t::zero() )
      {
        double host  = host_cd->duration.total_seconds();
        int    hit   = active_enemies( p );
        timespan_t cdr =
            std::min( timespan_t::from_seconds( effusive_anima_cdr_per_enemy( host ) * hit ),
                      timespan_t::from_seconds( effusive_anima_cdr_cap( host ) ) );

        // The cooldown has not started yet inside the caller's execute(), so
        // adjusting it there does nothing at all. Shadowlands hit the same thing
        // with Grounding Surge and solved it the same way: defer to an event.
        make_event( *p->sim, [ host_cd, cdr ] { host_cd->adjust( -cdr ); } );
      }
      break;

    case COVENANT_VENTHYR:
      if ( has( WASTELAND_PROPRIETY ) )
        wasteland_propriety->trigger();
      break;

    case COVENANT_NECROLORD:
      if ( has( LEAD_BY_EXAMPLE ) )
        lead_by_example->trigger();
      break;

    case COVENANT_NIGHT_FAE:
      if ( has( FIELD_OF_BLOSSOMS ) )
        field_of_blossoms->trigger();
      // Grove Invigoration: "Activating your Night Fae class ability grants you (8 * N) stacks of Redirected Anima" -
      // N is per class (Wowhead spell 342814 with each class ticked, 4 Oct 2026, a player report: a DK simmed an
      // average of 17 stacks where the game gives 2 per cast). Every class used to get 8.
      if ( has( GROVE_INVIGORATION ) )
      {
        int stacks = 8;
        switch ( p->type )
        {
          case DEATH_KNIGHT: stacks = 2; break;
          case MONK: stacks = 4; break;
          case MAGE: case PALADIN: stacks = 6; break;
          case WARLOCK: stacks = 8; break;
          case PRIEST: case ROGUE: case WARRIOR: case DEMON_HUNTER: stacks = 12; break;
          case DRUID: case HUNTER: case SHAMAN: case EVOKER: stacks = 16; break;
          default: break;
        }
        grove_invigoration->trigger( stacks );
      }
      break;

    default:
      break;
  }
}

void effects_t::combat_begin( player_t* p )
{
  if ( !enabled )
    return;

  // the author, 18 September 2026: *"id always pre cast fleshcraft before pull"*.
  // Modelled here rather than left to the action list, so the opener is right
  // even on a profile whose list never presses Fleshcraft. Shadowlands did the
  // same - `register_combat_begin( humanoid_buff )` - for the same reason.
  //
  // This is what makes the in-combat refresh a fair test: the buff is up at t=0
  // either way, so any gain measured from pressing Fleshcraft during the fight
  // is the gain from EXTENDING it, not from having it at all.
  if ( has( VOLATILE_SOLVENT ) && volatile_solvent_humanoid )
    volatile_solvent_humanoid->trigger();

  if ( has( POINTED_COURAGE ) )
    pointed_courage->trigger( std::min( 3, active_enemies( p ) + nearby_allies ) );

  if ( has( SPEAR_OF_THE_ARCHON ) )
    spear_of_the_archon->trigger();

  // Party Favors: the tea drunk before the pull (one, drawn per fight when the option is "random").
  if ( party_favor_count )
    party_favors[ party_favor_count == 1 ? 0 : static_cast<unsigned>( p->rng().range( 0, party_favor_count ) ) ]->trigger();

  // Valiant Strikes goes straight to its cap - see create_buffs for why
  // nothing ever consumes the stacks in a solo sim.
  if ( valiant_strikes )
    valiant_strikes->trigger( 20 );

  if ( has( HAMMER_OF_GENESIS ) )
    hammer_of_genesis->trigger( std::min( 5, active_enemies( p ) ) );

  if ( has( FORGEBORNE_REVERIES ) )
  {
    int stacks = std::min( 3, enchanted_armor_pieces( p ) );
    if ( stacks > 0 )
      forgeborne_reveries->trigger( stacks );
  }

  if ( has( MARROWED_GEMSTONE ) )
  {
    marrowed_gemstone->trigger();
    marrowed_gemstone_driver->trigger();
  }

  if ( has( CARVERS_EYE ) )
    carvers_eye->trigger( 5 );

  if ( has( BUILT_FOR_WAR ) )
    built_for_war->trigger( 5 );

  if ( has( THRILL_SEEKER ) )
    thrill_seeker->trigger();

  if ( has( SOCIAL_BUTTERFLY ) && nearby_allies >= 2 )
    social_butterfly->trigger();

  if ( has( FIRST_STRIKE ) )
    first_strike->trigger();

  if ( has( GROVE_INVIGORATION ) )
    grove_invigoration_driver->trigger();

  // Better Together is up for anyone standing near an ally, which in a raid
  // sim is everyone, for the whole fight.
  if ( has( BETTER_TOGETHER ) )
    better_together->trigger();

  // Gnashing Chompers needs something to die, so it fires from the kill
  // callback rather than at the start. On Patchwerk nothing dies and it
  // correctly measures nothing at all.
  if ( has( GNASHING_CHOMPERS ) )
  {
    buff_t* chompers = gnashing_chompers;
    p->register_on_kill_callback( [ chompers ]( player_t* ) {
      if ( !chompers->sim->event_mgr.canceled )
        chompers->trigger();
    } );
  }

  if ( has( SOOTHING_SHADE ) )
  {
    soothing_shade->trigger();
    soothing_shade_driver->trigger();
  }

  if ( has( SOULGLOW_SPECTROMETER ) )
    soulglow_spectrometer->trigger();

  if ( has( DREAM_DELVER ) )
    dream_delver->trigger();

  // Wild Hunt Stratagem only does anything for someone who also took Wild Hunt
  // Tactics, which is the trait that arms it.
  if ( has( WILD_HUNT_STRATAGEM ) && has( WILD_HUNT_TACTICS ) )
    wild_hunt_stratagem->trigger();

  /*
   * Newfound Resolve's Doubt: 351149 rolls every 3 sec (Periodic Dummy). Shadowlands SimulationCraft's model, kept: a shuffled
   * 1-in-30 roll (about once per 90 sec), and the player faces it after ~4 sec (gauss, 20%), never sooner than its 0.9 sec
   * travel + 2 sec, never later than the 11 sec Trial of Doubt lasts; facing it always succeeds.
   */
  if ( has( NEWFOUND_RESOLVE ) && newfound_doubt )
  {
    buff_t* resolve = newfound_resolve;
    shuffled_rng_t* doubt = newfound_doubt;
    timespan_t first = timespan_t::from_seconds( p->rng().range( 0.0, 3.0 ) );
    make_event( p->sim, first, [ p, resolve, doubt ] {
      make_repeating_event( p->sim, 3_s, [ p, resolve, doubt ] {
        if ( !doubt->trigger() )
          return;
        double face = std::clamp( p->rng().gauss( 4.0, 0.8 ), 2.9, 11.0 );
        make_event( p->sim, timespan_t::from_seconds( face ), [ resolve ] { resolve->trigger(); } );
      } );
    } );
  }
}

double effects_t::player_multiplier( const player_t* p ) const
{
  if ( !enabled )
    return 1.0;

  double m = 1.0;

  // "Each nearby enemy ... increasing your damage and healing done by 1% ...
  // up to a maximum of 3 enemies."
  if ( has( BATTLEFIELD_PRESENCE ) )
    m *= 1.0 + 0.01 * std::min( 3, active_enemies( p ) );

  // "The benefits of Well Fed, Flask, and weapon enchant effects are increased
  // by 20%." Raising each consumable's own value would mean reaching into the
  // consumable system from here; the whole-character effect of 20% more of
  // three buffs is small and flat, and is applied as such. Approximate, and
  // marked as approximate rather than presented as exact.
  if ( has( EXACTING_PREPARATION ) )
    m *= 1.005;

  return m;
}

double effects_t::target_multiplier( const player_t* p, const player_t* t ) const
{
  if ( !enabled || !t )
    return 1.0;

  double m = 1.0;

  // "Your damage to targets above 75% health ... is increased by 10%."
  if ( has( WILD_HUNT_TACTICS ) && t->health_percentage() > 75.0 )
    m *= 1.10;

  // "The first enemy you damage in combat is marked as your Adversary. You deal
  // 3% more damage to them." With one target set that is the primary target.
  if ( has( DAUNTLESS_DUELIST ) && t == p->target )
    m *= 1.03;

  // "Your first attack or spell cast on an enemy increases your damage done to
  // them by 12% for 15 sec. Limit 1." One target, so the opening 15 seconds.
  if ( has( PLAGUEYS_STRIKE ) && t == p->target && p->sim->current_time() < 15_s )
    m *= 1.12;

  // Soulglow Spectrometer and Dream Delver both build up on whatever you are
  // hitting, so they belong on the target rather than on the player.
  if ( has( SOULGLOW_SPECTROMETER ) && soulglow_spectrometer->check() )
    m *= 1.0 + soulglow_spectrometer->check_stack_value();

  if ( has( DREAM_DELVER ) && dream_delver->check() )
    m *= 1.0 + dream_delver->check_stack_value();

  // "increase your damage and healing to such targets by 5%" - such targets
  // being the ones below 35% health.
  if ( has( WILD_HUNT_STRATAGEM ) && wild_hunt_stratagem->check() && t->health_percentage() < 35.0 )
    m *= 1.0 + wild_hunt_stratagem->check_value();

  // "When you damage an enemy that's below 35% health, 3% of the damage done is
  // repeated over 5 sec." Modelled as the multiplier it amounts to rather than
  // as a separate periodic action, because it has no spell data to build one
  // from and the repeat cannot itself repeat.
  if ( has( MNEMONIC_EQUIPMENT ) && t->health_percentage() < 35.0 )
    m *= 1.03;

  return m;
}

/*
 * VOLATILE SOLVENT: HUMANOID - the mastery rating, at this actor's own level.
 *
 * The full derivation is in legacy_soulbind_effects.hpp under VOLATILE_SOLVENT.
 * The short version: Midnight's DBC has lost the spell, the Shadowlands DBC on
 * this machine still has its effect row, and that row is a plain scaled rating -
 * scaling class -1 (PLAYER_SPECIAL_SCALE), coefficient 1.263158. Midnight still
 * ships the scaling table the coefficient multiplies, so the value is computed
 * rather than remembered.
 *
 * The check that this is not numerology: it lands on 30.23 at level 60, and the
 * live client shows the author 30.2.
 */
double shadowlands_scaled( const player_t* p, double coefficient )
{
  if ( !p || !p->dbc )
    return 0.0;

  const unsigned level =
      static_cast<unsigned>( std::clamp( p->true_level, 1, MAX_SCALING_LEVEL ) );

  return p->dbc->spell_scaling( PLAYER_SPECIAL_SCALE, level ) * coefficient;
}

double volatile_solvent_humanoid_rating( const player_t* p )
{
  // From the Shadowlands spelleffect row for 323491 (_m_coeff).
  return shadowlands_scaled( p, 1.263158 );
}

/*
 * COMBAT MEDITATION, and how a hardcoded number was caught.
 *
 * This shipped as a flat 79.4 with a comment admitting it: "a RATING read from
 * the live client at level 60 and does not scale, so it is correct in the
 * Shadowlands bracket and negligible above it". The second half of that was
 * wrong in both directions - a flat value is too LARGE below 60, not negligible.
 *
 * the author read it on a level 40 monk on 18 September: "Weapons of Order increases
 * your Mastery by 53.2 for 20.0 sec". Against the level 60's 79.4:
 *
 *     53.2 / spell_scaling(40) = 3.3134
 *     79.4 / spell_scaling(60) = 3.3172
 *
 * Two independent readings agreeing to 0.1% on the same curve Volatile Solvent
 * uses. The flat 79.4 was 73% too generous at bracket 30 and 29% too stingy at
 * 80 - and it is a Kyrian trait, so it was wrong on every Kyrian profile at
 * every bracket but one.
 */
double combat_meditation_mastery( const player_t* p )
{
  return shadowlands_scaled( p, 3.3153 );
}

namespace
{
/*
 * FLESHCRAFT, spell 324631 - the Necrolord class ability.
 *
 * Unlike every covenant CLASS ability (Decimating Bolt, Soul Rot and so on),
 * which differ per class and therefore live in the class modules, Fleshcraft is
 * the same button for all twelve. So it is built once, here.
 *
 * WHAT IT DOES FOR DAMAGE, which is almost nothing on its own. The spell is a
 * flesh shield plus 20% damage reduction while channelling - both defensive.
 * Shadowlands had two soulbind traits that made it a damage button: Pustule
 * Eruption and Volatile Solvent. Pustule Eruption's spell (351094) is gone from
 * the Midnight client with no magnitude to recover, so Volatile Solvent is the
 * whole reason this action exists.
 *
 * THE THING THAT MAKES IT CHEAP, and it is the reason every Shadowlands action
 * list presses it. The buff lands when the cast STARTS, at full duration,
 * regardless of how much of the three second channel you actually finish. So
 * the real cost is one global cooldown, not three seconds - which is why the
 * Shadowlands lists all carry `interrupt_immediate=1,interrupt_if=1` or
 * `cancel_if=buff.volatile_solvent_humanoid.up` next to it. SimulationCraft's
 * own comment on the line that does it: "This triggers the full duration buff
 * at the start of the cast, regardless of channel".
 */
struct fleshcraft_t : public spell_t
{
  bool volatile_solvent;

  fleshcraft_t( player_t* p, std::string_view options_str )
    : spell_t( "fleshcraft", p, p->find_spell( 324631 ) ), volatile_solvent( false )
  {
    harmful = may_crit = may_miss = false;
    channeled = interrupt_auto_attack = true;

    parse_options( options_str );
  }

  void init_finished() override
  {
    spell_t::init_finished();
    volatile_solvent = player->legacy_soulbinds.has( VOLATILE_SOLVENT );
  }

  // Three seconds is three seconds. The channel does not hasten, which is what
  // makes a fast caster pay proportionally more for a full channel and is why
  // cancelling early is not merely an optimisation.
  double composite_haste() const override
  {
    return 1.0;
  }

  void execute() override
  {
    spell_t::execute();

    if ( volatile_solvent && player->legacy_soulbinds.volatile_solvent_humanoid )
      player->legacy_soulbinds.volatile_solvent_humanoid->trigger();

    // Pustule Eruption: a pre-pull cast has no channel, so it hands over the full 9 (Shadowlands did the same).
    if ( is_precombat && player->legacy_soulbinds.trembling_pustules )
      player->legacy_soulbinds.trembling_pustules->trigger( 9 );
  }

  void last_tick( dot_t* d ) override
  {
    spell_t::last_tick( d );

    // Pustule Eruption: 3 pustules per full second channeled, granted as the channel ends.
    if ( player->legacy_soulbinds.trembling_pustules )
    {
      int stacks = 3 * static_cast<int>( std::floor( ( base_tick_time * d->current_tick ) / 1_s ) );
      if ( stacks > 0 )
        player->legacy_soulbinds.trembling_pustules->trigger( stacks );
    }
  }

  timespan_t composite_dot_duration( const action_state_t* s ) const override
  {
    // A channel in the precombat list has nowhere to run: there is no combat to
    // tick in. Shadowlands hit this and solved it the same way - take the
    // execute() (which is where the buff comes from) and skip the channel.
    return is_precombat ? timespan_t::zero() : spell_t::composite_dot_duration( s );
  }
};

}  // namespace

action_t* create_action( player_t* p, std::string_view name, std::string_view options )
{
  if ( !p )
    return nullptr;

  if ( util::str_compare_ci( name, "fleshcraft" ) )
  {
    // Guarded rather than trusted: if the client ever drops 324631 the way it
    // dropped Volatile Solvent's own spell, this returns nothing instead of
    // building an action on a not_found spell.
    if ( !p->find_spell( 324631 )->ok() )
      return nullptr;
    return new fleshcraft_t( p, options );
  }

  return nullptr;
}

}  // namespace legacy_soulbind
