
// ==========================================================================
// Dedmonwakeen's DPS-DPM Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================
/*
]NOTES:

- To evaluate Combo Strikes in the APL, use:
    if=combo_break    true if action is a repeat and can combo strike
    if=combo_strike   true if action is not a repeat or can't combo strike

- To show CJL can be interupted in the APL, use:
     &!prev_gcd.crackling_jade_lightning,interrupt=1

TODO:

GENERAL:
- See other options of modeling Spinning Crane Kick

MISTWEAVER:
Manatee
                  _.---.._
    _        _.-'         ''-.
  .'  '-,_.-'                 '''.
 (       _                     o  :
  '._ .-'  '-._         \  \-  ---]
                '-.___.-')  )..-'
                         (_/lame

WINDWALKER:
- See about removing tick part of Crackling Tiger Lightning
*/
#include "sc_monk.hpp"

#include "action/action_callback.hpp"
#include "action/parse_effects.hpp"
#include "dbc/trait_data.hpp"
#include "player/pet.hpp"
#include "player/pet_spawner.hpp"
#include "report/charts.hpp"
#include "report/highchart.hpp"
#include "sc_enums.hpp"
#include "sim/profileset_control.hpp"

#include <deque>

#include "simulationcraft.hpp"

#define CAST_DURING( ... ) cast_during_ids = { __VA_ARGS__ }
#define SPINNING_CRANE_KICK_IDS \
  player->baseline.brewmaster.spinning_crane_kick->id(), player->baseline.monk.spinning_crane_kick->id()
#define CELESTIAL_CONDUIT_IDS player->talent.conduit_of_the_celestials.celestial_conduit_action->id()

namespace monk
{
namespace functions
{
struct missing_health_percentage_t
{
  monk_t *player;

  missing_health_percentage_t( monk_t *player ) : player( player )
  {
  }

  double operator()( double base ) const
  {
    return 1.0 + ( 1.0 - std::max( player->health_percentage() / 100.0, 0.0 ) ) * base;
  }
};
}  // namespace functions

namespace actions
{
template <class Base>
template <typename... Args>
monk_action_t<Base>::monk_action_t( Args &&...args )
  : parse_action_effects_t<Base>( std::forward<Args>( args )... ),
    ww_mastery( false ),
    may_combo_strike( false ),
    cast_during_ids(),
    track_cd_waste( false ),
    persistent_multiplier_effects(),
    _resource_by_stance()
{
  range::fill( _resource_by_stance, RESOURCE_MAX );

  apply_buff_effects();
  apply_debuff_effects();

  track_cd_waste = base_t::data().cooldown() > 0_ms || base_t::data().charge_cooldown() > 0_ms;
}

template <class Base>
std::string monk_action_t<Base>::full_name() const
{
  std::string n = base_t::data().name_cstr();
  return n.empty() ? base_t::name_str : n;
}

template <class Base>
monk_t *monk_action_t<Base>::p()
{
  return debug_cast<monk_t *>( base_t::player );
}

template <class Base>
const monk_t *monk_action_t<Base>::p() const
{
  return debug_cast<monk_t *>( base_t::player );
}

template <class Base>
monk_td_t *monk_action_t<Base>::get_td( player_t *target ) const
{
  return p()->get_target_data( target );
}

template <class Base>
const monk_td_t *monk_action_t<Base>::find_td( player_t *target ) const
{
  return p()->find_target_data( target );
}

template <class Base>
void monk_action_t<Base>::apply_buff_effects()
{
  /*
   * Temporary action-specific effects that apply to more than one action.
   * If so, the aura gets parsed here with `parse_effects`.
   */

  // Monk

  // Brewmaster
  parse_effects( p()->buff.blackout_combo );
  parse_effects( p()->buff.celestial_flames );
  parse_effects( p()->buff.counterstrike, CONSUME_BUFF );
  parse_effects( p()->buff.empty_barrel );

  // Windwalker
  if ( const auto &effect = p()->baseline.windwalker.mastery->effectN( 1 ); effect.ok() )
  {
    auto mastery_parse_entry = [ & ]( std::vector<player_effect_t> &effect_list ) {
      add_parse_entry( effect_list )
          .set_buff( p()->buff.combo_strikes )
          .set_func( [ & ] { return ww_mastery; } )
          .set_value( effect.mastery_value() )
          .set_mastery( true )
          .set_eff( &effect );
    };
    mastery_parse_entry( base_t::da_multiplier_effects );
    mastery_parse_entry( base_t::ta_multiplier_effects );
  }
  parse_effects( p()->buff.hit_combo );
  parse_effects( p()->buff.press_the_advantage );
  parse_effects( p()->buff.combo_breaker, affect_list_t( 1, 2, 3 ).remove_spell(
                                              p()->talent.windwalker.teachings_of_the_monastery_blackout_kick->id() ) );
  parse_effects( p()->buff.zenith );
  parse_effects( p()->buff.invoke_xuen, effect_mask_t( false ).enable( 3 ), "Ferociousness" );
  parse_effects( p()->buff.dance_of_chiji );

  // Conduit of the Celestials
  parse_effects( p()->buff.heart_of_the_jade_serpent,
                 [ & ] { return !p()->buff.heart_of_the_jade_serpent_unity_within->check(); } );
  parse_effects( p()->buff.heart_of_the_jade_serpent_yulons_avatar );
  parse_effects( p()->buff.heart_of_the_jade_serpent_unity_within );
  parse_effects( p()->buff.jade_sanctuary );
  if ( p()->talent.conduit_of_the_celestials.restore_balance->ok() )
    parse_effects( p()->buff.invoke_xuen, effect_mask_t( false ).enable( 4, 5 ), "Restore Balance" );

  // Master of Harmony
  // TODO: parse_effects implementation for A_MOD_HEALING_RECEIVED_FROM_SPELL (283)
  parse_effects( p()->talent.master_of_harmony.aspect_of_harmony_heal,
                 [ & ] { return p()->buff.aspect_of_harmony.heal_ticking(); } );
  parse_effects( p()->buff.balanced_stratagem_physical, CONSUME_BUFF );
  parse_effects( p()->buff.balanced_stratagem_magic,
                 affect_list_t( 1 )
                     .remove_spell( p()->baseline.monk.crackling_jade_lightning->id() )
                     .remove_spell( p()->talent.brewmaster.exploding_keg->id() ),
                 CONSUME_BUFF );

  // Shado-Pan

  // Midnight S1 Set
  // Midnight S2 Set
  // Midnight S3 Set
}

// Action-related parsing of debuffs. Does not work on spells
// These are action multipliers and what-not that only increase the damage
// of abilities. This does not work with tracking buffs or stat-buffs.
// Things like SEF and Serenity or debuffs that increase the crit chance
// of abilities.
template <class Base>
void monk_action_t<Base>::apply_debuff_effects()
{
  parse_target_effects( td_fn( &monk_td_t::debuff_t::mid2_brm_4pc ), p()->tier.mid2.brm_4pc_debuff );

  // BracketSim legacy compatibility: the Weapons of Order mark makes the target
  // take more of the monk's damage, straight from the debuff's own effects.
  parse_target_effects( td_fn( &monk_td_t::debuff_t::legacy_weapons_of_order ),
                        p()->legacy_covenant.weapons_of_order_debuff );

  // BracketSim legacy compatibility: Keefer's Skyreach.
  parse_target_effects( td_fn( &monk_td_t::debuff_t::legacy_keefers_skyreach ),
                        p()->find_spell( 344021 ) );

  parse_target_effects( td_fn( &monk_td_t::dots_t::aspect_of_harmony ),
                        p()->talent.master_of_harmony.aspect_of_harmony_damage );
}

template <class Base>
std::unique_ptr<expr_t> monk_action_t<Base>::create_expression( std::string_view name_str )
{
  if ( name_str == "combo_strike" )
    return make_mem_fn_expr( name_str, *this, &monk_action_t::is_combo_strike );
  else if ( name_str == "combo_break" )
    return make_mem_fn_expr( name_str, *this, &monk_action_t::is_combo_break );
  return base_t::create_expression( name_str );
}

template <class Base>
bool monk_action_t<Base>::usable_moving() const
{
  if ( base_t::usable_moving() )
    return true;

  if ( this->execute_time() > timespan_t::zero() )
    return false;

  if ( this->channeled )
    return false;

  if ( this->range > 0 && this->range < p()->current.distance_to_move )
    return false;

  return true;
}

template <class Base>
bool monk_action_t<Base>::ready()
{
  if ( cast_during_ids.size() )
    base_t::usable_while_casting = range::any_of( cast_during_ids, [ this ]( const unsigned &id ) {
      return id != 0 && p()->channeling && p()->channeling->id == id;
    } );

  return base_t::ready();
}

template <class Base>
void monk_action_t<Base>::init()
{
  base_t::init();

  // Iterate through power entries, and find if there are resources linked to one of our stances
  for ( const spellpower_data_t &pd : base_t::data().powers() )
  {
    switch ( pd.aura_id() )
    {
      case 137023:
        assert( _resource_by_stance[ dbc::spec_idx( MONK_BREWMASTER, Base::sim->dbc->ptr ) ] == RESOURCE_MAX &&
                "Two power entries per aura id." );
        _resource_by_stance[ dbc::spec_idx( MONK_BREWMASTER, Base::sim->dbc->ptr ) ] = pd.resource();
        break;
      case 137025:
        assert( _resource_by_stance[ dbc::spec_idx( MONK_WINDWALKER, Base::sim->dbc->ptr ) ] == RESOURCE_MAX &&
                "Two power entries per aura id." );
        _resource_by_stance[ dbc::spec_idx( MONK_WINDWALKER, Base::sim->dbc->ptr ) ] = pd.resource();
        break;
      default:
        break;
    }
  }

  if ( cast_during_ids.size() && !base_t::background && !base_t::dual )
    base_t::usable_while_casting = base_t::use_while_casting = true;
}

template <class Base>
void monk_action_t<Base>::init_finished()
{
  // 2H Weapon Scaling
  if ( base_t::attack_power_mod.direct > 0 )
  {
    if ( base_t::ap_type == attack_power_type::WEAPON_BOTH && p()->main_hand_weapon.group() == WEAPON_2H )
    {
      base_t::ap_type = attack_power_type::WEAPON_MAINHAND;
      base_t::base_multiplier *= 0.98;  // This value is not included in spelldata but is included in the tooltip label
    }
  }

  if ( !base_t::does_direct_damage() && !base_t::does_periodic_damage() )
    base_t::remove_damage_entries( persistent_multiplier_effects, "persistent_multiplier_effects" );

  base_t::init_finished();
}

template <class Base>
void monk_action_t<Base>::reset_swing()
{
  if ( p()->main_hand_attack && p()->main_hand_attack->execute_event )
  {
    p()->main_hand_attack->cancel();
    p()->main_hand_attack->schedule_execute();
  }
  if ( p()->off_hand_attack && p()->off_hand_attack->execute_event )
  {
    p()->off_hand_attack->cancel();
    p()->off_hand_attack->schedule_execute();
  }
}

template <class Base>
resource_e monk_action_t<Base>::current_resource() const
{
  if ( p()->specialization() == SPEC_NONE )
  {
    return base_t::current_resource();
  }

  resource_e resource_by_stance = _resource_by_stance[ dbc::spec_idx( p()->specialization() ) ];

  if ( resource_by_stance == RESOURCE_MAX )
    return base_t::current_resource();

  return resource_by_stance;
}

// Check if the combo ability under consideration is different from the last
template <class Base>
bool monk_action_t<Base>::is_combo_strike()
{
  if ( !may_combo_strike )
    return false;

  // We don't know if the first attack is a combo or not, so assume it
  // is. If you change this, also change is_combo_break so that it
  // doesn't combo break on the first attack.
  if ( p()->combo_strike_actions.empty() )
    return true;

  if ( p()->combo_strike_actions.back()->id != base_t::id )
    return true;

  return false;
}

// This differs from combo_strike when the ability can't combo strike. In
// that case both is_combo_strike and is_combo_break are false.
template <class Base>
bool monk_action_t<Base>::is_combo_break()
{
  if ( !may_combo_strike )
    return false;

  return !is_combo_strike();
}

// Trigger Windwalker's Combo Strike Mastery, the Hit Combo talent,
// and other effects that trigger from combo strikes.
// Triggers from execute() on abilities with may_combo_strike = true
// Side effect: modifies combo_strike_actions
template <class Base>
void monk_action_t<Base>::combo_strikes_trigger()
{
  if ( !p()->baseline.windwalker.mastery->ok() )
    return;

  if ( is_combo_strike() )
  {
    p()->buff.combo_strikes->trigger();

    p()->buff.hit_combo->trigger();

    // BracketSim legacy compatibility: Xuen's Bond pulls Invoke Xuen in on
    // every combo strike. The 0.1 s is effect 2 of conduit spell 336616,
    // absent from Midnight and read from the archived 9.2.7 client data.
    if ( p()->legacy_conduits.has( 24 ) )
      p()->get_cooldown( "invoke_xuen_the_white_tiger" )
          ->adjust( timespan_t::from_millis( -100 ), true );

    // Legacy Azerite: Fury of Xuen builds towards its proc on Combo Strikes.
    if ( p()->legacy_azerite.fury_of_xuen.ok() )
      p()->buff.legacy_fury_of_xuen_stacks->trigger();

    // Legacy Azerite: Meridian Strikes pulls Touch of Death in on every combo
    // strike. The value is stored in tenths of a second.
    if ( p()->legacy_azerite.meridian_strikes.ok() )
    {
      auto tod_cd = p()->get_cooldown( "touch_of_death" );
      if ( tod_cd->remains() > timespan_t::zero() )
        tod_cd->adjust( timespan_t::from_seconds(
                            -1 * p()->legacy_azerite.meridian_strikes.spell_ref().effectN( 2 ).base_value() / 100 ),
                        true );
    }
  }
  else
  {
    p()->combo_strike_actions.clear();
    p()->buff.combo_strikes->expire();
    p()->buff.hit_combo->expire();
  }

  // Record the current action in the history.
  p()->combo_strike_actions.push_back( this );
}

template <class Base>
void monk_action_t<Base>::consume_resource()
{
  base_t::consume_resource();

  if ( !base_t::execute_state )  // Fixes rare crashes at combat_end.
    return;

  if ( current_resource() == RESOURCE_CHI )
  {
    if ( p()->talent.windwalker.dance_of_chiji->ok() )
      p()->buff.dance_of_chiji->trigger();

    // Legacy Azerite: Dance of Chi-Ji procs off spending Chi, Windwalker only.
    if ( p()->specialization() == MONK_WINDWALKER && p()->legacy_azerite.dance_of_chiji.ok() )
      p()->buff.legacy_dance_of_chiji->trigger();

    if ( p()->talent.windwalker.tigereye_brew_1->ok() )
    {
      double base_cost = base_t::base_costs[ RESOURCE_CHI ].base;
      if ( base_t::id == 100784 )  // Blackout Kick
        base_cost = base_t::base_costs[ RESOURCE_CHI ];
      double current_value  = p()->buff.tigereye_brew_1_accumulator->stack_value() + base_cost;
      double trigger_amount = p()->talent.windwalker.tigereye_brew_1->effectN( 2 ).base_value();
      if ( current_value >= trigger_amount )
      {
        p()->buff.tigereye_brew_1->trigger();
        current_value -= trigger_amount;
      }

      p()->buff.tigereye_brew_1_accumulator->trigger( 1, current_value );
    }
  }

  // Chi Savings on Dodge & Parry & Miss
  if ( base_t::last_resource_cost > 0 )
  {
    double chi_restored = base_t::last_resource_cost;
    if ( !base_t::aoe && base_t::result_is_miss( base_t::execute_state->result ) )
      p()->resource_gain( RESOURCE_CHI, chi_restored, p()->gain.chi_refund );
  }

  // Energy refund, estimated at 80%
  if ( current_resource() == RESOURCE_ENERGY && base_t::last_resource_cost > 0 && !base_t::hit_any_target )
  {
    double energy_restored = base_t::last_resource_cost * 0.8;

    p()->resource_gain( RESOURCE_ENERGY, energy_restored, p()->gain.energy_refund );
  }
}

template <class Base>
void monk_action_t<Base>::execute()
{
  if ( p()->specialization() == MONK_WINDWALKER )
    if ( may_combo_strike )
      combo_strikes_trigger();

  base_t::execute();
}

template <class Base>
void monk_action_t<Base>::impact( action_state_t *state )
{
  trigger_mystic_touch( state );

  // BracketSim legacy compatibility: Keefer's Skyreach. Tiger Palm opens the
  // crit window, and cannot open it again until the exhaustion runs out.
  if ( p()->shadowlands_legacy.keefers_skyreach && state->result_amount > 0 &&
       util::str_compare_ci( state->action->name_str, "tiger_palm" ) )
  {
    auto td = get_td( state->target );
    if ( !td->debuff.legacy_skyreach_exhaustion->up() )
    {
      td->debuff.legacy_keefers_skyreach->trigger();
      td->debuff.legacy_skyreach_exhaustion->trigger();
    }
  }

  // BracketSim legacy compatibility: Bonedust Brew. Effect 2 is the chance for
  // a struck target to be hit a second time, effect 1 the share of the damage
  // that second hit is worth.
  if ( p()->legacy_covenant.bonedust_brew->ok() && p()->action.legacy_bonedust_brew_damage &&
       state->action != p()->action.legacy_bonedust_brew_damage && state->result_amount > 0 &&
       get_td( state->target )->debuff.legacy_bonedust_brew->up() &&
       Base::rng().roll( p()->legacy_covenant.bonedust_brew->effectN( 2 ).percent() ) )
  {
    auto bdb = p()->action.legacy_bonedust_brew_damage;
    // BracketSim legacy compatibility: Bone Marrow Hops (conduit 60) raises the
    // share of the original hit that the bonus hit is worth.
    bdb->base_dd_min = bdb->base_dd_max =
        state->result_amount * p()->legacy_covenant.bonedust_brew->effectN( 1 ).percent() *
        ( 1.0 + p()->legacy_conduits.percent( 60 ) );
    bdb->execute_on_target( state->target );
  }

  // BracketSim legacy compatibility: Bountiful Brew. A free Bonedust Brew on
  // the struck target, paced by 356592's own RPPM and internal cooldown. This
  // port has no player-side Bonedust Brew buff - only the target debuff - so
  // that debuff is what gets extended.
  if ( p()->shadowlands_legacy.bountiful_brew && state->result_amount > 0 &&
       p()->legacy_covenant.bonedust_brew->ok() && p()->legacy_bountiful_brew_rppm &&
       p()->legacy_bountiful_brew_rppm->trigger() )
  {
    get_td( state->target )
        ->debuff.legacy_bonedust_brew->extend_duration_or_trigger(
            p()->find_spell( 356592 )->effectN( 1 ).time_value() );
  }

  // BracketSim legacy compatibility: Sinister Teachings. A critical hit while
  // the mystic portal is open shortens Fallen Order. Shadowlands checked a
  // buff; this port models the portal as a dot, so the dot ticking IS the
  // portal being open.
  if ( p()->shadowlands_legacy.sinister_teachings && state->result == RESULT_CRIT &&
       state->result_amount > 0 && p()->cooldown.legacy_sinister_teachings->up() &&
       state->target->get_dot( "fallen_order", p() )->is_ticking() )
  {
    auto rune = p()->find_spell( 356818 );
    // Effect 4 is Mistweaver's smaller cut, effect 3 everyone else's.
    auto cut = p()->specialization() == MONK_MISTWEAVER ? rune->effectN( 4 ).time_value()
                                                        : rune->effectN( 3 ).time_value();
    p()->cooldown.legacy_fallen_order->adjust( -cut );
    p()->cooldown.legacy_sinister_teachings->start( rune->internal_cooldown() );
  }

  // Legacy Azerite: Sunrise Technique fires off the next physical hit into a
  // target Rising Sun Kick marked.
  if ( p()->legacy_azerite.sunrise_technique.ok() && p()->action.legacy_sunrise_technique &&
       state->action != p()->action.legacy_sunrise_technique &&  // must not retrigger itself
       state->action->school == SCHOOL_PHYSICAL && state->result_amount > 0 &&
       p()->buff.legacy_sunrise_technique->up() && get_td( state->target )->debuff.legacy_sunrise_technique->up() )
  {
    p()->action.legacy_sunrise_technique->execute_on_target( state->target );
  }

  base_t::impact( state );
}

template <class Base>
void monk_action_t<Base>::trigger_mystic_touch( action_state_t *s )
{
  if ( base_t::sim->overrides.mystic_touch )
    return;

  if ( base_t::result_is_miss( s->result ) || s->result_amount == 0.0 )
    return;

  if ( s->target->debuffs.mystic_touch && p()->baseline.monk.mystic_touch->ok() )
    s->target->debuffs.mystic_touch->trigger();
}

// BracketSim legacy compatibility: Faeline Harmony (7721). Fae Exposure raises
// the monk's damage on a marked target, and it does so with aura subtype 270 -
// A_MOD_DAMAGE_FROM_CASTER, which applies by SCHOOL MASK. parse_target_effects
// only handles 271, the whitelist-of-spells variant that Keefer's Skyreach uses,
// so this one has to be applied here. The 8% is still read from the spell.
template <class Base>
double monk_action_t<Base>::composite_target_multiplier( player_t *t ) const
{
  double m = base_t::composite_target_multiplier( t );

  if ( p()->shadowlands_legacy.faeline_harmony )
  {
    auto td = p()->find_target_data( t );
    if ( td && td->debuff.legacy_fae_exposure->check() )
      m *= 1.0 + p()->find_spell( 356773 )->effectN( 1 ).percent();
  }

  return m;
}

template <class Base>
double monk_action_t<Base>::composite_persistent_multiplier( const action_state_t *state ) const
{
  double cpm = base_t::composite_persistent_multiplier( state );

  for ( const auto &i : persistent_multiplier_effects )
    cpm *= 1.0 + base_t::get_effect_value( i, true );

  return cpm;
}

template <class Base>
size_t monk_action_t<Base>::total_effects_count() const
{
  return base_t::total_effects_count() + persistent_multiplier_effects.size();
}

template <class Base>
void monk_action_t<Base>::print_parsed_custom_type( report::sc_html_stream &os ) const
{
  using this_t = monk_action_t<Base>;
  base_t::template print_parsed_type<this_t>( os, &this_t::persistent_multiplier_effects, "Snapshots" );
}

monk_spell_t::monk_spell_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
  : monk_action_t<spell_t>( name, player, spell_data )
{
  ap_type = attack_power_type::WEAPON_MAINHAND;
}

monk_heal_t::monk_heal_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
  : monk_action_t<heal_t>( name, player, spell_data )
{
  harmful = false;
  ap_type = attack_power_type::WEAPON_MAINHAND;
}

monk_absorb_t::monk_absorb_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
  : monk_action_t<absorb_t>( name, player, spell_data )
{
}

monk_melee_attack_t::monk_melee_attack_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
  : monk_action_t<melee_attack_t>( name, player, spell_data )
{
  special    = true;
  may_glance = false;
  // Monk melee attacks do not have hasted GCD by default. Exceptions should be explicitly made.
  // While action_t sets attacks with 1s gcd to be non-hasted, monks use spec auras to lower ability gcds by 500ms which
  // is applied in init_finished, thus currently we cannot rely on action_t spell data parsing.
  gcd_type = gcd_haste_type::NONE;
}

// Physical tick_action abilities need amount_type() override, so the
// tick_action are properly physically mitigated.
result_amount_type monk_melee_attack_t::amount_type( const action_state_t *state, bool periodic ) const
{
  if ( tick_action && tick_action->school == SCHOOL_PHYSICAL )
  {
    return result_amount_type::DMG_DIRECT;
  }
  else
  {
    return base_t::amount_type( state, periodic );
  }
}

struct monk_snapshot_stats_t : public snapshot_stats_t
{
  monk_snapshot_stats_t( monk_t *player, std::string_view options ) : snapshot_stats_t( player, options )
  {
  }

  void execute() override
  {
    snapshot_stats_t::execute();
  }
};

struct vital_flame_t : public monk_heal_t
{
  vital_flame_t( monk_t *player ) : monk_heal_t( player, "vital_flame", player->talent.brewmaster.vital_flame_heal )
  {
    background      = true;
    proc            = true;
    target          = player;
    base_multiplier = player->talent.brewmaster.vital_flame->effectN( 1 ).percent();
  }

  void init() override
  {
    monk_heal_t::init();
    update_flags = snapshot_flags = STATE_NO_MULTIPLIER | STATE_MUL_SPELL_DA;
  }
};

template <typename base_action_t>
struct harmonic_surge_t : public base_action_t
{
  using base_t = harmonic_surge_t<base_action_t>;

  template <typename TBase>
  struct impact_t : TBase
  {
    impact_t( monk_t *player, std::string_view name, const spell_data_t *spell_data, unsigned effect_index = 0 )
      : TBase( player, fmt::format( "harmonic_surge_{}", name ), spell_data )
    {
      TBase::background = true;
      TBase::dual       = true;

      const auto &effect            = spell_data->effectN( effect_index ? effect_index : 1 );
      TBase::spell_power_mod.direct = effect.sp_coeff();

      if ( effect.target_1() == T_DESTINATION_TARGET_ENEMY && effect.target_2() == T_UNIT_DESTINATION_AREA_ENEMY )
      {
        TBase::aoe                    = -1;
        TBase::reduced_aoe_targets    = player->talent.master_of_harmony.harmonic_surge->effectN( 7 ).base_value();
        TBase::target_filter_callback = TBase::secondary_targets_only();
      }

      if ( effect.target_1() == T_DESTINATION_CASTER && effect.target_2() == T_UNIT_DESTINATION_AREA_ALLY )
      {
        TBase::aoe = as<int>( spell_data->effectN( 2 ).base_value() );
      }

      size_t offset = 1;
      switch ( effect.type() )
      {
        case E_SCHOOL_DAMAGE:
          break;
        case E_HEAL:
          offset += 2;
          break;
        default:
          assert( false );
      }

      if ( const spelleffect_data_t &effect = player->talent.master_of_harmony.harmonic_surge->effectN( offset );
           effect.ok() )
        add_parse_entry( TBase::da_multiplier_effects )
            .set_value( effect.percent() - 1.0 )
            .set_note( "Scripted Direct Damage/Healing Aura" )
            .set_eff( &effect );

      if ( const spelleffect_data_t &effect = player->talent.master_of_harmony.harmonic_surge->effectN( 1 );
           effect.ok() )
        add_parse_entry( TBase::da_multiplier_effects )
            .set_buff( player->buff.harmonic_surge )
            .set_use_stacks( true )
            .set_value( 1.0 )
            .set_note( "Potential Energy Stack Count" )
            .set_eff( &effect );
    }
  };

  action_t *aoe;
  action_t *st;
  action_t *heal;

  template <typename... Args>
  harmonic_surge_t( monk_t *player, std::string_view name, Args &&...args )
    : base_action_t( player, name, std::forward<Args>( args )... ), aoe( nullptr ), st( nullptr ), heal( nullptr )
  {
    if ( !player->talent.master_of_harmony.harmonic_surge->ok() )
      return;

    st   = new impact_t<monk_spell_t>( player, fmt::format( "damage_st_{}", name ),
                                       player->talent.master_of_harmony.harmonic_surge_damage, 1 );
    aoe  = new impact_t<monk_spell_t>( player, fmt::format( "damage_aoe_{}", name ),
                                       player->talent.master_of_harmony.harmonic_surge_damage, 2 );
    heal = new impact_t<monk_heal_t>( player, fmt::format( "heal_{}", name ),
                                      player->talent.master_of_harmony.harmonic_surge_heal );

    base_action_t::add_child( st );
    base_action_t::add_child( aoe );
    base_action_t::add_child( heal );
  }

  void execute() override
  {
    base_action_t::execute();

    if ( !base_action_t::p()->buff.harmonic_surge->up() )
      return;

    st->execute();
    aoe->execute();
    heal->execute();

    base_action_t::p()->buff.harmonic_surge->expire();
  }
};

namespace
{
struct high_impact_t : public monk_spell_t
{
  high_impact_t( monk_t *player )
    : monk_spell_t( player, "high_impact", player->talent.shado_pan.high_impact_debuff->effectN( 1 ).trigger() )
  {
    aoe        = -1;
    background = dual = true;
  }
};

struct shado_over_the_battlefield_t : public monk_spell_t
{
  shado_over_the_battlefield_t( monk_t *player )
    : monk_spell_t( player, "flurry_strike_shado_over_the_battlefield",
                    player->talent.shado_pan.flurry_strike_shado_over_the_battlefield )
  {
    background = dual   = true;
    reduced_aoe_targets = player->talent.shado_pan.shado_over_the_battlefield->effectN( 2 ).base_value();
    aoe                 = -1;
  }
};
};  // namespace

flurry_strikes_t::flurry_strike_t::flurry_strike_t( monk_t *player, std::string_view name )
  : monk_spell_t( player, name, player->talent.shado_pan.flurry_strike )
{
  background = dual = true;
  aoe               = 1;
}

void flurry_strikes_t::flurry_strike_t::impact( action_state_t *state )
{
  monk_spell_t::impact( state );

  if ( auto target_data = p()->get_target_data( state->target ); target_data )
    target_data->debuff.high_impact->trigger();
}

flurry_strikes_t::flurry_strikes_t( bool fallback, monk_t *player )
  : monk_spell_t( player, "flurry_strikes", player->talent.shado_pan.flurry_strikes ),
    fallback( !fallback ),
    high_impact( nullptr ),
    shado_over_the_battlefield( nullptr ),
    wisdom_of_the_wall( nullptr )
{
  background = dual = true;

  if ( this->fallback )
    return;

  if ( player->talent.shado_pan.high_impact->ok() )
  {
    high_impact = new high_impact_t( player );
    add_child( high_impact );
    player->register_on_kill_callback( [ & ]( player_t *target ) {
      if ( p()->sim->event_mgr.canceled )
        return;

      if ( auto target_data = p()->get_target_data( target ); target_data && target_data->debuff.high_impact->up() )
        high_impact->execute_on_target( target );
    } );
  }

  shado_over_the_battlefield = new shado_over_the_battlefield_t( player );
  add_child( shado_over_the_battlefield );

  flurry_strike_variants.insert( { FLURRY_STRIKES, new flurry_strike_t( player, "flurry_strike" ) } );
  flurry_strike_variants.insert( { STAND_READY, new flurry_strike_t( player, "flurry_strike_stand_ready" ) } );
  flurry_strike_variants.insert(
      { WISDOM_OF_THE_WALL, new flurry_strike_t( player, "flurry_strike_wisdom_of_the_wall" ) } );
  wisdom_of_the_wall = player->get_cooldown( "wisdom_of_the_wall" );

  for ( auto &[ key, variant ] : flurry_strike_variants )
  {
    add_child( variant );

    switch ( key )
    {
      case FLURRY_STRIKES:
      case WISDOM_OF_THE_WALL:
        break;
      case STAND_READY:
        if ( const auto &effect = player->talent.shado_pan.stand_ready->effectN( 2 ); effect.ok() )
          add_parse_entry( variant->da_multiplier_effects )
              .set_value( effect.percent() - 1.0 )
              .set_note( "Stand Ready Efficiency Multiplier" )
              .set_eff( &effect );
        break;
      default:
        assert( false );
    }
  }
}

void flurry_strikes_t::execute( source_e source )
{
  if ( fallback )
    return;

  int count = 0;
  switch ( source )
  {
    case FLURRY_STRIKES:
      count = p()->buff.flurry_charge->stack();
      p()->buff.flurry_charge->expire();
      break;
    case STAND_READY:
      if ( p()->buff.stand_ready->up() )
      {
        count = as<int>( p()->talent.shado_pan.stand_ready->effectN( 1 ).base_value() );
        p()->buff.stand_ready->expire();
      }
      break;
    case WISDOM_OF_THE_WALL:
      if ( ( p()->buff.zenith->check() || p()->buff.invoke_niuzao->check() ) && wisdom_of_the_wall->up() )
      {
        wisdom_of_the_wall->start( p()->talent.shado_pan.wisdom_of_the_wall->internal_cooldown() );
        count = as<int>( p()->talent.shado_pan.wisdom_of_the_wall->effectN( 1 ).base_value() );
      }
      break;
    default:
      assert( false );
  }

  for ( int i = 0; i < count; i++ )
  {
    make_event<events::delayed_execute_event_t>( *sim, p(), flurry_strike_variants.at( source ), p()->target,
                                                 i * 150_ms );
    make_event<events::delayed_execute_event_t>( *sim, p(), shado_over_the_battlefield, p()->target, i * 150_ms );
  }

  monk_spell_t::execute();
}

template <class base_action_t>
struct overwhelming_force_t : base_action_t
{
  using base_t = overwhelming_force_t<base_action_t>;
  struct damage_t : monk_spell_t
  {
    damage_t( monk_t *player, std::string_view name )
      : monk_spell_t( player, fmt::format( "overwhelming_force_{}", name ),
                      player->talent.master_of_harmony.overwhelming_force_damage )
    {
      background = dual = proc = true;
      base_multiplier          = player->talent.master_of_harmony.overwhelming_force->effectN( 1 ).percent();
      aoe                      = -1;
      reduced_aoe_targets      = player->talent.master_of_harmony.overwhelming_force->effectN( 2 ).base_value();
    }

    void init() override
    {
      monk_spell_t::init();
      update_flags = snapshot_flags &= STATE_NO_MULTIPLIER | STATE_MUL_SPELL_DA;
    }
  };

  damage_t *overwhelming_force_damage;

  template <typename... Args>
  overwhelming_force_t( monk_t *player, Args &&...args )
    : base_action_t( player, std::forward<Args>( args )... ), overwhelming_force_damage( nullptr )
  {
    if ( !player->talent.master_of_harmony.overwhelming_force->ok() )
      return;

    overwhelming_force_damage = new damage_t( player, base_action_t::name_str );
    base_action_t::add_child( overwhelming_force_damage );
  }

  void impact( action_state_t *state ) override
  {
    base_action_t::impact( state );

    if ( !base_action_t::p()->talent.master_of_harmony.overwhelming_force->ok() || state->chain_target > 0 )
      return;

    overwhelming_force_damage->execute_on_target( state->target, state->result_amount );
  }
};

struct tiger_palm_t : public harmonic_surge_t<overwhelming_force_t<monk_melee_attack_t>>
{
  tiger_palm_t( monk_t *player, std::string_view options_str )
    : base_t( player, "tiger_palm", player->baseline.monk.tiger_palm )
  {
    parse_options( options_str );

    ww_mastery       = true;
    may_combo_strike = true;
    CAST_DURING( SPINNING_CRANE_KICK_IDS );

    parse_effects( player->buff.combat_wisdom );

    // Legacy Azerite: Pressure Point. Battle for Azeroth hung this on a tier-set
    // buff; the current tooltip is a flat Tiger Palm damage bonus plus a higher
    // free-Blackout-Kick chance, and only the damage half has a host here.
    base_dd_adder += player->legacy_azerite.pressure_point.value( 1 );
  }

  bool ready() override
  {
    if ( p()->talent.brewmaster.press_the_advantage->ok() )
      return false;
    return base_t::ready();
  }

  void execute() override
  {
    if ( p()->buff.blackout_combo->up() )
      p()->proc.blackout_combo_tiger_palm->occur();

    if ( p()->buff.courage_of_the_white_tiger->up() )
      p()->action.courage_of_the_white_tiger.base->execute();

    base_t::execute();

    p()->buff.blackout_combo->expire();

    if ( result_is_miss( execute_state->result ) )
      return;

    if ( p()->talent.windwalker.combo_breaker->ok() )
      p()->buff.combo_breaker->trigger();

    // Reduces the remaining cooldown on your Brews by 1 sec
    p()->baseline.brewmaster.brews.adjust(
        timespan_t::from_seconds( p()->baseline.monk.tiger_palm->effectN( 3 ).base_value() ) );

    p()->baseline.brewmaster.brews.adjust( p()->talent.brewmaster.face_palm->effectN( 3 ).time_value() );

    if ( p()->buff.combat_wisdom->up() )
    {
      p()->action.combat_wisdom_eh->execute();
      p()->buff.combat_wisdom->expire();
    }
  }

  void impact( action_state_t *s ) override
  {
    base_t::impact( s );

    p()->buff.teachings_of_the_monastery->trigger();
  }
};

struct rising_sun_kick_t : monk_melee_attack_t
{
  enum source_e
  {
    RISING_SUN_KICK,
    RUSHING_WIND_KICK
  };

  static const char *skyfire_heel_source_string( source_e source )
  {
    switch ( source )
    {
      case RISING_SUN_KICK:
        return "rising_sun_kick";
      case RUSHING_WIND_KICK:
        return "rushing_wind_kick";
      default:
        assert( false );
        return "unknown";
    }
  }

  struct base_damage_t : monk_melee_attack_t
  {
    base_damage_t( monk_t *player, std::string_view name, const spell_data_t *spell )
      : monk_melee_attack_t( player, name, spell )
    {
      ww_mastery = true;
      background = dual = true;

      if ( const auto &effect = player->talent.windwalker.skyfire_heel_buff->effectN( 1 );
           player->talent.windwalker.skyfire_heel->ok() )
      {
        int max_count = as<int>( player->talent.windwalker.skyfire_heel->effectN( 3 ).base_value() );
        add_parse_entry( crit_chance_effects )
            .set_value( effect.percent() )
            .set_value_func( [ &ae = player->sim->active_enemies, max_count ]( double base ) {
              return base * std::min( ae, max_count );
            } )
            .set_note( "Nearby Enemy Scaling" )
            .set_eff( &effect );
      }

      if ( const auto &effect = player->talent.windwalker.sunfire_spiral->effectN( 1 ); effect.ok() )
        add_parse_entry( da_multiplier_effects )
            .set_buff( player->buff.combo_strikes )
            .set_value( effect.percent() )
            .set_note( "Applies when buffed by Mastery" )
            .set_eff( &effect );

      parse_effects( player->buff.mid2_ww_4pc, CONSUME_BUFF );
    }

    void impact( action_state_t *state ) override
    {
      monk_melee_attack_t::impact( state );

      // Legacy Azerite: Sunrise Technique marks the target and arms the player.
      if ( p()->legacy_azerite.sunrise_technique.ok() )
      {
        p()->buff.legacy_sunrise_technique->trigger();
        get_td( state->target )->debuff.legacy_sunrise_technique->trigger();
      }

      // BracketSim legacy compatibility: Xuen's Treasure pulls Fists of Fury
      // forward on a Rising Sun Kick critical strike.
      if ( p()->shadowlands_legacy.xuens_battlegear && state->result == RESULT_CRIT )
        // Spell 337481 carries Max Aura Level 60, and player_t::find_spell REJECTS a
        // spell whose max aura level is below the player's - it returns
        // not_found, whose every effect reads zero. That is why this measured
        // exactly nothing on a level 90 probe while the flag, the host and the
        // cooldown were all correct. dbc::find_spell is the same lookup without
        // the level gate. Only two sites in the whole engine hit this; both are
        // fixed, and both were fine at level 60 and below.
        p()->cooldown.fists_of_fury->adjust(
            -timespan_t::from_millis(
                dbc::find_spell( p(), 337481U )->effectN( 2 ).base_value() ), true );

      if ( p()->baseline.windwalker.combat_conditioning->ok() )
        state->target->debuffs.mortal_wounds->trigger();

      if ( !state->chain_target && state->result == RESULT_CRIT && p()->talent.windwalker.xuens_battlegear->ok() )
      {
        timespan_t value = -1 * p()->talent.windwalker.xuens_battlegear->effectN( 2 ).time_value();
        p()->cooldown.fists_of_fury->adjust( value, true );
      }
    }
  };

  template <typename TBase>
  struct glory_of_the_dawn_t : TBase
  {
    struct damage_t : base_damage_t
    {
      damage_t( monk_t *player, std::string_view parent_name )
        : base_damage_t( player, fmt::format( "glory_of_the_dawn_{}", parent_name ),
                         player->talent.windwalker.glory_of_the_dawn_damage )
      {
      }
    };

    action_t *damage;

    template <typename... Args>
    glory_of_the_dawn_t( monk_t *player, Args &&...args )
      : TBase( player, std::forward<Args>( args )... ), damage( nullptr )
    {
      if ( !player->talent.windwalker.glory_of_the_dawn->ok() )
        return;

      damage = new damage_t( player, TBase::name() );
      TBase::add_child( damage );
    }

    void execute() override
    {
      TBase::execute();

      if ( !damage )
        return;

      double chance = TBase::p()->talent.windwalker.glory_of_the_dawn->effectN( 2 ).percent() *
                      ( ( 1.0 / TBase::p()->composite_melee_haste() ) - 1.0 );
      if ( TBase::rng().roll( chance ) )
        damage->execute();

      // Legacy Azerite: Glory of the Dawn rolls separately from the modern
      // talent, at a flat chance and for a flat amount.
      auto *p = TBase::p();
      if ( p->legacy_azerite.glory_of_the_dawn.ok() && p->action.legacy_glory_of_the_dawn &&
           TBase::rng().roll( p->legacy_azerite.glory_of_the_dawn.spell_ref().effectN( 3 ).percent() ) )
      {
        double raw = p->legacy_azerite.glory_of_the_dawn.value();
        p->action.legacy_glory_of_the_dawn->base_dd_min = raw;
        p->action.legacy_glory_of_the_dawn->base_dd_max = raw;
        p->action.legacy_glory_of_the_dawn->execute_on_target( TBase::target );
      }
    }
  };

  template <typename TBase, source_e source>
  struct skyfire_heel_t : TBase
  {
    struct damage_t : monk_melee_attack_t
    {
      damage_t( monk_t *player )
        : monk_melee_attack_t( player, fmt::format( "skyfire_heel_{}", skyfire_heel_source_string( source ) ),
                               player->talent.windwalker.skyfire_heel_damage )
      {
        switch ( source )
        {
          case RISING_SUN_KICK:
            aoe                    = -1;
            target_filter_callback = secondary_targets_only();
            break;
          case RUSHING_WIND_KICK:
            break;
        }

        reduced_aoe_targets = player->talent.windwalker.skyfire_heel->effectN( 2 ).base_value();

        background = dual = true;
      }
    };

    action_t *damage;

    double total;
    std::map<player_t *, double> track;

    template <typename... Args>
    skyfire_heel_t( monk_t *player, Args &&...args )
      : TBase( player, std::forward<Args>( args )... ), damage( nullptr ), total( 0.0 ), track( {} )
    {
      if ( !player->talent.windwalker.skyfire_heel->ok() )
        return;

      damage = new damage_t( player );
      TBase::add_child( damage );
    }

    void execute() override
    {
      TBase::execute();

      if ( damage && TBase::aoe )
      {
        /*
          Manually handle Skyfire Heel when the source action is AOE as impact
          count scales quadratically with target count. This causes sims to get
          stuck in some DungeonRoute configurations.

          In most cases this linearization should not negatively impact proc
          behaviour, but comes with some potential future risk.
         */
        double coefficient = TBase::p()->talent.windwalker.skyfire_heel->effectN( 1 ).percent() *
                             std::sqrt( damage->reduced_aoe_targets /
                                        std::min<int>( TBase::sim->max_aoe_enemies, TBase::num_targets_hit ) );

        for ( const auto &[ target, amount ] : track )
          damage->execute_on_target( target, ( total - amount ) * coefficient );

        track.clear();
        total = 0.0;
      }
    }

    void impact( action_state_t *state ) override
    {
      TBase::impact( state );

      if ( !damage )
        return;

      if ( TBase::aoe )
      {
        assert( track.find( state->target ) == track.end() );
        total += state->result_amount;
        track.emplace( state->target, state->result_amount );
      }
      else
      {
        double value = state->result_amount * TBase::p()->talent.windwalker.skyfire_heel->effectN( 1 ).percent();
        damage->execute_on_target( state->target, value );
      }
    }

    void reset() override
    {
      TBase::reset();

      track.clear();
      total = 0.0;
    }
  };

  using combined_type_t = glory_of_the_dawn_t<skyfire_heel_t<base_damage_t, source_e::RISING_SUN_KICK>>;

  struct damage_t : combined_type_t
  {
    damage_t( monk_t *player )
      : combined_type_t( player, "rising_sun_kick_damage", player->talent.monk.rising_sun_kick->effectN( 1 ).trigger() )
    {
    }
  };

  action_t *rising_sun_kick;

  rising_sun_kick_t( monk_t *player, std::string_view options_str )
    : monk_melee_attack_t( player, "rising_sun_kick", player->talent.monk.rising_sun_kick ),
      rising_sun_kick( new damage_t( player ) )
  {
    parse_options( options_str );

    may_combo_strike = true;
    CAST_DURING( SPINNING_CRANE_KICK_IDS );

    add_child( rising_sun_kick );
  }

  bool ready() override
  {
    if ( p()->buff.rushing_wind_kick->check() )
      return false;

    return monk_melee_attack_t::ready();
  }

  void execute() override
  {
    monk_melee_attack_t::execute();

    rising_sun_kick->execute_on_target( target );

    if ( p()->specialization() == MONK_WINDWALKER )
      p()->action.flurry_strikes->execute( flurry_strikes_t::WISDOM_OF_THE_WALL );

    p()->buff.whirling_dragon_punch->trigger();
    p()->action.chi_wave->execute();
  }
};

struct rushing_wind_kick_t : monk_melee_attack_t
{
  using combined_type_t = rising_sun_kick_t::glory_of_the_dawn_t<rising_sun_kick_t::skyfire_heel_t<
      rising_sun_kick_t::base_damage_t, rising_sun_kick_t::source_e::RUSHING_WIND_KICK>>;

  struct damage_t : combined_type_t
  {
    damage_t( monk_t *player )
      : combined_type_t( player, "rushing_wind_kick_damage",
                         player->talent.windwalker.rushing_wind_kick_action->effectN( 1 ).trigger() )
    {
      background = dual = true;

      may_combo_strike = true;
      aoe              = -1;
      split_aoe_damage = true;

      add_parse_entry( da_multiplier_effects )
          .set_func( []() { return false; } )
          .set_value( player->talent.windwalker.rushing_wind_kick_action->effectN( 1 ).percent() )
          .set_note( "Target-count AoE Scaling" );
    }

    double composite_aoe_multiplier( const action_state_t *state ) const override
    {
      double cam = combined_type_t::composite_aoe_multiplier( state );

      double count = std::min( as<double>( state->n_targets ),
                               p()->talent.windwalker.rushing_wind_kick_action->effectN( 2 ).base_value() );
      double mult  = 1.0 + count * p()->talent.windwalker.rushing_wind_kick_action->effectN( 1 ).percent();
      cam *= mult;

      return cam;
    }
  };

  rushing_wind_kick_t( monk_t *player, std::string_view options_str )
    : monk_melee_attack_t( player, "rushing_wind_kick", player->talent.windwalker.rushing_wind_kick_action )
  {
    parse_options( options_str );

    // Note that Rushing Wind Kick cannot be parented to Rising Sun Kick
    // due to max parent depth.
    if ( !player->talent.windwalker.rushing_wind_kick->ok() )
      return;

    execute_action = new damage_t( player );
    add_child( execute_action );
  }

  bool ready() override
  {
    if ( p()->buff.rushing_wind_kick->up() )
      return monk_melee_attack_t::ready();

    return false;
  }

  void execute() override
  {
    monk_melee_attack_t::execute();
    p()->buff.rushing_wind_kick->expire();
  }
};

template <class base_action_t>
struct charred_passions_t : base_action_t
{
  using base_t = charred_passions_t<base_action_t>;
  struct damage_t : monk_spell_t
  {
    damage_t( monk_t *player, std::string_view name )
      : monk_spell_t( player, fmt::format( "charred_passions_{}", name ),
                      player->talent.brewmaster.charred_passions_damage )
    {
      background = dual = proc = true;
      base_multiplier          = player->talent.brewmaster.charred_passions->effectN( 1 ).percent();
    }

    void init() override
    {
      monk_spell_t::init();
      update_flags = snapshot_flags = STATE_NO_MULTIPLIER | STATE_MUL_SPELL_DA;
    }
  };

  damage_t *chp_damage;
  cooldown_t *chp_cooldown;

  template <typename... Args>
  charred_passions_t( monk_t *player, Args &&...args ) : base_action_t( player, std::forward<Args>( args )... )
  {
    if ( !player->talent.brewmaster.charred_passions->ok() )
      return;

    chp_cooldown = player->get_cooldown( "charred_passions" );
    chp_damage   = new damage_t( player, base_action_t::name_str );

    base_action_t::add_child( chp_damage );
  }

  void execute() override
  {
    base_action_t::execute();

    if ( !base_action_t::p()->buff.charred_passions->up() )
      return;

    if ( chp_cooldown->up() )
      chp_cooldown->start( chp_damage->data().effectN( 1 ).trigger()->internal_cooldown() );
  }

  void impact( action_state_t *state ) override
  {
    base_action_t::impact( state );

    if ( !base_action_t::p()->buff.charred_passions->up() )
      return;

    base_action_t::p()->proc.charred_passions->occur();
    chp_damage->execute_on_target( state->target, state->result_amount );

    if ( monk_td_t *target_data = base_action_t::get_td( state->target );
         target_data && target_data->dot.breath_of_fire->is_ticking() && chp_cooldown->up() )
      target_data->dot.breath_of_fire->refresh_duration();
  }
};

struct base_blackout_kick_t : monk_melee_attack_t
{
  proc_t *rising_sun_kick_reset;

  base_blackout_kick_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
    : monk_melee_attack_t( player, name, spell_data ), rising_sun_kick_reset( nullptr )
  {
    // TODO: check this
    ap_type = attack_power_type::WEAPON_BOTH;

    if ( const auto &effect = player->talent.windwalker.shadowboxing_treads->effectN( 3 ); effect.ok() )
      add_parse_entry( target_multiplier_effects )
          .set_func( [ this ]( actor_target_data_t *target_data ) { return target_data->target != target; } )
          .set_value( effect.percent() - 1.0 )
          .set_note( "Secondary Target Reduction" )
          .set_eff( &effect );
  }

  void init() override
  {
    monk_melee_attack_t::init();

    if ( !p()->talent.windwalker.teachings_of_the_monastery->ok() )
      return;

    rising_sun_kick_reset = p()->get_proc( "Teachings of the Monastery - Rising Sun Kick Reset" );
  }

  void impact( action_state_t *state ) override
  {
    monk_melee_attack_t::impact( state );

    if ( !p()->talent.windwalker.teachings_of_the_monastery->ok() )
      return;

    double chance = p()->talent.windwalker.teachings_of_the_monastery->effectN( 1 ).percent();
    if ( rng().roll( chance ) )
    {
      p()->cooldown.rising_sun_kick->reset( true );
      rising_sun_kick_reset->occur();
    }
  }
};

template <typename TBase>
struct teachings_of_the_monastery_t : TBase
{
  struct damage_t : base_blackout_kick_t
  {
    damage_t( monk_t *player )
      : base_blackout_kick_t( player, "teachings_of_the_monastery",
                              player->talent.windwalker.teachings_of_the_monastery_blackout_kick )
    {
      ww_mastery = false;
      background = dual = true;
    }
  };

  action_t *damage;

  template <typename... Args>
  teachings_of_the_monastery_t( monk_t *player, Args &&...args )
    : TBase( player, std::forward<Args>( args )... ), damage( nullptr )
  {
    if ( !player->talent.windwalker.teachings_of_the_monastery->ok() )
      return;

    damage = new damage_t( player );
    TBase::add_child( damage );
  }

  void execute() override
  {
    TBase::execute();

    double chance = TBase::p()->talent.conduit_of_the_celestials.xuens_guidance->effectN( 1 ).percent();
    unsigned proc = 0;
    if ( unsigned count = as<unsigned>( TBase::p()->buff.teachings_of_the_monastery->stack() ); count )
    {
      TBase::p()->buff.teachings_of_the_monastery->expire();
      for ( unsigned i = 0; i < count; ++i )
      {
        make_event<events::delayed_execute_event_t>( *TBase::sim, TBase::p(), damage, TBase::p()->target, i * 100_ms );
        if ( TBase::p()->rng().roll( chance ) )
          proc++;
      }
    }

    if ( proc )
      TBase::p()->buff.teachings_of_the_monastery->trigger( proc );
  }
};

struct blackout_kick_t : overwhelming_force_t<charred_passions_t<teachings_of_the_monastery_t<base_blackout_kick_t>>>
{
  blackout_kick_t( monk_t *player, std::string_view options_str )
    : base_t( player, "blackout_kick",
              ( player->specialization() == MONK_BREWMASTER ? player->baseline.brewmaster.blackout_kick
                                                            : player->baseline.monk.blackout_kick ) )
  {
    parse_options( options_str );

    ww_mastery       = true;
    may_combo_strike = true;
    CAST_DURING( SPINNING_CRANE_KICK_IDS );
  }

  void execute() override
  {
    base_t::execute();

    // BracketSim legacy compatibility: Swift Roundhouse stacks off Blackout
    // Kick and feeds the next Rising Sun Kick.
    if ( p()->legacy_azerite.swift_roundhouse.ok() )
      p()->buff.swift_roundhouse->trigger();

    if ( p()->buff.invoke_niuzao->check() )
      p()->pets.niuzao.active_pet()->stomp->execute();

    p()->buff.shuffle->trigger(
        timespan_t::from_seconds( p()->baseline.brewmaster.blackout_kick->effectN( 2 ).base_value() ) );

    p()->buff.blackout_combo->trigger();

    if ( !result_is_hit( execute_state->result ) )
      return;

    timespan_t reduction = 0_s;
    reduction += timespan_t::from_seconds( p()->talent.windwalker.sharp_reflexes->effectN( 1 ).base_value() );

    if ( p()->buff.zenith->up() )
    {
      if ( p()->talent.windwalker.obsidian_spiral->ok() )
        p()->resource_gain( RESOURCE_CHI, p()->talent.windwalker.obsidian_spiral_energize->effectN( 1 ).base_value() );

      if ( reduction != 0_s )
        reduction -= p()->talent.windwalker.zenith->effectN( 3 ).time_value();
    }

    p()->cooldown.rising_sun_kick->adjust( reduction );
    p()->cooldown.fists_of_fury->adjust( reduction );

    if ( p()->buff.combo_breaker->up() )
    {
      double rwk_chance = p()->talent.windwalker.rushing_wind_kick->effectN( 1 ).percent();
      if ( p()->rng().roll( rwk_chance ) )
      {
        p()->cooldown.rising_sun_kick->reset( true );
        p()->buff.rushing_wind_kick->trigger();
      }

      double eb_chance = p()->talent.windwalker.energy_burst->effectN( 1 ).percent();
      if ( p()->rng().roll( eb_chance ) )
        p()->resource_gain( RESOURCE_CHI, p()->talent.windwalker.energy_burst->effectN( 2 ).base_value(),
                            p()->gain.energy_burst );

      p()->buff.combo_breaker->decrement();
    }

    if ( p()->action.strength_of_the_black_ox.base )
      p()->action.strength_of_the_black_ox.base->execute();
  }

  void impact( action_state_t *s ) override
  {
    base_t::impact( s );

    unsigned eb_count = 1;
    if ( p()->talent.brewmaster.elusive_footwork->ok() && s->result == RESULT_CRIT )
    {
      eb_count += as<unsigned>( p()->talent.brewmaster.elusive_footwork->effectN( 2 ).base_value() );
      p()->proc.elusive_footwork_proc->occur();
    }

    // Legacy Azerite: Elusive Footwork stacks with the modern talent of the same
    // name - both grant extra Elusive Brawler stacks on a critical Blackout Kick.
    if ( p()->legacy_azerite.elusive_footwork.ok() && s->result == RESULT_CRIT )
      eb_count += as<unsigned>( p()->legacy_azerite.elusive_footwork.spell_ref().effectN( 2 ).base_value() );

    p()->buff.elusive_brawler->trigger( eb_count );

    // Legacy Azerite: Staggering Strikes clears a flat amount of Stagger.
    // BracketSim legacy compatibility: Staggering Strikes purifies Stagger, and
    // Stagger is Brewmaster's. The find_stagger() null check below is not
    // enough on its own - a Windwalker still took an access violation through
    // this path - so the spec is checked too. Nothing is lost: the trait's
    // purify half is meaningless without Stagger to purify.
    if ( p()->legacy_azerite.staggering_strikes.ok() &&
         p()->specialization() == MONK_BREWMASTER )
    {
      if ( auto* stagger = p()->find_stagger( "Stagger" ) )
        stagger->purify_flat( p()->legacy_azerite.staggering_strikes.value(), "legacy_staggering_strikes" );
    }

    if ( p()->talent.brewmaster.staggering_strikes->ok() )
    {
      double m = functions::missing_health_percentage_t( p() )(
          p()->talent.brewmaster.staggering_strikes->effectN( 3 ).percent() );

      p()->find_stagger( "Stagger" )
          ->purify_flat(
              s->composite_attack_power() * p()->talent.brewmaster.staggering_strikes->effectN( 2 ).percent() * m,
              "staggering_strikes" );
    }
  }
};

struct rushing_jade_wind_t : public monk_melee_attack_t
{
  struct tick_t : monk_melee_attack_t
  {
    tick_t( monk_t *player, std::string_view name, const spell_data_t *data )
      : monk_melee_attack_t( player, name, data )
    {
      background = dual   = true;
      aoe                 = -1;
      reduced_aoe_targets = player->talent.brewmaster.rushing_jade_wind->effectN( 1 ).base_value();

      if ( const auto &effect = player->talent.master_of_harmony.balanced_stratagem_physical->effectN( 1 );
           player->talent.master_of_harmony.balanced_stratagem->ok() )
        add_parse_entry( persistent_multiplier_effects )
            .set_buff( player->buff.balanced_stratagem_physical )
            .set_value( effect.percent() )
            .set_eff( &effect )
            .add_parse_callback( this, PARSE_CALLBACK_POST_EXECUTE, [ & ]( action_state_t * ) {
              p()->buff.balanced_stratagem_physical->consume( this );
            } );
    }
  };

  rushing_jade_wind_t( monk_t *player, std::string_view options_str )
    : monk_melee_attack_t( player, "rushing_jade_wind", player->talent.brewmaster.rushing_jade_wind )
  {
    parse_options( options_str );

    CAST_DURING( SPINNING_CRANE_KICK_IDS );

    tick_action = new tick_t( player, "rushing_jade_wind_tick",
                              player->talent.brewmaster.rushing_jade_wind->effectN( 1 ).trigger() );
    add_child( tick_action );
  }

  void execute() override
  {
    monk_melee_attack_t::execute();

    p()->buff.rushing_jade_wind->trigger();
  }
};

struct spinning_crane_kick_t : public monk_melee_attack_t
{
  struct tick_t : charred_passions_t<monk_melee_attack_t>
  {
    spinning_crane_kick_t *parent;

    tick_t( monk_t *player, const spell_data_t *data, spinning_crane_kick_t *parent )
      : charred_passions_t<monk_melee_attack_t>( player, "spinning_crane_kick_tick", data ), parent( parent )
    {
      dual = background   = true;
      aoe                 = -1;
      reduced_aoe_targets = player->baseline.monk.spinning_crane_kick->effectN( 1 ).base_value();
      ww_mastery          = true;
      ap_type             = attack_power_type::WEAPON_BOTH;

      if ( const auto &effect = player->talent.brewmaster.counterstrike->effectN( 1 ).trigger()->effectN( 1 );
           effect.ok() )
        add_parse_entry( persistent_multiplier_effects )
            .set_buff( player->buff.counterstrike )
            .set_value( effect.percent() )
            .set_eff( &effect );

      if ( const auto &effect = player->talent.master_of_harmony.balanced_stratagem_physical->effectN( 1 );
           player->talent.master_of_harmony.balanced_stratagem->ok() )
        add_parse_entry( persistent_multiplier_effects )
            .set_buff( player->buff.balanced_stratagem_physical )
            .set_value( effect.percent() )
            .set_eff( &effect );

      if ( const auto &effect = player->sets->set( MONK_WINDWALKER, MID2, B4 )->effectN( 1 ).trigger()->effectN( 2 );
           effect.ok() )
        add_parse_entry( persistent_multiplier_effects )
            .set_buff( player->buff.mid2_ww_4pc )
            .set_value( effect.percent() )
            .set_use_stacks( true )
            .set_eff( &effect );
    }

    result_amount_type report_amount_type( const action_state_t * ) const override
    {
      return result_amount_type::DMG_DIRECT;
    }

    // Legacy Azerite: Dance of Chi-Ji. The trait's value is the whole kick, so
    // it is divided across the four ticks - Battle for Azeroth did the same.
    double bonus_da( const action_state_t *s ) const override
    {
      double b = monk_melee_attack_t::bonus_da( s );

      if ( p()->legacy_azerite.dance_of_chiji.ok() && p()->buff.legacy_dance_of_chiji->check() )
        b += p()->legacy_azerite.dance_of_chiji.value() / 4;

      return b;
    }

    void impact( action_state_t *state ) override
    {
      monk_melee_attack_t::impact( state );

      parent->xuens_battlegear.insert( state->target->actor_spawn_index );
    }

    void execute() override
    {
      set_target( target_list()[ 0 ] );

      monk_melee_attack_t::execute();

      p()->buff.shuffle->trigger(
          timespan_t::from_seconds( p()->baseline.brewmaster.spinning_crane_kick_rank_2->effectN( 1 ).base_value() ) );
    }
  };

  struct jade_ignition_t : monk_spell_t
  {
    jade_ignition_t( monk_t *player )
      : monk_spell_t( player, "chi_explosion", player->talent.windwalker.jade_ignition->effectN( 1 ).trigger() )
    {
      dual = background = true;
      aoe               = -1;
    }
  };

  action_t *jade_ignition;
  std::unordered_set<int> xuens_battlegear;
  tick_t *spinning_crane_kick_tick;
  ground_aoe_params_t params;

  spinning_crane_kick_t( monk_t *player, std::string_view options_str )
    : monk_melee_attack_t(
          player, "spinning_crane_kick",
          ( player->specialization() == MONK_BREWMASTER ? player->baseline.brewmaster.spinning_crane_kick
                                                        : player->baseline.monk.spinning_crane_kick ) ),
      jade_ignition( nullptr ),
      xuens_battlegear(),
      spinning_crane_kick_tick( nullptr ),
      params()
  {
    parse_options( options_str );

    may_miss = may_dodge = may_parry = false;
    may_combo_strike                 = true;
    tick_zero                        = true;
    spinning_crane_kick_tick         = new tick_t( player, data().effectN( 1 ).trigger(), this );
    add_child( spinning_crane_kick_tick );

    params.duration( data().duration() )
        .n_pulses( 4 )
        .expiration_pulse( ground_aoe_params_t::NO_EXPIRATION_PULSE )
        .action( spinning_crane_kick_tick );

    if ( player->talent.windwalker.xuens_battlegear->ok() )
      params.expiration_callback(
          [ &, reduction = player->talent.windwalker.xuens_battlegear->effectN( 4 ).time_value(),
            max_targets =
                player->talent.windwalker.xuens_battlegear->effectN( 5 ).time_value() /
                player->talent.windwalker.xuens_battlegear->effectN( 4 ).time_value() ]( const action_state_t * ) {
            size_t reduction_count = std::min( xuens_battlegear.size(), as<size_t>( max_targets ) );
            p()->cooldown.fists_of_fury->adjust( -reduction * reduction_count, true );

            // Proc once per half second reduced
            for ( size_t i = 0; i < reduction_count; ++i )
              p()->proc.xuens_battlegear_sck_reduction->occur();

            xuens_battlegear.clear();
          } );

    interrupt_auto_attack = player->specialization() != MONK_WINDWALKER;
    if ( player->specialization() == MONK_BREWMASTER )
    {
      dot_behavior = DOT_EXTEND;
      CAST_DURING( SPINNING_CRANE_KICK_IDS );
    }

    if ( player->specialization() == MONK_WINDWALKER )
    {
      channeled    = true;
      dot_behavior = DOT_CLIP;
    }

    if ( player->talent.windwalker.jade_ignition->ok() )
    {
      jade_ignition = new jade_ignition_t( player );
      add_child( jade_ignition );
    }
  }

  bool ready() override
  {
    if ( p()->channeling && p()->channeling->id == id )
      return false;

    return monk_melee_attack_t::ready();
  }

  bool usable_moving() const override
  {
    return true;
  }

  void execute() override
  {
    set_target( player );

    params.target( p()->target )
        .start_time( sim->current_time() )
        .pulse_time( data().duration() * 0.25 * p()->cache.spell_haste() - 100_ms );

    make_event<ground_aoe_event_t>( *sim, p(), params, true );

    monk_melee_attack_t::execute();

    p()->buff.counterstrike->expire();
    if ( p()->buff.balanced_stratagem_physical )
      p()->buff.balanced_stratagem_physical->expire();
    p()->buff.mid2_ww_4pc->expire();

    if ( p()->specialization() == MONK_WINDWALKER )
    {
      if ( p()->buff.dance_of_chiji->up() )
      {
        p()->buff.dance_of_chiji->decrement();

        if ( p()->rng().roll( p()->talent.windwalker.sequenced_strikes->effectN( 1 ).percent() ) )
          p()->buff.combo_breaker->increment();  // increment is used to directly trigger without rolling chance
      }

      // Legacy Azerite: Dance of Chi-Ji is spent by the kick it empowered.
      if ( p()->buff.legacy_dance_of_chiji->up() )
        p()->buff.legacy_dance_of_chiji->decrement();

      p()->action.flurry_strikes->execute( flurry_strikes_t::WISDOM_OF_THE_WALL );
    }

    if ( jade_ignition )
      jade_ignition->execute();
  }

  // Legacy Azerite: Dance of Chi-Ji also cheapens the kick. Effect 3 is stored
  // as a negative number. Skipped while the modern buff is up, because that one
  // already makes the kick free and two discounts would go negative.
  double cost() const override
  {
    double c = monk_melee_attack_t::cost();

    if ( p()->legacy_azerite.dance_of_chiji.ok() && p()->buff.legacy_dance_of_chiji->check() &&
         !p()->buff.dance_of_chiji->check() )
      c += p()->buff.legacy_dance_of_chiji->data().effectN( 3 ).base_value();

    return std::max( 0.0, c );
  }

  void reset() override
  {
    monk_melee_attack_t::reset();
    xuens_battlegear.clear();
  }
};

struct fists_of_fury_t : monk_melee_attack_t
{
  struct tick_t : monk_melee_attack_t
  {
    tick_t( monk_t *player, std::string_view name = "fists_of_fury_damage" )
      : monk_melee_attack_t( player, name, player->talent.windwalker.fists_of_fury->effectN( 3 ).trigger() )
    {
      background = dual   = true;
      aoe                 = -1;
      full_amount_targets = 1;
      reduced_aoe_targets = player->talent.windwalker.fists_of_fury->effectN( 1 ).base_value();
      ww_mastery          = true;

      parse_effects( player->buff.momentum_boost_damage );
      if ( const auto &effect = player->talent.windwalker.momentum_boost->effectN( 1 ); effect.ok() )
        add_parse_entry( da_multiplier_effects )
            .set_value_func(
                [ & ]( double ) { return ( ( 1.0 / p()->composite_melee_haste() ) - 1.0 ) * effect.percent(); } )
            .set_eff( &effect );

      add_parse_entry( da_multiplier_effects )
          .set_value( player->talent.windwalker.fists_of_fury->effectN( 6 ).percent() - 1.0 )
          .set_func( [] { return false; } )
          .set_note( "Secondary Target Damage Modifier" )
          .set_eff( &player->talent.windwalker.fists_of_fury->effectN( 6 ) );

      // BracketSim legacy compatibility: the conduit Inner Fury (16) raises
      // Fists of Fury's damage. All of that damage is in this tick, not in the
      // channel that schedules it, so this is where Shadowlands'
      // action_multiplier() lands in Midnight's split of the ability.
      base_dd_multiplier *= 1.0 + player->legacy_conduits.percent( 16 );
    }

    double composite_aoe_multiplier( const action_state_t *state ) const override
    {
      double cam = monk_melee_attack_t::composite_aoe_multiplier( state );

      if ( state->chain_target )
        cam *= p()->talent.windwalker.fists_of_fury->effectN( 6 ).percent();

      return cam;
    }

    // Legacy Azerite: Open Palm Strikes
    double bonus_da( const action_state_t *s ) const override
    {
      double b = monk_melee_attack_t::bonus_da( s );

      if ( p()->legacy_azerite.open_palm_strikes.ok() )
        b += p()->legacy_azerite.open_palm_strikes.value( 4 );

      return b;
    }

    void execute() override
    {
      monk_melee_attack_t::execute();

      // Legacy Azerite: Open Palm Strikes refunds Chi at random.
      if ( p()->legacy_azerite.open_palm_strikes.ok() &&
           rng().roll( p()->legacy_azerite.open_palm_strikes.spell_ref().effectN( 2 ).percent() ) )
      {
        p()->resource_gain( RESOURCE_CHI,
                            p()->legacy_azerite.open_palm_strikes.spell_ref().effectN( 3 ).base_value(),
                            p()->gain.legacy_open_palm_strikes );
      }

      // Legacy Azerite: Iron Fists rewards catching enough targets.
      if ( p()->legacy_azerite.iron_fists.ok() &&
           as<double>( sim->target_non_sleeping_list.size() ) >=
               p()->legacy_azerite.iron_fists.spell_ref().effectN( 2 ).base_value() )
      {
        p()->buff.legacy_iron_fists->trigger();
      }

      p()->buff.mid2_ww_4pc->trigger();
    }

    void impact( action_state_t *state ) override
    {
      monk_melee_attack_t::impact( state );

      p()->buff.momentum_boost_damage->trigger();
    }
  };

  struct jadefire_stomp_t : monk_melee_attack_t
  {
    jadefire_stomp_t( monk_t *player )
      : monk_melee_attack_t( player, "jadefire_stomp", player->talent.windwalker.jadefire_stomp_damage )
    {
      aoe        = as<int>( player->talent.windwalker.jadefire_stomp->effectN( 2 ).base_value() );
      background = dual = true;
      ww_mastery        = true;

      if ( const auto &effect = player->talent.windwalker.path_of_jade->effectN( 1 ); effect.ok() )
        add_parse_entry( da_multiplier_effects )
            .set_value( effect.percent() )
            .set_func( [] { return false; } )
            .set_note( "Per-Target Increase" )
            .set_eff( &effect );
    }

    double composite_aoe_multiplier( const action_state_t *state ) const override
    {
      double cam = monk_melee_attack_t::composite_aoe_multiplier( state );

      if ( p()->talent.windwalker.path_of_jade->ok() )
      {
        double count =
            std::min( p()->talent.windwalker.path_of_jade->effectN( 2 ).base_value(), as<double>( state->n_targets ) );
        cam *= 1.0 + count * p()->talent.windwalker.path_of_jade->effectN( 1 ).percent();
      }

      return cam;
    }

    std::vector<player_t *> &target_list() const override
    {
      auto &tl = monk_melee_attack_t::target_list();

      p()->rng().shuffle( tl.begin(), tl.end() );

      return tl;
    }
  };

  action_t *jadefire_stomp;
  action_t *mid2_ww_tier;

  fists_of_fury_t( monk_t *player, std::string_view options_str )
    : monk_melee_attack_t( player, "fists_of_fury", player->talent.windwalker.fists_of_fury ),
      jadefire_stomp( nullptr ),
      mid2_ww_tier( nullptr )
  {
    parse_options( options_str );

    may_combo_strike        = true;
    attack_power_mod.direct = 0;
    channeled               = true;

    // expected base tick time is always 1_s
    // default: 5 ticks over 4s w/ tick 0, 4 / 4 = 1
    // crashing fists: 6 ticks over 5s w/ tick0, 5 / 5 = 1
    base_tick_time = 1_s;
    tick_action    = new tick_t( player );

    if ( player->talent.windwalker.jadefire_stomp->ok() )
    {
      jadefire_stomp = new jadefire_stomp_t( player );
      add_child( jadefire_stomp );
    }

    if ( player->sets->has_set_bonus( MONK_WINDWALKER, MID2, B2 ) )
    {
      mid2_ww_tier = new tick_t( player, "fists_of_fury_damage_mid2_2pc" );
      mid2_ww_tier->base_multiplier *= player->sets->set( MONK_WINDWALKER, MID2, B2 )->effectN( 1 ).percent();
      add_child( mid2_ww_tier );
    }
  }

  bool usable_moving() const override
  {
    return true;
  }

  void execute() override
  {
    // Legacy Azerite: Fury of Xuen rolls BEFORE the strike, as it did in Battle
    // for Azeroth, so the Haste applies to this cast rather than the next one.
    if ( p()->legacy_azerite.fury_of_xuen.ok() && p()->buff.legacy_fury_of_xuen_stacks->up() &&
         rng().roll( p()->buff.legacy_fury_of_xuen_stacks->stack_value() ) )
    {
      p()->buff.legacy_fury_of_xuen_haste->trigger();
      p()->pets.legacy_fury_of_xuen.spawn( p()->find_spell( 287063 )->duration(), 1 );
      p()->buff.legacy_fury_of_xuen_stacks->expire();
    }

    monk_melee_attack_t::execute();

    if ( mid2_ww_tier )
      mid2_ww_tier->execute_on_target( target );

    p()->action.flurry_strikes->execute( flurry_strikes_t::FLURRY_STRIKES );
    p()->buff.whirling_dragon_punch->trigger();
  }

  void last_tick( dot_t *dot ) override
  {
    monk_melee_attack_t::last_tick( dot );

    if ( jadefire_stomp )
      jadefire_stomp->execute();

    // TODO: is there a better way to do this?
    // Delay the expiration of the buffs until after the tick action happens.
    // Otherwise things trigger before the tick action happens; which is not intended.
    make_event( p()->sim, timespan_t::from_millis( 1 ), [ & ] {
      p()->buff.momentum_boost_damage->expire();
      p()->buff.momentum_boost_speed->trigger();
      p()->buff.tigereye_brew_3->expire();
    } );
  }
};

struct whirling_dragon_punch_t : public monk_melee_attack_t
{
  struct damage_t : monk_melee_attack_t
  {
    damage_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
      : monk_melee_attack_t( player, fmt::format( "whirling_dragon_punch_{}", name ), spell_data )
    {
      ww_mastery = true;
      background = dual = true;
    }

    using monk_melee_attack_t::execute;
    void execute( bool first )
    {
      monk_melee_attack_t::execute();

      if ( !first || !p()->talent.windwalker.thunderfist->ok() )
        return;

      unsigned count = as<unsigned>( p()->talent.windwalker.thunderfist->effectN( 1 ).base_value() );
      count += std::max( 0, num_targets_hit - 1 );
      p()->buff.thunderfist->trigger( count );
    }
  };

  damage_t *aoe;
  action_t *singletarget;

  whirling_dragon_punch_t( monk_t *player, std::string_view options_str )
    : monk_melee_attack_t( player, "whirling_dragon_punch", player->talent.windwalker.whirling_dragon_punch ),
      aoe( nullptr ),
      singletarget( nullptr )
  {
    parse_options( options_str );

    // ticks 0, 1, 2, but skips ticking on 4th tick. as a result, implmenting this
    // action using `tick_action` is nonviable. ticks must be scheduled manually.

    may_combo_strike = true;
    CAST_DURING( SPINNING_CRANE_KICK_IDS );

    aoe                      = new damage_t( player, "aoe", player->talent.windwalker.whirling_dragon_punch_aoe_tick );
    aoe->aoe                 = -1;
    aoe->reduced_aoe_targets = player->talent.windwalker.whirling_dragon_punch->effectN( 1 ).base_value();
    add_child( aoe );

    singletarget = new damage_t( player, "singletarget", player->talent.windwalker.whirling_dragon_punch_st_tick );
    add_child( singletarget );
  }

  void execute() override
  {
    monk_melee_attack_t::execute();

    singletarget->execute();

    // -1 to compensate for zero index, -1 to skip last tick
    aoe->target = target;
    for ( unsigned i = 0; i <= dot_duration / base_tick_time - 2.0; i++ )
      make_event<events::delayed_cb_event_t>( *p()->sim, p(), i * base_tick_time, [ i, this ] { aoe->execute( !i ); } );

    p()->buff.heart_of_the_jade_serpent->trigger();

    if ( const player_talent_t &talent = p()->talent.windwalker.knowledge_of_the_broken_temple; talent->ok() )
      p()->buff.teachings_of_the_monastery->trigger( as<unsigned>( talent->effectN( 1 ).base_value() ) );

    if ( p()->talent.windwalker.echo_technique->ok() )
      p()->buff.combo_breaker->increment();

    if ( p()->rng().roll( p()->talent.windwalker.revolving_whirl->effectN( 1 ).percent() ) )
      p()->buff.dance_of_chiji->increment();  // increment is used to not incur the rppm cooldown
  }

  bool ready() override
  {
    if ( p()->buff.whirling_dragon_punch->up() )
      return monk_melee_attack_t::ready();

    return false;
  }
};

struct strike_of_the_windlord_t : public monk_melee_attack_t
{
  struct damage_t : monk_melee_attack_t
  {
    slot_e slot;
    damage_t( monk_t *player, slot_e slot )
      : monk_melee_attack_t(
            player, fmt::format( "strike_of_the_windlord_{}", util::slot_type_string( slot ) ),
            player->talent.windwalker.strike_of_the_windlord->effectN( slot == SLOT_MAIN_HAND ? 3 : 4 ).trigger() ),
        slot( slot )
    {
      ww_mastery = true;
      dual = background = true;
      aoe               = -1;

      switch ( slot )
      {
        case SLOT_MAIN_HAND:
          ap_type = attack_power_type::WEAPON_MAINHAND;
          break;
        case SLOT_OFF_HAND:
          ap_type = attack_power_type::WEAPON_OFFHAND;
          break;
        default:
          assert( false );
      }
    }

    double composite_aoe_multiplier( const action_state_t *state ) const override
    {
      double cam = monk_melee_attack_t::composite_aoe_multiplier( state );

      if ( state->chain_target )
        cam /= state->n_targets;

      return cam;
    }

    void impact( action_state_t *state ) override
    {
      monk_melee_attack_t::impact( state );

      if ( slot != SLOT_OFF_HAND )
        return;

      if ( p()->talent.windwalker.thunderfist->ok() )
      {
        unsigned count = 1;

        if ( !state->chain_target )
          // The first target will trigger the 4 stacks of the Thunderfist buff, all others will trigger 1 stack
          count = as<unsigned>( p()->talent.windwalker.thunderfist->effectN( 1 ).base_value() );

        p()->buff.thunderfist->trigger( count );
      }
    }
  };

  // Off hand hits first followed by main hand
  // The ability does NOT require an off-hand weapon to be executed.
  // The ability uses the main-hand weapon damage for both attacks
  action_t *main_hand;
  action_t *off_hand;

  strike_of_the_windlord_t( monk_t *player, std::string_view options_str )
    : monk_melee_attack_t( player, "strike_of_the_windlord", player->talent.windwalker.strike_of_the_windlord ),
      main_hand( nullptr ),
      off_hand( nullptr )
  {
    may_combo_strike = true;
    CAST_DURING( SPINNING_CRANE_KICK_IDS );
    cooldown->hasted = false;
    trigger_gcd      = data().gcd();

    parse_options( options_str );

    main_hand = new damage_t( player, SLOT_MAIN_HAND );
    off_hand  = new damage_t( player, SLOT_OFF_HAND );

    add_child( main_hand );
    add_child( off_hand );
  }

  void execute() override
  {
    monk_melee_attack_t::execute();

    // Off-hand attack hits first
    off_hand->execute();

    if ( result_is_hit( off_hand->execute_state->result ) )
      main_hand->execute();

    p()->buff.heart_of_the_jade_serpent->trigger();

    if ( const player_talent_t &talent = p()->talent.windwalker.knowledge_of_the_broken_temple; talent->ok() )
      p()->buff.teachings_of_the_monastery->trigger( as<unsigned>( talent->effectN( 1 ).base_value() ) );

    if ( p()->talent.windwalker.echo_technique->ok() )
      p()->buff.combo_breaker->increment();

    if ( p()->rng().roll( p()->talent.windwalker.revolving_whirl->effectN( 1 ).percent() ) )
      p()->buff.dance_of_chiji->increment();
  }
};

struct auto_attack_t : public monk_melee_attack_t
{
  template <typename TBase>
  struct dual_threat_t : TBase
  {
    struct damage_t : public monk_spell_t
    {
      bool allowed;

      damage_t( monk_t *player, weapon_t *weapon )
        : monk_spell_t( player, "dual_threat", player->talent.windwalker.dual_threat_damage ), allowed( false )
      {
        background                = true;
        allow_class_ability_procs = false;
        may_miss                  = false;

        if ( weapon->group() == WEAPON_2H )
          add_parse_entry( da_multiplier_effects ).set_value( 3.6 / 2.6 * 2.0 - 1.0 ).set_note( "Two-hand adjustment" );
      }

      void reset() override
      {
        monk_spell_t::reset();
        allowed = false;
      }

      void impact( action_state_t *state ) override
      {
        monk_spell_t::impact( state );
        // We attribute all DT melees to MH weapon type, but it will be technically
        // accurate as you cannot equip a 2h in your offhand :)
        p()->buff.flurry_charge->trigger( state, &p()->main_hand_weapon );
      }
    };

    damage_t *damage;
    double chance;

    template <typename... Args>
    dual_threat_t( monk_t *player, weapon_t *weapon, Args &&...args )
      : TBase( player, weapon, std::forward<Args>( args )... ),
        damage( nullptr ),
        chance( player->talent.windwalker.dual_threat->effectN( 1 ).percent() )

    {
      if ( !player->talent.windwalker.dual_threat->ok() )
        return;

      if ( action_t *dt = player->find_action( "dual_threat" ); dt )
        damage = debug_cast<damage_t *>( dt );
      else
        damage = new damage_t( player, weapon );

      if ( action_t *aa = player->find_action( "auto_attack" ); aa )
        aa->add_child( damage );
    }

    void impact( action_state_t *state ) override
    {
      if ( damage && damage->allowed && result_is_hit( state->result ) && TBase::p()->rng().roll( chance ) )
      {
        damage->execute_on_target( state->target );
        damage->allowed = false;
      }
      else
      {
        TBase::impact( state );
        if ( damage )
          damage->allowed = true;
      }
    }
  };

  template <typename TBase>
  struct press_the_advantage_t : TBase
  {
    struct damage_t : public monk_spell_t
    {
      damage_t( monk_t *player )
        : monk_spell_t( player, "press_the_advantage", player->talent.brewmaster.press_the_advantage_damage )
      {
        background = dual = true;
      }
    };

    struct press_the_advantage_tiger_palm_t : public harmonic_surge_t<monk_melee_attack_t>
    {
      press_the_advantage_tiger_palm_t( monk_t *player )
        : base_t( player, "tiger_palm_press_the_advantage", player->talent.brewmaster.press_the_advantage_tiger_palm )
      {
        background = dual = true;

        if ( const auto &effect = player->buff.press_the_advantage->data().effectN( 2 ); effect.ok() )
          add_parse_entry( da_multiplier_effects ).set_value( effect.percent() ).set_eff( &effect );
      }
    };

    action_t *damage;
    action_t *tiger_palm;

    template <typename... Args>
    press_the_advantage_t( monk_t *player, weapon_t *weapon, Args &&...args )
      : TBase( player, weapon, std::forward<Args>( args )... ), damage( nullptr )
    {
      if ( !player->talent.brewmaster.press_the_advantage->ok() || weapon->slot != SLOT_MAIN_HAND )
        return;

      damage = new damage_t( player );
      TBase::add_child( damage );

      tiger_palm = new press_the_advantage_tiger_palm_t( player );
      TBase::add_child( tiger_palm );
    }

    void impact( action_state_t *state ) override
    {
      TBase::impact( state );

      if ( !damage || !tiger_palm || !result_is_hit( state->result ) )
        return;

      if ( TBase::p()->buff.press_the_advantage->stack() < 10 )
        TBase::p()->buff.press_the_advantage->trigger();
      else
      {
        TBase::p()->buff.press_the_advantage->expire();
        tiger_palm->execute_on_target( state->target );
      }

      TBase::p()->baseline.brewmaster.brews.adjust(
          TBase::p()->talent.brewmaster.press_the_advantage->effectN( 2 ).time_value() );
      damage->execute_on_target( state->target );
    }
  };

  template <typename TBase>
  struct thunderfist_t : TBase
  {
    struct damage_t : public monk_spell_t
    {
      damage_t( monk_t *player )
        : monk_spell_t( player, "thunderfist", player->talent.windwalker.thunderfist_buff->effectN( 1 ).trigger() )
      {
        background = dual = true;
        may_miss          = false;
      }
    };

    action_t *damage;

    template <typename... Args>
    thunderfist_t( monk_t *player, Args &&...args ) : TBase( player, std::forward<Args>( args )... ), damage( nullptr )
    {
      if ( !player->talent.windwalker.thunderfist->ok() )
        return;

      if ( action_t *tf = player->find_action( "thunderfist" ); tf )
        damage = tf;
      else
        damage = new damage_t( player );

      if ( action_t *wdp = TBase::player->find_action( "whirling_dragon_punch" ); wdp )
        wdp->add_child( damage );
      else if ( action_t *sotwl = TBase::player->find_action( "strike_of_the_windlord" ); sotwl )
        sotwl->add_child( damage );
    }

    void impact( action_state_t *state ) override
    {
      TBase::impact( state );

      if ( !damage || !result_is_hit( state->result ) || !TBase::p()->buff.thunderfist->up() )
        return;

      damage->execute_on_target( state->target );
      TBase::p()->buff.thunderfist->decrement();
    }
  };

  struct melee_t : public monk_melee_attack_t
  {
    bool first;
    bool sync_weapons;

    melee_t( monk_t *player, weapon_t *weapon, action_t *parent )
      : monk_melee_attack_t( player, fmt::format( "melee_{}", util::slot_type_string( weapon->slot ) ) ),
        first( true ),
        sync_weapons( false )
    {
      background = repeating = may_glance = true;
      may_crit = allow_class_ability_procs = not_a_proc = true;
      special                                           = false;
      trigger_gcd                                       = 0_ms;
      school                                            = SCHOOL_PHYSICAL;
      weapon_multiplier                                 = 1.0;

      switch ( weapon->slot )
      {
        case SLOT_MAIN_HAND:
          player->main_hand_attack = this;
          break;
        case SLOT_OFF_HAND:
          player->off_hand_attack = this;
          break;
        default:
          assert( false );
      }
      monk_melee_attack_t::weapon = weapon;
      base_execute_time           = weapon->swing_time;

      if ( player->main_hand_weapon.group() == WEAPON_1H )
        base_hit -= 0.19;

      parent->add_child( this );

      if ( const auto &effect = player->talent.monk.tiger_fang->effectN( 1 ); effect.ok() )
        add_parse_entry( crit_chance_effects ).set_value( effect.percent() ).set_eff( &effect );
    }

    void reset() override
    {
      monk_melee_attack_t::reset();
      first = true;
    }

    void execute() override
    {
      monk_melee_attack_t::execute();
      first = false;
    }

    void impact( action_state_t *state ) override
    {
      monk_melee_attack_t::impact( state );

      p()->buff.flurry_charge->trigger( state, weapon );
    }

    timespan_t execute_time() const override
    {
      timespan_t time = monk_melee_attack_t::execute_time();

      if ( !first )
        return time;

      if ( weapon->slot == SLOT_MAIN_HAND )
        return 0_ms;
      if ( first && weapon->slot == SLOT_OFF_HAND && sync_weapons )
        return 0_ms;
      if ( first && weapon->slot == SLOT_OFF_HAND && !sync_weapons )
        return time / 2;

      assert( false );
      return time;
    }
  };

  int sync_weapons;

  auto_attack_t( monk_t *player, std::string_view options_str )
    : monk_melee_attack_t( player, "auto_attack" ), sync_weapons( 0 )
  {
    add_option( opt_bool( "sync_weapons", sync_weapons ) );
    parse_options( options_str );

    trigger_gcd = 0_ms;

    // these pointers register themselves in places which cause them to get properly destructed
    new dual_threat_t<press_the_advantage_t<thunderfist_t<melee_t>>>( player, &player->main_hand_weapon, this );
    if ( player->off_hand_weapon.type != WEAPON_NONE )
      new dual_threat_t<press_the_advantage_t<thunderfist_t<melee_t>>>( player, &player->off_hand_weapon, this );
  }

  bool ready() override
  {
    if ( p()->current.distance_to_move > 5 )
      return false;

    // no execute event queued implies no swing ongoing
    return p()->main_hand_attack->execute_event == nullptr ||
           ( p()->off_hand_attack && p()->off_hand_attack->execute_event == nullptr );
  }

  void execute() override
  {
    if ( p()->main_hand_attack )
      p()->main_hand_attack->schedule_execute();
    if ( p()->off_hand_attack )
      p()->off_hand_attack->schedule_execute();

    monk_melee_attack_t::execute();
  }
};

struct keg_smash_t : monk_melee_attack_t
{
  struct empty_barrel_t : monk_spell_t
  {
    struct state_t : action_state_t
    {
      int count;

      state_t( action_t *a, player_t *t ) : action_state_t( a, t ), count( 0 )
      {
      }

      std::ostringstream &debug_str( std::ostringstream &s ) override
      {
        action_state_t::debug_str( s );
        fmt::print( s, " count={}", count );
        return s;
      }

      void initialize() override
      {
        action_state_t::initialize();
        count = 0;
      }

      void copy_state( const action_state_t *o ) override
      {
        action_state_t::copy_state( o );
        auto other = debug_cast<const state_t *>( o );
        count      = other->count;
      }
    };

    empty_barrel_t( monk_t *player )
      : monk_spell_t( player, "empty_barrel", player->talent.brewmaster.empty_barrel_damage )
    {
      background = dual = true;
      // aoe == 0 => state->chain_target == 0
      // as a result, chain_multiplier != 1 is ignored in default implementation
      aoe = 0;
    }

    double composite_da_multiplier( const action_state_t *state ) const override
    {
      double mul = monk_spell_t::composite_da_multiplier( state );

      auto chain_state = debug_cast<const state_t *>( state );
      mul *= pow( chain_multiplier, chain_state->count );

      return mul;
    }

    void impact( action_state_t *state ) override
    {
      monk_spell_t::impact( state );

      auto &tl = target_list();
      if ( tl.size() == 1 )
        return;

      if ( debug_cast<state_t *>( state )->count + 1 == data().effectN( 1 ).chain_target() )
        return;

      auto chain_state = debug_cast<state_t *>( get_state( state ) );
      chain_state->count += 1;
      chain_state->target = tl[ chain_state->count % tl.size() ];

      snapshot_state( chain_state, amount_type( chain_state ) );
      schedule_execute( chain_state );
    }

    action_state_t *new_state() override
    {
      return new state_t( this, target );
    }
  };

  struct extra_kick_t : monk_spell_t
  {
    extra_kick_t( monk_t *player ) : monk_spell_t( player, "extra_kick", player->tier.mid1.brm_4pc_extra_kick )
    {
      background = dual = true;
      aoe               = -1;
    }

    void impact( action_state_t *state ) override
    {
      if ( !get_td( state->target )->dot.breath_of_fire->is_ticking() )
        return;

      monk_spell_t::impact( state );
    }
  };

  struct fuel_on_the_fire_t : public monk_spell_t
  {
    fuel_on_the_fire_t( monk_t *player )
      : monk_spell_t( player, "fuel_on_the_fire", player->talent.brewmaster.fuel_on_the_fire_damage )
    {
      aoe        = -1;
      background = dual = true;
    }

    void impact( action_state_t *s ) override
    {
      // A "whirl of flame which spirals outwards" has a pretty high chance to hit, but doesn't always.
      // TODO: Improve hit chance calculation from real world data
      if ( rng().roll( 0.75 ) )
        monk_spell_t::impact( s );
    }
  };

  struct mid2_brm_2pc_t : monk_spell_t
  {
    mid2_brm_2pc_t( monk_t *player ) : monk_spell_t( player, "fiery_shrapnel", player->tier.mid2.brm_2pc_damage )
    {
      aoe                 = -1;
      full_amount_targets = 1;
      background = dual = true;
    }

    void execute() override
    {
      if ( !p()->buff.mid2_brm_2pc->up() )
        return;

      p()->buff.mid2_brm_2pc->expire();
      monk_spell_t::execute();
    }

    void impact( action_state_t *state ) override
    {
      monk_spell_t::impact( state );

      get_td( state->target )->debuff.mid2_brm_4pc->trigger();
    }
  };

  struct mid2_brm_4pc_t : monk_spell_t
  {
    action_t *damage;
    ground_aoe_params_t params;

    struct damage_t : monk_spell_t
    {
      damage_t( monk_t *player ) : monk_spell_t( player, "aflame", player->tier.mid2.brm_4pc_damage )
      {
        aoe        = -1;
        background = dual = true;
      }
    };

    mid2_brm_4pc_t( monk_t *player, action_t *parent )
      : monk_spell_t( player, "in_flames", player->tier.mid2.brm_4pc_action ), params()
    {
      damage = new damage_t( player );

      params.duration( data().duration() );
      params.pulse_time( 1_s );
      params.expiration_pulse( ground_aoe_params_t::FULL_EXPIRATION_PULSE );
      params.action( damage );

      parent->add_child( damage );
    }

    void execute() override
    {
      monk_spell_t::execute();

      params.target( p()->target );
      make_event<ground_aoe_event_t>( *sim, p(), params, true );
    }
  };

  cooldown_t *breath_of_fire;
  action_t *empty_barrel;
  action_t *extra_kick;
  action_t *fuel_on_the_fire;
  action_t *mid2_brm_2pc;
  action_t *mid2_brm_4pc;

  keg_smash_t( monk_t *player, std::string_view options_str, std::string_view name = "keg_smash" )
    : monk_melee_attack_t( player, name, player->talent.brewmaster.keg_smash ),
      breath_of_fire( nullptr ),
      empty_barrel( nullptr ),
      extra_kick( nullptr ),
      fuel_on_the_fire( nullptr ),
      mid2_brm_2pc( nullptr ),
      mid2_brm_4pc( nullptr )
  {
    parse_options( options_str );
    CAST_DURING( SPINNING_CRANE_KICK_IDS );

    full_amount_targets = 1;
    reduced_aoe_targets = data().effectN( 7 ).base_value();
    aoe                 = -1;

    // BracketSim legacy compatibility: Stormstout's Last Keg. Effect 1 of the
    // runeforge is how much harder Keg Smash hits and effect 2 is the extra
    // charge, both straight off spell 337288.
    if ( player->shadowlands_legacy.stormstouts_last_keg )
    {
      auto rune = player->find_spell( 337288 );
      base_multiplier *= 1.0 + rune->effectN( 1 ).percent();
      cooldown->charges += as<int>( rune->effectN( 2 ).base_value() );
    }

    // scalding brew is scripted
    if ( const auto &effect = player->talent.brewmaster.scalding_brew->effectN( 1 ); effect.ok() )
      add_parse_entry( target_multiplier_effects )
          .set_func( td_fn( &monk_td_t::dots_t::breath_of_fire ) )
          .set_value( effect.percent() )
          .set_eff( &effect );

    // BracketSim legacy compatibility: the Scalding Brew conduit works on the
    // same Breath of Fire window as the talent it later became, but it has to be
    // its own entry - putting it inside the talent's ok() guard would silently
    // require the talent to be taken as well.
    if ( player->legacy_conduits.has( 46 ) )
      add_parse_entry( target_multiplier_effects )
          .set_func( td_fn( &monk_td_t::dots_t::breath_of_fire ) )
          .set_value( player->legacy_conduits.percent( 46 ) );

    // increased damage to primary target
    if ( const auto &effect = data().effectN( 8 ); effect.ok() )
      add_parse_entry( target_multiplier_effects )
          .set_func( [ this ]( actor_target_data_t *target_data ) { return target_data->target == target; } )
          .set_value( effect.percent() )
          .set_eff( &effect )
          .set_note( "Primary Target" );

    // SSLK increased damage to primary target
    if ( const auto &effect = player->talent.brewmaster.stormstouts_last_keg->effectN( 1 ); effect.ok() )
      add_parse_entry( target_multiplier_effects )
          .set_func( [ this ]( actor_target_data_t *target_data ) { return target_data->target == target; } )
          .set_value( effect.percent() )
          .set_eff( &effect )
          .set_note( "Primary Target" );

    if ( player->talent.brewmaster.salsalabims_strength->ok() )
      breath_of_fire = player->get_cooldown( "breath_of_fire" );

    if ( player->talent.brewmaster.bring_me_another_1->ok() )
    {
      empty_barrel = new empty_barrel_t( player );
      add_child( empty_barrel );
    }

    if ( player->tier.mid1.brm_4pc->ok() )
    {
      extra_kick = new extra_kick_t( player );
      add_child( extra_kick );
    }

    if ( player->talent.brewmaster.fuel_on_the_fire->ok() )
    {
      fuel_on_the_fire = new fuel_on_the_fire_t( player );
      add_child( fuel_on_the_fire );
    }

    if ( player->sets->set( MONK_BREWMASTER, MID2, B2 ) )
    {
      mid2_brm_2pc = new mid2_brm_2pc_t( player );
      add_child( mid2_brm_2pc );
    }

    if ( player->sets->set( MONK_BREWMASTER, MID2, B4 ) && player->tier.mid2.brm_4pc_damage->ok() )
      mid2_brm_4pc = new mid2_brm_4pc_t( player, this );
  }

  void execute() override
  {
    if ( mid2_brm_2pc )
      mid2_brm_2pc->execute();
    if ( mid2_brm_4pc )
      mid2_brm_4pc->execute();

    monk_melee_attack_t::execute();

    if ( breath_of_fire )
    {
      breath_of_fire->reset( true );
      p()->proc.salsalabims_strength->occur();
    }

    p()->action.flurry_strikes->execute( flurry_strikes_t::FLURRY_STRIKES );
    p()->buff.shuffle->trigger( timespan_t::from_seconds( data().effectN( 6 ).base_value() ) );

    // BracketSim legacy compatibility: Walk with the Ox pulls Invoke Niuzao in
    // whenever Shuffle is refreshed and Niuzao is on cooldown. The 0.5 s is
    // effect 2 of conduit spell 337264, absent from Midnight and read from the
    // archived 9.2.7 client data.
    if ( p()->legacy_conduits.has( 57 ) )
    {
      cooldown_t* niuzao = p()->get_cooldown( "invoke_niuzao_the_black_ox" );
      if ( niuzao->down() )
        niuzao->adjust( timespan_t::from_millis( -500 ), true );
    }

    timespan_t reduction = timespan_t::from_seconds( data().effectN( 4 ).base_value() );
    if ( p()->buff.blackout_combo->up() )
    {
      reduction += timespan_t::from_seconds( p()->buff.blackout_combo->data().effectN( 3 ).base_value() );
      p()->proc.blackout_combo_keg_smash->occur();

      reduction -= p()->talent.master_of_harmony.meditative_focus->effectN( 3 ).time_value();
    }
    p()->buff.blackout_combo->expire();
    p()->baseline.brewmaster.brews.adjust( reduction );
    p()->action.chi_wave->execute();

    if ( p()->talent.master_of_harmony.potential_energy->ok() )
      p()->buff.harmonic_surge->trigger();

    if ( p()->buff.empty_barrel->up() )
    {
      p()->buff.empty_barrel->expire();
      empty_barrel->execute_on_target( target );
    }

    if ( extra_kick )
      extra_kick->execute();

    if ( p()->buff.fuel_on_the_fire->up() )
    {
      p()->buff.fuel_on_the_fire->decrement();

      make_event<ground_aoe_event_t>( *sim, p(),
                                      ground_aoe_params_t()
                                          .target( target )
                                          .duration( data().duration() )
                                          .pulse_time( timespan_t::from_seconds( 1.0 ) )
                                          .n_pulses( 3 )
                                          .action( fuel_on_the_fire ),
                                      true );
    }
  }

  void impact( action_state_t *state ) override
  {
    monk_melee_attack_t::impact( state );
    get_td( state->target )->debuff.keg_smash->trigger();
  }
};

struct stomp_t : monk_melee_attack_t
{
  stomp_t( monk_t *player ) : monk_melee_attack_t( player, "stomp", player->talent.brewmaster.walk_with_the_ox_stomp )
  {
    background = true;
    aoe        = -1;
  }
};

struct touch_of_death_t : public monk_melee_attack_t
{
  touch_of_death_t( monk_t *player, std::string_view options_str )
    : monk_melee_attack_t( player, "touch_of_death", player->baseline.monk.touch_of_death )
  {
    ww_mastery = true;
    may_crit = hasted_ticks = false;
    may_combo_strike        = true;
    CAST_DURING( SPINNING_CRANE_KICK_IDS );
    ignores_armor = true;  // instead use the trick to have no multipliers apply?
    parse_options( options_str );

    cooldown->duration = data().cooldown();

    // BracketSim legacy compatibility: Fatal Touch shortens Touch of Death.
    if ( p()->shadowlands_legacy.fatal_touch )
      cooldown->duration += timespan_t::from_millis(
          p()->find_spell( 337296 )->effectN( 1 ).base_value() );
  }

  void init() override
  {
    monk_melee_attack_t::init();

    snapshot_flags = update_flags = 0;
  }

  bool target_ready( player_t *target ) override
  {
    // Deals damage equal to 35% of your maximum health against players and stronger creatures under 15% health
    // 2023-10-19 Tooltip lies and the 15% health works on all non-player targets.
    if ( p()->talent.monk.improved_touch_of_death->ok() &&
         ( target->health_percentage() < p()->talent.monk.improved_touch_of_death->effectN( 1 ).base_value() ) )
      return monk_melee_attack_t::target_ready( target );

    // You exploit the enemy target's weakest point, instantly killing creatures if they have less health than you
    // Only applicable in health based sims
    if ( target->current_health() > 0 && target->current_health() <= p()->max_health() )
      return monk_melee_attack_t::target_ready( target );

    return false;
  }

  void execute() override
  {
    monk_melee_attack_t::execute();

    p()->buff.touch_of_death_ww->trigger();
  }

  void impact( action_state_t *s ) override
  {
    double max_hp, amount;

    // In execute range ToD deals player's max HP
    amount = max_hp = p()->max_health();

    // Not in execute range
    // or not a health-based fight style
    // or a secondary target... these always get hit for the 35% from Improved Touch of Death regardless if you're
    // talented into it or not
    if ( s->chain_target > 0 || target->current_health() == 0 || target->current_health() > max_hp )
    {
      amount *= p()->talent.monk.improved_touch_of_death->effectN( 2 ).percent();  // 0.35

      // Apply and parsed multipliers
      amount *= base_dd_multiplier;

      // Damage is only affected by Windwalker's Mastery
      // Versatility does not affect the damage of Touch of Death.
      if ( p()->buff.combo_strikes->up() )
        amount *= 1 + p()->cache.mastery_value();
    }

    s->result_total = s->result_raw = amount;

    monk_melee_attack_t::impact( s );

    if ( p()->talent.monk.stagger->ok() )
    {
      p()->find_stagger( "Stagger" )
          ->purify_flat( amount * p()->baseline.brewmaster.touch_of_death_rank_3->effectN( 1 ).percent(),
                         "touch_of_death" );
    }
  }
};

struct touch_of_karma_dot_t : public residual_action::residual_periodic_action_t<spell_t>
{
  using base_t = residual_action::residual_periodic_action_t<spell_t>;
  touch_of_karma_dot_t( monk_t *player )
    : base_t( "touch_of_karma", player, player->baseline.windwalker.touch_of_karma_tick )
  {
    may_miss = may_crit = false;
    dual                = true;
    proc                = true;
    ap_type             = attack_power_type::NO_WEAPON;
  }

  // Need to disable multipliers in init() so that it doesn't double-dip on anything
  void init() override
  {
    base_t::init();
    // disable the snapshot_flags for all multipliers
    snapshot_flags = update_flags = 0;
    snapshot_flags |= STATE_VERSATILITY;
  }
};

struct touch_of_karma_t : public monk_melee_attack_t
{
  // When Touch of Karma (ToK) is activated, two spells are placed. A buff on the player (id: 125174), and a
  // debuff on the target (id: 122470). Whenever the player takes damage, a dot (id: 124280) is placed on
  // the target that increases as the player takes damage. Each time the player takes damage, the dot is refreshed
  // and recalculates the dot size based on the current dot size. Just to make it easier to code, I'll wait until
  // the Touch of Karma buff expires before placing a dot on the target. Net result should be the same.

  // 8.1 Good Karma - If the player still has the ToK buff on them, each time the target hits the player, the amount
  // absorbed is immediatly healed by the Good Karma spell (id: 285594)
  double interval;
  double interval_stddev;
  double interval_stddev_opt;
  double pct_health;
  touch_of_karma_dot_t *touch_of_karma_dot;

  touch_of_karma_t( monk_t *player, std::string_view options_str )
    : monk_melee_attack_t( player, "touch_of_karma", player->baseline.windwalker.touch_of_karma ),
      interval( 100 ),
      interval_stddev( 0.05 ),
      interval_stddev_opt( 0 ),
      pct_health( 0.5 ),
      touch_of_karma_dot( new touch_of_karma_dot_t( player ) )
  {
    add_option( opt_float( "interval", interval ) );
    add_option( opt_float( "interval_stddev", interval_stddev_opt ) );
    add_option( opt_float( "pct_health", pct_health ) );
    parse_options( options_str );

    cooldown->duration = data().cooldown();
    base_dd_min = base_dd_max = 0;
    ap_type                   = attack_power_type::NO_WEAPON;
    CAST_DURING( SPINNING_CRANE_KICK_IDS );

    double max_pct = data().effectN( 3 ).percent();

    if ( pct_health > max_pct )  // Does a maximum of 50% of the monk's HP.
      pct_health = max_pct;

    if ( interval < cooldown->duration.total_seconds() )
    {
      sim->error( "{} minimum interval for Touch of Karma is 90 seconds.", *player );
      interval = cooldown->duration.total_seconds();
    }

    if ( interval_stddev_opt < 1 )
      interval_stddev = interval * interval_stddev_opt;
    // >= 1 seconds is used as a standard deviation normally
    else
      interval_stddev = interval_stddev_opt;

    trigger_gcd = timespan_t::zero();
    may_crit = may_miss = may_dodge = may_parry = false;
  }

  // Need to disable multipliers in init() so that it doesn't double-dip on anything
  void init() override
  {
    monk_melee_attack_t::init();
    // disable the snapshot_flags for all multipliers
    snapshot_flags = update_flags = 0;
  }

  bool target_ready( player_t *target ) override
  {
    if ( target->name_str == "Target Dummy" )
      return false;

    return monk_melee_attack_t::target_ready( target );
  }

  void execute() override
  {
    timespan_t new_cd        = timespan_t::from_seconds( rng().gauss( interval, interval_stddev ) );
    timespan_t data_cooldown = data().cooldown();
    if ( new_cd < data_cooldown )
      new_cd = data_cooldown;

    cooldown->duration = new_cd;

    monk_melee_attack_t::execute();

    if ( pct_health > 0 )
    {
      double damage_amount = pct_health * player->max_health();

      // Redirects 70% of the damage absorbed
      damage_amount *= data().effectN( 4 ).percent();

      residual_action::trigger( touch_of_karma_dot, execute_state->target, damage_amount );
    }
  }
};

struct flying_serpent_kick_t : public monk_melee_attack_t
{
  bool first_charge;
  flying_serpent_kick_t( monk_t *player, std::string_view options_str )
    : monk_melee_attack_t( player, "flying_serpent_kick", player->baseline.windwalker.flying_serpent_kick ),
      first_charge( true )
  {
    parse_options( options_str );
    may_crit              = true;
    ww_mastery            = true;
    ignore_false_positive = true;
    aoe                   = -1;
  }

  void reset() override
  {
    monk_melee_attack_t::reset();
    first_charge = true;
  }

  bool ready() override
  {
    if ( p()->talent.windwalker.slicing_winds->ok() )
      return false;
    if ( first_charge )  // Assumes that we fsk into combat, instead of setting initial distance to 20 yards.
      return monk_melee_attack_t::ready();

    return monk_melee_attack_t::ready();
  }

  void execute() override
  {
    monk_melee_attack_t::execute();

    if ( first_charge )
      first_charge = !first_charge;
  }
};

struct slicing_winds_t : public monk_melee_attack_t
{
  // TODO: Potentially add the "empowered" channel. "Empowered" channel increases
  // the "lunge" or movement of the attack, but also delays the actual attack by
  // as little as 500 milliseconds. Just pressing the attack; without the
  // "empowered" channel, animation lasts 1 second.
  // Empowered channel has 4 stages; with each lasting about 350 milliseconds.
  struct damage_t : monk_melee_attack_t
  {
    damage_t( monk_t *player )
      : monk_melee_attack_t( player, "slicing_winds_damage", player->talent.windwalker.slicing_winds_damage )
    {
      background = dual   = true;
      ww_mastery          = true;
      may_combo_strike    = true;
      aoe                 = -1;
      reduced_aoe_targets = player->talent.windwalker.slicing_winds->effectN( 3 ).base_value();
    }
  };

  slicing_winds_t( monk_t *player, std::string_view options_str )
    : monk_melee_attack_t( player, "slicing_winds", player->talent.windwalker.slicing_winds )
  {
    parse_options( options_str );

    execute_action = new damage_t( player );
    add_child( execute_action );

    // override gcd as we are not properly handling it as an empowered channel
    trigger_gcd = timespan_t::from_millis( 1400 );

    if ( player->talent.windwalker.airborne_rhythm->ok() )
      parse_effect_data( player->talent.windwalker.airborne_rhythm_energize->effectN( 1 ) );
  }
};

struct chi_wave_t : public monk_spell_t
{
  template <class TBase>
  struct bounce_t : TBase
  {
    using TBase::execute;
    std::function<void( unsigned )> other_cb;
    std::function<void( unsigned )> this_cb;
    unsigned count;

    bounce_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
      : TBase( player, fmt::format( "chi_wave_{}", name ), spell_data ), count( 0 )
    {
      TBase::dual         = true;
      TBase::travel_speed = player->talent.monk.chi_wave_driver->missile_speed();
      this_cb             = [ this ]( unsigned new_count ) { this->execute( new_count ); };
    }

    void execute( unsigned new_count )
    {
      count = new_count;
      if ( count > TBase::p()->talent.monk.chi_wave_driver->effectN( 1 ).base_value() )
        return;

      TBase::execute();
    }

    void impact( action_state_t *state ) override
    {
      TBase::impact( state );
      other_cb( ++count );
    }
  };

  bounce_t<monk_heal_t> *heal;
  bounce_t<monk_spell_t> *damage;

  chi_wave_t( monk_t *player )
    : monk_spell_t( player, "chi_wave", player->talent.monk.chi_wave_driver ),
      heal( new bounce_t<monk_heal_t>( player, "heal", player->talent.monk.chi_wave_heal ) ),
      damage( new bounce_t<monk_spell_t>( player, "damage", player->talent.monk.chi_wave_damage ) )
  {
    background = true;

    heal->other_cb   = damage->this_cb;
    damage->other_cb = heal->this_cb;

    add_child( damage );
    add_child( heal );
  }

  void execute() override
  {
    if ( !p()->buff.chi_wave->up() )
      return;
    p()->buff.aspect_of_harmony.trigger_path_of_resurgence();
    p()->buff.chi_wave->expire();
    monk_spell_t::execute();

    if ( player->target->is_enemy() )
      damage->execute( 0 );
    else
      heal->execute( 0 );
  }
};

struct chi_burst_t : monk_spell_t
{
  template <class TBase>
  struct hit_t : TBase
  {
    hit_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
      : TBase( player, fmt::format( "chi_burst_{}", name ), spell_data )
    {
      TBase::background = TBase::dual = true;
      TBase::reduced_aoe_targets      = player->talent.monk.chi_burst_projectile->effectN( 1 ).base_value();
      TBase::aoe                      = -1;

      // TODO: Helper to check if a damaging effect exists on the passed spell
      for ( const auto &effect : spell_data->effects() )
        if ( effect.type() == E_SCHOOL_DAMAGE )
          TBase::ww_mastery = true;

      if ( const auto &effect = player->talent.master_of_harmony.balanced_stratagem_magic->effectN( 1 );
           player->talent.master_of_harmony.balanced_stratagem->ok() )
        add_parse_entry( TBase::da_multiplier_effects )
            .set_buff( player->buff.balanced_stratagem_magic )
            .set_value( effect.percent() )
            .set_eff( &effect )
            .add_parse_callback( this, PARSE_CALLBACK_POST_EXECUTE, [ & ]( action_state_t * ) {
              TBase::p()->buff.balanced_stratagem_magic->consume( this );
            } );
    }
  };

  hit_t<monk_spell_t> *damage;
  hit_t<monk_heal_t> *heal;
  propagate_const<buff_t *> buff;

  // TODO: Figure out what you have to do to simulate this as a projectile.
  chi_burst_t( monk_t *player, std::string_view options_str )
    : monk_spell_t( player, "chi_burst",
                    player->specialization() == MONK_WINDWALKER ? player->talent.monk.chi_burst_projectile
                                                                : player->talent.monk.chi_burst ),
      damage( new hit_t<monk_spell_t>( player, "damage", player->talent.monk.chi_burst_damage ) ),
      heal( new hit_t<monk_heal_t>( player, "heal", player->talent.monk.chi_burst_heal ) ),
      buff( buff_t::find( player, "chi_burst" ) )
  {
    parse_options( options_str );

    stats = damage->stats;
    add_child( heal );
  }

  bool ready() override
  {
    if ( p()->specialization() != MONK_WINDWALKER )
      return monk_spell_t::ready();
    if ( buff && buff->up() )
      return monk_spell_t::ready();
    return false;
  }

  void execute() override
  {
    p()->buff.aspect_of_harmony.trigger_path_of_resurgence();

    if ( buff )
    {
      if ( p()->bugs )
        buff->expire();
      else
        buff->decrement();
    }

    damage->execute();
    heal->execute();

    // Defer consumption of buffs in `base_t::execute` until after the damage
    // and heal are executed
    monk_spell_t::execute();
  }
};

struct special_delivery_t : public monk_spell_t
{
  struct celestial_flames_t : public monk_spell_t
  {
    celestial_flames_t( monk_t *player )
      : monk_spell_t( player, "celestial_flames", player->talent.brewmaster.celestial_flames_damage )
    {
      background = dual = true;
      aoe               = as<int>( player->talent.brewmaster.celestial_flames->effectN( 2 ).base_value() );
    }
  };

  action_t *celestial_flames;

  special_delivery_t( monk_t *player )
    : monk_spell_t( player, "special_delivery",
                    player->talent.brewmaster.special_delivery_missile->effectN( 1 ).trigger() )
  {
    background   = true;
    travel_delay = player->talent.brewmaster.special_delivery_missile->missile_speed();
    aoe          = -1;

    if ( player->talent.brewmaster.celestial_flames->ok() )
    {
      celestial_flames = new celestial_flames_t( player );
      add_child( celestial_flames );
    }
  }

  void execute() override
  {
    if ( !p()->talent.brewmaster.special_delivery->ok() )
      return;
    monk_spell_t::execute();

    if ( p()->buff.celestial_flames->up() )
      celestial_flames->execute();
  }
};

struct black_ox_brew_t : public brew_t<monk_spell_t>
{
  black_ox_brew_t( monk_t *player, std::string_view options_str )
    : brew_t<monk_spell_t>( player, "black_ox_brew", player->talent.brewmaster.black_ox_brew )
  {
    parse_options( options_str );

    harmful       = false;
    use_off_gcd   = true;
    energize_type = action_energize::NONE;  // disable resource gain from spell data
  }

  void execute() override
  {
    brew_t<monk_spell_t>::execute();

    for ( auto &[ key, cooldown ] : p()->baseline.brewmaster.brews.cooldowns )
    {
      if ( key == p()->talent.brewmaster.purifying_brew->id() )
        cooldown->reset( true, 2 );
      if ( key == p()->talent.brewmaster.celestial_brew->id() )
        cooldown->reset( true, 1 );
      if ( key == p()->talent.brewmaster.celestial_infusion->id() )
        cooldown->reset( true, 1 );
    }

    p()->resource_gain( RESOURCE_ENERGY, p()->talent.brewmaster.black_ox_brew->effectN( 1 ).base_value(),
                        p()->gain.black_ox_brew_energy );
  }
};

struct crackling_jade_lightning_t : public monk_spell_t
{
  struct aoe_dot_t : public monk_spell_t
  {
    aoe_dot_t( monk_t *player )
      : monk_spell_t( player, "crackling_jade_lightning_aoe", player->baseline.monk.crackling_jade_lightning )
    {
      dual = background = true;
      ww_mastery        = true;
    }

    double cost_per_tick( resource_e ) const override
    {
      return 0.0;
    }
  };

  aoe_dot_t *aoe_dot;

  crackling_jade_lightning_t( monk_t *player, std::string_view options_str )
    : monk_spell_t( player, "crackling_jade_lightning", player->baseline.monk.crackling_jade_lightning ),
      aoe_dot( nullptr )
  {
    parse_options( options_str );

    may_combo_strike      = true;
    ww_mastery            = true;
    interrupt_auto_attack = true;
    channeled             = true;

    min_gcd = timespan_t::from_millis( 750 );

    if ( player->talent.brewmaster.jade_flash->ok() )
    {
      aoe_dot = new aoe_dot_t( player );
      add_child( aoe_dot );
    }

    if ( const auto &effect = player->talent.master_of_harmony.balanced_stratagem_magic->effectN( 1 );
         player->talent.master_of_harmony.balanced_stratagem->ok() )
      add_parse_entry( persistent_multiplier_effects )
          .set_buff( player->buff.balanced_stratagem_magic )
          .set_value( effect.percent() )
          .set_eff( &effect )
          .add_parse_callback( this, PARSE_CALLBACK_POST_EXECUTE,
                               [ & ]( action_state_t * ) { p()->buff.balanced_stratagem_magic->consume( this ); } );
  }

  void execute() override
  {
    monk_spell_t::execute();

    if ( aoe_dot )
    {
      auto &tl = target_list();
      range::erase_remove( tl, [ this ]( const auto &t ) { return t == target; } );
      size_t count = std::min( tl.size(), as<size_t>( p()->talent.brewmaster.jade_flash->effectN( 2 ).base_value() ) );
      for ( size_t i = 0; i < count; ++i )
        aoe_dot->execute_on_target( tl[ i ] );
    }
  }

  void last_tick( dot_t *dot ) override
  {
    monk_spell_t::last_tick( dot );

    // delay expiration so it occurs after final tick of cjl aoe
    if ( aoe_dot )
      make_event<events::delayed_cb_event_t>( *sim, p(), 1_ms, [ & ]() {
        // buff expire
        const auto &tl = target_list();
        for ( const auto &t : tl )
          get_td( t )->dot.crackling_jade_lightning_aoe->cancel();
      } );

    // Reset swing timer
    if ( player->main_hand_attack )
    {
      player->main_hand_attack->cancel();
      if ( !player->main_hand_attack->target->is_sleeping() )
      {
        player->main_hand_attack->schedule_execute();
      }
    }

    if ( player->off_hand_attack )
    {
      player->off_hand_attack->cancel();
      if ( !player->off_hand_attack->target->is_sleeping() )
      {
        player->off_hand_attack->schedule_execute();
      }
    }
  }
};

struct breath_of_fire_t : public monk_spell_t
{
  struct dot_t : public monk_spell_t
  {
    dot_t( monk_t *player ) : monk_spell_t( player, "breath_of_fire_dot", player->talent.brewmaster.breath_of_fire_dot )
    {
      background    = true;
      tick_may_crit = may_crit = true;
      hasted_ticks             = false;

      // Balanced Stratagem is consumed by the parent action, but still present when the dot is executed
      if ( const auto &effect = player->talent.master_of_harmony.balanced_stratagem_magic->effectN( 1 );
           player->talent.master_of_harmony.balanced_stratagem->ok() )
        add_parse_entry( persistent_multiplier_effects )
            .set_buff( player->buff.balanced_stratagem_magic )
            .set_value( effect.percent() )
            .set_eff( &effect );
    }

    // Legacy Azerite: Boiling Brew
    double bonus_ta( const action_state_t *s ) const override
    {
      double b = monk_spell_t::bonus_ta( s );

      if ( p()->legacy_azerite.boiling_brew.ok() )
        b += p()->legacy_azerite.boiling_brew.value( 2 );

      return b;
    }

    void impact( action_state_t *s ) override
    {
      monk_spell_t::impact( s );

      // Legacy Azerite: Boiling Brew drops healing spheres on an RPPM roll.
      if ( p()->legacy_boiling_brew_rppm && p()->legacy_boiling_brew_rppm->trigger() )
        p()->buff.gift_of_the_ox->trigger();
    }
  };

  struct dragonfire_brew_t : monk_spell_t
  {
    dragonfire_brew_t( monk_t *player )
      : monk_spell_t( player, "dragonfire_brew", player->talent.brewmaster.dragonfire_brew_hit )
    {
      background = true;
      aoe        = -1;
    }
  };

  action_t *dot;
  action_t *dragonfire_brew;
  bool no_bof_hit;

  breath_of_fire_t( monk_t *player, std::string_view options_str )
    : monk_spell_t( player, "breath_of_fire", player->talent.brewmaster.breath_of_fire ),
      dragonfire_brew( nullptr ),
      no_bof_hit( false )
  {
    add_option( opt_bool( "no_bof_hit", no_bof_hit ) );
    parse_options( options_str );

    aoe                 = -1;
    reduced_aoe_targets = 1.0;
    full_amount_targets = 1;
    CAST_DURING( SPINNING_CRANE_KICK_IDS );

    dot = new dot_t( player );
    add_child( dot );

    if ( player->talent.brewmaster.dragonfire_brew->ok() )
    {
      dragonfire_brew = new dragonfire_brew_t( player );
      add_child( dragonfire_brew );
    }
  }

  void execute() override
  {
    p()->buff.charred_passions->trigger();

    if ( no_bof_hit )
      return;

    if ( dragonfire_brew )
      for ( size_t i = 1; i <= p()->talent.brewmaster.dragonfire_brew->effectN( 1 ).base_value(); i++ )
        make_event<events::delayed_execute_event_t>( *sim, p(), dragonfire_brew, target,
                                                     i * timespan_t::from_seconds( 1.5 ) );

    p()->buff.mid2_brm_2pc->trigger();

    monk_spell_t::execute();

    p()->action.flurry_strikes->execute( flurry_strikes_t::WISDOM_OF_THE_WALL );
  }

  void impact( action_state_t *state ) override
  {
    monk_spell_t::impact( state );

    if ( get_td( state->target )->debuff.keg_smash->up() )
      dot->execute_on_target( state->target );
  }
};

struct fortifying_brew_t : brew_t<monk_spell_t>
{
  struct niuzaos_protection_t : public monk_absorb_t
  {
    niuzaos_protection_t( monk_t *player )
      : monk_absorb_t( player, "niuzaos_protection", player->talent.conduit_of_the_celestials.niuzaos_protection )
    {
      background  = true;
      target      = player;
      base_dd_min = player->max_health() * data().effectN( 2 ).percent();
      base_dd_max = base_dd_min;
    }
  };

  niuzaos_protection_t *absorb;

  fortifying_brew_t( monk_t *player, std::string_view options_str )
    : brew_t<monk_spell_t>( player, "fortifying_brew", player->talent.monk.fortifying_brew.find_override_spell() ),
      absorb( nullptr )
  {
    parse_options( options_str );

    harmful = may_crit = false;
    CAST_DURING( SPINNING_CRANE_KICK_IDS );

    if ( player->talent.conduit_of_the_celestials.niuzaos_protection->ok() )
      absorb = new niuzaos_protection_t( player );
  }

  void execute() override
  {
    brew_t<monk_spell_t>::execute();

    if ( p()->talent.conduit_of_the_celestials.niuzaos_protection->ok() )
      absorb->execute();
    p()->buff.fortifying_brew->trigger();
  }
};

struct exploding_keg_proc_t : public monk_spell_t
{
  exploding_keg_proc_t( monk_t *player )
    : monk_spell_t( player, "exploding_keg_proc", player->talent.brewmaster.exploding_keg->effectN( 4 ).trigger() )
  {
    background = dual = true;
    proc              = true;

    if ( const auto &effect = player->talent.master_of_harmony.balanced_stratagem_magic->effectN( 1 );
         player->talent.master_of_harmony.balanced_stratagem->ok() )
      add_parse_entry( da_multiplier_effects )
          .set_value( effect.percent() )
          .set_value_func( [ & ]( double base ) {
            // Balanced Stratagem stacks are captured as the value on the Exploding Keg buff when it's triggered.
            return base * p()->buff.exploding_keg->check_value();
          } )
          .set_eff( &effect );
  }
};

struct exploding_keg_t : public monk_spell_t
{
  cooldown_t *keg_smash;

  exploding_keg_t( monk_t *player, std::string_view options_str )
    : monk_spell_t( player, "exploding_keg", player->talent.brewmaster.exploding_keg )
  {
    parse_options( options_str );

    CAST_DURING( SPINNING_CRANE_KICK_IDS );
    aoe = -1;

    add_child( player->action.exploding_keg );
    keg_smash = player->get_cooldown( "keg_smash" );
  }

  void execute() override
  {
    // The initial EK hit is not affected by Balanced Stratagem, so we consume the stacks here.
    // The stacks are captured as the value on the buff for exploding_keg_proc_t to use later.
    p()->buff.exploding_keg->trigger(
        1, p()->buff.balanced_stratagem_magic ? p()->buff.balanced_stratagem_magic->consume( this ) : 0 );

    p()->buff.empty_the_cellar->trigger();

    if ( p()->talent.brewmaster.fuel_on_the_fire->ok() )
      p()->buff.fuel_on_the_fire->trigger(
          as<int>( p()->talent.brewmaster.fuel_on_the_fire->effectN( 1 ).base_value() ) );

    monk_spell_t::execute();

    keg_smash->reset( true );
  }

  void impact( action_state_t *s ) override
  {
    monk_spell_t::impact( s );
    get_td( s->target )->debuff.exploding_keg->trigger();
  }
};

struct empty_the_cellar_t : public monk_spell_t
{
  struct damage_t : public monk_spell_t
  {
    damage_t( monk_t *player )
      : monk_spell_t( player, "empty_the_cellar", player->talent.brewmaster.empty_the_cellar_damage )
    {
      background = dual = true;
    }

    void execute() override
    {
      monk_spell_t::execute();

      p()->baseline.brewmaster.brews.adjust( p()->talent.brewmaster.empty_the_cellar->effectN( 2 ).time_value() );
    }
  };

  action_t *damage;

  empty_the_cellar_t( monk_t *player, std::string_view options_str )
    : monk_spell_t( player, "empty_the_cellar", player->talent.brewmaster.empty_the_cellar_driver ), damage( nullptr )
  {
    parse_options( options_str );

    if ( player->talent.brewmaster.empty_the_cellar->ok() )
      damage = new damage_t( player );
  }

  bool ready() override
  {
    return p()->buff.empty_the_cellar->up();
  }

  void execute() override
  {
    p()->buff.empty_the_cellar->expire();

    size_t count        = as<size_t>( p()->talent.brewmaster.empty_the_cellar->effectN( 1 ).base_value() );
    timespan_t interval = data().effectN( 1 ).period();

    auto &tl = target_list();
    p()->rng().shuffle( tl.begin(), tl.end() );
    for ( size_t i = 0; i < count; ++i )
      make_event<events::delayed_execute_event_t>( *sim, p(), damage, tl[ i % tl.size() ], i * interval );

    monk_spell_t::execute();
  }
};

struct purifying_brew_t : public brew_t<monk_spell_t>
{
  struct gai_plins_imperial_brew_t : monk_heal_t
  {
    gai_plins_imperial_brew_t( monk_t *player )
      : monk_heal_t( player, "gai_plins_imperial_brew", player->talent.brewmaster.gai_plins_imperial_brew_heal )
    {
      background = true;
    }

    void init() override
    {
      monk_heal_t::init();
      snapshot_flags = update_flags = STATE_NO_MULTIPLIER;
    }
  };

  gai_plins_imperial_brew_t *gai_plins;

  purifying_brew_t( monk_t *player, std::string_view options_str )
    : brew_t<monk_spell_t>( player, "purifying_brew", player->talent.brewmaster.purifying_brew ),
      gai_plins( new gai_plins_imperial_brew_t( player ) )
  {
    parse_options( options_str );

    harmful = false;
    CAST_DURING( SPINNING_CRANE_KICK_IDS );
    use_off_gcd = true;
  }

  bool ready() override
  {
    if ( p()->find_stagger( "Stagger" )->is_ticking() )
      return monk_spell_t::ready();
    return false;
  }

  void execute() override
  {
    brew_t<monk_spell_t>::execute();

    p()->buff.ox_stance->trigger();

    // Legacy Azerite: Fit to Burst pays out when Purifying Brew is used out of
    // Heavy Stagger. Level index 2 is the heavy band in the stagger table.
    if ( p()->legacy_azerite.fit_to_burst.ok() && p()->find_stagger( "Stagger" )->level_index() >= 2 )
      p()->buff.legacy_fit_to_burst->trigger( p()->buff.legacy_fit_to_burst->max_stack() );

    // Legacy Azerite: Training of Niuzao follows the Stagger band, so purifying
    // out of it drops the stacks with it.
    if ( p()->legacy_azerite.training_of_niuzao.ok() )
    {
      int band = as<int>( p()->find_stagger( "Stagger" )->level_index() );
      p()->buff.legacy_training_of_niuzao->expire();
      if ( band > 0 )
        p()->buff.legacy_training_of_niuzao->trigger( std::min( band, 3 ) );
    }

    p()->buff.aspect_of_harmony.trigger_flat(
        p()->talent.master_of_harmony.clarity_of_purpose->effectN( 1 ).percent() *
        ( 1.0 + p()->composite_damage_versatility() ) *
        p()->composite_total_attack_power_by_type( attack_power_type::WEAPON_MAINHAND ) );

    double pool_size      = p()->find_stagger( "Stagger" )->pool_size();
    double purify_percent = data().effectN( 1 ).percent();
    double purify_amount  = std::max( pool_size * purify_percent, p()->max_health() * data().effectN( 2 ).percent() );
    double cleared        = p()->find_stagger( "Stagger" )->purify_flat( purify_amount, "purifying_brew" );
    p()->buff.elixir_of_determination->default_value = cleared;

    double healed = cleared * p()->talent.brewmaster.gai_plins_imperial_brew->effectN( 1 ).percent();
    if ( healed )
    {
      gai_plins->base_dd_min = gai_plins->base_dd_max = healed;
      gai_plins->target                               = p();
      gai_plins->execute();
    }
  }
};

struct courage_of_the_white_tiger_t : conduit_of_the_celestials_container_t
{
  enum cotwt_source_e
  {
    BASE,
    CELESTIAL
  };

  struct impact_t : monk_melee_attack_t
  {
    struct heal_t : monk_heal_t
    {
      heal_t( monk_t *player, std::string_view name )
        : monk_heal_t( player, fmt::format( "{}_heal", name ),
                       player->talent.conduit_of_the_celestials.courage_of_the_white_tiger_heal )
      {
        background = dual = true;
        target            = player;
        base_dd_min = base_dd_max = 1.0;
      }
    };

    cotwt_source_e source;
    action_t *heal;

    impact_t( monk_t *player, std::string_view name, cotwt_source_e source )
      : monk_melee_attack_t( player, name, player->talent.conduit_of_the_celestials.courage_of_the_white_tiger_damage ),
        source( source ),
        heal( new heal_t( player, name ) )
    {
      background     = true;
      time_to_travel = 2_s;

      if ( source == CELESTIAL )
        if ( const auto &effect = player->talent.conduit_of_the_celestials.unity_within_dmg_mult->effectN( 1 );
             effect.ok() )
          add_parse_entry( da_multiplier_effects ).set_value( effect.percent() - 1.0 ).set_eff( &effect );
    }

    void execute() override
    {
      if ( source == BASE )
      {
        p()->buff.strength_of_the_black_ox->trigger();
        p()->buff.courage_of_the_white_tiger->expire();
      }

      monk_melee_attack_t::execute();
    }

    void impact( action_state_t *state ) override
    {
      monk_melee_attack_t::impact( state );

      double result = state->result_total;
      result *= p()->talent.conduit_of_the_celestials.courage_of_the_white_tiger->effectN( 2 ).percent();
      heal->base_dd_min = heal->base_dd_max = result;
      heal->execute();
    }
  };

  courage_of_the_white_tiger_t( monk_t *player ) : conduit_of_the_celestials_container_t( player )
  {
    base      = new impact_t( player, "courage_of_the_white_tiger", BASE );
    celestial = new impact_t( player, "courage_of_the_white_tiger_celestial", CELESTIAL );
  }
};

struct xuen_summon_t : public monk_spell_t
{
  xuen_summon_t( monk_t *player, std::string_view options_str )
    : monk_spell_t( player, "invoke_xuen_the_white_tiger",
                    player->talent.conduit_of_the_celestials.invoke_xuen_the_white_tiger )
  {
    parse_options( options_str );

    CAST_DURING( SPINNING_CRANE_KICK_IDS );
  }

  void execute() override
  {
    monk_spell_t::execute();

    // BracketSim legacy compatibility: Invoker's Delight.
    p()->buff.legacy_invokers_delight->trigger();

    if ( p()->bugs )
      for ( auto target : p()->sim->target_non_sleeping_list )
        if ( auto *td = p()->get_target_data( target ); td )
          td->debuff.empowered_tiger_lightning->current_value = 0;

    p()->pets.xuen.spawn( p()->talent.conduit_of_the_celestials.invoke_xuen_the_white_tiger->duration(), 1 );
    p()->buff.invoke_xuen->trigger();
    p()->buff.flurry_of_xuen->trigger();
    p()->buff.courage_of_the_white_tiger->trigger();
    p()->buff.celestial_conduit->trigger();
  }
};

struct empowered_tiger_lightning_t : public monk_spell_t
{
  empowered_tiger_lightning_t( monk_t *player )
    : monk_spell_t( player, "empowered_tiger_lightning", player->baseline.windwalker.empowered_tiger_lightning_damage )
  {
    background = true;
  }
};

struct flurry_of_xuen_t : public monk_spell_t
{
  flurry_of_xuen_t( monk_t *player )
    : monk_spell_t( player, "flurry_of_xuen", player->talent.windwalker.flurry_of_xuen_driver->effectN( 1 ).trigger() )
  {
    background          = true;
    aoe                 = -1;
    reduced_aoe_targets = player->talent.windwalker.flurry_of_xuen->effectN( 2 ).base_value();
  }
};

struct strength_of_the_black_ox_t : conduit_of_the_celestials_container_t
{
  enum sotbo_source_e
  {
    BASE,
    CELESTIAL
  };

  struct impact_t : monk_spell_t
  {
    sotbo_source_e source;

    impact_t( monk_t *player, sotbo_source_e source )
      : monk_spell_t( player, fmt::format( "strength_of_the_black_ox_damage{}", BASE ? "" : "_celestial" ),
                      player->talent.conduit_of_the_celestials.strength_of_the_black_ox_damage ),
        source( source )
    {
      background = true;
      aoe        = -1;
      reduced_aoe_targets =
          player->talent.conduit_of_the_celestials.strength_of_the_black_ox->effectN( 2 ).base_value();

      if ( source == CELESTIAL )
        if ( const auto &effect = player->talent.conduit_of_the_celestials.unity_within_dmg_mult->effectN( 1 );
             effect.ok() )
          add_parse_entry( da_multiplier_effects ).set_value( effect.percent() - 1.0 ).set_eff( &effect );
    }

    void execute() override
    {
      if ( source == BASE )
      {
        if ( !p()->buff.strength_of_the_black_ox->up() )
          return;

        p()->buff.strength_of_the_black_ox->expire();
      }

      monk_spell_t::execute();

      p()->buff.teachings_of_the_monastery->trigger(
          as<int>( p()->talent.conduit_of_the_celestials.strength_of_the_black_ox->effectN( 3 ).base_value() ) );
    }
  };

  strength_of_the_black_ox_t( monk_t *player ) : conduit_of_the_celestials_container_t( player )
  {
    base      = new impact_t( player, BASE );
    celestial = new impact_t( player, CELESTIAL );
  }
};

struct flight_of_the_red_crane_t : conduit_of_the_celestials_container_t
{
  enum fotrc_source_e
  {
    BASE,
    CELESTIAL
  };

  struct impact_t : monk_spell_t
  {
    fotrc_source_e source;

    impact_t( monk_t *player, fotrc_source_e source )
      : monk_spell_t( player, fmt::format( "flight_of_the_red_crane_damage{}", BASE ? "" : "_celestial" ),
                      player->talent.conduit_of_the_celestials.flight_of_the_red_crane_damage ),
        source( source )
    {
      background = true;
      aoe        = data().effectN( 1 ).chain_target();

      if ( source == CELESTIAL )
        if ( const auto &effect = player->talent.conduit_of_the_celestials.unity_within_dmg_mult->effectN( 1 );
             effect.ok() )
          add_parse_entry( da_multiplier_effects ).set_value( effect.percent() - 1.0 ).set_eff( &effect );
    }
  };

  flight_of_the_red_crane_t( monk_t *player ) : conduit_of_the_celestials_container_t( player )
  {
    celestial = new impact_t( player, CELESTIAL );
  }
};

struct niuzao_spell_t : public monk_spell_t
{
  niuzao_spell_t( monk_t *player, std::string_view options_str )
    : monk_spell_t( player, "invoke_niuzao_the_black_ox", player->talent.brewmaster.invoke_niuzao_the_black_ox )
  {
    parse_options( options_str );

    CAST_DURING( SPINNING_CRANE_KICK_IDS );
    // Specifically set for 10.1 class trinket
    harmful = true;
    // Forcing the minimum GCD to 750 milliseconds
    min_gcd = timespan_t::from_millis( 750 );
  }

  void execute() override
  {
    monk_spell_t::execute();

    // BracketSim legacy compatibility: Invoker's Delight.
    p()->buff.legacy_invokers_delight->trigger();

    p()->pets.niuzao.spawn( p()->talent.brewmaster.invoke_niuzao_the_black_ox->duration(), 1 );
    p()->buff.invoke_niuzao->trigger();
    p()->buff.stand_ready->trigger();
  }
};

struct unity_within_t : public monk_spell_t
{
  unity_within_t( monk_t *player, std::string_view options_str )
    : monk_spell_t( player, "unity_within", player->talent.conduit_of_the_celestials.unity_within )
  {
    parse_options( options_str );

    harmful              = false;
    usable_while_casting = true;
  }

  bool ready() override
  {
    return p()->buff.unity_within->up();
  }

  void execute() override
  {
    // Expiring this before the execute so that there is an easy way to apply the damage bonus.
    p()->buff.unity_within->expire();

    monk_spell_t::execute();
  }
};

struct celestial_conduit_t : public monk_spell_t
{
  template <typename TBase>
  struct tick_action_t : TBase
  {
    tick_action_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
      : TBase( player, fmt::format( "celestial_conduit_{}", name ), spell_data )
    {
      TBase::background = true;

      if constexpr ( std::is_same_v<TBase, monk_spell_t> )
      {
        TBase::aoe              = -1;
        TBase::split_aoe_damage = true;
        TBase::ww_mastery       = true;
      }

      if constexpr ( std::is_same_v<TBase, monk_heal_t> )
        TBase::target = player;

      if ( const auto &effect = player->talent.conduit_of_the_celestials.path_of_the_falling_star->effectN( 1 );
           effect.ok() )
        add_parse_entry( TBase::da_multiplier_effects )
            .set_func( [] { return false; } )
            .set_value( effect.percent() )
            .set_eff( &effect )
            .set_note( "Target Count Scaling" );
    }

    double composite_aoe_multiplier( const action_state_t *state ) const override
    {
      double cam = TBase::composite_aoe_multiplier( state );

      if ( const spell_data_t *spell = TBase::p()->talent.conduit_of_the_celestials.celestial_conduit_action;
           state->n_targets && spell->ok() )
      {
        double target_scalar = std::min( as<double>( state->n_targets ), spell->effectN( 3 ).base_value() );
        cam *= 1.0 + spell->effectN( 1 ).percent() * target_scalar;
      }

      if ( const player_talent_t &talent = TBase::p()->talent.conduit_of_the_celestials.path_of_the_falling_star;
           talent->ok() )
      {
        double multiplier =
            std::max( 0.0, talent->effectN( 1 ).percent() - state->n_targets * talent->effectN( 2 ).percent() );
        cam *= 1.0 + multiplier;
      }

      return cam;
    }
  };

  action_t *damage;
  action_t *heal;

  celestial_conduit_t( monk_t *player, std::string_view options_str )
    : monk_spell_t( player, "celestial_conduit", player->talent.conduit_of_the_celestials.celestial_conduit_action ),
      damage( new tick_action_t<monk_spell_t>( player, "damage",
                                               player->talent.conduit_of_the_celestials.celestial_conduit_damage ) ),
      heal( new tick_action_t<monk_heal_t>( player, "heal",
                                            player->talent.conduit_of_the_celestials.celestial_conduit_heal ) )
  {
    parse_options( options_str );

    may_combo_strike      = true;
    channeled             = true;
    interrupt_auto_attack = false;

    tick_action = damage;
  }

  bool ready() override
  {
    return p()->buff.celestial_conduit->check() && monk_spell_t::ready();
  }

  bool usable_moving() const override
  {
    return true;
  }

  void execute() override
  {
    monk_spell_t::execute();

    p()->buff.unity_within->trigger();
  }

  void tick( dot_t *d ) override
  {
    monk_spell_t::tick( d );

    heal->execute();
  }

  void last_tick( dot_t *dot ) override
  {
    monk_spell_t::last_tick( dot );

    p()->buff.unity_within->expire();
  }
};

struct zenith_stomp_t : monk_spell_t
{
  enum source_e
  {
    ZENITH_STOMP_CAST,
    ZENITH_STOMP_TRIGGER
  };

  source_e source;

  zenith_stomp_t( monk_t *player, source_e source, std::string_view options_str )
    : monk_spell_t( player, fmt::format( "zenith_stomp_{}", source == ZENITH_STOMP_CAST ? "cast" : "trigger" ),
                    player->talent.monk.zenith_stomp_damage ),
      source( source )
  {
    parse_options( options_str );

    if ( source == ZENITH_STOMP_TRIGGER )
    {
      background = dual = true;
      trigger_gcd       = 0_ms;
    }

    aoe = -1;
    // 2026-07-11 Zenith Stomp is erroneously referencing effect 1 for sqrt scaling
    // in tooltip, which instead is used to set up the Zenith Stomp buff that controls
    // whether or not the action is available.
    // reduced_aoe_targets = player->talent.monk.zenith_stomp->effectN( 1 ).base_value();
    reduced_aoe_targets = 5.0;
    may_combo_strike    = player->wowv_ge( { 12, 1, 0 } );
    ww_mastery          = true;
    CAST_DURING( SPINNING_CRANE_KICK_IDS, CELESTIAL_CONDUIT_IDS );
  }

  void init() override
  {
    monk_spell_t::init();

    if ( auto *zenith = p()->find_action( "zenith" ); zenith )
      zenith->add_child( this );
  }

  bool usable_during_current_cast() const override
  {
    if ( p()->channeling &&
         p()->channeling->id == p()->talent.conduit_of_the_celestials.celestial_conduit_action->id() )
      return true;

    return monk_spell_t::usable_during_current_cast();
  }

  bool ready() override
  {
    if ( source == ZENITH_STOMP_TRIGGER )
      return true;

    if ( p()->buff.zenith_stomp->check() )
      return monk_spell_t::ready();

    return false;
  }

  void execute() override
  {
    monk_spell_t::execute();

    if ( source == ZENITH_STOMP_CAST )
      p()->buff.zenith_stomp->decrement();
  }
};

struct zenith_t : public monk_spell_t
{
  action_t *zenith_stomp;

  zenith_t( monk_t *player, std::string_view options_str )
    : monk_spell_t( player, "zenith", player->talent.windwalker.zenith ), zenith_stomp( nullptr )
  {
    parse_options( options_str );
    may_combo_strike = true;

    if ( player->talent.monk.zenith_stomp->ok() )
    {
      zenith_stomp = new zenith_stomp_t( player, zenith_stomp_t::ZENITH_STOMP_TRIGGER, "" );
      add_child( zenith_stomp );
    }
  }

  bool ready() override
  {
    if ( p()->buff.zenith_stomp->check() )
      return false;

    return monk_spell_t::ready();
  }

  void execute() override
  {
    p()->buff.heart_of_the_jade_serpent_yulons_avatar->trigger(
        p()->talent.conduit_of_the_celestials.yulons_avatar->effectN( 1 ).time_value() );

    monk_spell_t::execute();

    p()->buff.zenith_stomp->trigger();

    p()->buff.zenith->trigger();
    p()->cooldown.rising_sun_kick->reset( true );
    p()->buff.stand_ready->trigger();

    if ( zenith_stomp )
      zenith_stomp->execute_on_target( target );
  }
};

struct expel_harm_t : monk_heal_t
{
  // BracketSim legacy compatibility: the conduit Harm Denial (15). Shadowlands
  // was explicit that it raises the HEALING only and not the damage, so this
  // sits on the heal and nothing is added to damage_t below.
  double action_multiplier() const override
  {
    double am = monk_heal_t::action_multiplier();

    if ( p()->legacy_conduits.has( 15 ) )
      am *= 1.0 + p()->legacy_conduits.percent( 15 );

    return am;
  }

  struct damage_t : monk_spell_t
  {
    damage_t( monk_t *player ) : monk_spell_t( player, "expel_harm_damage", player->baseline.monk.expel_harm_damage )
    {
      background = dual = true;
      base_dd_min = base_dd_max = 1.0;
    }
  };

  damage_t *damage;

  expel_harm_t( monk_t *player, std::string_view options_str )
    : monk_heal_t( player, "expel_harm",
                   player->talent.windwalker.combat_wisdom->ok() ? player->talent.windwalker.combat_wisdom_expel_harm
                                                                 : player->baseline.monk.expel_harm ),
      damage( new damage_t( player ) )
  {
    parse_options( options_str );
    CAST_DURING( SPINNING_CRANE_KICK_IDS );
    if ( player->talent.windwalker.combat_wisdom->ok() )
      background = true;

    add_child( damage );

    if ( const auto &effect = player->talent.monk.strength_of_spirit->effectN( 1 ); effect.ok() )
      add_parse_entry( da_multiplier_effects )
          .set_value( effect.percent() )
          .set_value_func( functions::missing_health_percentage_t( player ) )
          .set_eff( &effect )
          .set_note( "Missing Health Scaling" );
  }

  void execute() override
  {
    if ( p()->talent.brewmaster.gift_of_the_ox->ok() )
      p()->buff.expel_harm_accumulator->trigger();

    monk_heal_t::execute();

    if ( p()->talent.brewmaster.tranquil_spirit->ok() )
    {
      double percent = p()->talent.brewmaster.tranquil_spirit->effectN( 1 ).percent();
      p()->find_stagger( "Stagger" )->purify_percent( percent, "tranquil_spirit_eh" );
      p()->proc.tranquil_spirit_expel_harm->occur();
    }
  }

  void impact( action_state_t *s ) override
  {
    monk_heal_t::impact( s );

    double result = s->result_total;
    if ( p()->buff.gift_of_the_ox && p()->buff.gift_of_the_ox->up() &&
         p()->baseline.brewmaster.expel_harm_rank_2->ok() )
    {
      p()->buff.gift_of_the_ox->consume( 5 );
      result += p()->buff.expel_harm_accumulator->check_value();
      p()->buff.expel_harm_accumulator->expire();
    }
    result *= data().effectN( 2 ).percent();

    damage->base_dd_min = damage->base_dd_max = result;
    damage->execute();
  }
};

struct celestial_fortune_t : public monk_heal_t
{
  celestial_fortune_t( monk_t *player )
    : monk_heal_t( player, "celestial_fortune", player->baseline.brewmaster.celestial_fortune_heal )
  {
    background = true;
    proc       = true;
    target     = player;
    may_crit   = false;

    base_multiplier = player->baseline.brewmaster.celestial_fortune->effectN( 1 ).percent();
  }

  void init() override
  {
    monk_heal_t::init();
    // disable the snapshot_flags for all multipliers, but specifically allow
    // action_multiplier() to be called so we can override.
    snapshot_flags &= STATE_NO_MULTIPLIER;
    snapshot_flags |= STATE_MUL_SPELL_DA | STATE_MUL_PLAYER_DAM;
  }
};

struct absorb_brew_t : public brew_t<monk_absorb_t>
{
  absorb_brew_t( monk_t *player, std::string_view options_str, std::string_view name, const spell_data_t *spell_data )
    : brew_t<monk_absorb_t>( player, name, spell_data )
  {
    parse_options( options_str );
    CAST_DURING( SPINNING_CRANE_KICK_IDS );
    harmful = false;
  }

  void execute() override
  {
    p()->buff.aspect_of_harmony.trigger_spend();

    brew_t<monk_absorb_t>::execute();

    if ( p()->talent.master_of_harmony.harmonic_surge->ok() )
      p()->buff.harmonic_surge->trigger(
          as<int>( p()->talent.master_of_harmony.harmonic_surge->effectN( 5 ).base_value() ) );
  }
};

struct celestial_brew_t : public absorb_brew_t
{
  celestial_brew_t( monk_t *player, std::string_view options_str )
    : absorb_brew_t( player, options_str, "celestial_brew", player->talent.brewmaster.celestial_brew )
  {
  }
};

struct celestial_infusion_t : public absorb_brew_t
{
  celestial_infusion_t( monk_t *player, std::string_view options_str )
    : absorb_brew_t( player, options_str, "celestial_infusion", player->talent.brewmaster.celestial_infusion )
  {
  }

  absorb_buff_t *create_buff( const action_state_t *state ) override
  {
    buff_t *b = buff_t::find( state->target, name_str, player );
    if ( b )
      return debug_cast<buffs::fractional_absorb_t *>( b );

    auto buff = make_buff<buffs::fractional_absorb_t>( p(), name_str, &data() );
    buff->set_absorb_fraction( data().effectN( 2 ).percent() );
    buff->set_absorb_source( stats );

    return buff;
  }
};

struct refreshing_drink_t : public monk_heal_t
{
  refreshing_drink_t( monk_t *player )
    : monk_heal_t( player, "refreshing_drink", player->talent.brewmaster.refreshing_drink_hot )
  {
    background = true;
    proc       = true;
    target     = player;
  }
};

template <class base_action_t>
template <typename... Args>
brew_t<base_action_t>::brew_t( monk_t *player, Args &&...args ) : base_action_t( player, std::forward<Args>( args )... )
{
  player->baseline.brewmaster.brews.insert_cooldown( this );
}

template <class base_action_t>
void brew_t<base_action_t>::execute()
{
  base_action_t::execute();

  base_action_t::p()->buff.swift_as_a_coursing_river->trigger();
  base_action_t::p()->buff.pretense_of_instability->trigger();
  base_action_t::p()->action.special_delivery->execute();
}

void brews_t::insert_cooldown( action_t *action )
{
  cooldowns.insert( { action->id, action->cooldown } );
}

bool brews_t::contains( action_t *action ) const
{
  return cooldowns.find( action->id ) != cooldowns.end();
}

void brews_t::adjust( timespan_t reduction )
{
  for ( auto &[ key, cooldown ] : cooldowns )
  {
    cooldown->adjust( -reduction );
    cooldown->sim.print_debug( "reducing cooldown of brew ({}) by {}", cooldown->name(), reduction );
  }
}
}  // namespace actions

namespace buffs
{
template <typename Base>
monk_buff_t<Base>::monk_buff_t( monk_t *player, std::string_view name, const spell_data_t *spell_data,
                                const item_t *item )
  : base_t( player, name, spell_data, item )
{
}

template <typename Base>
monk_buff_t<Base>::monk_buff_t( monk_td_t *target_data, std::string_view name, const spell_data_t *spell_data,
                                const item_t *item )
  : base_t( *target_data, name, spell_data, item )
{
}

template <typename Base>
monk_td_t &monk_buff_t<Base>::get_td( player_t *t )
{
  return *( base_t::p().get_target_data( t ) );
}

template <typename Base>
const monk_td_t *monk_buff_t<Base>::find_td( player_t *t ) const
{
  return base_t::p().find_target_data( t );
}

template <typename Base>
monk_t &monk_buff_t<Base>::p()
{
  return *debug_cast<monk_t *>( base_t::source );
}

template <typename Base>
const monk_t &monk_buff_t<Base>::p() const
{
  return *debug_cast<monk_t *>( base_t::source );
}

gift_of_the_ox_t::gift_of_the_ox_t( monk_t *player )
  : monk_buff_t<>( player, "gift_of_the_ox", player->talent.brewmaster.gift_of_the_ox_buff ),
    player( player ),
    heal_trigger(
        new orb_t( player, "gift_of_the_ox_trigger", player->talent.brewmaster.gift_of_the_ox_heal_trigger ) ),
    heal_expire( new orb_t( player, "gift_of_the_ox_expire", player->talent.brewmaster.gift_of_the_ox_heal_expire ) ),
    accumulator( 0.0 ),
    proc_data( player->talent.brewmaster.gift_of_the_ox )
{
  // we're just using buff tracking to provide stats.
  // stack changes are all controlled by the events we create, so duration is set
  // to be extra high
  set_max_stack( 5 );
  set_duration( 2 * buff_duration() );
  set_stack_behavior( buff_stack_behavior::ASYNCHRONOUS );
  set_refresh_behavior( buff_refresh_behavior::NONE );
}

void gift_of_the_ox_t::spawn_orb( int count )
{
  if ( is_fallback )
    return;

  for ( size_t i = 0; i < as<unsigned>( count ); ++i )
    player->trigger_aura_applied_callbacks( proc_data, player );

  int available = as<int>( queue.size() );
  int overflow  = std::max( count + available - max_stack(), 0 );
  player->sim->print_debug( "{} adding {} Gift of the Ox Orbs. start={} apply={} overflow={} end={}", player->name(),
                            count, available, count, overflow, std::min( count + available, max_stack() ) );

  count -= overflow;

  int remains = consume( overflow );
  overflow -= available - remains;

  for ( ; overflow > 0; --overflow )
    heal_trigger->execute();

  for ( ; count > 0; --count )
  {
    monk_buff_t::trigger();
    queue.emplace( make_event<orb_event_t>( *sim, player, data().duration(), &queue, [ this ]() {
      player->sim->print_debug( "{} expiring 1 out of {} Gift of the Ox Orbs. current={} expire={}", player->name(),
                                queue.size(), queue.size(), 1 );
      decrement();
      heal_expire->execute();
    } ) );
  }
}

void gift_of_the_ox_t::trigger_from_damage( double amount )
{
  if ( is_fallback )
    return;

  accumulator += amount;
  if ( accumulator < player->max_health() )
    return;

  int added = std::lround( accumulator / player->max_health() );
  accumulator -= added * player->max_health();
  spawn_orb( added );
}

int gift_of_the_ox_t::consume( int count )
{
  if ( is_fallback )
    return 0;

  if ( !count )
    return as<int>( queue.size() );

  int available = std::min( count, as<int>( queue.size() ) );
  player->sim->print_debug( "{} consuming {} out of {} Gift of the Ox Orbs. start={} quantity={} available={} end={}",
                            player->name(), available, queue.size(), queue.size(), count, available,
                            queue.size() - available );
  for ( int i = available; i > 0; --i )
  {
    heal_trigger->execute();
    remove();
  }
  return available;
}

void gift_of_the_ox_t::remove()
{
  event_t::cancel( queue.front() );
  queue.pop();
}

void gift_of_the_ox_t::reset()
{
  if ( is_fallback )
    return;

  monk_buff_t::reset();
  accumulator = 0.0;
  while ( !queue.empty() )
  {
    remove();
  }
}

gift_of_the_ox_t::orb_t::orb_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
  : monk_heal_t( player, name, spell_data )
{
  background = true;

  if ( const auto &effect = player->talent.monk.strength_of_spirit->effectN( 1 ); effect.ok() )
    add_parse_entry( da_multiplier_effects )
        .set_func( [ & ] { return p()->buff.expel_harm_accumulator->check(); } )
        .set_value( effect.percent() )
        .set_value_func( functions::missing_health_percentage_t( player ) )
        .set_eff( &effect )
        .set_note( "Missing Health Scaling" );
}

void gift_of_the_ox_t::orb_t::impact( action_state_t *state )
{
  if ( p()->talent.brewmaster.niuzaos_resolve->ok() )
  {
    p()->buff.niuzaos_resolve->trigger();
    return;
  }

  monk_heal_t::impact( state );

  if ( p()->talent.brewmaster.tranquil_spirit->ok() )
  {
    double percent = p()->talent.brewmaster.tranquil_spirit->effectN( 1 ).percent();
    p()->find_stagger( "Stagger" )->purify_percent( percent, "tranquil_spirit_goto" );
    p()->proc.tranquil_spirit_goto->occur();
  }

  if ( !p()->buff.expel_harm_accumulator->check() )
    return;

  double current = p()->buff.expel_harm_accumulator->check_value();
  double total   = current + state->result_amount;
  sim->print_debug( "{} adding {} to Expel Harm accumulator. current={} add={} total={}", p()->name(),
                    state->result_total, current, state->result_total, total );
  p()->buff.expel_harm_accumulator->trigger( 1, total );
}

gift_of_the_ox_t::orb_event_t::orb_event_t( monk_t *player, timespan_t duration, std::queue<orb_event_t *> *queue,
                                            std::function<void()> expire_cb )
  : event_t( *player, duration ), queue( queue ), expire_cb( std::move( expire_cb ) )
{
}

void gift_of_the_ox_t::orb_event_t::execute()
{
  assert( id == queue->front()->id );
  expire_cb();
  queue->pop();
}

const char *gift_of_the_ox_t::orb_event_t::name() const
{
  return "orb_event_t";
}

struct shuffle_t : monk_buff_t<>
{
  timespan_t accumulator;
  const timespan_t max_duration;

  shuffle_t( monk_t *player )
    : monk_buff_t<>( player, "shuffle", player->talent.brewmaster.shuffle_buff ),
      accumulator( 0_s ),
      max_duration( 3.0 * base_buff_duration )
  {
    set_trigger_spell( player->talent.brewmaster.shuffle );
  }

  void reset() override
  {
    monk_buff_t::reset();

    accumulator = 0_s;
  }

  bool trigger( int = -1, double = DEFAULT_VALUE(), double = -1.0, timespan_t duration = timespan_t::min() ) override
  {
    if ( !p().talent.brewmaster.shuffle->ok() )
      return false;

    accumulator += duration;
    duration = std::min( duration + remains(), max_duration );

    if ( p().talent.brewmaster.quick_sip->ok() )
    {
      // when you apply a shuffle refresh/application, quick sip's value is multiplied
      // by threshold // accumulator, where // refers to integer division
      timespan_t threshold = timespan_t::from_seconds( p().talent.brewmaster.quick_sip->effectN( 2 ).base_value() );
      int count            = as<int>( timespan_t::to_native( accumulator ) / timespan_t::to_native( threshold ) );
      if ( count > 0 )
        p().find_stagger( "Stagger" )
            ->purify_percent( as<double>( count ) * p().talent.brewmaster.quick_sip->effectN( 1 ).percent(),
                              "quick_sip" );
      accumulator -= threshold * count;
    }

    return monk_buff_t::trigger( -1, DEFAULT_VALUE(), -1.0, duration );
  }
};

struct fortifying_brew_t : public monk_buff_t<>
{
  int health_gain;
  fortifying_brew_t( monk_t *player )
    : monk_buff_t( player, "fortifying_brew", player->talent.monk.fortifying_brew_buff ), health_gain( 0 )
  {
    cooldown->duration = timespan_t::zero();
    set_trigger_spell( player->talent.monk.fortifying_brew );
    add_invalidate( CACHE_DODGE );
    add_invalidate( CACHE_ARMOR );
  }

  bool trigger( int stacks, double value, double chance, timespan_t duration ) override
  {
    double health_multiplier = p().talent.monk.fortifying_brew->effectN( 1 ).percent();

    // TODO: Fix spell data
    // if ( p().talent.brewmaster.fortifying_brew_determination->ok() )
    //   health_multiplier = p().talent.monk.fortifying_brew->effectN( 6 ).percent();

    // Extra Health is set by current max_health, doesn't change when max_health changes.
    health_gain = static_cast<int>( p().max_health() * health_multiplier );

    p().stat_gain( STAT_MAX_HEALTH, health_gain, (gain_t *)nullptr, (action_t *)nullptr, true );
    p().stat_gain( STAT_HEALTH, health_gain, (gain_t *)nullptr, (action_t *)nullptr, true );
    return monk_buff_t::trigger( stacks, value, chance, duration );
  }

  void expire_override( int expiration_stacks, timespan_t remaining_duration ) override
  {
    monk_buff_t::expire_override( expiration_stacks, remaining_duration );
    p().stat_loss( STAT_MAX_HEALTH, health_gain, (gain_t *)nullptr, (action_t *)nullptr, true );
    p().stat_loss( STAT_HEALTH, health_gain, (gain_t *)nullptr, (action_t *)nullptr, true );
  }
};

struct elixir_of_determination_t : monk_buff_t<absorb_buff_t>
{
  elixir_of_determination_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
    : monk_buff_t<absorb_buff_t>( player, name, spell_data )
  {
    set_cooldown( player->talent.brewmaster.elixir_of_determination_cooldown->duration() );
    set_absorb_source( player->get_stats( name ) );

    // absorb action is constructed for report buff-action linking.
    new actions::monk_absorb_t( player, name, spell_data );
  }

  void reset() override
  {
    default_value = DEFAULT_VALUE();

    monk_buff_t<absorb_buff_t>::reset();
  }

  bool trigger( int stacks, double, double, timespan_t ) override
  {
    double minimum    = p().max_health() * p().talent.brewmaster.elixir_of_determination->effectN( 3 ).percent();
    double multiplier = p().talent.brewmaster.elixir_of_determination->effectN( 2 ).percent();
    double amount     = std::max( minimum, default_value * multiplier );

    return base_t::trigger( stacks, amount );
  }
};

struct empty_barrel_buff_t : buffs::monk_buff_t<>
{
  cooldown_t *keg_smash;

  empty_barrel_buff_t( monk_t *player )
    : monk_buff_t( player, "empty_barrel", player->talent.brewmaster.bring_me_another_1->effectN( 1 ).trigger() ),
      keg_smash( nullptr )
  {
    if ( player->talent.brewmaster.bring_me_another_2->ok() )
      keg_smash = player->get_cooldown( "keg_smash" );
  }

  bool trigger( int stacks, double value, double chance, timespan_t duration ) override
  {
    if ( keg_smash )
      keg_smash->reset( true );

    return monk_buff_t::trigger( stacks, value, chance, duration );
  }
};

struct touch_of_karma_buff_t : public monk_buff_t<>
{
  touch_of_karma_buff_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
    : monk_buff_t( player, name, spell_data )
  {
    default_value = 0;
    set_cooldown( timespan_t::zero() );
    set_trigger_spell( player->baseline.windwalker.touch_of_karma );

    set_duration( spell_data->duration() );
  }

  bool trigger( int stacks, double value, double chance, timespan_t duration ) override
  {
    // Make sure the value is reset upon each trigger
    current_value = 0;

    return monk_buff_t::trigger( stacks, value, chance, duration );
  }

  void expire_override( int expiration_stacks, timespan_t remaining_duration ) override
  {
    monk_buff_t::expire_override( expiration_stacks, remaining_duration );
  }
};

struct whirling_dragon_punch_buff_t : monk_buff_t<>
{
  whirling_dragon_punch_buff_t( monk_t *player )
    : monk_buff_t( player, "whirling_dragon_punch", player->talent.windwalker.whirling_dragon_punch_buff )
  {
    // current measured value for grace period
    // partial testing as of 01/08/2024 dd/mm/yy
    base_buff_duration = 1500_ms;
    set_refresh_behavior( buff_refresh_behavior::NONE );
  }

  bool trigger( int, double, double, timespan_t ) override
  {
    timespan_t buff_duration =
        std::min( p().cooldown.rising_sun_kick->remains(), p().cooldown.fists_of_fury->remains() );

    if ( buff_duration > 0_ms )
      return monk_buff_t::trigger( -1, DEFAULT_VALUE(), -1.0,
                                   ( base_buff_duration * p().composite_melee_haste() ) + buff_duration );

    return false;
  }
};

struct zenith_t : monk_buff_t<>
{
  zenith_t( monk_t *player ) : monk_buff_t( player, "zenith", player->talent.windwalker.zenith )
  {
    if ( player->talent.windwalker.martial_agility->ok() )
      add_invalidate( CACHE_AUTO_ATTACK_SPEED );

    if ( player->talent.windwalker.tigereye_brew_1->ok() )
      set_pct_buff_type( STAT_PCT_BUFF_CRIT );
  }

  bool trigger( int stacks = -1, double = DEFAULT_VALUE(), double chance = -1.0,
                timespan_t duration = timespan_t::min() ) override
  {
    double value = 0;
    int stack    = std::min( p().buff.tigereye_brew_1->stack(),
                             as<int>( p().talent.windwalker.tigereye_brew_1->effectN( 3 ).base_value() ) );
    value        = p().buff.tigereye_brew_1->value() * stack;
    p().buff.tigereye_brew_1->decrement( stack );
    return monk_buff_t::trigger( stacks, value, chance, duration );
  }
};

struct invoke_xuen_the_white_tiger_buff_t : public monk_buff_t<>
{
  double multiplier;
  action_t *etl;

  invoke_xuen_the_white_tiger_buff_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
    : monk_buff_t( player, name, spell_data ), multiplier( 0.0 ), etl( nullptr )
  {
    set_cooldown( timespan_t::zero() );
    set_period( spell_data->effectN( 2 ).period() );

    if ( !player->baseline.windwalker.empowered_tiger_lightning->ok() )
      return;

    multiplier = player->baseline.windwalker.empowered_tiger_lightning->effectN( 2 ).percent();
    // defer etl action lookup until callback invocation, as (background) actions
    // are not yet constructed (see: sim_t::init_actor())

    set_tick_callback( [ & ]( buff_t *, int, timespan_t ) {
      if ( !etl )
        etl = p().find_action( "empowered_tiger_lightning" );

      for ( player_t *target : p().sim->target_non_sleeping_list )
        if ( monk_td_t *target_data = p().get_target_data( target ); target_data )
          if ( buff_t *debuff = target_data->debuff.empowered_tiger_lightning; debuff && debuff->up() )
            if ( double value = debuff->check_value(); value )
            {
              etl->base_dd_min = etl->base_dd_max = value * multiplier;
              etl->execute_on_target( target );
              debuff->expire();
            }
    } );
  }
};

empowered_tiger_lightning_t::empowered_tiger_lightning_t( monk_td_t &target_data )
  : monk_buff_t( &target_data, "empowered_tiger_lightning", spell_data_t::nil() )
{
  set_quiet( true );
  set_trigger_spell( target_data.monk.baseline.windwalker.empowered_tiger_lightning );
  set_duration( timespan_t::from_seconds(
                    target_data.monk.baseline.windwalker.empowered_tiger_lightning->effectN( 1 ).base_value() ) +
                1_ms );  // add small buffer to avoid expiring before tick action
  set_default_value( 0.0 );
}

bool empowered_tiger_lightning_t::trigger( const action_state_t *state )
{
  const std::array<unsigned, 4> blacklist = {
      p().baseline.monk.touch_of_death->id(),
      p().baseline.windwalker.empowered_tiger_lightning_damage->id(),
      p().baseline.windwalker.touch_of_karma_tick->id(),
      p().talent.windwalker.skyfire_heel_damage->id(),
  };

  if ( action_t::result_is_miss( state->result ) )
    return false;

  if ( !state->result_amount )
    return false;

  if ( !p().buff.invoke_xuen->check() )
    return false;

  if ( range::contains( blacklist, state->action->id ) )
    return false;

  switch ( state->result_type )
  {
    case result_amount_type::DMG_DIRECT:
    case result_amount_type::DMG_OVER_TIME:
      if ( check() )
        current_value += state->result_amount;
      else
        trigger( 1, state->result_amount );
      return true;
    default:
      return false;
  }
}

struct touch_of_death_ww_buff_t : public monk_buff_t<>
{
  // This buff is set up so that it provides the chi from
  // Windwalker's Touch of Death Rank 2. In-game, applying Touch of Death will spawn
  // three chi orbs that the player can pick up whenever they do not have max
  // Chi. Given we want to provide the chi but apply it slowly if the player is at
  // max chi, then we need to set up so that it triggers on it's own.
  touch_of_death_ww_buff_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
    : monk_buff_t( player, name, spell_data )
  {
    set_can_cancel( false );
    set_quiet( true );
    set_reverse( true );
    set_cooldown( timespan_t::zero() );
    set_trigger_spell( player->baseline.windwalker.touch_of_death_rank_3 );

    // TODO: find actual orb duration
    set_duration( timespan_t::from_minutes( 3 ) );
    set_period( timespan_t::from_seconds( 1 ) );
    set_tick_zero( true );

    set_max_stack( as<int>( player->baseline.windwalker.touch_of_death_rank_3->effectN( 1 ).base_value() ) );
    set_reverse_stack_count( 1 );
  }

  void decrement( int stacks, double value ) override
  {
    if ( stacks > 0 && player->resources.current[ RESOURCE_CHI ] < player->resources.max[ RESOURCE_CHI ] )
    {
      buff_t::decrement( stacks, value );

      player->resource_gain( RESOURCE_CHI, 1, p().gain.touch_of_death_ww );
    }
  }
};

aspect_of_harmony_t::aspect_of_harmony_t()
  : accumulator( nullptr ),
    spender( nullptr ),
    damage( nullptr ),
    heal( nullptr ),
    purified_spirit( nullptr ),
    path_of_resurgence( nullptr ),
    fallback( false )
{
}

void aspect_of_harmony_t::construct_buffs( monk_t *player )
{
  path_of_resurgence =
      make_buff_fallback( player->talent.master_of_harmony.path_of_resurgence->ok(), &*player, "path_of_resurgence",
                          player->talent.master_of_harmony.path_of_resurgence->effectN( 1 ).trigger() );

  if ( fallback || !player->talent.master_of_harmony.aspect_of_harmony->ok() )
  {
    fallback = true;
    buff_t::make_fallback( player, "aspect_of_harmony_accumulator", &*player );
    buff_t::make_fallback( player, "aspect_of_harmony_spender", &*player );
    return;
  }

  accumulator = new accumulator_t( player, this );
  spender     = new spender_t( player, this );
}

void aspect_of_harmony_t::construct_actions( monk_t *player )
{
  if ( fallback || !player->talent.master_of_harmony.aspect_of_harmony->ok() )
  {
    fallback = true;
    return;
  }

  damage = new spender_t::tick_t<actions::monk_spell_t>( player, "aspect_of_harmony_damage",
                                                         player->talent.master_of_harmony.aspect_of_harmony_damage );
  heal   = new spender_t::tick_t<actions::monk_heal_t>( player, "aspect_of_harmony_heal",
                                                        player->talent.master_of_harmony.aspect_of_harmony_heal );

  if ( player->talent.master_of_harmony.purified_spirit->ok() )
  {
    purified_spirit = new spender_t::purified_spirit_t<actions::monk_spell_t>(
        player, player->talent.master_of_harmony.purified_spirit_damage, this );
    damage->add_child( purified_spirit );

    spender->set_expire_callback( [ & ]( buff_t *, int, timespan_t ) { purified_spirit->execute(); } );
  }
}

void aspect_of_harmony_t::trigger( action_state_t *state )
{
  if ( fallback || state->result_amount <= 0.0 )
    return;

  if ( !spender->check() )
    accumulator->trigger_with_state( state );

  spender->trigger_with_state( state );
}

void aspect_of_harmony_t::trigger_flat( double amount )
{
  if ( fallback || spender->check() )
    return;

  accumulator->adjust( amount );
}

void aspect_of_harmony_t::trigger_spend()
{
  if ( fallback )
    return;

  spender->trigger();
}

void aspect_of_harmony_t::trigger_path_of_resurgence()
{
  if ( fallback )
    return;

  path_of_resurgence->trigger();
}

bool aspect_of_harmony_t::heal_ticking()
{
  if ( heal )
    return heal->get_dot()->is_ticking();
  return false;
}

aspect_of_harmony_t::accumulator_t::accumulator_t( monk_t *player, aspect_of_harmony_t *aspect_of_harmony )
  : monk_buff_t( player, "aspect_of_harmony_accumulator",
                 player->talent.master_of_harmony.aspect_of_harmony_accumulator ),
    aspect_of_harmony( aspect_of_harmony )
{
  set_default_value( 0.0 );

  set_refresh_behavior( buff_refresh_behavior::DURATION );
  set_period( 1_s );
  set_tick_behavior( buff_tick_behavior::REFRESH );
  set_partial_tick( true );

  freeze_stacks = true;

  set_tick_callback( [ this ]( buff_t *buff, int, timespan_t ) {
    if ( buff->sim->current_iteration == 0 )  // only collect data from the first iteration
      pool_size_percent.add_max( buff->sim->current_time(), buff->check_value() / buff->player->max_health() );
  } );
}

void aspect_of_harmony_t::accumulator_t::trigger_with_state( action_state_t *state )
{
  if ( p().talent.master_of_harmony.coalescence->ok() && state->action->id == p().talent.brewmaster.keg_smash->id() )
    return;

  size_t result_type_offset = 0;
  switch ( state->result_type )
  {
    case result_amount_type::DMG_DIRECT:
    case result_amount_type::DMG_OVER_TIME:
      result_type_offset = 1;
      break;
    case result_amount_type::HEAL_DIRECT:
    case result_amount_type::HEAL_OVER_TIME:
      result_type_offset = 3;
      break;
    default:
      assert( false && "result_type_offset is zero" );
      return;
  }

  size_t index_offset = p().specialization() == MONK_BREWMASTER ? 0 : 1;
  double multiplier =
      p().talent.master_of_harmony.aspect_of_harmony->effectN( result_type_offset + index_offset ).percent();

  if ( aspect_of_harmony->path_of_resurgence && aspect_of_harmony->path_of_resurgence->up() )
    multiplier *=
        1.0 + aspect_of_harmony->path_of_resurgence->data().effectN( result_type_offset + index_offset ).percent();

  const auto whitelist = { p().baseline.brewmaster.blackout_kick->id(),
                           p().talent.monk.rising_sun_kick->effectN( 1 ).trigger()->id(),
                           p().baseline.monk.tiger_palm->id() };

  if ( const auto &effect = p().talent.master_of_harmony.way_of_a_thousand_strikes->effectN( 1 );
       effect.ok() && std::find( whitelist.begin(), whitelist.end(), state->action->id ) != whitelist.end() )
    multiplier *= 1.0 + effect.percent();

  multiplier *= 1.0 + p().talent.master_of_harmony.coalescence->effectN( 3 ).percent();

  adjust( state->result_amount * multiplier );
  sim->print_debug( "AoH does_gen: {} {}", state->action->name(), state->action->id );
}

void aspect_of_harmony_t::accumulator_t::adjust( double amount )
{
  double previous = check_value();

  double value = std::max( std::min( previous + amount, p().max_health() ), 0.0 );

  if ( value > 0.0 )
    monk_buff_t::trigger( -1, value );
  else
    monk_buff_t::expire();

  sim->print_debug( "Aspect of Harmony +A: {}, P: {}, T: {}", amount, previous, value );
}

aspect_of_harmony_t::spender_t::spender_t( monk_t *player, aspect_of_harmony_t *aspect_of_harmony )
  : monk_buff_t( player, "aspect_of_harmony_spender", player->talent.master_of_harmony.aspect_of_harmony_spender ),
    aspect_of_harmony( aspect_of_harmony ),
    pool( 0.0 )
{
  set_default_value( 0.0 );
}

void aspect_of_harmony_t::spender_t::reset()
{
  monk_buff_t::reset();
  pool          = 0.0;
  current_value = 0.0;
  sim->print_debug( "Aspect of Harmony =P: 0" );
}

bool aspect_of_harmony_t::spender_t::trigger( int stacks, double, double chance, timespan_t duration )
{
  if ( check() && aspect_of_harmony->purified_spirit )
    aspect_of_harmony->purified_spirit->execute();

  pool = aspect_of_harmony->accumulator->check_value();
  aspect_of_harmony->accumulator->expire();

  sim->print_debug( "Aspect of Harmony +P: {}", current_value );
  return monk_buff_t::trigger( stacks, pool, chance, duration );
}

void aspect_of_harmony_t::spender_t::trigger_with_state( action_state_t *state )
{
  if ( !check() )
  {
    if ( p().talent.master_of_harmony.coalescence->ok() && state->action->id == p().talent.brewmaster.keg_smash->id() )
    {
      double amount = std::min( state->result_amount * p().talent.master_of_harmony.coalescence->effectN( 2 ).percent(),
                                aspect_of_harmony->accumulator->check_value() );
      aspect_of_harmony->accumulator->adjust( -amount );
      residual_action::trigger( aspect_of_harmony->damage, state->target, amount );
    }

    return;
  }

  const auto whitelist = { p().baseline.monk.expel_harm->id(), p().baseline.monk.vivify->id(),
                           p().baseline.monk.blackout_kick->id(), p().baseline.monk.tiger_palm->id() };

  auto in_hg_whitelist = [ whitelist, id = state->action->id, this ]() {
    return p().talent.master_of_harmony.harmonic_gambit->ok() &&
           std::find( whitelist.begin(), whitelist.end(), id ) != whitelist.end();
  };

  double multiplier = p().talent.master_of_harmony.aspect_of_harmony->effectN( 6 ).percent();

  action_t *spend_target = nullptr;
  switch ( state->result_type )
  {
    case result_amount_type::DMG_DIRECT:
    case result_amount_type::DMG_OVER_TIME:
      if ( p().specialization() == MONK_BREWMASTER )
        spend_target = aspect_of_harmony->damage;
      else if ( in_hg_whitelist() )
      {
        spend_target = aspect_of_harmony->damage;
        multiplier   = p().talent.master_of_harmony.harmonic_gambit->effectN( 2 ).percent();
      }
      break;
    case result_amount_type::HEAL_DIRECT:
    case result_amount_type::HEAL_OVER_TIME:
      if ( in_hg_whitelist() )
      {
        spend_target = aspect_of_harmony->heal;
        multiplier   = p().talent.master_of_harmony.harmonic_gambit->effectN( 1 ).percent();
      }
      break;
    default:
      break;
  }

  double bonus = 0.0;
  if ( spend_target )
  {
    double amount = std::min( state->result_amount * multiplier, current_value );
    current_value -= amount;

    // (bug) No evidence currently exists to support the idea that the intensify mechanic functions.
    // dot_t *dot = spend_target->get_dot( state->target );
    // if ( p().talent.master_of_harmony.coalescence->ok() && dot && dot->state && dot->is_ticking() && rng().roll( 0.0
    // ) )
    //   bonus = std::min( spend_target->base_ta( dot->state ) * dot->ticks_left() *
    //                         p().talent.master_of_harmony.aspect_of_harmony->effectN( 9 ).percent(),
    //                     current_value );
    // current_value -= bonus;

    sim->print_debug( "AoH does_spend: {} {}", state->action->name(), state->action->id );
    sim->print_debug( "Aspect of Harmony -P: {}, P: {}, T: {}, A: {}, B: {}", amount + bonus,
                      current_value + amount + bonus, current_value, amount, bonus );

    residual_action::trigger( spend_target, state->target, amount + bonus );
  }

  if ( current_value == 0.0 )
  {
    expire();
  }
}

template <class base_action_t>
aspect_of_harmony_t::spender_t::purified_spirit_t<base_action_t>::purified_spirit_t(
    monk_t *player, const spell_data_t *spell_data, aspect_of_harmony_t *aspect_of_harmony )
  : base_action_t( player, "purified_spirit", spell_data ), aspect_of_harmony( aspect_of_harmony )
{
  base_action_t::aoe = -1;
  // base_action_t::split_aoe_damage = true; this does not work for dots
  base_action_t::dot_behavior = DOT_CLIP;
}

template <class base_action_t>
void aspect_of_harmony_t::spender_t::purified_spirit_t<base_action_t>::init()
{
  base_action_t::init();
  base_action_t::update_flags = base_action_t::snapshot_flags &= STATE_NO_MULTIPLIER;
}

template <class base_action_t>
void aspect_of_harmony_t::spender_t::purified_spirit_t<base_action_t>::execute()
{
  // Avoid overwriting base_td with zero values.
  // It's possible for Purified Spirit to be executed from depleting the current spender while the PS DoT is still
  // ticking from a previous spender expiring.
  if ( !aspect_of_harmony->spender->current_value )
    return;

  double ticks = aspect_of_harmony->purified_spirit->dot_duration / aspect_of_harmony->purified_spirit->base_tick_time;
  base_action_t::base_td =
      aspect_of_harmony->spender->current_value / ticks / as<double>( base_action_t::num_targets() );
  base_action_t::sim->print_debug( "Purified Spirit consuming rest of pool. Pool: {} TA: {}",
                                   aspect_of_harmony->spender->current_value,
                                   aspect_of_harmony->spender->current_value / ticks );
  aspect_of_harmony->spender->current_value = 0.0;
  if ( base_action_t::base_td > 0.0 )
    base_action_t::execute();
}

template <class base_action_t>
aspect_of_harmony_t::spender_t::tick_t<base_action_t>::tick_t( monk_t *player, std::string_view name,
                                                               const spell_data_t *spell_data )
  : residual_action::residual_periodic_action_t<base_action_t>( player, name, spell_data )
{
}

balanced_stratagem_t::balanced_stratagem_t( monk_t *player, std::string_view name, const spell_data_t *spell_data,
                                            std::unordered_set<unsigned int> allowlist )
  : monk_buff_t<>( player, fmt::format( "balanced_stratagem_{}", name ), spell_data ),
    allowlist( std::move( allowlist ) )
{
  // Remove IDs that weren't found so they don't unintentionally trigger on auto attacks
  this->allowlist.erase( 0 );
}

bool balanced_stratagem_t::trigger( const spell_data_t *spell )
{
  if ( range::contains( allowlist, spell->id() ) )
    return monk_buff_t::trigger();

  return false;
}

struct balanced_stratagem_magic_t : balanced_stratagem_t
{
  balanced_stratagem_magic_t( monk_t *player )
    : balanced_stratagem_t(
          player, "magic", player->talent.master_of_harmony.balanced_stratagem_magic,
          { player->baseline.monk.blackout_kick->id(), player->baseline.brewmaster.blackout_kick->id(),
            player->talent.brewmaster.keg_smash->id(), player->talent.brewmaster.rushing_jade_wind->id(),
            player->baseline.monk.spinning_crane_kick->id(), player->baseline.monk.tiger_palm->id(),
            player->talent.brewmaster.press_the_advantage_tiger_palm->id() } )
  {
  }
};

struct balanced_stratagem_physical_t : balanced_stratagem_t
{
  balanced_stratagem_physical_t( monk_t *player )
    : balanced_stratagem_t( player, "physical", player->talent.master_of_harmony.balanced_stratagem_physical,
                            { player->talent.brewmaster.breath_of_fire->id(), player->talent.monk.chi_burst->id(),
                              player->baseline.monk.crackling_jade_lightning->id(),
                              player->baseline.monk.expel_harm->id(), player->talent.brewmaster.exploding_keg->id(),
                              player->talent.monk.soothing_mist->id(), player->baseline.monk.vivify->id() } )
  {
  }
};

fractional_absorb_t::fractional_absorb_t( monk_t *player, std::string_view name, const spell_data_t *spell_data )
  : monk_buff_t<absorb_buff_t>( player, name, spell_data ), absorb_fraction( 1.0 )
{
}

double fractional_absorb_t::consume( double amount, action_state_t *state )
{
  return base_t::consume( amount * absorb_fraction, state );
}

absorb_buff_t *fractional_absorb_t::set_absorb_fraction( double fraction )
{
  absorb_fraction = fraction;
  return this;
}

struct niuzaos_resolve_t : buffs::monk_buff_t<>
{
  struct heal_t : actions::monk_heal_t
  {
    heal_t( monk_t *player ) : monk_heal_t( player, "niuzaos_resolve", player->talent.brewmaster.niuzaos_resolve_hot )
    {
      background = true;
      proc       = true;
      may_crit   = true;
      target     = player;

      // Convert from a HoT to a direct heal, triggered by the buff
      attack_power_mod.direct = attack_power_mod.tick;
      attack_power_mod.tick   = 0.0;
      base_tick_time          = timespan_t::zero();
      dot_duration            = timespan_t::zero();

      if ( const auto &effect = player->talent.brewmaster.niuzaos_resolve->effectN( 1 ); effect.ok() )
        add_parse_entry( da_multiplier_effects )
            .set_value( effect.percent() )
            .set_value_func( functions::missing_health_percentage_t( player ) )
            .set_eff( &effect )
            .set_note( "Missing Health Scaling" );
    }
  };

  action_t *heal;

  niuzaos_resolve_t( monk_t *player )
    : monk_buff_t<>( player, "niuzaos_resolve", player->talent.brewmaster.niuzaos_resolve_hot ),
      heal( new heal_t( player ) )
  {
    set_stack_behavior( buff_stack_behavior::ASYNCHRONOUS );
    set_refresh_behavior( buff_refresh_behavior::DURATION );
    set_period( player->talent.brewmaster.niuzaos_resolve_hot->effectN( 1 ).period() );
    set_tick_behavior( buff_tick_behavior::REFRESH );
    set_partial_tick( true );

    freeze_stacks = true;

    set_tick_callback( [ & ]( buff_t *buff, int, timespan_t tick_time ) {
      // Handle partial ticks
      double m = tick_time / buff->buff_period;

      // Amplify by the current number of stacks
      m *= buff->check();

      heal->base_multiplier = m;
      heal->execute();
    } );
  }
};

flurry_charge_t::flurry_charge_t( monk_t *player )
  : monk_buff_t<>( player, "flurry_charge", player->talent.shado_pan.flurry_charge )
{
  set_default_value_from_effect( 1 );
}

bool flurry_charge_t::trigger( action_state_t *state, weapon_t *weapon )
{
  if ( !p().talent.shado_pan.flurry_strikes->ok() || action_t::result_is_miss( state->result ) )
    return false;

  unsigned flurry_charges = 0;
  switch ( weapon->group() )
  {
    case WEAPON_1H:
      flurry_charges = as<int>( p().talent.shado_pan.flurry_strikes->effectN( 1 ).base_value() );
      break;
    case WEAPON_2H:
      flurry_charges = as<int>( p().talent.shado_pan.flurry_strikes->effectN( 2 ).base_value() );
      break;
    default:
      assert( false );
  }

  if ( state->result == RESULT_CRIT )
    flurry_charges *= as<int>( 1.0 + p().talent.shado_pan.one_versus_many->effectN( 1 ).base_value() );

  return trigger( flurry_charges );
}
}  // namespace buffs
}  // end namespace monk

namespace monk
{
monk_td_t::monk_td_t( player_t *target, monk_t *player )
  : actor_target_data_t( target, player ), dot(), debuff(), monk( *player )
{
  // Legacy Azerite: Sunrise Technique
  debuff.legacy_sunrise_technique =
      make_buff_fallback( player->legacy_azerite.sunrise_technique.ok(), *this, "legacy_sunrise_technique",
                          player->find_spell( 273299 ) );

  // BracketSim legacy compatibility: Faeline Harmony.
  debuff.legacy_fae_exposure =
      make_buff_fallback( player->shadowlands_legacy.faeline_harmony, *this,
                          "fae_exposure", player->find_spell( 356773 ) );

  // BracketSim legacy compatibility: Keefer's Skyreach.
  debuff.legacy_keefers_skyreach =
      make_buff_fallback( player->shadowlands_legacy.keefers_skyreach, *this, "keefers_skyreach",
                          player->find_spell( 344021 ) );
  debuff.legacy_skyreach_exhaustion =
      make_buff_fallback( player->shadowlands_legacy.keefers_skyreach, *this, "skyreach_exhaustion",
                          player->find_spell( 337341 ) );

  // BracketSim legacy compatibility: Shadowlands covenant abilities.
  debuff.legacy_weapons_of_order =
      make_buff_fallback( player->legacy_covenant.weapons_of_order->ok(), *this, "weapons_of_order_debuff",
                          player->legacy_covenant.weapons_of_order_debuff );
  debuff.legacy_bonedust_brew =
      make_buff_fallback( player->legacy_covenant.bonedust_brew->ok(), *this, "bonedust_brew",
                          player->legacy_covenant.bonedust_brew );

  // Windwalker
  debuff.empowered_tiger_lightning = make_buff_fallback<buffs::empowered_tiger_lightning_t>(
      player->baseline.windwalker.empowered_tiger_lightning->ok(), *this, "empowered_tiger_lightning" );

  // Brewmaster
  debuff.keg_smash = make_buff_fallback( player->talent.brewmaster.keg_smash->ok(), *this, "keg_smash",
                                         player->talent.brewmaster.keg_smash )
                         ->set_cooldown( timespan_t::zero() )
                         ->set_default_value_from_effect( 3 );

  debuff.exploding_keg = make_buff_fallback( player->talent.brewmaster.exploding_keg->ok(), *this,
                                             "exploding_keg_debuff", player->talent.brewmaster.exploding_keg )
                             ->set_trigger_spell( player->talent.brewmaster.exploding_keg )
                             ->set_cooldown( timespan_t::zero() );

  // Shado-Pan

  debuff.high_impact = make_buff_fallback( player->talent.shado_pan.high_impact->ok(), *this, "high_impact",
                                           player->talent.shado_pan.high_impact_debuff )
                           ->set_trigger_spell( player->talent.shado_pan.high_impact )
                           ->set_quiet( true );

  // Tier
  debuff.mid2_brm_4pc = make_buff_fallback( player->sets->has_set_bonus( MONK_BREWMASTER, MID2, B4 ), *this, "scorched",
                                            player->tier.mid2.brm_4pc_debuff )
                            ->set_trigger_spell( player->talent.brewmaster.keg_smash );

  dot.breath_of_fire               = target->get_dot( "breath_of_fire_dot", player );
  dot.crackling_jade_lightning_aoe = target->get_dot( "crackling_jade_lightning_aoe", player );
  dot.aspect_of_harmony            = target->get_dot( "aspect_of_harmony_damage", player );
}

monk_t::monk_t( sim_t *sim, std::string_view name, race_e r )
  : base_t( sim, MONK, name, r ),
    action(),
    buff(),
    gain(),
    proc(),
    cooldown(),
    baseline(),
    talent(),
    tier(),
    pets( this ),
    user_options( options_t() )
{
  cooldown.anvil_and_stave = get_cooldown( "anvil_and_stave" );
  cooldown.blackout_kick   = get_cooldown( "blackout_kick" );
  cooldown.fists_of_fury   = get_cooldown( "fists_of_fury" );
  cooldown.rising_sun_kick = get_cooldown( "rising_sun_kick" );

  // BracketSim legacy compatibility: Sinister Teachings. The first must be the
  // SAME cooldown object the fallen_order action uses, or shortening it would
  // shorten nothing; get_cooldown returns exactly that.
  cooldown.legacy_fallen_order      = get_cooldown( "fallen_order" );
  cooldown.legacy_sinister_teachings = get_cooldown( "legacy_sinister_teachings" );

  resource_regeneration              = regen_type::DYNAMIC;
  regen_caches[ CACHE_HASTE ]        = true;
  regen_caches[ CACHE_ATTACK_HASTE ] = true;
  user_options.initial_chi =
      talent.windwalker.combat_wisdom.ok() ? (int)talent.windwalker.combat_wisdom->effectN( 1 ).base_value() : 0;
  user_options.chi_burst_healing_targets = 8;
}

bool monk_t::wowv_l( wowv_t value ) const
{
  return sim->dbc->wowv() < value;
}

bool monk_t::wowv_ge( wowv_t value ) const
{
  return sim->dbc->wowv() >= value;
}

void monk_t::parse_player_effects()
{
  /*
   * Permanent actor-specific effects go here.
   * Make sure to use a specific `find_spell` method (i.e. `find_specialization_spell`)
   * for all of these spells or they will be applied to actors of the incorrect spec.
   */

  // class and spec shared auras
  parse_effects( buff.fortifying_brew );

  // brewmaster player auras

  // mistweaver player auras

  // windwalker player auras
  parse_effects( buff.hit_combo, effect_mask_t( true ).disable( 4 ) );

  // class talent auras

  // brewmaster talent auras
  parse_effects( buff.pretense_of_instability );
  parse_effects( buff.swift_as_a_coursing_river );
  parse_effects( talent.brewmaster.heart_of_the_ox, [ & ]( double value ) {
    if ( buff.invoke_niuzao->check() )
      // value not available in spell data :(
      value *= 1.0 + talent.brewmaster.invoke_niuzao_the_black_ox->effectN( 6 ).percent();
    return value;
  } );
  parse_target_effects( td_fn( &monk_td_t::dots_t::breath_of_fire ), talent.brewmaster.breath_of_fire_dot );

  // windwalker talent auras
  parse_effects( buff.momentum_boost_speed );
  parse_effects( talent.windwalker.ferociousness, [ & ]( double value ) {
    if ( buff.invoke_xuen->check() )
      value *= 1.0 + talent.conduit_of_the_celestials.invoke_xuen_the_white_tiger->effectN( 3 ).percent();
    return value;
  } );
  parse_effects( talent.windwalker.martial_agility, [ & ]( double value ) {
    if ( buff.zenith->check() )
      return talent.windwalker.martial_agility->effectN( 3 ).percent();
    return value;
  } );

  // Shadopan
  parse_effects( buff.whirling_steel );
  parse_effects( buff.predictive_training );

  // Conduit of the Celestials
  parse_effects( buff.inner_compass_ox_stance );
  parse_effects( buff.inner_compass_serpent_stance );
  parse_effects( buff.inner_compass_tiger_stance );
  parse_effects( buff.inner_compass_crane_stance );

  effect_mask_t em = talent.conduit_of_the_celestials.flowing_wisdom->ok() ? effect_mask_t( true )
                                                                           : effect_mask_t( true ).disable( 8 );
  parse_effects( buff.heart_of_the_jade_serpent, em,
                 [ & ] { return !buff.heart_of_the_jade_serpent_unity_within->check(); } );
  parse_effects( buff.heart_of_the_jade_serpent_yulons_avatar, em );
  parse_effects( buff.heart_of_the_jade_serpent_unity_within, em );

  // Midnight S1 Set Effects
  // Midnight S2 Set Effects
  // Midnight S3 Set Effects
}

namespace actions
{
// BracketSim legacy compatibility: Shadowlands covenant abilities ==========
// Duration, cooldown, chance and damage all come from the covenant spells
// themselves, which still resolve in current client data.

struct legacy_weapons_of_order_t : public monk_spell_t
{
  legacy_weapons_of_order_t( monk_t *p, std::string_view options_str )
    : monk_spell_t( p, "weapons_of_order", p->legacy_covenant.weapons_of_order )
  {
    parse_options( options_str );
    harmful = may_miss = false;
    aoe     = -1;
  }

  void execute() override
  {
    monk_spell_t::execute();
    // BracketSim legacy compatibility: the soulbind traits that ride this
    // covenant ability. The shared player_t layer owns them because they are
    // identical on every class bar a duration that tracks whatever ability
    // they ride; only the host and its cooldown are class knowledge.
    player->legacy_soulbinds.covenant_ability_cast( player, legacy_soulbind::COVENANT_KYRIAN,
                                            cooldown );
    p()->buff.legacy_weapons_of_order->trigger();

    // BracketSim legacy compatibility: Call to Arms (7718). Weapons of Order
    // also summons the spec's celestial briefly.
    //
    // Shadowlands READ the duration from four spells - 358518 Xuen, 358520
    // Niuzao, 358521 Yu'lon, 358522 Chi-Ji - and every one of them is missing
    // from Midnight's export, which is why this sat blocked. The twelve
    // seconds below is not a guess: all four were read out of the live client
    // on 2026-09-06 and every one says "Summons an effigy ... for 12 sec".
    // Same sourcing as Merciless Bonegrinder's nine seconds.
    //
    // Only the damage celestials are summoned. Yu'lon and Chi-Ji are healing
    // and this sim does not track healing for these characters, so summoning
    // them would add a pet that cannot be measured either way.
    if ( p()->shadowlands_legacy.call_to_arms )
    {
      switch ( p()->specialization() )
      {
        case MONK_WINDWALKER:
          p()->pets.xuen.spawn( 12_s, 1 );
          break;
        case MONK_BREWMASTER:
          p()->pets.niuzao.spawn( 12_s, 1 );
          break;
        default:
          break;
      }
    }
  }

  void impact( action_state_t *s ) override
  {
    monk_spell_t::impact( s );
    // The mark that made the target take more of the monk's damage. The brew
    // cost reduction Rising Sun Kick granted is not modelled.
    p()->get_target_data( s->target )->debuff.legacy_weapons_of_order->trigger();
  }
};

struct legacy_bonedust_brew_t : public monk_spell_t
{
  legacy_bonedust_brew_t( monk_t *p, std::string_view options_str )
    : monk_spell_t( p, "bonedust_brew", p->legacy_covenant.bonedust_brew )
  {
    parse_options( options_str );
    harmful = may_miss = false;
    aoe     = -1;
    if ( p->action.legacy_bonedust_brew_damage )
      add_child( p->action.legacy_bonedust_brew_damage );
  }

  void impact( action_state_t *s ) override
  {
    monk_spell_t::impact( s );
    p()->get_target_data( s->target )->debuff.legacy_bonedust_brew->trigger();
  }

  void execute() override
  {
    monk_spell_t::execute();

    // BracketSim legacy compatibility: the soulbind traits that ride this
    // covenant ability. The shared player_t layer owns them because they are
    // identical on every class bar a duration that tracks whatever ability
    // they ride; only the host and its cooldown are class knowledge.
    player->legacy_soulbinds.covenant_ability_cast( player, legacy_soulbind::COVENANT_NECROLORD,
                                            cooldown );
  }
};

// BracketSim legacy compatibility: Fallen Order. One adept's whole six seconds
// of output, delivered as a single hit when it spawns. The coefficient is FITTED
// from one Brewmaster log rather than read from data - see PORT_STATE.md - and
// the adepts' own spec abilities beyond Brewmaster's are not modelled at all.
struct legacy_fallen_order_adept_t : public monk_spell_t
{
  legacy_fallen_order_adept_t( monk_t *p )
    : monk_spell_t( p, "fallen_order_adept", p->legacy_covenant.fallen_order )
  {
    background = dual = true;
    may_crit = true;
    aoe = 0;
    // The adept shares the driver's spell, and that spell carries the PORTAL's
    // 24 second duration. Without clearing it the adept spawns a 24 tick dot of
    // its own that deals nothing and inflates every tick count in the report.
    dot_duration = 0_ms;
    base_tick_time = 0_ms;
    // 2.286 attack power per adept: 9947 base damage over 9 adepts against 394
    // attack power, with the log's 22.679% versatility divided back out.
    attack_power_mod.direct = 2.286;
    spell_power_mod.direct  = 0.0;
    school = SCHOOL_SHADOW;
  }
};

struct legacy_fallen_order_t : public monk_spell_t
{
  legacy_fallen_order_t( monk_t *p, std::string_view options_str )
    : monk_spell_t( p, "fallen_order", p->legacy_covenant.fallen_order )
  {
    parse_options( options_str );
    may_miss = harmful = false;
    channeled = false;

    // The portal is a 24 second ground effect that spawns an adept every 3
    // seconds, so it is modelled as a ticking driver rather than as summons:
    // eight ticks, one per adept, unaffected by haste.
    dot_duration   = data().duration();
    base_tick_time = 3_s;
    hasted_ticks   = false;
    tick_zero      = false;

    tick_action = new legacy_fallen_order_adept_t( p );
    add_child( tick_action );

    // BracketSim legacy compatibility: Sinister Teachings adds one adept that
    // lasts 356818 effect 2 seconds, against a base adept's 326860 effect 4.
    // Both are read here rather than chosen, and the ratio is how many extra
    // adepts the one long-lived one is worth in a model that has no durations.
    if ( p->shadowlands_legacy.sinister_teachings )
    {
      double extra_secs = p->find_spell( 356818 )->effectN( 2 ).base_value();
      double one_adept  = p->legacy_covenant.fallen_order->effectN( 4 ).base_value();
      int extra = one_adept > 0 ? static_cast<int>( extra_secs / one_adept ) : 0;
      int ticks = static_cast<int>( dot_duration / base_tick_time );
      // Spread them out rather than dropping them all on one tick.
      legacy_extra_every = ( extra > 0 && ticks > extra ) ? ticks / extra : 0;
    }
  }

  // BracketSim legacy compatibility: Sinister Teachings. Fire the adept a
  // second time on every Nth tick, so the extra adepts arrive across the
  // portal the way the base ones do.
  int legacy_extra_every = 0;
  int legacy_tick_count  = 0;

  void tick( dot_t *d ) override
  {
    monk_spell_t::tick( d );

    if ( legacy_extra_every > 0 && ++legacy_tick_count % legacy_extra_every == 0 )
      tick_action->execute_on_target( d->target );
  }

  // A ground effect, not a channel: the monk keeps acting while it runs.
  timespan_t composite_dot_duration( const action_state_t * ) const override
  { return dot_duration; }

  void execute() override
  {
    monk_spell_t::execute();

    // BracketSim legacy compatibility: the soulbind traits that ride this
    // covenant ability. The shared player_t layer owns them because they are
    // identical on every class bar a duration that tracks whatever ability
    // they ride; only the host and its cooldown are class knowledge.
    player->legacy_soulbinds.covenant_ability_cast( player, legacy_soulbind::COVENANT_VENTHYR,
                                            cooldown );
  }
};

struct legacy_faeline_stomp_damage_t : public monk_spell_t
{
  legacy_faeline_stomp_damage_t( monk_t *p )
    : monk_spell_t( p, "faeline_stomp_damage", p->legacy_covenant.faeline_stomp_damage )
  {
    background = true;
    // Five enemies, as the driver's own description states.
    aoe = 5;
  }

  double composite_aoe_multiplier( const action_state_t *state ) const override
  {
    double cam = monk_spell_t::composite_aoe_multiplier( state );

    // BracketSim legacy compatibility: Way of the Fae (conduit 63) raises this
    // damage per target it hits, up to five.
    //
    // SimulationCraft read the cap from effect 2 of conduit spell 337303, which
    // Midnight does not ship. Five is not a guess: the conduit's own
    // description gives both halves - "10.0% per target hit ... up to a maximum
    // of 50.0%" - and the ability itself already hits at most five.
    if ( p()->legacy_conduits.has( 63 ) )
    {
      const std::vector<player_t *> &targets = state->action->target_list();
      if ( !targets.empty() )
      {
        cam *= 1.0 + p()->legacy_conduits.percent( 63 ) *
                         std::min( static_cast<double>( targets.size() ), 5.0 );
      }
    }

    return cam;
  }
};

struct legacy_faeline_stomp_t : public monk_spell_t
{
  action_t *damage;

  legacy_faeline_stomp_t( monk_t *p, std::string_view options_str )
    : monk_spell_t( p, "faeline_stomp", p->legacy_covenant.faeline_stomp ),
      damage( new legacy_faeline_stomp_damage_t( p ) )
  {
    parse_options( options_str );
    may_miss = false;
    add_child( damage );
  }

  void execute() override
  {
    monk_spell_t::execute();
    // BracketSim legacy compatibility: the soulbind traits that ride this
    // covenant ability. The shared player_t layer owns them because they are
    // identical on every class bar a duration that tracks whatever ability
    // they ride; only the host and its cooldown are class knowledge.
    player->legacy_soulbinds.covenant_ability_cast( player, legacy_soulbind::COVENANT_NIGHT_FAE,
                                            cooldown );
    // The faeline itself - a patch of ground that re-triggered on later casts -
    // is not modelled; only the stomp's own damage is.
    damage->execute_on_target( target );

    // BracketSim legacy compatibility: Faeline Harmony. Shadowlands applied
    // Fae Exposure to every target the stomp reached, whether or not it was
    // damaged or healed.
    if ( p()->shadowlands_legacy.faeline_harmony )
      p()->get_target_data( target )->debuff.legacy_fae_exposure->trigger();
  }
};
}  // namespace actions

action_t *monk_t::create_action( std::string_view name, std::string_view options_str )
{
  using namespace actions;

  // BracketSim legacy compatibility: Shadowlands covenant abilities.
  if ( name == "weapons_of_order" && legacy_covenant.weapons_of_order->ok() )
    return new legacy_weapons_of_order_t( this, options_str );
  if ( name == "bonedust_brew" && legacy_covenant.bonedust_brew->ok() )
    return new legacy_bonedust_brew_t( this, options_str );
  if ( name == "fallen_order" && legacy_covenant.fallen_order->ok() )
    return new legacy_fallen_order_t( this, options_str );
  if ( name == "faeline_stomp" && legacy_covenant.faeline_stomp->ok() )
    return new legacy_faeline_stomp_t( this, options_str );
  // Monk
  if ( name == "snapshot_stats" )
    return new monk_snapshot_stats_t( this, options_str );
  if ( name == "auto_attack" )
    return new auto_attack_t( this, options_str );
  if ( name == "crackling_jade_lightning" )
    return new crackling_jade_lightning_t( this, options_str );
  if ( name == "tiger_palm" )
    return new tiger_palm_t( this, options_str );
  if ( name == "blackout_kick" )
    return new blackout_kick_t( this, options_str );
  if ( name == "expel_harm" )
    return new expel_harm_t( this, options_str );
  if ( name == "rising_sun_kick" )
    return new rising_sun_kick_t( this, options_str );
  if ( name == "rushing_wind_kick" )
    return new rushing_wind_kick_t( this, options_str );
  if ( name == "spinning_crane_kick" )
    return new spinning_crane_kick_t( this, options_str );
  if ( name == "fortifying_brew" )
    return new fortifying_brew_t( this, options_str );
  if ( name == "touch_of_death" )
    return new touch_of_death_t( this, options_str );

  // Brewmaster
  if ( name == "breath_of_fire" )
    return new breath_of_fire_t( this, options_str );
  if ( name == "celestial_brew" && talent.brewmaster.celestial_infusion->ok() )
    return new celestial_infusion_t( this, options_str );
  if ( name == "celestial_brew" )
    return new celestial_brew_t( this, options_str );
  if ( name == "celestial_infusion" )
    return new celestial_infusion_t( this, options_str );
  if ( name == "empty_the_cellar" )
    return new empty_the_cellar_t( this, options_str );
  if ( name == "exploding_keg" )
    return new exploding_keg_t( this, options_str );
  if ( name == "invoke_niuzao" )
    return new niuzao_spell_t( this, options_str );
  if ( name == "invoke_niuzao_the_black_ox" )
    return new niuzao_spell_t( this, options_str );
  if ( name == "keg_smash" )
    return new keg_smash_t( this, options_str );
  if ( name == "purifying_brew" )
    return new purifying_brew_t( this, options_str );
  if ( name == "chi_burst" )
    return new chi_burst_t( this, options_str );
  if ( name == "black_ox_brew" )
    return new black_ox_brew_t( this, options_str );

  // Windwalker
  if ( name == "fists_of_fury" )
    return new fists_of_fury_t( this, options_str );
  if ( name == "flying_serpent_kick" && talent.windwalker.slicing_winds->ok() )
    return new slicing_winds_t( this, options_str );
  if ( name == "flying_serpent_kick" )
    return new flying_serpent_kick_t( this, options_str );
  if ( name == "slicing_winds" )
    return new slicing_winds_t( this, options_str );
  if ( name == "touch_of_karma" )
    return new touch_of_karma_t( this, options_str );
  if ( name == "strike_of_the_windlord" )
    return new strike_of_the_windlord_t( this, options_str );
  if ( name == "invoke_xuen" )
    return new xuen_summon_t( this, options_str );
  if ( name == "invoke_xuen_the_white_tiger" )
    return new xuen_summon_t( this, options_str );
  if ( name == "rushing_jade_wind" )
    return new rushing_jade_wind_t( this, options_str );
  if ( name == "whirling_dragon_punch" )
    return new whirling_dragon_punch_t( this, options_str );
  if ( name == "zenith" )
    return new zenith_t( this, options_str );
  if ( name == "zenith_stomp" )
    return new zenith_stomp_t( this, zenith_stomp_t::ZENITH_STOMP_CAST, options_str );

  // Conduit of the Celestials
  if ( name == "celestial_conduit" )
    return new celestial_conduit_t( this, options_str );
  if ( name == "unity_within" )
    return new unity_within_t( this, options_str );

  return base_t::create_action( name, options_str );
}

void monk_t::trigger_celestial_fortune( action_state_t *s )
{
  if ( !baseline.brewmaster.celestial_fortune->ok() || s->action == action.celestial_fortune || s->result_raw == 0.0 )
  {
    return;
  }
  switch ( s->action->id )
  {
    case 143924:  // Leech
      return;
  }

  // flush out percent heals
  if ( s->action->type == ACTION_HEAL )
  {
    auto *heal_cast = debug_cast<heal_t *>( s->action );
    if ( ( s->result_type == result_amount_type::HEAL_DIRECT && heal_cast->base_pct_heal > 0 ) ||
         ( s->result_type == result_amount_type::HEAL_OVER_TIME && heal_cast->tick_pct_heal > 0 ) )
      return;
  }

  // Attempt to proc the heal
  if ( action.celestial_fortune && rng().roll( composite_melee_crit_chance() ) )
  {
    sim->print_debug( "triggering celestial fortune from (id: {}, name: {}) with (amount: {}) damage base",
                      s->action->id, s->action->name_str, s->result_amount );
    action.celestial_fortune->base_dd_max = action.celestial_fortune->base_dd_min = s->result_amount;
    action.celestial_fortune->execute();
  }
}

bool monk_t::validate_actor()
{
  // BracketSim legacy compatibility: let a healing spec through when the sim
  // asks for it, the way sc_priest.cpp already does for Discipline and Holy.
  //
  // Every healer in the author's roster is a character somebody plays, and refusing
  // to sim one at all is a worse answer than simming its damage with a stated
  // caveat. The app supplies a damage rotation (HEALER_DAMAGE_APL in
  // comparison.js) and labels the result; without this flag the run never got
  // far enough to use it.
  if ( specialization() == MONK_MISTWEAVER && !sim->allow_experimental_specializations )
  {
    if ( !quiet )
      sim->error( "Mistweaver Monk for {} is not currently supported.", *this );
    return false;
  }

  if ( main_hand_weapon.type == WEAPON_NONE )
  {
    if ( !quiet )
      sim->error( "{} has no weapon equipped at the Main-Hand slot.", *this );
    return false;
  }

  if ( main_hand_weapon.group() == WEAPON_2H && off_hand_weapon.group() == WEAPON_1H )
  {
    if ( !quiet )
      sim->error( "{} both a 1-hand and 2-hand weapon equipped at once.", *this );
    return false;
  }

  int expected = 13;
  for ( const auto &hero_tree : player_sub_trees )
  {
    int count = as<int>( range::count_if(
        player_traits, [ is_ptr = is_ptr(), hero_tree ]( std::tuple<talent_tree, unsigned, unsigned> entry ) {
          if ( std::get<talent_tree>( entry ) != talent_tree::HERO )
            return false;
          const trait_data_t *trait = trait_data_t::find( std::get<1>( entry ), is_ptr );
          if ( !trait )
            return false;
          return static_cast<hero_tree_e>( trait->id_sub_tree ) == hero_tree;
        } ) );

    // Report without counting the hidden talent that activates the subtree
    count -= 1;
    if ( count < expected && count != 0 )
    {
      // BracketSim legacy compatibility: a short hero tree is only an ERROR at
      // max level.
      //
      // the author's own level 80 Brewmaster (a test character) has ten of the thirteen,
      // which is simply what a level 80 monk's hero tree holds, and this gate
      // refused the actor for it - "No active players in sim!" on a profile
      // that is not wrong about anything. The message already guessed the
      // reason ("possibly low level"); below MAX_LEVEL that guess is the
      // answer, so it says so and carries on. At 90 a short tree really is a
      // malformed talent string and it still refuses.
      if ( true_level < MAX_LEVEL )
      {
        if ( !quiet )
          sim->error( "{} has {} of {} hero talents at level {}; that is what the tree holds "
                      "below max level, so it is used as it is.",
                      *this, count, expected, true_level );
      }
      else
      {
        sim->error( SEVERE, "Invalid Hero Talent tree, possibly low level. Found {} talents, expected {}.", count,
                    expected );
        return false;
      }
    }
  }

  switch ( specialization() )
  {
    case MONK_BREWMASTER:
    case MONK_WINDWALKER:
      return true;
    // BracketSim legacy compatibility: the SECOND gate on Mistweaver, and the
    // one that actually stopped it. Opening the first (the explicit refusal
    // above) left this switch to drop the actor through `default`, so the run
    // still ended with "No active players in sim!" - two doors, one lock each.
    case MONK_MISTWEAVER:
      if ( sim->allow_experimental_specializations )
        return true;
      sim->error( "Mistweaver Monk for {} is not currently supported.", *this );
      return false;
    default:
      sim->error( "No specialization was selected for {}.", *this );
      return false;
  }

  return false;
}

bool monk_t::validate_fight_style( fight_style_e style ) const
{
  if ( specialization() == MONK_BREWMASTER )
  {
    switch ( style )
    {
      case FIGHT_STYLE_DUNGEON_ROUTE:
      case FIGHT_STYLE_DUNGEON_SLICE:
        return false;
      default:
        return true;
    }
  }
  if ( specialization() == MONK_WINDWALKER )
  {
    switch ( style )
    {
      case FIGHT_STYLE_PATCHWERK:
      case FIGHT_STYLE_CASTING_PATCHWERK:
      case FIGHT_STYLE_DUNGEON_ROUTE:
      case FIGHT_STYLE_DUNGEON_SLICE:
        return true;
      default:
        return false;
    }
  }
  return true;
}

void monk_t::init_spells()
{
  base_t::init_spells();

  // BracketSim legacy compatibility: Battle for Azeroth Azerite traits.
  legacy_azerite.sweep_the_leg      = find_azerite_spell( "Sweep the Leg" );

  // BracketSim legacy compatibility: Shadowlands runeforge legendaries, keyed
  // off the bonus id the original legendary item carried.
  auto legacy = [ this ]( int bonus_id ) {
    return shadowlands_legacy.legacy_shadowlands_enabled &&
           range::any_of( items, [ bonus_id ]( const item_t &item ) {
             return range::contains( item.parsed.bonus_id, bonus_id );
           } );
  };

  shadowlands_legacy.fatal_touch       = legacy( 7081 );
  shadowlands_legacy.invokers_delight  = legacy( 7082 );
  shadowlands_legacy.xuens_battlegear  = legacy( 7070 );
  shadowlands_legacy.keefers_skyreach  = legacy( 7068 );
  shadowlands_legacy.stormstouts_last_keg = legacy( 7077 );

  // BracketSim legacy compatibility: Unity (bonus 8124), the 9.2 legendary
  // whose effect is whichever covenant legendary matches the covenant you are
  // in. A real Unity item carries 8124 and NOT the legendary's own bonus id,
  // so a power keyed only off its own id misses every Unity wearer.
  auto legacy_unity = [ & ]( int bonus_id, std::string_view covenant_name )
  {
    return legacy( bonus_id ) ||
           ( legacy( 8124 ) && util::str_compare_ci( legacy_covenant.chosen, covenant_name ) );
  };

  shadowlands_legacy.bountiful_brew     = legacy_unity( 7707, "necrolord" );
  shadowlands_legacy.faeline_harmony    = legacy_unity( 7721, "night_fae" );
  shadowlands_legacy.sinister_teachings = legacy_unity( 7726, "venthyr" );
  shadowlands_legacy.call_to_arms       = legacy_unity( 7718, "kyrian" );

  // BracketSim legacy compatibility: Shadowlands covenant abilities.
  auto covenant = [ this ]( std::string_view name, unsigned id ) {
    return ( shadowlands_legacy.legacy_shadowlands_enabled &&
             util::str_compare_ci( legacy_covenant.chosen, name ) )
               ? find_spell( id )
               : spell_data_t::not_found();
  };

  legacy_covenant.weapons_of_order = covenant( "kyrian", 310454 );
  legacy_covenant.bonedust_brew    = covenant( "necrolord", 325216 );
  legacy_covenant.faeline_stomp    = covenant( "night_fae", 327104 );
  legacy_covenant.fallen_order     = covenant( "venthyr", 326860 );

  // BracketSim legacy compatibility: report the covenant abilities this
  // actor can cast, so player_t::init_actions() can put them into the
  // rotation. SimulationCraft's own action lists never press them.
  if ( legacy_covenant.weapons_of_order->ok() )
    legacy_apl_actions.emplace_back( "weapons_of_order" );
  if ( legacy_covenant.bonedust_brew->ok() )
    legacy_apl_actions.emplace_back( "bonedust_brew" );
  if ( legacy_covenant.faeline_stomp->ok() )
    legacy_apl_actions.emplace_back( "faeline_stomp" );
  if ( legacy_covenant.fallen_order->ok() )
    legacy_apl_actions.emplace_back( "fallen_order" );
  // Fallen Order's adepts are summons with their own stats, so their output is
  // FITTED from one Brewmaster log rather than read from data. Brewmaster is
  // also the only spec ever observed: Windwalker's adepts cast Fists of Fury
  // and Mistweaver's cast Enveloping Mist, and neither adept spell id is
  // known, so those specs carry a Brewmaster number.

  legacy_covenant.weapons_of_order_debuff =
      legacy_covenant.weapons_of_order->ok() ? find_spell( 312106 ) : spell_data_t::not_found();
  legacy_covenant.bonedust_brew_damage =
      legacy_covenant.bonedust_brew->ok() ? find_spell( 325217 ) : spell_data_t::not_found();
  legacy_conduits.parse();

  legacy_covenant.faeline_stomp_damage =
      legacy_covenant.faeline_stomp->ok() ? find_spell( 327264 ) : spell_data_t::not_found();
  legacy_azerite.glory_of_the_dawn  = find_azerite_spell( "Glory of the Dawn" );
  legacy_azerite.strength_of_spirit = find_azerite_spell( "Strength of Spirit" );
  legacy_azerite.sunrise_technique  = find_azerite_spell( "Sunrise Technique" );
  legacy_azerite.boiling_brew       = find_azerite_spell( "Boiling Brew" );
  legacy_azerite.elusive_footwork   = find_azerite_spell( "Elusive Footwork" );
  legacy_azerite.fit_to_burst       = find_azerite_spell( "Fit to Burst" );
  legacy_azerite.niuzaos_blessing   = find_azerite_spell( "Niuzao's Blessing" );
  legacy_azerite.staggering_strikes = find_azerite_spell( "Staggering Strikes" );
  legacy_azerite.training_of_niuzao = find_azerite_spell( "Training of Niuzao" );
  legacy_azerite.dance_of_chiji     = find_azerite_spell( "Dance of Chi-Ji" );
  legacy_azerite.fury_of_xuen       = find_azerite_spell( "Fury of Xuen" );
  legacy_azerite.iron_fists         = find_azerite_spell( "Iron Fists" );
  legacy_azerite.meridian_strikes   = find_azerite_spell( "Meridian Strikes" );
  legacy_azerite.open_palm_strikes  = find_azerite_spell( "Open Palm Strikes" );
  legacy_azerite.pressure_point     = find_azerite_spell( "Pressure Point" );
  legacy_azerite.swift_roundhouse   = find_azerite_spell( "Swift Roundhouse" );

  if ( legacy_azerite.boiling_brew.ok() )
    legacy_boiling_brew_rppm = get_rppm( "legacy_boiling_brew", legacy_azerite.boiling_brew.spell() );

  // BracketSim legacy compatibility: Bountiful Brew's own RPPM, from 356592.
  if ( shadowlands_legacy.bountiful_brew )
    legacy_bountiful_brew_rppm = get_rppm( "legacy_bountiful_brew", find_spell( 356592 ) );

  auto _CT = [ & ]( std::string_view name ) { return find_talent_spell( talent_tree::CLASS, name, specialization() ); };
  auto _ST = [ & ]( std::string_view name ) {
    return find_talent_spell( talent_tree::SPECIALIZATION, name, specialization() );
  };
  auto _HT = [ & ]( std::string_view name ) { return find_talent_spell( talent_tree::HERO, name ); };

  auto _STID = [ & ]( int id ) { return find_talent_spell( talent_tree::SPECIALIZATION, id, specialization() ); };

  /*
   * Always use the most specific possible `find_spell` method that you can use.
   * If you don't, you will have auras getting applied to incorrect specs, actions
   * usable for incorrect specs, etc.
   *
   * Place all relevant spells near the parent spell or talent
   * i.e. spells and talents relevant to Fortifying Brew go near the Fortifying Brew
   * talent.
   *
   * Default spells for the class and specs get placed in `baseline`.
   * If they are not owned by a specific spec, they get placed in `monk.`
   * If they are owned by a specific spec, they get placed in `spec_name`.
   *
   * Talent spells for the class and specs get placed in `talents`.
   * If they are class talents, they get placed in `monk`.
   * If they are spec or hero talents, they get placed in `spec/ht_name`.
   */

  // monk_t::baseline::monk
  {
    baseline.monk.aura                     = find_spell( 137022 );  // TODO: Blacklist 130610, use name instead
    baseline.monk.critical_strikes         = find_specialization_spell( "Critical Strikes" );
    baseline.monk.two_hand_adjustment      = find_specialization_spell( "Windwalker Monk Two-Hand Adjustment" );
    baseline.monk.leather_specialization   = find_specialization_spell( "Leather Specialization" );
    baseline.monk.expel_harm               = find_spell( 322101 );
    baseline.monk.expel_harm_damage        = find_spell( 115129 );
    baseline.monk.blackout_kick            = find_class_spell( "Blackout Kick" );
    baseline.monk.crackling_jade_lightning = find_class_spell( "Crackling Jade Lightning" );
    baseline.monk.leg_sweep                = find_class_spell( "Leg Sweep" );
    baseline.monk.mystic_touch             = find_spell( 113746 );
    baseline.monk.provoke                  = find_class_spell( "Provoke" );
    baseline.monk.roll                     = find_class_spell( "Roll" );
    baseline.monk.spinning_crane_kick      = find_spell( 101546 );
    baseline.monk.tiger_palm               = find_spell( 100780 );
    baseline.monk.touch_of_death           = find_spell( 322109 );
    baseline.monk.vivify                   = find_class_spell( "Vivify" );
  }

  // monk_t::baseline::brewmaster
  {
    baseline.brewmaster.mastery                = find_mastery_spell( MONK_BREWMASTER );
    baseline.brewmaster.aura                   = find_specialization_spell( "Brewmaster Monk" );
    baseline.brewmaster.aura_2                 = find_specialization_spell( 462087 );
    baseline.brewmaster.aura_3                 = find_specialization_spell( 1246978 );
    baseline.brewmaster.aura_4                 = find_specialization_spell( 1258153 );
    baseline.brewmaster.brewmasters_balance    = find_specialization_spell( "Brewmaster's Balance" );
    baseline.brewmaster.celestial_fortune      = find_specialization_spell( "Celestial Fortune" );
    baseline.brewmaster.celestial_fortune_heal = find_spell( 216521 );  // TODO: Can you be more specific?
    baseline.brewmaster.expel_harm_rank_2      = find_rank_spell( "Expel Harm", "Rank 2", specialization() );
    baseline.brewmaster.blackout_kick          = find_spell( 205523 );
    baseline.brewmaster.stagger_self_damage    = find_spell( 124255 );
    baseline.brewmaster.light_stagger          = find_spell( 124275 );
    baseline.brewmaster.moderate_stagger       = find_spell( 124274 );
    baseline.brewmaster.heavy_stagger          = find_spell( 124273 );
    baseline.brewmaster.spinning_crane_kick    = find_specialization_spell( "Spinning Crane Kick" );
    baseline.brewmaster.spinning_crane_kick_rank_2 =
        find_rank_spell( "Spinning Crane Kick", "Rank 2", specialization() );
    baseline.brewmaster.touch_of_death_rank_3 = find_rank_spell( "Touch of Death", "Rank 3", specialization() );
  }

  // monk_t::baseline::windwalker
  {
    baseline.windwalker.mastery                   = find_mastery_spell( MONK_WINDWALKER );
    baseline.windwalker.aura                      = find_specialization_spell( "Windwalker Monk" );
    baseline.windwalker.aura_2                    = find_specialization_spell( 462091 );
    baseline.windwalker.aura_3                    = find_specialization_spell( 1222923 );
    baseline.windwalker.aura_4                    = find_specialization_spell( 1258122 );
    baseline.windwalker.blackout_kick_rank_2      = find_rank_spell( "Blackout Kick", "Rank 2", MONK_WINDWALKER );
    baseline.windwalker.blackout_kick_rank_3      = find_rank_spell( "Blackout Kick", "Rank 3", MONK_WINDWALKER );
    baseline.windwalker.combat_conditioning       = find_specialization_spell( "Combat Conditioning" );
    baseline.windwalker.flying_serpent_kick       = find_specialization_spell( "Flying Serpent Kick" );
    baseline.windwalker.touch_of_death_rank_3     = find_rank_spell( "Touch of Death", "Rank 3", specialization() );
    baseline.windwalker.touch_of_karma            = find_specialization_spell( "Touch of Karma" );
    baseline.windwalker.touch_of_karma_buff       = find_spell( 125174 );
    baseline.windwalker.touch_of_karma_tick       = find_spell( 124280 );
    baseline.windwalker.empowered_tiger_lightning = find_specialization_spell( "Empowered Tiger Lightning" );
    baseline.windwalker.empowered_tiger_lightning_damage = find_spell( 335913 );
  }

  // monk_t::talent::monk
  {
    talent.monk.soothing_mist                = _CT( "Soothing Mist" );
    talent.monk.paralysis                    = _CT( "Paralysis" );
    talent.monk.rising_sun_kick              = _CT( "Rising Sun Kick" );
    talent.monk.stagger                      = _CT( "Stagger" );
    talent.monk.elusive_mists                = _CT( "Elusive Mists" );
    talent.monk.tigers_lust                  = _CT( "Tiger's Lust" );
    talent.monk.crashing_momentum            = _CT( "Crashing Momentum" );
    talent.monk.disable                      = _CT( "Disable" );
    talent.monk.fast_feet                    = _CT( "Fast Feet" );
    talent.monk.grace_of_the_crane           = _CT( "Grace of the Crane" );
    talent.monk.bounding_agility             = _CT( "Bounding Agility" );
    talent.monk.calming_presence             = _CT( "Calming Presence" );
    talent.monk.winds_reach                  = _CT( "Wind's Reach" );
    talent.monk.detox                        = _CT( "Detox" );
    talent.monk.vivacious_vivification       = _CT( "Vivacious Vivification" );
    talent.monk.jade_walk                    = _CT( "Jade Walk" );
    talent.monk.pressure_points              = _CT( "Pressure Points" );
    talent.monk.spear_hand_strike            = _CT( "Spear Hand Strike" );
    talent.monk.ancient_arts                 = _CT( "Ancient Arts" );
    talent.monk.chi_wave                     = _CT( "Chi Wave" );
    talent.monk.chi_wave_buff                = find_spell( 450380 );
    talent.monk.chi_wave_driver              = find_spell( 115098 );
    talent.monk.chi_wave_damage              = find_spell( 132467 );
    talent.monk.chi_wave_heal                = find_spell( 132463 );
    talent.monk.chi_burst                    = _CT( "Chi Burst" );
    talent.monk.chi_burst_buff               = find_spell( 460490 );
    talent.monk.chi_burst_projectile         = find_spell( 461404 );
    talent.monk.chi_burst_damage             = find_spell( 148135 );
    talent.monk.chi_burst_heal               = find_spell( 130654 );
    talent.monk.tiger_fang                   = _CT( "Tiger Fang" );
    talent.monk.transcendence                = _CT( "Transcendence" );
    talent.monk.energy_transfer              = _CT( "Energy Transfer" );
    talent.monk.celerity                     = _CT( "Celerity" );
    talent.monk.chi_torpedo                  = _CT( "Chi Torpedo" );
    talent.monk.stillstep_coil               = _CT( "Stillstep Coil" );
    talent.monk.quick_footed                 = _CT( "Quick Footed" );
    talent.monk.hasty_provocation            = _CT( "Hasty Provocation" );
    talent.monk.ferocity_of_xuen             = _CT( "Ferocity of Xuen" );
    talent.monk.ring_of_peace                = _CT( "Ring of Peace" );
    talent.monk.song_of_chi_ji               = _CT( "Song of Chi-Ji" );
    talent.monk.spirits_essence              = _CT( "Spirit's Essence" );
    talent.monk.tiger_tail_sweep             = _CT( "Tiger Tail Sweep" );
    talent.monk.improved_touch_of_death      = _CT( "Improved Touch of Death" );
    talent.monk.vigorous_expulsion           = _CT( "Vigorous Expulsion" );
    talent.monk.yulons_grace                 = _CT( "Yu'lon's Grace" );
    talent.monk.yulons_grace_buff            = find_spell( 414143 );
    talent.monk.peace_and_prosperity         = _CT( "Peace and Prosperity" );
    talent.monk.fortifying_brew              = _CT( "Fortifying Brew" );
    talent.monk.fortifying_brew_buff         = find_spell( 120954 );
    talent.monk.dance_of_the_wind            = _CT( "Dance of the Wind" );
    talent.monk.save_them_all                = _CT( "Save Them All" );
    talent.monk.swift_art                    = _CT( "Swift Art" );
    talent.monk.strength_of_spirit           = _CT( "Strength of Spirit" );
    talent.monk.profound_rebuttal            = _CT( "Profound Rebuttal" );
    talent.monk.summon_black_ox_statue       = _CT( "Summon Black Ox Statue" );
    talent.monk.zenith_stomp                 = _CT( "Zenith Stomp" );
    talent.monk.zenith_stomp_damage          = find_spell( 1272696 );
    talent.monk.zenith_stomp_buff            = find_spell( 1291484 );
    talent.monk.ironshell_brew               = _CT( "Ironshell Brew" );
    talent.monk.expeditious_fortification    = _CT( "Expeditious Fortification" );
    talent.monk.diffuse_magic                = _CT( "Diffuse Magic" );
    talent.monk.celestial_determination      = _CT( "Celestial Determination" );
    talent.monk.chi_proficiency              = _CT( "Chi Proficiency" );
    talent.monk.healing_winds                = _CT( "Healing Winds" );
    talent.monk.windwalking                  = _CT( "Windwalking" );
    talent.monk.chi_transfer                 = _CT( "Chi Transfer" );
    talent.monk.martial_instincts            = _CT( "Martial Instincts" );
    talent.monk.lighter_than_air             = _CT( "Lighter Than Air" );
    talent.monk.flow_of_chi                  = _CT( "Flow of Chi" );
    talent.monk.escape_from_reality          = _CT( "Escape from Reality" );
    talent.monk.transcendence_linked_spirits = _CT( "Transcendence: Linked Spirits" );
    talent.monk.fatal_touch                  = _CT( "Fatal Touch" );
    talent.monk.rushing_reflexes             = _CT( "Rushing Reflexes" );
  }

  // monk_t::talent::brewmaster
  {
    talent.brewmaster.keg_smash                        = _ST( "Keg Smash" );
    talent.brewmaster.purifying_brew                   = _ST( "Purifying Brew" );
    talent.brewmaster.shuffle                          = _ST( "Shuffle" );
    talent.brewmaster.shuffle_buff                     = find_spell( 215479 );
    talent.brewmaster.august_blessing                  = _ST( "August Blessing" );
    talent.brewmaster.staggering_strikes               = _ST( "Staggering Strikes" );
    talent.brewmaster.quick_sip                        = _ST( "Quick Sip" );
    talent.brewmaster.elixir_of_determination          = _ST( "Elixir of Determination" );
    talent.brewmaster.elixir_of_determination_cooldown = find_spell( 455180 );
    talent.brewmaster.improved_blackout_kick           = _ST( "Improved Blackout Kick" );
    talent.brewmaster.swift_as_a_coursing_river        = _ST( "Swift as a Coursing River" );
    talent.brewmaster.gift_of_the_ox                   = _ST( "Gift of the Ox" );
    talent.brewmaster.gift_of_the_ox_buff              = find_spell( 124506 );
    talent.brewmaster.gift_of_the_ox_heal_trigger      = find_spell( 124507 );
    talent.brewmaster.gift_of_the_ox_heal_expire       = find_spell( 178173 );
    talent.brewmaster.special_delivery                 = _ST( "Special Delivery" );
    talent.brewmaster.special_delivery_missile         = find_spell( 196732 );
    talent.brewmaster.rushing_jade_wind                = _ST( "Rushing Jade Wind" );
    talent.brewmaster.spirit_of_the_ox                 = _ST( "Spirit of the Ox" );
    talent.brewmaster.jade_flash                       = _ST( "Jade Flash" );
    talent.brewmaster.celestial_brew                   = _ST( "Celestial Brew" );
    talent.brewmaster.celestial_infusion               = _ST( "Celestial Infusion" );
    talent.brewmaster.niuzaos_resolve                  = _ST( "Niuzao's Resolve" );
    talent.brewmaster.niuzaos_resolve_hot              = find_spell( 1241109 );
    talent.brewmaster.celestial_flames                 = _ST( "Celestial Flames" );
    talent.brewmaster.celestial_flames_damage          = find_spell( 1263667 );
    talent.brewmaster.shadowboxing_treads              = _ST( "Shadowboxing Treads" );
    talent.brewmaster.fluidity_of_motion               = _ST( "Fluidity of Motion" );
    talent.brewmaster.elusive_footwork                 = _ST( "Elusive Footwork" );
    talent.brewmaster.one_with_the_wind                = _ST( "One With the Wind" );
    talent.brewmaster.breath_of_fire                   = _ST( "Breath of Fire" );
    talent.brewmaster.breath_of_fire_dot               = find_spell( 123725 );
    talent.brewmaster.gai_plins_imperial_brew          = _ST( "Gai Plin's Imperial Brew" );
    talent.brewmaster.gai_plins_imperial_brew_heal     = find_spell( 383701 );
    talent.brewmaster.light_brewing                    = _ST( "Light Brewing" );
    talent.brewmaster.training_of_niuzao               = _ST( "Training of Niuzao" );
    talent.brewmaster.pretense_of_instability          = _ST( "Pretense of Instability" );
    talent.brewmaster.scalding_brew                    = _ST( "Scalding Brew" );
    talent.brewmaster.salsalabims_strength             = _ST( "Sal'salabim's Strength" );
    talent.brewmaster.fortifying_brew_determination    = _ST( "Fortifying Brew: Determination" );
    talent.brewmaster.bob_and_weave                    = _ST( "Bob and Weave" );
    talent.brewmaster.black_ox_brew                    = _ST( "Black Ox Brew" );
    talent.brewmaster.walk_with_the_ox                 = _ST( "Walk With the Ox" );
    talent.brewmaster.walk_with_the_ox_stomp           = find_spell( 1242373 );
    talent.brewmaster.zen_state                        = _ST( "Zen State" );
    talent.brewmaster.tranquil_spirit                  = _ST( "Tranquil Spirit" );
    talent.brewmaster.face_palm                        = _ST( "Face Palm" );
    talent.brewmaster.dragonfire_brew                  = _ST( "Dragonfire Brew" );
    talent.brewmaster.dragonfire_brew_hit              = find_spell( 387621 );
    talent.brewmaster.charred_passions                 = _ST( "Charred Passions" );
    talent.brewmaster.charred_passions_damage          = find_spell( 386959 );
    talent.brewmaster.high_tolerance                   = _ST( "High Tolerance" );
    talent.brewmaster.press_the_advantage              = _ST( "Press the Advantage" );
    talent.brewmaster.press_the_advantage_damage       = find_spell( 418360 );
    talent.brewmaster.press_the_advantage_tiger_palm   = find_spell( 331433 );
    talent.brewmaster.blackout_combo                   = _ST( "Blackout Combo" );
    talent.brewmaster.anvil_and_stave                  = _ST( "Anvil and Stave" );
    talent.brewmaster.counterstrike                    = _ST( "Counterstrike" );
    talent.brewmaster.exploding_keg                    = _ST( "Exploding Keg" );
    talent.brewmaster.ox_stance                        = _ST( "Ox Stance" );
    talent.brewmaster.ox_stance_buff                   = find_spell( 455071 );
    talent.brewmaster.awakening_spirit                 = _ST( "Awakening Spirit" );
    talent.brewmaster.vital_flame                      = _ST( "Vital Flame" );
    talent.brewmaster.vital_flame_heal                 = find_spell( 1263408 );
    talent.brewmaster.invoke_niuzao_the_black_ox       = _ST( "Invoke Niuzao, the Black Ox" );
    talent.brewmaster.invoke_niuzao_the_black_ox_npc   = find_spell( 123904 );
    talent.brewmaster.invoke_niuzao_the_black_ox_stomp = find_spell( 227291 );
    talent.brewmaster.fuel_on_the_fire                 = _ST( "Fuel on the Fire" );
    talent.brewmaster.fuel_on_the_fire_buff            = find_spell( 1262035 );
    talent.brewmaster.fuel_on_the_fire_damage          = find_spell( 1262159 );
    talent.brewmaster.empty_the_cellar                 = _ST( "Empty the Cellar" );
    talent.brewmaster.empty_the_cellar_buff            = find_spell( 1262768 );
    talent.brewmaster.empty_the_cellar_driver          = find_spell( 1263438 );
    talent.brewmaster.empty_the_cellar_damage          = find_spell( 1262765 );
    talent.brewmaster.keg_volley                       = _ST( "Keg Volley" );
    talent.brewmaster.stormstouts_last_keg             = _ST( "Stormstout's Last Keg" );
    talent.brewmaster.heart_of_the_ox                  = _ST( "Heart of the Ox" );
    talent.brewmaster.mighty_stomp                     = _ST( "Mighty Stomp" );
    talent.brewmaster.bring_me_another_1               = _ST( "Bring Me Another" );
    talent.brewmaster.bring_me_another_2               = _STID( 1265138 );
    talent.brewmaster.bring_me_another_3               = _STID( 1265141 );
    talent.brewmaster.empty_barrel_damage              = find_spell( 1265133 );
    talent.brewmaster.refreshing_drink_buff            = find_spell( 1265140 );
    talent.brewmaster.refreshing_drink_hot             = find_spell( 1265145 );
  }

  // monk_t::talent::windwalker
  {
    talent.windwalker.fists_of_fury                   = _ST( "Fists of Fury" );
    talent.windwalker.momentum_boost                  = _ST( "Momentum Boost" );
    talent.windwalker.momentum_boost_speed            = find_spell( 451298 );
    talent.windwalker.combat_wisdom                   = _ST( "Combat Wisdom" );
    talent.windwalker.combat_wisdom_buff              = find_spell( 129914 );
    talent.windwalker.combat_wisdom_expel_harm        = find_spell( 451968 );
    talent.windwalker.sharp_reflexes                  = _ST( "Sharp Reflexes" );
    talent.windwalker.touch_of_the_tiger              = _ST( "Touch of the Tiger" );
    talent.windwalker.ferociousness                   = _ST( "Ferociousness" );
    talent.windwalker.hardened_soles                  = _ST( "Hardened Soles" );
    talent.windwalker.ascension                       = _ST( "Ascension" );  // TODO: NYI: EFFECT 2 ENERGY REGEN
    talent.windwalker.dual_threat                     = _ST( "Dual Threat" );
    talent.windwalker.dual_threat_damage              = find_spell( 451839 );
    talent.windwalker.teachings_of_the_monastery      = _ST( "Teachings of the Monastery" );
    talent.windwalker.teachings_of_the_monastery_buff = find_spell( 202090 );
    talent.windwalker.teachings_of_the_monastery_blackout_kick = find_spell( 228649 );
    talent.windwalker.glory_of_the_dawn                        = _ST( "Glory of the Dawn" );
    talent.windwalker.glory_of_the_dawn_damage                 = find_spell( 392959 );
    talent.windwalker.crane_vortex                             = _ST( "Crane Vortex" );
    talent.windwalker.meridian_strikes                         = _ST( "Meridian Strikes" );
    talent.windwalker.rising_star                              = _ST( "Rising Star" );
    talent.windwalker.zenith                                   = _ST( "Zenith" );
    talent.windwalker.hit_combo                                = _ST( "Hit Combo" );
    talent.windwalker.hit_combo_buff                           = find_spell( 196741 );
    talent.windwalker.brawlers_intensity                       = _ST( "Brawler's Intensity" );
    talent.windwalker.jade_ignition                            = _ST( "Jade Ignition" );
    talent.windwalker.cyclones_drift                           = _ST( "Cyclone's Drift" );
    talent.windwalker.crashing_fists                           = _ST( "Crashing Fists" );
    talent.windwalker.drinking_horn_cover                      = _ST( "Drinking Horn Cover" );
    talent.windwalker.spiritual_focus                          = _ST( "Spiritual Focus" );
    talent.windwalker.obsidian_spiral                          = _ST( "Obsidian Spiral" );
    talent.windwalker.obsidian_spiral_energize                 = find_spell( 1249833 );
    talent.windwalker.combo_breaker                            = _ST( "Combo Breaker" );
    talent.windwalker.combo_breaker_buff                       = find_spell( 116768 );
    talent.windwalker.dance_of_chiji =
        find_talent_spell( talent_tree::SPECIALIZATION, "Dance of Chi-Ji", MONK_WINDWALKER );
    // do not use talent.windwalker.dance_of_chiji->effectN( 1 ).trigger() to avoid talent known dependency
    talent.windwalker.dance_of_chiji_buff            = find_spell( 325202 );
    talent.windwalker.shadowboxing_treads            = _STID( 392982 );
    talent.windwalker.strike_of_the_windlord         = _ST( "Strike of the Windlord" );
    talent.windwalker.whirling_dragon_punch          = _ST( "Whirling Dragon Punch" );
    talent.windwalker.whirling_dragon_punch_aoe_tick = find_spell( 158221 );
    talent.windwalker.whirling_dragon_punch_st_tick  = find_spell( 451767 );
    talent.windwalker.whirling_dragon_punch_buff     = find_spell( 196742 );
    talent.windwalker.energy_burst                   = _ST( "Energy Burst" );
    talent.windwalker.inner_peace                    = _ST( "Inner Peace" );
    talent.windwalker.sequenced_strikes              = _ST( "Sequenced Strikes" );
    talent.windwalker.sunfire_spiral                 = _ST( "Sunfire Spiral" );
    talent.windwalker.communion_with_wind            = _ST( "Communion With Wind" );
    talent.windwalker.revolving_whirl                = _ST( "Revolving Whirl" );
    talent.windwalker.echo_technique                 = _ST( "Echo Technique" );
    talent.windwalker.universal_energy               = _ST( "Universal Energy" );
    talent.windwalker.memory_of_the_monastery        = _ST( "Memory of the Monastery" );
    talent.windwalker.rushing_wind_kick              = _ST( "Rushing Wind Kick" );
    talent.windwalker.rushing_wind_kick_buff         = find_spell( 1250554 );
    talent.windwalker.rushing_wind_kick_action       = find_spell( 467307 );
    talent.windwalker.xuens_battlegear               = _ST( "Xuen's Battlegear" );
    talent.windwalker.thunderfist                    = _ST( "Thunderfist" );
    talent.windwalker.thunderfist_buff               = find_spell( 393565 );
    talent.windwalker.knowledge_of_the_broken_temple = _ST( "Knowledge of the Broken Temple" );
    talent.windwalker.slicing_winds                  = _ST( "Slicing Winds" );
    talent.windwalker.slicing_winds_damage           = find_spell( 1217411 );
    talent.windwalker.jadefire_stomp                 = _ST( "Jadefire Stomp" );
    talent.windwalker.jadefire_stomp_damage          = find_spell( 1248815 );
    talent.windwalker.jadefire_stomp_targeting       = find_spell( 1248812 );
    talent.windwalker.skyfire_heel                   = _ST( "Skyfire Heel" );
    talent.windwalker.skyfire_heel_damage            = find_spell( 1248712 );
    talent.windwalker.skyfire_heel_buff              = find_spell( 1248705 );
    talent.windwalker.harmonic_combo                 = _ST( "Harmonic Combo" );
    talent.windwalker.flurry_of_xuen                 = _ST( "Flurry of Xuen" );
    talent.windwalker.flurry_of_xuen_driver          = find_spell( 452117 );
    talent.windwalker.martial_agility                = _ST( "Martial Agility" );
    talent.windwalker.airborne_rhythm                = _ST( "Airborne Rhythm" );
    talent.windwalker.airborne_rhythm_energize       = find_spell( 1248835 );
    talent.windwalker.hurricanes_vault               = _ST( "Hurricane's Vault" );
    talent.windwalker.path_of_jade                   = _ST( "Path of Jade" );
    talent.windwalker.singularly_focused_jade        = _ST( "Singularly Focused Jade" );
    talent.windwalker.tigereye_brew_1      = find_talent_spell( talent_tree::SPECIALIZATION, "Tigereye Brew", 1 );
    talent.windwalker.tigereye_brew_1_buff = find_spell( 1261724 );
    talent.windwalker.tigereye_brew_2      = find_talent_spell( talent_tree::SPECIALIZATION, "Tigereye Brew", 2 );
    talent.windwalker.tigereye_brew_3      = find_talent_spell( talent_tree::SPECIALIZATION, "Tigereye Brew", 3 );
    talent.windwalker.tigereye_brew_3_buff = find_spell( 1262042 );
  }

  // monk_t::talent::conduit_of_the_celestials
  {
    talent.conduit_of_the_celestials.invoke_xuen_the_white_tiger       = _HT( "Invoke Xuen, the White Tiger" );
    talent.conduit_of_the_celestials.invoke_xuen_the_white_tiger_npc   = find_spell( 132578 );
    talent.conduit_of_the_celestials.crackling_tiger_lightning_driver  = find_spell( 123999 );
    talent.conduit_of_the_celestials.temple_training                   = _HT( "Temple Training" );
    talent.conduit_of_the_celestials.xuens_guidance                    = _HT( "Xuen's Guidance" );
    talent.conduit_of_the_celestials.courage_of_the_white_tiger        = _HT( "Courage of the White Tiger" );
    talent.conduit_of_the_celestials.courage_of_the_white_tiger_buff   = find_spell( 460127 );
    talent.conduit_of_the_celestials.courage_of_the_white_tiger_damage = find_spell( 457917 );
    talent.conduit_of_the_celestials.courage_of_the_white_tiger_heal   = find_spell( 443106 );
    talent.conduit_of_the_celestials.restore_balance                   = _HT( "Restore Balance" );
    talent.conduit_of_the_celestials.xuens_bond                        = _HT( "Xuen's Bond" );
    talent.conduit_of_the_celestials.heart_of_the_jade_serpent         = _HT( "Heart of the Jade Serpent" );
    talent.conduit_of_the_celestials.heart_of_the_jade_serpent_buff    = find_spell( 443421 );
    talent.conduit_of_the_celestials.chijis_swiftness                  = _HT( "Chi-Ji's Swiftness" );
    talent.conduit_of_the_celestials.strength_of_the_black_ox          = _HT( "Strength of the Black Ox" );
    talent.conduit_of_the_celestials.strength_of_the_black_ox_buff     = find_spell( 443112 );
    talent.conduit_of_the_celestials.strength_of_the_black_ox_absorb   = find_spell( 443113 );
    talent.conduit_of_the_celestials.strength_of_the_black_ox_damage   = find_spell( 443127 );
    talent.conduit_of_the_celestials.path_of_the_falling_star          = _HT( "Path of the Falling Star" );
    talent.conduit_of_the_celestials.yulons_avatar                     = _HT( "Yu'lon's Avatar" );
    talent.conduit_of_the_celestials.yulons_avatar_buff                = find_spell( 1238904 );
    talent.conduit_of_the_celestials.niuzaos_protection                = _HT( "Niuzao's Protection" );
    talent.conduit_of_the_celestials.jade_sanctuary                    = _HT( "Jade Sanctuary" );
    talent.conduit_of_the_celestials.jade_sanctuary_buff               = find_spell( 448508 );
    talent.conduit_of_the_celestials.celestial_conduit                 = _HT( "Celestial Conduit" );
    talent.conduit_of_the_celestials.celestial_conduit_action          = find_spell( 443028 );
    talent.conduit_of_the_celestials.celestial_conduit_damage          = find_spell( 443038 );
    talent.conduit_of_the_celestials.celestial_conduit_heal            = find_spell( 443039 );
    talent.conduit_of_the_celestials.inner_compass                     = _HT( "Inner Compass" );
    talent.conduit_of_the_celestials.inner_compass_ox_stance_buff      = find_spell( 443574 );
    talent.conduit_of_the_celestials.inner_compass_tiger_stance_buff   = find_spell( 443575 );
    talent.conduit_of_the_celestials.inner_compass_serpent_stance_buff = find_spell( 443576 );
    talent.conduit_of_the_celestials.inner_compass_crane_stance_buff   = find_spell( 443572 );
    talent.conduit_of_the_celestials.flowing_wisdom                    = _HT( "Flowing Wisdom" );
    talent.conduit_of_the_celestials.unity_within                      = _HT( "Unity Within" );
    talent.conduit_of_the_celestials.unity_within_buff                 = find_spell( 443592 );
    talent.conduit_of_the_celestials.unity_within_heart_of_the_jade_serpent_buff = find_spell( 443616 );
    talent.conduit_of_the_celestials.unity_within_dmg_mult                       = find_spell( 443591 );
    talent.conduit_of_the_celestials.flight_of_the_red_crane_damage              = find_spell( 443611 );
  }

  // monk_t::talent::master_of_harmony
  {
    talent.master_of_harmony.aspect_of_harmony             = _HT( "Aspect of Harmony" );
    talent.master_of_harmony.aspect_of_harmony_driver      = find_spell( 450567 );
    talent.master_of_harmony.aspect_of_harmony_accumulator = find_spell( 450521 );
    talent.master_of_harmony.aspect_of_harmony_spender     = find_spell( 450711 );
    talent.master_of_harmony.aspect_of_harmony_damage      = find_spell( 450763 );
    talent.master_of_harmony.aspect_of_harmony_heal        = find_spell( 450769 );
    talent.master_of_harmony.manifestation                 = _HT( "Manifestation" );
    talent.master_of_harmony.purified_spirit               = _HT( "Purified Spirit" );
    talent.master_of_harmony.purified_spirit_damage        = find_spell( 450820 );
    talent.master_of_harmony.purified_spirit_heal          = find_spell( 450805 );
    talent.master_of_harmony.harmonic_gambit               = _HT( "Harmonic Gambit" );
    talent.master_of_harmony.balanced_stratagem            = _HT( "Balanced Stratagem" );
    talent.master_of_harmony.balanced_stratagem_magic      = find_spell( 451508 );
    talent.master_of_harmony.balanced_stratagem_physical   = find_spell( 451514 );
    talent.master_of_harmony.harmonic_surge                = _HT( "Harmonic Surge" );
    talent.master_of_harmony.harmonic_surge_buff           = find_spell( 1270990 );
    talent.master_of_harmony.harmonic_surge_damage         = find_spell( 1271011 );
    talent.master_of_harmony.harmonic_surge_heal           = find_spell( 1271045 );
    talent.master_of_harmony.tigers_vigor                  = _HT( "Tiger's Vigor" );
    talent.master_of_harmony.roar_from_the_heavens         = _HT( "Roar from the Heavens" );
    talent.master_of_harmony.endless_draught               = _HT( "Endless Draught" );
    talent.master_of_harmony.mantra_of_purity              = _HT( "Mantra of Purity" );
    talent.master_of_harmony.mantra_of_tenacity            = _HT( "Mantra of Tenacity" );
    talent.master_of_harmony.potential_energy              = _HT( "Potential Energy" );
    talent.master_of_harmony.overwhelming_force            = _HT( "Overwhelming Force" );
    talent.master_of_harmony.overwhelming_force_damage     = find_spell( 452333 );
    talent.master_of_harmony.path_of_resurgence            = _HT( "Path of Resurgence" );
    talent.master_of_harmony.way_of_a_thousand_strikes     = _HT( "Way of a Thousand Strikes" );
    talent.master_of_harmony.clarity_of_purpose            = _HT( "Clarity of Purpose" );
    talent.master_of_harmony.meditative_focus              = _HT( "Meditative Focus" );
    talent.master_of_harmony.coalescence                   = _HT( "Coalescence" );
  }

  // monk_t::talent::shado_pan
  {
    talent.shado_pan.flurry_strikes                           = _HT( "Flurry Strikes" );
    talent.shado_pan.flurry_charge                            = find_spell( 451021 );
    talent.shado_pan.flurry_strike                            = find_spell( 450617 );
    talent.shado_pan.pride_of_pandaria                        = _HT( "Pride of Pandaria" );
    talent.shado_pan.high_impact                              = _HT( "High Impact" );
    talent.shado_pan.high_impact_debuff                       = find_spell( 451037 );
    talent.shado_pan.veterans_eye                             = _HT( "Veteran's Eye" );
    talent.shado_pan.martial_precision                        = _HT( "Martial Precision" );
    talent.shado_pan.shado_over_the_battlefield               = _HT( "Shado Over the Battlefield" );
    talent.shado_pan.flurry_strike_shado_over_the_battlefield = find_spell( 451250 );
    talent.shado_pan.combat_stance                            = _HT( "Combat Stance" );
    talent.shado_pan.initiators_edge                          = _HT( "Initiator's Edge" );
    talent.shado_pan.one_versus_many                          = _HT( "One Versus Many" );
    talent.shado_pan.whirling_steel                           = _HT( "Whirling Steel" );
    talent.shado_pan.predictive_training                      = _HT( "Predictive Training" );
    talent.shado_pan.stand_ready                              = _HT( "Stand Ready" );
    talent.shado_pan.stand_ready_buff                         = find_spell( 1237196 );
    talent.shado_pan.against_all_odds                         = _HT( "Against All Odds" );
    talent.shado_pan.efficient_training                       = _HT( "Efficient Training" );
    talent.shado_pan.vigilant_watch                           = _HT( "Vigilant Watch" );
    talent.shado_pan.weapons_of_the_wall                      = _HT( "Weapons of the Wall" );
    talent.shado_pan.wisdom_of_the_wall                       = _HT( "Wisdom of the Wall" );
  }

  // monk_t::tier
  {
    tier.mid1.brm_2pc            = sets->set( MONK_BREWMASTER, MID1, B2 );
    tier.mid1.brm_4pc            = sets->set( MONK_BREWMASTER, MID1, B4 );
    tier.mid1.brm_4pc_extra_kick = find_spell( 1272464 );

    tier.mid2.ww_4pc_buff = sets->set( MONK_WINDWALKER, MID2, B4 )->effectN( 1 ).trigger();

    if ( sets->set( MONK_BREWMASTER, MID2, B2 ) )
    {
      tier.mid2.brm_2pc_buff   = find_spell( 1301477 );
      tier.mid2.brm_2pc_damage = find_spell( 1301619 );
    }

    if ( sets->set( MONK_BREWMASTER, MID2, B4 ) )
    {
      tier.mid2.brm_4pc_action = find_spell( 1301417 );
      tier.mid2.brm_4pc_damage = find_spell( 1301418 );
      tier.mid2.brm_4pc_debuff = find_spell( 1301410 );
    }
  }

  // Register passives
  // Instant Spells with a reduced GCD
  register_passive_affect_list( baseline.brewmaster.aura_2, affect_list_t( 3 ).remove_label( 640 ) );
  // Instant Spells with a reduced GCD
  register_passive_affect_list( baseline.brewmaster.aura_4,
                                affect_list_t( 7 ).remove_spell( 115098,  // Chi Wave Action
                                                                 130654,  // Chi Burst Heal
                                                                 148135,  // Chi Burst Damage
                                                                 185099,  // Rising Sun Kick Damage
                                                                 228649,  // Teachings of the Monastery Blackout Kick
                                                                 261682,  // Chi Burst Energize
                                                                 280184,  // Unknown Leg Sweep?
                                                                 322111,  // Touch of Death Damage
                                                                 331433,  // Press the Advantage Tiger Palm
                                                                 392959,  // Glory of the Dawn
                                                                 450342,  // Crashing Momentum
                                                                 451968,  // Combat Wisdom Expel Harm Heal
                                                                 468179,  // Rushing Wind Kick Damage
                                                                 1249625  // Zenith
                                                                 ) );
  // Chargeless Spells with a reduced Charge Cooldown
  register_passive_affect_list( talent.brewmaster.fluidity_of_motion,
                                affect_list_t( 2 ).remove_spell( 100784 ) );  // Blackout Kick
  // Instant Spells with a reduced GCD
  register_passive_affect_list( baseline.windwalker.aura_2, affect_list_t( 2 ).remove_label( 640 ) );
  // Instant Spells with a reduced GCD
  register_passive_affect_list( baseline.windwalker.aura_4,
                                affect_list_t( 8 ).remove_spell( 115098,  // Chi Wave Action
                                                                 130654,  // Chi Burst Heal
                                                                 148135,  // Chi Burst Damage
                                                                 185099,  // Rising Sun Kick Damage
                                                                 228649,  // Teachings of the Monastery Blackout Kick
                                                                 261682,  // Chi Burst Energize
                                                                 280184,  // Old Leg Sweep Modifier
                                                                 322111,  // Touch of Death Damage
                                                                 331433,  // Press the Advantage Tiger Palm
                                                                 392959,  // Glory of the Dawn
                                                                 450342,  // Crashing Momentum
                                                                 451968,  // Combat Wisdom Expel Harm Heal
                                                                 468179,  // Rushing Wind Kick Damage
                                                                 1249625  // Zenith
                                                                 ) );
  // Scripted enablement based on specialization
  register_passive_effect_mask( talent.shado_pan.efficient_training, specialization() == MONK_WINDWALKER
                                                                         ? effect_mask_t( true ).disable( 5 )
                                                                         : effect_mask_t( true ) );

  // brewmaster
  deregister_passive_spell( talent.brewmaster.heart_of_the_ox );
  // windwalker
  deregister_passive_spell( talent.windwalker.ferociousness );
  deregister_passive_spell( talent.windwalker.martial_agility );

  parse_all_class_passives();
  parse_all_passive_talents();
  parse_all_passive_sets();
  parse_raid_buffs();
}

void monk_t::init_background_actions()
{
  using namespace actions;
  base_t::init_background_actions();

  // General
  action.chi_wave = new chi_wave_t( this );

  // Conduit of the Celestials
  bool uw  = talent.conduit_of_the_celestials.unity_within->ok();
  bool cwt = talent.conduit_of_the_celestials.courage_of_the_white_tiger->ok() || uw;
  bool sbt = talent.conduit_of_the_celestials.strength_of_the_black_ox->ok() || uw;

  if ( cwt )
    action.courage_of_the_white_tiger = courage_of_the_white_tiger_t( this );

  if ( sbt )
    action.strength_of_the_black_ox = strength_of_the_black_ox_t( this );

  if ( uw )
    action.flight_of_the_red_crane = flight_of_the_red_crane_t( this );

  // Shado-Pan
  action.flurry_strikes = new flurry_strikes_t( talent.shado_pan.flurry_strikes->ok(), this );

  // Brewmaster
  if ( specialization() == MONK_BREWMASTER )
  {
    action.special_delivery  = new special_delivery_t( this );
    action.celestial_fortune = new celestial_fortune_t( this );
    action.exploding_keg     = new exploding_keg_proc_t( this );
    action.refreshing_drink  = new refreshing_drink_t( this );
    action.vital_flame       = new vital_flame_t( this );
    action.walk_with_the_ox  = new stomp_t( this );
  }

  // Windwalker
  if ( specialization() == MONK_WINDWALKER )
  {
    action.empowered_tiger_lightning = new empowered_tiger_lightning_t( this );
    action.flurry_of_xuen            = new flurry_of_xuen_t( this );
    action.combat_wisdom_eh          = new expel_harm_t( this, "" );
  }

  // Tier
  if ( sets->has_set_bonus( MONK_BREWMASTER, MID2, B4 ) )
    action.mid2_brm_4pc = new monk_spell_t( this, "aflame", tier.mid2.brm_4pc_damage );

  // Legacy Azerite: Glory of the Dawn's extra Rising Sun Kick hit.
  if ( legacy_azerite.glory_of_the_dawn.ok() )
  {
    auto gotd = new monk_melee_attack_t( this, "legacy_glory_of_the_dawn", find_spell( 288636 ) );
    gotd->background = true;
    action.legacy_glory_of_the_dawn = gotd;
  }

  // Legacy Azerite: Fit to Burst's periodic self-heal.
  if ( legacy_azerite.fit_to_burst.ok() )
  {
    auto ftb = new monk_heal_t( this, "legacy_fit_to_burst", find_spell( 275894 ) );
    ftb->background = ftb->proc = true;
    ftb->target = this;
    ftb->base_dd_min = ftb->base_dd_max = legacy_azerite.fit_to_burst.value();
    action.legacy_fit_to_burst = ftb;
  }

  // BracketSim legacy compatibility: Bonedust Brew's shadow echo.
  if ( legacy_covenant.bonedust_brew->ok() )
  {
    auto bdb = new monk_spell_t( this, "bonedust_brew_damage", legacy_covenant.bonedust_brew_damage );
    bdb->background = bdb->proc = true;
    bdb->may_crit = false;
    action.legacy_bonedust_brew_damage = bdb;
  }

  // Legacy Azerite: Sunrise Technique's extra hit.
  if ( legacy_azerite.sunrise_technique.ok() )
  {
    auto st = new monk_melee_attack_t( this, "legacy_sunrise_technique", find_spell( 275673 ) );
    st->background = true;
    st->may_crit = true;
    st->trigger_gcd = timespan_t::zero();
    st->min_gcd = timespan_t::zero();
    st->base_dd_min = st->base_dd_max = legacy_azerite.sunrise_technique.value();
    action.legacy_sunrise_technique = st;
  }
}

void monk_t::init_base_stats()
{
  if ( base.distance < 1 )
  {
    if ( specialization() == MONK_MISTWEAVER )
      base.distance = 40;
    else
      base.distance = 5;
  }

  switch ( specialization() )
  {
    case MONK_BREWMASTER:
      base.attack_power_per_agility              = 1.0;
      resources.active_resource[ RESOURCE_MANA ] = false;
      break;
    case MONK_MISTWEAVER:
      // BracketSim (29 Sep 2026): Mistweaver had no primary-stat conversion at all - intellect gave no spell power and
      // nothing gave attack power, so its kicks and melee scaled off nothing (41 DPS at level 30). Spell power from
      // intellect, and the passive 1258138 "Mistweaver Monk": attack power = 104% of spell power (aura 404).
      base.spell_power_per_intellect = 1.0;
      for ( const spelleffect_data_t& e : find_spell( 1258138 )->effects() )
        if ( e.type() == E_APPLY_AURA && e.subtype() == A_OVERRIDE_AP_PER_SP )
          base.attack_power_per_spell_power = e.percent();
      break;
    case MONK_WINDWALKER:
      if ( base.distance < 1 )
        base.distance = 5;
      base.attack_power_per_agility              = 1.0;
      resources.active_resource[ RESOURCE_MANA ] = false;
      break;
    default:
      break;
  }

  base_t::init_base_stats();
}

void monk_t::init_scaling()
{
  base_t::init_scaling();

  scaling->disable( STAT_STRENGTH );
  scaling->disable( STAT_INTELLECT );
  scaling->disable( STAT_SPELL_POWER );

  scaling->enable( STAT_AGILITY );
  scaling->enable( STAT_WEAPON_DPS );
  scaling->enable( STAT_STAMINA );

  if ( off_hand_weapon.type != WEAPON_NONE )
    scaling->enable( STAT_WEAPON_OFFHAND_DPS );
}

// TODO: move these to somewhere sensible
struct self_damage_override : stagger_impl::self_damage_t<monk_t>
{
  self_damage_override( monk_t *player, stagger_impl::stagger_effect_t<monk_t> *stagger_effect )
    : stagger_impl::self_damage_t<monk_t>( player, stagger_effect )
  {
    // not automatic
    dot_duration = player->baseline.brewmaster.heavy_stagger->duration();
    dot_duration +=
        timespan_t::from_seconds( player->talent.brewmaster.bob_and_weave->effectN( 1 ).base_value() / 10.0 );
  }
};

struct debuff_override_t : stagger_impl::debuff_t<monk_t>
{
  using base_t = stagger_impl::debuff_t<monk_t>;
  debuff_override_t( monk_t *player, const stagger_data_t *parent_data, const level_data_t *data )
    : base_t( player, parent_data, data )
  {
    set_stack_change_callback( [ player ]( buff_t *, int old_, int new_ ) {
      if ( old_ )
        player->buff.training_of_niuzao->expire();
      if ( new_ )
        player->buff.training_of_niuzao->trigger();
    } );
  }
};

struct training_of_niuzao_buff_t : buffs::monk_buff_t<>
{
  training_of_niuzao_buff_t( monk_t *player )
    : buffs::monk_buff_t<>( player, "training_of_niuzao", player->talent.brewmaster.training_of_niuzao )
  {
    set_default_value( 0.0 );
    set_pct_buff_type( STAT_PCT_BUFF_MASTERY );
  }

  bool trigger( int = -1, double = DEFAULT_VALUE(), double chance = -1.0,
                timespan_t duration = timespan_t::min() ) override
  {
    double v = p().find_stagger( "Stagger" )->level_index() * data().effectN( 1 ).base_value();
    return base_t::trigger( 1, v, chance, duration );
  }
};

void monk_t::create_buffs()
{
  // BracketSim legacy compatibility: Weapons of Order. Only effect 1, the
  // Mastery gain, is applied: effects 2 to 5 are unused template rows that the
  // ability never granted in game.
  buff.legacy_weapons_of_order =
      make_buff( this, "weapons_of_order", legacy_covenant.weapons_of_order )
          ->set_default_value_from_effect( 1, 0.01 )
          ->set_pct_buff_type( STAT_PCT_BUFF_MASTERY )
          ->set_chance( legacy_covenant.weapons_of_order->ok() ? 1.0 : 0.0 );

  // BracketSim legacy compatibility: Invoker's Delight (7082) was detected but
  // never had a buff to grant. Its haste comes straight from spell 338321.
  buff.legacy_invokers_delight =
      make_buff( this, "legacy_invokers_delight", find_spell( 338321 ) )
          ->set_default_value_from_effect( 1, 0.01 )
          ->set_pct_buff_type( STAT_PCT_BUFF_HASTE )
          ->set_chance( shadowlands_legacy.invokers_delight ? 1.0 : 0.0 );

  create_stagger<debuff_override_t, self_damage_override>(
      { baseline.brewmaster.stagger_self_damage,
        { { baseline.brewmaster.light_stagger, 0.0 },
          { baseline.brewmaster.moderate_stagger, 0.2 },
          { baseline.brewmaster.heavy_stagger, 0.6 },
          { spell_data_t::nil(), 10.0 } },
        // BracketSim legacy compatibility: "legacy_staggering_strikes" MUST be
        // in this list. purify_flat() asserts the token is registered and then
        // does mitigated_by_ability[ token ]->add(), and the assert is compiled
        // out in a release build - so a missing token default-constructs a null
        // pointer and dereferences it. The azerite trait crashed the sim with
        // an access violation on BOTH monk specs until this line was added.
        { "quick_sip", "staggering_strikes", "legacy_staggering_strikes", "touch_of_death",
          "purifying_brew", "tranquil_spirit_eh", "tranquil_spirit_goto" },
        [ this ]() { return specialization() == MONK_BREWMASTER; },
        [ this ]( school_e, result_amount_type, const action_state_t *state ) {
          if ( state->action->id == baseline.brewmaster.stagger_self_damage->id() )
            return false;
          return true;
        },
        [ this ]( school_e school, result_amount_type, action_state_t *state ) {
          double multiplier     = talent.monk.stagger->effectN( 1 ).percent();
          double stagger_rating = agility() * multiplier;

          if ( talent.brewmaster.fortifying_brew_determination->ok() && buff.fortifying_brew->up() )
            stagger_rating *= 1.0 + talent.monk.fortifying_brew_buff->effectN( 6 ).percent();

          if ( buff.shuffle->up() )
            stagger_rating *= 1.0 + talent.brewmaster.shuffle_buff->effectN( 1 ).percent();

          // multiplier is not available in spell data :(
          if ( buff.ox_stance->up() &&
               state->result_amount / current_health() > talent.brewmaster.ox_stance->effectN( 1 ).percent() )
          {
            stagger_rating *= 1.0 + 0.4;
            buff.ox_stance->decrement();
          }

          // multiplier is not available in spell data :(
          if ( talent.brewmaster.zen_state->ok() )
            stagger_rating *= functions::missing_health_percentage_t( this )( 1.3 );

          double k = dbc->armor_mitigation_constant( state->target->level() );
          k *= dbc->get_armor_constant_mod( difficulty_e::MYTHIC );

          double stagger_percent = stagger_rating / ( stagger_rating + k );

          // order of operations here is untestable with current in game values
          if ( school != SCHOOL_PHYSICAL )
            stagger_percent *= talent.monk.stagger->effectN( 5 ).percent();

          return std::min( stagger_percent, 0.99 );
        } } );

  base_t::create_buffs();

  // BracketSim legacy compatibility: Battle for Azeroth Azerite trait buffs.
  buff.swift_roundhouse = make_buff( this, "swift_roundhouse", find_spell( 278710 ) )
                              ->set_default_value( legacy_azerite.swift_roundhouse.value() );
  buff.legacy_pressure_point = make_buff( this, "legacy_pressure_point", find_spell( 247255 ) )
                                   ->set_default_value( legacy_azerite.pressure_point.value() );
  // Legacy Azerite: Dance of Chi-Ji. Deliberately NOT buff.dance_of_chiji -
  // that one belongs to the modern talent and is built with chance 1.0 when the
  // talent is absent, which would fire this on every Chi spend instead of on
  // the trait's own proc rate. 286586 carries that rate.
  buff.legacy_dance_of_chiji = make_buff( this, "legacy_dance_of_chiji", find_spell( 286587 ) )
                                   ->set_trigger_spell( find_spell( 286586 ) )
                                   ->set_chance( legacy_azerite.dance_of_chiji.ok()
                                                     ? find_spell( 286586 )->proc_chance()
                                                     : 0.0 );
  // Legacy Azerite: Fury of Xuen. 287062 stacks on Combo Strikes and its third
  // effect is the per-stack chance, stored as a percentage of a percent.
  // 287063 is the Haste reward and its duration is how long the tiger stays.
  buff.legacy_fury_of_xuen_stacks =
      make_buff( this, "legacy_fury_of_xuen_stacks", find_spell( 287062 ) )
          ->set_default_value( ( find_spell( 287062 )->effectN( 3 ).base_value() / 100 ) * 0.01 )
          ->set_chance( legacy_azerite.fury_of_xuen.ok() ? 1.0 : 0.0 );
  buff.legacy_fury_of_xuen_haste =
      make_buff<stat_buff_t>( this, "legacy_fury_of_xuen_haste", find_spell( 287063 ) )
          ->add_stat( STAT_HASTE_RATING, legacy_azerite.fury_of_xuen.value() );

  // Legacy Azerite: Iron Fists raises Fists of Fury's crit chance when it lands
  // on enough targets.
  buff.legacy_iron_fists = make_buff<stat_buff_t>( this, "legacy_iron_fists", find_spell( 272806 ) )
                               ->add_stat( STAT_CRIT_RATING, legacy_azerite.iron_fists.value() )
                               ->set_chance( legacy_azerite.iron_fists.ok() ? 1.0 : 0.0 );
  // Legacy Azerite: Fit to Burst heals over its stacks after a Purifying Brew
  // used out of Heavy Stagger.
  buff.legacy_fit_to_burst = make_buff( this, "legacy_fit_to_burst", find_spell( 275893 ) )
                                 ->set_chance( legacy_azerite.fit_to_burst.ok() ? 1.0 : 0.0 )
                                 ->set_period( 1_s )
                                 ->set_tick_behavior( buff_tick_behavior::CLIP )
                                 ->set_tick_callback( [ this ]( buff_t *, int, timespan_t ) {
                                   if ( action.legacy_fit_to_burst )
                                     action.legacy_fit_to_burst->execute();
                                 } );
  // Legacy Azerite: Sunrise Technique arms the extra hit after a Rising Sun Kick.
  buff.legacy_sunrise_technique = make_buff( this, "legacy_sunrise_technique", find_spell( 273298 ) )
                                      ->set_chance( legacy_azerite.sunrise_technique.ok() ? 1.0 : 0.0 );
  // Legacy Azerite: Training of Niuzao grants Mastery scaled by the Stagger band.
  buff.legacy_training_of_niuzao = make_buff<stat_buff_t>( this, "legacy_training_of_niuzao", find_spell( 278767 ) )
                                       ->add_stat( STAT_MASTERY_RATING, legacy_azerite.training_of_niuzao.value() )
                                       ->set_max_stack( 3 )
                                       ->set_chance( legacy_azerite.training_of_niuzao.ok() ? 1.0 : 0.0 );

  // Monk
  buff.combat_wisdom = make_buff_fallback( talent.windwalker.combat_wisdom->ok(), this, "combat_wisdom",
                                           talent.windwalker.combat_wisdom_buff )
                           ->set_trigger_spell( talent.windwalker.combat_wisdom )
                           ->set_default_value_from_effect( 1 );

  buff.chi_wave = make_buff_fallback( talent.monk.chi_wave->ok(), this, "chi_wave", talent.monk.chi_wave_buff );

  buff.fortifying_brew = make_buff_fallback<buffs::fortifying_brew_t>(
      talent.monk.fortifying_brew->ok() && specialization() == MONK_BREWMASTER, this, "fortifying_brew" );

  buff.rushing_jade_wind = make_buff_fallback( talent.brewmaster.rushing_jade_wind->ok(), this, "rushing_jade_wind",
                                               talent.brewmaster.rushing_jade_wind );

  buff.yulons_grace = make_buff_fallback<absorb_buff_t>( talent.monk.yulons_grace->ok(), this, "yulons_grace",
                                                         talent.monk.yulons_grace_buff );

  // Brewmaster
  buff.training_of_niuzao = make_buff_fallback<training_of_niuzao_buff_t>( talent.brewmaster.training_of_niuzao->ok(),
                                                                           this, "training_of_niuzao" );
  buff.ox_stance =
      make_buff_fallback( talent.brewmaster.ox_stance->ok(), this, "ox_stance", talent.brewmaster.ox_stance_buff );

  buff.blackout_combo = make_buff_fallback( talent.brewmaster.blackout_combo->ok(), this, "blackout_combo",
                                            talent.brewmaster.blackout_combo->effectN( 5 ).trigger() );

  buff.celestial_flames = make_buff_fallback( talent.brewmaster.celestial_flames->ok(), this, "celestial_flames",
                                              talent.brewmaster.celestial_flames->effectN( 1 ).trigger() );

  buff.charred_passions = make_buff_fallback( talent.brewmaster.charred_passions->ok(), this, "charred_passions",
                                              talent.brewmaster.charred_passions->effectN( 1 ).trigger() )
                              ->set_trigger_spell( talent.brewmaster.charred_passions );

  buff.counterstrike = make_buff_fallback( talent.brewmaster.counterstrike->ok(), this, "counterstrike",
                                           talent.brewmaster.counterstrike->effectN( 1 ).trigger() )
                           ->set_cooldown( talent.brewmaster.counterstrike->internal_cooldown() );

  buff.elusive_brawler = make_buff_fallback( specialization() == MONK_BREWMASTER, this, "elusive_brawler",
                                             baseline.brewmaster.mastery->effectN( 3 ).trigger() )
                             ->add_invalidate( CACHE_DODGE );

  buff.empty_barrel = make_buff_fallback<buffs::empty_barrel_buff_t>( talent.brewmaster.bring_me_another_1->ok(), this,
                                                                      "empty_barrel" );

  buff.empty_the_cellar = make_buff_fallback( talent.brewmaster.empty_the_cellar->ok(), this, "empty_the_cellar",
                                              talent.brewmaster.empty_the_cellar_buff );

  buff.exploding_keg = make_buff_fallback( talent.brewmaster.exploding_keg->ok(), this, "exploding_keg",
                                           talent.brewmaster.exploding_keg );

  if ( talent.brewmaster.gift_of_the_ox->ok() || talent.brewmaster.spirit_of_the_ox->ok() )
    buff.gift_of_the_ox = new buffs::gift_of_the_ox_t( this );
  else if ( specialization() == MONK_BREWMASTER )
    buff_t::make_fallback( this, "gift_of_the_ox", this );

  buff.expel_harm_accumulator =
      make_buff_fallback( talent.brewmaster.gift_of_the_ox->ok() || talent.brewmaster.spirit_of_the_ox->ok(), this,
                          "expel_harm_accumulator" )
          ->set_can_cancel( true )
          ->set_quiet( true )
          ->set_cooldown( 0_ms )
          ->set_duration( 1_ms )
          ->set_max_stack( 1 );

  buff.elixir_of_determination = make_buff_fallback<buffs::elixir_of_determination_t>(
      talent.brewmaster.elixir_of_determination->ok(), this, "elixir_of_determination",
      talent.brewmaster.elixir_of_determination->effectN( 1 ).trigger() );

  buff.fuel_on_the_fire = make_buff_fallback( talent.brewmaster.fuel_on_the_fire->ok(), this, "fuel_on_the_fire",
                                              talent.brewmaster.fuel_on_the_fire_buff );

  buff.invoke_niuzao = make_buff_fallback( talent.brewmaster.invoke_niuzao_the_black_ox->ok(), this,
                                           "invoke_niuzao_the_black_ox", talent.brewmaster.invoke_niuzao_the_black_ox )
                           ->set_default_value_from_effect( 2 )
                           ->set_cooldown( timespan_t::zero() )
                           ->add_invalidate( CACHE_MASTERY );

  buff.niuzaos_resolve =
      make_buff_fallback<buffs::niuzaos_resolve_t>( talent.brewmaster.niuzaos_resolve->ok(), this, "niuzaos_resolve" );

  buff.press_the_advantage =
      make_buff_fallback( talent.brewmaster.press_the_advantage->ok(), this, "press_the_advantage",
                          talent.brewmaster.press_the_advantage->effectN( 2 ).trigger() )
          ->set_default_value_from_effect( 1 );

  buff.pretense_of_instability =
      make_buff_fallback( talent.brewmaster.pretense_of_instability->ok(), this, "pretense_of_instability",
                          talent.brewmaster.pretense_of_instability->effectN( 1 ).trigger() )
          ->set_trigger_spell( talent.brewmaster.pretense_of_instability )
          ->add_invalidate( CACHE_DODGE );

  buff.refreshing_drink = make_buff_fallback( talent.brewmaster.bring_me_another_3->ok(), this, "refreshing_drink",
                                              talent.brewmaster.refreshing_drink_buff );

  // the override is a little weird, we'll just let this always init
  buff.shuffle = make_buff<buffs::shuffle_t>( this );

  buff.swift_as_a_coursing_river =
      make_buff_fallback( talent.brewmaster.swift_as_a_coursing_river->ok(), this, "swift_as_a_coursing_river",
                          talent.brewmaster.swift_as_a_coursing_river->effectN( 1 ).trigger() )
          ->set_trigger_spell( talent.brewmaster.swift_as_a_coursing_river );

  buff.mid2_brm_2pc =
      make_buff_fallback( sets->has_set_bonus( MONK_BREWMASTER, MID2, B2 ), this, "hot_potato", tier.mid2.brm_2pc_buff )
          ->set_trigger_spell( talent.brewmaster.breath_of_fire );

  // Windwalker
  buff.teachings_of_the_monastery =
      make_buff_fallback( talent.windwalker.teachings_of_the_monastery->ok(), this, "teachings_of_the_monastery",
                          talent.windwalker.teachings_of_the_monastery_buff )
          ->set_trigger_spell( talent.windwalker.teachings_of_the_monastery )
          ->set_default_value_from_effect( 1 );

  // Create the buff even if untalented - it is possible to get a Blackout Kick! proc without the talent from other
  // sources.
  buff.combo_breaker = make_buff_fallback( specialization() == MONK_WINDWALKER, this, "combo_breaker",
                                           talent.windwalker.combo_breaker_buff )
                           ->set_trigger_spell( talent.windwalker.combo_breaker )
                           ->set_chance( !talent.windwalker.combo_breaker->ok()
                                             ? 1.0
                                             : talent.windwalker.combo_breaker->effectN( 1 ).percent() );

  buff.combo_strikes =
      make_buff_fallback( baseline.windwalker.mastery->ok(), this, "combo_strikes" )
          ->set_trigger_spell( baseline.windwalker.mastery )
          ->set_duration( timespan_t::from_minutes( 60 ) )
          ->set_quiet( true );  // In-game does not show this buff but I would like to use it for background stuff

  // Create the buff even if untalented - it is possible to get a dance of chiji proc without the talent from other
  // sources.
  buff.dance_of_chiji =
      make_buff_fallback( specialization() == MONK_WINDWALKER, this, "dance_of_chiji",
                          talent.windwalker.dance_of_chiji_buff )
          ->set_trigger_spell( talent.windwalker.dance_of_chiji )
          ->set_chance( !talent.windwalker.dance_of_chiji->ok() ? 1.0
                                                                : talent.windwalker.dance_of_chiji->proc_chance() )
          ->set_rppm( !talent.windwalker.dance_of_chiji->ok() ? RPPM_NONE : RPPM_HASTE );

  buff.hit_combo =
      make_buff_fallback( talent.windwalker.hit_combo->ok(), this, "hit_combo", talent.windwalker.hit_combo_buff )
          ->set_default_value_from_effect( 1 );

  buff.flurry_of_xuen =
      make_buff_fallback( talent.windwalker.flurry_of_xuen->ok(), this, "flurry_of_xuen",
                          talent.windwalker.flurry_of_xuen_driver )
          ->set_tick_callback( [ this ]( buff_t * /* b */, int, timespan_t ) { action.flurry_of_xuen->execute(); } )
          ->set_tick_behavior( buff_tick_behavior::CLIP )
          ->set_refresh_behavior( buff_refresh_behavior::DURATION )
          ->set_freeze_stacks( true );

  buff.invoke_xuen = make_buff_fallback<buffs::invoke_xuen_the_white_tiger_buff_t>(
                         talent.conduit_of_the_celestials.invoke_xuen_the_white_tiger->ok(), this,
                         "invoke_xuen_the_white_tiger", talent.conduit_of_the_celestials.invoke_xuen_the_white_tiger )
                         ->add_invalidate( CACHE_CRIT_CHANCE );

  buff.momentum_boost_damage =
      make_buff_fallback( talent.windwalker.momentum_boost->ok(), this, "momentum_boost_damage",
                          talent.windwalker.momentum_boost->effectN( 1 ).trigger() )
          ->set_default_value_from_effect( 1 );

  buff.momentum_boost_speed = make_buff_fallback( talent.windwalker.momentum_boost->ok(), this, "momentum_boost_speed",
                                                  talent.windwalker.momentum_boost_speed )
                                  ->set_trigger_spell( talent.windwalker.momentum_boost );

  buff.thunderfist = make_buff_fallback( talent.windwalker.thunderfist->ok(), this, "thunderfist",
                                         talent.windwalker.thunderfist_buff );

  buff.touch_of_death_ww = new buffs::touch_of_death_ww_buff_t( this, "touch_of_death_ww", spell_data_t::nil() );

  buff.touch_of_karma =
      new buffs::touch_of_karma_buff_t( this, "touch_of_karma", baseline.windwalker.touch_of_karma_buff );

  buff.whirling_dragon_punch = make_buff_fallback<buffs::whirling_dragon_punch_buff_t>(
      talent.windwalker.whirling_dragon_punch->ok(), this, "whirling_dragon_punch" );

  buff.zenith = make_buff_fallback<buffs::zenith_t>( talent.windwalker.zenith->ok(), this, "zenith" )
                    ->set_expire_callback( [ & ]( buff_t *, int, timespan_t ) { buff.zenith_stomp->expire(); } );

  buff.zenith_stomp = make_buff_fallback( talent.windwalker.tigereye_brew_3->ok(), this, "zenith_stomp",
                                          talent.monk.zenith_stomp_buff );
  if ( wowv_l( { 12, 1, 0 } ) && !buff.zenith_stomp->is_fallback )
    buff.zenith_stomp->modify_initial_stack( 1 );

  buff.rushing_wind_kick = make_buff_fallback( talent.windwalker.rushing_wind_kick->ok(), this, "rushing_wind_kick",
                                               talent.windwalker.rushing_wind_kick_buff );

  buff.tigereye_brew_1 = make_buff_fallback( talent.windwalker.tigereye_brew_1->ok(), this, "tigereye_brew_1",
                                             talent.windwalker.tigereye_brew_1_buff )
                             ->set_default_value( talent.windwalker.tigereye_brew_1_buff->effectN( 1 ).percent() );

  buff.tigereye_brew_1_accumulator =
      make_buff_fallback( talent.windwalker.tigereye_brew_1->ok(), this, "tigereye_brew_1_accumulator" )
          ->set_quiet( true )
          ->set_cooldown( 0_ms )
          ->set_duration( 6000_s )
          ->set_max_stack( 1 );

  buff.tigereye_brew_3 = make_buff_fallback( talent.windwalker.tigereye_brew_3->ok(), this, "tigereye_brew_3",
                                             talent.windwalker.tigereye_brew_3_buff )
                             ->set_cooldown( talent.windwalker.tigereye_brew_3->internal_cooldown() );

  buff.mid2_ww_4pc = make_buff_fallback( sets->has_set_bonus( MONK_WINDWALKER, MID2, B4 ), this, "unbroken_rhythm",
                                         tier.mid2.ww_4pc_buff );

  // Conduit of the Celestials
  buff.celestial_conduit =
      make_buff_fallback( talent.conduit_of_the_celestials.celestial_conduit->ok(), this, "celestial_conduit",
                          talent.conduit_of_the_celestials.celestial_conduit->effectN( 1 ).trigger() );

  buff.courage_of_the_white_tiger = make_buff_fallback(
      talent.conduit_of_the_celestials.courage_of_the_white_tiger->ok(), this, "courage_of_the_white_tiger",
      talent.conduit_of_the_celestials.courage_of_the_white_tiger_buff );

  buff.heart_of_the_jade_serpent = make_buff_fallback(
      talent.conduit_of_the_celestials.heart_of_the_jade_serpent->ok(), this, "heart_of_the_jade_serpent",
      talent.conduit_of_the_celestials.heart_of_the_jade_serpent_buff );

  buff.heart_of_the_jade_serpent_yulons_avatar = make_buff_fallback(
      talent.conduit_of_the_celestials.yulons_avatar->ok(), this, "heart_of_the_jade_serpent_yulons_avatar",
      talent.conduit_of_the_celestials.yulons_avatar_buff );

  buff.heart_of_the_jade_serpent_unity_within =
      make_buff_fallback( talent.conduit_of_the_celestials.unity_within->ok(), this,
                          "heart_of_the_jade_serpent_unity_within",
                          talent.conduit_of_the_celestials.unity_within_heart_of_the_jade_serpent_buff )
          ->set_stack_change_callback( [ & ]( buff_t *, int old_, int new_ ) {
            if ( new_ && !old_ )
              buff.heart_of_the_jade_serpent->expire();
          } );

  buff.inner_compass_crane_stance =
      make_buff_fallback( talent.conduit_of_the_celestials.inner_compass->ok(), this, "crane_stance",
                          talent.conduit_of_the_celestials.inner_compass_crane_stance_buff );

  buff.inner_compass_ox_stance =
      make_buff_fallback( talent.conduit_of_the_celestials.inner_compass->ok(), this, "ox_stance",
                          talent.conduit_of_the_celestials.inner_compass_ox_stance_buff );

  buff.inner_compass_serpent_stance =
      make_buff_fallback( talent.conduit_of_the_celestials.inner_compass->ok(), this, "serpent_stance",
                          talent.conduit_of_the_celestials.inner_compass_serpent_stance_buff );

  buff.inner_compass_tiger_stance =
      make_buff_fallback( talent.conduit_of_the_celestials.inner_compass->ok(), this, "tiger_stance",
                          talent.conduit_of_the_celestials.inner_compass_tiger_stance_buff );

  buff.jade_sanctuary = make_buff_fallback( talent.conduit_of_the_celestials.jade_sanctuary->ok(), this,
                                            "jade_sanctuary", talent.conduit_of_the_celestials.jade_sanctuary_buff );

  buff.strength_of_the_black_ox =
      make_buff_fallback( talent.conduit_of_the_celestials.strength_of_the_black_ox->ok(), this,
                          "strength_of_the_black_ox", talent.conduit_of_the_celestials.strength_of_the_black_ox_buff );

  buff.unity_within = make_buff_fallback( talent.conduit_of_the_celestials.unity_within->ok(), this, "unity_within",
                                          talent.conduit_of_the_celestials.unity_within_buff )
                          ->set_expire_callback( [ this ]( buff_t *, double, timespan_t ) {
                            buff.jade_sanctuary->trigger();

                            action.strength_of_the_black_ox.celestial->execute();
                            action.courage_of_the_white_tiger.celestial->execute();
                            action.flight_of_the_red_crane.celestial->execute();

                            buff.heart_of_the_jade_serpent_unity_within->trigger();
                          } );

  buff.aspect_of_harmony.construct_buffs( this );

  if ( talent.master_of_harmony.balanced_stratagem->ok() )
  {
    buff.balanced_stratagem_magic    = new buffs::balanced_stratagem_magic_t( this );
    buff.balanced_stratagem_physical = new buffs::balanced_stratagem_physical_t( this );
  }

  // Master of Harmony
  buff.harmonic_surge = make_buff_fallback( talent.master_of_harmony.harmonic_surge->ok(), this, "harmonic_surge",
                                            talent.master_of_harmony.harmonic_surge_buff );

  // Shado-Pan
  // unconditionally construct, fallback-lite behaviour handled in custom trigger
  buff.flurry_charge = make_buff<buffs::flurry_charge_t>( this );

  buff.predictive_training =
      make_buff_fallback( talent.shado_pan.predictive_training->ok(), this, "predictive_training",
                          talent.shado_pan.predictive_training->effectN( 1 ).trigger() )
          ->set_trigger_spell( talent.shado_pan.predictive_training );

  buff.stand_ready =
      make_buff_fallback( talent.shado_pan.stand_ready->ok(), this, "stand_ready", talent.shado_pan.stand_ready_buff )
          ->set_default_value_from_effect( 1 );

  buff.whirling_steel = make_buff_fallback( talent.shado_pan.whirling_steel->ok(), this, "whirling_steel",
                                            talent.shado_pan.whirling_steel->effectN( 1 ).trigger() );
}

void monk_t::init_gains()
{
  base_t::init_gains();

  gain.legacy_open_palm_strikes = get_gain( "Open Palm Strikes (Azerite)" );
  gain.black_ox_brew_energy = get_gain( "black_ox_brew_energy" );
  gain.combo_breaker        = get_gain( "combo_breaker" );
  gain.chi_refund           = get_gain( "chi_refund" );
  gain.energy_burst         = get_gain( "energy_burst" );
  gain.energy_refund        = get_gain( "energy_refund" );
  gain.touch_of_death_ww    = get_gain( "touch_of_death_ww" );
  gain.zenith               = get_gain( "zenith" );
}

void monk_t::init_procs()
{
  base_t::init_procs();

  proc.anvil_and_stave                = get_proc( "Anvil & Stave" );
  proc.blackout_combo_tiger_palm      = get_proc( "Blackout Combo - Tiger Palm" );
  proc.blackout_combo_keg_smash       = get_proc( "Blackout Combo - Keg Smash" );
  proc.charred_passions               = get_proc( "Charred Passions" );
  proc.elusive_footwork_proc          = get_proc( "Elusive Footwork" );
  proc.salsalabims_strength           = get_proc( "Sal'salabim Breath of Fire Reset" );
  proc.tranquil_spirit_expel_harm     = get_proc( "Tranquil Spirit - Expel Harm" );
  proc.tranquil_spirit_goto           = get_proc( "Tranquil Spirit - Gift of the Ox" );
  proc.xuens_battlegear_sck_reduction = get_proc( "Xuen's Battlegear CD SCK Half-Second Reduction" );
  proc.elusive_brawler_preserved      = get_proc( "Elusive Brawler Stacks Preserved" );
}

monk_effect_callback_t::monk_effect_callback_t( const special_effect_t &effect, monk_t *player )
  : dbc_proc_callback_t( effect.player, effect ), player( player )
{
}

void monk_effect_callback_t::trigger( const proc_data_t &data, player_t *target, action_state_t *state,
                                      proc_trigger_type_e type )
{
  dbc_proc_callback_t::trigger( data, target, state, type );

  if ( player->sim->debug )
  {
    // Debug reporting
    auto find_a = range::find_if( player->proc_tracking[ effect.name() ],
                                  [ & ]( action_t *it ) { return it->id == data->id(); } );

    if ( find_a == player->proc_tracking[ effect.name() ].end() )
      player->proc_tracking[ effect.name() ].push_back( state->action );
  }
}

void monk_effect_callback_t::execute( const spell_data_t *spell, player_t *target, action_state_t *state )
{
  if ( !state->target->is_sleeping() )
  {
    // Dynamically find and execute proc tracking
    auto effect_proc = player->find_proc( effect.trigger()->_name );
    if ( effect_proc )
      effect_proc->occur();
  }

  dbc_proc_callback_t::execute( spell, target, state );
}

void monk_effect_callback_t::initialize()
{
  dbc_proc_callback_t::initialize();

  for ( const monk_effect_callback_t::post_init_callback_fn_t &fn : post_init_callbacks )
    fn( this );
}

monk_effect_callback_t *monk_effect_callback_t::register_post_init_callback(
    const monk_effect_callback_t::post_init_callback_fn_t &fn )
{
  post_init_callbacks.emplace_back( std::move( fn ) );
  return this;
}

monk_effect_callback_t *monk_effect_callback_t::register_callback_trigger_function(
    dbc_proc_callback_t::trigger_fn_type t, const dbc_proc_callback_t::trigger_fn_t &fn )
{
  player->callbacks.register_callback_trigger_function( effect.spell_id, t, fn );
  return this;
}

monk_effect_callback_t *monk_effect_callback_t::register_callback_execute_function(
    const dbc_proc_callback_t::execute_fn_t &fn )
{
  player->callbacks.register_callback_execute_function( effect.spell_id, fn );
  return this;
}

monk_effect_callback_t *monk_t::create_proc_callback( monk_callback_init_t params )
{
  special_effect_t *effect = new special_effect_t( this );

  effect->spell_id     = params.effect_driver->id();
  effect->cooldown_    = params.effect_driver->internal_cooldown();
  effect->proc_chance_ = params.effect_driver->proc_chance();
  effect->ppm_         = params.ppm ? params.ppm : params.effect_driver->_rppm;

  sim->print_debug( "initializing driver {} {} {}", effect->name_str, effect->spell_id, effect->cooldown_ );

  if ( !params.action_override )
  {
    // If we didn't define a custom action in initialization then
    // search action list for the first trigger we have a valid action for
    for ( const auto &e : params.effect_driver->effects() )
    {
      for ( auto t : action_list )
        if ( e.trigger()->ok() && t->id == e.trigger()->id() )
          params.action_override = t;

      if ( params.action_override )
        break;
    }
  }

  if ( params.action_override )
  {
    effect->name_str         = params.action_override->name_str;
    effect->trigger_str      = params.action_override->name_str;
    effect->trigger_spell_id = params.action_override->id;
    effect->action_disabled  = false;
    effect->execute_action   = effect->create_action();

    if ( !effect->execute_action )
      effect->execute_action = params.action_override;

    if ( params.action_override->harmful )
    {
      // Translate harmful proc_flags
      // e.g., the driver for a debuff uses MELEE_ABILITY_TAKEN instead of MELEE_ABILITY

      const std::unordered_map<uint64_t, uint64_t> translation_map = {
          { PF_MELEE_TAKEN, PF_MELEE },
          { PF_MELEE_ABILITY_TAKEN, PF_MELEE_ABILITY },
          { PF_RANGED_TAKEN, PF_RANGED },
          { PF_RANGED_ABILITY_TAKEN, PF_RANGED_ABILITY },
          { PF_NONE_HELPFUL_TAKEN, PF_NONE_HELPFUL },
          { PF_NONE_HARMFUL_TAKEN, PF_NONE_HARMFUL },
          { PF_MAGIC_HEAL_TAKEN, PF_MAGIC_HEAL },
          { PF_MAGIC_SPELL_TAKEN, PF_MAGIC_SPELL },
          { PF_PERIODIC_TAKEN, PF_PERIODIC },
          { PF_ALL_DAMAGE_TAKEN, PF_ALL_DAMAGE },
      };

      for ( auto t : translation_map )
      {
        if ( effect->proc_flags_ & t.first )
        {
          effect->proc_flags_ = ( effect->proc_flags_ & ~t.first ) | t.second;
        }
      }
    }
  }

  // defer configuration of proc flags in case proc_action_override is used
  effect->proc_flags_ = params.pf_override != 0ull ? params.pf_override : params.effect_driver->proc_flags();
  if ( params.pf2_override != 0ull )
    effect->proc_flags2_ = params.pf2_override;

  // We still haven't assigned a name, it is most likely a buff
  // dynamically find buff
  if ( effect->name_str == "" )
  {
    for ( const auto &e : params.effect_driver->effects() )
    {
      for ( auto t : buff_list )
      {
        if ( e.trigger()->ok() && e.trigger()->id() == t->data().id() )
        {
          effect->name_str    = t->name_str;
          effect->custom_buff = t;
        }
      }

      if ( effect->create_buff() != nullptr )
        break;
    }
  }

  special_effects.push_back( effect );  // Garbage collection

  return new monk_effect_callback_t( *effect, this );
}

void monk_t::init_special_effects()
{
  // TODO: CXX20: use designated initializers to make this suck less
  auto hp_percent_trigger = [ &, this ]( const spelleffect_data_t &effect ) {
    assert( effect.subtype() == A_TRIGGER_SPELL_BY_HEALTH_PCT );
    return [ &, this, effect ]( const dbc_proc_callback_t *, const proc_data_t &, player_t *, action_state_t *state,
                                proc_trigger_type_e ) {
      bool start_state = health_percentage() > effect.base_value();
      bool end_state   = health_percentage() - state->result_amount / max_health() * 100.0 < effect.base_value();
      switch ( effect.misc_value1() )
      {
        case 0:
          return !start_state && !end_state;
        case 1:
          return start_state && end_state;
      }
      return false;
    };
  };

  if ( talent.brewmaster.celestial_flames->ok() )
    create_proc_callback( { talent.brewmaster.celestial_flames, PF_CAST_SUCCESSFUL,
                            static_cast<proc_flag2>( PF2_CAST_GENERIC | PF2_CAST_HEAL ) } )
        ->register_callback_trigger_function(
            dbc_proc_callback_t::trigger_fn_type::CONDITION,
            [ & ]( const dbc_proc_callback_t *, const proc_data_t &, player_t *, action_state_t *state,
                   proc_trigger_type_e ) { return baseline.brewmaster.brews.contains( state->action ); } )
        ->register_callback_execute_function( [ & ]( const dbc_proc_callback_t *, const spell_data_t *, player_t *,
                                                     action_state_t * ) { buff.celestial_flames->trigger(); } );

  if ( talent.brewmaster.exploding_keg.ok() )
    create_proc_callback( { talent.brewmaster.exploding_keg.spell() } )
        ->register_callback_trigger_function( dbc_proc_callback_t::trigger_fn_type::CONDITION,
                                              [ & ]( const dbc_proc_callback_t *, const proc_data_t &, player_t *,
                                                     action_state_t *, proc_trigger_type_e ) {
                                                // Exploding keg damage is only triggered when the player buff is up,
                                                // regardless if the enemy has the debuff
                                                return buff.exploding_keg->check();
                                              } );

  if ( talent.windwalker.flurry_of_xuen.ok() )
    create_proc_callback( { talent.windwalker.flurry_of_xuen.spell() } )
        ->register_callback_trigger_function( dbc_proc_callback_t::trigger_fn_type::CONDITION,
                                              [ & ]( const dbc_proc_callback_t *, const proc_data_t &data, player_t *,
                                                     action_state_t *, proc_trigger_type_e ) {
                                                return data->id() != action.flurry_of_xuen->id &&
                                                       data->id() != action.empowered_tiger_lightning->id;
                                              } )
        ->register_callback_execute_function( [ & ]( const dbc_proc_callback_t *, const spell_data_t *, player_t *,
                                                     action_state_t * ) { buff.flurry_of_xuen->trigger(); } );

  if ( talent.monk.chi_burst->ok() && specialization() == MONK_WINDWALKER )
    create_proc_callback( { talent.monk.chi_burst.spell() } );

  if ( talent.brewmaster.spirit_of_the_ox->ok() )
    create_proc_callback( { talent.brewmaster.spirit_of_the_ox.spell() } )
        ->register_callback_trigger_function(
            dbc_proc_callback_t::trigger_fn_type::CONDITION,
            [ & ]( const dbc_proc_callback_t *, const proc_data_t &data, player_t *, action_state_t *,
                   proc_trigger_type_e ) { return data->id() == baseline.brewmaster.blackout_kick->id(); } )
        ->register_callback_execute_function( [ & ]( const dbc_proc_callback_t *, const spell_data_t *, player_t *,
                                                     action_state_t * ) { buff.gift_of_the_ox->spawn_orb( 1 ); } );

  if ( talent.master_of_harmony.aspect_of_harmony->ok() )
    create_proc_callback( { talent.master_of_harmony.aspect_of_harmony_driver,
                            static_cast<proc_flag>( PF_ALL_DAMAGE | PF_ALL_HEAL | PF_PERIODIC ), PF2_ALL_HIT } )
        ->register_callback_trigger_function( dbc_proc_callback_t::trigger_fn_type::TRIGGER,
                                              [ & ]( const dbc_proc_callback_t *, const proc_data_t &data, player_t *,
                                                     action_state_t *, proc_trigger_type_e ) {
                                                // TODO: don't hardcode these ids
                                                constexpr std::array<unsigned, 8> blacklist = {
                                                    216521,  // celestial fortune
                                                    178173,  // goto expire
                                                    124507,  // goto trigger
                                                    387621,  // dragonfire brew
                                                    115129,  // expel harm damage
                                                    124255,  // stagger
                                                    450820,  // purified spirit
                                                    450763,  // aspect of harmony tick
                                                };
                                                if ( range::contains( blacklist, data->id() ) )
                                                  return false;
                                                if ( data.allow_class_ability_procs )
                                                  return true;
                                                return false;
                                              } )
        ->register_callback_execute_function(
            [ & ]( const dbc_proc_callback_t *, const spell_data_t *, player_t *, action_state_t *state ) {
              buff.aspect_of_harmony.trigger( state );
            } );

  if ( talent.master_of_harmony.balanced_stratagem->ok() )
    create_proc_callback( { talent.master_of_harmony.balanced_stratagem,
                            static_cast<proc_flag>( PF_ALL_DAMAGE | PF_ALL_HEAL | PF_CAST_SUCCESSFUL ),
                            static_cast<proc_flag2>( PF2_ALL_CAST | PF2_ALL_HIT ) } )
        ->register_callback_trigger_function( dbc_proc_callback_t::trigger_fn_type::CONDITION,
                                              [ & ]( const dbc_proc_callback_t *, const proc_data_t &data, player_t *,
                                                     action_state_t *, proc_trigger_type_e ) {
                                                return buff.balanced_stratagem_magic->trigger( data ) ||
                                                       buff.balanced_stratagem_physical->trigger( data );
                                              } )
        ->register_post_init_callback( []( monk_effect_callback_t *cb ) {
          cb->proc_chance                        = 1.0;
          cb->can_proc_from_procs                = true;
          cb->can_only_proc_from_class_abilities = true;
        } );

  if ( talent.conduit_of_the_celestials.courage_of_the_white_tiger->ok() )
    create_proc_callback( { talent.conduit_of_the_celestials.courage_of_the_white_tiger, static_cast<proc_flag>( 0ull ),
                            static_cast<proc_flag2>( 0ull ), action.courage_of_the_white_tiger.base } )
        ->register_callback_trigger_function(
            dbc_proc_callback_t::trigger_fn_type::CONDITION,
            [ & ]( const dbc_proc_callback_t *, const proc_data_t &data, player_t *, action_state_t *,
                   proc_trigger_type_e ) { return data->id() == baseline.monk.tiger_palm->id(); } );

  if ( talent.brewmaster.walk_with_the_ox.ok() )
  {
    action.walk_with_the_ox_rng = get_accumulated_rng(
        "walk_with_the_ox", 0.0075 * talent.brewmaster.walk_with_the_ox->effectN( 1 ).base_value() );
    create_proc_callback( { talent.brewmaster.walk_with_the_ox, static_cast<proc_flag>( 0ull ),
                            static_cast<proc_flag2>( 0ull ), action.walk_with_the_ox } )
        ->register_callback_trigger_function( dbc_proc_callback_t::trigger_fn_type::CONDITION,
                                              [ & ]( const dbc_proc_callback_t *dbc_proc_cb, const proc_data_t &,
                                                     player_t *, action_state_t *, proc_trigger_type_e ) {
                                                if ( dbc_proc_cb->cooldown->down() )
                                                  return false;
                                                dbc_proc_cb->cooldown->start();
                                                return static_cast<bool>( action.walk_with_the_ox_rng->trigger() );
                                              } );
  }

  if ( talent.shado_pan.stand_ready->ok() )
    create_proc_callback( { talent.shado_pan.stand_ready_buff } )
        ->register_callback_trigger_function(
            dbc_proc_callback_t::trigger_fn_type::CONDITION,
            [ & ]( const dbc_proc_callback_t *, const proc_data_t &, player_t *, action_state_t *,
                   proc_trigger_type_e ) { return buff.stand_ready->check(); } )
        ->register_callback_execute_function(
            [ & ]( const dbc_proc_callback_t *, const spell_data_t *, player_t *, action_state_t * ) {
              action.flurry_strikes->execute( actions::flurry_strikes_t::STAND_READY );
            } );

  if ( talent.brewmaster.elixir_of_determination->ok() )
    create_proc_callback( { &buff.elixir_of_determination->data() } )
        ->register_callback_trigger_function(
            dbc_proc_callback_t::trigger_fn_type::CONDITION,
            hp_percent_trigger( talent.brewmaster.elixir_of_determination->effectN( 1 ) ) )
        ->register_callback_execute_function( [ & ]( const dbc_proc_callback_t *, const spell_data_t *, player_t *,
                                                     action_state_t * ) { buff.elixir_of_determination->trigger(); } );

  // Doesn't use effect 468 for trigger behaviour, let's just pretend it does (:
  if ( talent.shado_pan.whirling_steel->ok() )
    create_proc_callback( { talent.shado_pan.whirling_steel.spell() } )
        ->register_callback_trigger_function(
            dbc_proc_callback_t::trigger_fn_type::CONDITION,
            [ & ]( const dbc_proc_callback_t *, const proc_data_t &, player_t *, action_state_t *state,
                   proc_trigger_type_e ) {
              const spelleffect_data_t &effect = talent.shado_pan.whirling_steel->effectN( 1 );
              bool start_state                 = health_percentage() > effect.base_value();
              bool end_state = health_percentage() - state->result_amount / max_health() * 100.0 < effect.base_value();
              return start_state && end_state;
            } )
        ->register_callback_execute_function( [ & ]( const dbc_proc_callback_t *, const spell_data_t *, player_t *,
                                                     action_state_t * ) { buff.whirling_steel->trigger(); } );

  if ( talent.brewmaster.vital_flame->ok() )
    create_proc_callback( { talent.brewmaster.vital_flame, static_cast<proc_flag>( PF_ALL_DAMAGE | PF_PERIODIC ),
                            static_cast<proc_flag2>( PF2_ALL_HIT | PF2_PERIODIC_DAMAGE ) } )
        ->register_callback_trigger_function(
            dbc_proc_callback_t::trigger_fn_type::CONDITION,
            []( const dbc_proc_callback_t *, const proc_data_t &data, player_t *, action_state_t *state,
                proc_trigger_type_e ) {
              if ( state->action->school != SCHOOL_FIRE && state->action->school != SCHOOL_NATURE )
                return false;

              if ( data.allow_class_ability_procs )
                return true;

              return false;
            } )
        ->register_callback_execute_function(
            [ & ]( const dbc_proc_callback_t *, const spell_data_t *, player_t *, action_state_t *state ) {
              action.vital_flame->base_dd_max = action.vital_flame->base_dd_min = state->result_amount;
              action.vital_flame->execute();
            } );

  if ( talent.brewmaster.bring_me_another_1->ok() )
    create_proc_callback( { talent.brewmaster.bring_me_another_1, PF_CAST_SUCCESSFUL,
                            static_cast<proc_flag2>( PF2_CAST_GENERIC | PF2_CAST_HEAL ) } )
        ->register_callback_trigger_function(
            dbc_proc_callback_t::trigger_fn_type::CONDITION,
            [ & ]( const dbc_proc_callback_t *, const proc_data_t &, player_t *, action_state_t *state,
                   proc_trigger_type_e ) { return baseline.brewmaster.brews.contains( state->action ); } )
        ->register_callback_execute_function( [ & ]( const dbc_proc_callback_t *, const spell_data_t *, player_t *,
                                                     action_state_t * ) { buff.empty_barrel->trigger(); } );

  if ( talent.brewmaster.bring_me_another_3->ok() )
  {
    create_proc_callback( { talent.brewmaster.bring_me_another_3, PF_CAST_SUCCESSFUL,
                            static_cast<proc_flag2>( PF2_CAST_GENERIC | PF2_CAST_HEAL ) } )
        ->register_callback_trigger_function( dbc_proc_callback_t::trigger_fn_type::CONDITION,
                                              [ & ]( const dbc_proc_callback_t *, const proc_data_t &data, player_t *,
                                                     action_state_t *, proc_trigger_type_e ) {
                                                auto id = data->id();
                                                return id == talent.brewmaster.celestial_brew->id() ||
                                                       id == talent.brewmaster.celestial_infusion->id() ||
                                                       id == talent.monk.fortifying_brew.find_override_spell()->id();
                                              } )
        ->register_callback_execute_function(
            [ & ]( const dbc_proc_callback_t *, const spell_data_t *, player_t *, action_state_t * ) {
              buff.refreshing_drink->trigger();
              buff.empty_barrel->trigger();
            } )
        ->register_post_init_callback( []( monk_effect_callback_t *cb ) {
          cb->proc_chance                        = 1.0;
          cb->can_proc_from_procs                = true;
          cb->can_only_proc_from_class_abilities = true;
        } );

    create_proc_callback( { &buff.refreshing_drink->data(), PF_ALL_DAMAGE_TAKEN,
                            static_cast<proc_flag2>( PF2_ALL_HIT | PF2_PERIODIC_DAMAGE ) } )
        ->register_callback_trigger_function(
            dbc_proc_callback_t::trigger_fn_type::TRIGGER,
            [ & ]( const dbc_proc_callback_t *, const proc_data_t &, player_t *, action_state_t *,
                   proc_trigger_type_e ) { return buff.refreshing_drink->up(); } )
        ->register_callback_execute_function(
            [ & ]( const dbc_proc_callback_t *, const spell_data_t *, player_t *, action_state_t * ) {
              buff.refreshing_drink->expire();
              action.refreshing_drink->execute();
            } );
  }

  base_t::init_special_effects();
}

void monk_t::init_assessors()
{
  base_t::init_assessors();

  if ( baseline.windwalker.empowered_tiger_lightning->ok() )
    assessor_out_damage.add( assessor::TARGET_DAMAGE, [ this ]( result_amount_type, action_state_t *state ) {
      if ( !state->result_amount )
        return assessor::CONTINUE;

      monk_td_t *target_data = get_target_data( state->target );
      if ( !target_data )
        return assessor::CONTINUE;

      propagate_const<buff_t *> debuff = target_data->debuff.empowered_tiger_lightning;
      if ( !debuff )
        return assessor::CONTINUE;

      debug_cast<buffs::empowered_tiger_lightning_t *>( debuff.get() )->trigger( state );

      return assessor::CONTINUE;
    } );
}

void monk_t::init_finished()
{
  base_t::init_finished();
  parse_player_effects();
}


// BracketSim legacy compatibility: Vision of Perfection (Heart of Azeroth major
// essence). The engine procs it and calls this; each spec fires its signature
// cooldown early, at the fraction of its duration the essence grants.
void monk_t::vision_of_perfection_proc()
{
  auto essence = find_azerite_essence( "Vision of Perfection" );
  if ( !essence.enabled() )
    return;

  double mult = essence.spell( 1u )->effectN( 1 ).percent() +
                essence.spell( 2u, essence_spell::UPGRADE )->effectN( 1 ).percent();

  buff_t* window = nullptr;
  switch ( specialization() )
  {
    case MONK_WINDWALKER:
      window = buff.zenith;
      break;
    case MONK_BREWMASTER:
      window = buff.invoke_niuzao;
      break;
    default:
      break;
  }

  if ( !window || mult <= 0 )
    return;

  timespan_t dur = window->buff_duration() * mult;
  if ( window->check() )
    window->extend_duration( dur );
  else
    window->trigger( 1, buff_t::DEFAULT_VALUE(), -1.0, dur );
}

void monk_t::reset()
{
  base_t::reset();

  combo_strike_actions.clear();

  if ( sim->debug )
  {
    auto stream = sim->out_debug.raw().get_stream();
    bool first  = true;

    for ( auto &[ name, list ] : proc_tracking )
    {
      if ( list.size() > 0 )
      {
        if ( first )
        {
          *stream << "\n Monk Proc Tracking ... ( spellid: name )" << '\n' << '\n';
          first = false;
        }

        *stream << name << " procced from: " << '\n';
        for ( auto a : list )
          *stream << " - " << a->id << " : " << a->name_str << '\n';

        list.clear();
      }
    }
  }
}

double monk_t::composite_attack_power_multiplier() const
{
  double ap = base_t::composite_attack_power_multiplier();

  // TODO: implement using parse_effects
  ap *= 1.0 + cache.mastery() * baseline.brewmaster.mastery->effectN( 2 ).mastery_value();

  return ap;
}

double monk_t::composite_dodge() const
{
  double d = base_t::composite_dodge();

  if ( specialization() == MONK_BREWMASTER )
    d += buff.elusive_brawler->current_stack * cache.mastery_value();

  return d;
}

double monk_t::composite_player_target_armor( player_t *target ) const
{
  double armor = player_t::composite_player_target_armor( target );

  return armor;
}

void monk_t::invalidate_cache( cache_e c )
{
  base_t::invalidate_cache( c );

  if ( specialization() == MONK_MISTWEAVER )
    return;

  switch ( c )
  {
    case CACHE_ATTACK_POWER:
    case CACHE_AGILITY:
      base_t::invalidate_cache( CACHE_SPELL_POWER );
      break;
    case CACHE_MASTERY:
      if ( specialization() == MONK_WINDWALKER )
        base_t::invalidate_cache( CACHE_PLAYER_DAMAGE_MULTIPLIER );
      else if ( specialization() == MONK_BREWMASTER )
      {
        base_t::invalidate_cache( CACHE_ATTACK_POWER );
        base_t::invalidate_cache( CACHE_SPELL_POWER );
        base_t::invalidate_cache( CACHE_DODGE );
      }
      break;
    default:
      break;
  }
}

void monk_t::create_options()
{
  base_t::create_options();

  add_option( opt_bool( "monk.legacy_shadowlands_enabled",
                        shadowlands_legacy.legacy_shadowlands_enabled ) );
  add_option( opt_string( "monk.legacy_covenant", legacy_covenant.chosen ) );
  add_option( opt_string( "monk.legacy_conduits", legacy_conduits.option ) );
  add_option( opt_int( "monk.initial_chi", user_options.initial_chi, 0, 6 ) );
  add_option( opt_int( "monk.chi_burst_healing_targets", user_options.chi_burst_healing_targets, 0, 30 ) );

  // shado-pan options
  add_option(
      opt_int( "monk.shado_pan.initial_charge_accumulator", user_options.shado_pan_initial_charge_accumulator, 0, 9 ) );
}

void monk_t::copy_from( player_t *source )
{
  base_t::copy_from( source );
  user_options = debug_cast<monk_t *>( source )->user_options;
}

resource_e monk_t::primary_resource() const
{
  return RESOURCE_ENERGY;
}

role_e monk_t::primary_role() const
{
  // First, check for the user-specified role
  switch ( base_t::primary_role() )
  {
    case ROLE_TANK:
    case ROLE_ATTACK:
    case ROLE_HEAL:
      return base_t::primary_role();
      break;
    default:
      break;
  }

  // Else, fall back to spec
  switch ( specialization() )
  {
    case MONK_BREWMASTER:
      return ROLE_TANK;
      break;
    default:
      return ROLE_ATTACK;
      break;
  }
}

stat_e monk_t::convert_hybrid_stat( stat_e s ) const
{
  // this converts hybrid stats that either morph based on spec or only work
  // for certain specs into the appropriate "basic" stats
  switch ( s )
  {
    case STAT_STR_AGI_INT:
      switch ( specialization() )
      {
        case MONK_BREWMASTER:
        case MONK_WINDWALKER:
          return STAT_AGILITY;
        // BracketSim (29 Sep 2026): a Mistweaver wears the intellect half of hybrid gear.
        case MONK_MISTWEAVER:
          return STAT_INTELLECT;
        default:
          return STAT_NONE;
      }
    case STAT_AGI_INT:
      return specialization() == MONK_MISTWEAVER ? STAT_INTELLECT : STAT_AGILITY;
    case STAT_STR_AGI:
      return STAT_AGILITY;
    case STAT_STR_INT:
      return STAT_INTELLECT;
    case STAT_SPIRIT:
      return STAT_NONE;
    case STAT_BONUS_ARMOR:
      if ( specialization() == MONK_BREWMASTER )
        return s;
      else
        return STAT_NONE;
    default:
      return s;
  }
}

void monk_t::combat_begin()
{
  base_t::combat_begin();

  if ( talent.monk.chi_wave->ok() )
  {
    // Player starts combat with buff
    buff.chi_wave->trigger();
    // ... and then regains the buff in time intervals while in combat
    make_repeating_event( sim, talent.monk.chi_wave->effectN( 1 ).period(), [ this ]() { buff.chi_wave->trigger(); } );
  }

  // This is just for easier cleanup later
  if ( talent.windwalker.tigereye_brew_1->ok() )
  {
    const auto period_fn = []( monk_t *player ) -> timespan_t {
      return player->talent.windwalker.tigereye_brew_1->effectN( 1 ).period() * player->composite_melee_haste();
    };
    const auto callback = []( monk_t *player ) -> void {
      if ( !player->sim->active_enemies && player->buff.tigereye_brew_1->stack() <
                                               player->talent.windwalker.tigereye_brew_1->effectN( 1 ).base_value() )
        make_event<events::delayed_buff_trigger_event_t>( *player->sim, player, player->buff.tigereye_brew_1, 2_s );
    };
    make_event<events::repeating_dynamic_period_cb_event_t>( *sim, this, period_fn, callback );

    if ( !buff.tigereye_brew_1->check() )
      buff.tigereye_brew_1->trigger( as<int>( talent.windwalker.tigereye_brew_1->effectN( 1 ).base_value() ) );
  }

  if ( talent.conduit_of_the_celestials.inner_compass->ok() )
  {
    const std::array<buff_t *, 4> stances = { buff.inner_compass_crane_stance, buff.inner_compass_ox_stance,
                                              buff.inner_compass_tiger_stance, buff.inner_compass_serpent_stance };

    // Select a random stance to begin the iteration.
    rng().range( stances )->trigger();

    make_repeating_event( sim, talent.conduit_of_the_celestials.inner_compass->effectN( 1 ).period(), [ stances ]() {
      auto current_stance =
          std::find_if( stances.begin(), stances.end(), []( auto &stance ) { return stance->check(); } );
      ( *current_stance )->expire();
      if ( std::next( current_stance ) != stances.end() )
        ( *std::next( current_stance ) )->trigger();
      else
        ( *stances.begin() )->trigger();
    } );
  }

  if ( specialization() == MONK_WINDWALKER )
  {
    if ( user_options.initial_chi > 0 )
    {
      resources.current[ RESOURCE_CHI ] =
          clamp( as<double>( user_options.initial_chi + resources.current[ RESOURCE_CHI ] ), 0.0,
                 resources.max[ RESOURCE_CHI ] );
      sim->print_debug( "Combat starting chi has been set to {}", resources.current[ RESOURCE_CHI ] );
    }
    else
    {
      resources.current[ RESOURCE_CHI ] = talent.windwalker.combat_wisdom->effectN( 1 ).base_value();
    }

    if ( talent.windwalker.combat_wisdom->ok() )
    {
      // Player starts combat with buff
      buff.combat_wisdom->trigger();
      // ... and then regains the buff in time intervals while in combat
      bool trigger( double index );
      make_repeating_event( sim, talent.windwalker.combat_wisdom->effectN( 2 ).period(),
                            [ this ]() { buff.combat_wisdom->trigger(); } );
    }
  }
}

void monk_t::assess_damage( school_e school, result_amount_type dtype, action_state_t *s )
{
  if ( specialization() == MONK_BREWMASTER )
  {
    if ( s->result == RESULT_DODGE )
    {
      // In order to trigger the expire before the hit but not actually remove the buff until AFTER the hit
      // We are setting a 1 millisecond delay on the expire.

      if ( rng().roll( 1.0 - talent.brewmaster.one_with_the_wind->effectN( 1 ).percent() ) )
        buff.elusive_brawler->expire();
      else
        proc.elusive_brawler_preserved->occur();

      // Saved as 5/10 base values but need it as 0.5 and 1 base values
      if ( talent.brewmaster.anvil_and_stave->ok() && cooldown.anvil_and_stave->up() )
      {
        cooldown.anvil_and_stave->start( talent.brewmaster.anvil_and_stave->internal_cooldown() );
        proc.anvil_and_stave->occur();
        baseline.brewmaster.brews.adjust(
            timespan_t::from_seconds( talent.brewmaster.anvil_and_stave->effectN( 1 ).base_value() / 10 ) );
      }

      buff.counterstrike->trigger();
      buff.predictive_training->trigger();
    }
    if ( s->result == RESULT_MISS )
      buff.counterstrike->trigger();
    if ( s->result == RESULT_PARRY )
      buff.predictive_training->trigger();
  }

  // trigger the mastery if the player gets hit by a physical attack; but not from stagger
  if ( action_t::result_is_hit( s->result ) && school == SCHOOL_PHYSICAL &&
       s->action->id != baseline.brewmaster.stagger_self_damage->id() )
    buff.elusive_brawler->trigger();

  base_t::assess_damage( school, dtype, s );
}

void monk_t::target_mitigation( school_e school, result_amount_type dt, action_state_t *s )
{
  // Touch of Karma Absorbtion
  if ( buff.touch_of_karma->up() )
  {
    double percent_HP = baseline.windwalker.touch_of_karma->effectN( 3 ).percent() * max_health();
    if ( ( buff.touch_of_karma->value() + s->result_amount ) >= percent_HP )
    {
      double difference = percent_HP - buff.touch_of_karma->value();
      buff.touch_of_karma->current_value += difference;
      s->result_amount -= difference;
      buff.touch_of_karma->expire();
    }
    else
    {
      buff.touch_of_karma->current_value += s->result_amount;
      s->result_amount = 0;
    }
  }

  // Gift of the Ox is no longer a random chance, under the hood. When you are hit, it increments a counter by
  // (DamageTakenBeforeAbsorbsOrStagger / MaxHealth). It now drops an orb whenever that reaches 1.0, and decrements it
  // by 1.0. The tooltip still says ‘chance’, to keep it understandable.
  if ( buff.gift_of_the_ox && s->action->id != baseline.brewmaster.stagger_self_damage->id() )
    buff.gift_of_the_ox->trigger_from_damage( s->result_amount );

  base_t::target_mitigation( school, dt, s );
}

void monk_t::assess_heal( school_e school, result_amount_type dmg_type, action_state_t *s )
{
  base_t::assess_heal( school, dmg_type, s );

  if ( specialization() == MONK_BREWMASTER )
    trigger_celestial_fortune( s );
}

void monk_t::create_actions()
{
  base_t::create_actions();
  buff.aspect_of_harmony.construct_actions( this );
}

namespace profileset_control
{
valid_talents_t::valid_talents_t( sim_t *sim, unsigned int id ) : profileset_controller_t( sim, id )
{
}

const std::string valid_talents_t::name() const
{
  return "valid_talents";
}

std::function<bool( std::tuple<talent_tree, unsigned, unsigned> )> matching_talent( player_t *player,
                                                                                    unsigned hero_tree )
{
  return [ = ]( std::tuple<talent_tree, unsigned, unsigned> entry ) {
    if ( std::get<talent_tree>( entry ) != talent_tree::HERO )
      return false;
    const trait_data_t *trait = trait_data_t::find( std::get<1>( entry ), player->is_ptr() );
    if ( !trait )
      return false;
    return static_cast<hero_tree_e>( trait->id_sub_tree ) == hero_tree;
  };
}

std::function<unsigned( unsigned )> has_expected_count( player_t *player, unsigned expected_count )
{
  return [ = ]( unsigned hero_tree ) {
    unsigned count = as<unsigned>( range::count_if( player->player_traits, matching_talent( player, hero_tree ) ) );
    return count > expected_count;
  };
}

bool valid_talents_t::evaluate_post_init()
{
  if ( !player || sim->enable_all_talents )
    return true;

  switch ( player->specialization() )
  {
    case MONK_BREWMASTER:
    case MONK_WINDWALKER:
      return range::all_of( player->player_sub_trees, has_expected_count( player, count ) );
    default:
      break;
  }

  return true;
}

const std::string valid_talents_t::reason() const
{
  return fmt::format( "player {} does not have {} talents selected in hero tree", player->name(), count );
}

void valid_talents_t::create_options()
{
  add_option( opt_func( "count", [ this ]( sim_t *, util::string_view, util::string_view value ) {
    this->count = util::to_unsigned( value );
    return true;
  } ) );
  add_option( opt_func( "player", [ this ]( sim_t *sim, util::string_view, util::string_view value ) {
    for ( auto &player : sim->player_list )
    {
      if ( util::str_compare_ci( player->name(), value ) )
      {
        this->player = player;
        return true;
      }
    }
    return false;
  } ) );
}
}  // namespace profileset_control

std::unique_ptr<expr_t> monk_t::create_expression( std::string_view name_str )
{
  auto splits = util::string_split<std::string_view>( name_str, "." );

  if ( splits.size() >= 3 && splits[ 1 ] == "celestial_brew" && talent.brewmaster.celestial_infusion->ok() )
  {
    if ( splits[ 0 ] == "cooldown" )
      return get_cooldown( "celestial_infusion" )->create_expression( splits[ 2 ] );
    if ( splits[ 0 ] == "action" )
      return find_action( "celestial_infusion" )->create_expression( splits[ 2 ] );
    if ( splits[ 0 ] == "buff" )
    {
      buff_t *buff = buff_t::find( this, "celestial_infusion" );
      assert( buff );
      return buff_t::create_expression( splits[ 1 ], splits[ 2 ], *buff );
    }
  }

  return base_t::create_expression( name_str );
}

class monk_report_t : public player_report_extension_t
{
public:
  monk_report_t( monk_t &player ) : p( player )
  {
  }

  struct monk_bug
  {
    std::string desc;
    std::string date;
    bool match;
  };

  auto_dispose<std::vector<monk_bug *>> issues;

  void monk_bugreport( report::sc_html_stream &os )
  {
    auto ReportIssue = [ this ]( std::string_view desc, std::string_view date, bool match = false ) {
      monk_bug *new_issue = new monk_bug;
      new_issue->desc     = desc;
      new_issue->date     = date;
      new_issue->match    = match;
      issues.push_back( new_issue );
    };

    ReportIssue( "The ETL cache for both tigers resets to 0 when either spawn", "2023-08-03", true );
    ReportIssue( "Chi Burst consumes both stacks of the buff on use", "2024-08-09", true );
    ReportIssue( "Press the Advantage Tiger Palm does not trigger Overwhelming Force", "2026-02-09", true );
    ReportIssue(
        "Zenith Stomp erroneously references effect 1 for sqrt scaling in tooltip, which is instead used to modify the "
        "Zenith Stomp charge buff.",
        "2026-07-11", true );

    os << "<div class=\"player-section\">\n";
    os << "<h3 class=\"toggle\">Known Bugs and Issues</h3>\n";
    os << "<div class=\"toggle-content hide\">\n";

    for ( auto issue : issues )
    {
      if ( issue->desc.empty() )
        continue;

      os << "<h3>" << issue->desc << "</h3>\n";

      os << "<table class=\"sc even\">\n"
         << "<thead>\n"
         << "<tr>\n"
         << "<th class=\"left\">Effective Date</th>\n"
         << "<th class=\"left\">Sim Matches Game Behavior</th>\n"
         << "</tr>\n"
         << "</thead>\n";

      os << "<tr>\n"
         << "<td class=\"left\"><strong>" << issue->date << "</strong></td>\n"
         << "<td class=\"left\" colspan=\"5\"><strong>" << ( issue->match ? "YES" : "NO" ) << "</strong></td>\n"
         << "</tr>\n";

      os << "</table>\n";
    }

    os << "</table>\n";
    os << "</div>\n";
    os << "</div>\n";
  }

  void aspect_of_harmony_accumulator( report::sc_html_stream &os )
  {
    if ( !p.talent.master_of_harmony.aspect_of_harmony.ok() )
      return;

    os << "<div class=\"player-section aspect_of_harmony\">\n";
    os << "<h3 class=\"toggle\">Aspect of Harmony Vitality</h3>\n";
    os << "<div class=\"toggle-content hide\">\n";
    os << "<p>Note that this graph only displays data for a single iteration.</p>\n";

    highchart::time_series_t chart_( highchart::build_id( p, "AoH_pool" ), *p.sim );
    chart::generate_actor_timeline( chart_, p, "Vitality", color::resource_color( RESOURCE_HEALTH ),
                                    p.buff.aspect_of_harmony.pool_size_percent() );
    chart_.set_yaxis_title( "Vitality / Max Health" );
    chart_.set( "tooltip.headerFormat", "<b>{point.key}</b> s<br/>" );
    chart_.set( "chart.width", "575" );
    os << chart_.to_target_div();
    p.sim->add_chart_data( chart_ );

    os << "</div>\n";
    os << "</div>\n";
  }

  void html_customsection( report::sc_html_stream &os ) override
  {
    monk_bugreport( os );
    aspect_of_harmony_accumulator( os );
  }

private:
  monk_t &p;
};

struct monk_module_t : public module_t
{
  monk_module_t() : module_t( MONK )
  {
  }

  player_t *create_player( sim_t *sim, std::string_view name, race_e race ) const override
  {
    monk_t *player           = new monk_t( sim, name, race );
    player->report_extension = std::make_unique<monk_report_t>( *player );
    return player;
  }

  bool valid() const override
  {
    return true;
  }

  void register_actor_initializers( sim_t * ) const override
  {
  }

  void register_hotfixes() const override
  {
  }
};
}  // end namespace monk

const module_t *module_t::monk()
{
  static monk::monk_module_t m;
  return &m;
}
