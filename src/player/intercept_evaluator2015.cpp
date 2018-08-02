// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa Akiyama

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

#include "intercept_evaluator2015.h"

#include <rcsc/player/intercept_table.h>
#include <rcsc/player/world_model.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

#include <iostream>
#include <limits>

#define DEBUG_PRINT

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
InterceptEvaluator2015::InterceptEvaluator2015( const bool save_recovery )
    : M_count( 0 ),
      M_save_recovery( save_recovery )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
InterceptEvaluator2015::~InterceptEvaluator2015()
{
    std::cerr << "delete InterceptEvaluator2015" << std::endl;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
InterceptEvaluator2015::evaluate( const WorldModel & wm,
                                  const InterceptInfo & action )
{
    ++M_count;

    if ( M_save_recovery
         && action.staminaType() != InterceptInfo::NORMAL )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::INTERCEPT,
                      "%d: (InterceptEval2015) save recovery",
                      M_count );
#endif
        return -std::numeric_limits< double >::max();
    }

    const ServerParam & SP = ServerParam::i();

    const double max_x = ( SP.keepawayMode()
                           ? SP.keepawayLength() * 0.5 - 1.0
                           : SP.pitchHalfLength() - 1.0 );
    const double max_y = ( SP.keepawayMode()
                           ? SP.keepawayWidth() * 0.5 - 1.0
                           : SP.pitchHalfWidth() - 1.0 );
    //const double first_ball_speed = wm.ball().vel().r();

    const Vector2D ball_pos = wm.ball().inertiaPoint( action.reachCycle() );

    double value = 0.0;

    if ( ball_pos.absX() > max_x
         || ball_pos.absY() > max_y )
    {
        value = -1000.0 - action.reachCycle();
#ifdef DEBUG_PRINT
        dlog.addText( Logger::INTERCEPT,
                      "%d: (InterceptEval2015) out of pitch = %.3f",
                      M_count, value );
#endif
        return value;
    }

    addShootSpotValue( ball_pos, &value );
    addOpponentStepValue( wm, action, &value );
    addTeammateStepValue( wm, action, &value );
    addTurnMomentValue( wm, action, &value );
    addTurnPenalty( wm, action, &value );
    addMoveDistValue( wm, action, &value );
    addBallDistValue( wm, action, &value );
    addBallSpeedPenalty( wm, action, &value );

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
InterceptEvaluator2015::addShootSpotValue( const Vector2D & ball_pos,
                                           double * value )
{
    double spot_x_dist = std::fabs( ball_pos.x - 44.0 );
    double spot_y_dist = ball_pos.absY() * 0.5;
    double spot_dist = std::sqrt( std::pow( spot_x_dist, 2 ) + std::pow( spot_y_dist, 2 ) );
    double tmp_val = spot_dist * -0.2;

    *value += tmp_val;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::INTERCEPT,
                  "%d: (intercept) shoot spot dist = %.3f (%.3f)",
                  M_count, tmp_val, *value );
#endif
}


/*-------------------------------------------------------------------*/
/*!

 */
void
InterceptEvaluator2015::addOpponentStepValue( const WorldModel & wm,
                                              const InterceptInfo & action,
                                              double * value )
{
    const Vector2D ball_pos = wm.ball().inertiaPoint( action.reachCycle() );

    double tmp_val = 0.0;

    if ( wm.gameMode().type() == GameMode::GoalKick_
         && wm.gameMode().side() == wm.ourSide()
         && ball_pos.x < ServerParam::i().ourPenaltyAreaLineX() - 2.0
         && ball_pos.absY() < ServerParam::i().penaltyAreaHalfWidth() - 2.0 )
    {
        // no penalty
    }
    else
    {
        const int opponent_step = wm.interceptTable()->opponentReachStep();
        if ( opponent_step <= action.reachCycle() + 3 )
        {
            tmp_val = ( opponent_step - ( action.reachCycle() + 3 ) ) * 5.0;
        }
    }

    *value += tmp_val;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::INTERCEPT,
                  "%d: (InterceptEval2015) opponent diff = %.3f (%.3f)",
                  M_count, tmp_val, value );
#endif

}

/*-------------------------------------------------------------------*/
/*!

 */
void
InterceptEvaluator2015::addTeammateStepValue( const WorldModel & wm,
                                              const InterceptInfo & action,
                                              double * value )
{
    const int teammate_step = wm.interceptTable()->teammateReachStep();

    double tmp_val = 0.0;

    if ( teammate_step <= action.reachCycle() + 3 )
    {
        tmp_val = ( teammate_step - ( action.reachCycle() + 3 ) ) * 0.5;
    }

    *value += tmp_val;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::INTERCEPT,
                  "%d: (InterceptEval2015) teammate diff = %.3f (%.3f)",
                  M_count, tmp_val, *value );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
InterceptEvaluator2015::addTurnMomentValue( const WorldModel & wm,
                                            const InterceptInfo & action,
                                            double * value )
{
    if ( action.turnCycle() == 0 )
    {
        return;
    }

    if ( action.ballDist() < wm.self().playerType().kickableArea() - 0.3 )
    {
        return;
    }

    const Vector2D self_pos = wm.self().inertiaPoint( action.reachCycle() );
    const Vector2D ball_pos = wm.ball().inertiaPoint( action.reachCycle() );

    const AngleDeg body_angle = ( action.dashPower() < 0
                                  ? ( ball_pos - self_pos ).th() + 180.0
                                  : ( ball_pos - self_pos ).th() );
    const double turn_moment = ( body_angle - wm.self().body() ).abs();

    double tmp_val = std::fabs( turn_moment ) * -0.025;

    *value += tmp_val;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::INTERCEPT,
                  "%d: (InterceptEval2015) turn penalty = %.3f (moment=%.1f) (%.3f)",
                  M_count, tmp_val, turn_moment, *value );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
InterceptEvaluator2015::addTurnPenalty( const WorldModel & wm,
                                        const InterceptInfo & action,
                                        double * value )
{
    if ( action.turnStep() == 0 )
    {
        return;
    }

    double tmp_val = -0.01;
    *value += tmp_val;
#ifdef DEBUG_PRINT
    dlog.addText( Logger::INTERCEPT,
                  "%d: (InterceptEval2015) turn penalty = %.3f (%.3f)",
                  M_count, tmp_val, *value );
#endif

    if ( action.dashPower() < 0.0 )
    {
        const Vector2D self_pos = wm.self().inertiaPoint( action.reachCycle() );
        const Vector2D ball_pos = wm.ball().inertiaPoint( action.reachCycle() );
        const AngleDeg body_angle = ( ball_pos - self_pos ).th() + 180.0;


        tmp_val = std::max( 0.0, body_angle.abs() - 90.0 ) * -0.1;
        *value += tmp_val;

        dlog.addText( Logger::INTERCEPT,
                      "%d: (InterceptEval) turn back penalty = %.3f (%.3f)",
                      M_count, tmp_val, *value );

        if ( ( wm.self().body() - body_angle ).abs() > 90.0 )
        {
            tmp_val = -0.001;
            *value += tmp_val;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::INTERCEPT,
                          "%d: (InterceptEval2015) turn back penalty(2) = %.3f (%.3f)",
                          M_count, tmp_val, *value );
#endif
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
InterceptEvaluator2015::addMoveDistValue( const WorldModel & wm,
                                          const InterceptInfo & action,
                                          double * value )
{
    const Vector2D ball_pos = wm.ball().inertiaPoint( action.reachCycle() );

    double move_dist = action.selfPos().dist( wm.self().pos() );
    double tmp_val = 0.0;

    if ( ball_pos.x < wm.offsideLineX() )
    {
        tmp_val += move_dist * -0.3; //-0.1;
    }

    if ( action.ballDist() > wm.self().playerType().kickableArea() - 0.3 )
    {
        tmp_val += move_dist * -0.5; //-0.2;
    }

    *value += tmp_val;
#ifdef DEBUG_PRINT
    dlog.addText( Logger::INTERCEPT,
                  "%d: (intercept eval) move dist penalty = %.3f (%.3f)",
                  M_count, tmp_val, *value );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
InterceptEvaluator2015::addBallDistValue( const WorldModel & wm,
                                          const InterceptInfo & action,
                                          double * value )
{
    if ( action.ballDist() > wm.self().playerType().kickableArea() - 0.4 )
    {
        double dist = action.ballDist() - ( wm.self().playerType().kickableArea() - 0.4 );
        double tmp_val = dist * -3.0 - 0.5;

        *value += tmp_val;

#ifdef DEBUG_PRINT
        dlog.addText( Logger::INTERCEPT,
                      "%d: (intercept eval) ball dist penalty(1) = %.3f (%.3f)",
                      M_count, tmp_val, value );
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
InterceptEvaluator2015::addBallSpeedPenalty( const WorldModel & wm,
                                             const InterceptInfo & action,
                                             double * value )
{
    const double fast_speed_thr = wm.self().playerType().realSpeedMax() * 0.8;
    const double slow_ball_speed_thr = 0.55;

    const Vector2D self_pos = wm.self().inertiaPoint( action.reachCycle() );
    const Vector2D ball_pos = wm.ball().inertiaPoint( action.reachCycle() );
    const AngleDeg body_angle = ( action.turnStep() == 0
                                  ? wm.self().body()
                                  : action.dashPower() < 0.0
                                  ? ( ball_pos - self_pos ).th() + 180.0
                                  : ( ball_pos - self_pos ).th() );
    const AngleDeg ball_vel_angle = wm.ball().vel().th();
    const double ball_speed = wm.ball().vel().r()
        * std::pow( ServerParam::i().ballDecay(), action.reachStep() );


    if ( ( body_angle - ball_vel_angle ).abs() < 30.0
         && Segment2D( wm.ball().pos(), ball_pos ).dist( wm.self().pos() )
         < wm.self().playerType().kickableArea() - 0.3 )
    {
        if ( ball_speed < fast_speed_thr )
        {
            double tmp_val = ( fast_speed_thr - ball_speed ) * -20.0;
            *value += tmp_val;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::INTERCEPT,
                          "%d: (InterceptEval2015) ball speed penalty(1) = %.3f (%.3f)",
                          M_count, tmp_val, *value );
#endif
        }
    }
    else
    {
        if ( ball_speed < slow_ball_speed_thr ) // magic number
        {
            double tmp_val = std::pow( slow_ball_speed_thr - ball_speed, 2 ) * -70.0;
            *value += tmp_val;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::INTERCEPT,
                          "%d: (InterceptEval2015) ball speed penalty(2) = %.3f (%.3f)",
                          M_count, tmp_val, *value );
#endif
        }
    }
}
