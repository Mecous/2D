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

#include "bhv_tackle_intercept.h"

#include "defense_system.h"
#include "field_analyzer.h"
#include "move_simulator.h"

#include <rcsc/action/body_turn_to_point.h>
#include <rcsc/action/neck_turn_to_ball.h>

#include <rcsc/player/intercept_table.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/world_model.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

using namespace rcsc;

namespace {

struct Move {
    int turn_step_;
    int dash_step_;
    double turn_moment_;
    double dash_power_;
    double tackle_prob_;

    Move()
        : turn_step_( -1 ),
          dash_step_( -1 ),
          turn_moment_( 0.0 ),
          dash_power_( 0.0 ),
          tackle_prob_( 0.0 )
      { }
};

/*-------------------------------------------------------------------*/
/*!

 */
int
predict_min_step( const WorldModel & wm,
                  const Vector2D & ball_pos,
                  const int max_step )
{
    const Vector2D inertia_self_pos = wm.self().inertiaPoint( max_step );
    const double inertia_ball_dist = inertia_self_pos.dist( ball_pos );
    int min_step = static_cast< int >( std::ceil( ( inertia_ball_dist - ServerParam::i().tackleDist() )
                                                  / wm.self().playerType().realSpeedMax() ) );

    return std::max( 0, min_step );
}

/*-------------------------------------------------------------------*/
/*!

 */
int
predict_turn_step( const WorldModel & wm,
                   const Vector2D & ball_pos,
                   const int max_step,
                   AngleDeg * result_dash_angle )
{
    const double y_thr = ServerParam::i().tackleWidth() - 0.1;

    const Vector2D inertia_self_pos = wm.self().inertiaPoint( max_step );
    const Vector2D inertia_ball_rel = ( ball_pos - inertia_self_pos ).rotatedVector( -wm.self().body() );

    int n_turn = 0;

    if ( inertia_ball_rel.x < 0.0
         || inertia_ball_rel.absY() > y_thr )
    {
        const PlayerType & ptype = wm.self().playerType();

        const AngleDeg ball_angle = inertia_ball_rel.th();
        const double turn_margin = std::max( 15.0, // Magic Number
                                             AngleDeg::asin_deg( y_thr / inertia_ball_rel.r() ) );

        double angle_diff = ball_angle.abs();
        double speed = wm.self().vel().r();
        while ( angle_diff > turn_margin )
        {
            angle_diff -= ptype.effectiveTurn( ServerParam::i().maxMoment(), speed );
            speed *= ptype.playerDecay();
            ++n_turn;
        }

        if ( angle_diff <= 0.0 )
        {
            *result_dash_angle = wm.self().body() + ball_angle;
        }
        else
        {
            AngleDeg rel_angle = wm.self().body() - ball_angle;
            if ( rel_angle.degree() > 0.0 )
            {
                *result_dash_angle = wm.self().body() + ball_angle + angle_diff;
            }
            else
            {
                *result_dash_angle = wm.self().body() + ball_angle - angle_diff;
            }
        }

    }

    return n_turn;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
can_tackle_after_turn_dash( const WorldModel & wm,
                            const Vector2D & ball_pos,
                            const int max_step,
                            Move * move )
{
    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    const int min_step = predict_min_step( wm, ball_pos, max_step );
    if ( min_step > max_step )
    {
        return false;
    }

    AngleDeg dash_angle = wm.self().body();

    //
    // turn
    //

    int n_turn = predict_turn_step( wm, ball_pos, max_step, &dash_angle );

    if ( n_turn >= max_step )
    {
        return false;
    }

    //
    // dash
    //

    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -dash_angle );

    Vector2D self_pos( 0.0, 0.0 );
    Vector2D self_vel = rotate_matrix.transform( wm.self().vel() );
    StaminaModel stamina_model = wm.self().staminaModel();

    for ( int i = 0; i < n_turn; ++i )
    {
        self_pos += self_vel;
        self_vel *= ptype.playerDecay();
        stamina_model.simulateWait( ptype );
    }

    const Vector2D ball_rel = rotate_matrix.transform( ball_pos - wm.self().pos() );
    const int max_dash_step = max_step - n_turn;

    double first_dash_power = 0.0;
    for ( int i = 0; i < max_dash_step; ++i )
    {
        double required_vel_x = ( ball_rel.x - self_pos.x )
            * ( 1.0 - ptype.playerDecay() )
            / ( 1.0 - std::pow( ptype.playerDecay(), max_dash_step - i ) );
        double required_accel_x = required_vel_x - self_vel.x;
        double dash_power = required_accel_x / ( ptype.dashPowerRate() * stamina_model.effort() );
        dash_power = bound( SP.minDashPower(), dash_power, SP.maxDashPower() );
        dash_power = stamina_model.getSafetyDashPower( ptype, dash_power, 1.0 );

        double accel_x = dash_power * ptype.dashPowerRate() * stamina_model.effort();

        self_vel.x += accel_x;
        self_pos += self_vel;
        self_vel *= ptype.playerDecay();
        stamina_model.simulateDash( ptype, dash_power );

        if ( i == 0 )
        {
            first_dash_power = dash_power;
        }
    }

    if ( ball_rel.x - self_pos.x > SP.tackleDist() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (can_tackle_after_turn_dash) never reach. max_step=%d",
                      max_step );
        return false;
    }

    //
    // success
    //

    double x_dist = std::max( 0.0, ball_rel.x - self_pos.x );
    double y_dist = std::fabs( ball_rel.y - self_pos.y );
    double tackle_fail_prob
        = std::pow( x_dist / SP.tackleDist(), SP.tackleExponent() )
        + std::pow( y_dist / SP.tackleWidth(), SP.tackleExponent() );

    move->turn_step_ = n_turn;
    move->dash_step_ = max_dash_step;
    move->turn_moment_ = ( dash_angle - wm.self().body() ).degree();
    move->dash_power_ = first_dash_power;
    move->tackle_prob_ = std::max( 0.0, 1.0 - tackle_fail_prob );

    dlog.addText( Logger::TEAM,
                  __FILE__": (can_tackle_after_turn_dash) found. step=%d n_turn=%d n_dash=%d dash_angle=%.1f",
                  max_step, n_turn, max_dash_step, dash_angle.degree() );
    dlog.addText( Logger::TEAM,
                  __FILE__": (can_tackle_after_turn_dash) found. step=%d x_dist=%.2f y_dist=%.2f prob=%.3f",
                  max_step, x_dist, y_dist, move->tackle_prob_ );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
Move
get_best_tackle_intercept( const WorldModel & wm )
{
    static GameTime s_last_time( -1, 0 );
    static Move s_last_result;
    if ( s_last_time == wm.time() )
    {
        return s_last_result;
    }
    s_last_time = wm.time();
    s_last_result = Move();

    const ServerParam & SP = ServerParam::i();

    const int max_step = std::min( 30,
                                   std::min( wm.interceptTable()->teammateReachStep() - 1,
                                             wm.interceptTable()->opponentReachStep() - 1 ) );

    Vector2D ball_pos = wm.ball().pos();
    Vector2D ball_vel = wm.ball().vel();

    ball_pos += ball_vel;
    ball_vel *= SP.ballDecay();

    Move best_move;
    best_move.tackle_prob_ = wm.self().tackleProbability() - 1.0e-5;

    for ( int step = 2; step <= max_step; ++step )
    {
        if ( ball_pos.absX() > SP.pitchHalfLength()
             || ball_pos.absY() > SP.pitchHalfWidth() )
        {
            break;
        }

        ball_pos += ball_vel;
        ball_vel *= SP.ballDecay();

        Move move;
        if ( can_tackle_after_turn_dash( wm, ball_pos, step, &move ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (tryTurnDash) OK turn=%d dash=%d tackle_prob=%.3f",
                          move.turn_step_, move.dash_step_, move.tackle_prob_ );

            if ( move.tackle_prob_ > best_move.tackle_prob_ )
            {
                best_move = move;
                if ( best_move.tackle_prob_ > 0.95 )
                {
                    break;
                }
            }
        }
    }

    s_last_result = best_move;
    return s_last_result;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TackleIntercept::execute( PlayerAgent * agent )
{
    if ( ! isTackleSituation( agent ) )
    {
        return false;
    }

    if ( tryOneStepAdjust( agent ) )
    {
        return true;
    }

    if ( tryTurnDash( agent ) )
    {
        return true;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(execute) failed" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TackleIntercept::isTackleSituation( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.kickableTeammate() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(execute) kickable teammate" );
        return false;
    }

    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->teammateReachStep();

    const Vector2D teammate_ball_pos = wm.ball().inertiaPoint( teammate_step );
    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    if ( teammate_step <= opponent_step - 3
         && teammate_ball_pos.absX() < ServerParam::i().pitchHalfLength() - 0.5
         && teammate_ball_pos.absY() < ServerParam::i().pitchHalfWidth() - 0.5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(isTackleSituation) our ball" );
        return false;
    }

    if ( FieldAnalyzer::is_ball_moving_to_our_goal( wm ) )
    {
        // true
        dlog.addText( Logger::TEAM,
                      __FILE__":(isTackleSituation) ball is moving to our goal" );
        return true;
    }
    else
    {
        if ( opponent_ball_pos.absX() > ServerParam::i().pitchHalfLength() + 2.0
             || opponent_ball_pos.absY() > ServerParam::i().pitchHalfWidth() + 2.0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(isTackleSituation) out of pitch" );
            return false;
        }

        if ( wm.kickableOpponent() )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(isTackleSituation exist kickable opponent" );
            return false;
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TackleIntercept::tryOneStepAdjust( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    double dash_power = 0.0;
    double dash_dir = 0.0;
    const double dash_tackle_prob = FieldAnalyzer::get_tackle_probability_after_dash( wm,
                                                                                      &dash_power,
                                                                                      &dash_dir );
    const double turn_tackle_prob = FieldAnalyzer::get_tackle_probability_after_turn( wm );

    const Move best_move = get_best_tackle_intercept( wm );

    dlog.addText( Logger::TEAM,
                  __FILE__": (tryOneStepAdjust) prob: turn=%.3f dash=%.3f move=%.3f",
                  turn_tackle_prob, dash_tackle_prob, best_move.tackle_prob_ );

    if ( turn_tackle_prob > wm.self().tackleProbability()
         && turn_tackle_prob > best_move.tackle_prob_
         && turn_tackle_prob > dash_tackle_prob )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (tryOneStepAdjust) turn" );
        agent->debugClient().addMessage( "TackleIntercept1:Turn" );
        Body_TurnToPoint( wm.ball().pos() + wm.ball().vel(), 1 ).execute( agent );
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    if ( dash_tackle_prob > wm.self().tackleProbability()
         && dash_tackle_prob > best_move.tackle_prob_
         && dash_tackle_prob > turn_tackle_prob )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (tryOneStepAdjust) dash: power=%.2f dir=%.2f",
                      dash_power, dash_dir );
        agent->debugClient().addMessage( "TackleIntercept:Dash:%.0f,%.0f", dash_power, dash_dir );
        agent->doDash( dash_power, dash_dir );
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TackleIntercept::tryTurnDash( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    //
    // check normal intercept
    //
    const int self_step = wm.interceptTable()->selfReachStep();
    const Vector2D self_ball_pos = wm.ball().inertiaPoint( self_step );
    if ( self_ball_pos.absX() < SP.pitchHalfLength()
         && self_ball_pos.absY() < SP.pitchHalfWidth() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (tryTurnDash) normal intercept available" );
        return false;
    }

    //
    // only if shoot block situation
    //
    if ( ! FieldAnalyzer::is_ball_moving_to_our_goal( wm ) )
    {
        return false;
    }

    const Move best_move = get_best_tackle_intercept( wm );

    //
    // action
    //

    if ( best_move.turn_step_ > 0 )
    {
        agent->debugClient().addMessage( "TackleIntercept%d:Turn%.1f",
                                         best_move.turn_step_ + best_move.dash_step_,
                                         best_move.turn_moment_ );
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (tryTurnDash) turn moment=%.1f", best_move.turn_moment_ );
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (tryTurnDash) tackle prob=%.3f", best_move.tackle_prob_ );
        agent->doTurn( best_move.turn_moment_ );
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    if ( best_move.dash_step_ > 0 )
    {
        agent->debugClient().addMessage( "TackleIntercept%d:Dash%.1f",
                                         best_move.turn_step_ + best_move.dash_step_,
                                         best_move.dash_power_ );
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (tryTurnDash) dash power=%.1f", best_move.dash_power_ );
        dlog.addText( Logger::INTERCEPT,
                      __FILE__": (tryTurnDash) tackle prob=%.3f", best_move.tackle_prob_ );
        agent->doDash( best_move.dash_power_ );
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    dlog.addText( Logger::INTERCEPT,
                  __FILE__": (tryTurnDash) false" );
    return false;
}
