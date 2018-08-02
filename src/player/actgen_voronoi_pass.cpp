// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

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

#include "actgen_voronoi_pass.h"

#include "act_pass.h"
#include "simple_pass_checker.h"

#include "field_analyzer.h"

#include "predict_state.h"
#include "action_state_pair.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/geom/voronoi_diagram.h>
#include <rcsc/timer.h>

// #define DEBUG_PROFILE
// #define DEBUG_PRINT
// #define DEBUG_PRINT_SUCCESS

using namespace rcsc;

namespace {
int g_action_count = 0;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActGen_VoronoiPass::generate( std::vector< ActionStatePair > * result,
                              const PredictState & state,
                              const WorldModel & wm,
                              const std::vector< ActionStatePair > & path ) const
{
    static GameTime s_update_time;
    static int s_call_counter = 0;
#ifdef DEBUG_PROFILE
    static double s_cumulative_msec = 0.0;
#endif

    // not generate as first action
    if ( path.empty() )
    {
        return;
    }

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    if ( s_update_time != wm.time() )
    {
        s_call_counter = 0;
        g_action_count = 0;
        s_update_time = wm.time();

#ifdef DEBUG_PROFILE
        s_cumulative_msec = 0.0;
        dlog.addText( Logger::ACTION_CHAIN,
                      __FILE__": create candidate pointss, elapsed %f [ms]",
                      timer.elapsedReal() );
#endif
    }

    ++s_call_counter;

    PredictPlayerObject::Cont receivers;
    receivers.reserve( state.ourPlayers().size() );

    if ( ! createReceiverCandidates( receivers, state, wm, path ) )
    {
        return;
    }

    generateActions( result, state, FieldAnalyzer::i().voronoiTargetPoints(), receivers );

#ifdef DEBUG_PROFILE
    double msec = timer.elapsedReal();
    s_cumulative_msec += msec;
    dlog.addText( Logger::ACTION_CHAIN,
                  __FILE__": PROFILE %d: path=%d, generated=%d/%d, elapsed %f / %f [ms] ",
                  s_call_counter,
                  path.size(),
                  generated_count, s_action_count,
                  msec, s_cumulative_msec );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
ActGen_VoronoiPass::createReceiverCandidates( PredictPlayerObject::Cont & receivers,
                                              const PredictState & state,
                                              const rcsc::WorldModel & wm,
                                              const std::vector< ActionStatePair > & path ) const
{
    //
    // check previous ball holder in order to forbid the previous ball holder becomes the receiver.
    //

    bool old_holder_table[11];
    for ( int i = 0; i < 11; ++i )
    {
        old_holder_table[i] = false;
    }

    for ( int c = static_cast< int >( path.size() ) - 1; c >= 0; --c )
    {
        int ball_holder_unum = Unum_Unknown;

        if ( c == 0 )
        {
            int self_min = wm.interceptTable()->selfReachCycle();
            int teammate_min = wm.interceptTable()->teammateReachCycle();

            if ( teammate_min < self_min )
            {
                const PlayerObject * teammate = wm.interceptTable()->fastestTeammate();
                if ( teammate )

                {
                    ball_holder_unum = teammate->unum();
                }
            }
            else
            {
                ball_holder_unum = wm.self().unum();
            }
        }
        else
        {
            ball_holder_unum = path[c - 1].state().ballHolder().unum();
        }

        if ( ball_holder_unum == Unum_Unknown )
        {
            continue;
        }

        old_holder_table[ ball_holder_unum - 1 ] = true;
    }

    //
    // create receiver candidates
    //

#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  __FILE__": ball = (%.1f, %.1f)",
                  state.ball().pos().x, state.ball().pos().y );
#endif

    for ( PredictPlayerObject::Cont::const_iterator p = state.ourPlayers().begin(), end = state.ourPlayers().end();
          p != end;
          ++p )
    {
        if ( (*p)->goalie() ) continue;

        if ( (*p)->posCount() > 10
             || (*p)->unum() == Unum_Unknown
             || (*p)->unum() == state.ballHolder().unum() )
        {
            continue;
        }

        if ( old_holder_table[ (*p)->unum() - 1 ] )
        {
            // ignore previous ball holders
            continue;
        }

        receivers.push_back( *p );
    }

    if ( receivers.empty() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      __FILE__": no receiver candidates." );
#endif
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActGen_VoronoiPass::generateActions( std::vector< ActionStatePair > * result,
                                     const PredictState & state,
                                     const std::vector< rcsc::Vector2D > & candidates,
                                     const PredictPlayerObject::Cont & receivers ) const
{

    const ServerParam & SP = ServerParam::i();

    int generated_count = 0;

    for ( std::vector< Vector2D >::const_iterator it = candidates.begin(), end = candidates.end();
          it != end;
          ++it )
    {
        const Vector2D & receive_point = *it;

        // dlog.addText( Logger::ACTION_CHAIN,
        //               "voronoi_pass: receive_point = (%.1f, %.1f)",
        //               receive_point.x, receive_point.y );

        if ( ( receive_point.x < 12.0
               && receive_point.x < state.ball().pos().x - 10.0 )
             || receive_point.x < state.ball().pos().x - 16.0 )
        {
            continue;
        }

        const double ball_dist = ( state.ball().pos() - receive_point ).r();
        if ( ball_dist < 5.0
             || 25.0 < ball_dist )
        {
            continue;
        }

        PredictPlayerObject::ConstPtr receiver;
        int min_receiver_step = 1000;
        for ( PredictPlayerObject::Cont::const_iterator p = receivers.begin();
              p != receivers.end();
              ++p )
        {
            const double receiver_move = (*p)->inertiaFinalPoint().dist( receive_point );
            int receiver_step = (*p)->playerTypePtr()->cyclesToReachDistance( receiver_move );
            //receiver_step += 2; // turn margin
            //receiver_step += 2; // buffer

            if ( min_receiver_step > receiver_step )
            {
                min_receiver_step = receiver_step;
                receiver = *p;
            }
        }

        if ( ! receiver )
        {
            continue;
        }

        double ball_first_speed
            = std::min( calc_first_term_geom_series( ball_dist, SP.ballDecay(), min_receiver_step ),
                        SP.ballSpeedMax() );

        if ( ball_first_speed < 1.2 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          "voronoi_pass: receive_point=(%.2f %.2f) NG ball_first_speed=%.2f < 1.2",
                          receive_point.x, receive_point.y,
                          ball_first_speed );
#endif
            continue;
        }

        int ball_step = min_receiver_step;
        if ( ball_first_speed > SP.ballSpeedMax() )
        {
            ball_first_speed = SP.ballSpeedMax();

            ball_step
                = static_cast< int >( std::ceil( calc_length_geom_series( ball_first_speed,
                                                                          ball_dist,
                                                                          SP.ballDecay() ) ) );
            if ( min_receiver_step > ball_step )
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::ACTION_CHAIN,
                              "voronoi_pass: receive_point=(%.2f %.2f) NG min_recv_step=%d > ball_step=%d",
                              receive_point.x, receive_point.y,
                              min_receiver_step, ball_step );
#endif
                continue;
            }
        }

        const double success_prob = SimplePassChecker::check( state,
                                                              &(state.ballHolder()),
                                                              receiver.get(),
                                                              receive_point,
                                                              ball_first_speed,
                                                              ball_step );

        // TODO: determine the suitable value.
        if ( success_prob < 0.5 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          "voronoi_pass: receive_point=(%.2f %.2f) NG pass_check",
                          receive_point.x, receive_point.y );
#endif
            continue;
        }

        int kick_step = ( ball_first_speed > 2.5
                          ? 3
                          : ball_first_speed > 1.5
                          ? 2
                          : 1 );

        PredictState::ConstPtr result_state( new PredictState( state,
                                                               ball_step + kick_step,
                                                               receiver->unum(),
                                                               receive_point ) );
        CooperativeAction::Ptr action( new ActPass( state.ballHolder().unum(),
                                                    receiver->unum(),
                                                    receive_point,
                                                    Vector2D( 0.0, 0.0 ), // dummy velocity
                                                    kick_step + ball_step,
                                                    kick_step,
                                                    "Voronoi" ) );
        ++g_action_count;
        ++generated_count;
        action->setIndex( g_action_count );
        action->setMode( ActPass::VORONOI );
        action->setSafetyLevel( CooperativeAction::MaybeDangerous );
        result->push_back( ActionStatePair( action, result_state ) );

#ifdef DEBUG_PRINT_SUCCESS
        dlog.addText( Logger::ACTION_CHAIN,
                      "voronoi_pass: passer=%d receiver=%d receive_point=(%.2f %.2f)",
                      holder.unum(),
                      receiver->unum(),
                      receive_point.x, receive_point.y );
#endif
        if ( generated_count > 5 )
        {
            break;
        }
    }
}
