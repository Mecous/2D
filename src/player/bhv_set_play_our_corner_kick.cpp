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

#include "bhv_set_play_our_corner_kick.h"

#include "options.h"
#include "strategy.h"

#include "action_chain_holder.h"
#include "action_chain_graph.h"
#include "cooperative_action.h"

#include "bhv_set_play.h"
#include "bhv_set_play_avoid_mark_move.h"
#include "bhv_go_to_static_ball.h"
#include "bhv_chain_action.h"

#include "intention_setplay_move.h"
#include "intention_wait_after_set_play_kick.h"

#include "field_analyzer.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_advance_ball.h>
#include <rcsc/action/body_pass.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/say_message_builder.h>

#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include <rcsc/math_util.h>

#include <algorithm>
#include <limits>

using namespace rcsc;

namespace {

const int g_kick_time_before_drop = 6;


/*!

 */
class IntentionAvoidOffsideAfterCornerKick
    : public SoccerIntention {
private:

    GameTime M_last_time;
    int M_count;
public:

    IntentionAvoidOffsideAfterCornerKick( const GameTime & time )
        : M_last_time( time ),
          M_count( 0 )
      { }

    bool finished( const PlayerAgent * agent );
    bool execute( PlayerAgent * agent );
};

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionAvoidOffsideAfterCornerKick::finished( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    ++M_count;
    if ( M_count >= 13 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(finished) true: count over %d", M_count );
        return true;
    }

    if ( M_last_time.cycle() != wm.time().cycle() - 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(finished) true: illegal time" );
        return true;
    }

    if ( wm.self().pos().x < wm.offsideLineX() - 1.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(finished) true: safe" );
        return true;
    }

    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();
    if ( opponent_step < teammate_step
         || wm.kickableOpponent() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(intention::finished) true: their ball" );
        return true;
    }

    if ( wm.audioMemory().passTime() == wm.time()
         && ! wm.audioMemory().pass().empty()
         && wm.audioMemory().pass().front().receiver_ == wm.self().unum() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(intention::finished) true: receive pass message" );
        return true;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(finished) false" );
    return false;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionAvoidOffsideAfterCornerKick::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point = wm.self().pos();
    target_point.x = std::min( 40.0, wm.offsideLineX() - 1.0 );

    Line2D move_line( wm.self().pos(), target_point );

    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const Vector2D teammate_ball_pos = wm.ball().inertiaPoint( teammate_step );

    if ( move_line.dist( teammate_ball_pos ) < 3.0 )
    {
        Vector2D projection = move_line.projection( wm.ball().pos() );
        target_point = wm.ball().pos() + ( projection - wm.ball().pos() ).setLengthVector( 5.0 );

        dlog.addText( Logger::TEAM,
                      __FILE__":(Intention::execute) update target point (%.2f %.2f)",
                      target_point.x, target_point.y );
    }

    double dist_tolerance = 0.5;
    double dash_power = ServerParam::i().maxDashPower();

    agent->debugClient().addMessage( "IntentionAvoidOffside" );
    agent->debugClient().setTarget( target_point );

    if ( ! Body_GoToPoint( target_point,
                           dist_tolerance,
                           dash_power ).execute( agent ) )
    {
        Body_TurnToBall().execute( agent );
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );

    M_last_time = wm.time();

    return true;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayOurCornerKick::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__":(execute)" );

    if ( Bhv_SetPlay::is_kicker( agent ) )
    {
        doKicker( agent );
    }
    else
    {
        doMove( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayOurCornerKick::doKicker( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    //
    // go to the kick position
    //
    AngleDeg ball_place_angle = ( wm.ball().pos().y > 0.0
                                  ? -90.0
                                  : 90.0 );
    if ( Bhv_GoToStaticBall( ball_place_angle ).execute( agent ) )
    {
        return;
    }

    //
    // wait
    //
    if ( doKickerWait( agent ) )
    {
        return;
    }

    //
    // turn to ball (if necessary)
    //
    if ( doKickerTurnToBall( agent ) )
    {
        return;
    }

    //
    // pass
    //
    if ( doKickerNormalPass( agent ) )
    {
        return;
    }

    //
    // kick to the nearest teammate
    //
    if ( doKickerNearestPass( agent ) )
    {
        return;
    }

    //
    // no pass target. try to kick the ball to somewhere
    //
    doKickerFinal( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayOurCornerKick::doKickerWait( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const int time_to_kick = ServerParam::i().dropBallTime() - g_kick_time_before_drop;

    if ( wm.getSetPlayCount() >= time_to_kick )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickerWait) SetplayCount=%d >= KickTime=%d",
                      wm.getSetPlayCount(), time_to_kick );
        return false;
    }

    if ( Bhv_SetPlay::is_fast_restart_situation( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickerWait) fast restart" );
        return false;
    }

    agent->debugClient().addMessage( "CornerKick:Wait" );

    //
    // body action
    //
    if ( wm.getSetPlayCount() >= time_to_kick - 4 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickerWait) turn to ball. angle=%.1f angle_diff=%.1f",
                      wm.ball().angleFromSelf().degree(),
                      ( wm.ball().angleFromSelf() - wm.self().body() ).abs() );
        AngleDeg angle_diff = wm.ball().angleFromSelf() - wm.self().body();
        agent->doTurn( angle_diff );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickerWait) wait turn to center" );
        Body_TurnToPoint( Vector2D( 0.0, 0.0 ) ).execute( agent );
    }

    //
    // neck action
    //
    if ( wm.getSetPlayCount() >= time_to_kick - 3 )
    {
        // dlog.addText( Logger::TEAM,
        //               __FILE__": (doKickerWait) check receiver. wide view" );
        //setTurnNeckToReceiver( agent );
        const AngleDeg face_angle = ( wm.self().pos().y < 0.0
                                      ? 90.0
                                      : -90.0 );
        const AngleDeg body_angle = agent->effector().queuedNextSelfBody();
        AngleDeg target_neck_angle = face_angle - body_angle;
        if ( target_neck_angle.degree() > +90.0 ) target_neck_angle = +90.0;
        if ( target_neck_angle.degree() < -90.0 ) target_neck_angle = -90.0;

        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickerWait) check receiver. next_body=%.0f target_face=%.0f target_neck=%.0f",
                      body_angle.degree(), face_angle.degree(), target_neck_angle.degree() );

        agent->setNeckAction( new Neck_TurnToRelative( target_neck_angle ) );
        agent->setViewAction( new View_Wide() );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickerWait) turn_neck scan" );
        agent->setNeckAction( new Neck_ScanField() );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayOurCornerKick::doKickerTurnToBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    AngleDeg angle_diff = wm.ball().angleFromSelf() - wm.self().body();

    if ( angle_diff.abs() > 2.05
         || wm.seeTime() != wm.time() )
    {
        agent->debugClient().addMessage( "CornerKick:TurnToBall" );
        dlog.addText( Logger::TEAM,
                      __FILE__":(doKickerTurnToBall) ball_angle=%.1f angle_diff=%.3f",
                      wm.ball().angleFromSelf().degree(), angle_diff.abs() );

        agent->doTurn( angle_diff );
        setTurnNeckToReceiver( agent );
        agent->setViewAction( new View_Wide() );

        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayOurCornerKick::setTurnNeckToReceiver( PlayerAgent * agent )
{
    const CooperativeAction & action = ActionChainHolder::i().graph().bestFirstAction();
    if ( action.type() == CooperativeAction::Pass )
    {
        const AbstractPlayerObject * receiver = agent->world().ourPlayer( action.targetPlayerUnum() );
        if ( receiver )
        {
            agent->setNeckAction( new Neck_TurnToPoint( receiver->pos() + receiver->vel() ) );
            return;
        }
    }

    agent->setNeckAction( new Neck_ScanField( ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayOurCornerKick::doKickerNormalPass( PlayerAgent * agent )
{
    if ( Bhv_ChainAction().execute( agent ) )
    {
        agent->setIntention( new IntentionAvoidOffsideAfterCornerKick( agent->world().time() ) );
        // agent->setIntention( new IntentionWaitAfterSetPlayKick() );

        agent->debugClient().addMessage( "CornerKick:NormalPass" );

        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayOurCornerKick::doKickerNearestPass( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const double receiver_dist_thr = 15.0; // Magic Number

    //
    // find the nearest teammate
    //
    const PlayerObject * receiver = static_cast< const PlayerObject * >( 0 );

    for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromBall().begin(),
              t_end = wm.teammatesFromBall().end();
          t != t_end;
          ++t )
    {
        if ( (*t)->posCount() > 10 ) continue;
        if ( (*t)->distFromBall() > receiver_dist_thr ) break;

        if ( (*t)->pos().absX() > ServerParam::i().pitchHalfLength()
             || (*t)->pos().absY() > ServerParam::i().pitchHalfWidth() )
        {
            continue;
        }

        receiver = *t;
        break;
    }

    if ( ! receiver )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(doKickerNearestPass) no candidate teammate" );
        return false;
    }

    //
    // kick the ball to the nearest teammate
    //

    const double max_ball_speed = wm.self().kickRate() * ServerParam::i().maxPower();

    Vector2D target_point = receiver->inertiaFinalPoint();
    target_point.x += 0.5;

    double ball_move_dist = wm.ball().pos().dist( target_point );
    int ball_step
        = static_cast< int >( std::ceil( calc_length_geom_series( max_ball_speed,
                                                                  ball_move_dist,
                                                                  ServerParam::i().ballDecay() ) ) );
    double ball_speed = 0.0;
    if ( ball_step > 3 )
    {
        ball_speed = calc_first_term_geom_series( ball_move_dist,
                                                  ServerParam::i().ballDecay(),
                                                  ball_step );
    }
    else
    {
        ball_speed = calc_first_term_geom_series_last( 1.4, // Magic Number
                                                       ball_move_dist,
                                                       ServerParam::i().ballDecay() );
        ball_step
            = static_cast< int >( std::ceil( calc_length_geom_series( ball_speed,
                                                                      ball_move_dist,
                                                                      ServerParam::i().ballDecay() ) ) );
    }

    ball_speed = std::min( ball_speed, max_ball_speed );

    agent->debugClient().addMessage( "CornerKick:NearestPass%.3f", ball_speed );
    agent->debugClient().setTarget( target_point );

    dlog.addText( Logger::TEAM,
                  __FILE__":(doKickerNearestPass) target teammate=[%d](%.2f %.2f) ball_speed=%.2f step=%d",
                  receiver->unum(), target_point.x, target_point.y,
                  ball_speed, ball_step );

    Body_KickOneStep( target_point,
                      ball_speed
                      ).execute( agent );
    agent->setNeckAction( new Neck_ScanField() );

    agent->setIntention( new IntentionAvoidOffsideAfterCornerKick( wm.time() ) );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayOurCornerKick::doKickerFinal( PlayerAgent * agent )
{
    //
    // kick the ball to the opposite side
    //

    const WorldModel & wm = agent->world();

    Vector2D target_point( ServerParam::i().pitchHalfLength() - 2.0,
                           ( ServerParam::i().pitchHalfWidth() - 5.0 )
                           * ( 1.0 - ( wm.self().pos().x
                                       / ServerParam::i().pitchHalfLength() ) ) );
    if ( wm.self().pos().y < 0.0 )
    {
        target_point.y *= -1.0;
    }

    agent->debugClient().addMessage( "CornerKick:KickerFinal" );
    dlog.addText( Logger::TEAM,
                  __FILE__":(doKickerFinal) target=(%.2f, %.2f)",
                  target_point.x, target_point.y );

    Body_KickOneStep( target_point,
                      ServerParam::i().ballSpeedMax()
                      ).execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayOurCornerKick::doMove( PlayerAgent * agent )
{
    const Vector2D second_target_point = get2ndMovePoint( agent );

    if ( is2ndMovePeriod( agent, second_target_point ) )
    {
        if ( do2ndMove( agent, second_target_point ) )
        {
            return;
        }
    }

    if ( do1stMove( agent ) )
    {
        return;
    }

    doDefaultMove( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_SetPlayOurCornerKick::doDefaultMove( PlayerAgent * agent )
{
    static GameTime s_previous_time( -1, 0 );
    static Vector2D s_previous_target_point = Vector2D::INVALIDATED;

    if ( Bhv_SetPlayAvoidMarkMove().execute( agent ) )
    {
        return;
    }

    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    Vector2D target_point = home_pos;

    if ( home_pos.dist2( wm.ball().pos() ) < std::pow( 10.0, 2 )  )
    {
        target_point = wm.ball().pos() + ( home_pos - wm.ball().pos() ).setLengthVector( 7.0 );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doDefaultMove) adjust target (%.1f %.1f) -> (%.1f %.1f)",
                      home_pos.x, home_pos.y,
                      target_point.x, target_point.y );
    }

    target_point.x = bound( -ServerParam::i().pitchHalfLength() + 1.0,
                            target_point.x,
                            +ServerParam::i().pitchHalfLength() - 1.0 );
    target_point.y = bound( -ServerParam::i().pitchHalfWidth() + 1.0,
                            target_point.y,
                            +ServerParam::i().pitchHalfWidth() - 1.0 );

    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
    double dist_thr = std::max( 0.4, wm.ball().distFromSelf() * 0.085 );

    if ( s_previous_time.cycle() == wm.time().cycle() - 1
         && s_previous_target_point.isValid()
         && s_previous_target_point.absY() > ServerParam::i().pitchHalfWidth() - 3.0 )
    {
        if ( s_previous_target_point.dist( target_point ) > dist_thr )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (doDefaultMove) adjust to the previous point (%.1f %.1f)->(%.1f %.1f)",
                          target_point.x, target_point.y,
                          s_previous_target_point.x, s_previous_target_point.y );
            target_point = s_previous_target_point;
        }
    }

    s_previous_time = wm.time();
    s_previous_target_point = target_point;

    agent->debugClient().addMessage( "CornerKick:DefaultMove" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    dlog.addText( Logger::TEAM,
                  __FILE__": (doDefaultMove) target=(%.1f %.1f) power=%.1f dist_thr=%.3f",
                  target_point.x, target_point.y,
                  dash_power, dist_thr );

    if ( Body_GoToPoint( target_point,
                         dist_thr,
                         dash_power ).execute( agent ) )
    {
        agent->setNeckAction( new Neck_TurnToBallOrScan( 2 ) );
    }
    else
    {
        // already there
        const int time_to_kick = ServerParam::i().dropBallTime() - g_kick_time_before_drop;

        if ( wm.getSetPlayCount() < time_to_kick - 3
             || wm.time().stopped() > 0 )
        {
            // wait period
            if ( wm.getSetPlayCount() % 4 == 1 )
            {
                Body_TurnToBall().execute( agent );
                agent->setNeckAction( new Neck_TurnToBallOrScan( -1 ) );
                agent->debugClient().addMessage( "CornerKick:CheckBall" );
            }
            else
            {
                agent->doTurn( 90.0 );
                agent->setNeckAction( new Neck_ScanField() );            }
                agent->debugClient().addMessage( "CornerKick:Scan" );
        }
        else
        {
            Body_TurnToBall().execute( agent );
            agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
                agent->debugClient().addMessage( "CornerKick:TurnToBall" );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlayOurCornerKick::do1stMove( PlayerAgent * agent )
{
    const Vector2D target_point = get1stMovePoint( agent );

    if ( ! target_point.isValid() )
    {
        return false;
    }

    const WorldModel & wm = agent->world();

    const double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
    const double dist_thr = std::max( 0.4, wm.ball().distFromSelf() * 0.085 );

    dlog.addText( Logger::TEAM,
                  __FILE__": (do1stMove) target=(%.2f %.2f) dash_power=%.1f dist_thr=%.2f",
                  target_point.x, target_point.y,
                  dash_power, dist_thr );

    if ( Body_GoToPoint( target_point, dist_thr, dash_power ).execute( agent ) )
    {
        agent->setNeckAction( new Neck_TurnToBallOrScan( 2 ) );
    }
    else
    {
        if ( wm.getSetPlayCount() % 4 == 1 )
        {
            Body_TurnToBall().execute( agent );
            agent->setNeckAction( new Neck_TurnToBallOrScan( -1 ) );
            agent->debugClient().addMessage( "CornerKick:CheckBall" );
        }
        else
        {
            agent->doTurn( 90.0 );
            agent->setNeckAction( new Neck_ScanField() );
            agent->debugClient().addMessage( "CornerKick:Scan" );
        }
    }


    agent->debugClient().addMessage( "CornerKick:1stMove" );
    agent->debugClient().setTarget( target_point );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlayOurCornerKick::do2ndMove( PlayerAgent * agent,
                                     const Vector2D & target_point )
{
    if ( ! target_point.isValid() )
    {
        return false;
    }

    const WorldModel & wm = agent->world();

    const double dash_power = ServerParam::i().maxDashPower();
    const double dist_thr = std::max( 0.4, wm.ball().distFromSelf() * 0.085 );

    dlog.addText( Logger::TEAM,
                  __FILE__": (do2ndMove) target=(%.2f %.2f) dash_power=%.1f dist_thr=%.2f",
                  target_point.x, target_point.y,
                  dash_power, dist_thr );

    if ( ! Body_GoToPoint( target_point, dist_thr, dash_power ).execute( agent ) )
    {
        // TODO: determine better facing direction
        Body_TurnToBall().execute( agent );
    }

    agent->debugClient().addMessage( "CornerKick:2ndMove" );
    agent->debugClient().setTarget( target_point );

    agent->setNeckAction( new Neck_ScanField() );
    agent->setArmAction( new Arm_PointToPoint( target_point ) );
    agent->setIntention( new IntentionSetplayMove( target_point, 20, wm.time() ) );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlayOurCornerKick::is2ndMovePeriod( const PlayerAgent * agent,
                                           const Vector2D & target_point )
{
    if ( ! target_point.isValid() )
    {
        return false;
    }

    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    const double my_move_dist = wm.self().inertiaFinalPoint().dist( target_point );
    const int my_move_step = wm.self().playerType().cyclesToReachDistance( my_move_dist );

    const double ball_move_dist = wm.ball().pos().dist( target_point );
    const double ball_speed = SP.kickPowerRate() * SP.maxPower() * 0.97;

    int ball_move_step
        = static_cast< int >( std::ceil( calc_length_geom_series( ball_speed,
                                                                  ball_move_dist,
                                                                  SP.ballDecay() ) ) );
    if ( ball_move_step < 0 ) ball_move_step = 100;

    const int time_to_kick = SP.dropBallTime() - g_kick_time_before_drop;
    //const int ball_reach_time = time_to_kick + ball_move_step;
    //const int time_to_move = ball_reach_time - my_move_step;
    const int time_to_move = time_to_kick - my_move_step;

    dlog.addText( Logger::TEAM,
                  __FILE__":(is2ndMoveCycle) [?] MyMoveStep=%d BallMoveStep=%d",
                  my_move_step, ball_move_step );

    dlog.addText( Logger::TEAM,
                  __FILE__":(is2ndMoveCycle) [?] SetPlayCount=%d TimeToKick=%d TimeToMove=%d",
                  wm.getSetPlayCount(), time_to_kick, time_to_move );

    if ( wm.getSetPlayCount() > time_to_move
         || wm.getSetPlayCount() > time_to_kick - 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(is2ndMoveCycle) [true]" );
        return true;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(is2ndMoveCycle) [false]" );

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
Bhv_SetPlayOurCornerKick::get1stMovePoint( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Formation::ConstPtr f = Strategy::i().getCornerKickPreFormation( wm.theirTeamName() );

    if ( ! f )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      __FILE__": (get1stMovePoint) could not get the formation data");
#endif
        return Vector2D::INVALIDATED;
    }

    return f->getPosition( wm.self().unum(), wm.ball().pos() );
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
Bhv_SetPlayOurCornerKick::get2ndMovePoint( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Formation::ConstPtr f = Strategy::i().getCornerKickPostFormation( wm.theirTeamName(),
                                                                      Strategy::i().getCornerKickType() );
    if ( ! f )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      __FILE__": (get2ndMovePoint) could not get the formation data");
#endif
        return Vector2D::INVALIDATED;
    }

    return f->getPosition( wm.self().unum(), wm.ball().pos() );
}
