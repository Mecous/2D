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

#include "bhv_tactical_intercept.h"

#include "strategy.h"

#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>
#include <rcsc/action/view_normal.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/audio_memory.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

#include "bhv_tactical_tackle.h"

#include <iostream>
using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TacticalIntercept::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const bool result = isInterceptSituation( agent );

    dlog.addText( Logger::INTERCEPT,
                  "TAC_INTERCEPT %d %d %d %d",
                  wm.interceptTable()->selfReachCycle(),
                  wm.interceptTable()->teammateReachCycle(),
                  wm.interceptTable()->opponentReachCycle(),
                  (int)result );
    if ( ! result )
    {
        return false;
    }

    // dlog.addText( Logger::INTERCEPT,
    //               __FILE__": (execute)" );
    agent->debugClient().addMessage( "TacIntercept" );

    if ( ! Body_Intercept().execute( agent ) )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (execute) failed intercept." );
        return false;
    }

    const InterceptInfo info = Body_Intercept::get_best_intercept( wm, true );

    const int self_step = info.reachStep();
    const int opponent_step = wm.interceptTable()->opponentReachCycle();

    const Vector2D self_trap_pos = wm.ball().inertiaPoint( self_step );

    if ( ServerParam::i().theirTeamGoalPos().dist2( self_trap_pos ) < std::pow( 20.0, 2 ) )
    {
        if ( self_step == 3 && opponent_step >= 3
             && wm.ball().seenPosCount() <= 1 )
        {
            dlog.addText( Logger::INTERCEPT,
                          __FILE__": View_Normal" );
            agent->setViewAction( new View_Normal() );
        }

        if ( self_step >= 2 )
        {
            if ( wm.ball().velCount() > 1 )
            {
                agent->setNeckAction( new Neck_TurnToBallOrScan( -1 ) );
            }
            else if ( ! doTurnNeckToCrossPoint( agent ) )
            {
                dlog.addText( Logger::INTERCEPT,
                              __FILE__": cross neck. turn neck to goalie or scan" );
                agent->setNeckAction( new Neck_TurnToGoalieOrScan( 0 ) );
            }
        }
        else
        {
            dlog.addText( Logger::INTERCEPT,
                          __FILE__": shoot neck. turn neck to goalie or scan" );
            const AbstractPlayerObject * goalie = wm.getTheirGoalie();
            if ( goalie
                 && goalie->posCount() == 0
                 && self_step >= 2 )
            {
                dlog.addText( Logger::INTERCEPT,
                              __FILE__": shoot neck. check teammate" );
                agent->setNeckAction( new Neck_TurnToLowConfTeammate() );
            }
            else
            {
                dlog.addText( Logger::INTERCEPT,
                              __FILE__": shoot neck. check goalie" );
                agent->setNeckAction( new Neck_TurnToGoalieOrScan( -1 ) );
            }
        }
    }
    else if ( wm.self().pos().x > wm.ourOffensePlayerLineX() - 10.0 )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": offensive neck" );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
    }
    else
    {
        if ( self_trap_pos.x > wm.ourDefenseLineX() + 10.0
             && opponent_step >= self_step + 3 )
        {
            dlog.addText( Logger::INTERCEPT,
                          __FILE__": normal turn neck. offensive" );
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        }
        else
        {
            dlog.addText( Logger::INTERCEPT,
                          __FILE__": normal turn neck. default" );
            agent->setNeckAction( new Neck_DefaultInterceptNeck() );
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TacticalIntercept::doTurnNeckToCrossPoint( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.self().pos().x < 35.0 )
    {
        return false;
    }

    const AbstractPlayerObject * opponent_goalie = wm.getTheirGoalie();
    if ( opponent_goalie
         && opponent_goalie->posCount() >= 2 )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (doTurnNeckToCrossPoint) goalie should be checked" );
        return false;
    }

    Vector2D opposite_pole( ServerParam::i().pitchHalfLength() - 5.5,
                            ServerParam::i().goalHalfWidth() );
    if ( wm.self().pos().y > 0.0 ) opposite_pole.y *= -1.0;

    AngleDeg opposite_pole_angle = ( opposite_pole - wm.self().pos() ).th();


    if ( wm.dirCount( opposite_pole_angle ) <= 1 )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (doTurnNeckToCrossPoint) enough accuracy. angle %.1f",
                      opposite_pole_angle.degree() );
        return false;
    }

    AngleDeg angle_diff = agent->effector().queuedNextAngleFromBody( opposite_pole );
    if ( angle_diff.abs() > 100.0 )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (doTurnNeckToCrossPoint) over view range. angle_diff=%.1f",
                      angle_diff.degree() );
        return false;
    }


    agent->setNeckAction( new Neck_TurnToPoint( opposite_pole ) );
    agent->debugClient().addMessage( "NeckToOpposite" );
    dlog.addText( Logger::INTERCEPT,
                  __FILE__": (doTurnNeckToCrossPoint) Neck to oppsite pole" );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TacticalIntercept::isInterceptSituation( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.self().goalie() )
    {
        return false;
    }

    if ( wm.self().isKickable() )
    {
        return false;
    }

    if ( ! wm.self().posValid()
         || ! wm.ball().posValid() )
    {
        return false;
    }

    if ( wm.kickableTeammate() )
    {
        return false;
    }

    const double current_offside_line = std::max( wm.ball().pos().x, wm.theirDefensePlayerLineX() );
    if ( wm.self().pos().x > current_offside_line + 5.0 )
    {
        return false;
    }

    // chase ball
    const int self_step = wm.interceptTable()->selfReachCycle();

    if ( self_step > 500 )
    {
        return false;
    }

    if ( wm.audioMemory().passTime() == wm.time()
         && wm.audioMemory().pass().empty()
         && wm.audioMemory().pass().front().receiver_ != wm.self().unum() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (isIntercetSituation) false. not a pass receiver" );
        return false;
    }

    const int teammate_step = wm.interceptTable()->teammateReachCycle();
    const int opponent_step = wm.interceptTable()->opponentReachCycle();

    const PlayerObject * first_teammate = wm.interceptTable()->fastestTeammate();

    const Vector2D self_ball_pos = wm.ball().inertiaPoint( self_step );

    if ( first_teammate
         && opponent_step >= 5
         && ( self_step <= 3 || first_teammate->distFromSelf() < 0.8 )
         && self_step >= teammate_step )
    {
        const Vector2D teammate_ball_pos = wm.ball().inertiaPoint( teammate_step );
        double teammate_dist = first_teammate->pos().dist( teammate_ball_pos );
        double self_dist = wm.self().pos().dist( self_ball_pos );

        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (isInterceptSituation) check teammate. teammate_dist=%.3f self_dist=%.3f",
                      teammate_dist, self_dist );

        if ( ( first_teammate->distFromSelf() < 1.0 && teammate_dist < self_dist - 0.1 )
             || teammate_dist < self_dist * 0.8 )
        {
            dlog.addText( Logger::INTERCEPT,
                          __FILE__": (isInterceptSituation) false. exist teammate" );
            return false;
        }
    }

    //
    // check offensive situation
    //
    if ( self_ball_pos.x > ServerParam::i().theirPenaltyAreaLineX() - 10.0
         && self_ball_pos.x > wm.theirDefenseLineX() - 10.0 )
    {
        return isOffensiveInterceptSituation( agent );
    }

    //
    // check defensive situation first
    //
    if( wm.lastKickerSide() == wm.theirSide()
        && ! wm.kickableOpponent()
        && wm.ball().pos().x < ServerParam::i().ourPenaltyAreaLineX()  )
    {
        return isDefensiveInterceptSituation( agent );

    }

    //
    // check normal situation
    //

    if ( self_step <= opponent_step - 3
         || teammate_step <= opponent_step - 3 )
    {
        return isNormalInterceptSituation( agent );

    }

    //
    // check defensive situation second
    //

    return isDefensiveInterceptSituation( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TacticalIntercept::isOffensiveInterceptSituation( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int self_step = wm.interceptTable()->selfReachCycle();
    const int teammate_step = wm.interceptTable()->teammateReachCycle();

    if ( self_step == 1
         && teammate_step == 1 )
    {
        Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
        if ( wm.self().pos().dist( ball_next )
             < wm.interceptTable()->fastestTeammate()->pos().dist( ball_next ) )
        {
            dlog.addText( Logger::INTERCEPT,
                          __FILE__": (isOffensiveInterceptSituation) step=1 true" );
            return true;
        }
        else
        {
            dlog.addText( Logger::INTERCEPT,
                          __FILE__": (isOffensiveInterceptSituation) step=1 false" );
            return false;
        }
    }
#if 0
    if ( 1 < teammate_step
         && self_step <= teammate_step )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (isOffensiveInterceptSituation) true (1)" );
        return true;
    }

    if ( 3 <= teammate_step
         && self_step <= teammate_step + 1 )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (isOffensiveInterceptSituation) true (2)" );
        return true;
    }

    if ( 5 <= teammate_step
         && self_step <= teammate_step + 2 )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (isOffensiveInterceptSituation) true (3)" );
        return true;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    if ( 2 <= teammate_step
         && self_step <= 6
         && wm.ball().pos().dist( home_pos ) < 10.0 )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (isOffensiveInterceptSituation) true (4)" );
        return true;
    }
#endif

    return false;
}



/*-------------------------------------------------------------------*/
/*!

 */

/*
bool
Bhv_TacticalIntercept::isDefensiveInterceptSituation( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int self_step = wm.interceptTable()->selfReachCycle();
    const int teammate_step = wm.interceptTable()->teammateReachCycle();
    const int opponent_step = wm.interceptTable()->opponentReachCycle();

    if (  self_step <= opponent_step + 1
	  && self_step < teammate_step + 2
	  && self_step < 10
	  && opponent_step > 3 )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (isDeffensiveInterceptSituation) true" );
        return true;
    }


    return false;
}
*/

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TacticalIntercept::isNormalInterceptSituation( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.kickableTeammate() )
    {
        return false;
    }

    const int self_step = wm.interceptTable()->selfReachCycle();
    const int teammate_step = wm.interceptTable()->teammateReachCycle();
    const int opponent_step = wm.interceptTable()->opponentReachCycle();

    if ( self_step == 1
         && teammate_step == 1
         && opponent_step > 1 )
    {
        Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
        if ( wm.self().pos().dist( ball_next )
             < wm.interceptTable()->fastestTeammate()->pos().dist( ball_next ) )
        {
            dlog.addText( Logger::INTERCEPT,
                          __FILE__": (isNormalInterceptSituation) step=1 true" );
            return true;
        }
        else
        {
            dlog.addText( Logger::INTERCEPT,
                          __FILE__": (isNormalInterceptSituation) step=1 false" );
            return false;
        }
    }

    if ( self_step <= opponent_step
         && self_step <= teammate_step )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (isNormalInterceptSituation) true (1)" );
        return true;
    }

    if ( ! wm.kickableOpponent()
         && self_step <= teammate_step
         && self_step <= opponent_step + 3 )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (isNormalInterceptSituation) true (2)" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TacticalIntercept::isDefensiveInterceptSituation( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const int self_step = wm.interceptTable()->selfReachCycle();
    const int teammate_step = wm.interceptTable()->teammateReachCycle();
    const int opponent_step = wm.interceptTable()->opponentReachCycle();

    const double ball_speed = wm.ball().vel().r() * std::pow( ServerParam::i().ballDecay(), opponent_step );

    //
    // opponent through pass check
    //

    // if( self_step > 10
    //     && self_step < opponent_step + 4
    //     && teammate_step > self_step + 2 )
    // {
    //     dlog.addText( Logger::INTERCEPT,
    //                   __FILE__": (isDefensiveInterceptSituation) true ( for through pass )" );
    //     return true;
    // }

    //
    // normal defense
    //

    //if ( self_step == 20 && teammate_step == 20 && opponent_step == 17 ) return false

    if ( self_step < opponent_step - 1 )
    {
        if ( self_step <= teammate_step )
        {
            dlog.addText( Logger::INTERCEPT,
                          __FILE__": (isDefensiveInterceptSituation) true (1)" );
            return true;
        }
    }
    else
    {
        if ( ! wm.kickableOpponent()
             && self_step <= teammate_step + 1
             && self_step <= opponent_step + 1
             && ball_speed < 0.8 )
        {
            dlog.addText( Logger::INTERCEPT,
                          __FILE__": (isDefensiveInterceptSituation) true (2)" );
            return true;
        }
    }

    if ( opponent_step >= 2
         && self_step <= opponent_step + 1
         && self_step <= teammate_step + 2
         && ball_speed < 0.8 )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (isDefensiveInterceptSituation) true (3)" );
        return true;
    }

    if ( opponent_step >= 3
         && self_step <= opponent_step + 1
         && self_step <= teammate_step * 1.3
         && ball_speed < 0.8 )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (isDefensiveInterceptSituation) true (4)" );
        return true;
    }

    if ( opponent_step >= 4
         && self_step <= opponent_step + 1
         && self_step <= teammate_step + 2
         && ball_speed < 0.8 )
    {
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (isDefensiveInterceptSituation) true (5)" );
        return true;
    }

    return false;
}
