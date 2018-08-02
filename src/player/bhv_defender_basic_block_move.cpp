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

#include "bhv_defender_basic_block_move.h"

#include "bhv_basic_move.h"
#include "bhv_get_ball.h"

#include <rcsc/action/body_intercept.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include "strategy.h"
#include "defense_system.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefenderBasicBlockMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_DefenderBasicBlock" );

    // get ball
    {
        Rect2D bounding_rect( Vector2D( -50.0, -30.0 ),
                              Vector2D( -5.0, 30.0 ) );
        if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
        {
            return true;
        }
    }

    const WorldModel & wm = agent->world();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    // positioning

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    const double ball_xdiff = wm.ball().pos().x - wm.self().pos().x;

    if ( ball_xdiff > 10.0
         && ( wm.kickableTeammate()
              || mate_min < opp_min - 1
              || self_min < opp_min - 1 )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": ball is front and our team keep ball" );
        Bhv_BasicMove().execute( agent );
        return true;
    }

    double dash_power = DefenseSystem::get_defender_dash_power( wm, home_pos );

    double dist_thr = agent->world().ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.5 ) dist_thr = 1.5;

    agent->debugClient().addMessage( "DefMove%.0f", dash_power );
    agent->debugClient().setTarget( home_pos );
    agent->debugClient().addCircle( home_pos, dist_thr );
    dlog.addText( Logger::TEAM,
                  __FILE__": go home. power=%.1f",
                  dash_power );

    if ( ! Body_GoToPoint( home_pos,
                           dist_thr,
                           dash_power ).execute( agent ) )
    {
        AngleDeg body_angle = 180.0;
        if ( agent->world().ball().angleFromSelf().abs() < 80.0 )
        {
            body_angle = 0.0;
        }
        Body_TurnToAngle( body_angle ).execute( agent );
    }

    if ( wm.kickableOpponent()
         && wm.ball().distFromSelf() <  7.0
         && wm.self().pos().x < wm.ball().pos().x )
    {
        const PlayerObject * opponent = wm.getOpponentNearestToBall( 5 );
        if ( opponent )
        {
            agent->setNeckAction( new Neck_TurnToBallAndPlayer( opponent, 0 ) );
        }
        else
        {
            agent->setNeckAction( new Neck_TurnToBall() );
        }
    }
    else
    {
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    }

    return true;
}
