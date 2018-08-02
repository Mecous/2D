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

#include "bhv_side_half_pass_request_move2014.h"

#include "strategy.h"
#include "field_analyzer.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/arm_point_to_point.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/world_model.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/time/timer.h>
#include <rcsc/types.h>

// #define DEBUG_PRINT

using namespace rcsc;

namespace {

/*-------------------------------------------------------------------*/
/*!

 */
void
pass_request_go_to_point( rcsc::PlayerAgent * agent,
                          const rcsc::Vector2D & target_point )
{
    const WorldModel & wm = agent->world();

    double dist_thr = wm.ball().pos().dist( target_point ) * 0.1 + 0.5;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;
    if ( wm.self().pos().x > wm.offsideLineX() - 0.5 )
    {
        dist_thr = 0.5;
    }

    double dash_power = ServerParam::i().maxDashPower();

    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );


    if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                         -1.0, // dash speed
                         100, // cycle
                         true, // save recovery
                         25.0 // angle threshold
                         ).execute( agent ) )
    {
        // still moving
    }
    else
    {
        // already there
        const Vector2D my_inertia = wm.self().inertiaFinalPoint();
        const double target_dist = my_inertia.dist( target_point );
        AngleDeg body_angle = 0.0;
        if ( target_dist > dist_thr )
        {
            body_angle = ( ServerParam::i().theirTeamGoalPos() - my_inertia ).th();
        }

        Body_TurnToAngle( body_angle ).execute( agent );
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( 1 ) );
    agent->addSayMessage( new PassRequestMessage( target_point ) );
}

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfPassRequestMove2014::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( ! isMoveSituation( wm ) )
    {
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      __FILE__": no move situation" );
        return false;
    }

    const Vector2D target_point = getTargetPoint( wm );

    if ( ! target_point.isValid() )
    {
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      __FILE__": no target point" );
        return false;
    }

#if 0
    if ( ! checkStamina( wm, target_point ) )
    {
        return false;
    }
#endif

    agent->debugClient().addMessage( "SH:PassReqMove" );

    pass_request_go_to_point( agent, target_point );
    agent->setArmAction( new Arm_PointToPoint( target_point ) );

    //agent->setIntention( new IntentionPassRequestMove( target_point ) );

    return true;
}

/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_SideHalfPassRequestMove2014::getTeammateReachStep( const WorldModel & wm )
{
    const PlayerObject * t = wm.getTeammateNearestToBall( 5 );
    if ( t
         && t->distFromBall() < ( t->playerTypePtr()->kickableArea()
                                  + t->distFromSelf() * 0.05
                                  + wm.ball().distFromSelf() * 0.05 ) )
    {
        return 0;
    }

    return wm.interceptTable()->teammateReachStep();
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfPassRequestMove2014::isMoveSituation( const WorldModel & wm )
{
    if ( wm.self().pos().x > 37.0 )
    {
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      __FILE__":(isMoveSituation) [false] too front" );
        return false;
    }

    const double offside_buf = ( Strategy::i().opponentType() == Strategy::Type_WrightEagle
                                 ? +1.0
                                 : -1.0 );
    if ( wm.self().pos().x > wm.offsideLineX() + offside_buf )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(isMoveSituation) [false] in offside area" );
        return false;
    }

    if ( wm.self().pos().x < wm.offsideLineX() - 15.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(isMoveSituation) [false] far offside line" );
        return false;
    }

    if ( wm.teammatesFromBall().empty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (isMoveSituation) [false] no teammate" );
        return false;
    }

    const PlayerObject * t = wm.getTeammateNearestToBall( 5 );
    if ( t
         && t->distFromBall() < ( t->playerTypePtr()->kickableArea()
                                  + t->distFromSelf() * 0.05
                                  + wm.ball().distFromSelf() * 0.05 ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (isMoveSituation) [true] maybe teammate kickable" );
        return true;
    }

    const int teammate_step = wm.interceptTable()->teammateReachCycle();
    const int opponent_step = wm.interceptTable()->opponentReachCycle();

    if ( teammate_step + 1 > opponent_step )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (isMoveSituation) [false] their ball? t_step=%d o_step=%d",
                      teammate_step, opponent_step );
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(isMoveSituation) [true] our ball? t_step=%d o_step=%d",
                  teammate_step, opponent_step );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfPassRequestMove2014::checkStamina( const WorldModel & wm,
                                               const Vector2D & target_point )
{
    const double move_dist = wm.self().pos().dist( target_point );
    const int move_step = wm.self().playerType().cyclesToReachDistance( move_dist );
    StaminaModel stamina_model = wm.self().staminaModel();

    stamina_model.simulateDashes( wm.self().playerType(),
                                  move_step,
                                  ServerParam::i().maxDashPower() );

    dlog.addText( Logger::TEAM,
                  __FILE__":(checkStamina) move_step=%d stamina=%.1f",
                  move_step, stamina_model.stamina() );

    if ( stamina_model.stamina() < ServerParam::i().recoverDecThrValue() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(checkStamina) [false]" );
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(checkStamina) [true]" );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
namespace {
bool
check_heuristic_filter( const WorldModel & wm,
                        const Vector2D & home_pos,
                        const Vector2D & ball_pos,
                        const Vector2D & target_point )
{
    if ( target_point.y * home_pos.y < 0.0 )
    {
        // opposite side
        return false;
    }

    // if ( target_point.absY() < 5.0 )
    // {
    //     // too center
    //     return false;
    // }

    //
    // y diff
    //
    if ( std::fabs( target_point.y - home_pos.y ) > 30.0 ) // Magic Number
    {
// #ifdef DEBUG_PRINT
//         dlog.addText( Logger::TEAM,
//                       "skip(1) target_y=%.1f home_y=%.1f",
//                       target_point.y, home_pos.y );
// #endif
        return false;
    }

    //
    // player will keep away from the ball
    //
    if ( ( wm.self().pos().y - target_point.y ) * ( ball_pos.y - target_point.y ) > 0.0
         && std::fabs( wm.self().pos().y - target_point.y ) < ( ball_pos.y - target_point.y ) )
    {
// #ifdef DEBUG_PRINT
//         dlog.addText( Logger::TEAM,
//                       "skip(2) target_y=%.1f home_y=%.1f",
//                       target_point.y, home_pos.y );
// #endif
        return false;
    }

    //
    // ok
    //
    return true;
}
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SideHalfPassRequestMove2014::getTargetPoint( const rcsc::WorldModel & wm )
{
    Vector2D best_point = Vector2D::INVALIDATED;
    double best_value = -1000000.0;

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const int teammate_step = getTeammateReachStep( wm );
    const Vector2D ball_pos = wm.ball().inertiaPoint( teammate_step );

    int count = 0;

    // TODO: adjust magic numbers

    //for ( double target_x = 44.0; target_x > 35.0; target_x -= 8.0 )
    for ( double target_x = 44.0; target_x > 40.0; target_x -= 8.0 )
    {
        if ( target_x < ball_pos.x + 10.0 )
        {
            continue;
        }

        for ( double target_y = -20.0; target_y < 21.0; target_y += 8.0 )
        {
            const Vector2D target_point( target_x, target_y );
            ++count;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM, "%d: ===== start =====", count );
#endif
            //
            // heuristic filter
            //
            if ( ! check_heuristic_filter( wm, home_pos, ball_pos, target_point ) )
            {
                continue;
            }

            //
            // evaluation
            //
            double value = evaluateTargetPoint( wm, home_pos, ball_pos, target_point );

#ifdef DEBUG_PRINT
            dlog.addRect( Logger::TEAM,
                          target_point.x - 0.1, target_point.y - 0.1, 0.2, 0.2,
                          "#F00" );
            char num[16];
            snprintf( num, 16, "%d:%.3f", count, value );
            dlog.addMessage( Logger::TEAM,
                             target_point.x + 0.2, target_point.y + 0.1, num );
#endif

            if ( value > best_value )
            {
                best_point = target_point;
                best_value = value;
            }
        }
    }

    return best_point;
}

/*-------------------------------------------------------------------*/
/*!
  TODO:
  Not only the receive point but also receivable area (line) must be considered.

  candidate evaluation criteria:
  - opponent_reach_step
  - opponent_distance_to_target_point
  - y_diff [target_point, goal]
  - ball_move_angle
  - opponent_step_to(project_point) - ball_step_to(project_point)
 */
double
Bhv_SideHalfPassRequestMove2014::evaluateTargetPoint( const rcsc::WorldModel & wm,
                                                      const rcsc::Vector2D & home_pos,
                                                      const rcsc::Vector2D & ball_pos,
                                                      const rcsc::Vector2D & target_point )
{
    (void)wm;

    double value = 0.0;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM, "(evaluate)>>>>> (%.1f %.1f)",
                  target_point.x, target_point.y );
#endif

    {
        const double home_y_diff = std::fabs( home_pos.y - target_point.y );
        double tmp_value = 0.05 * home_y_diff;
        value += tmp_value;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM, "home_y_diff=%.2f value=%f [%f]",
                      home_y_diff, tmp_value, value );
#endif
    }
    {
        const double ball_y_diff = std::fabs( ball_pos.y - target_point.y );
        double tmp_value = -ball_y_diff;
        value += tmp_value;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM, "ball_y_diff=%.2f value=%f [%f]",
                      ball_y_diff, tmp_value, value );
#endif
    }
    // {
    //     const double goal_y_diff = target_point.absY();
    //     //value = -0.1 * goal_y_diff;
    // }
#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM, "<<<<< result value=%f", value );
#endif
    return value;
}
