// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "actgen_simple_cross.h"

#include "act_pass.h"
#include "simple_pass_checker.h"

#include "predict_state.h"
#include "action_state_pair.h"

#include <rcsc/player/world_model.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/geom/segment_2d.h>
#include <rcsc/math_util.h>

#include <vector>

// #define DEBUG_PRINT

using namespace rcsc;

namespace {
/*-------------------------------------------------------------------*/
/*!

 */
bool
exist_opponent_on_ball_move( const PredictState & state,
                             const Segment2D & ball_move,
                             const Vector2D & receive_pos )
{
    for ( PredictPlayerObject::Cont::const_iterator o = state.theirPlayers().begin(),
              end = state.theirPlayers().end();
          o != end;
          ++o )
    {
        if ( (*o)->posCount() > 10 ) continue;
        if ( (*o)->isTackling() ) continue;

        if ( (*o)->pos().dist2( receive_pos ) < std::pow( 3.0, 2 ) )
        {
            return true;
        }

        const double control_area = ( (*o)->goalie()
                                      ? ServerParam::i().catchableArea()
                                      : (*o)->playerTypePtr()->kickableArea() );
        if ( ball_move.dist( (*o)->pos() ) < control_area + 0.3 )
        {
            return true;
        }
    }
    return false;
}
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActGen_SimpleCross::generate( std::vector< ActionStatePair > * result,
                              const PredictState & state,
                              const WorldModel & current_wm,
                              const std::vector< ActionStatePair > & path ) const
{
    static GameTime s_last_call_time( 0, 0 );
    static int s_action_count = 0;

    if ( path.empty() )
    {
        return;
    }

    if ( current_wm.time() != s_last_call_time )
    {
        s_action_count = 0;
        s_last_call_time = current_wm.time();
    }

    const Vector2D goal = ServerParam::i().theirTeamGoalPos();

    if ( state.ball().pos().dist2( goal ) > std::pow( 25.0, 2 ) )
    {
        return;
    }

    const AbstractPlayerObject & holder = state.ballHolder();
    int generated_count = 0;

    for ( PredictPlayerObject::Cont::const_iterator t = state.ourPlayers().begin(),
              end = state.ourPlayers().end();
          t != end;
          ++t )
    {
        if ( (*t)->posCount() > 10 ) continue;
        if ( ! (*t)->isValid() ) continue;
        if ( (*t)->unum() == state.ballHolder().unum() ) continue;
        if ( (*t)->isTackling() ) continue;
        if ( (*t)->pos().dist2( state.ball().pos() ) > std::pow( 15.0, 2 ) ) continue;
        //doubl player_move_dist = ( (*t)->playerTypePtr()->realSpeedMax()*0.8 ) * state.spentTime();
        //if ( (*t)->pos().dist( goal ) - player_move_dist > 15.0 ) continue;
        if ( (*t)->pos().dist2( goal ) > std::pow( 15.0, 2 ) ) continue;

        Vector2D target_pos = Vector2D::INVALIDATED;

        for ( int ix = +2; ix >= -1; --ix )
        {
            Vector2D receive_pos = (*t)->pos();
            receive_pos.x += 1.0*ix;

            Vector2D buf = ( state.ball().pos() - receive_pos ).setLengthVector( 1.0 );
            Segment2D ball_move( state.ball().pos() - buf, receive_pos + buf );

            if ( ! exist_opponent_on_ball_move( state, ball_move, receive_pos ) )
            {
                target_pos = receive_pos;
                break;
            }
        }

        if ( target_pos.isValid() )
        {
            const unsigned long kick_step = 2;
            const unsigned long spent_time
                = calc_length_geom_series( ServerParam::i().ballSpeedMax(),
                                           state.ball().pos().dist( target_pos ),
                                           ServerParam::i().ballDecay() )
                + kick_step;
            PredictState::ConstPtr result_state( new PredictState( state,
                                                                   spent_time,
                                                                   (*t)->unum(),
                                                                   target_pos ) );

            CooperativeAction::Ptr action( new ActPass( holder.unum(),
                                                        (*t)->unum(),
                                                        target_pos,
                                                        Vector2D( 0.0, 0.0 ), // dummy velocity
                                                        spent_time,
                                                        kick_step,
                                                        "simpleCross" ) );
            ++s_action_count;
            ++generated_count;
            action->setIndex( s_action_count );
            action->setMode( ActPass::SIMPLE_CROSS );
            action->setSafetyLevel( CooperativeAction::MaybeDangerous );
            result->push_back( ActionStatePair( action, result_state ) );
        }
    }

#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  "(ActGen_SimpleCross) path=%zd, generated=%d total=%d",
                  path.size(),
                  generated_count, s_action_count );
#endif
}
