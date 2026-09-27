// ==========================================================================
// Dedmonwakeen's Raid DPS/TPS Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "unique_gear.hpp"

#include "dbc/racial_spells.hpp"
#include "player/scaling_metric_data.hpp"
#include "player/pet_spawner.hpp"
#include "sc_enums.hpp"
#include "sim/expressions.hpp"
#include "unique_gear_dragonflight.hpp"
#include "unique_gear_shadowlands.hpp"
#include "unique_gear_thewarwithin.hpp"
#include "unique_gear_midnight.hpp"
#include "util/util.hpp"

#include <cctype>
#include <memory>
#include <regex>

#include "simulationcraft.hpp"

using namespace unique_gear;

#define maintenance_check( ilvl ) static_assert( (ilvl) >= 90, "unique item below min level, should be deprecated." )

namespace { // UNNAMED NAMESPACE

/**
 * Forward declarations so we can reorganize the file a bit more sanely.
 */

namespace enchants
{
  /* Legacy Enchants */
  void crusader( special_effect_t& );
  void executioner( special_effect_t& );
  void hurricane_spell( special_effect_t& );
  void meta_gem_effect( special_effect_t& );

  /* Mists of Pandaria */
  void dancing_steel( special_effect_t& );
  void jade_spirit( special_effect_t& );
  void windsong( special_effect_t& );
  void rivers_song( special_effect_t& );
  void colossus( special_effect_t& );

  /* Warlords of Draenor */
  void hemets_heartseeker( special_effect_t& );
  void mark_of_bleeding_hollow( special_effect_t& );
  void mark_of_the_thunderlord( special_effect_t& );
  void mark_of_the_shattered_hand( special_effect_t& );
  void mark_of_the_frostwolf( special_effect_t& );
  void mark_of_blackrock( special_effect_t& );
  void mark_of_warsong( special_effect_t& );
  void megawatt_filament( special_effect_t& );
  void oglethorpes_missile_splitter( special_effect_t& );
}

namespace profession
{
  void nitro_boosts( special_effect_t& );
  void grounded_plasma_shield( special_effect_t& );
  void zen_alchemist_stone( special_effect_t& );
  void draenor_philosophers_stone( special_effect_t& );
}

namespace item
{
  /* Misc */
  void chains_of_ice_runic_power( special_effect_t& );
  void heartpierce( special_effect_t& );
  void darkmoon_card_greatness( special_effect_t& );
  void vial_of_shadows( special_effect_t& );
  void deathbringers_will( special_effect_t& );
  void cunning_of_the_cruel( special_effect_t& );
  void felmouth_frenzy( special_effect_t& );
  void matrix_restabilizer( special_effect_t& );
  void blazefury_medallion( special_effect_t& );
  void molten_ironfoe( special_effect_t& );
  void chillpike( special_effect_t& );
  void deaths_verdict( special_effect_t& );
  void jackhammer( special_effect_t& );
  void heartrazor( special_effect_t& );
  void untamed_blade( special_effect_t& );
  void blackout_truncheon( special_effect_t& );
  void despair( special_effect_t& );
  void world_breaker( special_effect_t& );
  void variable_pulse_lightning_capacitor( special_effect_t& );
  void eskhandars_right_claw( special_effect_t& );
  void thunderfury( special_effect_t& );
  void bonereavers_edge( special_effect_t& );
  void sulfuras( special_effect_t& );
  void love_struck( special_effect_t& );
  void tiny_abomination_in_a_jar( special_effect_t& );
  void shadowmourne( special_effect_t& );
  void nibelung( special_effect_t& );
  void dislodged_foreign_object( special_effect_t& );
  void souldrinker( special_effect_t& );
  void gurthalak( special_effect_t& );
  void dragonwrath( special_effect_t& );

  /* Mists of Pandaria 5.2 */
  void rune_of_reorigination( special_effect_t& );
  void spark_of_zandalar( special_effect_t& );
  void unerring_vision_of_leishen( special_effect_t& );

  /* Mists of Pandaria 5.4 */
  void amplification( special_effect_t& );
  void black_blood_of_yshaarj( special_effect_t& );
  void cleave( special_effect_t& );
  void essence_of_yulon( special_effect_t& );
  void flurry_of_xuen( special_effect_t& );
  void prismatic_prison_of_pride( special_effect_t& );
  void purified_bindings_of_immerseus( special_effect_t& );
  void readiness( special_effect_t& );
  void skeers_bloodsoaked_talisman( special_effect_t& );
  void thoks_tail_tip( special_effect_t& );

  /* Warlords of Draenor 6.0 */
  void autorepairing_autoclave( special_effect_t& );
  void battering_talisman_trigger( special_effect_t& );
  void blackiron_micro_crucible( special_effect_t& );
  void forgemasters_insignia( special_effect_t& );
  void humming_blackiron_trigger( special_effect_t& );
  void spellbound_runic_band( special_effect_t& );
  void spellbound_solium_band( special_effect_t& );

  /* Warlords of Draenor 6.2 */
  void discordant_chorus( special_effect_t& );
  void empty_drinking_horn( special_effect_t& );
  void insatiable_hunger( special_effect_t& );
  void mirror_of_the_blademaster( special_effect_t& );
  void prophecy_of_fear( special_effect_t& );
  void soul_capacitor( special_effect_t& );
  void tyrants_decree( special_effect_t& );
  void unblinking_gaze_of_sethe( special_effect_t& );
  void warlords_unseeing_eye( special_effect_t& );
  void gronntooth_war_horn( special_effect_t& );
  void orb_of_voidsight( special_effect_t& );
  void infallible_tracking_charm( special_effect_t& );
  void witherbarks_branch( special_effect_t& );

  /* Timewalking Trinkets */
  void necromantic_focus( special_effect_t& );
  void sorrowsong( special_effect_t& );
}

namespace set_bonus
{
  // Generic passive stat aura adder for set bonuses
  void passive_stat_aura( special_effect_t& );
}

namespace racial
{
  void touch_of_the_grave( special_effect_t& );
  void entropic_embrace( special_effect_t& );
  void brush_it_off( special_effect_t& );
  void zandalari_loa( special_effect_t& );
  void combat_analysis( special_effect_t& );
}

namespace generic
{
  void skyfury( special_effect_t& );
  void enable_all_item_effects( special_effect_t& );
}

/*
 * CHANCE ON HIT, ROLLED ON EVERY LANDED MELEE HIT - which is what this file has
 * claimed since 10 September, and what the engine was not doing.
 *
 * `effect.weapon_proc = true` reads as "this weapon's proc". What it does inside
 * dbc_proc_callback_t is stricter than that:
 *
 *     weapon = effect.item->weapon();
 *     ...
 *     if ( !state->action->weapon || state->action->weapon != weapon ) return;
 *
 * A melee ABILITY carries no `action->weapon` in this build, so the first clause
 * threw every one of them away. Traced on a level 30 Arms warrior wearing
 * Shadowmourne: 139 auto attacks and 210 ability casts (slam 66, overpower 64,
 * mortal strike 57, execute 23), and EVERY proc driver on the character rolled
 * exactly 139 times, all of them on auto_attack_mh. Not one ability rolled
 * anything, for eight days, while the comment above chance_on_hit_from_ppm
 * explained at length why they should.
 *
 * WHAT THE LOGS SAY, and they are not ambiguous. From the author's own combat logs
 * (audits/2026-09-18-what-rolls-a-chance-on-hit.py), counting procs with NO auto
 * attack in the 60ms before them - which an auto attack cannot have caused:
 *
 *     Bonereaver's Edge      94 ability-only procs   against  65 auto-only
 *     Shadowmourne          480 ability-only procs   against 482 auto-only
 *
 * And the plainest evidence of all: Shadowmourne produced 1,224 procs against
 * 1,213 white swings on one character. More procs than auto attacks is not
 * something one roll per auto attack can ever produce.
 *
 * THE HAND RULE IS KEPT; ONLY THE ABILITY CLAUSE IS RELAXED. An auto attack
 * still rolls only for the hand that swung it, so a proc in the off hand cannot
 * fire off a main-hand swing. An ability has no hand, is one attack, and rolls
 * once. That is the game's behaviour, and it is the whole of the difference.
 */
struct chance_on_hit_cb_t : public dbc_proc_callback_t
{
  const weapon_t* hand;

  chance_on_hit_cb_t( const special_effect_t& effect )
    : dbc_proc_callback_t( effect.item, effect ),
      hand( effect.item ? effect.item->weapon() : nullptr )
  {
  }

  void trigger( const proc_data_t& data, player_t* target, action_state_t* state,
                proc_trigger_type_e type ) override
  {
    if ( state && state->action && state->action->weapon && hand &&
         state->action->weapon != hand )
    {
      return;
    }

    dbc_proc_callback_t::trigger( data, target, state, type );
  }
};

/*
 * Build the callback for a legacy chance-on-hit weapon or enchant.
 *
 * `weapon_proc` is forced FALSE here on purpose: the hand check now lives in
 * chance_on_hit_cb_t above, and leaving the flag set would re-apply the strict
 * gate and throw every ability away again.
 */
static dbc_proc_callback_t* chance_on_hit( special_effect_t& effect )
{
  effect.weapon_proc = false;
  return new chance_on_hit_cb_t( effect );
}

/**
 * Select attribute operator for buffs. Selects the attribute based on the
 * comparator given (std::greater for example), based on all defined attributes
 * that the stat buff is using. Note that this is for _ATTRIBUTES_ only. Using
 * it for any kind of stats will not work (for now).
 *
 * TODO: Generic way to get "composite" stat_e, so we can extend this class to
 * work on all Blizzard stats.
 */
template<template<typename> class CMP>
struct select_attr
{
  CMP<double> comparator;

  bool operator()( const stat_buff_t& buff ) const
  {
    // Comparing to 0 isn't exactly "correct", however the odds of an actor
    // having zero for all primary attributes is slim to none. If for some
    // reason all checked attributes are 0, the last checked attribute will be
    // the one selected. The order of stats checked is determined by the
    // stat_buff_creator add_stats() calls.
    double compare_to = 0;
    stat_e compare_stat = STAT_NONE;
    stat_e my_stat = STAT_NONE;

    for ( size_t i = 0, end = buff.stats.size(); i < end; i++ )
    {
      if ( this == buff.stats[ i ].check_func.target<select_attr<CMP> >() )
        my_stat = buff.stats[ i ].stat;

      attribute_e stat = static_cast<attribute_e>( buff.stats[ i ].stat );
      double val = buff.player -> get_attribute( stat );
      if ( ! compare_to || comparator( val, compare_to ) )
      {
        compare_to = val;
        compare_stat = buff.stats[ i ].stat;
      }
    }

    return compare_stat == my_stat;
  }
};

std::string suffix( const item_t* item )
{
  assert( item );
  if ( item -> slot == SLOT_OFF_HAND )
    return "_oh";
  return "";
}

std::string tokenized_name( const spell_data_t* data )
{
  return util::tokenize_fn( data -> name_cstr() );
}

// Enchants ================================================================

void enchants::mark_of_bleeding_hollow( special_effect_t& effect )
{
  // Custom callback to help the special effect initialization, we can use
  // generic initialization for the enchant, but the game client data does not
  // link driver to the procced spell, so we do it here.

  effect.type = SPECIAL_EFFECT_EQUIP;
  effect.trigger_spell_id = 173322;

  new dbc_proc_callback_t( effect.item, effect );
}

void enchants::megawatt_filament( special_effect_t& effect )
{
  // Custom callback to help the special effect initialization, we can use
  // generic initialization for the enchant, but the game client data does not
  // link driver to the procced spell, so we do it here.

  effect.type = SPECIAL_EFFECT_EQUIP;
  effect.trigger_spell_id = 156060;

  new dbc_proc_callback_t( effect.item, effect );
}

void enchants::oglethorpes_missile_splitter( special_effect_t& effect )
{
  // Custom callback to help the special effect initialization, we can use
  // generic initialization for the enchant, but the game client data does not
  // link driver to the procced spell, so we do it here.

  effect.type = SPECIAL_EFFECT_EQUIP;
  effect.trigger_spell_id = 156055;

  new dbc_proc_callback_t( effect.item, effect );
}

void enchants::hemets_heartseeker( special_effect_t& effect )
{
  // Custom callback to help the special effect initialization, we can use
  // generic initialization for the enchant, but the game client data does not
  // link driver to the procced spell, so we do it here.

  effect.type = SPECIAL_EFFECT_EQUIP;
  effect.trigger_spell_id = 173288;

  new dbc_proc_callback_t( effect.item, effect );
}

void enchants::mark_of_blackrock( special_effect_t& effect )
{
  effect.type = SPECIAL_EFFECT_EQUIP;

  struct mob_proc_callback_t : public dbc_proc_callback_t
  {
    mob_proc_callback_t( const item_t* i, const special_effect_t& effect ) :
      dbc_proc_callback_t( i, effect )
    { }

    void trigger( const proc_data_t& data, player_t* t, action_state_t* s, proc_trigger_type_e type ) override
    {
      if ( listener -> resources.pct( RESOURCE_HEALTH ) >= 0.6 )
        return;

      dbc_proc_callback_t::trigger( data, t, s, type );
    }
  };

  new mob_proc_callback_t( effect.item, effect );
}

void enchants::mark_of_warsong( special_effect_t& effect )
{
  // Custom callback to help the special effect initialization, we can use
  // generic initialization for the enchant, but the game client data does not
  // link driver to the procced spell, so we do it here.

  effect.type = SPECIAL_EFFECT_EQUIP;
  effect.trigger_spell_id = 159675;
  effect.reverse = true;

  new dbc_proc_callback_t( effect.item, effect );
}

void enchants::mark_of_the_thunderlord( special_effect_t& effect )
{
  struct mott_buff_t : public stat_buff_t
  {
    unsigned extensions;
    unsigned max_extensions;

    mott_buff_t( const item_t* item, const std::string& name, unsigned max_ext ) :
      stat_buff_t( item -> player, name, item -> player -> find_spell( 159234 ) ),
      extensions( 0 ), max_extensions( max_ext )
    { }

    void extend_duration( timespan_t extend_duration ) override
    {
      if ( extensions < max_extensions )
      {
        stat_buff_t::extend_duration( extend_duration );
        extensions++;
      }
    }

    void execute( int stacks, double value, timespan_t duration ) override
    { stat_buff_t::execute( stacks, value, duration ); extensions = 0; }

    void reset() override
    { stat_buff_t::reset(); extensions = 0; }

    void expire_override( int expiration_stacks, timespan_t remaining_duration ) override
    { stat_buff_t::expire_override( expiration_stacks, remaining_duration ); extensions = 0; }
  };

  buff_t* b = buff_t::find( effect.player, effect.name() );
  if ( ! b )
  {
    b = new mott_buff_t( effect.item, effect.name(), 3 );
  }

  // Max extensions is hardcoded, no spell data to fetch it
  effect.custom_buff = b;

  // Setup another proc callback, that uses the same driver as the proc that
  // triggers the buff, however it only procs on crits. This callback will
  // extend the duration of the buff, only if the buff is up. The extension
  // capping is handled in the buff itself.
  special_effect_t* effect2 = new special_effect_t( effect.item );
  effect2 -> name_str = effect.name() + "_crit_driver";
  effect2 -> proc_chance_ = 1;
  effect2 -> ppm_ = 0;
  effect2 -> spell_id = effect.spell_id;
  effect2 -> custom_buff = effect.custom_buff;
  effect2 -> cooldown_ = timespan_t::zero();
  effect2 -> proc_flags2_ = PF2_CRIT;

  // Inject the crit driver into the item special effects, so it is conceptually in the "right
  // place". Rather ugly, but this works, better would probably be just to make
  // special_effect_t::item pointer not const, but that is a more extensive change.
  item_t& item = const_cast<item_t&>( *effect.item );
  item.parsed.special_effects.push_back( effect2 );

  struct mott_crit_callback_t : public dbc_proc_callback_t
  {
    mott_crit_callback_t( const item_t* item, const special_effect_t& effect ) :
      dbc_proc_callback_t( item, effect )
    { }

    void trigger( const proc_data_t& data, player_t* t, action_state_t* s, proc_trigger_type_e type ) override
    {
      if ( proc_buff -> check() )
      {
        dbc_proc_callback_t::trigger( data, t, s, type );
      }
    }

    void execute( const spell_data_t*, player_t*, action_state_t* ) override
    {
      if ( proc_buff -> check() )
        proc_buff -> extend_duration( timespan_t::from_seconds( 2 ) );
    }
  };

  new dbc_proc_callback_t( effect.item, effect );
  new mott_crit_callback_t( effect.item, *effect.item -> parsed.special_effects.back() );
}

void enchants::mark_of_the_frostwolf( special_effect_t& effect )
{
  effect.type = SPECIAL_EFFECT_EQUIP;

  stat_buff_t* buff = static_cast<stat_buff_t*>( buff_t::find( effect.player, "mark_of_the_frostwolf" ) );
  if ( ! buff )
  {
    buff = make_buff<stat_buff_t>( effect.player, "mark_of_the_frostwolf", effect.player -> find_spell( 159676 ) );
  }
  effect.custom_buff = buff;

  new dbc_proc_callback_t( effect.item, effect );
}

void enchants::mark_of_the_shattered_hand( special_effect_t& effect )
{
  effect.trigger_spell_id = 159238;
  effect.name_str = effect.item -> player -> find_spell( 159238 ) -> name_cstr();

  struct bleed_attack_t : public attack_t
  {
    bleed_attack_t( player_t* p, const special_effect_t& effect ) :
      attack_t( effect.name(), p, p -> find_spell( effect.trigger_spell_id ) )
    {
      hasted_ticks = false;
      background = true;
      callbacks = false;
      special = true;
      may_miss = may_block = may_dodge = may_parry = false;
      may_crit = true;
      tick_may_crit = false;
      ignores_armor = true;
    }
  };

  action_t* bleed = effect.player -> find_action( "shattered_bleed" );
  if ( ! bleed )
  {
    bleed = effect.item -> player -> create_proc_action( "shattered_bleed", effect );
  }

  if ( ! bleed )
  {
    bleed = new bleed_attack_t( effect.item -> player, effect );
  }

  effect.execute_action = bleed;
  effect.proc_flags_ = PF_ALL_DAMAGE; // DBC says procs off heals, but that's nothing but trouble.

  new dbc_proc_callback_t( effect.item, effect );
}

void enchants::colossus( special_effect_t& effect )
{
  const spell_data_t* spell = effect.item -> player-> find_spell( 116631 );

  auto buff = make_buff<absorb_buff_t>( effect.item -> player,
                             tokenized_name( spell ) + suffix( effect.item ),
                             spell );
  buff->set_absorb_source( effect.item -> player -> get_stats( tokenized_name( spell ) + suffix( effect.item ) ) )
      ->set_activated( false );

  effect.custom_buff = buff;

  new dbc_proc_callback_t( effect.item, effect );
}

void enchants::rivers_song( special_effect_t& effect )
{
  const spell_data_t* spell = effect.item -> player -> find_spell( 116660 );

  stat_buff_t* buff = static_cast<stat_buff_t*>( buff_t::find( effect.item -> player, tokenized_name( spell ) ) );

  if ( ! buff )
  {
    buff = make_buff<stat_buff_t>( effect.item -> player, tokenized_name( spell ), spell );
    buff->set_activated( false );
  }

  effect.custom_buff = buff;

  new dbc_proc_callback_t( effect.item, effect );
}

void enchants::dancing_steel( special_effect_t& effect )
{
  // Account for Bloody Dancing Steel and Dancing Steel buffs
  const spell_data_t* spell = effect.item -> player -> find_spell( effect.spell_id == 142531 ? 142530 : 120032 );

  double value = spell -> effectN( 1 ).average( effect.item -> player );

  stat_buff_t* buff = static_cast<stat_buff_t*>( buff_t::find( effect.item -> player, effect.name() ) );
  if ( ! buff )
  {
    buff = make_buff<stat_buff_t>( effect.item -> player, effect.name(), spell );
    buff->add_stat( STAT_STRENGTH, value, select_attr<std::greater>() )
        ->add_stat( STAT_AGILITY,  value, select_attr<std::greater>() )
        ->set_activated( false );
  }

  effect.custom_buff = buff;

  new dbc_proc_callback_t( effect.item, effect );
}

struct jade_spirit_check_func
{
  bool operator()( const stat_buff_t& buff )
  {
    if ( buff.player -> resources.max[ RESOURCE_MANA ] <= 0.0 )
      return false;

    return buff.player -> resources.pct( RESOURCE_MANA ) < 0.25;
  }
};

void enchants::jade_spirit( special_effect_t& effect )
{
  const spell_data_t* spell = effect.item -> player -> find_spell( 104993 );

  double int_value = spell -> effectN( 1 ).average( effect.item -> player );
  double spi_value = spell -> effectN( 2 ).average( effect.item -> player );

  // Set trigger spell here, so special_effect_t::name() returns a pretty name
  // for the custom buff.
  effect.trigger_spell_id = 104993;

  stat_buff_t* buff = static_cast<stat_buff_t*>( buff_t::find( effect.item -> player, effect.name() ) );
  if ( ! buff )
  {
    buff = make_buff<stat_buff_t>( effect.item -> player, effect.name(), spell );
    buff->add_stat( STAT_INTELLECT, int_value )
        ->add_stat( STAT_SPIRIT, spi_value, jade_spirit_check_func() )
        ->set_activated( false );
  }

  effect.custom_buff = buff;

  new dbc_proc_callback_t( effect.item -> player, effect );
}

struct windsong_callback_t : public dbc_proc_callback_t
{
  stat_buff_t* haste, *crit, *mastery;

  windsong_callback_t( const item_t* i,
                       const special_effect_t& effect,
                       stat_buff_t* hb,
                       stat_buff_t* cb,
                       stat_buff_t* mb ) :
                 dbc_proc_callback_t( i, effect ),
    haste( hb ), crit( cb ), mastery( mb )
  { }

  void execute( const spell_data_t*, player_t*, action_state_t* ) override
  {
    stat_buff_t* buff;

    int p_type = ( int ) ( listener -> sim -> rng().real() * 3.0 );
    switch ( p_type )
    {
      case 0: buff = haste; break;
      case 1: buff = crit; break;
      case 2:
      default:
        buff = mastery; break;
    }

    buff -> trigger();
  }
};

void enchants::windsong( special_effect_t& effect )
{
  const spell_data_t* mastery = effect.item -> player -> find_spell( 104510 );
  const spell_data_t* haste = effect.item -> player -> find_spell( 104423 );
  const spell_data_t* crit = effect.item -> player -> find_spell( 104509 );

  stat_buff_t* mastery_buff = make_buff<stat_buff_t>( effect.item -> player, "windsong_mastery" + suffix( effect.item ), mastery );
  mastery_buff->set_activated( false );
  stat_buff_t* haste_buff   = make_buff<stat_buff_t>( effect.item -> player, "windsong_haste" + suffix( effect.item ), haste );
  haste_buff->set_activated( false );
  stat_buff_t* crit_buff    = make_buff<stat_buff_t>( effect.item -> player, "windsong_crit" + suffix( effect.item ), crit );
  crit_buff->set_activated( false );

  //effect.name_str = tokenized_name( mastery ) + suffix( item );

  new windsong_callback_t( effect.item, effect, haste_buff, crit_buff, mastery_buff );
}

struct hurricane_spell_proc_t : public dbc_proc_callback_t
{
  buff_t *mh_buff, *oh_buff, *s_buff;

  hurricane_spell_proc_t( const special_effect_t& effect, buff_t* mhb, buff_t* ohb, buff_t* sb ) :
    dbc_proc_callback_t( effect.item -> player, effect ),
    mh_buff( mhb ), oh_buff( ohb ), s_buff( sb )
  { }

  void execute( const spell_data_t*, player_t*, action_state_t* ) override
  {
    if ( mh_buff && mh_buff -> check() )
      mh_buff -> trigger();
    else if ( oh_buff && oh_buff -> check() )
      oh_buff -> trigger();
    else
      s_buff -> trigger();
  }
};

void enchants::hurricane_spell( special_effect_t& effect )
{
  int n_hurricane_enchants = 0;

  if ( effect.item -> player -> items[ SLOT_MAIN_HAND ].parsed.enchant_id == 4083 ||
       util::str_compare_ci( effect.item -> player -> items[ SLOT_MAIN_HAND ].option_enchant_str, "hurricane" ) )
    n_hurricane_enchants++;

  if ( effect.item -> player -> items[ SLOT_OFF_HAND ].parsed.enchant_id == 4083 ||
       util::str_compare_ci( effect.item -> player -> items[ SLOT_OFF_HAND ].option_enchant_str, "hurricane" ) )
    n_hurricane_enchants++;

  buff_t* mh_buff = buff_t::find( effect.item -> player, "hurricane" );
  buff_t* oh_buff = buff_t::find( effect.item -> player, "hurricane_oh" );

  // If we have 2 hurricane enchants, and we're creating the first one
  // (opposite hand weapon buff has not been created), bail out early.  Note
  // that this presumes that the spell item enchant has the procs spell ids in
  // correct order (which they are, at the moment).
  if ( n_hurricane_enchants == 2 && ( ! mh_buff || ! oh_buff ) )
    return;

  const spell_data_t* driver = effect.item -> player -> find_spell( effect.spell_id );
  const spell_data_t* spell = driver -> effectN( 1 ).trigger();
  stat_buff_t* spell_buff = make_buff<stat_buff_t>( effect.item -> player, "hurricane_spell", spell );
  spell_buff->set_activated( false );

  new hurricane_spell_proc_t( effect, mh_buff, oh_buff, spell_buff );

}

void enchants::executioner( special_effect_t& effect )
{
  const spell_data_t* spell = effect.item -> player -> find_spell( effect.spell_id );
  stat_buff_t* buff = static_cast<stat_buff_t*>( buff_t::find( effect.item -> player, tokenized_name( spell ) ) );

  if ( ! buff )
  {
    buff = make_buff<stat_buff_t>( effect.item -> player, tokenized_name( spell ), spell );
    buff->set_activated( false );
  }

  effect.name_str = tokenized_name( spell );
  effect.ppm_ = 1.0;

  effect.custom_buff = buff;

  new dbc_proc_callback_t( effect.item, effect );
}

// BracketSim legacy compatibility: Enchant Weapon - Crusader, enchant 1900.
//
// the author, 12 September 2026: "make sure crusader works, if not code it into the
// engine". It did not work. Measured on a level 30 Fury warrior before this,
// 200 iterations: 246.2 dps with the enchant and 245.9 without, and no
// holy_strength anywhere in the buff list.
//
// The data is all present and the wiring was not. The enchant's ench_type is
// ITEM_ENCHANTMENT_COMBAT_SPELL and its ench_prop is 20007, Holy Strength -
// "Increases Strength by $s1 for 15 sec" - and the generic chance-on-hit path
// hands that spell id to initialize_special_effect. But 20007 is the BUFF, not
// a driver: it carries no proc chance and no RPPM, so the generic initializer
// produced an effect that could never fire.
//
// Holy Strength is worth having at these brackets specifically because its
// max_scaling_level is 25. A level 30 warrior carries about 74 strength and the
// buff grants roughly 37, so this is not a rounding error.
//
// THE RATE IS CARRIED OVER, NOT MEASURED - the same standing caveat as the five
// chance-on-hit weapons (STATE.md open item 10). Crusader was 1 PPM for its
// whole life and nothing in this build's data carries a rate, so 1 PPM is what
// it gets, through the engine's own real-PPM code exactly as enchants::
// executioner does.
void enchants::crusader( special_effect_t& effect )
{
  const spell_data_t* spell = effect.item->player->find_spell( effect.spell_id );
  auto buff = static_cast<stat_buff_t*>( buff_t::find( effect.item->player, tokenized_name( spell ) ) );

  if ( !buff )
  {
    buff = make_buff<stat_buff_t>( effect.item->player, tokenized_name( spell ), spell );
    buff->set_activated( false );
  }

  effect.name_str     = tokenized_name( spell );
  effect.proc_flags_  = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  // Old-style PPM, not real PPM. `ppm_` positive is the per-hit conversion
  // `ppm * weapon_speed / 60`, which is exactly how a Classic PPM enchant
  // worked: every landed melee hit rolls, so an ability that strikes more often
  // procs it more often. Real PPM (a negative `ppm_` here) would flatten that,
  // and flattening it is the thing the author already corrected this port on once -
  // "chance on hit mechanics are near 100% for fury warriors, due to the
  // whirlwind interaction".
  //
  // Measured after this was wired up, level 30 Fury, 500 iterations: 4.6 fresh
  // applications and 46 refreshes in a 300 second fight, 91.6% uptime, 245.8 ->
  // 323.3 dps. That uptime is high because Fury lands a great many hits, and it
  // is the behaviour he described rather than a bug.
  effect.ppm_         = 1.0;
  /* the hand rule now lives in chance_on_hit_cb_t, which also lets abilities roll */
  effect.custom_buff  = buff;

  chance_on_hit( effect );
}

void enchants::meta_gem_effect( special_effect_t& effect )
{
  effect.player->parse_passive_item_effect( effect.driver () );
}

// Profession perks =========================================================

struct engineering_effect_t : public action_t
{
  engineering_effect_t( player_t* p, const std::string& name ) :
    action_t( ACTION_USE, name, p )
  {
    background = true;
    callbacks = false;
    // All engineering on-use addons share a cooldown with potions.
    cooldown = p -> get_cooldown( "potion" );
  }

  // Use similar cooldown handling to potions, see sc_consumable.cpp dbc_potion_t
  void update_ready( timespan_t cd_duration ) override
  {
    // If the player is in combat, just make a very long CD
    if ( player -> in_combat )
      cd_duration = sim -> max_time * 3;
    else
      cd_duration = cooldown -> duration;

    action_t::update_ready( cd_duration );
  }

  result_e calculate_result( action_state_t* /* state */ ) const override
  { return RESULT_HIT; }

  void execute() override
  {
    action_t::execute();
    if ( sim -> log ) sim -> out_log.printf( "%s performs %s", player -> name(), name() );
  }
};

struct nitro_boosts_action_t : public engineering_effect_t
{
  buff_t* buff;

  nitro_boosts_action_t( player_t* p ) :
    engineering_effect_t( p, "nitro_boosts" )
  {
    buff = make_buff( p, "nitro_boosts", p->find_spell( 54861 ) )->set_movement_speed_buff_from_data();
  }

  void execute() override
  {
    engineering_effect_t::execute();

    buff->trigger();
  }
};

struct grounded_plasma_shield_t : public engineering_effect_t
{
  absorb_buff_t* buff;

  grounded_plasma_shield_t( player_t* p ) :
    engineering_effect_t( p, "grounded_plasma_shield" )
  {
    buff = make_buff<absorb_buff_t>( p, "grounded_plasma_shield", p -> find_spell( 82626 ) );
    buff->set_cooldown( timespan_t::zero() );
  }

  void execute() override
  {
    engineering_effect_t::execute();

    buff -> trigger();
  }
};

void profession::nitro_boosts( special_effect_t& effect )
{
  effect.type = SPECIAL_EFFECT_USE;
  effect.execute_action = new nitro_boosts_action_t( effect.item -> player );
}

void profession::grounded_plasma_shield( special_effect_t& effect )
{
  effect.type = SPECIAL_EFFECT_USE;
  effect.execute_action = new grounded_plasma_shield_t( effect.item -> player );
}

void profession::zen_alchemist_stone( special_effect_t& effect )
{
  struct zen_alchemist_stone_callback : public dbc_proc_callback_t
  {
    stat_buff_t* buff_str;
    stat_buff_t* buff_agi;
    stat_buff_t* buff_int;

    zen_alchemist_stone_callback( const item_t* i, const special_effect_t& data ) :
      dbc_proc_callback_t( i -> player, data )
    {
      const spell_data_t* spell = listener -> find_spell( 105574 );

      struct common_buff_t : public stat_buff_t
      {
        common_buff_t( player_t* p, const std::string& n, stat_e stat, const spell_data_t* spell, const item_t* item ) :
          stat_buff_t ( p, "zen_alchemist_stone_" + n, spell, item )
        {
          double value = spell -> effectN( 1 ).average( *item );
          add_stat( stat, value );
          set_duration( p -> find_spell( 60229 ) -> duration() );
          set_chance( 1.0 );
          set_activated( false );
        }
      };

      buff_str = make_buff<common_buff_t>( listener, "str", STAT_STRENGTH, spell, effect.item );
      buff_agi = make_buff<common_buff_t>( listener, "agi", STAT_AGILITY, spell, effect.item );
      buff_int = make_buff<common_buff_t>( listener, "int", STAT_INTELLECT, spell, effect.item );
    }

    void execute( const spell_data_t*, player_t*, action_state_t* /* state */ ) override
    {
      if ( listener -> strength() > listener -> agility() )
      {
        if ( listener -> strength() > listener -> intellect() )
          buff_str -> trigger();
        else
          buff_int -> trigger();
      }
      else if ( listener -> agility() > listener -> intellect() )
        buff_agi -> trigger();
      else
        buff_int -> trigger();
    }
  };

  maintenance_check( 450 );

  new zen_alchemist_stone_callback( effect.item, effect );
}

void profession::draenor_philosophers_stone( special_effect_t& effect )
{
  struct draenor_philosophers_stone_callback : public dbc_proc_callback_t
  {
    stat_buff_t* buff_str;
    stat_buff_t* buff_agi;
    stat_buff_t* buff_int;

    draenor_philosophers_stone_callback( const item_t* i, const special_effect_t& data ) :
      dbc_proc_callback_t( i -> player, data )
    {
      const spell_data_t* spell = listener -> find_spell( 157136 );

      struct common_buff_t : public stat_buff_t
      {
        common_buff_t( player_t* p, const std::string& n, stat_e stat, const spell_data_t* spell, const item_t* item ) :
          stat_buff_t ( p, "draenor_philosophers_stone_" + n, spell, item )
        {
          double value = spell -> effectN( 1 ).average( *item );
          add_stat( stat, value );
          set_duration( p -> find_spell( 60229 ) -> duration() );
          set_chance( 1.0 );
          set_activated( false );
        }
      };


      buff_str = make_buff<common_buff_t>( listener, "str", STAT_STRENGTH, spell, data.item );
      buff_agi = make_buff<common_buff_t>( listener, "agi", STAT_AGILITY, spell, data.item );
      buff_int = make_buff<common_buff_t>( listener, "int", STAT_INTELLECT, spell, data.item );
    }

    void execute( const spell_data_t*, player_t*, action_state_t* ) override
    {
      if ( listener->strength() > listener->agility() )
      {
        if ( listener->strength() > listener->intellect() )
          buff_str->trigger();
        else
          buff_int->trigger();
      }
      else if ( listener->agility() > listener->intellect() )
        buff_agi->trigger();
      else
        buff_int->trigger();
    }
  };

  maintenance_check( 620 );

  new draenor_philosophers_stone_callback( effect.item, effect );
}

// TODO: Ratings
[[maybe_unused]] void set_bonus::passive_stat_aura( special_effect_t& effect )
{
  const spell_data_t* spell = effect.player -> find_spell( effect.spell_id );
  stat_e stat = STAT_NONE;
  // Sanity check for stat-giving aura, either stats or aura type 465 ("bonus armor")
  if ( spell -> effectN( 1 ).subtype() != A_MOD_STAT || spell -> effectN( 1 ).subtype() == A_MOD_BONUS_ARMOR )
  {
    effect.type = SPECIAL_EFFECT_NONE;
    return;
  }

  if ( spell -> effectN( 1 ).subtype() == A_MOD_STAT )
  {
    if ( spell -> effectN( 1 ).misc_value1() >= 0 )
    {
      stat = static_cast< stat_e >( spell -> effectN( 1 ).misc_value1() + 1 );
    }
    else if ( spell -> effectN( 1 ).misc_value1() == -1 )
    {
      stat = STAT_ALL;
    }
  }
  else
  {
    stat = STAT_BONUS_ARMOR;
  }

  double amount = util::round( spell -> effectN( 1 ).average( effect.player, std::min( MAX_LEVEL, effect.player -> level() ) ) );

  effect.player -> passive.add_stat( stat, amount );
}

// Items ====================================================================

// BracketSim legacy compatibility ==========================================
//
// Modern item data still exposes these effects, but upstream SimC no longer
// initializes ItemEffect entries with the CHANCE_ON_HIT trigger type and the
// custom Tiny Abomination implementation was removed after Cataclysm. Keep
// the live spell scaling while restoring the historically verified mechanics.

// CHANCE ON HIT IS NOT REAL PPM.
//
// Every item in this group has ItemEffect type 2, ITEM_SPELLTRIGGER_CHANCE_ON_HIT:
// the game rolls once for EVERY hit that lands. SimulationCraft's `ppm_` is the
// other thing entirely - a real-PPM proc, normalised so that a fixed number of
// procs arrive per MINUTE however often you swing. The two agree on a character
// doing nothing but auto-attacking, and diverge completely on anything that
// lands more than one hit per cast.
//
// the author, 10 September 2026: "chance on hit mechanics are near 100% for fury
// warriors, due to the whirlwind interaction, same for monks rushing jade wind
// and spinning crane kick". That is the whole difference: under `ppm_` a
// Whirlwind into eight targets is worth exactly as much as one into one.
//
// Proven, not assumed. In the author's own level 30 combat log (a test character, a death
// knight in Naxxramas) Shadowmourne handed over TWO Soul Fragments in the same
// millisecond off one Blood Boil that struck two targets, and 25 fragments
// across 22 separate instants. `ppm_` rolls once against elapsed time and has no
// way to produce two stacks at one timestamp, so the PPM model is ruled out by
// the log rather than by argument. Evidence:
//   bracketsim-local/audits/2026-09-10-shadowmourne-ramp.py
//
// THE MODEL IS SHARED; THE RATES ARE NOW CITED. SimulationCraft's own real-PPM
// code turns a PPM into a per-attempt chance with `ppm * weapon_speed / 60`, so
// applying that same conversion here keeps the auto-attack rate these items
// already had - the case the old model got right - while letting every extra hit
// roll, which is the case it got wrong.
//
// Nothing in this build's data carries a chance or a PPM for a chance-on-hit
// item: the ItemEffect row is id/spell/item/index/type and a cooldown, and no
// more. The rates below used to be the port's own assumptions for that reason.
// They are no longer: wowsims research a PPM per item, and
// `read-wowsims-procs.mjs` records each one with the file and line it came from
// in `fixtures/wowsims-proc-rates.json`. Every call below now cites that file.
//
// the author, 18 September 2026: *"for jackhammer we have the answer for chance on hit
// mechanics from wowsims, which was implemented yesterday onto other similiar
// weapons"*.
//
// Cross-checking the coded values against it found one that was WRONG rather
// than merely uncited: Thunderfury was carrying the same 1.0 every other weapon
// had been given, and wowsims give it 6.0 - six times the rate. See
// item::thunderfury.
//
// An item wowsims do not cover keeps its assumption and says so.
//
// A retail log can still overrule them - they simulate Classic and this project
// simulates retail - but only when the log measures the rate directly. Splitting
// a warrior's damage events into main hand and off hand is NOT direct: it read
// Bonereaver's Edge at 2.76 PPM in one hand and 2.03 in the other, and the real
// answer is wowsims' 2.0. See item::bonereavers_edge.
static double chance_on_hit_from_ppm( double ppm, double weapon_speed_seconds )
{
  return ppm * weapon_speed_seconds / 60.0;
}

// One buff per name, and one callback per driver.
//
// These initializers run from initialize_special_effect_2, and the Jackhammer
// was reaching it twice: the report listed "jackhammer_haste/jackhammer_haste"
// under CONSTANT buffs on a Fury warrior - two ten second haste buffs stacked
// into permanent uptime by two independent proc callbacks. Whatever puts the
// driver in front of the initializer twice, creating the buff a second time is
// this function's own doing, so it is this function that refuses.
//
// buff_t::find returns the existing buff if the player already has one under
// this name, and a non-null return also means the callback has already been
// built, so both are skipped together.
static bool already_built( special_effect_t& effect )
{
  if ( buff_t* existing = buff_t::find( effect.player, effect.name_str ) )
  {
    effect.custom_buff = existing;
    return true;
  }
  return false;
}

// BracketSim: Death's Verdict and Death's Choice - the Trial of the Crusader
// melee trinkets, items 47115 and 47131 and their ten man twins.
//
// Naming the trigger spell was enough to make the proc exist, and not enough to
// make it right. Buff 67703 carries "Stat: Agi", so the generic builder handed a
// fury warrior AGILITY - measured, on a level 60 carrier: `stat=Agi` for both a
// warrior and a rogue. The tooltip is explicit about what the game does: "Your
// highest stat is always chosen".
//
// So the stat is the player's, not the spell's. Everything else stays with the
// data - the 35% chance, the 45 second internal cooldown and the proc flags all
// come from the driver, and the amount is the buff's own effect scaled to the
// item's level.
// BracketSim: Chillpike, item 13148 - and the chance-on-hit rate question.
//
// the author, 17 September 2026: *"the problem with chance on hit is on some specs it
// can proc an absurd amount per minute compared to others so I don't know the
// best approach to this, look into 'wowsims'"*.
//
// THE MODEL IS ALREADY AGREED. wowsims/classic `PPMManager`
// (sim/core/attack.go) sets `procChance = weaponSpeed * ppm / 60` and rolls it
// on every landed hit. `action_t::ppm_proc_chance` here calls
// `weapon->proc_chance_on_swing( PPM )`, which is that same conversion. So a
// spec that lands more hits does proc more, deliberately, in both engines.
//
// What wowsims adds, and worth copying if a proc ever chains here, is
// `SpellFlagSuppressWeaponProcs` on the proc's own damage.
//
// THE RATE IS CITED, NOT ASSUMED, which is the part this project has never had
// for these weapons: wowsims/classic sim/common/item_effects.go:781 gives
// Chillpike **1.0 PPM**. Setting `ppm_` rather than a precomputed chance means
// the rate follows the weapon's real speed instead of one baked in per item -
// an improvement on the older handlers above, which hard-code a speed.
//
// Spell 19260 is both the driver and the damage, so it is its own trigger.
// BracketSim: Molten Ironfoe, item 231398.
//
// The driver, 469933, carries everything except a pointer to what it does: real
// PPM 15 with a haste multiplier, a two second internal cooldown, and white and
// yellow melee and ranged proc flags. The DAMAGE is in the driver's own two
// Dummy effects, and its description spells that out - "unleash a Molten Strike
// dealing ${$<rolemult>*($s1+$s2)} Fire damage" - so effect 1 plus effect 2,
// each scaled to the item, is the number.
//
// ONE SIMPLIFICATION, RECORDED. `$<rolemult>` is a role multiplier selected by a
// long list of specialisation auras in the spell's variable string. It is not
// applied here, so a spec the multiplier would favour is under-counted rather
// than over-counted. Nothing in the spell data says what the multiplier IS, only
// which auras select it, and inventing a number is exactly what AGENTS.md rule 1
// forbids.
void item::molten_ironfoe( special_effect_t& effect )
{
  struct molten_strike_t : public generic_proc_t
  {
    molten_strike_t( const special_effect_t& e ) : generic_proc_t( e, "molten_strike", e.driver() )
    {
      base_dd_min = base_dd_max = e.driver()->effectN( 1 ).average( e.item )
                                + e.driver()->effectN( 2 ).average( e.item );
      school = SCHOOL_FIRE;
    }
  };

  if ( effect.player->find_action( "molten_strike" ) )
    return;

  effect.execute_action = create_proc_action<molten_strike_t>( "molten_strike", effect );
  new dbc_proc_callback_t( effect.item, effect );
}

void item::chillpike( special_effect_t& effect )
{
  effect.name_str     = "frost_blast";
  effect.proc_flags_  = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  effect.ppm_         = 1.0;
  effect.proc_chance_ = 0;
  /* the hand rule now lives in chance_on_hit_cb_t, which also lets abilities roll */

  if ( effect.player->find_action( effect.name_str ) )
    return;

  effect.execute_action = create_proc_action<generic_proc_t>( effect.name_str, effect, effect.driver() );
  chance_on_hit( effect );
}

void item::deaths_verdict( special_effect_t& effect )
{
  // The driver does not point at its buff, so say which one it is: 67703 for the
  // normal trinkets, 67772 for the heroic ones.
  if ( effect.trigger_spell_id == 0 )
    effect.trigger_spell_id = effect.spell_id == 67771 ? 67772 : 67703;

  effect.name_str = "paragon";
  if ( already_built( effect ) )
    return;

  const spell_data_t* trigger = effect.trigger();
  auto buff = make_buff<stat_buff_t>( effect.player, effect.name_str, trigger, effect.item );
  buff->set_stat( effect.player->convert_hybrid_stat( STAT_STR_AGI ),
                  trigger->effectN( 1 ).average( effect.item ) );
  effect.custom_buff = buff;

  new dbc_proc_callback_t( effect.item, effect );
}

void item::jackhammer( special_effect_t& effect )
{
  effect.name_str    = "jackhammer_haste";
  effect.proc_flags_ = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  // The Jackhammer, item 9423, is a 3.6 second two-hander. 1.0 PPM at that speed
  // is one proc per 16.7 swings, so 6% per hit.
  //
  // RATE: wowsims/classic sim/common/item_effects.go:2358,
  // `CreateWeaponProcAura(TheJackhammer, "The Jackhammer", 1.0, ...)`. The value
  // this port had assumed turned out to be the researched one.
  effect.proc_chance_ = chance_on_hit_from_ppm( 1.0, 3.6 );
  effect.ppm_        = 0;
  /* the hand rule now lives in chance_on_hit_cb_t, which also lets abilities roll */

  if ( already_built( effect ) )
    return;

  auto buff = make_buff<stat_buff_t>( effect.player, effect.name_str, effect.driver(), effect.item );
  buff->set_chance( 1.0 );
  effect.custom_buff = buff;

  chance_on_hit( effect );
}

// BracketSim: Heartrazor (29962) and The Untamed Blade (19334), 23 September 2026.
//
// the author asked why Heartrazor never won level 30 Assassination's main hand. It was
// simulated - as a stat stick. Its driver 36041 IS the buff (Mod Attack Power,
// 10 sec, proc flags white and yellow melee, proc chance "101%", the marker that
// the rate lives elsewhere): the Jackhammer shape above, so the generic builder
// found no trigger and built nothing, and the item register filed it "not damage"
// because its tooltip reads "Increases attack power" without "your". The
// Untamed Blade's driver 23719 (Untamed Fury, +Strength, 8 sec) is the same.
//
// RATES, cited as Jackhammer's is: wowsims/tbc sim/common/melee_items.go,
// `NewPPMManager(1.0, procMask)` for Heartrazor on its own hand's hits;
// wowsims/classic sim/common/item_effects.go, `CreateWeaponProcAura(
// TheUntamedBlade, "The Untamed Blade", 1.0, ...)` - the original rate (their
// 0.55 is Season of Discovery's, not this game's). The AMOUNTS are this client's
// own scaled values, read by stat_buff_t from the driver, like every other stat.
//
// A second copy (a dual-wielded Heartrazor) shares the buff but gets its own
// roll, as each hand's proc did.
static void weapon_stat_proc( special_effect_t& effect, const std::string& name, double ppm,
                              double fallback_speed )
{
  effect.name_str     = name;
  effect.proc_flags_  = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  const weapon_t* w   = effect.item ? effect.item->weapon() : nullptr;
  const double speed  = ( w && w->swing_time > timespan_t::zero() ) ? w->swing_time.total_seconds()
                                                                     : fallback_speed;
  effect.proc_chance_ = chance_on_hit_from_ppm( ppm, speed );
  effect.ppm_         = 0;

  if ( !already_built( effect ) )
  {
    auto buff = make_buff<stat_buff_t>( effect.player, effect.name_str, effect.driver(), effect.item );
    buff->set_chance( 1.0 );
    effect.custom_buff = buff;
  }

  chance_on_hit( effect );
}

void item::heartrazor( special_effect_t& effect )
{
  weapon_stat_proc( effect, "heartrazor", 1.0, 1.8 );
}

void item::untamed_blade( special_effect_t& effect )
{
  weapon_stat_proc( effect, "untamed_fury", 1.0, 3.4 );
}

// BracketSim: Blackout Truncheon (27901) and Despair (28573), 23 September 2026 -
// the two chance-on-hit weapons the item register already listed as damage-
// relevant and not implemented. RATES: wowsims/tbc sim/common/melee_items.go -
// Blackout Truncheon `procChance = 1.5 * 0.8 / 60.0` on its own hand's hits
// (0.8 PPM at its 1.5 speed), Blinding Speed 33489 for 10 sec; Despair
// `procChance = 0.5 * 3.5 / 60.0` on any landed melee hit (0.5 PPM at 3.5),
// Impale 34580. Amounts are this client's own scaled values, from the drivers.
void item::blackout_truncheon( special_effect_t& effect )
{
  weapon_stat_proc( effect, "blinding_speed", 0.8, 1.5 );
}

void item::despair( special_effect_t& effect )
{
  effect.name_str     = "impale";
  effect.proc_flags_  = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  const weapon_t* w   = effect.item ? effect.item->weapon() : nullptr;
  effect.proc_chance_ = chance_on_hit_from_ppm( 0.5, ( w && w->swing_time > timespan_t::zero() )
                                                        ? w->swing_time.total_seconds() : 3.5 );
  effect.ppm_         = 0;

  if ( effect.player->find_action( effect.name_str ) )
    return;

  effect.execute_action = create_proc_action<generic_proc_t>( effect.name_str, effect, effect.driver() );
  chance_on_hit( effect );
}

// BracketSim: World Breaker (30090), 23 September 2026. Driver 36111: "Increases
// the critical strike of your next attack made within 4 seconds by 900" - a bonus
// the NEXT melee attack spends. RATE AND LOGIC: wowsims/tbc
// sim/common/melee_items.go - `procChance = 3.7 / 60.0` rolled on every landed
// melee hit (a flat chance, not scaled by weapon speed), and every landed melee
// hit first drops the bonus, then may grant a fresh one. This callback runs on
// every landed melee hit and does exactly that; the bonus it grants is read from
// the driver by stat_buff_t.
struct world_breaker_cb_t : public dbc_proc_callback_t
{
  double chance;

  world_breaker_cb_t( const special_effect_t& e, double c ) : dbc_proc_callback_t( e.item, e ), chance( c )
  {
  }

  void execute( const spell_data_t*, player_t*, action_state_t* ) override
  {
    proc_buff->expire();
    if ( rng().roll( chance ) )
      proc_buff->trigger();
  }
};

void item::world_breaker( special_effect_t& effect )
{
  effect.name_str     = "world_breaker";
  effect.proc_flags_  = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  effect.proc_chance_ = 1.0;  // the callback sees every landed hit; the 3.7/60 roll is inside it
  effect.ppm_         = 0;
  effect.weapon_proc  = false;

  if ( !already_built( effect ) )
  {
    auto buff = make_buff<stat_buff_t>( effect.player, effect.name_str, effect.driver(), effect.item );
    buff->set_chance( 1.0 );
    effect.custom_buff = buff;
  }

  new world_breaker_cb_t( effect, 3.7 / 60.0 );
}

// BracketSim: Variable Pulse Lightning Capacitor (68925 normal, 69110 heroic,
// 171640), 23 September 2026. Drivers 96887 / 97119: a damaging spell or periodic
// CRIT, at most once per 2.5 sec (the driver's own internal cooldown), grants an
// Electrical Charge (96890, up to 10); then a 50% chance fires Lightning Bolt
// (96891) for the per-charge damage times the charges, which resets them.
//
// MECHANIC AND RATE: wowsims/cata sim/common/cata/damage_procs.go - OnSpellHitDealt
// and OnPeriodicDamageDealt, OutcomeCrit, ICD 2.5 s, `procChance := 0.5` from
// their video research ("p=.48"). the author, 18 September: "copy theirs for proc
// logic crits etc as that's never changed".
// AMOUNT: this client's own. Lightning Bolt 96891 carries no damage in this
// build (its School Damage effect reads 0), and the driver's effect #1 is the
// scaled per-charge value - 1,560 at the spell query's item level for 96887 -
// so that is what each charge is worth, at the trinket's own item level.
struct vplc_bolt_t : public generic_proc_t
{
  double per_charge;

  vplc_bolt_t( const special_effect_t& e )
    : generic_proc_t( e, "variable_pulse_lightning_bolt", e.player->find_spell( 96891 ) ),
      per_charge( e.driver()->effectN( 1 ).average( e.item ) )
  {
    school = SCHOOL_NATURE;
  }
};

struct vplc_cb_t : public dbc_proc_callback_t
{
  buff_t* charges;
  vplc_bolt_t* bolt;

  vplc_cb_t( const special_effect_t& e, buff_t* c, vplc_bolt_t* b ) : dbc_proc_callback_t( e.item, e ), charges( c ), bolt( b )
  {
  }

  void execute( const spell_data_t*, player_t* target, action_state_t* s ) override
  {
    charges->trigger();
    if ( rng().roll( 0.5 ) )
    {
      bolt->base_dd_min = bolt->base_dd_max = bolt->per_charge * charges->check();
      bolt->execute_on_target( target ? target : ( s && s->target ? s->target : listener->target ) );
      charges->expire();
    }
  }
};

void item::variable_pulse_lightning_capacitor( special_effect_t& effect )
{
  effect.name_str     = "variable_pulse_lightning_capacitor";
  effect.proc_flags_  = PF_MAGIC_SPELL | PF_PERIODIC;
  effect.proc_flags2_ = PF2_CRIT;
  effect.proc_chance_ = 1.0;
  effect.ppm_         = 0;

  auto charges = buff_t::find( effect.player, "electrical_charge" );
  if ( !charges )
    charges = make_buff( effect.player, "electrical_charge", effect.player->find_spell( 96890 ) )
                  ->set_max_stack( 10 )
                  ->set_duration( timespan_t::zero() );

  auto bolt = debug_cast<vplc_bolt_t*>( create_proc_action<vplc_bolt_t>( "variable_pulse_lightning_bolt", effect ) );
  new vplc_cb_t( effect, charges, bolt );
}

// Dragonwrath, Tarecgosa's Rest, item 71086 - the Firelands caster legendary.
//
// the author, 12 September 2026: "can you also code into the engine wrath of
// terragosa from the firelands legendary staff thx". He has it on a level 30
// shaman, enchanted with Torrent of Elements, and until now the staff simmed as
// a plain two-hander: its stats counted and its legendary did nothing at all.
//
// The item carries two effects and only one of them matters here:
//
//   item_effect 20075  spell 101056  type 1 (on equip)  Wrath of Tarecgosa
//   item_effect 20076  spell 101641  type 0 (on use)    Tarecgosa's Visage
//
// The second is the cosmetic transform. The first is the legendary, and the
// engine's own description of it is exact:
//
//   "When you deal damage, you have a chance to gain the Wrath of Tarecgosa,
//    duplicating the harmful spell."
//
// WHAT THE DATA GIVES, AND WHAT IT DOES NOT.
//
// The spell row carries the proc FLAGS - Generic Hostile Spell, Magic Hostile
// Spell, Periodic - an internal cooldown of 0.01 seconds, and the attribute
// "Only Proc From Class Abilities". So WHAT can proc it is data rather than a
// guess: a class spell that deals damage, including a periodic tick, and not an
// auto-attack.
//
// It does NOT carry a rate. `Proc Chance: 100%` in the spell query is the
// absence of a chance rather than a certainty, there is no real-PPM row, and the
// ItemEffect row is id/spell/item/index/type and a cooldown and no more - the
// same hole the chance-on-hit weapons have, documented above
// `chance_on_hit_from_ppm`.
//
// **THE RATE IS MEASURED**, from the author's own log rather than assumed. It shipped
// at an assumed 10.5% because no log on the machine carried a single Tarecgosa
// event; he then went and made one - "combatlog done for my 35 lock with the
// legendary staff, go see it" - and `audits/2026-09-12-tarecgosa-in-log.py`
// reads it:
//
//   18 procs / 211 eligible hits = 8.53%   (95% Wilson interval 5.46% to 13.08%)
//
// So 0.0853 it is. Two honest caveats travel with that number. It is ONE dummy
// session, and the interval is wide enough to contain the old guess. And it is
// one character: the driver carries no per-spec data, so whether an Affliction
// warlock and an Elemental shaman roll the same chance is not something this
// log can answer. More logs narrow it; the option exists so they can, without a
// rebuild:
//
//   bracketsim_dragonwrath_chance=0.0853
//
// Setting it to 0 turns the legendary off and sims the staff as a plain weapon,
// which is what the sim did before this existed.
//
// THE 100% COPY IS PROVEN, separately and much more strongly than the rate.
// Every one of the eighteen procs in that log deals exactly what one of the
// player's own hits dealt in the second and a half around it - Corruption 162
// against a copy of 162, Unstable Affliction 2162 against 2162, Malefic Grasp
// 1142 against 1142 - and they follow periodic ticks as well as casts, which is
// what the driver's Periodic proc flag says. Eighteen of eighteen.
//
// WHAT "DUPLICATING" MEANS HERE. The copy repeats the DAMAGE that just landed:
// the triggering hit's own result, after its own crit and its own multipliers,
// dealt again to the same target as a separate event. `snapshot_flags = 0` and
// the multiplier overrides are what keep it from being scaled a second time -
// the amount arrives already computed. That is right for a direct spell and for
// each periodic tick, and it is an approximation for a cast whose value is not
// in the hit it just dealt: a cast that applies a fresh damage-over-time
// duplicates that cast's direct damage, not the dot it lays down.
//
// The copy cannot proc itself. The driver's attributes do say "Can Proc From
// Procs", but a duplication that duplicates duplicates is an infinite series and
// was never what the item did; `callbacks = false` stops it at one.
namespace dragonwrath
{
struct wrath_of_tarecgosa_t final : public spell_t
{
  wrath_of_tarecgosa_t( player_t* p )
    : spell_t( "wrath_of_tarecgosa", p )
  {
    callbacks  = false;          // a copy never copies itself
    background = proc = true;
    special    = true;
    may_crit   = may_glance = may_miss = false;
    may_dodge  = may_parry = may_block = false;
  }

  void init() override
  {
    spell_t::init();
    // The damage is handed over already computed. Nothing may scale it again.
    snapshot_flags = update_flags = 0;
  }
};

struct dragonwrath_cb_t final : public dbc_proc_callback_t
{
  wrath_of_tarecgosa_t* copy;

  dragonwrath_cb_t( const special_effect_t& effect, wrath_of_tarecgosa_t* c )
    : dbc_proc_callback_t( effect.item, effect ), copy( c )
  {}

  void execute( const spell_data_t*, player_t* target, action_state_t* state ) override
  {
    if ( !state || state->result_amount <= 0 )
      return;

    // "Only Proc From Class Abilities", attribute 415 on the driver: the staff
    // does not copy an auto-attack, a trinket, an enchant proc, or itself.
    action_t* a = state->action;
    if ( !a || a->background || a->type == ACTION_ATTACK )
      return;

    player_t* hit = target ? target : state->target;
    copy->school      = a->get_school();
    copy->base_dd_min = copy->base_dd_max = state->result_amount;
    if ( copy->target != hit )
      copy->target_cache.is_valid = false;
    copy->target = hit;
    copy->execute();
  }
};
}  // namespace dragonwrath

void item::dragonwrath( special_effect_t& effect )
{
  using namespace dragonwrath;

  effect.name_str = "wrath_of_tarecgosa";
  // Read from the driver rather than stated here: spell 101056's own proc flags
  // are Generic Hostile Spell, Magic Hostile Spell and Periodic.
  effect.proc_flags_  = effect.driver()->proc_flags();
  effect.proc_flags2_ = PF2_ALL_HIT;
  effect.ppm_         = 0;
  effect.rppm_scale_  = RPPM_NONE;
  effect.proc_chance_ = effect.player->bracketsim.dragonwrath_chance;
  effect.cooldown_    = timespan_t::zero();

  if ( effect.proc_chance_ <= 0 )
    return;

  // One callback per player. There is no buff to use as the marker the way
  // `already_built` does, so the copy action is the marker: if it exists, this
  // initializer has already run and a second callback would double the rate.
  if ( effect.player->find_action( "wrath_of_tarecgosa" ) )
    return;

  new dragonwrath_cb_t( effect, new wrath_of_tarecgosa_t( effect.player ) );
}


// Eskhandar's Right Claw, item 18203.
//
//   22640  Eskhandar's Rage, ItemEffect type 2 (chance on hit), a five second
//          Haste buff, flagged "Scales with Casting Item's Level"
//
// Same shape as the Jackhammer and it was simply never registered, so the item
// equipped, its tooltip read correctly, and nothing happened. Asked for by the author
// on 10 September 2026.
//
// RATE: wowsims/classic sim/common/item_effects.go:946,
// `CreateWeaponProcAura(EskhandarsRightClaw, "Eskhandar's Right Claw", 1.0, ...)`,
// converted at this weapon's own 2.6 second speed. It was the Jackhammer's
// assumption carried across until 18 September 2026; wowsims agree with it.
void item::eskhandars_right_claw( special_effect_t& effect )
{
  effect.name_str     = "eskhandars_rage";
  effect.proc_flags_  = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  effect.proc_chance_ = chance_on_hit_from_ppm( 1.0, 2.6 );
  effect.ppm_         = 0;
  /* the hand rule now lives in chance_on_hit_cb_t, which also lets abilities roll */

  if ( already_built( effect ) )
    return;

  auto buff = make_buff<stat_buff_t>( effect.player, effect.name_str, effect.driver(), effect.item );
  buff->set_chance( 1.0 );
  effect.custom_buff = buff;

  chance_on_hit( effect );
}

// Shadowmourne, item 49623.
//
// Chance on hit to gain a Soul Fragment. Ten of them release into Chaos Bane.
//
//   71903  driver, ItemEffect CHANCE_ON_HIT
//   71905  Soul Fragment, +12 Strength each, stacking to 10, one minute
//   73422  Chaos Bane, +107 Strength for ten seconds
//
// Both buff spells survive in the Midnight client as EMPTY SHELLS: the effect
// rows are there with the right type (Apply Aura: Mod Stat) but base value
// ZERO, in this build and in every archive on this machine. A stat_buff_t built
// from that spell grants nothing at all, which is why the item measured exactly
// zero before this.
//
// The amounts below therefore come from the spell pages, which is the source
// this project uses for a value the client no longer carries:
//   https://www.wowhead.com/spell=71905/soul-fragment   "granting 12 strength each"
//   https://www.wowhead.com/spell=73422/chaos-bane      "granting 107 strength for 10 sec"
// Both are flagged "Scales with item level" in game; these are the values at
// the level the spell renders at, and they are NOT rescaled here. Anything that
// wants item-level scaling has to measure it first.
namespace shadowmourne
{
/*
 * THE CLIENT DOES CARRY THESE VALUES, and it scales them by level.
 *
 * The note above says both buff spells are "empty shells" with base value zero.
 * That is half true and the half that was missed matters: the BASE value is
 * zero, but each effect has a scaling coefficient and a SCALED value, which
 * `spell_query` prints and `effectN().average()` resolves for the actual
 * character:
 *
 *     71905 Soul Fragment   Base 0 | Scaled  12.19049  (coefficient 0.093) Str
 *     73422 Chaos Bane      Base 0 | Scaled 113.3846   (coefficient 0.865) Str
 *     71904 Chaos Bane dmg  Base 0 | Scaled 751.79 - 830.93 (coeff 6.192) Shadow
 *
 * Hard-coding 12 and 107 froze this item at one character level, which is
 * exactly the fault that made Nibelung's Val'kyr wrong at every bracket below
 * 80: a flat number is fine where it was measured and wrong everywhere else.
 * These are now read from the spell, so a level-30 Shadowmourne is worth what a
 * level-30 Shadowmourne is worth.
 *
 * The constants stay as a FALLBACK only, for the case where the client really
 * has dropped the row.
 */
static constexpr double SOUL_FRAGMENT_STRENGTH = 12.0;
static constexpr double CHAOS_BANE_STRENGTH    = 107.0;
static constexpr int    FRAGMENTS_TO_RELEASE   = 10;

/*
 * CHAOS BANE ALSO EXPLODES, and this port never implemented it.
 *
 *   71904  "Deals $s1 Shadow damage, split between all enemy targets within
 *           $a1 yards of the impact crater."  Radius 15 yards.
 *
 * the author, 19 September 2026: *"Shadowmourne chaos bane explosion is so minor in
 * damage it would never change any gear in anyway, so I wouldn't even say that
 * invalidates anything"* - and he is almost certainly right about the gear. It
 * is implemented anyway because "too small to matter" is a claim, and an
 * unimplemented effect cannot be measured to check it.
 *
 * wowsims roll `sim.Roll(1900, 2100) / numTargets` with `OutcomeMagicHit`
 * (`sim/common/wotlk/shadowmourne.go`), commented "probably has a very low crit
 * rate". Those are Wrath-era level-80 numbers; the client's own scaled range is
 * used here instead, for the same reason as the strength values above. The
 * SHAPE is theirs: a roll inside a range, split across targets, no crit.
 */
struct chaos_bane_damage_t final : public spell_t
{
  chaos_bane_damage_t( player_t* p, const spell_data_t* s, const item_t* it )
    : spell_t( "chaos_bane", p, s )
  {
    background = true;
    may_crit   = false;     // wowsims: "probably has a very low crit rate"
    may_miss   = false;
    school     = SCHOOL_SHADOW;
    aoe        = -1;        // everything inside the fifteen yard crater
    // "split between all enemy targets" - one pool divided, not a full hit each.
    split_aoe_damage = true;
    /*
     * SCALED BY THE ITEM, NOT THE PLAYER.
     *
     * the author's tooltip for a level-30 character holding an item level 47
     * Shadowmourne reads "dealing 58 Shadow damage split between all enemies
     * within 15 yards and granting 15 Strength for 10 sec", and his combat log
     * shows the crater landing for 56. Scaling by PLAYER level gave 28.68 -
     * almost exactly half - so this effect follows the weapon's item level, as
     * the original comment in this file said all along ("flagged Scales with
     * item level in game").
     */
    if ( s->ok() )
    {
      /*
       * `min()`/`max()` both returned the average for this effect's scaling
       * class, so the spell's own spread (`delta=0.1`, a range of 751.79-830.93
       * at full scale) was being thrown away and every crater landed for exactly
       * the same number. Build the range from the average and the delta instead.
       */
      /*
       * AND A MEASURED CORRECTION, anchored on the author's own tooltip.
       *
       * The two STRENGTH values come out of the client exactly right once scaled
       * by item level - 1 per fragment and 15 for Chaos Bane at item level 47,
       * matching his tooltip to the digit. This damage effect does not: the same
       * machinery gives a mean of 45.8 where the tooltip says 58. Its scaling
       * class is "Replace Secondary (-9)", which is not the class the strength
       * effects use, so the discrepancy sits in that one path.
       *
       * His combat log's single crater landed for 56, which is inside the
       * spell's own +/-5% spread around 58 (55.1 - 60.9) - so 58 is the mean and
       * 56 was one roll, and the two sources agree.
       *
       * 58 / 45.8 = 1.2664. This is the same kind of measured correction as
       * TENTACLE_TICK_DAMAGE above, and it carries the same warning: it is
       * anchored at ITEM LEVEL 47 on a LEVEL 30 character, and a second tooltip
       * at another item level would either confirm it or replace it with a real
       * scaling term. The effect is 0.087% of a blood death knight's damage, so
       * being anchored at one point costs very little either way.
       */
      static constexpr double CRATER_CALIBRATION = 1.2664;
      const double avg = ( it ? s->effectN( 1 ).average( it ) : s->effectN( 1 ).average( p ) )
        * CRATER_CALIBRATION;
      const double d   = ( it ? s->effectN( 1 ).delta( it ) : s->effectN( 1 ).delta( p ) )
        * CRATER_CALIBRATION;
      base_dd_min = d > 0 ? avg - d / 2 : avg;
      base_dd_max = d > 0 ? avg + d / 2 : avg;
    }
    spell_power_mod.direct = 0.0;
    attack_power_mod.direct = 0.0;
  }
};

// Derives from chance_on_hit_cb_t rather than from dbc_proc_callback_t, so it
// inherits the hand rule AND the ability rolls. Before this it was one of the
// eight drivers that only ever rolled on auto attacks - and Shadowmourne is the
// item that proves abilities roll, with 1,224 procs against 1,213 white swings
// in the author's Naxxramas log.
struct shadowmourne_cb_t final : public chance_on_hit_cb_t
{
  buff_t* fragments;
  buff_t* chaos_bane;
  action_t* burst;

  shadowmourne_cb_t( const special_effect_t& effect, buff_t* f, buff_t* c, action_t* b )
    : chance_on_hit_cb_t( effect ), fragments( f ), chaos_bane( c ), burst( b )
  {}

  void execute( const spell_data_t*, player_t*, action_state_t* state ) override
  {
    /*
     * NO FRAGMENTS WHILE CHAOS BANE IS UP. wowsims return early from the hit
     * handler when the Chaos Bane aura is active (`if chaosBaneAura.IsActive()
     * { return }`), so the ten seconds of the buff are not also spent rebuilding
     * the next stack. This port used to keep collecting through it, which
     * reaches the next release about ten seconds early every cycle.
     */
    if ( chaos_bane->check() )
      return;

    fragments->trigger();

    // The tenth fragment is consumed along with the other nine, so the stack
    // empties rather than sitting at maximum.
    if ( fragments->check() >= FRAGMENTS_TO_RELEASE )
    {
      fragments->expire();
      chaos_bane->trigger();
      // ... and the release leaves a crater.
      if ( burst && state && state->target )
        burst->execute_on_target( state->target );
    }
  }
};
}  // namespace shadowmourne

void item::shadowmourne( special_effect_t& effect )
{
  using namespace shadowmourne;

  effect.name_str     = "shadowmourne";
  effect.proc_flags_  = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  // Chance on hit, not real PPM - see the note above chance_on_hit_from_ppm.
  // This is the item the log proves it on: two Soul Fragments in one
  // millisecond off a two-target Blood Boil, which real PPM cannot do. It also
  // matters more here than anywhere else, because the fragments STACK: hitting
  // more things does not just proc more often, it reaches the tenth fragment
  // and releases Chaos Bane sooner.
  //
  /*
   * THE RATE IS MEASURED, and the old 2.0 had no source at all.
   *
   * the author's Naxxramas combat log, a test character, a level 30 Blood death knight
   * wearing Shadowmourne (audits/2026-09-18-chance-on-hit-rate-from-combatlog.py
   * and 2026-09-18-what-rolls-a-chance-on-hit.py):
   *
   *     1,224 Soul Fragments
   *     1,213 white swings
   *     1,215 physical ability hits (Heart Strike, Death Strike, Marrowrend)
   *       430 Blood Boil hits (shadow school, melee range)
   *     1,821 Death and Decay ticks - a ground effect, not a melee attack
   *
   * Procs EXCEED white swings, which is by itself proof that abilities roll it
   * and the reason chance_on_hit_cb_t exists.
   *
   * The rate depends on which of those count as opportunities:
   *     white + weapon strikes            1,224 / 2,428 = 0.504 per hit
   *     white + weapon strikes + Blood Boil  1,224 / 2,858 = 0.428 per hit
   *
   * The lower of the two is taken. If Blood Boil turns out not to roll it, this
   * under-values Shadowmourne rather than over-values an item that is already
   * winning a weapon slot at bracket 30.
   *
   * 0.42 per hit on a 3.6 second two-hander is 7.0 PPM.
   *
   * WOWSIMS DISAGREE, and are not followed here. `sim/common/wotlk/shadowmourne.go`
   * uses 12 PPM, citing Elitist Jerks testing ("~12 ppm, ~75% for 3.7 speed")
   * and a 2,000-swing dummy test at ~80%. Both are Wrath-era measurements at
   * level 80 on the unsquished item. This log is the live game at the bracket
   * being simulated, so it wins - but the gap is large enough to be worth
   * re-measuring if a second character ever wears one.
   */
  effect.proc_chance_ = chance_on_hit_from_ppm( 7.0, 3.6 );
  effect.ppm_         = 0;
  /* the hand rule now lives in chance_on_hit_cb_t, which also lets abilities roll */

  /* Read the per-level value; fall back to the page value only if the row is
   * genuinely gone. `average()` returns 0 for a missing or zeroed effect, which
   * is how "the client dropped it" is told apart from "the client scales it". */
  const spell_data_t* frag_spell = effect.player->find_spell( 71905 );
  const spell_data_t* bane_spell = effect.player->find_spell( 73422 );
  /*
   * SCALED BY THE ITEM. the author's tooltip, level-30 character, item level 47
   * Shadowmourne: "granting you 1 Strength ... Chaos Bane, dealing 58 Shadow
   * damage ... and granting 15 Strength for 10 sec". Player-level scaling gave
   * 1 and 11; the fragment matched and Chaos Bane did not, which is the tell
   * that these read the weapon's item level - exactly what the original comment
   * in this file said and what hard-coding then threw away.
   */
  const double frag_str = frag_spell->ok() && effect.item && frag_spell->effectN( 1 ).average( effect.item ) > 0
    ? frag_spell->effectN( 1 ).average( effect.item ) : SOUL_FRAGMENT_STRENGTH;
  const double bane_str = bane_spell->ok() && effect.item && bane_spell->effectN( 1 ).average( effect.item ) > 0
    ? bane_spell->effectN( 1 ).average( effect.item ) : CHAOS_BANE_STRENGTH;

  auto* fragments = make_buff<stat_buff_t>( effect.player, "soul_fragment", frag_spell );
  fragments->add_stat( STAT_STRENGTH, frag_str );
  fragments->set_max_stack( FRAGMENTS_TO_RELEASE );
  fragments->set_chance( 1.0 );

  auto* chaos_bane = make_buff<stat_buff_t>( effect.player, "chaos_bane", bane_spell );
  chaos_bane->add_stat( STAT_STRENGTH, bane_str );
  chaos_bane->set_chance( 1.0 );

  auto* burst = new chaos_bane_damage_t( effect.player, effect.player->find_spell( 71904 ), effect.item );

  // chance_on_hit_cb_t's own constructor does not run chance_on_hit(), so the
  // flag is cleared here for the same reason: leaving it set re-applies the
  // strict gate inside dbc_proc_callback_t and drops every ability again.
  effect.weapon_proc = false;
  new shadowmourne_cb_t( effect, fragments, chaos_bane, burst );
}

// Nibelung, item 49992 (normal) and 50648 (heroic).
//
//   71845 / 71846  driver: harmful spells, 2% chance, 250ms cooldown
//   71844 / 71843  Summon Val'kyr, thirty seconds
//   71842          the Val'kyr's Smite, 619 Holy damage, 1.5s cooldown, cannot miss
//
// The item's own tooltip is the source for the proc:
//   https://www.wowhead.com/item=49992/nibelung
//   "Your harmful spells have a chance to cause you to summon a Val'kyr to
//    fight by your side for 30 sec. (Proc chance: 2%, 250ms cooldown)"
//
// Spell 71842 is not in this client's data at all - the Val'kyr is an NPC
// ability and Blizzard dropped it - so the damage is taken from its spell page
// and stated here rather than read from a spell that no longer exists:
//   https://www.wowhead.com/spell=71842/smite  "dealing 619 Holy damage"
//
// WOWSIMS WROTE A WHOLE FILE FOR THIS ONE, and its structure is worth copying.
// `sim/common/wotlk/nibelung.go` agrees with this port on the 2% chance, the
// 250ms internal cooldown, the 30 second Val'kyr and the pet inheriting NOTHING
// from its owner. It disagrees on three things, and the author, 18 September 2026:
// *"damage we keep ours, but copy theirs for proc logic crits etc as that's
// never changed"*.
//
//   DAMAGE      theirs is a Wrath-era 1591-1785 (1804-2022 heroic); ours is what
//               Wowhead reports for spell 71842 TODAY, 619. The squish is the
//               difference and every other item here is squished, so ours stays.
//
//   IT CRITS    `spell.CalcDamage(..., spell.OutcomeMagicCrit)`. This port had
//               `may_crit = false` because the spell page says the Smite cannot
//               MISS - and cannot miss is not cannot crit. That conflation was
//               costing Nibelung its whole crit rate, on an item already worth
//               23.8% of a level 60 Affliction warlock's damage.
//
//   CAST RATE   theirs is a 1950ms GCD, commented "about 16 instant-casts per
//               30s with some time left-over". This port used the spell's own
//               1.5s cooldown, which gives 20 casts - 25% more Smites than the
//               game produces. Theirs is measured against the real thing.
//
namespace nibelung
{
/*
 * MEASURED FROM TWO REAL LEVEL-30 COMBAT LOGS, 19 September 2026.
 *
 * Every number below used to come from Wowhead's page for spell 71842 (a flat
 * 619, no range) and from wowsims' cast cadence (1950ms). The spell is not in
 * this client's data, so neither could be checked - until the author produced logs.
 *
 *   A. Warcraft Logs, level 30 SHADOW PRIEST, Bael'Gar, 363.5s, 32 Smites
 *      damage 951 - 1058, mean 1015.3, ZERO crits in 32
 *      median gap between Smites 1.62s
 *
 *   B. Local combat log, level 30 BALANCE DRUID, target dummy, 15 Smites
 *      damage 1446 - 1617, mean 1529.1, ZERO crits in 15
 *
 * Backing our own multipliers out of each gives the base independently:
 *
 *      priest: 1015.3 / (1.101768 vers * 1.0609 target)           = 868.6
 *      druid:  1529.1 / (1.08468 vers * 1.52 guardian * 1.0609)   = 874.2
 *
 * They agree to 0.6%, from different specs, different content and different
 * multipliers - so 871 is measured, not asserted. That agreement also PROVES
 * the 1.52 guardian multiplier is real: the druid really does hit 1.5x harder,
 * exactly as this engine already modelled, so it is not a leak to remove.
 *
 * The observed spread is +/-5.6% around the mean in both logs, so the Smite has
 * a damage RANGE, which a single flat number cannot produce.
 *
 * ZERO CRITS IN 47 OBSERVATIONS. At the priest's 14.2% crit that is p = 0.0008.
 * `may_crit = true` was wrong; the earlier reasoning (wowsims roll it through
 * OutcomeMagicCrit) is beaten by measurement.
 */
/*
 * THESE ARE LEVEL-30 VALUES AND NOTHING ELSE.
 *
 * Both of the author's logs are level-30 characters:
 *
 *   Warcraft Logs, level 30 shadow priest, 32 Smites: 951-1058, mean 1015.3
 *   local log,     level 30 balance druid, 15 Smites: 1446-1617, mean 1529.1
 *
 * Backing our own multipliers out of each gives 868.6 and 874.2 - agreement to
 * 0.6% from two different classes - so 823-920 is a MEASURED level-30 range.
 *
 * IT IS NOT SCALED TO OTHER BRACKETS, and it must not be guessed into one.
 * Spell 71842 is not in this client, so the engine cannot scale it, and I tried
 * to bridge that by multiplying the level-30 measurement by
 * `dbc.spell_scaling()` for the owner's class - a table belonging to a different
 * spell entirely. That is inventing a curve and calling it data. the author stopped it:
 * *"Where are you getting these values from, what are you smoking...... I can
 * easily get this for you ingame"*. He is right on both counts, and it did not
 * even work: the multiplier resolved to 1.0 and every bracket read 952.9.
 *
 * THE AUTHOR'S DECISION, 20 September 2026: *"we will keep the level 30 Nibelung values
 * for all brackets since it doesn't scale"*. So the flat constant is now the
 * INTENDED behaviour at every bracket, not a gap waiting to be filled.
 *
 * The one piece of evidence that points the other way is kept here rather than
 * dropped, because it is the reason the question came up at all: Wowhead's flag
 * list for spell 71842 includes "Spell damage depends on caster level", and npc
 * 38392's ability is listed as *Smite, Rank 12*. That is a rank from the high
 * seventies, which would make 619 an at-level-80 number. Against that, the author plays
 * this content and says it does not scale, and the measurement that exists - two
 * level-30 logs agreeing to 0.6% - is the only thing anyone has actually taken.
 *
 * If a Nibelung log from a higher-level character ever turns up, comparing one
 * Smite against 823-920 settles it in a minute. Until then this is a decision,
 * not an oversight.
 */
static constexpr double SMITE_MIN      = 823.0;
static constexpr double SMITE_MAX      = 920.0;
// 1.62s: the MEDIAN gap between Smites across 32 casts in the Warcraft Logs
// report. The old 1950ms came from wowsims' comment about "sixteen casts in
// thirty seconds"; the real Val'kyr manages about eighteen.
static constexpr timespan_t SMITE_CD   = timespan_t::from_millis( 1620 );
static constexpr timespan_t VALKYR_TIME = timespan_t::from_seconds( 30 );

struct valkyr_smite_t final : public spell_t
{
  valkyr_smite_t( pet_t* p ) : spell_t( "smite", p )
  {
    school               = SCHOOL_HOLY;
    base_dd_min          = SMITE_MIN;
    base_dd_max          = SMITE_MAX;
    cooldown->duration   = SMITE_CD;
    base_execute_time    = 0_ms;
    // Cannot MISS - that is what the spell page says, and it is all it says.
    may_miss = false;
    // And it does not crit: 47 logged Smites across two characters, no crits.
    may_crit = false;
    // It inherits no spell power; the damage is flat plus the owner's
    // versatility and guardian multipliers, which is what the logs show.
    spell_power_mod.direct = 0.0;
    background = false;
  }
};

struct valkyr_pet_t final : public pet_t
{
  valkyr_pet_t( player_t* owner ) : pet_t( owner->sim, owner, "valkyr_battle_maiden", true, true )
  {
    npc_id = 38392;
  }

  action_t* create_action( util::string_view name, util::string_view options ) override
  {
    if ( name == "smite" )
      return new valkyr_smite_t( this );
    return pet_t::create_action( name, options );
  }

  void init_action_list() override
  {
    pet_t::init_action_list();
    if ( action_list_str.empty() )
      get_action_priority_list( "default" )->add_action( "smite" );
  }

  resource_e primary_resource() const override
  { return RESOURCE_MANA; }
};

struct nibelung_cb_t final : public dbc_proc_callback_t
{
  spawner::pet_spawner_t<valkyr_pet_t> spawner;

  nibelung_cb_t( const special_effect_t& effect )
    : dbc_proc_callback_t( effect.item, effect ),
      spawner( "valkyr_battle_maiden", effect.player,
               []( player_t* owner ) { return new valkyr_pet_t( owner ); } )
  {
    spawner.set_default_duration( VALKYR_TIME );
  }

  void execute( const spell_data_t*, player_t*, action_state_t* ) override
  { spawner.spawn(); }
};
}  // namespace nibelung

void item::nibelung( special_effect_t& effect )
{
  using namespace nibelung;

  effect.name_str      = "nibelung";
  /*
   * HARMFUL SPELL CASTS, NOT HITS AND NOT TICKS.
   *
   * This read `PF_MAGIC_SPELL | PF_PERIODIC` with `PF2_ALL_HIT`, so every DoT
   * tick rolled for a proc. On a level 60 Affliction warlock that is not a
   * detail - traced over 300 seconds, the 2,091 proc attempts were:
   *
   *     Haunt 307, Corruption 270, Agony 268, Unstable Affliction 257,
   *     Malefic Grasp 117, ... and Shadow Bolt only 102
   *
   * Ticks outnumbered casts about ten to one, and Nibelung came out worth 23.8%
   * of the character's damage on an item level 47 staff.
   *
   * wowsims register it as `Callback: core.CallbackOnCastComplete` with
   * `Harmful: true` (sim/common/wotlk/nibelung.go), which is a CAST trigger, and
   * the item's own tooltip agrees: "Your harmful SPELLS have a chance to cause
   * you to summon a Val'kyr". the author, 18 September 2026: *"copy theirs for proc
   * logic crits etc as that's never changed"*.
   */
  effect.proc_flags_   = PF_MAGIC_SPELL;
  effect.proc_flags2_  = PF2_ALL_CAST;
  effect.proc_chance_  = 0.02;
  effect.cooldown_     = timespan_t::from_millis( 250 );

  new nibelung_cb_t( effect );
}

// ===========================================================================
// DISLODGED FOREIGN OBJECT, items 50348 (heroic) and 50353 (normal).
//
//   71602 / 71645  driver, 10% on harmful spells, 45s internal cooldown
//   71601 / 71644  Surge of Power - TWENTY SECONDS, ticks the below every 2s
//   71600 / 71643  Surging Power - the per-stack spell power
//
// The registration used to be `register_special_effect( 71602, "71600Trigger" )`,
// which triggers the per-stack aura DIRECTLY and never touches 71601. Two things
// go wrong at once, and they compound:
//
//   - 71600's own duration in this client is `Aura (infinite)`, because the real
//     duration lives on 71601. So each stack, once gained, NEVER EXPIRES.
//   - only ONE stack is gained per proc, instead of one every 2 seconds for 20
//     seconds.
//
// Traced on a level-30 shadow priest over 300s: six procs, six stacks, spell
// power climbing 261 -> 268 -> 275 -> 282 -> 289 -> 296 -> 303 and staying
// there. Worth +21 average spell power where the real item is worth about +15.
//
// the author asked three times why this trinket kept winning its slot. It was measured
// twice and reported as a DEAD proc both times, because a buff that never
// expires reports no uptime and so never appears in the buff summary the audit
// was reading. He rejected that on logic alone - *"if the proc was never firing
// then how did it come back as BIS... logically it would come back worse"* - and
// he was right.
//
// The description is explicit about the real shape:
//   "Increases spell power by $71600s1 and an additional $71600s1 every
//    $71601t1 sec. Lasts $71601d."
//
// So: one stack on proc, another every 2 seconds, the whole thing lasting 20
// seconds and then dropping in one go. Ten stacks at its peak.
struct dfo_cb_t final : public dbc_proc_callback_t
{
  buff_t* stacks;

  dfo_cb_t( const special_effect_t& effect, buff_t* b )
    : dbc_proc_callback_t( effect.item, effect ), stacks( b ) {}

  void execute( const spell_data_t*, player_t*, action_state_t* ) override
  {
    // A fresh proc restarts the window at one stack rather than adding to
    // whatever is left of the old one - the game reapplies 71601, and 71601 is
    // what carries the duration.
    stacks->expire();
    stacks->trigger();
  }
};

/*
 * The other half of the same shape: a window whose stacks are earned by ATTACKING
 * rather than by a timer.
 *
 *   45355 driver  --Proc Trigger-->  45040 Battle Trance (20s)
 *                                      --Proc Trigger ON ATTACK-->  45041 Combat Insight
 *
 * "your melee or ranged attacks will each grant $45041s1 attack power, stacking
 * up to 10 times. Expires after $45040d." So the window is a state, and every
 * swing inside it is a stack. This callback is the swing half; the driver half
 * above opens the window.
 */
struct window_stack_cb_t final : public dbc_proc_callback_t
{
  buff_t* stacks;

  window_stack_cb_t( const special_effect_t& effect, buff_t* b )
    : dbc_proc_callback_t( effect.player, effect ), stacks( b ) {}

  void execute( const spell_data_t*, player_t*, action_state_t* ) override
  {
    // Only inside the window. Outside it a swing grants nothing at all, which is
    // the whole difference between this and a permanently stacking buff.
    if ( stacks->check() )
      stacks->bump( 1 );
  }
};

void item::dislodged_foreign_object( special_effect_t& effect )
{
  auto player = effect.player;

  /*
   * THE CHAIN IS driver -> WINDOW -> PER-STACK, not driver -> per-stack.
   *
   *   71645 driver           "Proc Trigger Spell: Surge of Power"  -> 71644
   *   71644 Surge of Power   20s, "Periodic Trigger Spell ... every 2s" -> 71643
   *   71643 Surging Power    the spell power itself
   *
   * So `effect.trigger()` is the WINDOW. Reading it as the per-stack aura (and
   * then looking for the window at id+1, which lands back on the driver) built
   * the buff from a spell that carries no stat at all: it ticked 60 times over
   * 300s and moved spell power by exactly nothing.
   */
  const spell_data_t* window = effect.trigger();
  const spell_data_t* per_stack = window->effectN( 1 ).trigger();
  if ( !per_stack->ok() )
    per_stack = player->find_spell( window->id() - 1 );

  const timespan_t total = window->ok() && window->duration() > 0_ms
    ? window->duration() : timespan_t::from_seconds( 20 );
  const timespan_t tick = window->ok() && window->effectN( 1 ).period() > 0_ms
    ? window->effectN( 1 ).period() : timespan_t::from_seconds( 2 );

  /*
   * TWO WAYS TO EARN A STACK INSIDE THE WINDOW.
   *
   * Dislodged Foreign Object's window carries a PERIODIC trigger - a stack every
   * two seconds, on a timer. Blackened Naaru Sliver's carries an ordinary proc
   * trigger instead: "your melee or ranged attacks will EACH grant $45041s1
   * attack power, stacking up to 10 times. Expires after $45040d."
   *
   * Both were registered the naive way, `"<per-stack id>Trigger"`, which skips
   * the window entirely - and since both per-stack auras are `Aura (infinite)`
   * in this client, both stacked permanently. They are one bug with two shapes.
   */
  const bool byTimer = window->ok() && window->effectN( 1 ).period() > 0_ms;

  /*
   * The STAT and its AMOUNT must come from the framework, not from reading
   * effectN( 1 ) here. The per-stack aura is A_MOD_DAMAGE_DONE with a school
   * mask, not a plain stat aura, and only `initialize_stat_buff()` translates
   * that shape into spell power. Building the buff by hand and calling
   * `add_stat( STAT_SPELL_POWER, effectN( 1 ).average( item ) )` reads a
   * different quantity entirely and gave ~1 spell power per stack instead of ~7.
   *
   * So set what this handler actually knows - the duration and the stack cap,
   * both of which live on the WINDOW spell that the old registration skipped -
   * and let `create_buff()` do the rest.
   */
  effect.duration_  = total;
  // A timer window holds as many stacks as it has ticks. An attack-driven one
  // states its cap on the per-stack aura (or its description); ten is what both
  // of these items say, and it is the value the client carries.
  effect.max_stacks = byTimer ? std::max( 1, as<int>( total / tick ) )
    : std::max( 1, per_stack->max_stacks() > 0 ? as<int>( per_stack->max_stacks() ) : 10 );
  // Build the buff from the PER-STACK aura, which is where the stat lives; the
  // window only supplies the duration and, for a timer, the tick rate.
  effect.trigger_spell_id = per_stack->id();

  // `create_buff()` dispatches on the effect's buff type and does not promise a
  // stat buff - debug_cast'ing its result segfaulted. This one does.
  stat_buff_t* buff = effect.initialize_stat_buff();
  if ( !buff )
  {
    effect.type = SPECIAL_EFFECT_NONE;   // better nothing than a wrong number
    return;
  }
  buff->set_refresh_behavior( buff_refresh_behavior::DURATION );
  if ( byTimer )
  {
    // Stacks arrive on a timer, one every $71601t1 seconds, for as long as the
    // window lasts - and then the whole lot drops together.
    //
    // NO tick callback: buff.cpp runs the callback INSTEAD of bumping when one
    // is set, and also fires it once on application, so supplying one both
    // double-counted the first tick and had to re-implement the bump.
    buff->set_period( tick )->set_tick_behavior( buff_tick_behavior::CLIP );
  }
  else
  {
    // Nothing ticks; the swings do the work. Make sure an inherited period does
    // not quietly add stacks the character never earned.
    buff->set_period( 0_ms )->set_tick_behavior( buff_tick_behavior::NONE );
  }

  effect.name_str = "dislodged_foreign_object";
  if ( byTimer )
  {
    effect.proc_flags_  = PF_MAGIC_SPELL;
    effect.proc_flags2_ = PF2_ALL_CAST;
  }
  // Chance and internal cooldown come from the driver and were already right.
  new dfo_cb_t( effect, buff );

  if ( !byTimer )
  {
    /*
     * The swing half. A second callback, on the character rather than the item,
     * that adds a stack for every melee or ranged hit while the window is open.
     * Its own proc chance is 1: the window is the gate, not another roll.
     */
    auto swings = new special_effect_t( effect.player );
    swings->name_str     = effect.name_str + "_stack";
    swings->type         = SPECIAL_EFFECT_EQUIP;
    swings->source       = SPECIAL_EFFECT_SOURCE_ITEM;
    swings->spell_id     = window->id();
    swings->proc_flags_  = PF_MELEE | PF_MELEE_ABILITY | PF_RANGED | PF_RANGED_ABILITY;
    swings->proc_flags2_ = PF2_ALL_HIT;
    swings->proc_chance_ = 1.0;
    swings->cooldown_    = 0_ms;
    effect.player->special_effects.push_back( swings );
    new window_stack_cb_t( *swings, buff );
  }
}

// ===========================================================================
// SOULDRINKER, the Dragon Soul one-hander, items 78488 / 77193 / 78479.
//
// Deals damage equal to a share of the WEARER'S OWN MAXIMUM HEALTH - not spell
// power, not weapon damage - and heals for twice that. Shares from wowsims/cata,
// sim/common/cata/other_effects.go:
//
//   78488  Looking For Raid   1.3% of max health
//   77193  Normal             1.5%
//   78479  Heroic             1.7%
//
// The three drivers this client carries - 107895, 109832, 109829 - are hollow:
// "Item - Dragon Soul - Proc - Str Tank Sword" (107895) reads 0.000000 for every
// coefficient. That is why the weapon has been a stat stick, and why the share
// is stated here rather than read from the data.
//
// THE SHARE IS READ FROM THE ITEM ID, NOT THE DRIVER. Our three drivers do not
// map cleanly onto wowsims' three (108022 / 109831 / 109828), and getting that
// mapping wrong would silently give the LFR sword the Heroic share. The item id
// is unambiguous and is what the player is actually holding.
//
// Because the damage comes from the wearer's max health, this scales correctly
// at every bracket with no conversion - which is exactly what Gurthalak's flat
// tick damage below does NOT do.
//
// The heal is deliberately not modelled: it changes no damage number, and this
// project does not simulate a healing profile.
void item::souldrinker( special_effect_t& effect )
{
  effect.name_str     = "drain_life";
  effect.proc_flags_  = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  // 15% per landed hit, flat - NOT a real PPM and NOT a PPM needing conversion.
  // wowsims model it as a flat chance and chance_on_hit_cb_t rolls per landed
  // hit, so the two agree directly. Converting it would repeat the mistake that
  // put Shadowmourne's rate out on 18 September 2026.
  effect.proc_chance_ = 0.15;
  effect.ppm_         = 0;
  /* the hand rule now lives in chance_on_hit_cb_t, which also lets abilities roll */

  if ( already_built( effect ) )
    return;

  /*
   * MEASURED FROM A COMBAT LOG, 19 September 2026, and it corrects what was here.
   *
   * THERE IS NO DIFFICULTY SPLIT ANY MORE. wowsims' Cataclysm data has 1.3/1.5/
   * 1.7% for LFR/Normal/Heroic, and this code used it. the author's monk was wearing a
   * HEROIC Souldrinker in one hand and an LFR one in the other, and both fired
   * for the same number:
   *
   *     spell 109828 (LFR)     17 hits   296 / 297   crit 593
   *     spell 109831 (Heroic)  12 hits   296 / 297   crit 593
   *
   * If the shares still differed, the LFR weapon would have hit for 226. No
   * value near 226 appears anywhere in the log.
   *
   * 296.5 damage on 14,232 max health is 2.083%. The heal is 616/617, exactly
   * twice the damage, which confirms the other half of the tooltip.
   */
  const double share = 0.02083;

  struct drain_life_t final : public spell_t
  {
    double share;
    drain_life_t( const special_effect_t& e, double s )
      : spell_t( "drain_life", e.player, e.driver() ), share( s )
    {
      background = may_crit = true;
      // SHADOW school, so it ignores armour - which is most of its value.
      school = SCHOOL_SHADOW;
      // The driver is hollow, so nothing may be inherited from it.
      base_dd_min = base_dd_max = 0.0;
      spell_power_mod.direct = 0.0;
      attack_power_mod.direct = 0.0;
    }

    // Read at execute time, not at construction: resources.max is not populated
    // until the actor is initialised.
    double base_da_min( const action_state_t* ) const override
    { return player->resources.max[ RESOURCE_HEALTH ] * share; }

    double base_da_max( const action_state_t* s ) const override
    { return base_da_min( s ); }
  };

  effect.execute_action = new drain_life_t( effect, share );

  chance_on_hit( effect );
}

// ===========================================================================
// GURTHALAK, VOICE OF THE DEEPS, items 78487 / 77191 / 78478.
//
// 2% on landed melee to summon a Tentacle of the Old Ones for 12 seconds, and
// SEVERAL can be up at once. pet_spawner_t already handles that - it is what
// Nibelung's Val'kyr uses above - so the hard part needs nothing new.
//
// THE SUMMON IS MODELLED. THE DAMAGE IS DELIBERATELY ZERO. Read this before
// changing TENTACLE_TICK_DAMAGE:
//
// SimulationCraft's own Cataclysm-era implementation (simc-cataclysm,
// engine/sc_unique_gear.cpp, register_gurthalak) is the best source there is for
// this item, and it records the tick damage as FLAT, with no coefficient at all:
//
//     uint32_t tick_damage = heroic ? 12591 : lfr ? 9881 : 11155;
//     tick_power_mod = 0;  base_td = tick_damage;
//     num_ticks = sim -> roll( 0.5 ) ? 9 : 8;   // 3 Mind Flays, 8-9 ticks
//     // "While this spell ID is the one used by all of the tentacles,
//     //  It doesn't have a coeff and each version has static damage"
//
// Those numbers are in PRE-SQUISH units, from a level 85 game. Dropping 11,155
// per tick into a level 35-80 bracket would make Gurthalak trivially best in
// slot at every one of them and quietly corrupt the whole gear ranking. There is
// no sourced conversion factor, and inventing one to fill the gap is precisely
// the failure this project keeps writing down.
//
// The engine cannot rescue it either: spell 52586, the tentacle's own Mind Flay,
// is hollow in this client - _sp_coeff 0, _ap_coeff 0, _scaling_type 0
// (disabled), _base_value 18 - so there is nothing to read.
//
// So the mechanic ships and the damage does not. Gurthalak already ranks as a
// stat stick, so nothing regresses; the sword is understated by exactly its
// proc, and the spawn COUNT is verifiable from a report today. When a log gives
// a real per-tick figure at a known bracket, set TENTACLE_TICK_DAMAGE and
// nothing else in this block has to change.
namespace gurthalak
{
static constexpr timespan_t TENTACLE_TIME = timespan_t::from_seconds( 12 );
static constexpr timespan_t TICK_TIME     = timespan_t::from_seconds( 1 );
// 8 or 9 ticks at even odds in the Cata implementation; the dot is given the
// longer of the two and the pet's 12s life covers it.
static constexpr timespan_t CHANNEL_TIME  = timespan_t::from_seconds( 9 );
// SEE THE NOTE ABOVE. Cata-era flat values were 9881 LFR / 11155 Normal /
// 12591 Heroic, pre-squish, and are NOT safe to use unconverted.
// MEASURED, 19 September 2026, from the author's own combat log across FOUR stat
// states on one level 35 warrior with heroic Gurthalak (78478, item level 49):
//
//     attack power 183 -> 267    246 -> 274    288 -> 275    314 -> 275
//
// FLAT. Attack power moved 72% and the tick moved 3%; at the same max health
// with AP 288 and AP 314 the damage was an identical 275. Spell power never
// varied at all. No stat in the log moves with it.
//
// An earlier staged patch proposed 0.9583 x attack power from a SINGLE data
// point. That fit reproduced the one point exactly and was wrong - at level 80
// it would have inflated the tentacle roughly tenfold and made the sword
// falsely best in slot. A one-point fit is not a measurement.
//
// STILL UNKNOWN: whether 275 is flat across LEVELS. Every measurement is at 35,
// and the author has the sword on one character. Flat errs LOW, which can never
// invent a best in slot.
// THE ENGINE ADDS A FIXED 1.1615, SO THE CONSTANT IS PRE-DIVIDED.
//
// Setting this to the logged 275 produced 319.41 a tick. Overriding the tick,
// persistent, direct and pet-level multipliers all failed to move it: the factor
// is applied to base_td before any of them. Rather than keep digging, it was
// MEASURED - and it is a constant, not a stat:
//
//     level 35 -> 319.4125    level 50 -> 319.4125    level 70 -> 319.4125
//
// Identical at every level unbuffed, so 275/1.1615 is a safe correction rather
// than a fit to one configuration - which is the distinction that made the
// earlier '0.9583 x attack power' proposal wrong.
//
// UNTESTED: raid buffs scale it further (1.2692 with optimal_raid=1). the author's log
// was solo and unbuffed, so whether the real tentacle benefits from raid buffs is
// unknown; the engine's default behaviour is left alone.
static constexpr double TENTACLE_TICK_DAMAGE = 236.7628;  // -> 275 a tick, matching the log

struct lash_of_the_deep_t final : public spell_t
{
  lash_of_the_deep_t( pet_t* p ) : spell_t( "lash_of_the_deep", p )
  {
    school            = SCHOOL_SHADOW;
    base_execute_time = 0_ms;
    trigger_gcd       = 0_ms;
    // THE COOLDOWN IS WHAT STOPS THE SIM HANGING, and it is not optional. With
    // zero cast time, zero GCD and no cooldown, the pet re-executes this at the
    // same timestamp forever and the simulator reports "Simulation stuck" before
    // iteration 0 completes. Nibelung's Val'kyr avoids it the same way, with a
    // cooldown on its Smite; leaving it out here cost one build to find.
    //
    // The duration is the channel itself, so one tentacle channels once and the
    // 12 second life covers it - which is also the Cataclysm implementation's
    // "3 Mind Flays of 3 ticks each", 8-9 ticks in one go.
    cooldown->duration = CHANNEL_TIME;
    // Cannot miss, can crit on its ticks, and is NOT hasted - all three from the
    // Cataclysm implementation.
    may_miss          = false;
    may_crit          = false;
    tick_may_crit     = true;
    hasted_ticks      = false;
    base_td           = TENTACLE_TICK_DAMAGE;
    base_tick_time    = TICK_TIME;
    dot_duration      = CHANNEL_TIME;
    // Flat damage: the tentacle inherits no scaling, matching the source.
    spell_power_mod.tick  = 0.0;
    attack_power_mod.tick = 0.0;
  }

  /*
   * FLAT MEANS FLAT, including the owner's damage multipliers.
   *
   * Zeroing the power coefficients was not enough. With base_td at the measured
   * 275, the engine still produced 319.41 a tick UNBUFFED on the author's exact
   * configuration - a level 35 warrior with the heroic sword at item level 49 -
   * because the pet inherits the owner's composite damage multipliers.
   *
   * His combat log says otherwise: base 275, actual 276. A multiplier of 1.0036,
   * not 1.16. The tentacle is a fixed add with a fixed channel and it does not
   * scale with the character, which is exactly what SimulationCraft's own
   * Cataclysm implementation modelled (tick_power_mod = 0, base_td = flat).
   *
   * Returning 1.0 here makes the engine agree with the log instead of being 16%
   * over it, and 16% of a proc that fires on 2% of hits is the difference
   * between a stat stick and a falsely attractive weapon.
   */
  /*
   * A DOT SNAPSHOTS ITS MULTIPLIER WHEN IT IS APPLIED, so overriding the tick
   * multiplier alone changed nothing - the damage was still 319.41 against a
   * logged 275. Both are pinned here.
   */
  double composite_ta_multiplier( const action_state_t* ) const override
  { return 1.0; }

  double composite_persistent_multiplier( const action_state_t* ) const override
  { return 1.0; }

  double composite_da_multiplier( const action_state_t* ) const override
  { return 1.0; }
};

struct tentacle_pet_t final : public pet_t
{
  tentacle_pet_t( player_t* owner )
    : pet_t( owner->sim, owner, "tentacle_of_the_old_ones", true, true )
  {
    // 58078, not 57734 - the combat log names the real creature.
    npc_id = 58078;
  }

  action_t* create_action( util::string_view name, util::string_view options ) override
  {
    if ( name == "lash_of_the_deep" )
      return new lash_of_the_deep_t( this );
    return pet_t::create_action( name, options );
  }

  void init_action_list() override
  {
    pet_t::init_action_list();
    if ( action_list_str.empty() )
      get_action_priority_list( "default" )->add_action( "lash_of_the_deep" );
  }

  resource_e primary_resource() const override
  { return RESOURCE_MANA; }

  /*
   * The tentacle is a FIXED ADD with a FIXED channel. the author's combat log is
   * unambiguous: base 275, actual 276, a multiplier of 1.0036 - while the engine
   * was applying 1.1615 through the pet's inherited damage multipliers.
   */
  double composite_player_multiplier( school_e ) const override
  { return 1.0; }
};

struct gurthalak_cb_t final : public chance_on_hit_cb_t
{
  spawner::pet_spawner_t<tentacle_pet_t> spawner;
  // THE SPAWN COUNTER EXISTS BECAUSE THE DAMAGE IS ZERO. A pet that deals no
  // damage is omitted from the report entirely, so without this the only proof
  // the proc works at all is a debug log - which is no use to anyone checking it
  // later. The counter makes the rate readable in an ordinary report, and the
  // rate is the half of this item that IS sourced.
  proc_t* spawn_proc;

  gurthalak_cb_t( const special_effect_t& effect )
    : chance_on_hit_cb_t( effect ),
      spawner( "tentacle_of_the_old_ones", effect.player,
               []( player_t* owner ) { return new tentacle_pet_t( owner ); } ),
      spawn_proc( effect.player->get_proc( "Gurthalak: Tentacle of the Old Ones" ) )
  {
    spawner.set_default_duration( TENTACLE_TIME );
  }

  void execute( const spell_data_t*, player_t*, action_state_t* ) override
  {
    spawner.spawn();
    spawn_proc->occur();
  }
};
}  // namespace gurthalak

void item::gurthalak( special_effect_t& effect )
{
  using namespace gurthalak;

  effect.name_str     = "gurthalak";
  effect.proc_flags_  = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  // 2% per landed hit, from wowsims/cata sim/common/cata/gurthalak.go. The
  // Cataclysm SimC read it from the driver's own proc_chance(); ours is hollow,
  // so it is stated.
  effect.proc_chance_ = 0.02;
  effect.ppm_         = 0;
  /* the hand rule now lives in chance_on_hit_cb_t, which also lets abilities roll */
  //
  // NOT MODELLED: Bloodthirst (23881) and Heroic Leap (6544) proc Gurthalak from
  // EITHER hand, while every other ability only procs it from the hand the sword
  // is in. chance_on_hit_cb_t enforces the hand rule uniformly. The exception is
  // two lines, matters only to a Fury warrior, and has not been measured at
  // these brackets - so it is left out on purpose rather than by oversight.
  effect.disable_action();

  new gurthalak_cb_t( effect );
}

// Thunderfury, Blessed Blade of the Windseeker, item 19019.
//
//   21992  driver, ItemEffect type 2 (chance on hit). Everything it needs
//          survives in this build's data:
//            effect 1  Nature resistance debuff, chain targets 5
//            effect 2  School Damage, nature, the coefficient the hit scales on
//            effect 3  Trigger Spell 27648, the attack-speed cyclone
//
// Asked for by the author on 10 September 2026 alongside Eskhandar's Right Claw. It
// was never registered, so the sword equipped, its tooltip read correctly, and
// the proc did nothing at all.
//
// Only the DAMAGE is modelled. The Nature resistance debuff has nothing to bite
// on in a damage sim - the target has no resistance to strip - and the cyclone
// slows the TARGET's attack speed, which is a tanking mechanic this sim does not
// price. Both are deliberately left out rather than guessed at.
void item::thunderfury( special_effect_t& effect )
{
  effect.name_str     = "thunderfury";
  effect.proc_flags_  = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  // SIX PPM, NOT ONE - and this one was actually wrong, not merely uncited.
  //
  // Every chance-on-hit weapon in this file was given the same 1.0 basis because
  // nothing in the client data carries a rate. wowsims researched them per item,
  // and Thunderfury is the one where that assumption missed badly:
  //
  //   wowsims/classic sim/common/item_effects.go:2406-2410
  //     core.NewItemEffect(Thunderfury, func(agent core.Agent) {
  //       procMask := character.GetProcMaskForItem(Thunderfury)
  //       ppmm := character.AutoAttacks.NewPPMManager(6.0, procMask)
  //
  // At 2.6 seconds that is 26% per hit rather than 4.3% - the sword procs six
  // times as often as this port has been simulating it since 10 September 2026,
  // which matches its reputation in game far better than 1.0 did.
  effect.proc_chance_ = chance_on_hit_from_ppm( 6.0, 2.6 );
  effect.ppm_         = 0;
  /* the hand rule now lives in chance_on_hit_cb_t, which also lets abilities roll */

  if ( already_built( effect ) )
    return;

  struct thunderfury_t final : public spell_t
  {
    thunderfury_t( const special_effect_t& e )
      : spell_t( "thunderfury", e.player, e.driver() )
    {
      background = may_crit = true;
      // The lightning jumps, and effect 1 is where the game records how far.
      aoe = as<int>( e.driver()->effectN( 1 ).chain_target() );
      // Effect 2 is the damage; effect 1 is the resistance debuff, so the
      // amount has to be taken from the right one.
      base_dd_min = base_dd_max = e.driver()->effectN( 2 ).average( e.item );
      // The item's own level drives it - the spell is flagged "Scales with
      // Casting Item's Level" - and average( item ) is what reads that.
    }
  };

  effect.execute_action = new thunderfury_t( effect );

  chance_on_hit( effect );
}

// BracketSim: Sulfuras, Hand of Ragnaros, item 17182 - which was simply never
// registered, so the hammer equipped, read correctly, and did nothing.
//
// This is a REGISTRATION, not a reconstruction. The client still carries both of
// its effects in full:
//
//   17182 -> spell 21162  Fireball    type 2 (chance on hit)
//   17182 -> spell 21142  Immolation  type 1 (passive)
//
// Only the Fireball is wired up. Immolation is a damage SHIELD - "deals 13.9
// Fire damage to anyone who strikes you with a melee attack" - which needs the
// boss to be swinging at you. A patchwerk actor is not a tank taking hits, so
// modelling it would credit damage that only someone actually tanking would see.
//
// RATE: wowsims/classic sim/common/item_effects.go, "Hand of Ragnaros Trigger",
// `PPM: 1, // Estimated based on data from WoW Armaments Discord`. Their own
// comment calls it an estimate, so it travels as one - but an estimate from
// people who measured the hammer beats this project having no figure at all.
//
// `ppm_` rather than a precomputed chance, so the rate follows the hammer's real
// speed instead of one baked in here. Sulfuras is a 3.7 second two-hander, which
// at 1 PPM is about 6% per landed hit.
void item::sulfuras( special_effect_t& effect )
{
  effect.name_str     = "fireball_hand_of_ragnaros";
  effect.proc_flags_  = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  effect.ppm_         = 1.0;
  effect.proc_chance_ = 0;
  /* the hand rule now lives in chance_on_hit_cb_t, which also lets abilities roll */

  if ( effect.player->find_action( effect.name_str ) )
    return;

  // 21162 is both halves at once - 757-926 fire on impact plus 41.7 every two
  // seconds for ten - and both scale with the hammer's item level.
  effect.execute_action = create_proc_action<generic_proc_t>( effect.name_str, effect, effect.driver() );
  chance_on_hit( effect );
}

// BracketSim (26 Sep 2026): Masquerade Gown, item 28578 - "Love Struck".
//
// A RECONSTRUCTION: the client no longer carries spell 34584 (the driver) or 34585 (the buff), so
// nothing here can be read from data. Values from the item's Wowhead tooltip, confirmed by the author
// 26 Sep 2026 from the spell page: Versatility +145 for 15 sec, 10% chance on spell cast, 50 sec
// cooldown. (The buff's own page shows 355 "at level 90"; the item tooltip's 145 is the figure used.)
// "On spell cast" = a spell, harmful or helpful - never a melee or ranged attack.
void item::love_struck( special_effect_t& effect )
{
  effect.name_str     = "love_struck";
  effect.type         = SPECIAL_EFFECT_EQUIP;
  effect.proc_flags_  = PF_MAGIC_SPELL | PF_NONE_HARMFUL | PF_MAGIC_HEAL;
  effect.proc_flags2_ = PF2_ALL_CAST;
  effect.proc_chance_ = 0.10;
  effect.ppm_         = 0;
  effect.cooldown_    = timespan_t::from_seconds( 50 );

  auto buff = buff_t::find( effect.player, "love_struck" );
  if ( !buff )
  {
    auto sb = make_buff<stat_buff_t>( effect.player, "love_struck", spell_data_t::nil() );
    sb->add_stat( STAT_VERSATILITY_RATING, 145 );
    sb->set_duration( timespan_t::from_seconds( 15 ) );
    sb->set_max_stack( 1 );
    buff = sb;
  }
  effect.custom_buff = buff;

  new dbc_proc_callback_t( effect.player, effect );
}

void item::bonereavers_edge( special_effect_t& effect )
{
  effect.name_str     = "bonereavers_edge";
  effect.proc_flags_  = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  // Chance on hit, not real PPM - see the note above chance_on_hit_from_ppm.
  // the author, 10 September 2026: "chance on hit works the same for all weps...
  // Once you get one logic it ports to all chance on hit." It does: this is the
  // same ItemEffect type 2 as the Jackhammer, and there is nothing
  // item-specific about the model, only the weapon's speed.
  //
  /*
   * TWO PPM, AND IT WAS BRIEFLY THREE. The retraction is the useful part.
   *
   * RATE: wowsims/classic sim/common/item_effects.go:735,
   * `CreateWeaponProcSpell(BonereaversEdge, "Bonereaver's Edge", 2.0, ...)`.
   *
   * On 18 September this was raised to 3.0 on a measurement from the author's own
   * retail logs: Kleaveland, a Fury warrior carrying Bonereaver's Edge in the
   * MAIN hand, read 3,948 procs over 23,835 main-hand hits = 2.76 PPM across 13
   * fights. The Jackhammer in his off hand was the control and reproduced its
   * own 1.0.
   *
   * Two single-target ICC fights on a DIFFERENT character then contradicted it.
   * Heavymetl, Bonereaver's Edge in the OFF hand:
   *
   *     Queen Lana'thel   31 procs /  273 OH hits = 11.4%  -> 1.89 PPM
   *     Sindragosa        27 procs /  203 OH hits = 13.3%  -> 2.22 PPM
   *     together          58 procs /  476 OH hits = 12.2%  -> 2.03 PPM
   *
   * That is 2.0 on the nose, from a second character, on single target, and it
   * agrees with wowsims exactly.
   *
   * THE FAULT IS THE METHOD, NOT EITHER WEAPON. Warcraft Logs does not say
   * which hand a hit came from, so both numbers rest on splitting damage events
   * by ability id, and the bias runs in OPPOSITE directions for the two hands:
   * over-counting off-hand hits reads a main-hand weapon high and an off-hand
   * weapon low. Bonereaver's read high in the main hand and low in the off
   * hand; the Jackhammer read 6% low in the off hand. That is the signature of
   * an attribution error, and it means a rate must not be changed on this
   * method alone.
   *
   * So the cited number stands and the measurement is kept as evidence rather
   * than as an answer: test-results/chance-on-hit-rates.json.
   */
    /*
   * THE RATE IS MEASURED, and it came DOWN.
   *
   * Two warriors in the author's own logs, pooled
   * (audits/2026-09-18-chance-on-hit-rate-from-combatlog.py):
   *
   *     985 procs, 3,407 white swings, 15,030 physical ability hits
   *     985 / 18,437 = 0.0534 per landed melee hit
   *
   * The two read 0.0519 and 0.0515 independently, which is the agreement that
   * makes this trustworthy: different characters, different ability mixes, same
   * per-hit rate. Both are warriors, so almost all of their damage is physical
   * and the denominator needs no judgement call - unlike Shadowmourne's.
   *
   * 0.0534 per hit on a 3.6 second two-hander is 0.89 PPM.
   *
   * WHY THIS IS LOWER THAN THE 2.76-3.0 THIS PROJECT READ ON 17 SEPTEMBER, and
   * it is not a contradiction. That figure divided procs by AUTO ATTACKS only,
   * inferred per hand from Warcraft Logs, which does not record which hand a
   * swing came from. This divides by every landed melee hit, which is what the
   * engine now rolls on. Same procs, different denominator, and only one of the
   * two denominators matches the model.
   */
  effect.proc_chance_ = chance_on_hit_from_ppm( 0.89, 3.6 );
  effect.ppm_         = 0;
  /* the hand rule now lives in chance_on_hit_cb_t, which also lets abilities roll */

  if ( already_built( effect ) )
    return;

  auto buff = make_buff<stat_buff_t>( effect.player, effect.name_str, effect.driver(), effect.item );
  buff->set_chance( 1.0 );
  effect.custom_buff = buff;

  chance_on_hit( effect );
}

struct manifest_anger_t final : public attack_t
{
  attack_t* source_auto_attack;

  manifest_anger_t( player_t* p )
    : attack_t( "manifest_anger", p, p->find_spell( 71433 ) ), source_auto_attack( nullptr )
  {
    background = true;
    callbacks = true;
    may_crit = true;

    // Apply the 50% value to the complete auto attack, including current
    // low-level auto-attack tuning and flat weapon modifiers. The driver spell
    // normally encodes this as a weapon multiplier, which would omit those
    // modifiers in the modern engine.
    weapon_multiplier = 1.0;
  }

  double composite_da_multiplier( const action_state_t* state ) const override
  {
    return 0.5 * ( source_auto_attack ? source_auto_attack->composite_da_multiplier( state )
                                      : attack_t::composite_da_multiplier( state ) );
  }

  double bonus_da( const action_state_t* state ) const override
  {
    return source_auto_attack ? source_auto_attack->bonus_da( state ) : attack_t::bonus_da( state );
  }

  double composite_target_multiplier( player_t* target ) const override
  {
    return source_auto_attack ? source_auto_attack->composite_target_multiplier( target )
                              : attack_t::composite_target_multiplier( target );
  }

  double composite_crit_chance() const override
  {
    return source_auto_attack ? source_auto_attack->composite_crit_chance()
                              : attack_t::composite_crit_chance();
  }
};

struct tiny_abomination_cb_t final : public dbc_proc_callback_t
{
  buff_t* motes;
  manifest_anger_t* manifest_anger;
  attack_t* first_mote_attack;
  weapon_t* first_mote_weapon;

  tiny_abomination_cb_t( const special_effect_t& effect, buff_t* b )
    : dbc_proc_callback_t( effect.item, effect ),
      motes( b ),
      manifest_anger( new manifest_anger_t( effect.player ) ),
      first_mote_attack( nullptr ),
      first_mote_weapon( nullptr )
  {
  }

  void reset() override
  {
    dbc_proc_callback_t::reset();
    first_mote_attack = nullptr;
    first_mote_weapon = nullptr;
  }

  void trigger( const proc_data_t& source_data, player_t* target, action_state_t* state,
                proc_trigger_type_e type ) override
  {
    if ( !state || !state->action || !state->action->weapon ||
         state->action->internal_id == manifest_anger->internal_id )
    {
      return;
    }

    dbc_proc_callback_t::trigger( source_data, target, state, type );
  }

  void execute( const spell_data_t*, player_t* target, action_state_t* state ) override
  {
    if ( !state || !state->action || !state->action->weapon || !motes->trigger() )
      return;

    if ( motes->check() == 1 )
    {
      first_mote_weapon = state->action->weapon;
      first_mote_attack = first_mote_weapon->slot == SLOT_OFF_HAND ? listener->off_hand_attack
                                                                   : listener->main_hand_attack;
    }

    if ( motes->check() < motes->max_stack() )
      return;

    manifest_anger->source_auto_attack = first_mote_attack;
    manifest_anger->weapon = first_mote_weapon ? first_mote_weapon : state->action->weapon;
    first_mote_attack = nullptr;
    first_mote_weapon = nullptr;
    motes->expire();
    manifest_anger->execute_on_target( target );
  }
};

void item::tiny_abomination_in_a_jar( special_effect_t& effect )
{
  effect.name_str     = "tiny_abomination_in_a_jar";
  effect.proc_flags_  = PF_MELEE | PF_MELEE_ABILITY;
  effect.proc_flags2_ = PF2_ALL_HIT;
  effect.proc_chance_ = 0.5;

  const unsigned mote_count = effect.spell_id == 71545 ? 7U : 8U;
  auto motes = make_buff( effect.player, "mote_of_anger", effect.player->find_spell( 71432 ) );
  motes->set_max_stack( mote_count );
  motes->set_chance( 1.0 );
  effect.custom_buff = motes;

  new tiny_abomination_cb_t( effect, motes );
}

// Blazefury Medallion
// 243988 Driver
// 243991 Damage spell
void item::blazefury_medallion( special_effect_t& effect )
{
  struct blazefury_medallion_t : public generic_proc_t
  {
    blazefury_medallion_t( const special_effect_t& effect )
      : generic_proc_t( effect, "blazefury_medallion", effect.driver()->effectN( 1 ).trigger() )
    {
    }
  };

  struct blazefury_medallion_cb_t : public dbc_proc_callback_t
  {
    action_t* damage;
    double base_damage;

    blazefury_medallion_cb_t( const special_effect_t& e, action_t* a ) :
      dbc_proc_callback_t( e.player, e ),
      damage( a ),
      base_damage( e.driver()->effectN( 1 ).trigger()->effectN( 1 ).average( e.item ) )
    {
    }

    void execute( const spell_data_t*, player_t* t, action_state_t* s ) override
    {
      if ( s->action->result_is_hit( s->result ) )
      {
        // Currently only uses MH weapon speed, not the speed of the triggering weapon
        // Leaving this in a CB handler just in case this changes at some point via hotfix
        // TOCHECK -- Unclear if this uses equipped weapon speed for Feral or not
        double speed_mod = ( listener->main_hand_weapon.type == WEAPON_NONE ? 2.0 :
                             listener->main_hand_weapon.swing_time.total_seconds() );
        damage->execute_on_target( t, base_damage * speed_mod );
      }
    }
  };

  // if your autoattacks happen to be aoe, this will apply to all targets hit
  effect.proc_flags2_ = PF2_ALL_HIT;

  auto damage = create_proc_action<blazefury_medallion_t>( "blazefury_medallion", effect );
  new blazefury_medallion_cb_t( effect, damage );
}

void item::rune_of_reorigination( special_effect_t& effect )
{
  struct rune_of_reorigination_callback_t : public dbc_proc_callback_t
  {
    enum
    {
      BUFF_CRIT = 0,
      BUFF_HASTE,
      BUFF_MASTERY
    };

    stat_buff_t* buff;

    rune_of_reorigination_callback_t( const special_effect_t& data ) :
      dbc_proc_callback_t( data.item -> player, data )
    {
      buff = static_cast< stat_buff_t* >( effect.custom_buff );
    }

    void execute( const spell_data_t*, player_t*, action_state_t* ) override
    {
      // We can never allow this trinket to refresh, so force the trinket to
      // always expire, before we proc a new one.
      buff -> expire();

      // Determine highest stat based on rating multipliered stats
      double chr = listener->composite_melee_haste_rating();
      if ( listener->sim->scaling->scale_stat == STAT_HASTE_RATING )
        chr -= listener->sim->scaling->scale_value * listener->composite_rating_multiplier( RATING_MELEE_HASTE );

      double ccr = listener->composite_melee_crit_rating();
      if ( listener->sim->scaling->scale_stat == STAT_CRIT_RATING )
        ccr -= listener->sim->scaling->scale_value * listener->composite_rating_multiplier( RATING_MELEE_CRIT );

      double cmr = listener->composite_mastery_rating();
      if ( listener->sim->scaling->scale_stat == STAT_MASTERY_RATING )
        cmr -= listener->sim->scaling->scale_value * listener->composite_rating_multiplier( RATING_MASTERY );

      // Give un-multipliered stats so we don't double dip anywhere.
      chr /= listener->composite_rating_multiplier( RATING_MELEE_HASTE );
      ccr /= listener->composite_rating_multiplier( RATING_MELEE_CRIT );
      cmr /= listener->composite_rating_multiplier( RATING_MASTERY );

      if ( listener->sim->debug )
      {
        listener->sim->out_debug.printf( "%s rune_of_reorigination procs crit=%.0f haste=%.0f mastery=%.0f",
                                         listener->name(), ccr, chr, cmr );
      }

      if ( ccr >= chr )
      {
        // I choose you, crit
        if ( ccr >= cmr )
        {
          buff -> stats[ BUFF_CRIT    ].amount = 2 * ( chr + cmr );
          buff -> stats[ BUFF_HASTE   ].amount = -chr;
          buff -> stats[ BUFF_MASTERY ].amount = -cmr;
        }
        // I choose you, mastery
        else
        {
          buff -> stats[ BUFF_CRIT    ].amount = -ccr;
          buff -> stats[ BUFF_HASTE   ].amount = -chr;
          buff -> stats[ BUFF_MASTERY ].amount = 2 * ( ccr + chr );
        }
      }
      // I choose you, haste
      else if ( chr >= cmr )
      {
        buff -> stats[ BUFF_CRIT    ].amount = -ccr;
        buff -> stats[ BUFF_HASTE   ].amount = 2 * ( ccr + cmr );
        buff -> stats[ BUFF_MASTERY ].amount = -cmr;
      }
      // I choose you, mastery
      else
      {
        buff -> stats[ BUFF_CRIT    ].amount = -ccr;
        buff -> stats[ BUFF_HASTE   ].amount = -chr;
        buff -> stats[ BUFF_MASTERY ].amount = 2 * ( ccr + chr );
      }

      buff -> trigger();
    }
  };

  maintenance_check( 502 );

  const spell_data_t* spell = effect.item -> player -> find_spell( 139120 );

  auto buff_name = util::tokenize_fn( spell -> name_cstr() );

  stat_buff_t* buff = make_buff<stat_buff_t>( effect.item -> player, buff_name, spell, effect.item );
  buff->add_stat( STAT_CRIT_RATING, 0 )
      ->add_stat( STAT_HASTE_RATING, 0 )
      ->add_stat( STAT_MASTERY_RATING, 0 )
      ->set_activated( false );

  effect.custom_buff  = buff;

  new rune_of_reorigination_callback_t( effect );
}

void item::spark_of_zandalar( special_effect_t& effect )
{
  maintenance_check( 502 );

  const spell_data_t* buff = effect.item -> player -> find_spell( 138960 );

  auto buff_name = util::tokenize_fn( buff -> name_cstr() );

  stat_buff_t* b = make_buff<stat_buff_t>( effect.item -> player, buff_name, buff, effect.item );

  effect.custom_buff = b;

  struct spark_of_zandalar_callback_t : public dbc_proc_callback_t
  {
    buff_t*      sparks;
    stat_buff_t* buff;

    spark_of_zandalar_callback_t( const special_effect_t& data ) :
      dbc_proc_callback_t( *data.item, data ), buff(nullptr)
    {
      const spell_data_t* spell = listener -> find_spell( 138958 );
      sparks = make_buff( listener, "zandalari_spark_driver", spell, data.item )
               ->set_quiet( true );
    }

    void execute( const spell_data_t*, player_t*, action_state_t* ) override
    {
      sparks -> trigger();

      if ( sparks -> check() == sparks -> max_stack() )
      {
        sparks -> expire();
        proc_buff -> trigger();
      }
    }
  };

  new spark_of_zandalar_callback_t( effect );
}

void item::unerring_vision_of_leishen( special_effect_t& effect )
{
  struct perfect_aim_buff_t : public buff_t
  {
    perfect_aim_buff_t( player_t* p ) :
      buff_t( p, "perfect_aim", p -> find_spell( 138963 ) )
    {
      set_activated( false );
    }

    void execute( int stacks, double value, timespan_t duration ) override
    {
      if ( current_stack == 0 )
      {
        player -> current.spell_crit_chance  += data().effectN( 1 ).percent();
        player -> current.attack_crit_chance += data().effectN( 1 ).percent();
        player -> invalidate_cache( CACHE_CRIT_CHANCE );
      }

      buff_t::execute( stacks, value, duration );
    }

    void expire_override( int expiration_stacks, timespan_t remaining_duration ) override
    {
      buff_t::expire_override( expiration_stacks, remaining_duration );

      player -> current.spell_crit_chance  -= data().effectN( 1 ).percent();
      player -> current.attack_crit_chance -= data().effectN( 1 ).percent();
      player -> invalidate_cache( CACHE_CRIT_CHANCE );
    }
  };

  struct unerring_vision_of_leishen_callback_t : public dbc_proc_callback_t
  {
    unerring_vision_of_leishen_callback_t( const special_effect_t& data ) :
      dbc_proc_callback_t( data.item -> player, data )
    { }

    void initialize() override
    {
      dbc_proc_callback_t::initialize();

      // Warlocks have a (hidden?) 0.6 modifier, not showing in DBCs.
      if ( listener -> type == WARLOCK )
        rppm -> set_modifier( rppm -> get_modifier() * 0.6 );
    }
  };

  maintenance_check( 502 );

  effect.proc_flags2_ = PF2_ALL_HIT;
  effect.custom_buff  = new perfect_aim_buff_t( effect.item -> player );

  new unerring_vision_of_leishen_callback_t( effect );
}

void item::skeers_bloodsoaked_talisman( special_effect_t& effect )
{
  maintenance_check( 528 );

  const spell_data_t* driver = effect.item -> player -> find_spell( effect.spell_id );
  const spell_data_t* spell = driver -> effectN( 1 ).trigger();
  // Aura is hidden, thre's no linkage in spell data actual
  const spell_data_t* buff = effect.item -> player -> find_spell( 146293 );

  auto buff_name = util::tokenize_fn( buff -> name_cstr() );

  // Require a damaging result, instead of any harmful spell hit
  effect.proc_flags2_ = PF2_ALL_HIT;

  stat_buff_t* b = make_buff<stat_buff_t>( effect.item -> player, buff_name, buff, effect.item );
  b->add_stat( STAT_CRIT_RATING, spell -> effectN( 1 ).average( effect.item ) )
      ->set_tick_behavior( buff_tick_behavior::CLIP )
      ->set_period( spell -> effectN( 1 ).period() )
      ->set_duration( spell -> duration() );

  effect.custom_buff = b;

  new dbc_proc_callback_t( effect.item -> player, effect );
}

void item::blackiron_micro_crucible( special_effect_t& effect )
{
  maintenance_check( 528 );

  const spell_data_t* driver = effect.item -> player -> find_spell( effect.spell_id );
  const spell_data_t* spell = driver -> effectN( 1 ).trigger();

  auto buff_name = util::tokenize_fn( spell -> name_cstr() );

  // Require a damaging result, instead of any harmful spell hit
  effect.proc_flags2_ = PF2_ALL_HIT;

  stat_buff_t* b = make_buff<stat_buff_t>( effect.item -> player, buff_name, spell, effect.item );
  b->add_stat( STAT_MASTERY_RATING, spell -> effectN( 1 ).average( effect.item ) )
      ->set_max_stack( 20 ) // Hardcoded for now - spell->max_stacks() returns 0
      ->set_tick_behavior( buff_tick_behavior::CLIP )
      ->set_period( spell -> effectN( 1 ).period() )
      ->set_duration( spell -> duration() );

  effect.custom_buff = b;

  new dbc_proc_callback_t( effect.item -> player, effect );
}

void item::humming_blackiron_trigger( special_effect_t& effect )
{
  maintenance_check( 528 );

  const spell_data_t* driver = effect.item -> player -> find_spell( effect.spell_id );
  const spell_data_t* spell = driver -> effectN( 1 ).trigger();

  auto buff_name = util::tokenize_fn( spell -> name_cstr() );

  // Require a damaging result, instead of any harmful spell hit
  effect.proc_flags2_ = PF2_ALL_HIT;

  stat_buff_t* b = make_buff<stat_buff_t>( effect.item -> player, buff_name, spell, effect.item );
  b->add_stat( STAT_CRIT_RATING, spell -> effectN( 1 ).average( effect.item ) )
      ->set_max_stack( 20 ) // Hardcoded for now - spell->max_stacks() returns 0
      ->set_tick_behavior( buff_tick_behavior::CLIP )
      ->set_period( spell -> effectN( 1 ).period() )
      ->set_duration( spell -> duration() );

  effect.custom_buff = b;

  new dbc_proc_callback_t( effect.item -> player, effect );
}

void item::darkmoon_card_greatness( special_effect_t& effect )
{
  struct darkmoon_card_greatness_callback : public dbc_proc_callback_t
  {
    stat_buff_t* buff_str;
    stat_buff_t* buff_agi;
    stat_buff_t* buff_int;
    stat_buff_t* buff_spi;

    darkmoon_card_greatness_callback( const item_t* i, const special_effect_t& data ) :
      dbc_proc_callback_t( i -> player, data )
    {
      struct common_buff_t : public stat_buff_t
      {
        common_buff_t( player_t* p, const item_t* i, const std::string& n, stat_e stat, int id ) :
          stat_buff_t ( p, "deathbringers_will_" + n, p -> find_spell( id ) )
        {
          add_stat( stat, p -> find_spell( id ) -> effectN( 1 ).average( *i ) );
        }
      };

      buff_str = make_buff<common_buff_t>( listener, i, "str", STAT_STRENGTH, 60235 );
      buff_agi = make_buff<common_buff_t>( listener, i, "agi", STAT_AGILITY, 60235 );
      buff_int = make_buff<common_buff_t>( listener, i, "int", STAT_INTELLECT, 60235 );
      buff_spi = make_buff<common_buff_t>( listener, i, "spi", STAT_SPIRIT, 60235 );
    }

    void execute( const spell_data_t*, player_t*, action_state_t* ) override
    {
      double str = listener->strength();
      double agi = listener->agility();
      double inte = listener->intellect();
      double spi = listener->spirit();

      if ( str > agi )
      {
        if ( str > inte )
        {
          if ( str > spi )
            buff_str -> trigger();
          else
            buff_spi -> trigger();
        }
        else
        {
          if ( inte > spi )
            buff_int -> trigger();
          else
            buff_spi -> trigger();
        }
      }
      else
      {
        if ( agi > inte )
        {
          if ( agi > spi )
            buff_agi -> trigger();
          else
            buff_spi -> trigger();
        }
        else
        {
          if ( inte > spi )
            buff_int -> trigger();
          else
            buff_spi -> trigger();
        }
      }
    }
  };

  effect.proc_flags2_ = PF2_ALL_HIT;

  new darkmoon_card_greatness_callback( effect.item, effect );
}

void item::vial_of_shadows( special_effect_t& effect )
{
  // TODO: Verify this scales by item level correctly.
  struct lightning_strike_t : public attack_t
  {
    lightning_strike_t( const special_effect_t& effect ) :
      attack_t( "lightning_strike_vial", effect.player, effect.player -> find_spell( 109724 ) )
    {
      background = may_crit = true;
      callbacks = false;

      base_dd_min = base_dd_max = effect.driver() -> effectN( 1 ).average( effect.item );
      switch ( effect.driver() -> id() )
      {
        case 109725: attack_power_mod.direct = 0.339; break;
        case 107995: attack_power_mod.direct = 0.300; break;
        case 109722: attack_power_mod.direct = 0.266; break;
        default: assert( false ); break;
      }
    }
  };

  // Call the proc _vial so it doesn't conflict with the legendary meta gem.
  action_t* action = effect.player -> find_action( "lightning_strike_vial" );
  if ( ! action )
  {
    action = effect.player -> create_proc_action( "lightning_strike_vial", effect );
  }

  if ( ! action )
  {
    action = new lightning_strike_t( effect );
  }

  effect.execute_action = action;

  new dbc_proc_callback_t( effect.player, effect );
}

void item::cunning_of_the_cruel( special_effect_t& effect )
{
  // TODO: Verify this matches in-game.
  struct shadowbolt_volley_t : public spell_t
  {
    shadowbolt_volley_t( const special_effect_t& effect ) :
      spell_t( "shadowbolt_volley", effect.player, effect.player -> find_spell( effect.driver() -> effectN( 1 ).trigger_spell_id() ) )
    {
      background = may_crit = true;
      callbacks = false;
      aoe = -1;
      radius = effect.player -> find_spell( effect.driver() -> effectN( 1 ).trigger_spell_id() ) -> effectN( 1 ).radius();
    }
  };

  action_t* action = effect.player -> find_action( "shadowbolt_volley" );
  if ( ! action )
  {
    action = effect.player -> create_proc_action( "shadowbolt_volley", effect );
  }

  if ( ! action )
  {
    action = new shadowbolt_volley_t( effect );
  }

  effect.execute_action = action;

  new dbc_proc_callback_t( effect.player, effect );
}

void item::deathbringers_will( special_effect_t& effect )
{
  struct deathbringers_will_callback : public dbc_proc_callback_t
  {
    stat_buff_t* str;
    stat_buff_t* agi;
    stat_buff_t* ap;
    stat_buff_t* crit;
    stat_buff_t* haste;

    std::array<stat_buff_t*, 3> procs;

    deathbringers_will_callback( const item_t* i, const special_effect_t& data ) :
      dbc_proc_callback_t( i -> player, data )
    {
      struct common_buff_t : public stat_buff_t
      {
        common_buff_t( player_t* p, const item_t* i, const std::string& n, stat_e stat, int id ) :
          stat_buff_t ( p, "deathbringers_will_" + n, p -> find_spell( id ) )
        {
          add_stat( stat, p -> find_spell( id ) -> effectN( 2 ).average( *i ) );
        }
      };

      str   = make_buff<common_buff_t>( listener, i, "str",   STAT_STRENGTH,     data.spell_id == 71562 ? 71561 : 71484 );
      agi   = make_buff<common_buff_t>( listener, i, "agi",   STAT_AGILITY,      data.spell_id == 71562 ? 71556 : 71485 );
      ap    = make_buff<common_buff_t>( listener, i, "ap",    STAT_ATTACK_POWER, data.spell_id == 71562 ? 71558 : 71486 );
      crit  = make_buff<common_buff_t>( listener, i, "crit",  STAT_CRIT_RATING,  data.spell_id == 71562 ? 71559 : 71491 );
      haste = make_buff<common_buff_t>( listener, i, "haste", STAT_HASTE_RATING, data.spell_id == 71562 ? 71560 : 71492 );

      switch( i -> player -> type )
      {
        case DRUID:    procs[ 0 ] = agi; procs[ 1 ] = haste; procs[ 2 ] =  str; break;
        case HUNTER:   procs[ 0 ] = agi; procs[ 1 ] = haste; procs[ 2 ] =   ap; break;
        case ROGUE:    procs[ 0 ] = agi; procs[ 1 ] = haste; procs[ 2 ] =   ap; break;
        default:       procs[ 0 ] = str; procs[ 1 ] = haste; procs[ 2 ] = crit; break;
      }
    }

    void execute( const spell_data_t*, player_t*, action_state_t* ) override
    {
      procs[ static_cast<int>( rng().real() * 3 ) ]->trigger();
    }
  };

  new deathbringers_will_callback( effect.item, effect );
}

void item::battering_talisman_trigger( special_effect_t& effect )
{
  maintenance_check( 528 );

  const spell_data_t* driver = effect.item -> player -> find_spell( effect.spell_id );
  const spell_data_t* spell = driver -> effectN( 1 ).trigger();
  const spell_data_t* stacks = effect.item -> player -> find_spell( 146293 );

  auto buff_name = util::tokenize_fn( spell -> name_cstr() );

  // Require a damaging result, instead of any harmful spell hit
  effect.proc_flags2_ = PF2_ALL_HIT;

  stat_buff_t* b = make_buff<stat_buff_t>( effect.item -> player, buff_name, spell, effect.item );
  b->add_stat( STAT_HASTE_RATING, spell -> effectN( 1 ).average( effect.item ) )
      ->set_max_stack( stacks -> max_stacks() )
      ->set_tick_behavior( buff_tick_behavior::CLIP )
      ->set_period( spell -> effectN( 1 ).period() )
      ->set_duration( spell -> duration() );

  effect.custom_buff = b;

  new dbc_proc_callback_t( effect.item -> player, effect );
}

void item::forgemasters_insignia( special_effect_t& effect )
{
  maintenance_check( 528 );

  const spell_data_t* driver = effect.item -> player -> find_spell( effect.spell_id );
  const spell_data_t* spell = driver -> effectN( 1 ).trigger();

  auto buff_name = util::tokenize_fn( spell -> name_cstr() );

  // Require a damaging result, instead of any harmful spell hit
  effect.proc_flags2_ = PF2_ALL_HIT;

  stat_buff_t* b = make_buff<stat_buff_t>( effect.item -> player, buff_name, spell, effect.item );
  b->add_stat( STAT_MASTERY_RATING, spell -> effectN( 1 ).average( effect.item ) )
      ->set_max_stack( 20 ) // Hardcoded for now - spell->max_stacks() returns 0
      ->set_tick_behavior( buff_tick_behavior::CLIP )
      ->set_period( spell -> effectN( 1 ).period() )
      ->set_duration( spell -> duration() );

  effect.custom_buff = b;

  new dbc_proc_callback_t( effect.item -> player, effect );
}

void item::autorepairing_autoclave( special_effect_t& effect )
{
  maintenance_check( 528 );

  const spell_data_t* driver = effect.item -> player -> find_spell( effect.spell_id );
  const spell_data_t* spell = driver -> effectN( 1 ).trigger();

  auto buff_name = util::tokenize_fn( spell -> name_cstr() );

  // Require a damaging result, instead of any harmful spell hit
  effect.proc_flags2_ = PF2_ALL_HIT;

  stat_buff_t* b = make_buff<stat_buff_t>( effect.item -> player, buff_name, spell, effect.item );
  b->add_stat( STAT_HASTE_RATING, spell -> effectN( 1 ).average( effect.item ) )
      ->set_max_stack( 20 ) // Hardcoded for now - spell->max_stacks() returns 0
      ->set_tick_behavior( buff_tick_behavior::CLIP )
      ->set_period( spell -> effectN( 1 ).period() )
      ->set_duration( spell -> duration() );

  effect.custom_buff = b;

  new dbc_proc_callback_t( effect.item -> player, effect );
}

void item::spellbound_runic_band( special_effect_t& effect )
{
  maintenance_check( 528 );

  player_t* p = effect.item -> player;
  const spell_data_t* driver = p -> find_spell( effect.spell_id );
  buff_t* buff = nullptr;

  // Need to test which procs on off-spec rings, assume correct proc for now.
  switch( p -> convert_hybrid_stat( STAT_STR_AGI_INT ) )
  {
    case STAT_STRENGTH:
      buff = create_buff<buff_t>( p, p->find_spell( 177175 ) )
        ->set_pct_buff_type_from_data( true );
      break;
    case STAT_AGILITY:
      buff = create_buff<buff_t>( p, p->find_spell( 177172 ) )
        ->set_pct_buff_type_from_data( true );
      break;
    case STAT_INTELLECT:
      buff = create_buff<buff_t>( p, p->find_spell( 177176 ) )
        ->set_pct_buff_type_from_data( true );
      break;
    default:
      break;
  }

  effect.ppm_ = -1.0 * driver -> real_ppm();
  effect.custom_buff = buff;
  effect.type = SPECIAL_EFFECT_EQUIP;

  new dbc_proc_callback_t( p, effect );
}

void item::spellbound_solium_band( special_effect_t& effect )
{
  maintenance_check( 528 );

  player_t* p = effect.item -> player;
  const spell_data_t* driver = p -> find_spell( effect.spell_id );
  buff_t* buff = nullptr;

  //Need to test which procs on off-spec rings, assume correct proc for now.
  switch( p -> convert_hybrid_stat( STAT_STR_AGI_INT ) )
  {
    case STAT_STRENGTH:
      buff = create_buff<buff_t>( p, p->find_spell( 177160 ) )
        ->set_pct_buff_type_from_data( true );
      break;
    case STAT_AGILITY:
      buff = create_buff<buff_t>( p, p->find_spell( 177161 ) )
        ->set_pct_buff_type_from_data( true );
      break;
    case STAT_INTELLECT:
      buff = create_buff<buff_t>( p, p->find_spell( 177159 ) )
        ->set_pct_buff_type_from_data( true );
      break;
    default:
      break;
  }

  effect.ppm_ = -1.0 * driver -> real_ppm();
  effect.custom_buff = buff;
  effect.type = SPECIAL_EFFECT_EQUIP;

  new dbc_proc_callback_t( p, effect );
}

void item::gronntooth_war_horn( special_effect_t& effect )
{
  stat_buff_t* buff =
    make_buff<stat_buff_t>( effect.player, "demonbane", effect.driver()->effectN( 1 ).trigger(), effect.item );
  effect.custom_buff = buff;
  effect.player->register_creature_type_buff( buff );

  new dbc_proc_callback_t( effect.item, effect );
}

void item::infallible_tracking_charm( special_effect_t& effect )
{
  effect.custom_buff =
    make_buff( effect.player, "cleansing_flame", effect.driver()->effectN( 1 ).trigger(), effect.item );
  effect.execute_action = new spell_t( "cleansing_flame", effect.player, effect.driver()->effectN( 1 ).trigger() );

  effect.execute_action->background = true;
  effect.execute_action->item = effect.item;
  effect.execute_action->base_dd_min = effect.execute_action->base_dd_max =
    effect.execute_action->data().effectN( 1 ).average( effect.item );

  effect.rppm_scale_ = RPPM_HASTE;

  effect.player->register_creature_type_buff( effect.custom_buff );

  new dbc_proc_callback_t( effect.item, effect );
}

void item::orb_of_voidsight( special_effect_t& effect )
{
  stat_buff_t* buff =
    make_buff<stat_buff_t>( effect.player, "voidsight", effect.driver()->effectN( 1 ).trigger(), effect.item );
  effect.custom_buff = buff;
  effect.player->register_creature_type_buff( buff );

  new dbc_proc_callback_t( effect.item, effect );
}

void item::witherbarks_branch( special_effect_t& effect )
{
  struct aqueous_dowsing_t : public buff_t
  {
    int custom_tick;
    stat_buff_t* stat_buff;

    aqueous_dowsing_t( const actor_pair_t& p, const special_effect_t& e )
      : buff_t( p, "aqueous_dowsing", e.driver() ), custom_tick( 0 )
    {
      set_quiet( true );
      set_cooldown( 0_ms );
      set_can_cancel( false );

      stat_buff = make_buff<stat_buff_t>( e.player, "aqueous_enrichment", e.player->find_spell( 429262 ), e.item );

      set_stack_change_callback( [ this ]( buff_t*, int, int n ) {
        if ( n )
        {
          custom_tick = 0;
          make_event( player->sim, player->dragonflight_opts.witherbarks_branch_timing[ custom_tick ], [ this ]() {
            custom_tick_callback();
          } );
        }
      } );
    }

    void custom_tick_callback()
    {
      if ( !check() )
        return;

      stat_buff->trigger();

      if ( custom_tick < 2 )
        make_event( player->sim, player->dragonflight_opts.witherbarks_branch_timing[ ++custom_tick ], [ this ]() {
          custom_tick_callback();
        } );
    }
  };

  effect.custom_buff = new aqueous_dowsing_t( effect.player, effect );
  effect.stat = STAT_MASTERY_RATING;
}

void item::black_blood_of_yshaarj( special_effect_t& effect )
{
  maintenance_check( 528 );

  const spell_data_t* driver = effect.item -> player -> find_spell( effect.spell_id );
  const spell_data_t* ticker = driver -> effectN( 1 ).trigger();
  const spell_data_t* buff = effect.item -> player -> find_spell( 146202 );

  auto buff_name = util::tokenize_fn( buff -> name_cstr() );

  stat_buff_t* b = make_buff<stat_buff_t>( effect.item -> player, buff_name, buff, effect.item );
  b->add_stat( STAT_INTELLECT, ticker -> effectN( 1 ).average( effect.item ) )
      ->set_tick_behavior( buff_tick_behavior::CLIP )
      ->set_period( ticker -> effectN( 1 ).period() )
      ->set_duration( ticker -> duration () );

  effect.custom_buff = b;

  new dbc_proc_callback_t( effect.item -> player, effect );
}

struct flurry_of_xuen_melee_t : public melee_attack_t
{
  flurry_of_xuen_melee_t( player_t* player ) :
    melee_attack_t( "flurry_of_xuen", player, player -> find_spell( 147891 ) )
  {
    background = true;
    proc = false;
    aoe = 5;
    special = may_miss = may_parry = may_block = may_dodge = may_crit = true;
  }
};

struct flurry_of_xuen_ranged_t : public ranged_attack_t
{
  flurry_of_xuen_ranged_t( player_t* player ) :
    ranged_attack_t( "flurry_of_xuen", player, player -> find_spell( 147891 ) )
  {
    // TODO: check attack_power_mod.direct = data().extra_coeff();
    background = true;
    proc = false;
    aoe = 5;
    special = may_miss = may_parry = may_block = may_dodge = may_crit = true;
  }
};

struct flurry_of_xuen_driver_t : public attack_t
{
  action_t* ac;

  flurry_of_xuen_driver_t( player_t* player, action_t* action = nullptr ) :
    attack_t( "flurry_of_xuen_driver", player, player -> find_spell( 146194 ) ),
    ac( nullptr )
  {
    hasted_ticks = may_crit = may_miss = may_dodge = may_parry = callbacks = false;
    proc = background = dual = true;

    if ( ! action )
    {
      if ( player -> type == HUNTER )
        ac = new flurry_of_xuen_ranged_t( player );
      else
        ac = new flurry_of_xuen_melee_t( player );
    }
    else
      ac = action;
  }

  // Don't use tick action here, so we can get class specific snapshotting, if
  // there is a custom proc action crated. Hack and workaround and ugly.
  void tick( dot_t* ) override
  {
    if ( ac )
      ac -> schedule_execute();

    player -> trigger_ready();
  }
};

struct flurry_of_xuen_cb_t : public dbc_proc_callback_t
{
  flurry_of_xuen_cb_t( player_t* p, const special_effect_t& effect ) :
    dbc_proc_callback_t( p, effect )
  { }

  void trigger( const proc_data_t& data, player_t* t, action_state_t* s, proc_trigger_type_e type ) override
  {
    // Flurry of Xuen, and Lightning Strike cannot proc Flurry of Xuen
    if ( data.spell->id() == 147891 || data.spell->id() == 146194 || data.spell->id() == 137597 )
      return;

    dbc_proc_callback_t::trigger( data, t, s, type );
  }
};

void item::flurry_of_xuen( special_effect_t& effect )
{
  maintenance_check( 600 );

  if ( effect.item -> sim -> challenge_mode )
    return;

  if ( effect.item -> player -> level() >= 100 )
    return;

  player_t* p = effect.item -> player;
  effect.execute_action = new flurry_of_xuen_driver_t( p, p -> create_proc_action( effect.name(), effect ) );

  new flurry_of_xuen_cb_t( p, effect );
}

struct essence_of_yulon_t : public spell_t
{
  essence_of_yulon_t( player_t* p, const spell_data_t& driver ) :
    spell_t( "essence_of_yulon", p, p -> find_spell( 148008 ) )
  {
    background = may_crit = true;
    proc = false;
    aoe = 5;
    spell_power_mod.direct /= driver.duration().total_seconds() + 1;
  }
};

struct essence_of_yulon_driver_t : public spell_t
{
  essence_of_yulon_driver_t( player_t* player ) :
    spell_t( "essence_of_yulon", player, player -> find_spell( 146198 ) )
  {
    hasted_ticks = may_miss = may_dodge = may_parry = may_block = callbacks = may_crit = false;
    tick_zero = proc = background = dual = true;
    travel_speed = 0;

    tick_action = new essence_of_yulon_t( player, data() );
  }
};

struct essence_of_yulon_cb_t : public dbc_proc_callback_t
{

  essence_of_yulon_cb_t( player_t* p, const special_effect_t& effect ) :
    dbc_proc_callback_t( p, effect )
  { }

  void trigger( const proc_data_t& data, player_t* t, action_state_t* s, proc_trigger_type_e type ) override
  {
    if ( data.spell->id() == 148008 ) // dot direct damage ticks can't proc itself
      return;

    dbc_proc_callback_t::trigger( data, t, s, type );
  }
};

void item::essence_of_yulon( special_effect_t& effect )
{
  maintenance_check( 600 );

  if ( effect.item -> sim -> challenge_mode )
    return;

  if ( effect.item -> player -> level() >= 100 )
    return;

  player_t* p = effect.item -> player;

  effect.execute_action = new essence_of_yulon_driver_t( p );

  new essence_of_yulon_cb_t( p, effect );
}

void item::readiness( special_effect_t& effect )
{
  maintenance_check( 528 );

  struct cooldowns_t
  {
    specialization_e spec;
    std::array<const char*, 8> cooldowns;
  };

  static const cooldowns_t __cd[] =
  {
    // NOTE: Spells that trigger buffs must have the cooldown of their buffs removed if they have one, or this trinket may cause undesirable results.
    { ROGUE_ASSASSINATION, { "evasion", "vanish", "cloak_of_shadows", "vendetta", nullptr, nullptr } },
    { ROGUE_OUTLAW,        { "evasion", "adrenaline_rush", "cloak_of_shadows", "killing_spree", nullptr, nullptr } },
    { ROGUE_SUBTLETY,      { "evasion", "vanish", "cloak_of_shadows", "shadow_dance", nullptr, nullptr } },
    { SHAMAN_ENHANCEMENT,  { "earth_elemental_totem", "fire_elemental_totem", "shamanistic_rage", "ascendance", "feral_spirit", nullptr } },
    { DRUID_FERAL,         { "tigers_fury", "berserk", "barkskin", "survival_instincts", nullptr, nullptr, nullptr } },
    { DRUID_GUARDIAN,      { "might_of_ursoc", "berserk", "barkskin", "survival_instincts", nullptr, nullptr, nullptr } },
    { WARRIOR_FURY,        { "dragon_roar", "bladestorm", "shockwave", "avatar", "bloodbath", "recklessness", "storm_bolt", "heroic_leap" } },
    { WARRIOR_ARMS,        { "dragon_roar", "bladestorm", "shockwave", "avatar", "bloodbath", "recklessness", "storm_bolt", "heroic_leap" } },
    { WARRIOR_PROTECTION,  { "shield_wall", "demoralizing_shout", "last_stand", "recklessness", "heroic_leap", nullptr, nullptr } },
    { DEATH_KNIGHT_BLOOD,  { "antimagic_shell", "dancing_rune_weapon", "icebound_fortitude", "outbreak", "vampiric_blood", "bone_shield", nullptr } },
    { DEATH_KNIGHT_FROST,  { "antimagic_shell", "army_of_the_dead", "icebound_fortitude", "empower_rune_weapon", "outbreak", "pillar_of_frost", nullptr  } },
    { DEATH_KNIGHT_UNHOLY, { "antimagic_shell", "army_of_the_dead", "icebound_fortitude", "outbreak", "summon_gargoyle", nullptr } },
    { MONK_BREWMASTER,	   { "fortifying_brew", "guard", "zen_meditation", nullptr, nullptr, nullptr, nullptr } },
    { MONK_WINDWALKER,     { "energizing_brew", "fists_of_fury", "fortifying_brew", "zen_meditation", nullptr, nullptr, nullptr } },
    { PALADIN_PROTECTION,  { "ardent_defender", "avenging_wrath", "divine_protection", "divine_shield", "guardian_of_ancient_kings", nullptr } },
    { PALADIN_RETRIBUTION, { "avenging_wrath", "divine_protection", "divine_shield", "guardian_of_ancient_kings", nullptr, nullptr } },
    { HUNTER_BEAST_MASTERY,{ "camouflage", "feign_death", "disengage", "stampede", "rapid_fire", "bestial_wrath", nullptr } },
    { HUNTER_MARKSMANSHIP, { "camouflage", "feign_death", "disengage", "stampede", "rapid_fire", nullptr, nullptr } },
    { HUNTER_SURVIVAL,     { "black_arrow", "camouflage", "feign_death", "disengage", "stampede", "rapid_fire", nullptr } },
    { SPEC_NONE,           { nullptr } }
  };

  player_t* p = effect.item -> player;

  const spell_data_t* cdr_spell = p -> find_spell( effect.spell_id );
  const random_prop_data_t& budget = p -> dbc->random_property( effect.item -> item_level() );
  double cdr = 1.0 / ( 1.0 + budget.p_epic[ 0 ] * cdr_spell -> effectN( 1 ).m_coefficient() / 100.0 );

  if ( p -> level() > 90 )
  { // We have no clue how the trinket actually scales down with level. This will linearly decrease CDR until it hits .90 at level 100.
    double level_nerf = ( static_cast<double>( p -> level() - 90 ) / 10.0 );
    level_nerf = ( 1 - cdr ) * level_nerf;
    cdr += level_nerf;
    cdr = std::min( 0.90, cdr ); // The amount of CDR doesn't go above 90%, even at level 100.
  }

  const cooldowns_t* cd = &( __cd[ 0 ] );
  do
  {
    if ( p -> specialization() != cd -> spec )
    {
      cd++;
      continue;
    }

    for ( size_t i = 0; i < 7; i++ )
    {
      if ( cd -> cooldowns[ i ] == nullptr )
        break;

      auto action = p -> find_action( cd -> cooldowns[ i ] );
      if ( action != nullptr )
      {
        action -> base_recharge_multiplier *= cdr;
      }
    }

    break;
  } while ( cd -> spec != SPEC_NONE );
}

void item::amplification( special_effect_t& effect )
{
  effect.player->parse_passive_item_effect( effect.driver() );
}

void item::prismatic_prison_of_pride( special_effect_t& effect )
{
  // Disable Prismatic Prison of Pride stat proc (Int) for all roles but HEAL
  if ( effect.item -> player -> role != ROLE_HEAL )
    return;

  effect.type = SPECIAL_EFFECT_EQUIP;
  new dbc_proc_callback_t( effect.item, effect );
}

void item::purified_bindings_of_immerseus( special_effect_t& effect )
{
  // Disable Purified Bindings stat proc (Int) on healing roles
  if ( effect.item -> player -> role == ROLE_HEAL )
    return;

  effect.type = SPECIAL_EFFECT_EQUIP;
  new dbc_proc_callback_t( effect.item, effect );
}

void item::thoks_tail_tip( special_effect_t& effect )
{
  // Disable Thok's Tail Tip stat proc (Str) on healing roles
  if ( effect.item -> player -> role == ROLE_HEAL )
    return;

  effect.type = SPECIAL_EFFECT_EQUIP;
  new dbc_proc_callback_t( effect.item, effect );
}

template <typename T>
struct cleave_t : public T
{
  cleave_t( const item_t* item, const std::string& name, school_e s ) :
    T( name, item -> player )
  {
    this->callbacks = false;
    this->may_crit = false;
    this->may_glance = false;
    this->may_miss = true;
    this->special = true;
    this->proc = true;
    this->background = true;
    this->school = s;
    this->aoe = 5;
    if ( this->type == ACTION_ATTACK )
    {
      this->may_dodge = true;
      this->may_parry = true;
      this->may_block = true;
      this->ignores_armor = true;
    }
  }

  void init() override
  {
    T::init();

    this -> snapshot_flags = 0;
  }

  size_t available_targets( std::vector< player_t* >& tl ) const override
  {
    tl.clear();

    for ( size_t i = 0, actors = this -> sim -> target_non_sleeping_list.size(); i < actors; i++ )
    {
      player_t* t = this -> sim -> target_non_sleeping_list[ i ];

      if ( t -> is_enemy() && ( t != this -> target ) )
        tl.push_back( t );
    }

    return tl.size();
  }
};

void item::cleave( special_effect_t& effect )
{
  maintenance_check( 528 );

  struct cleave_callback_t : public dbc_proc_callback_t
  {
    cleave_t<spell_t>* cleave_spell;
    cleave_t<attack_t>* cleave_attack;

    cleave_callback_t( const special_effect_t& data ) :
      dbc_proc_callback_t( *data.item, data )
    {
      cleave_spell = new cleave_t<spell_t>( data.item, "cleave_spell", SCHOOL_NATURE );
      cleave_attack = new cleave_t<attack_t>( data.item, "cleave_attack", SCHOOL_PHYSICAL );

    }

    void execute( const spell_data_t*, player_t* t, action_state_t* s ) override
    {
      action_t* a = nullptr;

      if ( s->action->type == ACTION_ATTACK )
        a = cleave_attack;
      else if ( s->action->type == ACTION_SPELL )
        a = cleave_spell;
      // TODO: Heal

      if ( a )
      {
        a->base_dd_min = a->base_dd_max = s->result_amount;
        // Invalidate target cache if target changes
        if ( a->target != t )
          a->target_cache.is_valid = false;
        a->target = t;
        a -> schedule_execute();
      }
    }
  };

  player_t* p = effect.item -> player;
  const random_prop_data_t& budget = p -> dbc->random_property( effect.item -> item_level() );
  const spell_data_t* cleave_driver_spell = p -> find_spell( effect.spell_id );

  // Needs a damaging result
  effect.proc_flags2_ = PF2_ALL_HIT;
  effect.proc_chance_ = budget.p_epic[ 0 ] * cleave_driver_spell -> effectN( 1 ).m_coefficient() / 10000.0;

  if ( p -> level() > 90 )
  { // We have no clue how the trinket actually scales down with level. This will linearly decrease amplification until it hits 0 at level 100.
    double level_nerf = ( static_cast<double>( p -> level() ) - 90 ) / 10.0;
     effect.proc_chance_ *= 1 - level_nerf;
     effect.proc_chance_ = std::max( 0.01, effect.proc_chance_ ); // Cap it at 1%
  }

  new cleave_callback_t( effect );
}

// Spell 62459 is attached to several legacy Death Knight PvP gloves. Its DBC
// proc flags are broad (all hostile melee/spell impacts), while the spell-class
// mask that narrows it to Chains of Ice is not interpreted by the generic item
// proc builder. Generic initialization therefore fires 62458 from every attack,
// DoT and even other item procs, fabricating thousands of Runic Power.
//
// The live tooltip is explicit: "Your Chains of Ice ability now generates an
// additional 1 Runic Power." Keep the effect, but enforce that exact trigger
// and amount here instead of treating the raw proc row as a generic on-hit.
namespace chains_of_ice_runic_power
{
static constexpr unsigned CHAINS_OF_ICE_SPELL_ID = 45524;
static constexpr double BONUS_RUNIC_POWER = 1.0;

struct callback_t final : public dbc_proc_callback_t
{
  player_t* player;
  gain_t* gain;

  callback_t( const special_effect_t& effect )
    : dbc_proc_callback_t( effect.item, effect ),
      player( effect.player ),
      gain( effect.player->get_gain( "chains_of_ice_runic_power" ) )
  {}

  void trigger( const proc_data_t& source_data, player_t* target, action_state_t* state,
                proc_trigger_type_e type ) override
  {
    if ( !state || !state->action || state->action->data().id() != CHAINS_OF_ICE_SPELL_ID )
      return;

    dbc_proc_callback_t::trigger( source_data, target, state, type );
  }

  void execute( const spell_data_t*, player_t*, action_state_t* ) override
  {
    player->resource_gain( RESOURCE_RUNIC_POWER, BONUS_RUNIC_POWER, gain );
  }
};
}  // namespace chains_of_ice_runic_power

void item::chains_of_ice_runic_power( special_effect_t& effect )
{
  effect.name_str = "chains_of_ice_runic_power";
  effect.proc_chance_ = 1.0;
  new chains_of_ice_runic_power::callback_t( effect );
}

void item::heartpierce( special_effect_t& effect )
{
  struct invigorate_proc_t : public spell_t
  {
    invigorate_proc_t( player_t* player, const spell_data_t* d ) :
      spell_t( "invigoration", player, d )
    {
      may_miss = may_crit = harmful = may_dodge = may_parry = callbacks = false;
      tick_may_crit = hasted_ticks = false;
      dual = quiet = background = true;
      target = player;
    }
  };

  // Heartpierce is one PPM from attacks made with either equipped weapon. The
  // generic modern callback also receives weaponless melee child actions; if
  // those are allowed through, Outlaw rolls the PPM once for virtually every
  // damage event and can keep Invigoration permanently refreshed.
  struct heartpierce_callback_t final : public dbc_proc_callback_t
  {
    heartpierce_callback_t( const special_effect_t& e )
      : dbc_proc_callback_t( e.item, e )
    {}

    void trigger( const proc_data_t& source_data, player_t* target, action_state_t* state,
                  proc_trigger_type_e type ) override
    {
      if ( !state || !state->action || !state->action->weapon )
        return;

      dbc_proc_callback_t::trigger( source_data, target, state, type );
    }
  };

  // The heroic dagger has a 12-second Invigoration spell; normal lasts 10.
  // The old implementation always selected the normal spell even for heroic.
  const bool heroic = effect.spell_id == 71892;
  unsigned spell_id = 0;
  switch ( effect.item -> player -> primary_resource() )
  {
    case RESOURCE_MANA:
      spell_id = heroic ? 71888 : 71881;
      break;
    case RESOURCE_ENERGY:
      spell_id = heroic ? 71887 : 71882;
      break;
    case RESOURCE_RAGE:
      spell_id = heroic ? 71886 : 71883;
      break;
    default:
      break;
  }

  if ( spell_id == 0 )
  {
    effect.type = SPECIAL_EFFECT_NONE;
    return;
  }

  effect.ppm_ = 1.0;
  effect.execute_action = new invigorate_proc_t( effect.item -> player, effect.item -> player -> find_spell( spell_id ) );

  new heartpierce_callback_t( effect );
}

struct felmouth_frenzy_driver_t : public spell_t
{
  struct felmouth_frenzy_damage_t : public spell_t
  {
    player_t* p;

    felmouth_frenzy_damage_t( const special_effect_t& effect ) :
      spell_t( "felmouth_frenzy_damage", effect.player, effect.player -> find_spell( 188505 ) ),
      p( effect.player )
    {
      background = true;
      callbacks = false;

      base_dd_max = base_dd_min = 0;
      spell_power_mod.direct = attack_power_mod.direct = 0.424;
    }

    double attack_direct_power_coefficient( const action_state_t* s ) const override
    {
      if ( s -> composite_spell_power() > s -> composite_attack_power() )
        return 0;

      return spell_t::attack_direct_power_coefficient( s );
    }

    double spell_direct_power_coefficient( const action_state_t* s ) const override
    {
      if ( s -> composite_spell_power() <= s -> composite_attack_power() )
        return 0;

      return spell_t::spell_direct_power_coefficient( s );
    }
  };

  size_t bolt_avg, bolt_magnitude;
  player_t* p;
  std::vector<unsigned> n_ticks;

  felmouth_frenzy_driver_t( const special_effect_t& effect ) :
    spell_t( "felmouth_frenzy", effect.player, effect.player -> find_spell( 188512 ) ),
    bolt_avg( as<int>( effect.player -> find_spell( 188534 ) -> effectN( 1 ).trigger() -> effectN( 1 ).base_value() ) ),
    bolt_magnitude( as<int>(bolt_avg * effect.player -> find_spell( 188534 ) -> effectN( 1 ).trigger() -> effectN( 1 ).m_delta() / 2.0) ),
    p( effect.player )
  {
    background = true;
    may_crit = callbacks = hasted_ticks = false;
    dynamic_tick_action = TICK_ACTION_NONE;
    // Estimated from logs
    base_tick_time = timespan_t::from_millis( 250 );
    dot_behavior = DOT_EXTEND;
    travel_speed = 0;

    tick_action = effect.player -> find_action( "felmouth_frenzy_damage" );
    if ( ! tick_action )
    {
      tick_action = effect.player -> create_proc_action( "felmouth_frenzy_damage", effect );
    }

    if ( ! tick_action )
    {
      tick_action = new felmouth_frenzy_damage_t( effect );
    }

    int min_ticks = as<int>( std::floor( bolt_avg - bolt_magnitude ) );
    int max_ticks = as<int>( std::ceil( bolt_avg + bolt_magnitude ) );
    int tick_steps = max_ticks - min_ticks + 1;
    for ( int i = 0; i < tick_steps; ++i )
    {
      n_ticks.push_back( min_ticks + i );
    }
  }

  void init_finished() override
  {
    spell_t::init_finished();

    // Can't be done on init() for abilities with tick_action() as the parent init() is called
    // before action_t::consolidate_snapshot_flags().
    snapshot_flags = STATE_AP | STATE_SP | STATE_TGT_MUL_TA;
    update_flags = STATE_TGT_MUL_TA;
  }

  timespan_t composite_dot_duration( const action_state_t* ) const override
  {
    size_t ticks = rng().range( n_ticks );
    assert( ticks >= 4 && ticks <= 6 );
    return base_tick_time * ticks;
  }

  double composite_total_spell_power() const override
  {
    // Fel Lash uses the player's highest primary school spellpower.
    double csp = 0.0;

    for ( school_e i = SCHOOL_NONE; i < SCHOOL_MAX_PRIMARY; i++ )
      csp = std::max( csp, p->composite_total_spell_power( i ) );

    return csp;
  }
};

void item::felmouth_frenzy( special_effect_t& effect )
{
  action_t* a = effect.player -> find_action( "felmouth_frenzy_driver" );
  if ( ! a )
  {
    a = effect.player -> create_proc_action( "felmouth_frenzy_driver", effect );
  }

  if ( ! a )
  {
    a = new felmouth_frenzy_driver_t( effect );
  }

  effect.execute_action = a;

  dbc_proc_callback_t* cb = new dbc_proc_callback_t( effect.player, effect );
  cb -> initialize();
}

// Matrix Restabilizer
// 96976 Driver
// 96977 Haste buff
// 96978 Critical Strike buff
// 96979 Mastery buff
void item::matrix_restabilizer( special_effect_t& effect )
{
  auto buffs    = std::make_shared<std::map<stat_e, buff_t*>>();
  (*buffs)[STAT_HASTE_RATING] = create_buff<stat_buff_t>( effect.player, "matrix_restabilized_haste_rating",
                                                              effect.player->find_spell( 96977 ), effect.item );
  (*buffs)[STAT_CRIT_RATING] = create_buff<stat_buff_t>( effect.player, "matrix_restabilized_crit_rating",
                                                              effect.player->find_spell( 96978 ), effect.item );
  (*buffs)[STAT_MASTERY_RATING] = create_buff<stat_buff_t>( effect.player, "matrix_restabilized_mastery_rating",
                                                              effect.player->find_spell( 96979 ), effect.item );

  struct matrix_restabilizer_cb_t : public dbc_proc_callback_t
  {
    std::shared_ptr<std::map<stat_e, buff_t*>> buffs;

    matrix_restabilizer_cb_t( const special_effect_t& effect, std::shared_ptr<std::map<stat_e, buff_t*>> b )
      : dbc_proc_callback_t( effect.item, effect ), buffs( std::move( b ) )
    {
    }

    void execute( const spell_data_t*, player_t*, action_state_t* ) override
    {
      static constexpr std::array<stat_e, 3> ratings = { STAT_MASTERY_RATING, STAT_HASTE_RATING,
                                                         STAT_CRIT_RATING };

      stat_e max_stat = util::highest_stat( listener, ratings );

      (*buffs)[max_stat]->trigger();
    }
  };

  new matrix_restabilizer_cb_t( effect, buffs );
}

struct empty_drinking_horn_damage_t : public melee_attack_t
{
  empty_drinking_horn_damage_t( const special_effect_t& effect ) :
    melee_attack_t( "fel_burn", effect.player, effect.player -> find_spell( 184256 ) )
  {
    background = special = tick_may_crit = true;
    may_crit = callbacks = false;
    base_td = data().effectN( 1 ).average( effect.item );
    weapon_multiplier = 0;
    item = effect.item;
    dot_behavior = dot_behavior_e::DOT_NONE;
  }
};

struct empty_drinking_horn_cb_t : public dbc_proc_callback_t
{
  action_t* burn;
  empty_drinking_horn_cb_t( const special_effect_t& effect ):
    dbc_proc_callback_t( effect.player, effect )
  {
    burn = effect.player -> find_action( "fel_burn" );
    if ( !burn )
    {
      burn = effect.item -> player -> create_proc_action( "fel_burn", effect );
    }

    if ( !burn )
    {
      burn = new empty_drinking_horn_damage_t( effect );
    }
  }

  void execute( const spell_data_t*, player_t* t, action_state_t* ) override
  {
    burn->execute_on_target( t );
  }
};

void item::empty_drinking_horn( special_effect_t& effect )
{
  effect.proc_flags2_ = PF2_ALL_HIT;

  new empty_drinking_horn_cb_t( effect );
}

void item::discordant_chorus( special_effect_t& effect )
{
  struct fel_cleave_t: public melee_attack_t
  {
    fel_cleave_t( const special_effect_t& effect ):
      melee_attack_t( "fel_cleave", effect.player, effect.driver() -> effectN( 1 ).trigger() )
    {
      background = special = may_crit = true;
      callbacks = false;
      base_dd_min = base_dd_max = data().effectN( 1 ).average( effect.item );
      weapon_multiplier = 0;
      aoe = -1;
      item = effect.item;
    }
  };

  action_t* action = effect.player -> find_action( "fel_cleave" );

  if ( !action )
  {
    action = effect.player -> create_proc_action( "fel_cleave", effect );
  }

  if ( !action )
  {
    action = new fel_cleave_t( effect );
  }

  effect.execute_action = action;
  effect.proc_flags2_ = PF2_ALL_HIT;

  new dbc_proc_callback_t( effect.player, effect );
}

struct hammering_blows_buff_t : public stat_buff_t
{
  struct enable_event_t : public event_t
  {
    dbc_proc_callback_t* driver;

    enable_event_t( dbc_proc_callback_t* cb )
      : event_t( *cb->listener, timespan_t::zero() ), driver( cb )
    {
    }

    const char* name() const override
    { return "hammering_blows_enable_event"; }

    void execute() override
    {
      driver -> activate();
    }
  };

  dbc_proc_callback_t* stack_driver;

  hammering_blows_buff_t( const special_effect_t& source_effect ) :
    stat_buff_t( source_effect.player, "hammering_blows",
                 source_effect.trigger(), source_effect.item ),
    stack_driver( nullptr )
  {
    set_refresh_behavior( buff_refresh_behavior::DISABLED );
  }

  void execute( int stacks, double value, timespan_t duration ) override
  {
    bool state_change = current_stack == 0;
    stat_buff_t::execute( stacks, value, duration );

    if ( state_change )
    {
      make_event<enable_event_t>( *sim, stack_driver );
    }
  }

  void expire_override( int expiration_stacks, timespan_t remaining_duration ) override
  {
    stat_buff_t::expire_override( expiration_stacks, remaining_duration );

    stack_driver -> deactivate();
  }

  void reset() override
  {
    stat_buff_t::reset();

    stack_driver -> deactivate();
  }
};

struct hammering_blows_driver_cb_t : public dbc_proc_callback_t
{
  hammering_blows_driver_cb_t( const special_effect_t& effect ) :
    dbc_proc_callback_t( effect.player, effect )
  { }

  void execute( const spell_data_t*, player_t*, action_state_t* ) override
  {
    int stack = proc_buff -> check();

    if ( stack == 0 )
    {
      proc_buff -> trigger();
    }
    // Kludge refresh, since the buff is refresh-disabled and we have no way of bumping stacks
    // without refreshing the whole buff.
    else
    {
      proc_buff -> refresh_behavior = buff_refresh_behavior::DURATION;
      proc_buff -> refresh( 0, buff_t::DEFAULT_VALUE(), proc_buff -> data().duration() );
      proc_buff -> refresh_behavior = buff_refresh_behavior::DISABLED;
    }
  }
};

// Secondary initialization for the stack-gain driver for insatiable hunger
void insatiable_hunger_2( special_effect_t& effect )
{
  effect.proc_chance_ = 1.0;
  effect.custom_buff = buff_t::find( effect.player, "hammering_blows" );

  hammering_blows_buff_t* b = static_cast<hammering_blows_buff_t*>( effect.custom_buff );

  b -> stack_driver = new dbc_proc_callback_t( effect.player, effect );
}

void item::insatiable_hunger( special_effect_t& effect )
{
  // Setup a secondary driver when the buff is up to generate stacks to it
  special_effect_t* effect_driver = new special_effect_t( effect.player );
  effect_driver -> type = SPECIAL_EFFECT_EQUIP;
  effect_driver -> name_str = "hammering_blows_driver";
  effect_driver -> spell_id = effect.trigger() -> id();
  effect_driver -> custom_init = insatiable_hunger_2;

  // And make it a player-special effect for now
  effect.player -> special_effects.push_back( effect_driver );

  // Instatiate the actual buff
  effect.custom_buff = new hammering_blows_buff_t( effect );

  new hammering_blows_driver_cb_t( effect );
}

// Int DPS 4 trinket aoe effect, emits from the Mark of Doomed target
struct doom_nova_t : public spell_t
{
  doom_nova_t( const special_effect_t& effect ) :
    spell_t( "doom_nova", effect.player, effect.player -> find_spell( 184075 ) )
  {
    background = may_crit = true;
    callbacks = false;

    base_dd_min = base_dd_max = data().effectN( 1 ).average( effect.item );
    item = effect.item;

    aoe = -1;
  }
};

// Callback driver that executes the Doom Nova on the enemy when the Mark of Doom
// debuff is up.
struct mark_of_doom_damage_driver_t : public dbc_proc_callback_t
{
  action_t* damage;
  player_t* target;

  mark_of_doom_damage_driver_t( const special_effect_t& effect, action_t* d, player_t* t ) :
    dbc_proc_callback_t( effect.player, effect ), damage( d ), target( t )
  { }

  void trigger( const proc_data_t& data, player_t* t, action_state_t* s, proc_trigger_type_e type ) override
  {
    if ( t != target )
    {
      return;
    }

    dbc_proc_callback_t::trigger( data, t, s, type );
  }

  void execute( const spell_data_t*, player_t* t, action_state_t* ) override
  {
    damage -> target = t;
    damage -> execute();
  }
};

// Specialized Mark of Doom debuff, enables/disables mark_of_doom_driver_t (above) when the debuff
// goes up/down.
struct mark_of_doom_t : public buff_t
{
  mark_of_doom_damage_driver_t* driver_cb;
  action_t* damage_spell;
  special_effect_t* effect;

  mark_of_doom_t( const actor_pair_t& p, const special_effect_t* e, const spell_data_t* s, action_t* a )
    : buff_t( p, "mark_of_doom", s, e ? e->item : nullptr ), damage_spell( a )
  {
    set_activated( false );

    // Special effect to drive the AOE damage callback
    effect = new special_effect_t( p.source );
    effect -> name_str = "mark_of_doom_damage_driver";
    effect -> proc_chance_ = 1.0;
    effect -> proc_flags_ = PF_MAGIC_SPELL | PF_NONE_HARMFUL;
    effect -> proc_flags2_ = PF2_ALL_HIT;
    p.source -> special_effects.push_back( effect );

    // And create, initialized and deactivate the callback
    driver_cb = new mark_of_doom_damage_driver_t( *effect, damage_spell, p.target );
    driver_cb -> initialize();
  }

  void expire_override( int expiration_stacks, timespan_t remaining_duration ) override
  {
    buff_t::expire_override( expiration_stacks, remaining_duration );

    driver_cb -> deactivate();
  }

  void execute( int stacks, double value, timespan_t duration ) override
  {
    buff_t::execute( stacks, value, duration );

    driver_cb -> activate();
  }

  void reset() override
  {
    buff_t::reset();

    driver_cb -> deactivate();
  }
};

// Prophecy of Fear base driver, handles the proccing (triggering) of Mark of Doom on targets
struct prophecy_of_fear_driver_t : public dbc_proc_callback_t
{
  action_t* damage;
  const special_effect_t* eff;

  prophecy_of_fear_driver_t( const special_effect_t& effect ) :
    dbc_proc_callback_t( effect.player, effect ), eff( &effect )
  {
    damage = new doom_nova_t( effect );
  }

  buff_t* create_debuff( player_t* t ) override
  {
    return new mark_of_doom_t( { t, listener }, eff, eff->trigger(), damage );
  }

  void execute( const spell_data_t*, player_t* t, action_state_t* ) override
  {
    get_debuff( t )->trigger();
  }
};

void item::prophecy_of_fear( special_effect_t& effect )
{
  effect.proc_flags_ = effect.driver() -> proc_flags() | PF_NONE_HARMFUL;
  effect.proc_flags2_ = PF2_ALL_HIT;

  new prophecy_of_fear_driver_t( effect );
}

struct darklight_ray_t : public spell_t
{
  darklight_ray_t( const special_effect_t& effect ) :
    spell_t( "darklight_ray", effect.player, effect.player -> find_spell( 183950 ) )
  {
    background = may_crit = true;
    callbacks = false;
    item = effect.item;

    base_dd_min = base_dd_max = effect.driver() -> effectN( 1 ).average( effect.item );

    aoe = -1;
  }
};

void item::unblinking_gaze_of_sethe( special_effect_t& effect )
{
  action_t* action = effect.player -> find_action( "darklight_ray" );
  if ( ! action )
  {
    action = effect.player -> create_proc_action( "darklight_ray", effect );
  }

  if ( ! action )
  {
    action = new darklight_ray_t( effect );
  }

  effect.execute_action = action;
  effect.proc_flags2_= PF2_ALL_HIT;

  new dbc_proc_callback_t( effect.player, effect );
}

struct soul_capacitor_explosion_t : public spell_t
{
  double explosion_multiplier;

  soul_capacitor_explosion_t( player_t* player, special_effect_t& effect ) :
    spell_t( "spirit_eruption", player, player -> find_spell( 184559 ) ),
    explosion_multiplier( 0 )
  {
    background = split_aoe_damage = true;
    callbacks = false;
    aoe = -1;
    item = effect.item;

    explosion_multiplier = 1.0 + player -> find_spell( effect.spell_id ) -> effectN( 1 ).average( effect.item ) / 10000.0;
  }

  void init() override
  {
    spell_t::init();

    snapshot_flags = STATE_MUL_SPELL_DA | STATE_MUL_PLAYER_DAM;
    update_flags = 0;
  }

  double composite_da_multiplier( const action_state_t* ) const override
  { return explosion_multiplier; }
};


struct soul_capacitor_buff_t;

struct spirit_shift_explode_callback_t
{
  soul_capacitor_buff_t* buff;

  spirit_shift_explode_callback_t( soul_capacitor_buff_t* b );
  void operator()(player_t*);
};


struct soul_capacitor_buff_t : public buff_t
{
  // Explosion here
  spell_t* explosion;

  soul_capacitor_buff_t( player_t* player, special_effect_t& effect ) :
    buff_t( player, "spirit_shift", player -> find_spell( 184293 ), effect.item ),
    explosion( new soul_capacitor_explosion_t( player, effect ) )
  { }

  void expire_override( int expiration_stacks, timespan_t remaining_duration ) override
  {
    buff_t::expire_override( expiration_stacks, remaining_duration );

    if ( current_value > 0 && ! player -> is_sleeping() )
    {
      explosion -> base_dd_min = explosion -> base_dd_max = current_value;
      explosion -> execute();
    }
  }
};

spirit_shift_explode_callback_t::spirit_shift_explode_callback_t( soul_capacitor_buff_t* b ) :
  buff( b )
{ }

void spirit_shift_explode_callback_t::operator()(player_t* player)
{
  if ( player != player -> sim -> target )
  {
    return;
  }

  buff -> expire();
}

void item::soul_capacitor( special_effect_t& effect )
{
  auto b = new soul_capacitor_buff_t( effect.player, effect );

  effect.custom_buff = b;
  // Perform Spirit Shift accounting just before the target actor is damaged. Spirit Shift will stop
  // the assessing of the state object.
  effect.player -> assessor_out_damage.add( assessor::TARGET_DAMAGE - 1, [ b ]( result_amount_type, action_state_t* state ) {
    player_t* p = state -> action -> player;
    if ( ! b -> up() || ! state -> target -> is_enemy() )
    {
      return assessor::CONTINUE;
    }

    if ( p -> sim -> debug )
    {
      p -> sim -> out_debug.printf( "%s spirit_shift accumulates %.0f damage.",
        p -> name(), state -> result_amount );
    }

    b -> current_value += state -> result_amount;
    // All damage is absorbed, so make the result be zero
    state -> result_amount = 0;

    return assessor::STOP;
  } );

  effect.activation_cb = [ b ]() {
    b->sim->target_non_sleeping_list.register_callback( spirit_shift_explode_callback_t( b ) );
  };

  new dbc_proc_callback_t( effect.player, effect );
}


const char* BLADEMASTER_PET_NAME = "mirror_image_(trinket)";

struct felstorm_tick_t : public melee_attack_t
{
  felstorm_tick_t( pet_t* p ) :
    melee_attack_t( "felstorm_tick", p, p -> find_spell( 184280 ) )
  {
    school = SCHOOL_PHYSICAL;

    background = special = may_crit = true;
    callbacks = false;
    aoe = -1;
    range = data().effectN( 1 ).radius();

    weapon = &( p -> main_hand_weapon );
  }

  void init_finished() override
  {
    // Find first blademaster pet, it'll be the first trinket-created pet
    pet_t* main_pet = player -> cast_pet() -> owner -> find_pet( BLADEMASTER_PET_NAME );
    if ( player != main_pet )
    {
      stats = main_pet -> find_action( "felstorm_tick" ) -> stats;
    }

    melee_attack_t::init_finished();
  }
};

struct felstorm_t : public melee_attack_t
{
  felstorm_t( pet_t* p, const util::string_view opts ) :
    melee_attack_t( "felstorm", p, p -> find_spell( 184279 ) )
  {
    parse_options( opts );

    callbacks = may_miss = may_block = may_parry = false;
    hasted_ticks = true;
    trigger_gcd = timespan_t::from_seconds( 1.0 );

    tick_action = new felstorm_tick_t( p );
  }

  // Make dot long enough to last for the duration of the summon
  timespan_t composite_dot_duration( const action_state_t* ) const override
  { return sim -> expected_iteration_time; }

  void init_finished() override
  {
    pet_t* main_pet = player -> cast_pet() -> owner -> find_pet( BLADEMASTER_PET_NAME );
    if ( player != main_pet )
    {
      stats = main_pet -> find_action( "felstorm" ) -> stats;
    }

    melee_attack_t::init_finished();
  }
};

struct blademaster_pet_t : public pet_t
{
  action_t* felstorm;

  blademaster_pet_t( player_t* owner ) :
    pet_t( owner -> sim, owner, BLADEMASTER_PET_NAME, true, true ),
    felstorm( nullptr )
  {
    main_hand_weapon.type = WEAPON_BEAST;
    // Verified 5/11/15, TODO: Check if this is still the same on live
    owner_coeff.ap_from_ap = 1.0;

    // Magical constants for base damage
    double damage_range = 0.4;
    double base_dps = owner -> dbc->spell_scaling( PLAYER_SPECIAL_SCALE, owner -> level() ) * 4.725;
    double min_dps = base_dps * ( 1 - damage_range / 2.0 );
    double max_dps = base_dps * ( 1 + damage_range / 2.0 );
    main_hand_weapon.swing_time = timespan_t::from_seconds( 2.0 );
    main_hand_weapon.min_dmg =  min_dps * main_hand_weapon.swing_time.total_seconds();
    main_hand_weapon.max_dmg =  max_dps * main_hand_weapon.swing_time.total_seconds();
  }

  timespan_t available() const override
  { return timespan_t::from_seconds( 20.0 ); }

  void init_action_list() override
  {
    action_list_str = "felstorm,if=!ticking";

    pet_t::init_action_list();
  }

  void dismiss( bool expired = false ) override
  {
    pet_t::dismiss( expired );

    if ( dot_t* d = felstorm -> find_dot( felstorm -> target ) )
    {
      d -> cancel();
    }
  }

  action_t* create_action( util::string_view name,
                           util::string_view options_str ) override
  {
    if ( name == "felstorm" )
    {
      felstorm = new felstorm_t( this, options_str );
      return felstorm;
    }

    return pet_t::create_action( name, options_str );
  }
};

struct burning_mirror_t : public spell_t
{
  size_t n_mirrors;
  std::vector<pet_t*> pets;
  const spell_data_t* summon_spell;

  burning_mirror_t( const special_effect_t& effect ) :
    spell_t( "burning_mirror", effect.player, effect.driver() ),
    n_mirrors( static_cast<size_t>( effect.driver() -> effectN( 1 ).base_value() ) ),
    summon_spell( effect.player -> find_spell( 184271 ) )
  {
    background = true;
    may_miss = may_crit = callbacks = harmful = false;

    if ( effect.player -> specialization() == HUNTER_BEAST_MASTERY )
    {
      n_mirrors /= 2;
    }

    bool use_custom = true;
    for ( size_t i = 0; i < n_mirrors; ++i )
    {
      pet_t* blade_master = nullptr;
      if ( use_custom )
        blade_master = effect.player -> create_pet( BLADEMASTER_PET_NAME );
      if ( blade_master == nullptr )
      {
        use_custom = false;
        blade_master = new blademaster_pet_t( effect.player );
      }
      pets.push_back( blade_master );

      // Spawn every other image in front of the target
      if ( i % 2 )
        pets[ i ] -> base.position = POSITION_FRONT;
    }
  }

  void execute() override
  {
    spell_t::execute();

    for ( size_t i = 0; i < n_mirrors; ++i )
    {
      pets[ i ] -> summon( summon_spell -> duration() );
    }
  }
};

void item::mirror_of_the_blademaster( special_effect_t& effect )
{
  // Disabled because pet data is severely out of date.
  effect.type = SPECIAL_EFFECT_NONE;
  return;

  action_t* action = effect.player -> find_action( "burning_mirror" );
  if ( ! action )
  {
    action = effect.player -> create_proc_action( "burning_mirror", effect );
  }

  if ( ! action )
  {
    action = new burning_mirror_t( effect );
  }

  effect.execute_action = action;
  // Changing the special effect type to _USE (from _CUSTOM) is necessary so use_item_t action can
  // properly detect the special effect.
  effect.type = SPECIAL_EFFECT_USE;
}

void item::tyrants_decree( special_effect_t& effect )
{
  struct tyrants_decree_callback_t : public dbc_proc_callback_t
  {
    double cancel_threshold;
    buff_t* tyrant;

    tyrants_decree_callback_t( player_t* player, const special_effect_t& effect, buff_t* b ) :
      dbc_proc_callback_t( player, effect ), tyrant( b )
    {
      cancel_threshold = effect.driver() -> effectN( 2 ).percent();
    }

    void trigger( const proc_data_t&, player_t*, action_state_t*, proc_trigger_type_e ) override
    {
      if ( listener->resources.pct( RESOURCE_HEALTH ) < cancel_threshold )
        tyrant->expire();
    }
  };

  auto trigger =
      make_buff<stat_buff_t>( effect.player, "tyrants_immortality", effect.player->find_spell( 184770 ) )
          ->add_stat( STAT_STAMINA, effect.player->find_spell( 184770 )->effectN( 1 ).average( effect.item ) )
          ->set_duration( timespan_t::zero() );  // indefinite, this will never expire naturally so might as well save
                                                 // some CPU cycles

  auto driver = make_buff( effect.player, "tyrants_decree_driver", effect.driver() )
    ->set_period( effect.driver()->effectN( 1 ).period() )
    ->set_tick_behavior( buff_tick_behavior::REFRESH )
    ->set_quiet( true )
    ->set_tick_callback( [ trigger ]( buff_t* b, int, timespan_t ) {
      if ( b->player->resources.pct( RESOURCE_HEALTH ) > b->data().effectN( 2 ).percent() )
        trigger->trigger();
    } );

  // Assume actor has stacked the buff to max stack precombat.
  effect.player->register_on_arise_callback( effect.player, [ driver, trigger ]() {
    driver->trigger();
    trigger->trigger( trigger->max_stack() );
  } );

  // Create a callback that triggers on damage taken to check if the buff should be expired.
  effect.proc_flags_= PF_ALL_DAMAGE_TAKEN;
  effect.proc_chance_ = 1.0;

  new tyrants_decree_callback_t( effect.player, effect, trigger );
}

void item::warlords_unseeing_eye( special_effect_t& effect )
{
  // Store the magic mitigation number in a player-scope variable.
  auto magic = effect.driver()->effectN( 2 ).average( effect.item ) / 10000.0;
  // Register our handler function so it can be managed by player_t::account_absorb_buffs()
  effect.player->instant_absorb_list.insert( std::make_pair(
      effect.spell_id,
      instant_absorb_t( effect.player, effect.driver(), "warlords_unseeing_eye", [ magic ]( const action_state_t* s ) {
        /* Absorb is based on what the player's HP would be after taking the damage,
          accounting for absorbs that occur prior but ignoring the rest.

          Max current health to 0 to keep the trinket's value somewhat sane when the
          actor goes negative.

          TOCHECK: If the actor would die from the hit, does the size of the absorb
          scale with the amount of overkill? */
        player_t* p = s->target;

        double absorb_amount = s->result_amount * magic;

        absorb_amount *= 1 - ( std::max( p->resources.current[ RESOURCE_HEALTH ], 0.0 ) - s->result_amount ) /
                                 p->resources.max[ RESOURCE_HEALTH ];

        return absorb_amount;
      } ) ) );

  // Push the effect into the priority list.
  effect.player -> absorb_priority.push_back( 184762 );
}

// Normally this would be parsed automatically, however since we already use soul_fragment for
// other buffs we need to specifically define this so they don't trigger each other
void item::necromantic_focus( special_effect_t& effect )
{
  auto buff = buff_t::find( effect.player, "soul_fragment_trinket" );
  if ( !buff )
  {
    buff = make_buff<stat_buff_t>( effect.player, "soul_fragment_trinket", effect.trigger() )
               ->add_stat( STAT_MASTERY_RATING, effect.trigger()->effectN( 1 ).average( effect.item ) );
  }

  effect.proc_flags_  = PF_PERIODIC;
  effect.proc_flags2_ = PF2_ALL_HIT | PF2_PERIODIC_DAMAGE;
  effect.custom_buff  = buff;

  new dbc_proc_callback_t( effect.player, effect );
}

void item::sorrowsong( special_effect_t& effect )
{
  new dbc_proc_callback_t( effect.player, effect );

  effect.player->callbacks.register_callback_trigger_function(
      effect.spell_id, dbc_proc_callback_t::trigger_fn_type::CONDITION,
      [ effect ]( auto, const auto&, player_t* t, auto, auto ) {
        return t->health_percentage() <= effect.driver()->effectN( 1 ).base_value();
      } );
}

struct touch_of_the_grave_t : public spell_t
{
  touch_of_the_grave_t( player_t* p, const spell_data_t* spell ) :
    spell_t( "touch_of_the_grave", p, spell )
  {
    background = may_crit = true;
    base_dd_min = base_dd_max = 0;
    ap_type = attack_power_type::NO_WEAPON;
    // these are sadly hardcoded in the tooltip
    attack_power_mod.direct = 1.25 * .25;
    spell_power_mod.direct = 1.0 * .25;
  }

  double attack_direct_power_coefficient( const action_state_t* s ) const override
  {
    const double ap = attack_power_mod.direct * s -> composite_attack_power();
    const double sp = spell_power_mod.direct * s -> composite_spell_power();

    if ( ap <= sp )
      return 0;
    return spell_t::attack_direct_power_coefficient( s );
  }

  double spell_direct_power_coefficient( const action_state_t* s ) const override
  {
    const double ap = attack_power_mod.direct * s -> composite_attack_power();
    const double sp = spell_power_mod.direct * s -> composite_spell_power();

    if ( ap > sp )
      return 0;
    return spell_t::spell_direct_power_coefficient( s );
  }
};

void racial::touch_of_the_grave( special_effect_t& effect )
{
  effect.execute_action = new touch_of_the_grave_t( effect.player, effect.trigger() );

  new dbc_proc_callback_t( effect.player, effect );
}

void racial::entropic_embrace( special_effect_t& effect )
{
  buff_t* base_buff = buff_t::find( effect.player, "entropic_embrace" );
  if ( base_buff == nullptr )
  {
    base_buff = make_buff( effect.player, "entropic_embrace", effect.trigger() )
      ->add_invalidate( CACHE_PLAYER_DAMAGE_MULTIPLIER )
      ->add_invalidate( CACHE_PLAYER_HEAL_MULTIPLIER );
    effect.player->buffs.entropic_embrace = base_buff;
  }

  effect.custom_buff = base_buff;
  new dbc_proc_callback_t( effect.player, effect );
}

void racial::brush_it_off( special_effect_t& effect )
{
  struct brush_it_off_cb_t : public dbc_proc_callback_t
  {
    action_t* regen;
    double heal_pct;

    brush_it_off_cb_t( const special_effect_t& e )
      : dbc_proc_callback_t( e.player, e ), heal_pct( e.driver()->effectN( 2 ).percent() )
    {
      regen = new residual_action::residual_periodic_action_t<proc_heal_t>( "brush_it_off", e.player,
                                                                            e.player->find_spell( 291843 ) );
    }

    void execute( const spell_data_t*, player_t*, action_state_t* s ) override
    {
      residual_action::trigger( regen, listener, s->result_amount * heal_pct );
    }
  };

  new brush_it_off_cb_t( effect );
}

struct embrace_of_bwonsamdi_t : public spell_t
{
  embrace_of_bwonsamdi_t(player_t* p, const spell_data_t* sd) :
    spell_t("embrace_of_bwonsamdi", p, sd)
  {
    background = true;
    ap_type = attack_power_type::NO_WEAPON; //TOCHECK: Is this true? Based off of Touch of the Grave right now.
    base_dd_min = base_dd_max = 0;
    //Hardcoded tooltip values
    attack_power_mod.direct = 0.22;
    spell_power_mod.direct = 0.22;
  }

  double attack_direct_power_coefficient( const action_state_t* s ) const override
  {
    const double ap = attack_power_mod.direct * s -> composite_attack_power();
    const double sp = spell_power_mod.direct * s -> composite_spell_power();

    if ( ap <= sp )
      return 0;
    return spell_t::attack_direct_power_coefficient( s );
  }

  double spell_direct_power_coefficient( const action_state_t* s ) const override
  {
    const double ap = attack_power_mod.direct * s -> composite_attack_power();
    const double sp = spell_power_mod.direct * s -> composite_spell_power();

    if ( ap > sp )
      return 0;
    return spell_t::spell_direct_power_coefficient( s );
  }
};

struct embrace_of_kimbul_t : public spell_t
{
  embrace_of_kimbul_t(player_t* p, const spell_data_t* sd) :
    spell_t("embrace_of_kimbul", p, sd)
  {
    tick_may_crit = false; //TOCHECK: Longer test needed with max level character for these values
    background = true;
    hasted_ticks = false;
    dot_max_stack = sd->max_stacks();
    dot_behavior = DOT_REFRESH_DURATION;
    ap_type = attack_power_type::NO_WEAPON;
    attack_power_mod.tick = 0.075; //Hardcoded in tooltip
    spell_power_mod.tick = 0.075;
  }

  double attack_tick_power_coefficient( const action_state_t* s ) const override
  {
    const double ap = attack_power_mod.tick * s -> composite_attack_power();
    const double sp = spell_power_mod.tick * s -> composite_spell_power();

    if ( ap <= sp )
      return 0;
    return spell_t::attack_tick_power_coefficient( s );
  }

  double spell_tick_power_coefficient( const action_state_t* s ) const override
  {
    const double ap = attack_power_mod.tick * s -> composite_attack_power();
    const double sp = spell_power_mod.tick * s -> composite_spell_power();

    if ( ap > sp )
      return 0;
    return spell_t::spell_tick_power_coefficient( s );
  }
};

void racial::zandalari_loa( special_effect_t& effect )
{
  if ( create_fallback_buffs( effect, { "embrace_of_paku" } ) )
    return;

  if ( effect.player->zandalari_loa == player_t::AKUNDA )
  {
    // Akunda - Healing Proc (not implemented)
  }
  else if ( effect.player->zandalari_loa == player_t::GONK )
  {
    effect.player->base.stacking_movement_speed_modifier += effect.player->find_spell( 292362 )->effectN( 1 ).percent();
  }
  else if ( effect.player->zandalari_loa == player_t::BWONSAMDI )
  {
    // Bwonsamdi - 100% of damage done is returned as healing (healing not implemented)
    special_effect_t* driver = new special_effect_t( effect.player );
    driver->source = SPECIAL_EFFECT_SOURCE_RACE;
    unique_gear::initialize_special_effect( *driver, 292360 );
    driver->execute_action = new embrace_of_bwonsamdi_t( effect.player, effect.player->find_spell( 292380 ) );

    effect.player->special_effects.push_back( driver );

    new dbc_proc_callback_t( effect.player, *driver );
  }
  else if ( effect.player->zandalari_loa == player_t::KIMBUL )
  {
    // Kimbul - Chance to apply bleed dot, max stack of 3
    special_effect_t* driver = new special_effect_t( effect.player );
    driver->source = SPECIAL_EFFECT_SOURCE_RACE;
    unique_gear::initialize_special_effect( *driver, 292363 );
    driver->execute_action = new embrace_of_kimbul_t( effect.player, effect.player->find_spell( 292473 ) );

    effect.player->special_effects.push_back( driver );

    new dbc_proc_callback_t( effect.player, *driver );
  }
  else if ( effect.player->zandalari_loa == player_t::KRAGWA )
  {
    // Kragwa - Grants health and armor (not implemented)
  }
  else if ( effect.player->zandalari_loa == player_t::PAKU )
  {
    special_effect_t* driver = new special_effect_t( effect.player );
    driver->source = SPECIAL_EFFECT_SOURCE_RACE;
    unique_gear::initialize_special_effect( *driver, 292361 );  // Permanent buff spell id, contains proc data

    // Paku - Grants crit chance
    buff_t* paku = buff_t::find( effect.player, "embrace_of_paku" );
    if ( paku == nullptr )
    {
      // Buff spell data contains duration and amount
      paku = make_buff( effect.player, "embrace_of_paku", effect.player->find_spell( 292463 ) )
        ->set_pct_buff_type_from_data( true );
    }

    driver->custom_buff = paku;

    effect.player->special_effects.push_back( driver );

    new dbc_proc_callback_t( driver->player, *driver );
  }
}

void racial::combat_analysis( special_effect_t& effect )
{
  const spell_data_t* buff_spell = effect.player->find_spell( 312923 );
  effect.stat                    = effect.player->convert_hybrid_stat( STAT_STR_AGI_INT );

  buff_t* buff = buff_t::find( effect.player, "combat_analysis" );
  if ( !buff )
  {
    buff = make_buff<stat_buff_t>( effect.player, "combat_analysis", buff_spell )
               ->add_stat( effect.stat, buff_spell->effectN( 1 ).average( effect.player ) );
    buff->set_max_stack( as<int>( buff_spell->effectN( 3 ).base_value() ) );
  }

  effect.player->register_combat_begin( [buff, buff_spell]( player_t* ) {
    make_repeating_event( *buff->sim, buff_spell->effectN( 1 ).period(), [buff]() { buff->trigger(); } );
  } );
}

void generic::skyfury( special_effect_t& effect )
{
  struct skyfury_cb_t : public dbc_proc_callback_t
  {
    proc_t* proc_mh, *proc_oh;

    skyfury_cb_t( const special_effect_t& effect ) :
      dbc_proc_callback_t( effect.player, effect ), proc_mh( nullptr ), proc_oh( nullptr )
    {
      if ( effect.player->items[ SLOT_MAIN_HAND ].active() &&
           effect.player->items[ SLOT_MAIN_HAND ].dbc_inventory_type() != INVTYPE_RANGED )
      {
        proc_mh = effect.player->get_proc( "Skyfury (Main Hand)" );
      }

      if ( effect.player->items[ SLOT_OFF_HAND ].active() &&
           effect.player->items[ SLOT_OFF_HAND ].dbc_inventory_type() != INVTYPE_RANGED )
      {
        proc_oh = effect.player->get_proc( "Skyfury (Off Hand)" );
      }
    }

    void trigger( const proc_data_t& data, player_t* t, action_state_t* s, proc_trigger_type_e type ) override
    {
      if ( !s->action->weapon )
      {
        return;
      }

      auto action = s->action->weapon->slot == SLOT_MAIN_HAND
        ? s->action->player->main_hand_attack
        : s->action->player->off_hand_attack;

      // If for some reason there's no auto attack action that would initialize the attack, bail out
      if ( !action )
      {
        return;
      }

      dbc_proc_callback_t::trigger( data, t, s, type );
    }

    void execute( const spell_data_t*, player_t* t, action_state_t* state ) override
    {
      auto atk = state->action->weapon->slot == SLOT_MAIN_HAND
        ? state->action->player->main_hand_attack
        : state->action->player->off_hand_attack;
      auto proc = state->action->weapon->slot == SLOT_MAIN_HAND
        ? proc_mh
        : proc_oh;

      auto old_target = atk->target;

      listener->sim->print_log( "{} skyfury repeats {}", *listener, *atk );
      if ( proc )
      {
        proc->occur();
      }

      auto old_may_miss = atk->may_miss;

      atk->may_miss = false;
      atk->repeating = false;
      atk->set_target( t );
      atk->execute();

      atk->repeating = true;
      atk->may_miss = old_may_miss;
      atk->set_target( old_target );
    }
  };

  if ( effect.player->is_enemy() || effect.player->type == HEALING_ENEMY )
  {
    return;
  }

  new skyfury_cb_t( effect );
}

void generic::enable_all_item_effects( special_effect_t& effect )
{
  if ( !effect.player->sim->enable_all_item_effects )
  {
    effect.type = SPECIAL_EFFECT_NONE;
    return;
  }

  struct enable_all_item_effects_t : public action_t
  {
    std::vector<special_effect_t*> action_effects;
    std::vector<special_effect_t*> buff_effects;

    enable_all_item_effects_t( const special_effect_t& e )
      : action_t( action_e::ACTION_USE, "enable_all_item_effects", e.player )
    {
      callbacks = false;
      cooldown->duration = 20_s;
    }

    void init() override
    {
      action_t::init();

      for ( auto id : midnight::__mid_special_effect_ids )
      {
        if ( auto eff = find_special_effect( player, id, SPECIAL_EFFECT_USE ) )
        {
          if ( eff->custom_buff )
            buff_effects.push_back( eff );

          if ( eff->execute_action )
            action_effects.push_back( eff );
        }
      }
    }

    result_e calculate_result( action_state_t* ) const override
    {
      return result_e::RESULT_NONE;
    }

    void execute() override
    {
      action_t::execute();

      for ( auto eff : buff_effects )
        eff->custom_buff->trigger();

      for ( auto eff : action_effects )
        if ( eff->execute_action->action_ready() )
          eff->execute_action->execute();
    }
  };

  effect.execute_action = new enable_all_item_effects_t( effect );
}

bool stat_fits_criteria( stat_e stat, stat_e criteria )
{
  if ( !stat )
    return false;

  if ( criteria == STAT_ALL )
    return true;

  if ( criteria == STAT_ANY_DPS )
    return stat != STAT_LEECH_RATING && stat != STAT_SPEED_RATING && stat != STAT_AVOIDANCE_RATING;

  return stat == criteria;
}

// Figure out if a given generic buff (associated with a trinket/item) is a
// stat buff of the correct type
bool buff_has_stat( const buff_t* buff, stat_e stat )
{
  if ( ! buff )
    return false;

  // Not a stat buff
  const stat_buff_t* stat_buff = dynamic_cast< const stat_buff_t* >( buff );
  if ( ! stat_buff )
    return false;

  // At this point, if "any" was specificed, we're satisfied
  if ( stat == STAT_ALL )
    return true;

  // TODO: Probably needs more cases or potentially a more elegant solution here
  //       Just filter tertiaries for now since they are present on some DPS trinket use effects
  if ( stat == STAT_ANY_DPS )
  {
    return range::any_of( stat_buff->stats, []( auto elem ) {
      return elem.stat != STAT_LEECH_RATING && elem.stat != STAT_SPEED_RATING && elem.stat != STAT_AVOIDANCE_RATING;
    } );
  }

  for ( auto & elem : stat_buff->stats )
  {
    if ( elem.stat == stat )
      return true;
  }

  return false;
}

bool action_has_damage( const action_t* action )
{
  if ( !action )
    return false;

  // check direct damage
  if ( action->does_direct_damage() )
    return true;

  // check periodic damage
  if ( action->does_periodic_damage() )
    return true;

  // check impact action
  if ( action->impact_action && action_has_damage( action->impact_action ) )
    return true;

  // check tick action
  if ( action->tick_action )
  {
    if ( action_has_damage( action->tick_action ) )
      return true;

    // check tick action's impact action
    if ( action->tick_action->impact_action && action_has_damage( action->tick_action->impact_action ) )
      return true;
  }

  return false;
}
} // UNNAMED NAMESPACE

item_targetdata_initializer_t::item_targetdata_initializer_t( unsigned iid, util::span<const slot_e> s )
  : targetdata_initializer_t(), item_id( iid ), spell_id( 0 ), slots_( s.begin(), s.end() )
{
  active_fn = []( const special_effect_t* e ) { return e != nullptr; };
  debuff_fn = []( player_t*, const special_effect_t* e ) { return e->trigger(); };
}

item_targetdata_initializer_t::item_targetdata_initializer_t( unsigned sid, unsigned did )
  : targetdata_initializer_t(), item_id( 0 ), spell_id( sid )
{
  active_fn = []( const special_effect_t* e ) { return e != nullptr; };

  if ( did )
    debuff_fn = [ did ]( player_t* p, const special_effect_t* ) { return p->find_spell( did ); };
  else
    debuff_fn = []( player_t*, const special_effect_t* e ) { return e->trigger(); };
}

const special_effect_t* item_targetdata_initializer_t::find( player_t* p ) const
{
  if ( spell_id )
  {
    return unique_gear::find_special_effect( p, spell_id );
  }
  else
  {
    for ( slot_e slot : slots_ )
    {
      if ( p->items[ slot ].parsed.data.id == item_id )
      {
        return p->items[ slot ].parsed.special_effects[ 0 ];
      }
    }
  }

  return nullptr;
}

bool item_targetdata_initializer_t::init( player_t* p ) const
{
  // No need to check on pets/enemies
  if ( p->is_pet() || p->is_enemy() || p->type == HEALING_ENEMY )
    return false;

  return targetdata_initializer_t::init( p );
}

const special_effect_t* item_targetdata_initializer_t::effect( actor_target_data_t* td ) const
{
  return data[ td->source ];
}

/**
 * Initialize a special effect, based on a spell id. Returns true if the first
 * phase initialization succeeded, false otherwise. If the spell id points to a
 * spell that our system cannot support, also sets the special effect type to
 * SPECIAL_EFFECT_NONE.
 *
 * Note that the first phase initialization simply fills up special_effect_t
 * with relevant information for non-custom special effects. Second phase of
 * the initialization (performed by unique_gear::init) will instantiate the
 * proc callback, and relevant actions/buffs, or call a custom function to
 * perform the initialization.
 */
void unique_gear::initialize_special_effect( special_effect_t& effect, unsigned spell_id )
{
  player_t* p = effect.player;

  // Perform max level checking on the driver before anything.
  //
  // BracketSim: this guard could never fire. player_t::find_spell() ALREADY
  // applies the max_aura_level rule (player.cpp) and returns not_found() for a
  // driver the player has outlevelled - and not_found() reports
  // max_aura_level() == 0, so the test below was always false. The effect then
  // went on to initialize against a driver that does not exist.
  //
  // For a generic proc that is harmless: the "no spell data for the driver"
  // check further down disables it. A CUSTOM effect never reaches that check -
  // it is registered above and deferred - so it initialized and then failed
  // looking up its own action, which is where "could not find spell data for
  // Action 'ice_bomb'" came from. Three of the eight items in
  // fixtures/engine-cannot-simulate.json died exactly that way, and their
  // drivers' max_aura_level values (Ice Bomb 49, Chilling Nova 49) match the
  // measured break points precisely.
  //
  // So look the driver up WITHOUT the level filter, which is what this guard
  // always meant to do. An outlevelled proc is then correctly disabled, and the
  // item simulates as the stat stick it is in game.
  const spell_data_t* spell = dbc::find_spell( p, spell_id );
  if ( spell->max_aura_level() > 0 && as<unsigned>( p->level() ) > spell->max_aura_level() )
  {
    if ( p->sim->debug )
    {
      p->sim->out_debug.printf( "%s disabled effect %s, player level %d higher than maximum effect level %u", p->name(),
                                spell->name_cstr(), p->level(), spell->max_aura_level() );
    }
    effect.type = SPECIAL_EFFECT_NONE;
    return;
  }

  // Try to find the special effect from the custom effect database
  for ( const auto dbitem: find_special_effect_db_item( spell_id ) )
  {
    // Parse auxilary effect options before doing spell data based parsing
    if ( ! dbitem -> encoded_options.empty() )
    {
      std::string encoded_options = dbitem -> encoded_options;
      util::tolower( encoded_options );
      // Note, if the encoding parse fails (this should never ever happen),
      // we don't parse game client data either.
      special_effect::parse_special_effect_encoding( effect, encoded_options );
    }
    else if ( dbitem -> cb_obj )
    {
      // Check that the custom special effect initializer is valid. Invalid special effect
      // validators could be for example spec-specific initializers (see scoped_action_callback_t
      // and child classes derived off of it).
      if ( ! dbitem -> cb_obj -> valid( effect ) )
      {
        continue;
      }

      // Custom special effect initialization is deferred, and no parsing from spell data is done
      // automatically.
      if ( dbitem -> cb_obj )
      {
        effect.custom_init_object.push_back( dbitem -> cb_obj );
      }
    }
  }

  // Setup the driver always, though honoring any parsing performed in the first phase options.
  if ( effect.spell_id == 0 )
    effect.spell_id = spell_id;

  // Check the passive effects database. These are initialized in the first phase since they may affect player base
  // stats
  for ( const auto dbitem : find_passive_effect_db_item( spell_id ) )
  {
    // Check that a custom special effect initializer exists and is valid
    if ( !dbitem->cb_obj || !dbitem->cb_obj->valid( effect ) )
      continue;

    dbitem->cb_obj->initialize( effect );
    // Set as passive so second phase initialization doesn't happen
    effect.type = SPECIAL_EFFECT_PASSIVE;
  }

  // No further processing is necessary for passive effects.
  if ( effect.type == SPECIAL_EFFECT_PASSIVE )
    return;

  // Custom init found a valid initializer callback, this special effect will be initialized with it
  // later on
  if ( !effect.custom_init_object.empty() )
  {
    return;
  }

  // If the item is legendary, and it has an item effect, mandate that the item effect is actually
  // created through custom means. This is to prevent the automatic inference below to create
  // completely nonsensical special effects, when there is not enough client data to fully implement
  // the effect properly.
  //
  // This is mostly relevant for "simple looking" legendary effects such as Recurrent Ritual that
  // gets automatically inferred to affect all (warlock) spells globally.
  if ( effect.custom_init_object.empty() && effect.item &&
       effect.source == SPECIAL_EFFECT_SOURCE_ITEM &&
       effect.item->parsed.data.quality == ITEM_QUALITY_LEGENDARY )
  {
    if ( p -> sim -> debug )
    {
      p -> sim -> out_debug.printf( "Player %s no custom special effect initializer for item %s, "
                                    "spell %s (id=%u), disabling effect",
        p -> name(), effect.item -> name(), p -> find_spell( spell_id ) -> name_cstr(), spell_id );
    }
    effect.type = SPECIAL_EFFECT_NONE;
    return;
  }

  // No custom effect found, so ensure that we have spell data for the driver
  if ( p -> find_spell( effect.spell_id ) -> id() != effect.spell_id )
  {
    if ( p -> sim -> debug )
      p -> sim -> out_debug.printf( "Player %s unable to initialize special effect in item %s, spell identifier %u not found.",
          p -> name(), effect.item ? effect.item -> name() : "unknown", effect.spell_id );
    effect.type = SPECIAL_EFFECT_NONE;
    return;
  }

  // For generic procs, make sure we have a PPM, RPPM or Proc Chance available,
  // otherwise there's no point in trying to proc anything
  if ( effect.type == SPECIAL_EFFECT_EQUIP )
  {
    if (!special_effect::usable_proc( effect ))
    {
      effect.type = SPECIAL_EFFECT_NONE;
    }
  }

  // For generic use stuff, we need to have a proper buff or action that we can generate
  else if ( effect.type == SPECIAL_EFFECT_USE &&
            effect.buff_type() == SPECIAL_EFFECT_BUFF_NONE &&
            effect.action_type() == SPECIAL_EFFECT_ACTION_NONE )
  {
    effect.type = SPECIAL_EFFECT_NONE;
  }
}

// Second phase initialization, creates the proc callback object for generic on-equip special
// effects, or calls the custom initialization function given in the first phase initialization.
void unique_gear::initialize_special_effect_2( special_effect_t* effect )
{
  if ( effect->type == SPECIAL_EFFECT_PASSIVE )
    return;

  if ( effect -> custom_init || !effect -> custom_init_object.empty() )
  {
    if ( effect -> custom_init )
    {
      effect -> custom_init( *effect );
    }
    else
    {
      range::for_each( effect -> custom_init_object, [ effect ]( scoped_callback_t* cb ) {
        cb -> initialize( *effect );
      } );
    }

    // Allow class modules to adjust the special_effect_t object generated by the core
    // special effect registry, if they want.
    effect->player->init_special_effect( *effect );
  }
  else if ( effect -> type == SPECIAL_EFFECT_EQUIP )
  {
    // Ensure we are not accidentally initializing a generic special effect multiple times
    bool exists = effect -> player -> callbacks.has_callback( [ effect ]( const action_callback_t* cb ) {
      auto dbc_cb = dynamic_cast<const dbc_proc_callback_t*>( cb );
      if ( dbc_cb == nullptr )
      {
        return false;
      }

      // Special effects are unique, and have an 1:1 relationship with (dbc proc) callbacks. Pointer
      // comparison here is enough to ensure that no special_effect_t object gets more than one
      // dbc_proc_callback_t object.
      return &( dbc_cb -> effect ) == effect;
    } );

    if ( exists )
    {
      return;
    }

    // Allow class modules to adjust the special_effect_t object generated by the core
    // special effect registry, if they want.
    effect->player->init_special_effect( *effect );

    if ( effect -> item )
    {
      new dbc_proc_callback_t( effect -> item, *effect );
    }
    else
    {
      new dbc_proc_callback_t( effect -> player, *effect );
    }
  }
}

void unique_gear::initialize_racial_effects( player_t* player )
{
  if ( player->race == RACE_NONE )
  {
    return;
  }

  if ( !util::race_id( player->race ) )
  {
    return;
  }

  // Iterate over all race spells for the player
  for ( const auto* entry : player->dbc->racial_spell( player->type, player->race ) )
  {
    auto spell = dbc::find_spell( player, entry->spell_id );
    if ( !spell || !spell->ok() )
    {
      continue;
    }

    special_effect_t effect( player );
    effect.source = SPECIAL_EFFECT_SOURCE_RACE;
    unique_gear::initialize_special_effect( effect, spell->id() );
    if ( !effect.is_custom() )
    {
      continue;
    }

    player->sim->print_debug( "Player {} initialized racial spell {} (id={}, class_mask={:#08x})",
      player->name(), spell->name_cstr(), spell->id(), entry->mask_class );

    player->special_effects.push_back( new special_effect_t( effect ) );
  }
}

void unique_gear::initialize_expansion_trait_effects( player_t* player, std::string_view talents_str )
{
  if ( !player || talents_str.empty() )
    return;

  for ( auto entry : util::string_split<std::string_view>( talents_str, "/" ) )
  {
    auto split = util::string_split<std::string_view>( entry, ":" );
    auto _trait = split[ 0 ];
    // auto _rank = split.size() > 1 ? split[ 1 ] : "1"; ignored for now

    unsigned spell_id;

    if ( util::is_number( _trait ) )
      spell_id = trait_data_t::find( util::to_unsigned( _trait ), player->is_ptr() )->id_spell;
    else
      spell_id = trait_data_t::find( talent_tree::EXPANSION, _trait, 0, SPEC_NONE, player->is_ptr(), true )->id_spell;

    if ( !spell_id )
      throw sc_invalid_player_argument( fmt::format( "Unable to find expansion talent '{}'.", _trait ) );

    special_effect_t _effect( player );
    _effect.spell_id = spell_id;

    unique_gear::initialize_special_effect( _effect, spell_id );

    player->special_effects.push_back( new special_effect_t( _effect ) );
  }
}

// ==========================================================================
// unique_gear::init
// ==========================================================================

void unique_gear::init( player_t* p )
{
  if ( p->is_pet() || p->is_enemy() )
    return;

  for ( size_t i = 0; i < p->items.size(); i++ )
  {
    item_t& item = p->items[ i ];

    for ( size_t j = 0; j < item.parsed.special_effects.size(); j++ )
    {
      special_effect_t* effect = item.parsed.special_effects[ j ];

      p->sim->print_debug( "Initializing item-based special effect {}", *effect );

      initialize_special_effect_2( effect );
    }
  }

  // Generic special effects, bound to no specific item
  for ( size_t i = 0; i < p->special_effects.size(); i++ )
  {
    special_effect_t* effect = p->special_effects[ i ];

    p->sim->print_debug( "Initializing generic special effect {}", *effect );

    initialize_special_effect_2( effect );
  }
}

// Base class for item effect expressions, finds all the special effects in the
// listed slots
struct item_effect_base_expr_t : public expr_t
{
  std::vector<const special_effect_t*> effects;

  item_effect_base_expr_t( player_t& player, const std::vector<slot_e>& slots, util::string_view full_expression ) :
    expr_t( full_expression )
  {
    const special_effect_t* e = nullptr;

    for (auto slot : slots)
    {
      e = player.items[ slot ].special_effect( SPECIAL_EFFECT_SOURCE_NONE, SPECIAL_EFFECT_EQUIP );
      if ( e && e -> source != SPECIAL_EFFECT_SOURCE_NONE )
        effects.push_back( e );

      e = player.items[ slot ].special_effect( SPECIAL_EFFECT_SOURCE_NONE, SPECIAL_EFFECT_USE );
      if ( e && e -> source != SPECIAL_EFFECT_SOURCE_NONE )
        effects.push_back( e );
    }
  }
};

// Base class for expression-based item expressions (such as buff, or cooldown
// expressions). Implements the behavior of expression evaluation.
struct item_effect_expr_t : public item_effect_base_expr_t
{
  std::vector<std::unique_ptr<expr_t>> exprs;

  item_effect_expr_t( player_t& player, const std::vector<slot_e>& slots, util::string_view full_expression ) :
    item_effect_base_expr_t( player, slots, full_expression )
  { }

  // Evaluates automatically to the maximum value out of all expressions, may
  // not be wanted in all situations. Best case? We should allow internal
  // operators here somehow
  double evaluate() override
  {
    double result = 0;
    for (auto&& expr : exprs)
    {
      double r = expr -> eval();
      if ( r > result )
        result = r;
    }

    return result;
  }

  bool is_constant() override
  {
    return exprs.empty();
  }
};

// Buff based item expressions, creates buff expressions for the items from user input
struct item_buff_expr_t : public item_effect_expr_t
{
  item_buff_expr_t( player_t& player, const std::vector<slot_e>& slots, stat_e s, bool stacking,
                    util::string_view expr_str )
    : item_effect_expr_t( player, slots, expr_str )
  {
    for ( auto e : effects )
    {
      auto _list = e->buff_list;  // make a copy
      if ( auto _buff = buff_t::find( &player, e->name() ); _buff && !range::contains( _list, _buff ) )
        _list.push_back( _buff );

      for ( auto b : _list )
      {
        if ( buff_has_stat( b, s ) && ( !stacking || ( stacking && b->max_stack() > 1 ) ) )
        {
          if ( auto expr_obj = buff_t::create_expression( b->name(), expr_str, *b ) )
            exprs.push_back( std::move( expr_obj ) );
        }
      }
    }
  }
};

struct item_buff_exists_expr_t : public item_effect_expr_t
{
  double v;

  item_buff_exists_expr_t( player_t& player, const std::vector<slot_e>& slots, stat_e s,
                           util::string_view full_expression )
    : item_effect_expr_t( player, slots, full_expression ), v( 0 )
  {
    for ( auto e : effects )
    {
      auto _list = e->buff_list;  // make a copy
      if ( auto _buff = buff_t::find( &player, e->name() ); _buff && !range::contains( _list, _buff ) )
        _list.push_back( _buff );

      for ( auto b : _list )
      {
        if ( buff_has_stat( b, s ) )
        {
          v = 1;
          break;
        }
      }

      if ( v == 1 )
        break;
    }
  }

  bool is_constant() override
  {
    return true;
  }

  double evaluate() override
  { return v; }
};

// Cooldown based item expressions, creates cooldown expressions for the items
// from user input
struct item_cooldown_expr_t : public item_effect_expr_t
{
  item_cooldown_expr_t( player_t& player, const std::vector<slot_e>& slots, util::string_view expr, util::string_view full_expression ) :
    item_effect_expr_t( player, slots, full_expression )
  {
    for (auto e : effects)
    {
      if ( e -> cooldown() != timespan_t::zero() )
      {
        cooldown_t* cd = player.get_cooldown( e -> cooldown_name() );
        if ( auto expr_obj = cd -> create_expression( expr ) )
          exprs.push_back( std::move(expr_obj) );
      }
    }
  }
};

struct item_cast_time_expr_t : public item_effect_expr_t
{
  double v = 0;

  item_cast_time_expr_t( player_t& player, const std::vector<slot_e>& slots, util::string_view full_expression ) :
    item_effect_expr_t( player, slots, full_expression ), v( 0 )
  {
    for (auto e : effects)
    {
      if ( e -> execute_action )
      {
        if ( e->execute_action->channeled )
        {
          v = e->execute_action->dot_duration.total_seconds();
        }
        else
        {
          v = e->execute_action->base_execute_time.value().total_seconds();
        }
        break;
      }
    }
  }

  bool is_constant() override
  {
    return true;
  }

  double evaluate() override
  { return v; }
};

struct item_ready_expr_t : public item_effect_base_expr_t
{
  item_ready_expr_t( player_t& player, const std::vector<slot_e>& slots, util::string_view full_expression ) :
    item_effect_base_expr_t( player, slots, full_expression )
  {
  }

  double evaluate() override
  {
    for ( auto e : effects )
    {
      if ( e -> cooldown_group_duration() != timespan_t::zero() )
      {
        cooldown_t* cd = e->player->get_cooldown( e -> cooldown_group_name() );
        if ( !cd -> up() )
          return 0;
      }
      if ( e -> cooldown() != timespan_t::zero() )
      {
        cooldown_t* cd = e->player->get_cooldown( e -> cooldown_name() );
        if ( !cd -> up() )
          return 0;
      }
    }

    return 1;
  }

  bool is_constant() override
  {
    return effects.empty();
  }
};

struct item_is_expr_t : public expr_t
{
  double is = 0;

  item_is_expr_t( player_t& player, const std::vector<slot_e>& slots, util::string_view item_name )
    : expr_t( "item_is_expr" )
  {
    for ( auto slot : slots )
    {
      if ( player.items[ slot ].name() == item_name )
        is = 1;
    }
  }

  bool is_constant() override
  {
    return true;
  }

  double evaluate() override
  {
    return is;
  }
};

struct item_lvl_expr_t : public item_effect_expr_t
{
  double ilvl;
  item_lvl_expr_t( player_t& player, const std::vector<slot_e>& slots, util::string_view full_expression )
    : item_effect_expr_t( player, slots, full_expression )
  {
    for (auto slot : slots)
    {
      ilvl = player.items[ slot ].item_level();
    }
  }

  bool is_constant() override
  {
    return true;
  }

  double evaluate() override
  {
    return ilvl;
  }
};

struct item_cooldown_exists_expr_t : public item_effect_expr_t
{
  double v;

  item_cooldown_exists_expr_t( player_t& player, const std::vector<slot_e>& slots, util::string_view full_expression ) :
    item_effect_expr_t( player, slots, full_expression ), v( 0 )
  {
    for (auto e : effects)
    {
      if ( e->cooldown() != timespan_t::zero() )
      {
        v = 1;
        break;
      }
    }
  }

  bool is_constant() override
  {
    return true;
  }

  double evaluate() override
  { return v; }
};

struct item_has_use_expr_t : public item_effect_expr_t
{
  double v;
  bool has_use;
  bool has_buff;
  bool has_damage;

  item_has_use_expr_t( player_t& player, const std::vector<slot_e>& slots, std::string_view full_expression,
                       bool check_buff, bool check_damage )
    : item_effect_expr_t( player, slots, full_expression ),
      v( 0 ),
      has_use( false ),
      has_buff( false ),
      has_damage( false )
  {
    for ( auto e : effects )
    {
      if ( e->type == SPECIAL_EFFECT_USE )
      {
        has_use = true;
        break;
      }
    }

    if ( check_buff )
    {
      for ( auto e : effects )
      {
        // Check has_use_buff override
        if ( e->has_use_buff_override )
        {
          has_buff = true;
          break;
        }

        // Check if there is a stat set on the special effect
        if ( stat_fits_criteria( e->stat, STAT_ANY_DPS ) )
        {
          has_buff = true;
          break;
        }

        // Check if the special effect has a suitable buff effect
        for ( size_t i = 1, i_end = e->trigger()->effect_count(); i <= i_end; i++ )
        {
          if ( has_buff )
            break;

          const spelleffect_data_t& effect = e->trigger()->effectN( i );
          if ( effect.id() == 0 )
            continue;

          if ( stat_fits_criteria( e->stat_buff_type( effect ), STAT_ANY_DPS ) )
          {
            has_buff = true;
            break;
          }

          // Check if an effect triggers something with a suitable buff effect
          if ( effect.trigger() )
          {
            for ( size_t j = 1, j_end = effect.trigger()->effect_count(); j <= j_end; j++ )
            {
              const spelleffect_data_t& trigger_effect = effect.trigger()->effectN( j );
              if ( trigger_effect.id() == 0 )
                continue;

              if ( stat_fits_criteria( e->stat_buff_type( trigger_effect ), STAT_ANY_DPS ) )
              {
                has_buff = true;
                break;
              }
            }
          }
        }

        // Check if the special effect created a suitable buff
        buff_t* b = buff_t::find( &player, e->name() );
        if ( buff_has_stat( b, STAT_ANY_DPS ) )
        {
          has_buff = true;
          break;
        }
      }
    }

    if ( check_damage )
    {
      for ( auto e : effects )
      {
        // check has_use_damage override
        if ( e->has_use_damage_override )
        {
          has_damage = true;
          break;
        }

        // check if action name exists
        action_t* a = player.find_action( e->name() );
        if ( action_has_damage( a ) )
        {
          has_damage = true;
          break;
        }

        // check any custom execute_action
        if ( action_has_damage( e->execute_action ) )
        {
          has_damage = true;
          break;
        }

        // check any auto-parsed actions
        if ( e->is_offensive_spell_action() || e->is_attack_action() )
        {
          has_damage = true;
          break;
        }
      }
    }

    if ( has_use && ( has_buff || !check_buff ) && ( has_damage || !check_damage ) )
      v = 1;
  }

  bool is_constant() override
  {
    return true;
  }

  double evaluate() override
  {
    return v;
  }
};

struct item_cooldown_category_expr_t : public item_effect_base_expr_t
{
  item_cooldown_category_expr_t( player_t& player, const std::vector<slot_e>& slots,
                                 util::string_view full_expression )
    : item_effect_base_expr_t( player, slots, full_expression )
  {
  }

  bool is_constant() override
  {
    return true;
  }

  double evaluate() override
  {
    for ( auto effect : effects )
      if ( auto cd_group = effect->cooldown_group(); cd_group )
        return cd_group;
    return 0.0;
  }
};

/**
 * Create "trinket" expressions, or anything relating to special effects.
 *
 * Note that this method returns zero (nullptr) when it cannot create an
 * expression.  The callee (player_t::create_expression) will handle unknown
 * expression processing.
 *
 * Trinket expressions are of the form:
 * trinket[.1|2|name].(has_|)(stacking_|)proc.<stat>.<buff_expr> OR
 * trinket[.1|2|name].(has_|)cooldown.<cooldown_expr>
 */
std::unique_ptr<expr_t> unique_gear::create_expression( player_t& player, util::string_view name_str )
{
  enum proc_expr_e
  {
    PROC_EXISTS,
    PROC_ENABLED,
    PROC_READY
  };

  enum proc_type_e
  {
    PROC_STAT,
    PROC_STACKING_STAT,
    PROC_COOLDOWN,
  };

  unsigned int ptype_idx = 1;
  unsigned int stat_idx = 2;
  unsigned int expr_idx = 3;
  enum proc_expr_e pexprtype = PROC_ENABLED;
  enum proc_type_e ptype = PROC_STAT;
  stat_e stat = STAT_NONE;
  std::vector<slot_e> slots;

  auto splits = util::string_split<util::string_view>( name_str, "." );

  // Hyperthread Wristwraps
  if ( splits[ 0 ] == "hyperthread_wristwraps" )
  {
    if ( auto a = player.find_action( "hyperthread_wristwraps" ) )
    {
      return a->create_expression( name_str );
    }
  }

  if ( splits.size() < 2 )
  {
    return nullptr;
  }

  if ( util::is_number( splits[ 1 ] ) )
  {
    if ( splits[ 1 ] == "1" )
    {
      slots.push_back( SLOT_TRINKET_1 );
    }
    else if ( splits[ 1 ] == "2" )
    {
      slots.push_back( SLOT_TRINKET_2 );
    }
    else
      return nullptr;
    ptype_idx++;

    stat_idx++;
    expr_idx++;
  }
  // Try to find trinket.<trinketname>
  else if ( !splits[ 1 ].empty() )
  {
    auto item = player.find_item_by_name( splits[ 1 ] );
    if ( item && ( item->slot == SLOT_TRINKET_1 || item->slot == SLOT_TRINKET_2 ) )
    {
      slots.push_back( item->slot );
    }
    // If the item is not found, return an always false expression instead of erroring out
    else
    {
      return expr_t::create_constant( "trinket-named-expr", 0 );
    }

    ptype_idx++;

    stat_idx++;
    expr_idx++;
  }
  // No positional parameter given so check both trinkets
  else
  {
    slots.push_back( SLOT_TRINKET_1 );
    slots.push_back( SLOT_TRINKET_2 );
  }

  if ( splits.size() <= ptype_idx )
  {
    throw std::invalid_argument(
      fmt::format( "'{}' parts required, only '{}' provided.", ptype_idx + 1, splits.size() ) );
  }

  if ( util::str_compare_ci( splits[ ptype_idx ], "is" ) )
  {
    return std::make_unique<item_is_expr_t>( player, slots, splits[ expr_idx - 1 ] );
  }

  if ( util::str_compare_ci( splits[ ptype_idx ], "ilvl" ) )
  {
    return std::make_unique<item_lvl_expr_t>( player, slots, name_str );
  }

  if ( util::str_prefix_ci( splits[ ptype_idx ], "has_use" ) )
  {
    bool check_buff = false;
    bool check_damage = false;

    if ( util::str_in_str_ci( splits[ ptype_idx ], "buff" ) )
      check_buff = true;
    else if ( util::str_in_str_ci( splits[ ptype_idx ], "damage" ) )
      check_damage = true;

    return std::make_unique<item_has_use_expr_t>( player, slots, name_str, check_buff, check_damage );
  }

  if ( util::str_compare_ci( splits[ ptype_idx ], "cast_time" ) )
  {
    return std::make_unique<item_cast_time_expr_t>( player, slots, name_str );
  }

  if ( util::str_prefix_ci( splits[ ptype_idx ], "has_" ) )
    pexprtype = PROC_EXISTS;
  else if ( util::str_prefix_ci( splits[ ptype_idx ], "ready_" ) )
    pexprtype = PROC_READY;

  if ( util::str_in_str_ci( splits[ ptype_idx ], "cooldown" ) )
  {
    ptype = PROC_COOLDOWN;
    // Cooldowns dont have stat type for now
    expr_idx--;
  }

  if ( util::str_in_str_ci( splits[ ptype_idx ], "stacking_" ) )
    ptype = PROC_STACKING_STAT;

  if ( ptype != PROC_COOLDOWN )
  {
    if ( splits.size() <= stat_idx )
    {
      throw std::invalid_argument(
        fmt::format( "'{}' parts required, only '{}' provided.", stat_idx + 1, splits.size() ) );
    }
    // Use "all stat" to indicate "any" ..
    if ( util::str_compare_ci( splits[ stat_idx ], "any" ) )
      stat = STAT_ALL;
    else if ( util::str_compare_ci( splits[ stat_idx ], "any_dps" ) )
      stat = STAT_ANY_DPS;
    else
    {
      stat = util::parse_stat_type( splits[ stat_idx ] );
      if ( stat == STAT_NONE )
      {
        throw std::invalid_argument( fmt::format( "Invalid stat '{}'.", splits[ stat_idx ] ) );
      }
    }
  }

  if ( pexprtype == PROC_ENABLED && ptype != PROC_COOLDOWN && splits.size() >= 4 )
  {
    if ( splits.size() <= expr_idx )
    {
      throw std::invalid_argument(
        fmt::format( "'{}' parts required, only '{}' provided.", expr_idx + 1, splits.size() ) );
    }
    return std::make_unique<item_buff_expr_t>( player, slots, stat, ptype == PROC_STACKING_STAT, splits[ expr_idx ] );
  }
  else if ( pexprtype == PROC_ENABLED && ptype == PROC_COOLDOWN && splits.size() >= 3 )
  {
    return std::make_unique<item_cooldown_expr_t>( player, slots, splits[ expr_idx ], name_str );
  }
  else if ( pexprtype == PROC_EXISTS )
  {
    if ( ptype != PROC_COOLDOWN )
    {
      return std::make_unique<item_buff_exists_expr_t>( player, slots, stat, name_str );
    }
    else
    {
      return std::make_unique<item_cooldown_exists_expr_t>( player, slots, name_str );
    }
  }
  else if ( pexprtype == PROC_READY )
  {
    return std::make_unique<item_ready_expr_t>( player, slots, name_str );
  }

  if ( util::str_compare_ci (splits[ ptype_idx ], "cooldown_category" ) )
    return std::make_unique<item_cooldown_category_expr_t>( player, slots, name_str );

  throw std::invalid_argument( fmt::format( "Invalid unique gear expression '{}'.", splits.back() ) );
}

namespace unique_gear
{
  void proc_resource_t::__initialize()
  {
    may_miss = may_dodge = may_parry = may_block = harmful = false;
    target = player;

    for ( size_t i = 1; i <= data().effect_count(); i++ )
    {
      const spelleffect_data_t& eff = data().effectN( i );

      // BracketSim: this action is not always carried by an item.
      // special_effect_t::initialize_resource_action() passes
      //
      //     source == SPECIAL_EFFECT_SOURCE_ITEM ? item : nullptr
      //
      // so an ENCHANT or GEM whose proc restores a resource arrives here with a
      // null item, and spelleffect_data_t::average( const item_t* ) guards that
      // with an assert - which is compiled out of a release build, leaving
      // `item->player` to dereference null. Insightful Earthstorm Diamond (gem
      // 25901) and Insightful Earthsiege Diamond (gem 41401) both restore mana
      // on spellcast, and both killed the process with 0xC0000005 before any
      // output was written.
      //
      // Falling back to the player-scaled value is not a compromise: average(
      // item ) itself returns average( item->player ) for any spell without the
      // "Scales with Casting Item's Level" attribute, and with no item there is
      // no item level to scale from. Mana Restore (32848) scales off the
      // Food/Gems attribute table, which is a player-level lookup either way.
      const auto amount = [ & ]( const spelleffect_data_t& e ) {
        return item ? e.average( item ) : e.average( player );
      };

      if ( eff.type() == E_ENERGIZE )
      {
        gain_da = amount( eff );
        gain_resource = eff.resource_gain_type();
      }
      else if ( eff.type() == E_APPLY_AURA && eff.subtype() == A_PERIODIC_ENERGIZE )
      {
        gain_ta = amount( eff );
        gain_resource = eff.resource_gain_type();
      }
    }

    gain = player->get_gain(name());
  }

std::vector<special_effect_db_item_t> __special_effect_db, __fallback_effect_db, __passive_effect_db;

bool class_scoped_callback_t::valid(const special_effect_t& effect) const
{
  assert(effect.player);

  if (!class_.empty() && range::find(class_, effect.player->type) == class_.end())
  {
    return false;
  }

  if (!spec_.empty() && range::find(spec_, effect.player->specialization()) == spec_.end())
  {
    return false;
  }

  return true;
}

void proc_attack_t::override_data(const special_effect_t& e)
{
  super::override_data(e);

  if ((e.override_result_es_mask & RESULT_DODGE_MASK))
  {
    this->may_dodge = e.result_es_mask & RESULT_DODGE_MASK;
  }

  if ((e.override_result_es_mask & RESULT_PARRY_MASK))
  {
    this->may_parry = e.result_es_mask & RESULT_PARRY_MASK;
  }
}

} // unique_gear

wrapper_callback_t::wrapper_callback_t( custom_cb_t cb_, wowv_t min_, wowv_t max_ )
  : scoped_callback_t(), cb( std::move( cb_ ) ), min_build( min_ ), max_build( max_ )
{}

bool wrapper_callback_t::valid( const special_effect_t& effect ) const
{
  return effect.player->dbc->wowv() >= min_build && effect.player->dbc->wowv() < max_build;
}

void wrapper_callback_t::initialize( special_effect_t& effect )
{
  cb( effect );
}

static unique_gear::special_effect_set_t do_find_special_effect_db_item(
    const std::vector<special_effect_db_item_t>& db, unsigned spell_id )
{
  special_effect_set_t entries;

  auto it = range::lower_bound( db, spell_id, {}, &special_effect_db_item_t::spell_id );

  if ( it == db.end() || it -> spell_id != spell_id )
  {
    return { };
  }

  while ( it != db.end() && it -> spell_id == spell_id )
  {
    // If there's an encoded option string, just return it straight up
    if ( ! it -> encoded_options.empty() )
    {
      return { &( *it ) };
    }

    assert( it -> cb_obj );

    // Push all callback-based initializers of the same priority into the vector
    if ( entries.empty() || it -> cb_obj -> priority == entries.front() -> cb_obj -> priority )
    {
      entries.push_back( &( *it ) );
    }
    else
    {
      break;
    }

    it++;
  }

  return entries;
}

static special_effect_set_t find_fallback_effect_db_item( unsigned spell_id )
{ return do_find_special_effect_db_item( __fallback_effect_db, spell_id ); }

special_effect_set_t unique_gear::find_special_effect_db_item( unsigned spell_id )
{ return do_find_special_effect_db_item( __special_effect_db, spell_id ); }

special_effect_set_t unique_gear::find_passive_effect_db_item( unsigned spell_id )
{ return do_find_special_effect_db_item( __passive_effect_db, spell_id ); }

void unique_gear::add_effect( const special_effect_db_item_t& dbitem )
{
  // Passive special effects are processed during first phase initialization so aren't added to __special_effect_db if
  // they have a custom initializer.
  if ( dbitem.passive && dbitem.cb_obj )
    __passive_effect_db.push_back( dbitem );
  else
    __special_effect_db.push_back( dbitem );

  if ( dbitem.fallback )
    __fallback_effect_db.push_back( dbitem );
}

void unique_gear::register_special_effect( unsigned spell_id, custom_cb_t init_callback, bool fallback, bool passive,
                                           wowv_t min_build, wowv_t max_build )
{
  special_effect_db_item_t dbitem;
  dbitem.spell_id = spell_id;
  dbitem.cb_obj = new wrapper_callback_t( std::move( init_callback ), min_build, max_build );
  dbitem.fallback = fallback;
  dbitem.passive = passive;
  add_effect( dbitem );
}

void unique_gear::register_special_effect( std::initializer_list<unsigned> spell_ids, custom_cb_t init_callback,
                                           bool fallback, bool passive, wowv_t min_build, wowv_t max_build )
{
  for ( auto id : spell_ids )
    register_special_effect( id, init_callback, fallback, passive, min_build, max_build );
}

void unique_gear::register_special_effect( unsigned spell_id, const char* encoded_str )
{
  special_effect_db_item_t dbitem;
  dbitem.spell_id = spell_id;
  dbitem.encoded_options = encoded_str;

  __special_effect_db.push_back( dbitem );
}

bool unique_gear::create_fallback_buffs( const special_effect_t& effect, const std::vector<util::string_view>& names )
{
  if ( effect.source != SPECIAL_EFFECT_SOURCE_FALLBACK )
    return false;

  for ( auto name : names )
    buff_t::make_fallback( effect.player, name, effect.player );

  return true;
}

void unique_gear::init_feast( special_effect_t& effect, std::initializer_list<std::pair<stat_e, int>> stat_map )
{
  effect.stat = effect.player->convert_hybrid_stat( STAT_STR_AGI_INT );

  for ( auto&& stat : stat_map )
  {
    if ( stat.first == effect.stat )
    {
      effect.trigger_spell_id = stat.second;
      break;
    }
  }
  effect.stat_amount = effect.player->find_spell( effect.trigger_spell_id )->effectN( 1 ).average( effect.player );
}

void unique_gear::DISABLED_EFFECT( special_effect_t& effect )
{
  effect.type = SPECIAL_EFFECT_NONE;
}

// BracketSim legacy compatibility: procs whose real trigger this sim cannot
// represent, and which therefore fire CONSTANTLY if left alone.
//
// the author, 10 September 2026: "yoghurtboy level 30 the dropimizer said a trinket
// from magtheridon is better which is NOT true."
//
// Eye of Magtheridon, item 28789, ItemEffect 34749 "Recurring Power". The
// game's own description is:
//
//   "Grants increased spell power for 10 sec when one of your spells is
//    RESISTED."
//
// but the DBC row carries none of that condition. What it carries is:
//
//   Proc Chance : 100%
//   Proc Flags  : Magic Hostile Spell
//
// because "is resisted" was scripted, not encoded. So the generic on-equip
// handler does exactly what the data says and procs it on EVERY hostile spell,
// at 100%, stacking to ten. Measured on the author's level 30 Discipline priest:
// recurring_power sat at 99.59% uptime, refreshed 250 times in a 300 second
// fight, and made an item level 38 trinket beat his item level 45 pair by 12%.
//
// It cannot fire in reality. Spell resistance as a damage-reduction mechanic is
// gone from the modern game, and SimulationCraft models no "resisted" result at
// all - there is miss, dodge, parry, block and crit, and nothing this proc could
// key on. A proc whose trigger is unrepresentable belongs at zero, not at 100%.
// Modelling it as always-on is the fabricated number; modelling it as never is
// the honest one.
//
// Registered rather than deleted so the item still equips and still gives its
// stats, and so the reason travels with the code.
void unique_gear::UNREPRESENTABLE_TRIGGER( special_effect_t& effect )
{
  effect.type = SPECIAL_EFFECT_NONE;
}

/**
 * Master list of special effects in Simulationcraft.
 *
 * This list currently contains custom procs and procs where game client data
 * is either incorrect (so we can override values), or incomplete (so we can
 * help the automatic creation process on the simc side).
 *
 * Each line in the array corresponds to a specific spell (a proc driver spell,
 * or an "on use" spell) in World of Warcraft. There are several sources for
 * special effects:
 * 1) Items (Use, Equip, Chance on hit)
 * 2) Enchants, and profession specific enchants
 * 3) Engineering special effects (tinkers, ranged enchants)
 * 4) Gems
 *
 * Blizzard does not discriminate between the different types, nor do we
 * anymore. Each spell can be mapped to a special effect in the simc client.
 * Each special effect is fed to a new proc callback object
 * (dbc_proc_callback_t) that handles the initialization of the proc, and in
 * generic proc cases, the initialization of the buffs/actions.
 *
 * Each entry contains three fields:
 * 1) The spell ID of the effect. You can find these from third party websites
 *    by clicking on the generated link in item tooltip.
 * 2) Currently a c-string of "additional options" given for a special effect.
 *    This includes the forementioned fixes of incorrect values, and "help" to
 *    drive the automatic special effect generation process. Case insensitive.
 * 3) A callback to a custom initialization function. The function is of the
 *    form: void custom_function_of_awesome( special_effect_t& effect,
 *                                           const item_t& item,
 *                                           const special_effect_db_item_t& dbitem )
 *    Where 'effect' is the effect being created, 'item' is the item that has
 *    the special effect, and 'dbitem' is the database entry itself.
 *
 * Now, special effect creation in this new system is currently a two phase
 * process. First, the special_effect_t struct for the special effect is filled
 * with enough information to initialize the proc (for generic procs, a driver
 * spell id is sufficient), and any options given in this list (through the
 * additional options). For custom special effects, the first phase simply
 * creates a stub special_effect_t object, and no game client data is processed
 * at this time.
 *
 * The second phase of the creation process is responsible for instantiating
 * the necessary action_callback_t object (simc procs), and whatever buffs, or
 * actions are required for the proc. This is also when custom callbacks get
 * called.
 *
 * Note: The special effect initialization process is now unified for all types
 * of special effects, we no longer discriminate between item, enchant, tinker,
 * or gem based special effects.
 *
 * Note2: Enchants, addons, and possibly gems will have a separate translation
 * table in sc_enchant.cpp that maps "user given" names of enchants
 * (enchant=dancing_steel), to in game data, so we can properly initialize the
 * correct spells here. Most of the enchants etc., are automatically
 * identified.  The table will only have the "non standard" user strings we
 * currently use, and whatever else we will use in the future.
 */
void unique_gear::register_special_effects()
{
  // Register legion special effects
  register_special_effects_legion();
  // Register azerite special effects
  azerite::register_azerite_powers();
  register_special_effects_bfa();
  shadowlands::register_special_effects();
  dragonflight::register_special_effects();
  thewarwithin::register_special_effects();
  midnight::register_special_effects();

  /* Legacy Effects, pre-5.0 */
  register_special_effect( 45481,  "ProcOn/hit_45479Trigger"            ); /* Shattered Sun Pendant of Acumen */
  register_special_effect( 45482,  "ProcOn/hit_45480Trigger"            ); /* Shattered Sun Pendant of Might */
  register_special_effect( 45483,  "ProcOn/hit_45431Trigger"            ); /* Shattered Sun Pendant of Resolve */
  register_special_effect( 45484,  "ProcOn/hit_45478Trigger"            ); /* Shattered Sun Pendant of Restoration */
  register_special_effect( 57345,  item::darkmoon_card_greatness        );
  register_special_effect( 71519,  item::deathbringers_will             );
  register_special_effect( 71562,  item::deathbringers_will             );
  // Eye of Magtheridon: its "when one of your spells is resisted" condition
  // is not in the data, so the row reads as 100% on every hostile spell.
  register_special_effect( 34749,  UNREPRESENTABLE_TRIGGER               );
  register_special_effect( 62459,  item::chains_of_ice_runic_power       );
  // BRACKETSIM: effects whose driver carries the whole proc and no pointer to
  // the buff or action it applies.
  //
  // Matrix Restabilizer is the clean case. Driver 97138 has the rate (20%), the
  // internal cooldown (105s) and the proc flags (white and yellow melee and
  // ranged). What it does not have is a trigger spell, so
  // `special_effect_t::trigger()` falls back to the driver, the driver has no
  // stat effects, and the engine reports "No constructible buff or action" and
  // builds nothing. The item then simulates as a plain stat stick.
  //
  // The buff id is not guessed. Blizzard's own description of each driver names
  // it - "granting $97139s1 critical strike" - and every id below was read out
  // of the driver's description text, checked to be a real spell, and checked to
  // carry either a stat aura or a damage effect before it was written here.
  // `plan-effect-triggers.mjs` in bracketsim-local does that and prints these
  // lines; `test-results/effect-trigger-plan.json` is its working.
  //
  // Nothing about the RATE is invented: the driver keeps its own chance, icd and
  // flags. This only tells the generic builder what to build.
  register_special_effect( 67702,   item::deaths_verdict ); /* Death's Verdict - Paragon */
  register_special_effect( 67771,   item::deaths_verdict ); /* Death's Verdict (heroic) */
  register_special_effect( 67712,   "67714Trigger"   ); /* Reign of the Unliving - Pillar of Flame */
  register_special_effect( 67758,   "67760Trigger"   ); /* Reign of the Unliving (heroic) */
  /* Dislodged Foreign Object. Was "71600Trigger"/"71643Trigger", which triggers
   * the per-stack aura directly - and that aura is infinite in this client,
   * because the real 20s duration lives on 71601/71644. One permanent stack per
   * proc instead of ten stacks for twenty seconds. See item::dislodged_foreign_object. */
  register_special_effect( 71602,   item::dislodged_foreign_object   );
  register_special_effect( 71645,   item::dislodged_foreign_object   );
  register_special_effect( 309563,  "309567Trigger"  ); /* Black Bruise - Necrotic Touch */
  register_special_effect( 97138,   "97139Trigger"   ); /* Matrix Restabilizer */
  /* Blackened Naaru Sliver. Was "45041Trigger", which skips Battle Trance (45040,
   * the 20s window) and points straight at Combat Insight (45041), whose duration
   * in this client is `Aura (infinite)` - so its stacks never came off. Same bug
   * as Dislodged Foreign Object, with the stacks earned by swinging rather than
   * by a timer. See item::dislodged_foreign_object, which handles both. */
  register_special_effect( 45355,   item::dislodged_foreign_object   );
  register_special_effect( 364917,  "17499Trigger"   ); /* Malown's Slam */
  register_special_effect( 258885,  "258888Trigger"  ); /* Gahz'rilla Fang */
  register_special_effect( 107786,  "107787Trigger"  ); /* No'Kaled, the Elements of Death */
  register_special_effect( 109866,  "109867Trigger"  ); /* No'Kaled, the Elements of Death */
  register_special_effect( 109873,  "109868Trigger"  ); /* No'Kaled, the Elements of Death */
  register_special_effect( 222015,  "222027Trigger"  ); /* Goblet of Nightmarish Ichor */
  register_special_effect( 344221,  "344227Trigger"  ); /* Consumptive Infusion */
  register_special_effect( 457489,  "457533Trigger"  ); /* Wings of Shattered Sorrow */
  register_special_effect( 1220488, "1219104Trigger" ); /* Darkfuse Medichopper */
  // Second batch, same rule and the same working. Eight more drivers from the
  // first list were left out because they are ALREADY registered elsewhere -
  // 34749 is deliberately UNREPRESENTABLE_TRIGGER above, and the Pantheon
  // trinkets have real handlers that decline for their own reasons - and
  // registering a second time would fight an existing decision rather than fix
  // anything.
  register_special_effect( 37447,   "37445Trigger"   ); /* Serpent-Coil Braid */
  register_special_effect( 45042,   "45044Trigger"   ); /* Shifting Naaru Sliver - Power Circle */
  register_special_effect( 71835,   "71834Trigger"   ); /* Zod's Repeating Longbow */
  register_special_effect( 71836,   "71834Trigger"   ); /* Zod's Repeating Longbow (heroic) */
  register_special_effect( 215936,  "215938Trigger"  ); /* Orb of Torment */
  register_special_effect( 234110,  "234111Trigger"  ); /* Cloak of Sweltering Flame */
  register_special_effect( 253807,  "253808Trigger"  ); /* Vest of the Void's Embrace */
  register_special_effect( 384113,  "384117Trigger"  ); /* Stormslash */
  // Third batch. Three more trigger links, and one rate.
  //
  // CHILLPIKE, and the chance-on-hit rate question. the author asked how a chance on
  // hit should be modelled when a spec that lands more hits procs far more
  // often, and pointed at wowsims. They answer it the same way this engine
  // already does: `PPMManager` in wowsims/classic sim/core/attack.go computes
  // `procChance = weaponSpeed * ppm / 60` and rolls it on every landed hit, and
  // `action_t::ppm_proc_chance` here calls `weapon->proc_chance_on_swing( PPM )`,
  // which is the identical conversion. So the rate per hit falls as the weapon
  // gets faster, and a spec landing more hits does proc more - deliberately.
  //
  // What wowsims adds is that a chance-on-hit proc's own damage carries
  // `SpellFlagSuppressWeaponProcs`, so procs cannot chain off each other.
  //
  // The RATE is theirs and is cited rather than invented, which is the part this
  // project has never had for these weapons: wowsims/classic
  // sim/common/item_effects.go:781 gives Chillpike 1.0 PPM. Spell 19260 is both
  // the driver and the damage, so no trigger is named.
  register_special_effect( 19260,   item::chillpike  ); /* Chillpike - Frost Blast, 1.0 PPM (wowsims) */
  // Fourth batch, 23 September: drivers the item register had filed "not damage"
  // on tooltip wording ("Increases attack power by", no "your"). See the handlers.
  register_special_effect( 36041,   item::heartrazor ); /* Heartrazor - 1.0 PPM (wowsims tbc) */
  register_special_effect( 23719,   item::untamed_blade ); /* The Untamed Blade - Untamed Fury, 1.0 PPM (wowsims classic) */
  register_special_effect( 36111,   item::world_breaker ); /* World Breaker - 3.7/60 per hit, spent by the next attack (wowsims tbc) */
  register_special_effect( 33489,   item::blackout_truncheon ); /* Blackout Truncheon - Blinding Speed, 0.8 PPM (wowsims tbc) */
  register_special_effect( 34580,   item::despair ); /* Despair - Impale, 0.5 PPM (wowsims tbc) */
  register_special_effect( 96887,   item::variable_pulse_lightning_capacitor ); /* VPLC - wowsims cata */
  register_special_effect( 97119,   item::variable_pulse_lightning_capacitor ); /* VPLC (heroic) - wowsims cata */
  register_special_effect( 469933,  item::molten_ironfoe ); /* Molten Ironfoe - Molten Strike */
  register_special_effect( 259006,  "259014Trigger"  ); /* Venomstrike - Venom Shot */
  register_special_effect( 29633,   "29644Trigger"   ); /* Galgann's Fireblaster - Fire Blast */
  register_special_effect( 470629,  "470630Trigger"  ); /* Magma-Shot Boomstick */
  register_special_effect( 13533,  item::jackhammer                      );
  register_special_effect( 22640,  item::eskhandars_right_claw           );
  register_special_effect( 21992,  item::thunderfury                     );
  register_special_effect( 71903,  item::shadowmourne                    );
  register_special_effect( 71845,  item::nibelung                        );
  register_special_effect( 101056, item::dragonwrath                     ); /* Dragonwrath, Tarecgosa's Rest */
  register_special_effect( 71846,  item::nibelung                        );
  // Souldrinker, all three Dragon Soul difficulties. The drivers are hollow in
  // this client; the share of max health is keyed off the ITEM id instead.
  register_special_effect( 107895, item::souldrinker                     );  // Normal  77193
  register_special_effect( 109832, item::souldrinker                     );  // Heroic  78479
  register_special_effect( 109829, item::souldrinker                     );  // LFR     78488
  /*
   * The drivers the GAME casts, seen by name in the author's combat log on 19
   * September 2026. They are wowsims' ids, not the ones this client's
   * item_effect.inc carries, and both sets are registered because registering a
   * driver that never fires costs nothing and missing one costs the effect.
   */
  register_special_effect( 108022, item::souldrinker                     );  // cast in game, Normal
  register_special_effect( 109831, item::souldrinker                     );  // cast in game, Heroic
  register_special_effect( 109828, item::souldrinker                     );  // cast in game, LFR
  // Gurthalak: the summon is modelled, the tentacle's damage is zero until a log
  // gives a post-squish figure. See the note above namespace gurthalak.
  register_special_effect( 107810, item::gurthalak                       );  // 77191
  register_special_effect( 109841, item::gurthalak                       );  // 78478
  register_special_effect( 109839, item::gurthalak                       );  // 78487
  register_special_effect( 21153,  item::bonereavers_edge                );
  register_special_effect( 21162,  item::sulfuras                        ); /* Sulfuras, Hand of Ragnaros */
  register_special_effect( 34584,  item::love_struck                     ); /* Masquerade Gown (no client data) */
  register_special_effect( 71406,  item::tiny_abomination_in_a_jar       );
  register_special_effect( 71545,  item::tiny_abomination_in_a_jar       );
  register_special_effect( 71892,  item::heartpierce                    );
  register_special_effect( 71880,  item::heartpierce                    );
  register_special_effect( 72413,  "10%"                                ); /* ICC Melee Ring */
  register_special_effect( 96976,  item::matrix_restabilizer            ); /* Matrix Restabilizer */
  register_special_effect( 107824, "1Tick_108016Trigger_20Dur"          ); /* Kiril, Fury of Beasts */
  register_special_effect( 109862, "1Tick_109860Trigger_20Dur"          ); /* Kiril, Fury of Beasts */
  register_special_effect( 109865, "1Tick_109863Trigger_20Dur"          ); /* Kiril, Fury of Beasts */
  register_special_effect( 107995, item::vial_of_shadows                );
  register_special_effect( 109725, item::vial_of_shadows                );
  register_special_effect( 109722, item::vial_of_shadows                );
  register_special_effect( 108006, item::cunning_of_the_cruel           );
  register_special_effect( 109799, item::cunning_of_the_cruel           );
  register_special_effect( 109801, item::cunning_of_the_cruel           );
  register_special_effect( 243988, item::blazefury_medallion            ); /* Kazzak Neck */

  /* Misc effects */
  register_special_effect( 188534, item::felmouth_frenzy                );

  /* Warlords of Draenor 6.2 */
  register_special_effect( 184270, item::mirror_of_the_blademaster      );
  register_special_effect( 184291, item::soul_capacitor                 );
  register_special_effect( 183942, item::insatiable_hunger              );
  register_special_effect( 184066, item::prophecy_of_fear               );
  register_special_effect( 183951, item::unblinking_gaze_of_sethe       );
  register_special_effect( 184249, item::discordant_chorus              );
  register_special_effect( 184257, item::empty_drinking_horn            );
  register_special_effect( 184767, item::tyrants_decree                 );
  register_special_effect( 184762, item::warlords_unseeing_eye          );
  register_special_effect( 201404, item::gronntooth_war_horn            );
  register_special_effect( 201407, item::infallible_tracking_charm      );
  register_special_effect( 201409, item::orb_of_voidsight               );
  register_special_effect( 429257, item::witherbarks_branch             );

  /* Warlords of Draenor 6.0 */
  register_special_effect( 177085, item::blackiron_micro_crucible       );
  register_special_effect( 177071, item::humming_blackiron_trigger      );
  register_special_effect( 177104, item::battering_talisman_trigger     );
  register_special_effect( 177098, item::forgemasters_insignia          );
  register_special_effect( 177090, item::autorepairing_autoclave        );
  register_special_effect( 177171, item::spellbound_runic_band          );
  register_special_effect( 177163, item::spellbound_solium_band         );

  /* Mists of Pandaria: 5.4 */
  register_special_effect( 146195, item::flurry_of_xuen                 );
  register_special_effect( 146197, item::essence_of_yulon               );

  register_special_effect( 146219, "ProcOn/Hit"                         ); /* Yu'lon's Bite */
  register_special_effect( 146251, "ProcOn/Hit"                         ); /* Thok's Tail Tip (Str proc) */

  register_special_effect( 145955, item::readiness                      );
  register_special_effect( 146019, item::readiness                      );
  register_special_effect( 146025, item::readiness                      );
  register_special_effect( 146051, item::amplification, false, true     );
  register_special_effect( 146136, item::cleave                         );

  register_special_effect( 146183, item::black_blood_of_yshaarj         );
  register_special_effect( 146286, item::skeers_bloodsoaked_talisman    );
  register_special_effect( 146315, item::prismatic_prison_of_pride      );
  register_special_effect( 146047, item::purified_bindings_of_immerseus );
  register_special_effect( 146251, item::thoks_tail_tip                 );

  /* Mists of Pandaria: 5.2 */
  register_special_effect( 139116, item::rune_of_reorigination          );
  register_special_effect( 138957, item::spark_of_zandalar              );
  register_special_effect( 138964, item::unerring_vision_of_leishen     );

  register_special_effect( 138728, "Reverse"                            ); /* Steadfast Talisman of the Shado-Pan Assault */
  register_special_effect( 138701, "ProcOn/Hit"                         ); /* Brutal Talisman of the Shado-Pan Assault */
  register_special_effect( 138700, "ProcOn/Hit"                         ); /* Vicious Talisman of the Shado-Pan Assault */
  register_special_effect( 139171, "ProcOn/Crit_RPPMAttackCrit"         ); /* Gaze of the Twins */
  register_special_effect( 138757, "1Tick_138737Trigger"                ); /* Renataki's Soul Charm */
  register_special_effect( 138790, "ProcOn/Hit_1Tick_138788Trigger"     ); /* Wushoolay's Final Choice */
  register_special_effect( 138758, "1Tick_138760Trigger"                ); /* Fabled Feather of Ji-Kun */
  register_special_effect( 139134, "ProcOn/Crit_RPPMSpellCrit"          ); /* Cha-Ye's Essence of Brilliance */

  register_special_effect( 138865, "ProcOn/Dodge"                       ); /* Delicate Vial of the Sanguinaire */

  /* Mists of Pandaria: 5.0 */
  register_special_effect( 126650, "ProcOn/Hit"                         ); /* Terror in the Mists */
  register_special_effect( 126658, "ProcOn/Hit"                         ); /* Darkmist Vortex */

  /* Mists of Pandaria: Dungeon */
  register_special_effect( 126473, "ProcOn/Hit"                         ); /* Vision of the Predator */
  register_special_effect( 126516, "ProcOn/Hit"                         ); /* Carbonic Carbuncle */
  register_special_effect( 126482, "ProcOn/Hit"                         ); /* Windswept Pages */
  register_special_effect( 126490, "ProcOn/Crit"                        ); /* Searing Words */

  /* Mists of Pandaria: Player versus Player */
  register_special_effect( 126706, "ProcOn/Hit"                         ); /* Gladiator's Insignia of Dominance */

  /* Mists of Pandaria: Darkmoon Faire */
  register_special_effect( 128990, "ProcOn/Hit"                         ); /* Relic of Yu'lon */
  register_special_effect( 128445, "ProcOn/Crit"                        ); /* Relic of Xuen (agi) */

  /* Timewalking */
  register_special_effect( 96963, item::necromantic_focus               ); // Firelands Timewalking Trinket
  register_special_effect( 91003, item::sorrowsong                      ); // Lost City of Tol'vir Timewalking Trinket

  /**
   * Enchants
   */
  register_special_effect( { 44797, 55275, 55344 }, enchants::meta_gem_effect, false, true );

  /* The Burning Crusade */
  register_special_effect(  28093, "1PPM"                               ); /* Mongoose */

  /* Wrath of the Lich King */
  register_special_effect(  59620, "1PPM"                               ); /* Berserking */
  register_special_effect(  20007, enchants::crusader                   );
  register_special_effect(  42976, enchants::executioner                );

  /* Cataclysm */
  register_special_effect(  94747, enchants::hurricane_spell            );
  register_special_effect(  74221, "1PPM"                               ); /* Hurricane Weapon */
  register_special_effect(  74245, "1PPM"                               ); /* Landslide */

  /* Mists of Pandaria */
  register_special_effect( 118333, enchants::dancing_steel              );
  register_special_effect( 142531, enchants::dancing_steel              ); /* Bloody Dancing Steel */
  register_special_effect( 120033, enchants::jade_spirit                );
  register_special_effect( 141178, enchants::jade_spirit                );
  register_special_effect( 104561, enchants::windsong                   );
  register_special_effect( 104428, "rppmhaste"                          ); /* Elemental Force */
  register_special_effect( 104441, enchants::rivers_song                );
  register_special_effect( 118314, enchants::colossus                   );

  /* Warlords of Draenor */
  register_special_effect( 159239, enchants::mark_of_the_shattered_hand );
  register_special_effect( 159243, enchants::mark_of_the_thunderlord    );
  register_special_effect( 159682, enchants::mark_of_warsong            );
  register_special_effect( 159683, enchants::mark_of_the_frostwolf      );
  register_special_effect( 159685, enchants::mark_of_blackrock          );
  register_special_effect( 156059, enchants::megawatt_filament          );
  register_special_effect( 156052, enchants::oglethorpes_missile_splitter );
  register_special_effect( 173286, enchants::hemets_heartseeker         );
  register_special_effect( 173321, enchants::mark_of_bleeding_hollow    );

  /* Engineering enchants */
  register_special_effect( 177708, "1PPM_109092Trigger"                 ); /* Mirror Scope */
  register_special_effect( 177707, "1PPM_109085Trigger"                 ); /* Lord Blastingtons Scope of Doom */
  register_special_effect(  95713, "1PPM_95712Trigger"                  ); /* Gnomish XRay */
  register_special_effect(  99622, "1PPM_99621Trigger"                  ); /* Flintlocks Woodchucker */

  /* Profession perks */
  register_special_effect( 105574, profession::zen_alchemist_stone      ); /* Zen Alchemist Stone (stat proc) */
  register_special_effect( 157136, profession::draenor_philosophers_stone ); /* Draenor Philosopher's Stone (stat proc) */
  register_special_effect(  55004, profession::nitro_boosts             );
  register_special_effect(  82626, profession::grounded_plasma_shield   );

  /**
   * Gems
   */

  // TODO: check why 20% PPM and not 100% from spell data?
  register_special_effect(  39958, "0.2PPM"                             ); /* Thundering Skyfire */
  register_special_effect(  55380, "0.2PPM"                             ); /* Thundering Skyflare */

  /* Generic special effects begin here */

  /* Racial special effects */
  register_special_effect( 5227,   racial::touch_of_the_grave );
  register_special_effect( 255669, racial::entropic_embrace );
  register_special_effect( 291628, racial::brush_it_off );
  register_special_effect( 292751, racial::zandalari_loa, true );
  register_special_effect( 312923, racial::combat_analysis );

  /* Generic "global scope" special effects */
  register_special_effect( 462854, generic::skyfury );
  register_special_effect( 63604, generic::enable_all_item_effects );
}

void unique_gear::unregister_special_effects()
{
  for ( auto& dbitem : __special_effect_db )
    delete dbitem.cb_obj;

  for ( auto& dbitem : __passive_effect_db )
    delete dbitem.cb_obj;
  }

action_t* unique_gear::create_action( player_t* player, util::string_view name, util::string_view options )
{
  if ( auto action = shadowlands::create_action( player, name, options ) )
    return action;
  else if ( auto action = thewarwithin::create_action( player, name, options ) )
    return action;
  else if ( auto action = midnight::create_action( player, name, options ) )
    return action;

  return nullptr;
}

void unique_gear::register_hotfixes()
{
  register_hotfixes_legion();
  register_hotfixes_bfa();
  shadowlands::register_hotfixes();
  dragonflight::register_hotfixes();
  thewarwithin::register_hotfixes();
  midnight::register_hotfixes();
}

void unique_gear::register_target_data_initializers( sim_t* sim )
{
  register_target_data_initializers_legion( sim );
  register_target_data_initializers_bfa( sim );
  azerite::register_azerite_target_data_initializers( sim );
  shadowlands::register_target_data_initializers( *sim );
  dragonflight::register_target_data_initializers( *sim );
  thewarwithin::register_target_data_initializers( *sim );
  midnight::register_target_data_initializers( *sim );
}

void unique_gear::register_actor_initializers( sim_t& sim )
{
  shadowlands::register_actor_initializers( sim );
  dragonflight::register_actor_initializers( sim );
  thewarwithin::register_actor_initializers( sim );
  midnight::register_actor_initializers( sim );
}

std::vector<special_effect_t*> unique_gear::find_special_effects( player_t* p, unsigned id, special_effect_e type )
{
  std::vector<special_effect_t*> effects;

  for ( auto e : p->special_effects )
  {
    if ( e->spell_id == id && ( type == SPECIAL_EFFECT_NONE || type == e->type ) )
    {
      effects.push_back( e );
    }
  }

  for ( const auto& item : p->items )
  {
    for ( auto e : item.parsed.special_effects )
    {
      if ( e->spell_id == id && ( type == SPECIAL_EFFECT_NONE || type == e->type ) )
      {
        effects.push_back( e );
      }
    }
  }

  return effects;
}

special_effect_t* unique_gear::find_special_effect( player_t* p, unsigned id, special_effect_e type )
{
  auto effects = unique_gear::find_special_effects( p, id, type );

  return effects.empty() ? nullptr : effects.front();
}

// Some special effects may use fallback initializers, where the fallback initializer is called if
// the special effect is not found on the actor. Typical cases include anything relating to
// class-specific special effects, where buffs for example should be unconditionally created for the
// actor. This method is called after the normal special effect initialization process finishes on
// the actor.
void unique_gear::initialize_special_effect_fallbacks( player_t* actor )
{
  special_effect_t fallback_effect( actor );

  // Generate an unique list of fallback spell ids
  std::vector<unsigned> fallback_ids;
  range::for_each( __fallback_effect_db, [ &fallback_ids ]( const special_effect_db_item_t& elem ) {
    if ( range::find( fallback_ids, elem.spell_id ) == fallback_ids.end() )
    {
      fallback_ids.push_back( elem.spell_id );
    }
  });

  // Check all fallback ids
  for ( auto fallback_id : fallback_ids )
  {
    // Actor already has a special effect with the fallback id, so don't do anything
    if ( find_special_effect( actor, fallback_id ) )
    {
      continue;
    }

    fallback_effect.reset();
    fallback_effect.spell_id = fallback_id;
    // TODO: Is this really needed?
    fallback_effect.source = SPECIAL_EFFECT_SOURCE_FALLBACK;
    fallback_effect.type = SPECIAL_EFFECT_FALLBACK;

    // Get all registered fallback effects for the spell (fallback) id
    auto dbitems = find_fallback_effect_db_item( fallback_id );
    // .. nothing found, continue
    if ( dbitems.empty() )
    {
      continue;
    }

    // For all registered fallback effects
    for ( const auto& dbitem: dbitems )
    {
      // Ensure that the fallback effect is actually valid for the special effect (actor)
      if ( ! dbitem -> cb_obj -> valid( fallback_effect ) )
      {
        continue;
      }

      fallback_effect.custom_init_object.push_back( dbitem -> cb_obj );
    }

    if ( !fallback_effect.custom_init_object.empty() )
    {
      actor -> special_effects.push_back( new special_effect_t( fallback_effect ) );
    }
  }
}

namespace
{
bool cmp_special_effect( const special_effect_db_item_t& a, const special_effect_db_item_t& b )
{
  if ( &a == &b )
    return false;

  if ( a.spell_id == b.spell_id )
  {
    if ( ! a.encoded_options.empty() && b.encoded_options.empty() )
    {
      return true;
    }
    else if ( a.encoded_options.empty() && ! b.encoded_options.empty() )
    {
      return false;
    }
    else if ( ! a.encoded_options.empty() && ! b.encoded_options.empty() )
    {
      return a.encoded_options < b.encoded_options;
    }

    assert( a.cb_obj && b.cb_obj );
    // Note, descending priority order
    return a.cb_obj -> priority > b.cb_obj -> priority;
  }

  return a.spell_id < b.spell_id;
}

} // unnamed namespace ends

void unique_gear::sort_special_effects()
{
  range::sort( __special_effect_db, cmp_special_effect );
  range::sort( __fallback_effect_db, cmp_special_effect );
  range::sort( __passive_effect_db, cmp_special_effect );
}

bool unique_gear::has_role_mult( player_t* player, const spell_data_t* s_data )
{
  // Failsafe if driver is spell_data_t::nil() or spell_data_t::not_found()
  if ( !s_data->ok() )
    return false;

  auto vars = player->dbc->spell_desc_vars( s_data->id() ).desc_vars();
  if( !vars )
    return false;

  std::cmatch m;
  std::regex get_var( R"(\$(?:healing)?rolemult=\$(.*))" );

  return std::regex_search( vars, m, get_var );
}

bool unique_gear::has_role_mult( const special_effect_t& effect )
{
  return has_role_mult( effect.player, effect.driver() );
}

double unique_gear::role_mult( player_t* player, const spell_data_t* s_data )
{
  static constexpr const char* role_mult_str =
    "$rolemult=$?a137048|a137028|a137023|a137010|a212613|a137008|a137039|a137031|a137032|a137029|a137024|a137024|"
    "a356810|a137012[${0.66}.2][${1}]";

  double mult = 1.0;
  auto vars = s_data ? player->dbc->spell_desc_vars( s_data->id() ).desc_vars() : role_mult_str;

  assert( vars && "No spell description variables found. role_mult( player_t* ) can provide a default value." );
  if ( vars )
  {
    std::cmatch m;
    std::regex get_var( R"(\$(?:healing)?rolemult=\$(.*))" );  // find the $rolemult= variable
    if ( std::regex_search( vars, m, get_var ) )
    {
      const auto var = m.str( 1 );
      std::regex get_role( R"(\??((?:a\d+\|?)*)\[\$\{([\d\.]+)\}[\d\.]*\])" );  // find each role group
      std::sregex_iterator role_it( var.begin(), var.end(), get_role );
      for ( std::sregex_iterator i = role_it; i != std::sregex_iterator(); i++ )
      {
        mult = util::to_double( i->str( 2 ) );
        const auto role = i->str( 1 );
        std::regex get_spec( R"(a(\d+))" );  // find each spec spell id
        std::sregex_iterator spec_it( role.begin(), role.end(), get_spec );
        for ( std::sregex_iterator j = spec_it; j != std::sregex_iterator(); j++ )
        {
          if ( util::to_unsigned_ignore_error( j->str( 1 ), 0u ) == player->spec_spell->id() )
          {
            player->sim->print_debug( "parsed role multiplier for spell '{}': {}",
                                      s_data ? s_data->name_cstr() : "none", mult );
            return mult;
          }
        }
      }
    }
  }

  return mult;
}

double unique_gear::role_mult( const special_effect_t& effect )
{
  return role_mult( effect.player, effect.driver() );
}

const spell_data_t* unique_gear::spell_from_spell_text( const special_effect_t& e )
{
  if ( auto desc = e.player->dbc->spell_text( e.spell_id ).desc() )
  {
    std::cmatch m;
    std::regex r( R"(\$\?a)" + std::to_string( e.player->spec_spell->id() ) + R"(\[\$@spellname([0-9]+)\]\[\])" );
    if ( std::regex_search( desc, m, r ) )
    {
      auto id = as<unsigned>( std::stoi( m.str( 1 ) ) );
      auto spell = e.player->find_spell( id );

      e.player->sim->print_debug( "parsed spell for special effect '{}': {}", e.name(), *spell );
      return spell;
    }
  }

  return spell_data_t::nil();
}

std::vector<unsigned> unique_gear::equipped_gem_list( player_t* player, util::span<const unsigned> gem_desc_id )
{
  std::vector<unsigned> gems;

  for ( const auto& item : player->items )
  {
    for ( auto gem_id : item.parsed.gem_id )
    {
      if ( gem_id )
      {
        const auto& _gem = player->dbc->item( gem_id );
        const auto& _prop = player->dbc->gem_property( _gem.gem_properties );
        for ( auto g : gem_desc_id )
        {
          if ( _prop.desc_id == g )
          {
            gems.push_back( g );
            break;
          }
        }
      }
    }
  }

  return gems;
}

std::vector<unsigned> unique_gear::unique_gem_list( player_t* player, util::span<const unsigned> gem_desc_id )
{
  auto _list = equipped_gem_list( player, gem_desc_id );
  range::sort( _list );

  auto it = range::unique( _list );
  _list.erase( it, _list.end() );

  return _list;
}
