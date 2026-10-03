// ==========================================================================
// Dedmonwakeen's DPS-DPM Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "player/talent.hpp"
#include "player/player.hpp"
#include "dbc/dbc.hpp"
#include "sim/sim.hpp"

// Invalid, trait does not exist in db2
player_talent_t::player_talent_t() :
  m_player( nullptr ), m_trait( &( trait_data_t::nil() ) ), m_spell( spell_data_t::not_found() ),
  m_rank( 0 )
{ }

// Not found, trait exists but is not found amongst player's selected traits
player_talent_t::player_talent_t( const player_t* player ) :
  m_player( player ), m_trait( &( trait_data_t::nil() ) ), m_spell( spell_data_t::not_found() ),
  m_rank( 0 )
{ }

// Found and valid
//
// BracketSim (3 Oct 2026): a TAKEN talent's spell is the player's at any level, as in the game - the talent tree
// decides when it is learnt, not the spell's own level. find_spell() is level-checked, so a talent whose spell is
// listed above the character's level came back empty: taken, but dead. Power Infusion (10060) is level 58 in the
// spell data, so every level-30 priest with it talented pressed a button that did nothing (the author: "priests can use PI
// at 30, they should always be using it"). The 29 Sep fix in action_t::verify_actor_level covered only the cast check.
player_talent_t::player_talent_t( const player_t* player, const trait_data_t* trait, unsigned rank ) :
  m_player( player ), m_trait( trait ), m_spell( m_player->find_spell( trait->id_spell ) ),
  m_rank ( rank )
{
  if ( !m_spell->ok() && trait->id_spell )
  {
    const spell_data_t* s = dbc::find_spell( m_player, trait->id_spell );
    if ( s->ok() )
    {
      m_spell = s;
      m_player->sim->print_debug( "BRACKETSIM_TALENT_ABOVE_LEVEL {} {} '{}' spell_level={} player_level={}", *m_player,
                                  trait->id_spell, s->name_cstr(), s->level(), m_player->true_level );
    }
  }
}

const spell_data_t* player_talent_t::find_override_spell( bool require_talent ) const
{
  return ( !require_talent || enabled() ) ? m_player->find_spell( m_trait->id_override_spell ) : spell_data_t::not_found();
}

const spell_data_t* player_talent_t::find_replaced_spell( bool require_talent ) const
{
  return ( !require_talent || enabled() ) ? m_player->find_spell( m_trait->id_replace_spell ) : spell_data_t::not_found();
}
