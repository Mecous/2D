// -*-c++-*-

/*!
  \file predict_state.cpp
  \brief predicted field state class Source File
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA, Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "predict_state.h"

#include "field_analyzer.h"

#include <rcsc/formation/formation.h>
#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>

#include <algorithm>

using namespace rcsc;

const int PredictState::VALID_PLAYER_THRESHOLD = 8;

/*-------------------------------------------------------------------*/
/*!

 */
PredictState::PredictState()
    : M_game_mode( GameMode::PlayOn, NEUTRAL, GameTime( 1, 0 ), 0, 0 ),
      M_current_time( 1, 0 ),
      M_spent_time( 0 ),
      M_ball(),
      M_self( new PredictPlayerObject() ),
      M_our_players(),
      M_their_players(),
      M_ball_holder( new PredictPlayerObject() ),
      M_offside_line_x( 0.0 ),
      M_our_defense_line_x( 0.0 ),
      M_our_offense_player_line_x( 0.0 ),
      M_their_defense_player_line_x( 0.0 )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
PredictState::PredictState( const PredictState & rhs )
    : M_game_mode( rhs.M_game_mode ),
      M_current_time( rhs.M_current_time ),
      M_spent_time( rhs.M_spent_time ),
      M_ball( rhs.M_ball ),
      M_self( rhs.M_self ),
      M_our_players( rhs.M_our_players ),
      M_their_players( rhs.M_their_players ),
      M_ball_holder( rhs.M_ball_holder ),
      M_offside_line_x( 0.0 ),
      M_our_defense_line_x( 0.0 ),
      M_our_offense_player_line_x( 0.0 ),
      M_their_defense_player_line_x( 0.0 )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
PredictState::PredictState( const WorldModel & wm )
    : M_game_mode( wm.gameMode() ),
      M_current_time( wm.time() ),
      M_spent_time( 0 ),
      M_ball( wm.ball().pos(), wm.ball().vel() ),
      M_self(),
      M_our_players(),
      M_their_players(),
      M_ball_holder(),
      M_offside_line_x( 0.0 ),
      M_our_defense_line_x( 0.0 ),
      M_our_offense_player_line_x( 0.0 ),
      M_their_defense_player_line_x( 0.0 )
{
    //
    // find ball holder
    //
    const AbstractPlayerObject * holder = static_cast< const AbstractPlayerObject * >( 0 );
    if ( wm.interceptTable()->selfReachStep() > wm.interceptTable()->teammateReachStep() )
    {
        holder = wm.interceptTable()->firstTeammate();
    }
    else
    {
        holder = &(wm.self());
    }

    //
    // initialize self, our players, ball holder
    //
    M_our_players.reserve( wm.ourPlayers().size() );

    for ( AbstractPlayerObject::Cont::const_iterator p = wm.ourPlayers().begin(), end = wm.ourPlayers().end();
          p != end;
          ++p )
    {
        PredictPlayerObject::Ptr ptr( new PredictPlayerObject( **p ) );
        M_our_players.push_back( ptr );

        if ( holder == *p )
        {
            M_ball_holder = ptr;
        }

        if ( (*p)->unum() == wm.self().unum() )
        {
            M_self = ptr;
        }
    }

    if ( ! M_self )
    {
        M_self = PredictPlayerObject::Ptr( new PredictPlayerObject( wm.self() ) );
    }

    if ( ! M_ball_holder )
    {
        M_ball_holder = M_self;
    }

    //
    // initialize their players
    //
    for ( AbstractPlayerObject::Cont::const_iterator p = wm.theirPlayers().begin(), end = wm.theirPlayers().end();
          p != end;
          ++p )
    {
        PredictPlayerObject::Ptr ptr( new PredictPlayerObject( **p ) );
        M_their_players.push_back( ptr );
    }

    //
    // initialize lines
    //

    M_offside_line_x = wm.offsideLineX();
    M_our_defense_line_x = wm.ourDefenseLineX();
    M_our_offense_player_line_x = wm.ourOffensePlayerLineX();
    M_their_defense_player_line_x = wm.theirDefensePlayerLineX();
}

/*-------------------------------------------------------------------*/
/*!

 */
PredictState::PredictState( const PredictState & rhs,
                            unsigned long append_spent_time )
    : M_game_mode( rhs.gameMode() ),
      M_current_time( rhs.currentTime() ),
      M_spent_time( rhs.spentTime() + append_spent_time ),
      M_ball( rhs.M_ball ),
      M_self( rhs.M_self ),
      M_our_players( rhs.M_our_players ),
      M_their_players( rhs.M_their_players ),
      M_ball_holder( rhs.M_ball_holder ),
      M_offside_line_x( rhs.M_offside_line_x ),
      M_our_defense_line_x( rhs.M_our_defense_line_x ),
      M_our_offense_player_line_x( rhs.M_our_offense_player_line_x ),
      M_their_defense_player_line_x( rhs.M_their_defense_player_line_x )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
PredictState::PredictState( const PredictState & rhs,
                            const unsigned long append_spent_time,
                            const int ball_holder_unum,
                            const Vector2D & ball_and_holder_pos )
    : M_game_mode( rhs.M_game_mode ),
      M_current_time( rhs.M_current_time ),
      M_spent_time( rhs.M_spent_time + append_spent_time ),
      M_ball( ball_and_holder_pos ),
      M_self( rhs.M_self ),
      M_our_players( rhs.M_our_players ),
      M_their_players( rhs.M_their_players ),
      M_ball_holder(),
      M_offside_line_x( std::max( rhs.M_offside_line_x, ball_and_holder_pos.x ) ),
      M_our_defense_line_x( rhs.M_our_defense_line_x ),
      M_our_offense_player_line_x( std::max( rhs.M_our_offense_player_line_x,
                                             ball_and_holder_pos.x ) ),
      M_their_defense_player_line_x( rhs.M_their_defense_player_line_x )
{
    if ( 1 <= ball_holder_unum && ball_holder_unum <= 11 )
    {
        for ( PredictPlayerObject::Cont::iterator p = M_our_players.begin(), end = M_our_players.end();
              p != end;
              ++p )
        {
            if ( (*p)->unum() == ball_holder_unum )
            {
                PredictPlayerObject::Ptr ptr( new PredictPlayerObject( **p, ball_and_holder_pos ) );

                *p = ptr;
                M_ball_holder = ptr;
                if ( M_self->unum() == ball_holder_unum )
                {
                    M_self = ptr;
                }
                break;
            }
        }
    }

    if ( ! M_ball_holder )
    {
        M_ball_holder = rhs.M_ball_holder;
    }

    predictPlayerPositions();
    updateLines();
}

/*-------------------------------------------------------------------*/
/*!

 */
PredictState::PredictState( const PredictState & rhs,
                            const unsigned long append_spent_time,
                            const Vector2D & ball_pos )
    : M_game_mode( rhs.M_game_mode ),
      M_current_time( rhs.M_current_time ),
      M_spent_time( rhs.M_spent_time + append_spent_time ),
      M_ball( ball_pos ),
      M_self( rhs.M_self ),
      M_our_players( rhs.M_our_players ),
      M_their_players( rhs.M_their_players ),
      M_ball_holder( rhs.M_ball_holder ),
      M_offside_line_x( std::max( rhs.M_offside_line_x, ball_pos.x ) ),
      M_our_defense_line_x( rhs.M_our_defense_line_x ),
      M_our_offense_player_line_x( rhs.M_our_offense_player_line_x ),
      M_their_defense_player_line_x( rhs.M_their_defense_player_line_x )
{
    predictPlayerPositions();
    updateLines();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::update()
{
    updateBallHolder();
    predictPlayerPositions();
    updateLines();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::updateBallHolder()
{
    if ( ! M_ball_holder
         || ! M_ball_holder->isValid() )
    {
        double min_dist2 = 1000000.0;
        for ( PredictPlayerObject::Cont::const_iterator it = M_our_players.begin(), end = M_our_players.end();
              it != end;
              ++it )
        {
            if ( ! (*it)->isValid() )
            {
                continue;
            }

            const double d2 = (*it)->pos().dist2( M_ball.pos() );

            if ( d2 < min_dist2 )
            {
                min_dist2 = d2;
                M_ball_holder = *it;
            }
        }
    }

    if ( ! M_ball_holder
         || ! M_ball_holder->isValid() )
    {
        M_ball_holder = M_self;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::predictPlayerPositions()
{
#if 0
    Formation::ConstPtr f = FieldAnalyzer::i().ourFormation();
    if ( ! f )
    {
        std::cerr << "(PredictState::predictPlayerPosition) No formation." << std::endl;
        return;
    }

    const int ball_holder_unum = ( M_ball_holder
                                   ? M_ball_holder->unum()
                                   : Unum_Unknown );
    for ( PredictPlayerObject::Cont::iterator p = M_our_players.begin(), end = M_our_players.end();
              p != end;
              ++p )
    {
        if ( (*p)->unum() != ball_holder_unum )
        {
            Vector2D new_pos = f->getPosition( (*p)->unum(), M_ball.pos() );
            PredictPlayerObject::Ptr ptr( new PredictPlayerObject( **p, new_pos ) );
            *p = ptr;
            if ( (*p)->unum() == M_self->unum() )
            {
                M_self = ptr;
            }
        }
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::updateLines()
{
    // offside_line, their_defense_player_line
    {
        double first = -ServerParam::i().pitchHalfLength();
        double second = first;
        for ( PredictPlayerObject::Cont::const_iterator p = M_their_players.begin(), end = M_their_players.end();
              p != end;
              ++p )
        {
            if ( (*p)->pos().x > second )
            {
                second = (*p)->pos().x;
                if ( second > first )
                {
                    std::swap( first, second );
                }
            }
        }

        M_offside_line_x = std::max( 0.0, std::max( second, M_ball.pos().x ) );
        M_their_defense_player_line_x = second;
    }

    // our_offense_player_line, our_defense_line
    {
        double first_max = -ServerParam::i().pitchHalfLength();
        double first_min = ServerParam::i().pitchHalfLength();
        double second_min = first_min;
        for ( PredictPlayerObject::Cont::const_iterator p = M_our_players.begin(), end = M_our_players.end();
              p != end;
              ++p )
        {
            if ( (*p)->pos().x < second_min )
            {
                second_min = (*p)->pos().x;
                if ( second_min < first_min )
                {
                    std::swap( first_min, second_min );
                }
            }

            if ( (*p)->pos().x > first_max )
            {
                first_max = (*p)->pos().x;
            }
        }

        M_our_defense_line_x = std::min( second_min, M_ball.pos().x );
        M_our_offense_player_line_x = first_max;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::setGameMode( const GameMode & mode )
{
    M_game_mode = mode;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::setCurrentTime( const rcsc::GameTime & time )
{
    M_current_time = time;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::setSpentTime( const unsigned long spent_time )
{
    M_spent_time = spent_time;
}


/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::setBall( const Vector2D & pos,
                               const Vector2D & vel )
{
    M_ball.assign( pos, vel );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::setBall( const Vector2D & pos )
{
    M_ball.assign( pos ); // the velicity is set to 0.
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::setSelf( PredictPlayerObject::Ptr ptr )
{
    if ( ! ptr )
    {
        std::cerr << "(PredictState::setSelf) NULL pointer" << std::endl;
        return;
    }

    M_self = ptr;

    bool found = false;
    for ( PredictPlayerObject::Cont::iterator p = M_our_players.begin(), end = M_our_players.end();
          p != end;
          ++p )
    {
        if ( (*p)->unum() == M_self->unum() )
        {
            *p = ptr;
            found = true;
            break;
        }
    }

    if ( ! found )
    {
        M_our_players.push_back( ptr );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::setSelfPos( const Vector2D & pos )
{
    if ( ! M_self )
    {
        std::cerr << "(PredictState::setSelfPos) no self instance." << std::endl;
        return;
    }

    PredictPlayerObject::Ptr ptr( new PredictPlayerObject( *M_self, pos ) );
    M_self = ptr;

    bool found = false;
    for ( PredictPlayerObject::Cont::iterator p = M_our_players.begin(), end = M_our_players.end();
          p != end;
          ++p )
    {
        if ( (*p)->unum() == M_self->unum() )
        {
            *p = ptr;
            found = true;
            break;
        }
    }

    if ( ! found )
    {
        M_our_players.push_back( ptr );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::setPlayer( PredictPlayerObject::Ptr ptr )
{
    if ( ! ptr )
    {
        std::cerr << "(PredictState::setPlayer) NULL pointer." << std::endl;
        return;
    }

    if ( ! M_self )
    {
        std::cerr << "(PredictState::setPlayer) no self instance." << std::endl;
        return;
    }

    if ( ptr->side() == NEUTRAL )
    {
        setUnknownPlayer( ptr );
    }
    else if ( ptr->side() == M_self->side() )
    {
        setTeammate( ptr );
    }
    else
    {
        setOpponent( ptr );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::setTeammate( PredictPlayerObject::Ptr ptr )
{
    bool found = false;
    for ( PredictPlayerObject::Cont::iterator p = M_our_players.begin(), end = M_our_players.end();
          p != end;
          ++p )
    {
        if ( (*p)->unum() == ptr->unum() )
        {
            *p = ptr;
            found = true;
            break;
        }
    }

    if ( ! found )
    {
        M_our_players.push_back( ptr );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::setOpponent( PredictPlayerObject::Ptr ptr )
{
    for ( PredictPlayerObject::Cont::iterator p = M_their_players.begin(), end = M_their_players.end();
          p != end;
          ++p )
    {
        if ( (*p)->unum() == ptr->unum() )
        {
            *p = ptr;
            return;
        }
    }

    M_their_players.push_back( ptr );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::setUnknownPlayer( PredictPlayerObject::Ptr ptr )
{
    M_their_players.push_back( ptr );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::setPlayerPos( const SideID side,
                            const int unum,
                            const Vector2D & pos )
{
    if ( ! M_self )
    {
        return;
    }

    if ( M_self->side() == side )
    {
        for ( PredictPlayerObject::Cont::iterator p = M_our_players.begin(), end = M_our_players.end();
              p != end;
              ++p )
        {
            if ( (*p)->unum() == unum )
            {
                PredictPlayerObject::Ptr ptr( new PredictPlayerObject( **p, pos ) );
                *p = ptr;
                if ( M_self->unum() == unum )
                {
                    M_self = ptr;
                }
                if ( M_ball_holder
                     && M_ball_holder->unum() == unum )
                {
                    M_ball_holder = ptr;
                }
                break;
            }
        }
    }
    else
    {
        for ( PredictPlayerObject::Cont::iterator p = M_their_players.begin(), end = M_their_players.end();
              p != end;
              ++p )
        {
            if ( (*p)->unum() == unum )
            {
                PredictPlayerObject::Ptr ptr( new PredictPlayerObject( **p, pos ) );
                *p = ptr;
                break;
            }
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::setBallHolderUnum( const int ball_holder_unum )
{
    for ( PredictPlayerObject::Cont::iterator p = M_our_players.begin(), end = M_our_players.end();
          p != end;
          ++p )
    {
        if ( (*p)->unum() == ball_holder_unum )
        {
            M_ball_holder = *p;
            break;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
const AbstractPlayerObject *
PredictState::ourPlayer( const int unum ) const
{
    if ( unum < 1 || 11 < unum  )
    {
        std::cerr << __FILE__ << ' ' << __LINE__ << ": (ourPlayer)"
                  << "invalid unum " << unum << std::endl;
        return static_cast< const AbstractPlayerObject * >( 0 );
    }

    for ( PredictPlayerObject::Cont::const_iterator p = M_our_players.begin(), end = M_our_players.end();
          p != end;
          ++p )
    {
        if ( (*p)->unum() == unum )
        {
            return p->get();
        }
    }

    return static_cast< const AbstractPlayerObject * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
const AbstractPlayerObject *
PredictState::theirPlayer( const int unum ) const
{
    if ( unum < 1 || 11 < unum  )
    {
        std::cerr << __FILE__ << ' ' << __LINE__ << ": (ourPlayer)"
                  << "invalid unum " << unum << std::endl;
        return static_cast< const AbstractPlayerObject * >( 0 );
    }

    for ( PredictPlayerObject::Cont::const_iterator p = M_their_players.begin(), end = M_their_players.end();
          p != end;
          ++p )
    {
        if ( (*p)->unum() == unum )
        {
            return p->get();
        }
    }

    return static_cast< const AbstractPlayerObject * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
const AbstractPlayerObject *
PredictState::getOurGoalie() const
{
    for ( PredictPlayerObject::Cont::const_iterator p = M_our_players.begin(), end = M_our_players.end();
          p != end;
          ++p )
    {
        if ( (*p)->goalie() )
        {
            return p->get();
        }
    }

    return static_cast< const AbstractPlayerObject * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
const AbstractPlayerObject *
PredictState::getPlayerNearestTo( const PredictPlayerObject::Cont & players,
                                  const Vector2D & point,
                                  const int count_thr,
                                  double * dist_to_point ) const
{
    const AbstractPlayerObject * nearest_player = static_cast< AbstractPlayerObject * >( 0 );
    double min_dist2 = 10000000.0;

    for ( PredictPlayerObject::Cont::const_iterator it = players.begin(), end = players.end();
          it != end;
          ++ it )
    {
        const AbstractPlayerObject * pl = &(**it);

        if ( pl -> isGhost()
             || pl -> posCount() > count_thr )
        {
            continue;
        }

        const double d2 = point.dist2( pl->pos() );
        if ( min_dist2 > d2 )
        {
            nearest_player = pl;
            min_dist2 = d2;
        }
    }

    if ( nearest_player
         && dist_to_point )
    {
        *dist_to_point = std::sqrt( min_dist2 );
    }

    return nearest_player;
}

/*-------------------------------------------------------------------*/
/*!

 */
AbstractPlayerObject::Cont
PredictState::getPlayers( const PlayerPredicate * predicate ) const
{
    AbstractPlayerObject::Cont ret;

    if ( ! predicate )
    {
        return ret;
    }

    for ( PredictPlayerObject::Cont::const_iterator it = M_our_players.begin(), end = M_our_players.end();
          it != end;
          ++it )
    {
        if ( (*predicate)( **it ) )
        {
            ret.push_back( &(**it) );
        }
    }

    for ( PredictPlayerObject::Cont::const_iterator it = M_their_players.begin(), end = M_their_players.end();
          it != end;
          ++it )
    {
        if ( (*predicate)( **it ) )
        {
            ret.push_back( &(**it) );
        }
    }

    delete predicate;
    return ret;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictState::getPlayers( AbstractPlayerObject::Cont & cont,
                          const PlayerPredicate * predicate ) const
{
    if ( ! predicate )
    {
        return;
    }

    for ( PredictPlayerObject::Cont::const_iterator it = M_our_players.begin(), end = M_our_players.end();
          it != end;
          ++it )
    {
        if ( (*predicate)( **it ) )
        {
            cont.push_back( &(**it) );
        }
    }

    for ( PredictPlayerObject::Cont::const_iterator it = M_their_players.begin(), end = M_their_players.end();
          it != end;
          ++it )
    {
        if ( (*predicate)( **it ) )
        {
            cont.push_back( &(**it) );
        }
    }

    delete predicate;
}

/*-------------------------------------------------------------------*/
/*!

 */
std::ostream &
PredictState::print( std::ostream & os ) const
{
    os << "(state " << M_current_time << '\n';
    os << " (ball " << M_ball.pos() << ')' << '\n';

    if ( M_self )
    {
        os << " (self " << M_self->pos() << ')' << '\n';
    }

    for ( PredictPlayerObject::Cont::const_iterator p = M_our_players.begin(), end = M_our_players.end();
          p != end;
          ++p )
    {
        os << " (our " << (*p)->unum() << ' ' << (*p)->pos() << ')' << '\n';
    }

    for ( PredictPlayerObject::Cont::const_iterator p = M_their_players.begin(), end = M_their_players.end();
          p != end;
          ++p )
    {
        os << " (opp " << (*p)->unum() << ' ' << (*p)->pos() << ')' << '\n';
    }

    if ( M_ball_holder )
    {
        os << " (holder " << M_ball_holder->unum() << ')' << '\n';
    }

    os << " (offside_x " << M_offside_line_x << ')' << '\n';
    os << " (their_defense_x " << M_their_defense_player_line_x << ')' << '\n';
    os << " (our_offense_x " << M_our_offense_player_line_x << ')' << '\n';
    os << " (our_defense_x " << M_our_defense_line_x << ')' << '\n';

    os << ')';
    return os;
}
