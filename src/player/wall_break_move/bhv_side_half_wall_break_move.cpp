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

#include "bhv_side_half_wall_break_move.h"

#include "../strategy.h"
#include "../field_analyzer.h"
#include "../move_simulator.h"

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

inline
double
get_offside_buf()
{
    return ( Strategy::i().opponentType() == Strategy::Type_WrightEagle
             ? +1.0
             //: -0.5 );
             : -0.75 );
}
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfWallBreakMove::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( ! isMoveSituation( wm ) )
    {
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      __FILE__": no move situation" );
        return false;
    }
    /*
    if ( isReverseSide( agent ) )
    {
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      __FILE__": reverse side" );
        return true;
    }
    */
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

    doGoToPoint( agent, target_point );
    //agent->setIntention( new IntentionPassRequestMove( target_point ) );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideHalfWallBreakMove::doGoToPoint( PlayerAgent * agent,
                                              const Vector2D & target_point )
{
    const WorldModel & wm = agent->world();

    double dist_thr = wm.ball().pos().dist( target_point ) * 0.1 + 0.5;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;
    if ( wm.self().pos().x > wm.offsideLineX() - 0.5 )
    {
        dist_thr = 0.5;
    }
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );


    const double dash_power = ServerParam::i().maxDashPower();

    const int wait_step = getWaitToAvoidOffside( wm, target_point, dist_thr );
    dlog.addText( Logger::TEAM | Logger::ROLE,
                  __FILE__": (doGoToPoint) avoid offside wait = %d", wait_step );

    if ( wait_step > 0 )
    {
        if ( fabs( wm.self().pos().y - target_point.y ) > 5.0 )
        {
            if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                                 -1.0, // dash speed
                                 100, // cycle
                                 true, // save recovery
                                 25.0 // angle threshold
                                 ).execute( agent ) )
            {
                agent->debugClient().addMessage( "SH:WallBreakMustGoTo" );
                dlog.addText( Logger::TEAM | Logger::ROLE,
                              __FILE__": (doGoToPoint) moving to (%.1f %.1f)",
                              target_point.x, target_point.y );
            }
        }
        else
        {
            const Vector2D self_pos = wm.self().inertiaFinalPoint();
            const AngleDeg body_angle = ( target_point - self_pos ).th();

            agent->debugClient().addMessage( "SH:WallBreakPassReqMoveWait" );
            dlog.addText( Logger::TEAM | Logger::ROLE,
                          __FILE__": (doGoToPoint) wait turn" );
            Body_TurnToAngle( body_angle ).execute( agent );
        }
    }
    else if ( doDashAdjust( agent, target_point ) )
    {
        agent->debugClient().addMessage( "SH:WallBreakPassReqMoveAdjust" );
    }
    else if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                              -1.0, // dash speed
                              100, // cycle
                              true, // save recovery
                              25.0 // angle threshold
                              ).execute( agent ) )
    {
        // still moving
        agent->debugClient().addMessage( "SH:WallBreakPassReqMoveGoTo" );
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      __FILE__": (doGoToPoint) moving to (%.1f %.1f)",
                      target_point.x, target_point.y );
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

        agent->debugClient().addMessage( "SH:WallBreakPassReqMoveTurn" );
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      __FILE__": (doGoToPoint) turn to %.1f", body_angle.degree() );
        Body_TurnToAngle( body_angle ).execute( agent );
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( 1 ) );
    agent->setArmAction( new Arm_PointToPoint( target_point ) );
    agent->addSayMessage( new PassRequestMessage( target_point ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfWallBreakMove::doDashAdjust( PlayerAgent * agent,
                                               const Vector2D & target_point )
{
    const WorldModel & wm = agent->world();

    if ( wm.self().pos().x <  wm.offsideLineX() - 2.0 )
    {
        return false;
    }

    const Vector2D self_final = wm.self().inertiaFinalPoint();
    const AngleDeg body_angle = ( target_point - self_final ).th();

    if ( ( wm.self().body() - body_angle ).abs() > 25.0 )
    {
        return false;
    }

    const double offside_x = wm.offsideLineX() + get_offside_buf() - 0.5;
    const int dash_step = 2;

    double move_x = offside_x - self_final.x;
    double move_dist = move_x / body_angle.cos();
    double first_accel = move_dist
        * ( 1.0 - wm.self().playerType().playerDecay() )
        / ( 1.0 - std::pow( wm.self().playerType().playerDecay(), dash_step ) );
    double dash_power = bound( ServerParam::i().minDashPower(),
                               first_accel / wm.self().dashRate(),
                               ServerParam::i().maxDashPower() );
    dash_power = wm.self().getSafetyDashPower( dash_power );

    dlog.addText( Logger::TEAM | Logger::ROLE,
                  "(doDasnOnOffsideLine) self_pos=(%.1f %.1f) offside=%.1f",
                  self_final.x, self_final.y, offside_x );
    dlog.addText( Logger::TEAM | Logger::ROLE,
                  "(doDasnOnOffsideLine) move_x=%.1f move_dist=%.1f dash_power=%.1f",
                  move_x, move_dist, dash_power );

    agent->doDash( dash_power );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_SideHalfWallBreakMove::predictTurnStep( const WorldModel & wm,
                                                  const Vector2D & target_point,
                                                  const double dist_thr,
                                                  const int n_wait,
                                                  const int total_step,
                                                  AngleDeg * dash_angle )
{
    const PlayerType & ptype = wm.self().playerType();
    const double max_moment = ServerParam::i().maxMoment();
    const Vector2D inertia_pos = wm.self().inertiaPoint( total_step );
    const double target_dist = ( target_point - inertia_pos ).r();
    const AngleDeg target_angle = ( target_point - inertia_pos ).th();

    double angle_diff = ( target_angle - wm.self().body() ).abs();

    double turn_margin = 180.0;
    if ( dist_thr < target_dist )
    {
        turn_margin = std::max( 20.0,
                                AngleDeg::asin_deg( dist_thr / target_dist ) );
    }

    int n_turn = 0;
    if ( angle_diff > turn_margin )
    {
        double self_speed = wm.self().vel().r();
        if ( n_wait > 0 ) self_speed *= std::pow( ptype.playerDecay(), n_wait );

        while ( angle_diff > turn_margin )
        {
            angle_diff -= ptype.effectiveTurn( max_moment, self_speed );
            self_speed *= ptype.playerDecay();
            ++n_turn;
        }
    }

    if ( n_turn > 0 )
    {
        angle_diff = std::max( 0.0, angle_diff );
        if ( ( target_angle - wm.self().body() ).degree() > 0.0 )
        {
            *dash_angle = target_angle - angle_diff;
        }
        else
        {
            *dash_angle = target_angle + angle_diff;
        }
    }

    return n_turn;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_SideHalfWallBreakMove::predictOverOffsideDashStep( const WorldModel & wm,
                                                             const AngleDeg & dash_angle,
                                                             const int n_wait_turn,
                                                             const int total_step )
{
    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();
    Vector2D self_pos = wm.self().inertiaPoint( n_wait_turn );
    Vector2D self_vel = wm.self().vel() * std::pow( ptype.playerDecay(), n_wait_turn );
    StaminaModel stamina = wm.self().staminaModel();
    stamina.simulateWaits( ptype, n_wait_turn );

    const double offside_x = wm.offsideLineX() + get_offside_buf();

    const Vector2D accel_unit = Vector2D::from_polar( 1.0, dash_angle );
#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM | Logger::ROLE,
                  "(predictOverOffsideDashStep) n_wait_turn=%d total_step=%d  offside=%.1f",
                  n_wait_turn, total_step, offside_x );
#endif
    for ( int step = n_wait_turn + 1; step <= total_step; ++step )
    {
        double available_stamina = std::max( 0.0, stamina.stamina() - SP.recoverDecThrValue() - 1.0 );
        double dash_power = std::min( SP.maxDashPower(), available_stamina );
        double accel_mag = dash_power * ptype.dashRate( stamina.effort() );
        Vector2D accel = accel_unit * accel_mag;

        self_vel += accel;
        self_pos += self_vel;
        self_vel *= ptype.playerDecay();
        stamina.simulateDash( ptype, dash_power );
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      "(predictOverOffsideDashStep) dash:%d self=(%.1f %.1f)",
                      step - n_wait_turn, self_pos.x, self_pos.y );
#endif
        if ( self_pos.x > offside_x )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM | Logger::ROLE,
                          "(predictOverOffsideDashStep) over offside. n_step=%d", step );
#endif
            return step;
        }
    }
#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM | Logger::ROLE,
                  "(predictOverOffsideDashStep) over offside not found" );
#endif
    return -1;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_SideHalfWallBreakMove::predictOverOffsideStep( const WorldModel & wm,
                                                         const Vector2D & target_point,
                                                         const double dist_thr,
                                                         const int wait_step,
                                                         const int pass_kick_step )
{
    for ( int step = wait_step + 1; step <= pass_kick_step; ++step )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      "(predictOverOffsideStep) step=%d", step );
#endif
        Vector2D self_pos = wm.self().inertiaPoint( step );
        if ( self_pos.x > wm.offsideLineX() )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM | Logger::ROLE,
                          "(predictOverOffsideStep) __ over offside by inertia move" );
#endif
            return step;
        }

        AngleDeg dash_angle = wm.self().body();
        int n_turn = predictTurnStep( wm, target_point, dist_thr, wait_step, step, &dash_angle );

        if ( n_turn > step )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM | Logger::ROLE,
                          "(predictOverOffsideStep) __ (skip) n_turn=%d > step=%d", n_turn, step );
#endif
            continue;
        }
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      "(predictOverOffsideStep) __ n_turn=%d", n_turn );
#endif
        int over_step = predictOverOffsideDashStep( wm, dash_angle, wait_step + n_turn, step );
        if ( over_step > 0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM | Logger::ROLE,
                          "(predictOverOffsideStep) found over step %d", over_step );
#endif
            return over_step;
        }
    }
#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM | Logger::ROLE,
                  "(predictOverOffsideStep) over offside not found" );
#endif
    return -1;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_SideHalfWallBreakMove::getWaitToAvoidOffside( const WorldModel & wm,
                                                        const Vector2D & target_point,
                                                        const double dist_thr )
{
    // if ( wm.kickableTeammate() )
    // {
    //     dlog.addText( Logger::TEAM | Logger::ROLE,
    //                   "(getWaitToAvoidOffside) kickable teammate" );
    //     return 0;
    // }

    if ( target_point.x < wm.offsideLineX() + 0.5 )
    {
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      "(getWaitToAvoidOffside) target.x < offside_x" );
        return 0;
    }

    const PlayerType & ptype = wm.self().playerType();
    const int min_step = static_cast< int >( std::ceil( wm.self().pos().dist( target_point )
                                                        / ptype.realSpeedMax() ) );
    const Vector2D self_inertia = wm.self().inertiaPoint( min_step );

    //const int teammate_step = getTeammateReachStep( wm );
    //const PlayerObject * fastest_teammate = wm.interceptTable()->fastestTeammate();
    const int teammate_step = wm.interceptTable()->teammateReachStep();
    //const int pass_kick_step = teammate_step + ( teammate_step > 1 ? 2 : 1 ); // magic number
    //const int pass_kick_step = teammate_step + ( teammate_step > 1 ? 3 : 2 ); // magic number
    const int pass_kick_step = teammate_step + ( teammate_step > 1 ? 1 : 0 ); // magic number
#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM | Logger::ROLE,
                  "(getWaitToAvoidOffside) teammate_step = %d", teammate_step );
    dlog.addText( Logger::TEAM | Logger::ROLE,
                  "(getWaitToAvoidOffside) pass_kick_step = %d", pass_kick_step );
#endif
    if ( self_inertia.x < wm.offsideLineX() - 3.0 )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      "(getWaitToAvoidOffside) far offside line. no wait" );
#endif
        return 0;
    }

    //if ( self_inertia.x > wm.offsideLineX() + get_offside_buf() )
    const double max_dash = ptype.dashRate( wm.self().effort() ) * ServerParam::i().maxDashPower();
    const Vector2D max_accel = Vector2D::from_polar( max_dash, wm.self().body() );
    if ( self_inertia.x + max_accel.x > wm.offsideLineX() + get_offside_buf() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      "(getWaitToAvoidOffside) self_inertia(%.1f) > offside.x wait_step=%d",
                      self_inertia.x, pass_kick_step );
#endif
        //return pass_kick_step;
        return 1;
    }

    for ( int wait_step = 0; wait_step < pass_kick_step; ++wait_step )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      "=== wait_step=%d  pass_kick_step=%d", wait_step, pass_kick_step );
#endif
        int over_offside_step = predictOverOffsideStep( wm, target_point, dist_thr, wait_step, pass_kick_step );
        if ( over_offside_step < 0
             || over_offside_step >= pass_kick_step )
        {
            dlog.addText( Logger::TEAM | Logger::ROLE,
                          "***found*** pass_kick_step=%d wait_step=%d", pass_kick_step, wait_step );
            return wait_step;
        }
    }
#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM | Logger::ROLE,
                  "***offside not found*** nowait" );
#endif
    return 0;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_SideHalfWallBreakMove::getTeammateReachStep( const WorldModel & wm )
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
Bhv_SideHalfWallBreakMove::isMoveSituation( const WorldModel & wm )
{
    if ( wm.self().pos().x > 37.0 )
    {
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      __FILE__":(isMoveSituation) [false] too front" );
        return false;
    }

    const double offside_buf = get_offside_buf();

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
Bhv_SideHalfWallBreakMove::checkStamina( const WorldModel & wm,
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
#if 0
    dlog.addText( Logger::TEAM,
                  "(heuristic_filter) home_y=%.1f ball_y=%.1f target_y=%.1f",
                  home_pos.y, ball_pos.y, target_point.y );
    dlog.addText( Logger::TEAM,
                  "(heuristic_filter) target_diff=%.1f",
                  std::fabs( target_point.y - home_pos.y ) );
    dlog.addText( Logger::TEAM,
                  "(heuristic_filter) ball_diff=%.1f",
                  std::fabs( ball_pos.y - home_pos.y ) );
#endif

    if ( std::fabs( target_point.y - home_pos.y ) > 30.0 )
    {
        return false;
    }

    if ( std::fabs( home_pos.y - ball_pos.y ) > 40.0 )
    {
        return false;
    }

    //
    // player will keep away from the ball
    //
    if ( ( wm.self().pos().y - target_point.y ) * ( ball_pos.y - target_point.y ) > 0.0
         && std::fabs( wm.self().pos().y - target_point.y ) < ( ball_pos.y - target_point.y ) )
    {
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
Bhv_SideHalfWallBreakMove::getTargetPoint( const rcsc::WorldModel & wm )
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

        for ( double target_y = -34.0; target_y < 34.0; target_y += 4.0 )
        {
            if ( std::fabs( target_y ) < 20.0 ) continue;
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
Bhv_SideHalfWallBreakMove::evaluateTargetPoint( const rcsc::WorldModel & wm,
                                                      const rcsc::Vector2D & home_pos,
                                                      const rcsc::Vector2D & ball_pos,
                                                      const rcsc::Vector2D & target_point )
{
    //(void)wm;

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
        if ( wm.ball().pos().absY() - 22.0 < 0
             && wm.self().pos().y * wm.ball().pos().y > 0 )
        {
            value -= tmp_value;
        }
        else
        {
            value += tmp_value;
        }
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


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfWallBreakMove::isReverseSide( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const int teammate_step = getTeammateReachStep( wm );
    const Vector2D ball_pos = wm.ball().inertiaPoint( teammate_step );
    Vector2D goalie_pos = Vector2D::INVALIDATED;

    if ( home_pos.y * ball_pos.y > 0 )
    {
        return false;
    }

    for( int i = 1; i < 11; i++ )
    {
        const AbstractPlayerObject * opp =  wm.theirPlayer( i );
        if ( opp
             && opp->goalie() )
        {
            goalie_pos = opp->pos();
            break;
        }
    }

    if ( ! goalie_pos.isValid() )
    {
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      __FILE__": unknown goalie pos" );
        return false;
    }

    const Vector2D target_point( wm.offsideLineX(), goalie_pos.y );

    if ( ! target_point.isValid() )
    {
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      __FILE__": no target point" );
        return false;
    }

    double dist_thr = wm.ball().pos().dist( target_point ) * 0.1 + 0.5;

    if ( dist_thr < 1.0 ) dist_thr = 1.0;
    if ( wm.self().pos().x > wm.offsideLineX() - 0.5 )
    {
        dist_thr = 0.5;
    }

    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );


    const double dash_power = ServerParam::i().maxDashPower();
    if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                              -1.0, // dash speed
                              100, // cycle
                              true, // save recovery
                              25.0 // angle threshold
                              ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SH:WallBreakPassReqMoveGoTo" );
        dlog.addText( Logger::TEAM | Logger::ROLE,
                      __FILE__": (doGoToPoint) moving to (%.1f %.1f)",
                      target_point.x, target_point.y );
        return true;
    }
    return false;

}
