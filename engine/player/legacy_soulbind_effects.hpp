// BracketSim legacy compatibility: Shadowlands soulbind traits, wired up.
//
// The data lives in legacy_soulbinds.hpp, generated from a live-client dump
// because Midnight's DBC export drops 127 of the 134 trait spells. This file is
// the hand-written half: it turns that data into buffs and multipliers.
//
// It hangs off player_t rather than off each class module, because MOST soulbind
// traits are class-agnostic - Pointed Courage gives the same 2% crit per nearby
// unit to a mage and to a monk. Implementing those once here means all twelve
// classes get them, and no class module has to know anything about soulbinds.
//
// SIX TRAITS ARE NOT CLASS-AGNOSTIC, and an earlier version of this comment said
// they were. Combat Meditation, Lead by Example, Kevin's Oozeling, Field of
// Blossoms, Wasteland Propriety and Effusive Anima Accelerator each ride their
// own class's covenant ability and inherit that ability's duration - Kevin's
// Oozeling is a 60 second pet for a mage and an 8 second pet for a druid. Those
// live in class_values() below, measured per class from a live client dump.
//
// Values come from the live client's own tooltips, which resolve correctly for
// soulbinds. That is NOT true of conduits, whose rank table was deleted and
// whose tooltips now print stale or zeroed numbers; see legacy_conduits.hpp.

#pragma once

#include "config.hpp"

// covenant_e comes from legacy_soulbinds.hpp below; sc_enums.hpp is here for
// the stat and attribute enums the effect signatures use.
#include "sc_enums.hpp"

// Deliberately light on includes: this header is pulled into player.hpp, so it
// must not drag in buff.hpp or sim.hpp - doing so reorders the engine's include
// graph and breaks unrelated translation units. Everything heavier is included
// by legacy_soulbind_effects.cpp instead.
#include "player/legacy_soulbinds.hpp"

#include <string>

struct player_t;
struct buff_t;
struct cooldown_t;
struct action_t;
struct shuffled_rng_t;

namespace legacy_soulbind
{
// Every trait implemented here, by spell id, so the wiring reads as names.
enum trait_e : unsigned
{
  POINTED_COURAGE     = 329778,  // Kyrian   / Kleia
  SPEAR_OF_THE_ARCHON = 351488,  // Kyrian   / Kleia
  HAMMER_OF_GENESIS   = 333935,  // Kyrian   / Forgelite Prime Mikanikos
  FORGEBORNE_REVERIES = 326514,  // Necrolord/ Bonesmith Heirmir
  MARROWED_GEMSTONE   = 326572,  // Necrolord/ Bonesmith Heirmir
  CARVERS_EYE         = 350899,  // Necrolord/ Bonesmith Heirmir
  BUILT_FOR_WAR       = 319973,  // Venthyr  / General Draven
  BATTLEFIELD_PRESENCE = 352417, // Venthyr  / General Draven
  THRILL_SEEKER       = 331586,  // Venthyr  / Nadjia the Mistblade
  FATAL_FLAW          = 352373,  // Venthyr  / Nadjia the Mistblade
  DAUNTLESS_DUELIST   = 331584,  // Venthyr  / Nadjia the Mistblade
  SOCIAL_BUTTERFLY    = 319210,  // Night Fae/ Dreamweaver
  FIRST_STRIKE        = 325069,  // Night Fae/ Korayn
  WILD_HUNT_TACTICS   = 325066,  // Night Fae/ Korayn
  GROVE_INVIGORATION  = 322721,  // Night Fae/ Niya
  WILD_HUNT_STRATAGEM = 352805,  // Night Fae/ Korayn
  DREAM_DELVER        = 352786,  // Night Fae/ Dreamweaver
  PLAGUEYS_STRIKE     = 323090,  // Necrolord/ Plague Deviser Marileth
  MNEMONIC_EQUIPMENT  = 350936,  // Necrolord/ Bonesmith Heirmir
  SOULGLOW_SPECTROMETER = 352186, // Kyrian  / Forgelite Prime Mikanikos
  VALIANT_STRIKES     = 329791,  // Kyrian   / Kleia
  LIGHT_THE_PATH      = 351491,  // Kyrian   / Kleia
  SOOTHING_SHADE      = 336239,  // Venthyr  / Theotar the Mad Duke
  EXACTING_PREPARATION = 331580, // Venthyr  / Nadjia the Mistblade

  // The covenant-gated four. Each rides its own class's covenant ability and
  // inherits that ability's window, so unlike everything above they carry a
  // class-dependent duration out of class_values() rather than a fixed one.
  COMBAT_MEDITATION   = 328266,  // Kyrian   / Pelagos
  EFFUSIVE_ANIMA      = 352188,  // Kyrian   / Forgelite Prime Mikanikos
  WASTELAND_PROPRIETY = 319983,  // Venthyr  / Theotar the Mad Duke
  LEAD_BY_EXAMPLE     = 342156,  // Necrolord/ Emeni
  FIELD_OF_BLOSSOMS   = 319191,  // Night Fae/ Dreamweaver

  // Kevin's Oozeling is class-dependent in the same way and is NOT here: it is
  // a pet, and its Kevin's Wrath coefficient (0.1196) was derived from a PRIEST
  // combat log against that priest's spell power. Nothing gives the attack
  // power form a warrior or rogue would need, so it stays in the priest module
  // and takes only its LIFETIME from class_values().
  KEVINS_OOZELING     = 352110,  // Necrolord/ Plague Deviser Marileth
  BONDED_HEARTS       = 352503,  // Night Fae/ Niya
  GNASHING_CHOMPERS   = 323919,  // Necrolord/ Emeni
  BETTER_TOGETHER     = 351146,  // Kyrian   / Pelagos
  NEWFOUND_RESOLVE    = 351149,  // Kyrian   / Pelagos     (7 Oct 2026)
  NIYAS_TOOLS_BURRS   = 320659,  // Night Fae/ Niya        (7 Oct 2026)
  REFINED_PALATE      = 336243,  // Venthyr  / Theotar the Mad Duke (7 Oct 2026)
  PARTY_FAVORS        = 351750,  // Venthyr  / Theotar the Mad Duke (7 Oct 2026)

  /*
   * VOLATILE SOLVENT, and how a spell the client no longer exports was recovered
   * to the decimal place.
   *
   * Spell 323074 and all five of its benefit buffs are gone from Midnight's DBC
   * export - `spell_query` returns nothing for 323074, 323491, 323498, 323502,
   * 323504 or 323506. They are alive in the CLIENT, which is a different thing:
   * the author has the buff on his level 60 Necrolord frost mage and screenshotted it.
   *
   * THE TRAIT AS IT ACTUALLY BEHAVES. The tooltip is two sentences, and the
   * second one is the only one that matters to a sim:
   *
   *   "Fleshcraft's passive effect consumes a corpse's essence completely,
   *    granting a benefit based on the creature's type.
   *    Casting Fleshcraft also consumes a small portion of your own essence,
   *    granting you the Humanoid benefit."
   *
   * A sim has no corpses to walk over, so the first sentence never fires. The
   * second always does. the author, 18 September 2026: *"so it should always just give
   * mastery"*. Exactly so - and SimulationCraft's own Shadowlands build reached
   * the same conclusion from the other direction, commenting "Humanoid Buff is
   * now always granted, regardless of what you absorb."
   *
   * This corrects an earlier reading of "+2% primary stat". That is the BEAST
   * benefit (323498) and it needs a beast corpse. Humanoid is mastery RATING -
   * the only one of the five that is a rating rather than a percentage, which is
   * why SimC built it as a stat_buff_t and the other four as pct buffs.
   *
   * WHERE THE NUMBER COMES FROM, and it is not a guess. The Shadowlands DBC
   * still on this machine carries the effect row for 323491:
   *
   *   spell   323491  duration 120000 ms
   *   effect  type 6 (apply aura), subtype 189 (A_MOD_RATING),
   *           misc value 33554432 = bit 25 = mastery rating,
   *           scaling class -1 (PLAYER_SPECIAL_SCALE), coefficient 1.263158
   *
   * Feeding that coefficient through the MIDNIGHT client's own spell scaling
   * table gives 23.9356 * 1.263158 = 30.23 mastery rating at level 60. the author's
   * screenshot reads "Mastery increased by 30.2". The two agree to the digit,
   * which means the Shadowlands coefficient and the Midnight scaling curve can
   * be combined to get the right answer at EVERY bracket, not just the one that
   * happened to be measured:
   *
   *   30   35   40   45   50   60     70   80
   *   17.5 18.7 20.3 22.1 24.4 30.2   37.8 42.5
   *
   * See volatile_solvent_humanoid_rating() below. Nothing here is hardcoded to
   * level 60, and the level 60 value is the control that proves the rest.
   */
  VOLATILE_SOLVENT    = 323074,  // Necrolord/ Plague Deviser Marileth
};

// The six class-dependent traits. Durations in seconds, from a live dump of one
// level 60 character of every class; see this patch's header for the full table.
struct class_values_t
{
  double combat_meditation;
  double lead_by_example;
  double kevins_oozeling;
  double field_of_blossoms;
  double wasteland_propriety;
  bool   measured;  // false for warrior, whose dump came back without descriptions
};

// Values for this actor's class. Never returns null; an unknown class gets the
// mode of the measured eleven, with measured == false.
const class_values_t& class_values( const player_t* p );

/*
 * A SHADOWLANDS SCALED VALUE, at THIS actor's level.
 *
 * Several of these traits carry a rating that scales with level, and Midnight's
 * export has lost the spells that state it. Every one of them turns out to sit
 * on the same curve - the engine's own `spell_scaling( PLAYER_SPECIAL_SCALE )` -
 * so a single coefficient recovers the value at every bracket.
 *
 * The coefficient comes either from the Shadowlands effect row (Volatile
 * Solvent: 1.263158) or from two live readings at different levels agreeing on
 * one (Combat Meditation: 3.3134 at level 40 and 3.3172 at level 60). Either way
 * it is derived and checkable, never fitted to a single point - which is exactly
 * what the hardcoded 79.4 was, and it was wrong everywhere except level 60.
 */
double shadowlands_scaled( const player_t* p, double coefficient );

// Volatile Solvent: Humanoid - the mastery RATING this actor gets at its own
// level. Derived, not tabulated: the Shadowlands effect coefficient (1.263158)
// applied to Midnight's own spell scaling curve. See the VOLATILE_SOLVENT note
// above for the arithmetic and for the live reading that confirms it.
double volatile_solvent_humanoid_rating( const player_t* p );

/*
 * Combat Meditation's mastery, which SCALES - the engine gave every bracket the
 * level 60 figure until 18 September.
 *
 *     30    35    40    45    50    60     70    80
 *     45.8  49.2  53.2  58.1  63.9  79.4   99.3  111.5
 *
 * 53.2 and 79.4 are the author's own readings at 40 and 60; the rest is the curve
 * they both sit on.
 */
double combat_meditation_mastery( const player_t* p );

// How long the Humanoid buff lasts, in seconds. Straight from the Shadowlands
// spell row for 323491, and confirmed by the "2 minutes remaining" on the author's
// own screenshot of a freshly cast one.
constexpr double VOLATILE_SOLVENT_DURATION = 120.0;

// Fleshcraft, spell 324631 - the Necrolord class ability. Every Necrolord of
// every class has the same button, so it is built once here rather than twelve
// times in twelve class modules, exactly as the rest of this file argues.
// Returns nullptr for any other action name.
action_t* create_action( player_t* p, std::string_view name, std::string_view options );

// Effusive Anima Accelerator: CD/15 seconds of cooldown reduction per enemy hit,
// capped at CD/3, where cd is the host covenant ability's cooldown. Confirmed
// against all eleven measured classes.
double effusive_anima_cdr_per_enemy( double host_cooldown_seconds );
double effusive_anima_cdr_cap( double host_cooldown_seconds );

struct effects_t
{
  set_t chosen;

  // Several traits count units standing near you, which a single-actor sim has
  // no way to know. Shadowlands took the same approach: ask.
  int nearby_allies = 4;

  bool enabled = false;

  buff_t* pointed_courage       = nullptr;
  buff_t* spear_of_the_archon   = nullptr;
  buff_t* hammer_of_genesis     = nullptr;
  buff_t* forgeborne_reveries   = nullptr;
  buff_t* marrowed_gemstone     = nullptr;
  buff_t* carvers_eye           = nullptr;
  buff_t* built_for_war         = nullptr;
  buff_t* thrill_seeker         = nullptr;
  buff_t* euphoria              = nullptr;
  buff_t* fatal_flaw_crit       = nullptr;
  buff_t* fatal_flaw_vers       = nullptr;
  buff_t* social_butterfly      = nullptr;
  buff_t* first_strike          = nullptr;
  buff_t* grove_invigoration    = nullptr;
  buff_t* gnashing_chompers     = nullptr;
  buff_t* better_together       = nullptr;
  buff_t* valiant_strikes       = nullptr;

  // Quiet repeat drivers. A buff's own period is bounded by its duration, so
  // anything that has to re-fire on a longer cadence than it lasts needs one.
  buff_t* marrowed_gemstone_driver  = nullptr;
  buff_t* grove_invigoration_driver = nullptr;

  buff_t* soothing_shade         = nullptr;
  buff_t* soothing_shade_driver  = nullptr;

  // The covenant-gated four, fired by covenant_ability_cast() below.
  buff_t* combat_meditation      = nullptr;
  buff_t* lead_by_example        = nullptr;
  buff_t* field_of_blossoms      = nullptr;
  buff_t* wasteland_propriety    = nullptr;
  buff_t* soulglow_spectrometer  = nullptr;
  buff_t* dream_delver           = nullptr;
  buff_t* wild_hunt_stratagem    = nullptr;

  // Volatile Solvent's Humanoid benefit, triggered by casting Fleshcraft.
  buff_t* volatile_solvent_humanoid = nullptr;
  buff_t* newfound_resolve      = nullptr;
  action_t* spiked_burrs        = nullptr;
  shuffled_rng_t* newfound_doubt = nullptr;   // built at init, as class modules do (a mid-fight build crashed)
  buff_t* party_favors[ 4 ]     = {};        // the Mad Duke's Teas this actor may drink (one per fight)
  unsigned party_favor_count    = 0;

  bool has( unsigned id ) const
  {
    return enabled && chosen.has( id );
  }

  void create_buffs( player_t* p );
  void combat_begin( player_t* p );

  // Called by a class module from its Shadowlands covenant ability's execute().
  // Fires whichever of the covenant-gated traits ride that covenant's class
  // ability, and applies Effusive Anima Accelerator's cooldown reduction to
  // host_cd. Doing it here rather than in twelve class modules is the whole
  // point: the traits are identical everywhere, only the DURATION differs, and
  // that comes from class_values().
  //
  // host_cd may be null for an ability with no cooldown; Effusive Anima is then
  // skipped and everything else still fires.
  void covenant_ability_cast( player_t* p, covenant_e cov, cooldown_t* host_cd = nullptr );

  // Flat damage multipliers that are not carried by a buff.
  double player_multiplier( const player_t* p ) const;
  double target_multiplier( const player_t* p, const player_t* t ) const;
};

}  // namespace legacy_soulbind
