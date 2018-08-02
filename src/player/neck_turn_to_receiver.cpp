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

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "neck_turn_to_receiver.h"

#include "action_chain_holder.h"
#include "action_chain_graph.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/player/player_agent.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

// #define DEBUG_PRINT

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Neck_TurnToReceiver::execute( PlayerAgent * agent )
{
    if ( agent->effector().queuedNextBallKickable() )
    {
        if ( executeImpl( agent ) )
        {

        }
        else if ( agent->world().self().pos().x > 35.0
                  || agent->world().self().pos().absY() > 20.0 )

        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(execute) scan " );
            Neck_ScanField().execute( agent );
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(execute) check other teammate" );
            Neck_TurnToLowConfTeammate().execute( agent );
        }
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(execute) look ball" );
        Neck_TurnToBall().execute( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Neck_TurnToReceiver::executeImpl( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const CooperativeAction & pass = ActionChainHolder::i().graph().bestFirstAction();
    const AbstractPlayerObject * receiver = wm.ourPlayer( pass.targetPlayerUnum() );

    if ( ! receiver )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(executeImpl) null receiver" );
        return false;
    }

    // if ( receiver->posCount() == 0 )
    // {
    //     dlog.addText( Logger::TEAM,
    //                   __FILE__":(executeImpl) already seen" );
    //     return false;
    // }

    dlog.addText( Logger::TEAM,
                  __FILE__":(executeImpl) target=%d", pass.targetPlayerUnum() );


    // if ( receiver->posCount() == 0 )
    // {
    //     dlog.addText( Logger::TEAM,
    //                         __FILE__": Neck_TurnToReceiver. current seen." );
    //     return false;
    // }

    if ( receiver->unum() == wm.self().unum() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(executeImpl) self pass." );
        return false;
    }

    const Vector2D next_self_pos = agent->effector().queuedNextSelfPos();
    const double next_view_width = agent->effector().queuedNextViewWidth().width() * 0.5;
    const AngleDeg next_self_body = agent->effector().queuedNextSelfBody();

    const Vector2D receiver_pos = receiver->pos() + receiver->vel();
    const AngleDeg receiver_angle = ( receiver_pos - next_self_pos ).th();
    const double pass_dist = next_self_pos.dist( pass.targetBallPos() );
    const AngleDeg pass_angle = ( pass.targetBallPos() - next_self_pos ).th();
#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM,
                  __FILE__":(executeImpl) receiver(%.1f %.1f) target=(%.1f %.1f) angle=%.1f",
                  receiver_pos.x, receiver_pos.y,
                  pass.targetBallPos().x, pass.targetBallPos().y,
                  pass_angle.degree() );
#endif

    AngleDeg best_angle;
    int best_dir = 180;
    int best_opponent_count = 0;

    for ( int dir = -30; dir <= 30; dir += 4 )
    {
        AngleDeg angle = pass_angle + static_cast< double >( dir );

        if ( ( angle - next_self_body ).abs() > ServerParam::i().maxNeckAngle() )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          "__ angle=%.1f dir=%d never turn neck to",
                          angle.degree(), dir );
#endif
            continue;
        }

        if ( receiver->posCount() > 0
             && ( receiver_angle - angle ).abs() > next_view_width - 3.0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          "__ angle=%.1f dir=%d cannot see the receiver",
                          angle.degree(), dir );
#endif
            continue;
        }

        const Sector2D view_cone( next_self_pos, 0.0, pass_dist,
                                  angle - std::max( 0.0, next_view_width - 5.0 ),
                                  angle + std::max( 0.0, next_view_width - 5.0 ) );

        int opponent_count = 0;

        for ( PlayerObject::Cont::const_iterator p = wm.opponentsFromSelf().begin(),
                  end = wm.opponentsFromSelf().end();
              p != end;
              ++p )
        {
            if ( view_cone.contains( (*p)->pos() ) )
            {
                opponent_count += std::min( 10, (*p)->posCount() );
            }
        }

#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      "__ angle=%.1f dir=%d opponent_count=%d",
                      angle.degree(), dir, opponent_count );
#endif

        if ( opponent_count > best_opponent_count )
        {
            best_angle = angle;
            best_dir = dir;
            best_opponent_count = opponent_count;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          ">>> updated(1)" );
#endif
        }
        else if ( opponent_count == best_opponent_count )
        {
            if ( std::abs( best_dir ) > std::abs( dir ) )
            {
                best_angle = angle;
                best_dir = dir;
                best_opponent_count = opponent_count;
#ifdef DEBUG_PRINT
                dlog.addText( Logger::TEAM,
                              ">>> updated(2)" );
#endif
            }
        }
    }

    if ( best_dir == 180 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(executeImpl) not found." );
        return false;
    }

    AngleDeg neck_angle = best_angle - next_self_body;

    dlog.addText( Logger::TEAM,
                  __FILE__": face_angle=%.1f neck_angle=%.1f",
                  best_angle.degree() );
    agent->debugClient().addMessage( "NeckToReceiver%.0f",
                                     best_angle.degree() );

    agent->doTurnNeck( neck_angle - wm.self().neck() );

    return true;
}
