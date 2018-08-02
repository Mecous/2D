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

#include "bhv_set_play_goal_kick.h"

#include "strategy.h"

#include "bhv_chain_action.h"
#include "bhv_set_play.h"
#include "bhv_set_play_avoid_mark_move.h"
#include "bhv_prepare_set_play_kick.h"
#include "bhv_go_to_static_ball.h"
#include "bhv_clear_ball.h"

#include "intention_wait_after_set_play_kick.h"

#include "field_analyzer.h"

#include <rcsc/action/body_clear_ball.h>
#include <rcsc/action/body_stop_ball.h>
#include <rcsc/action/body_kick_to_relative.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_pass.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/audio_memory.h>

#include <rcsc/geom/rect_2d.h>

#include <rcsc/math_util.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!
  execute action
*/
bool
Bhv_SetPlayGoalKick::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_SetPlayGoalKick" );

    if ( Bhv_SetPlay::is_kicker( agent ) )
    {
        doKick( agent );
    }
    else
    {
        doMove( agent );
        // doMoveOld( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayGoalKick::doKick( PlayerAgent * agent )
{
    if ( doSecondKick( agent ) )
    {
        return;
    }

    // go to ball
    if ( Bhv_GoToStaticBall( 0.0 ).execute( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKick) go to ball" );
        // tring to reach the ball yet
        return;
    }

    // already ball point

    if ( doKickerWait( agent ) )
    {
        return;
    }

    if ( doPass( agent ) )
    {
        return;
    }

    if ( doKickToFarSide( agent ) )
    {
        return;
    }

    const WorldModel & wm = agent->world();

    if ( wm.getSetPlayCount() <= ServerParam::i().dropBallTime() - 30 )
    {
        agent->debugClient().addMessage( "GoalKick:FinalWait%d", wm.getSetPlayCount() );
        Body_TurnToBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return;
    }

    agent->debugClient().addMessage( "GoalKick:Clear" );
    dlog.addText( Logger::TEAM,
                  __FILE__": clear ball" );

    Bhv_ClearBall().execute( agent );
    //agent->setNeckAction( new Neck_ScanField() );
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayGoalKick::doSecondKick( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.ball().pos().x < -ServerParam::i().pitchHalfLength() + ServerParam::i().goalAreaLength() + 1.0
         && wm.ball().pos().absY() < ServerParam::i().goalAreaWidth() * 0.5 + 1.0 )
    {
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (doSecondKick) ball is moving." );

    if ( wm.self().isKickable() )
    {
        if ( doStopBall( agent ) )
        {
            agent->debugClient().addMessage( "GoalKick:Stop" );
            dlog.addText( Logger::TEAM,
                          __FILE__": (doSecondKick) stop the ball" );
            return true;
        }

        if ( doPass( agent ) )
        {
            return true;
        }

        if ( doHoldBall( agent ) )
        {
            return true;
        }

        agent->debugClient().addMessage( "GoalKick:Clear" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doSecondKick) clear ball" );

        Bhv_ClearBall().execute( agent );
        //agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( doIntercept( agent ) )
    {
        return true;
    }

    Vector2D ball_final = wm.ball().inertiaFinalPoint();

    agent->debugClient().addMessage( "GoalKick:GoTo" );
    agent->debugClient().setTarget( ball_final );
    dlog.addText( Logger::TEAM,
                  __FILE__": (doSecondKick) go to ball final point(%.2f %.2f)",
                  ball_final.x, ball_final.y );

    if ( ! Body_GoToPoint( ball_final,
                           2.0,
                           ServerParam::i().maxDashPower() ).execute( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doSecondKick) turn to center" );
        Body_TurnToPoint( Vector2D( 0.0, 0.0 ) ).execute( agent );
    }

    agent->setNeckAction( new Neck_ScanField() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayGoalKick::doKickerWait( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.getSetPlayCount() >= getKickStartCount() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) real set play count = %d > drop_time-30, no wait",
                      wm.getSetPlayCount() );
        return false;
    }

    agent->debugClient().addMessage( "GoalKick:Wait%d", wm.getSetPlayCount() );
    dlog.addText( Logger::TEAM,
                  __FILE__": (doKickWait) wait. setplay count %d", wm.getSetPlayCount() );

    Body_TurnToBall().execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayGoalKick::doPass( PlayerAgent * agent )
{
    if ( Bhv_ChainAction().execute( agent ) )
    {
        agent->setIntention( new IntentionWaitAfterSetPlayKick() );
        return true;
    }

    return false;
}
/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayGoalKick::doKickToFarSide( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point( ServerParam::i().ourPenaltyAreaLineX() - 5.0,
                           ServerParam::i().penaltyAreaHalfWidth() );
    if ( wm.ball().pos().y > 0.0 )
    {
        target_point.y *= -1.0;
    }

    double ball_move_dist = wm.ball().pos().dist( target_point );
    double ball_first_speed = calc_first_term_geom_series_last( 0.7,
                                                                ball_move_dist,
                                                                ServerParam::i().ballDecay() );
    ball_first_speed = std::min( ServerParam::i().ballSpeedMax(),
                                 ball_first_speed );
    ball_first_speed = std::min( wm.self().kickRate() * ServerParam::i().maxPower(),
                                 ball_first_speed );

    Vector2D accel = target_point - wm.ball().pos();
    accel.setLength( ball_first_speed );

    double kick_power = std::min( ServerParam::i().maxPower(),
                                  accel.r() / wm.self().kickRate() );
    AngleDeg kick_angle = accel.th();


    dlog.addText( Logger::TEAM,
                  __FILE__" (doKickToFarSide) target=(%.2f %.2f) dist=%.3f ball_speed=%.3f",
                  target_point.x, target_point.y,
                  ball_move_dist,
                  ball_first_speed );
    dlog.addText( Logger::TEAM,
                  __FILE__" (doKickToFarSide) kick_power=%f kick_angle=%.1f",
                  kick_power,
                  kick_angle.degree() );

    agent->doKick( kick_power, kick_angle - wm.self().body() );

    agent->setNeckAction( new Neck_ScanField() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayGoalKick::doIntercept( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    if ( wm.ball().pos().x < -SP.pitchHalfLength() + SP.goalAreaLength() + 1.0
         && wm.ball().pos().absY() < SP.goalAreaWidth() * 0.5 + 1.0 )
    {
        return false;
    }

    if ( wm.self().isKickable() )
    {
        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    if ( self_min > mate_min )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doIntercept) other ball kicker" );
        return false;
    }

    if ( Body_Intercept().execute( agent ) )
    {
        agent->debugClient().addMessage( "GoalKick:Intercept" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doIntercept) intercept" );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    agent->debugClient().addMessage( "GoalKick:GetBall" );
    dlog.addText( Logger::TEAM,
                  __FILE__": (doIntercept) move to get the ball" );

    Vector2D trap_pos = wm.ball().inertiaPoint( self_min + 1 );
    trap_pos.x -= wm.self().playerType().playerSize();

    if( Body_GoToPoint( trap_pos,
                        SP.ballSize(),
                        SP.maxDashPower() ).execute( agent ) )
    {
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    Body_TurnToPoint( trap_pos ).execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayGoalKick::doStopBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D next_ball_pos = wm.ball().pos() + wm.ball().vel();
    Vector2D next_self_pos = wm.self().pos() + wm.self().vel();

    if ( wm.ball().vel().r2() < 0.01
         && next_self_pos.dist( next_ball_pos ) < wm.self().playerType().kickableArea() * 0.8 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (doStopBall) ball already stops and player will be kickable" );
        return false;
    }

    //
    // try to collide with ball
    //

    // the target relative position is next self position (== self velocity vector)

    // Vector2D target_rel_pos = wm.self().vel();
    Vector2D required_accel = wm.self().vel();
    required_accel -= wm.ball().rpos();
    required_accel -= wm.ball().vel();

    double kick_power = required_accel.r()/ wm.self().kickRate();
    if ( kick_power < ServerParam::i().maxPower() * 1.1 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (doStopBall) try collision. kick_power=%f",
                      kick_power );
        agent->debugClient().addMessage( "GoalKick:Collide" );

        kick_power = std::min( kick_power, ServerParam::i().maxPower() );
        agent->doKick( kick_power, required_accel.th() - wm.self().body() );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    const double hold_dist
        = wm.self().playerType().playerSize()
        + ServerParam::i().ballSize();

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (doStopBall) try hold. dist=%.3f", hold_dist );
    agent->debugClient().addMessage( "GoalKick:Hold" );

    if ( Body_KickToRelative( hold_dist,
                              AngleDeg( 0.0 ), // at the front of agent body
                              true ).execute( agent ) )
    {
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayGoalKick::doHoldBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.getSetPlayCount() >= ServerParam::i().dropBallTime() - 10 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doHoldBall) set play count(%d), no wait",
                      wm.getSetPlayCount() );
        return false;
    }

    const double capacity_thr = ServerParam::i().staminaCapacity() * Strategy::get_remaining_time_rate( wm );

    if ( wm.getSetPlayCount() >= 15
         && wm.seeTime() == wm.time()
         && wm.self().stamina() > ServerParam::i().staminaMax() * 0.9
         && wm.self().staminaCapacity() > capacity_thr )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doHoldBall) set play count = %d, force kick mode",
                      wm.getSetPlayCount() );
        return false;
    }

    agent->debugClient().addMessage( "GoalKick:TurnToBall" );
    Body_TurnToBall().execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayGoalKick::doMoveOld( PlayerAgent * agent )
{
    if ( doIntercept( agent ) )
    {
        return;
    }

    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const Rect2D penalty_area( Vector2D( SP.ourTeamGoalLineX(),
                                         - ( SP.penaltyAreaHalfWidth() + 1.0 ) ),
                               Size2D( SP.penaltyAreaLength() + 1.0,
                                       ( SP.penaltyAreaHalfWidth() + 1.0 ) * 2.0 ) );

    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );

    target_point.y += wm.ball().pos().y * 0.5;
    if ( target_point.absY() > SP.pitchHalfWidth() - 1.0 )
    {
        target_point.y = sign( target_point.y ) * ( SP.pitchHalfWidth() - 1.0 );
    }

    int teammate_count = 1;
    int opponent_count = 0;

    for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromSelf().begin(),
              end = wm.teammatesFromSelf().end();
          t != end;
          ++t )
    {
        if ( target_point.dist( (*t)->pos() ) < 5.0 )
        {
            ++teammate_count;
        }
    }

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        if ( target_point.dist( (*o)->pos() ) < 5.0 )
        {
            ++opponent_count;
        }
    }

    if ( opponent_count > teammate_count
         || wm.self().stamina() < SP.staminaMax() * 0.6 )
    {

    }
    else
    {
        if ( ! penalty_area.contains( target_point )
             && ! penalty_area.contains( wm.self().pos() ) )
        {
            if ( Bhv_SetPlayAvoidMarkMove().execute( agent ) )
            {
                return;
            }
        }
    }

    agent->debugClient().addMessage( "GoalKickMove" );
    agent->debugClient().setTarget( target_point );

    if ( ! Body_GoToPoint( target_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Body_TurnToBall().execute( agent );
    }

    if ( wm.self().pos().dist( target_point ) > wm.ball().pos().dist( target_point ) * 0.2 + 6.0
         || wm.self().stamina() < SP.staminaMax() * 0.7
         || wm.self().staminaCapacity() < SP.staminaCapacity() * Strategy::get_remaining_time_rate( wm ) )
    {
        if ( ! wm.self().staminaModel().capacityIsEmpty() )
        {
            agent->debugClient().addMessage( "SayWait" );
            agent->addSayMessage( new WaitRequestMessage() );
        }
    }

    agent->setNeckAction( new Neck_ScanField() );
}


/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayGoalKick::doMove( PlayerAgent * agent )
{
    if ( doIntercept( agent ) )
    {
        return;
    }

    if ( getMoveStartCount() < agent->world().getSetPlayCount() )
    {
        doSecondMove( agent );
    }
    else
    {
        doFirstMove( agent );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayGoalKick::doFirstMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point;

    StaminaModel stamina_model = wm.self().staminaModel();
    stamina_model.simulateWaits( wm.self().playerType(),
                                 std::max( 0, getMoveStartCount() - wm.getSetPlayCount() ) );

    if ( stamina_model.stamina() > ServerParam::i().staminaMax() * 0.5 )
    {
        target_point = getFirstMoveTarget( agent );
    }
    else
    {
        target_point = getSecondMoveTarget( agent );
    }

    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
    double dist_thr = wm.ball().pos().dist( target_point ) * 0.07;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    agent->debugClient().addMessage( "GoalKickMove1st" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );
    dlog.addText( Logger::TEAM,
                  __FILE__":(doFirstMove) target=(%.2f %.2f)",
                  target_point.x, target_point.y );

    if ( ! Body_GoToPoint( target_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Body_TurnToBall().execute( agent );
    }

    agent->setNeckAction( new Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayGoalKick::doSecondMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point = getSecondMoveTarget( agent );

    double dash_power = ServerParam::i().maxDashPower();
    double dist_thr = wm.ball().pos().dist( target_point ) * 0.07;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    agent->debugClient().addMessage( "GoalKickMove2nd" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Body_TurnToBall().execute( agent );
    }

    agent->setNeckAction( new Neck_ScanField() );

    if ( wm.self().armMovable() == 0 )
    {
        agent->setArmAction( new Arm_PointToPoint( target_point ) );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SetPlayGoalKick::getFirstMoveTarget( const PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );

    double rate = bound( SP.ourPenaltyAreaLineX(), target_point.x, 0.0 ) / SP.ourPenaltyAreaLineX();
    double shift = std::fabs( 8.0 * rate );

    dlog.addText( Logger::TEAM,
                  __FILE__":(getFirstMoveTarget) base=(%.2f %.2f) rate=%.3f shift=%.3f",
                  target_point.x, target_point.y, rate, shift );

    double adjust_x = target_point.x;
    double adjust_y = ( wm.ball().pos().y < 0.0
                        ? target_point.y + shift
                        : target_point.y - shift );

    if ( std::fabs( adjust_y ) > SP.pitchHalfWidth() - 1.0 )
    {
        // double y_diff = std::fabs( std::fabs( adjust_y ) - ( SP.pitchHalfWidth() - 1.0 ) );
        // adjust_x = target_point.x + std::sqrt( std::pow( shift, 2 ) - std::pow( y_diff, 2 ) );
        adjust_y = sign( adjust_y ) * ( SP.pitchHalfWidth() - 1.0 );
    }

    target_point.assign( adjust_x, adjust_y );

    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SetPlayGoalKick::getSecondMoveTarget( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
    //target_point.y += wm.ball().pos().y * 0.5;

    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_SetPlayGoalKick::getMoveStartCount()
{
    return bound( 10, ServerParam::i().dropBallTime() - 35, 70 );
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_SetPlayGoalKick::getKickStartCount()
{
    return bound( 10, ServerParam::i().dropBallTime() - 25, 80 );
}
