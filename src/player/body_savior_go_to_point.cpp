// -*-c++-*-

/*!
  \file body_savior_go_to_point.cpp
*/

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

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "body_savior_go_to_point.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/math_util.h>
#include <rcsc/action/body_turn_to_angle.h>


using namespace rcsc;

GameTime Body_SaviorGoToPoint::S_cache_time;
Vector2D Body_SaviorGoToPoint::S_cache_target_point;
double Body_SaviorGoToPoint::S_cache_max_error = 0.0;
double Body_SaviorGoToPoint::S_cache_max_power = 0.0;
bool Body_SaviorGoToPoint::S_cache_use_back_dash = true;
bool Body_SaviorGoToPoint::S_cache_force_back_dash = false;
bool Body_SaviorGoToPoint::S_cache_emergency_mode = false;
int Body_SaviorGoToPoint::S_turn_count = 0;

namespace {

double
s_required_dash_power_to_reach_next_step( const PlayerType & type,
                                          double dist )
{
    const double needed_power = dist / ( type.dashPowerRate() * type.effortMax() );
    const double max_power = ServerParam::i().maxDashPower();
    const double min_power = ServerParam::i().minDashPower();

    if ( needed_power >= max_power )
    {
        return max_power;
    }
    else if ( needed_power <= min_power )
    {
        return min_power;
    }
    else
    {
        return needed_power;
    }
}

}

/*-------------------------------------------------------------------*/
/*!

 */
Body_SaviorGoToPoint::Body_SaviorGoToPoint( const Vector2D & target_point,
                                            const double max_error,
                                            const double max_power,
                                            const bool use_back_dash,
                                            const bool force_back_dash,
                                            const bool emergency_mode,
                                            const bool look_ball )
    : M_target_point( target_point ),
      M_max_error( max_error ),
      M_max_power( max_power ),
      M_use_back_dash( use_back_dash ),
      M_force_back_dash( force_back_dash ),
      M_emergency_mode( emergency_mode ),
      M_look_ball( look_ball )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Body_SaviorGoToPoint::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    //
    // update cache
    //
    if ( wm.time().cycle() > S_cache_time.cycle() + 1
         || (S_cache_target_point - M_target_point).r() > EPS
         || std::fabs( S_cache_max_error - M_max_error ) > EPS
         || S_cache_use_back_dash != M_use_back_dash
         || std::fabs( S_cache_max_power - M_max_error ) > EPS
         || S_cache_force_back_dash != M_force_back_dash
         || S_cache_emergency_mode != M_emergency_mode )
    {
        S_turn_count = 0;
    }

    S_cache_time = wm.time();
    S_cache_target_point = M_target_point;
    S_cache_max_error = M_max_error;
    S_cache_use_back_dash = M_use_back_dash;
    S_cache_max_power = M_max_power;
    S_cache_force_back_dash = M_force_back_dash;
    S_cache_emergency_mode = M_emergency_mode;

    dlog.addText( Logger::ROLE,
                  __FILE__": target_point=[%.2f, %.2f] dist_thr=%.3f",
                  M_target_point.x, M_target_point.y, M_max_error );

    dlog.addText( Logger::ROLE,
                  "__ emergency_mode = %s",
                  ( M_emergency_mode ? "true" : "false" ) );

    dlog.addText( Logger::ROLE,
                  "__ look_ball = %s",
                  ( M_look_ball ? "true" : "false" ) );

    dlog.addText( Logger::ROLE,
                  "__ self pos [%.2f, %.2f](%.2f)",
                  wm.self().pos().x, wm.self().pos().y,
                  wm.self().body().degree() );

    // already there
    if ( wm.self().pos().dist2( M_target_point ) <= std::pow( M_max_error, 2 ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__ ": no need to do" );

        return false;
    }

    const double ball_dist_from_goal = wm.ball().pos().dist( ServerParam::i().ourTeamGoalPos() );

    dlog.addText( Logger::ROLE,
                  __FILE__ ": ball dist from goal %.3f", ball_dist_from_goal );

    if ( M_look_ball
         && ball_dist_from_goal < 25.0 )
    {
        if ( doMoveLookBall( agent ) )
        {
            return true;
        }
    }


    const Vector2D next_self_pos = wm.self().inertiaPoint( 1 );
    const Vector2D target_rel = M_target_point - next_self_pos;
    const double target_dist = std::max( target_rel.r(), 0.01 );
    const AngleDeg target_angle = target_rel.th();

    dlog.addText( Logger::ROLE,
                  __FILE__": next self pos [%.2f, %.2f]",
                  next_self_pos.x, next_self_pos.y );

    dlog.addText( Logger::ROLE,
                  __FILE__": dist_thr=%.3f target_dist=%.3f",
                  M_max_error, target_dist );

    const AngleDeg angle_diff = target_angle - wm.self().body();

    double turn_limit = 180.0;
    if ( M_max_error < target_dist )
    {
        turn_limit = AngleDeg::asin_deg( M_max_error / target_dist );

        double minimum_turn_limit = 12.0;
        if ( target_dist <= 10.0 )
        {
            minimum_turn_limit = 10.0;
        }
        else if ( target_dist < 30.0 )
        {
            minimum_turn_limit = 10.0 + 2.0 * ( ( target_dist - 10.0 ) / 20.0 );
        }
        else
        {
            minimum_turn_limit = 12.0;
        }

        dlog.addText( Logger::ROLE,
                      __FILE__": turn_limit=%.2f minimum_turn_limit=%.2f",
                      turn_limit, minimum_turn_limit );

        if ( turn_limit < minimum_turn_limit )
        {
            turn_limit = minimum_turn_limit;
        }
    }

    if ( wm.self().goalie()
         && wm.gameMode().isPenaltyKickMode() )
    {
        double dist_to_target = M_target_point.dist( wm.self().pos() );
        double increase_rate = std::max( 1.0, dist_to_target / 5.0 ) * 2.0; // MAGIC NUMBER

        turn_limit *= increase_rate;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": angle_diff=%.3f, turn_limit=%.3f",
                  angle_diff.degree(), turn_limit );


    bool back_dash = false;

    const double back_angle_diff = ( angle_diff + 180.0 ).abs();
    const bool back_dash_stamina_ok
        = ( M_emergency_mode
            || ( wm.self().stamina() > ServerParam::i().staminaMax() / 2.0
                 && wm.self().stamina() > ServerParam::i().recoverDecThrValue() + 500.0 ) );


    //
    // Back Dash
    //
    if ( ! M_look_ball
         && ball_dist_from_goal < 25.0 )
    {
        double back_dash_dist = 10.0;
        if ( M_emergency_mode )
        {
            back_dash_dist = 15.0;
        }

        dlog.addText( Logger::ROLE,
                      __FILE__": back dash stamina ok = %s",
                      ( back_dash_stamina_ok ? "true" : "false" ) );

        dlog.addText( Logger::ROLE,
                      __FILE__": back angle_diff = %.3f",
                      back_angle_diff );
        dlog.addText( Logger::ROLE,
                      __FILE__": target_dist=%.3f, back_dash_dist=%.3f",
                      target_dist, back_dash_dist );

        if ( ( ( M_use_back_dash && target_dist <= back_dash_dist )
               || M_force_back_dash )
             && back_angle_diff <= turn_limit
             && back_dash_stamina_ok )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__ ": back_dash true(1)" );
            back_dash = true;
        }

        if ( ! back_dash
             && back_dash_stamina_ok
             && target_dist <= back_dash_dist
             && back_angle_diff < angle_diff.abs() )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__ ": back_dash true(2)" );
            back_dash = true;
        }
    }

    //
    // omnidir dash
    //
    if ( ball_dist_from_goal < 25.0
         && target_dist < 5.0 )
    {
        const int teammate_step = wm.interceptTable()->teammateReachStep();
        const int opponent_step = wm.interceptTable()->opponentReachStep();

        if ( opponent_step <= teammate_step + 5 )
        {
            if ( tryOmniDash( agent, false ) )
            {
                return true;
            }

            if ( tryOmniDash( agent, true ) )
            {
                return true;
            }
        }
    }

    //
    // Turn
    //
    if ( ! back_dash
         && ( S_turn_count <= 3
              && ( S_turn_count < 1
                   || angle_diff.abs() > 30.0 ) ) )
    {
        if ( angle_diff.abs() > turn_limit )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__ ": TURN  S_turn_count=%d", S_turn_count );

            ++S_turn_count;

            if ( Body_TurnToAngle( target_angle ).execute( agent ) )
            {
                return true;
            }
        }
    }

    // back dash turn
    if ( back_dash
         && ( S_turn_count <= 3
             && ( S_turn_count < 1
                  || back_angle_diff > 30.0 ) ) )
    {
        if ( back_angle_diff > turn_limit )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__ ": BACK_TURN  S_turn_count=%d", S_turn_count );

            agent->debugClient().addLine( wm.self().pos()
                                          + Vector2D( -5.0, -5.0 ),
                                          wm.self().pos()
                                          + Vector2D( +5.0, +5.0 ) );
            agent->debugClient().addLine( wm.self().pos()
                                          + Vector2D( -5.0, +5.0 ),
                                          wm.self().pos()
                                          + Vector2D( +5.0, -5.0 ) );

            ++S_turn_count;

            if ( Body_TurnToAngle( target_angle + 180.0 ).execute( agent ) )
            {
                return true;
            }
        }
    }

#if 0
    double move_distance = target_dist * ( target_angle - wm.self().body() ).cos();
    double required_power = bound( ServerParam::i().minDashPower(),
                                   move_distance / wm.self().dashRate(),
                                   ServerParam::i().maxDashPower() );
    double dash_power = wm.self().getSafetyDashPower( std::min( M_max_power, required_power ) );

    if ( ball_dist_from_goal > 50.0 )
    {
        double my_inc = wm.self().playerType().staminaIncMax() * wm.self().recovery();
        dash_power = std::min( dash_power, my_inc * 0.9 );
    }

    //
    // Dash
    //
    dlog.addText( Logger::ROLE,
                  __FILE__ ": DASH power=%.3f", dash_power );

    return agent->doDash( dash_power );
#else
    doAdjustDash( agent );
    return true;
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Body_SaviorGoToPoint::doAdjustDash( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();
    const PlayerType & ptype = wm.self().playerType();

    const int intercept_step = std::min( wm.interceptTable()->teammateReachStep(),
                                         wm.interceptTable()->opponentReachStep() );
    int move_step = std::max( 1, intercept_step - 1 );
    if ( wm.ball().distFromSelf() > 20.0 )
    {
        move_step = std::max( 2, move_step );
    }

    //
    // TODO: change move step according to the body angle
    //

    Vector2D required_vel = M_target_point - wm.self().pos();
    required_vel *= ( ( 1.0 - ptype.playerDecay() )
                      / ( 1.0 - std::pow( ptype.playerDecay(), move_step ) ) );
    Vector2D required_accel = required_vel - wm.self().vel();
    required_accel.rotate( -wm.self().body() );

    double dash_power = required_accel.x / wm.self().dashRate();
    dash_power = wm.self().getSafetyDashPower( dash_power );

    if ( wm.ball().pos().dist2( SP.ourTeamGoalPos() ) > std::pow( 50.0, 2 ) )
    {
        double my_inc = wm.self().playerType().staminaIncMax() * wm.self().recovery();
        dash_power = std::min( dash_power, my_inc * 0.9 );
    }

    dlog.addText( Logger::ROLE,
                  __FILE__ ":(doAdjustDash) move_step=%d", move_step );
    dlog.addText( Logger::ROLE,
                  __FILE__ ":(doAdjustDash) required_accel=(%.3f %.3f) required_power=%.1f",
                  required_accel.x, required_accel.y,
                  required_accel.x / wm.self().dashRate() );
    dlog.addText( Logger::ROLE,
                  __FILE__ ": DASH power=%.3f", dash_power );

    agent->doDash( dash_power );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Body_SaviorGoToPoint::doMoveLookBall( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const Vector2D self_pos = wm.self().inertiaFinalPoint();
    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();

    const double target_dist = self_pos.dist( M_target_point );
    const AngleDeg target_angle = ( M_target_point - self_pos ).th();
    const AngleDeg ball_angle = ( ball_next - self_pos ).th();

    dlog.addText( Logger::ROLE,
                  __FILE__ ":(doMoveLookBall) start" );

    if ( ( target_angle - ball_angle ).abs()
         < SP.maxNeckAngle() + std::max( 0.0, ViewWidth::width( ViewWidth::NARROW ) * 0.5 - 10.0 ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__ ":(doMoveLookBall) can move to the point by normal mode" );
        return false;
    }

    //
    // turn
    //
    {
        const AngleDeg target_reverse_angle = ( self_pos - M_target_point ).th();
        double dir_margin = 10.0;
        if ( M_max_error < target_dist )
        {
            dir_margin = std::max( 10.0,
                                   AngleDeg::asin_deg( M_max_error / self_pos.dist( M_target_point ) ) );
        }

        if ( ( target_reverse_angle - wm.self().body() ).abs() > dir_margin )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__ ":(doMoveLookBall) TURN" );
            Body_TurnToAngle( target_reverse_angle ).execute( agent );
            return true;
        }
    }

    //
    // omni dash, only back
    //
    if ( tryOmniDash( agent, true ) )
    {
        dlog.addText( Logger::ROLE,
              __FILE__ ":(doMoveLookBall) OMNI DASH BACK" );
        return true;
    }

    double dash_power = wm.self().staminaModel().getSafetyDashPower( wm.self().playerType(),
                                                                     SP.minDashPower() );

    dlog.addText( Logger::ROLE,
                  __FILE__ ":(doMoveLookBall) DASH BACK power=%.1f", dash_power );

    agent->doDash( dash_power );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Body_SaviorGoToPoint::tryOmniDash( PlayerAgent * agent,
                                   const bool back_dash )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();
    const PlayerType & ptype = wm.self().playerType();

    Vector2D self_next = wm.self().pos() + wm.self().vel();

    Vector2D target_rel = M_target_point - self_next;

    AngleDeg body_angle = wm.self().body() + ( back_dash ? 180.0 : 0.0 );
    target_rel.rotate( -body_angle );

    dlog.addText( Logger::ROLE,
                  __FILE__ ":(tryOmniDash) %s target_rel=(%.3f %.3f)",
                  back_dash ? "back" : "forward",
                  target_rel.x, target_rel.y );

    if ( target_rel.x < 0.0 )
    {
        return false;
    }

    if ( target_rel.absY() < M_max_error * 0.5 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__ ":(tryOmniDash) small y error %.3f", target_rel.y );
        return false;
    }

    if ( target_rel.absY() > M_max_error + 1.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__ ":(tryOmniDash) big y diff %.3f", target_rel.y );
        return false;
    }

    StaminaModel stamina_model = wm.self().staminaModel();

    double max_dash_power = stamina_model.getSafetyDashPower( ptype,
                                                              back_dash
                                                              ? SP.minDashPower()
                                                              : SP.maxDashPower() );

    double max_x_accel45 = std::fabs( max_dash_power )
        * ( ptype.dashPowerRate()
            * SP.dashDirRate( 45.0 )
            * stamina_model.effort() )
        * AngleDeg::cos_deg( 45.0 );
    double max_y_accel45 = std::fabs( max_dash_power )
        * ( ptype.dashPowerRate()
            * SP.dashDirRate( 45.0 )
            * stamina_model.effort() )
        * AngleDeg::sin_deg( 45.0 );

    dlog.addText( Logger::ROLE,
                  __FILE__ ":(tryOmniDash)(45) max_x_accel=%.3f max_y_accel=%.3f",
                  max_x_accel45, max_y_accel45 );

    if ( target_rel.x > max_x_accel45
         && target_rel.absY() > max_y_accel45 )
    {
        double dash_power = max_dash_power;
        double dash_dir = 45.0 * sign( target_rel.y );
        dlog.addText( Logger::ROLE,
                      __FILE__ ":(tryOmniDash)(45)(1) power=%.1f dir=%.1f",
                      dash_power, dash_dir );
        agent->doDash( dash_power, dash_dir );
        return true;
    }

    if ( ! back_dash )
    {
        if ( target_rel.x < max_x_accel45
             && target_rel.absY() > max_y_accel45 )
        {
            double dash_power = std::min( max_dash_power,
                                          target_rel.absY() / ( ptype.dashPowerRate()
                                                                * SP.dashDirRate( 90.0 )
                                                                * stamina_model.effort()
                                                                * AngleDeg::sin_deg( 90.0 ) ) );
            double dash_dir = 90.0 * sign( target_rel.y );
            dlog.addText( Logger::ROLE,
                          __FILE__ ":(tryOmniDash)(90) power=%.1f dir=%.1f",
                          dash_power, dash_dir );
            agent->doDash( dash_power, dash_dir );
            return true;
        }
    }

    if ( target_rel.x < max_x_accel45
         && target_rel.absY() < max_y_accel45 )
    {
        double dash_power = std::min( target_rel.x / ( ptype.dashPowerRate()
                                                       * SP.dashDirRate( 45.0 )
                                                       * stamina_model.effort()
                                                       * AngleDeg::cos_deg( 45.0 ) ),
                                      target_rel.absY() / ( ptype.dashPowerRate()
                                                            * SP.dashDirRate( 45.0 )
                                                            * stamina_model.effort()
                                                            * AngleDeg::sin_deg( 45.0 ) ) );
        dash_power = std::min( std::fabs( max_dash_power ), dash_power );
        if ( back_dash ) dash_power = -dash_power;
        double dash_dir = 45.0 * sign( target_rel.y );
        dlog.addText( Logger::ROLE,
                      __FILE__ ":(tryOmniDash)45(2) power=%.1f dir=%.1f",
                      dash_power, dash_dir );
        agent->doDash( dash_power, dash_dir );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__ ":(tryOmniDash) failed" );

    return false;
}
