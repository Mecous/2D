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

#include "bhv_set_play_indirect_free_kick.h"

#include "strategy.h"
#include "field_analyzer.h"

#include "bhv_set_play.h"
#include "bhv_set_play_avoid_mark_move.h"
#include "bhv_prepare_set_play_kick.h"
#include "bhv_go_to_static_ball.h"
#include "bhv_chain_action.h"

#include "intention_wait_after_set_play_kick.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_pass.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/circle_2d.h>
#include <rcsc/math_util.h>

using namespace rcsc;

/*!

 */
class IntentionOurIndirectFreeKickStay
    : public SoccerIntention {
private:

    int M_step_count;

public:

    IntentionOurIndirectFreeKickStay()
        : M_step_count( 0 )
      { }

    bool finished( const PlayerAgent * agent );
    bool execute( PlayerAgent * agent );

};

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionOurIndirectFreeKickStay::finished( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const int self_step = wm.interceptTable()->selfReachStep();
    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( self_step < teammate_step )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (intention::finished) true: my ball" );
        return true;
    }

    if ( wm.kickableOpponent()
         || opponent_step <= 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (intention::finished) true: their ball" );
        return true;
    }

    if ( wm.ball().pos().dist2( ServerParam::i().theirTeamGoalPos() ) > std::pow( 20.0, 2 ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (intention::finished) true: no shoot chance" );
        return true;
    }

    ++M_step_count;

    if ( M_step_count > 20 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (intention::finished) true: over step count" );
        return true;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (intention::finished) false" );
    return false;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionOurIndirectFreeKickStay::execute( PlayerAgent * agent )
{
    agent->debugClient().addMessage( "Intention:IndFK:Stay" );
    Bhv_BodyNeckToBall().execute( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayIndirectFreeKick::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const bool our_kick = ( ( wm.gameMode().type() == GameMode::BackPass_
                              && wm.gameMode().side() == wm.theirSide() )
                            || ( wm.gameMode().type() == GameMode::IndFreeKick_
                                 && wm.gameMode().side() == wm.ourSide() )
                            || ( wm.gameMode().type() == GameMode::FoulCharge_
                                 && wm.gameMode().side() == wm.theirSide() )
                            || ( wm.gameMode().type() == GameMode::FoulPush_
                                 && wm.gameMode().side() == wm.theirSide() )
                            );

    if ( our_kick )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (execute) our kick" );

        if ( Bhv_SetPlay::is_kicker( agent ) )
        {
            doKicker( agent );
        }
        else
        {
            doOffenseMove( agent );
        }
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (execute) their kick" );

        doDefenseMove( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayIndirectFreeKick::doKicker( PlayerAgent * agent )
{
    // go to ball
    if ( Bhv_GoToStaticBall( 0.0 ).execute( agent ) )
    {
        return;
    }

    //
    // wait
    //

    if ( doKickWait( agent ) )
    {
        return;
    }

    //
    // kick to the teammate exist at the front of their goal
    //

    if ( doKickToShooter( agent ) )
    {
        return;
    }

    //
    // pass
    //
    if ( Bhv_ChainAction().execute( agent ) )
    {
        agent->setIntention( new IntentionWaitAfterSetPlayKick() );
        return;
    }

    const WorldModel & wm = agent->world();

    //
    // wait(2)
    //
    if ( wm.getSetPlayCount() <= 3 )
    {
        Body_TurnToPoint( Vector2D( 50.0, 0.0 ) ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return;
    }

    //
    // no teammate
    //
    if ( doWaitTeammateShooter( agent ) )
    {
        return;
    }

    //
    // kick to the teammate nearest to the segment between self and opponent goal
    //

    doKickToTeammateNearestToGoal( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayIndirectFreeKick::doKickWait( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D face_point = wm.ball().pos(); //( 50.0, 0.0 );
    const AngleDeg face_angle = ( face_point - wm.self().pos() ).th();

    const int actual_set_play_count = static_cast< int >( wm.time().cycle() - wm.lastSetPlayStartTime().cycle() );

    dlog.addText( Logger::TEAM,
                  __FILE__": (doKickWait) actual set play count = %d",
                  actual_set_play_count );

    if ( actual_set_play_count >= ServerParam::i().dropBallTime() - 5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) no more wait" );
        return false;
    }

    if ( wm.gameMode().type() == GameMode::IndFreeKick_
         && actual_set_play_count <= 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) just after playmode change" );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.time().stopped() > 0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) stoppage time" );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( ( face_angle - wm.self().body() ).abs() > 1.5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) turn to adjust. angle_diff=%f",
                      ( face_angle - wm.self().body() ).abs() );

        agent->debugClient().addMessage( "IndKick:TurnTo" );
        agent->debugClient().setTarget( face_point );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.getSetPlayCount() <= 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) wait teammate" );
        agent->debugClient().addMessage( "IndKick:Wait%d", wm.getSetPlayCount() );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.teammatesFromSelf().empty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) no teammate" );

        agent->debugClient().addMessage( "IndKick:NoTeammate" );
        agent->debugClient().setTarget( face_point );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayIndirectFreeKick::doKickToShooter( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    const Segment2D goal_segment( Vector2D( +SP.pitchHalfLength(),
                                            -SP.goalHalfWidth() + 0.5 ),
                                  Vector2D( +SP.pitchHalfLength(),
                                            +SP.goalHalfWidth() - 0.5 ) );
    const Vector2D nearest_goal_pos = goal_segment.nearestPoint( wm.ball().pos() );

    double min_dist = 100000.0;
    const PlayerObject * receiver = static_cast< const PlayerObject * >( 0 );

    for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromBall().begin(),
              end = wm.teammatesFromBall().end();
          t != end;
          ++t )
    {
        if ( (*t)->posCount() > 5 ) continue;
        if ( (*t)->distFromBall() > 20.0 ) continue;
        if ( (*t)->pos().x > wm.offsideLineX() ) continue;
        if ( (*t)->pos().x < wm.ball().pos().x - 3.0 ) continue;

        double teammate_goal_dist = (*t)->pos().dist( nearest_goal_pos );
        if ( teammate_goal_dist > 16.0 )
        {
            continue;
        }

        double dist = teammate_goal_dist * 0.4 + (*t)->distFromBall() * 0.6;

        if ( dist < min_dist )
        {
            min_dist = dist;
            receiver = (*t);
        }
    }

    if ( ! receiver )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKicToShooter) no shooter" );
        return false;
    }

    if ( wm.getSetPlayCount() <= 10 )
    {
        if ( receiver->posCount() >= 1
             || receiver->vel().r2() > std::pow( 0.1, 2 ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (doKicToShooter) check receiver" );
            agent->debugClient().addMessage( "IndFK:CheckShooter" );
            Body_TurnToBall().execute( agent );
            agent->setNeckAction( new Neck_TurnToPoint( receiver->pos() ) );
            return true;
        }
    }


    const Vector2D receiver_pos = receiver->pos() + receiver->vel();
    const Segment2D ball_line( wm.ball().pos(), nearest_goal_pos );

    const double max_ball_speed = wm.self().kickRate() * SP.maxPower();

    Vector2D target_point = Vector2D::INVALIDATED;


    dlog.addLine( Logger::TEAM,
                  ball_line.origin(), ball_line.terminal(), "#00F" );
    dlog.addText( Logger::TEAM,
                  __FILE__": (doKicToShooter) receiver %d pos=(%.2f %.2f)",
                  receiver->unum(), receiver_pos.x, receiver_pos.y );

    const Vector2D project_point = ball_line.projection( receiver_pos );
    if ( project_point.isValid() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKicToShooter) project_point=(%.2f %.2f)",
                      project_point.x, project_point.y );
        if ( //project_point.x < SP.pitchHalfLength() - SP.tackleDist() - 0.1
            project_point.x > SP.pitchHalfLength() - SP.ballSpeedMax() + 0.2 )
        {
            const double receiver_ball_line_dist = project_point.dist( receiver_pos );
            dlog.addText( Logger::TEAM,
                          __FILE__": (doKicToShooter) receiver_ball_line_dist=%.2f",
                          receiver_ball_line_dist );

            if ( receiver_ball_line_dist < receiver->playerTypePtr()->kickableArea() - 0.2
                     && receiver_ball_line_dist > receiver->playerTypePtr()->playerSize() + SP.ballSize() + 0.1 )
            {
                target_point = ball_line.projection( receiver_pos );
                dlog.addText( Logger::TEAM,
                              __FILE__": (doKicToShooter) set projection point (%.2f %.2f)",
                              target_point.x, target_point.y );
            }
        }

        if ( ! target_point.isValid() )
        {
            Vector2D best_point = Vector2D::INVALIDATED;
            double best_x = -10000.0;
            for ( int a = 0; a < 36; ++a )
            {
                AngleDeg angle = 360.0 / 36 * a;
                Vector2D point = receiver_pos;
                point += Vector2D::from_polar( receiver->playerTypePtr()->playerSize()
                                               + SP.ballSize()
                                               + 0.2,
                                               angle );
                dlog.addText( Logger::TEAM,
                              __FILE__": (doKicToShooter) check angle=%.0f (%.2f %.2f)",
                              angle.degree(), point.x, point.y );

                if ( point.x > SP.pitchHalfLength() - SP.tackleDist() - 0.1 )
                {
                    dlog.addText( Logger::TEAM,
                                  __FILE__": (doKicToShooter) check angle=%.0f: maybe tackle",
                                  angle.degree() );
                    continue;
                }

                if ( point.x < SP.pitchHalfLength() - SP.ballSpeedMax() + 0.2 )
                {
                    dlog.addText( Logger::TEAM,
                                  __FILE__": (doKicToShooter) check angle=%.0f: no one step shoot",
                                  angle.degree() );
                    continue;
                }

                const double ball_move_dist = wm.ball().pos().dist( point );
                const int ball_step
                    = static_cast< int >( std::ceil( calc_length_geom_series( max_ball_speed,
                                                                              ball_move_dist,
                                                                              SP.ballDecay() ) ) );
                if ( ball_step > 3 )
                {
                    dlog.addText( Logger::TEAM,
                                  __FILE__": (doKicToShooter) check angle=%.0f ball step=%d",
                                  angle.degree(), ball_step );
                    continue;
                }

                if ( ball_step > 1 )
                {
                    bool ok = true;
                    double speed = calc_first_term_geom_series( ball_move_dist,
                                                                SP.ballDecay(),
                                                                ball_step );
                    Vector2D ball_vel = ( point - wm.ball().pos() ).setLengthVector( speed );
                    Vector2D ball_pos = wm.ball().pos();
                    for ( int i = 0; i < ball_step - 1; ++i )
                    {
                        ball_pos += ball_vel;
                        ball_vel *= SP.ballDecay();
                        if ( receiver_pos.dist( ball_pos ) < receiver->playerTypePtr()->kickableArea() + 0.2 )
                        {
                            dlog.addText( Logger::TEAM,
                                          __FILE__": (doKicToShooter) check angle=%.0f receiver may kick the ball",
                                          angle.degree(), ball_step );
                            ok = false;
                            break;
                        }
                    }

                    if ( ! ok )
                    {
                        continue;
                    }
                }

                dlog.addText( Logger::TEAM,
                              __FILE__": (doKicToShooter) angle=%.0f ball_move=%.3f",
                              angle.degree(), ball_move_dist );
                if ( point.x > best_x )
                {
                    dlog.addText( Logger::TEAM,
                                  __FILE__": (doKicToShooter) angle=%.0f update best point (%.2f %.2f)",
                                  angle.degree(), point.x, point.y );
                    best_point = point;
                    best_x = point.x;
                }
            }

            target_point = best_point;
        }
    }

    if ( ! target_point.isValid() )
    {
        target_point = receiver_pos;
        target_point += Vector2D::from_polar( receiver->playerTypePtr()->playerSize()
                                              + SP.ballSize()
                                              + 0.2,
                                              0.0 );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickToShooter) no target generated. set target to (%.2f %.2f)",
                      target_point.x, target_point.y );
    }

    const double target_dist = wm.ball().pos().dist( target_point );

    const int ball_reach_step
        = static_cast< int >( std::ceil( calc_length_geom_series( max_ball_speed,
                                                                  target_dist,
                                                                  SP.ballDecay() ) ) );
    double ball_speed = calc_first_term_geom_series( target_dist,
                                                     SP.ballDecay(),
                                                     ball_reach_step );

    ball_speed = std::min( ball_speed, max_ball_speed );

    agent->debugClient().addMessage( "IndKick:KickToShooter%.3f", ball_speed );
    agent->debugClient().setTarget( target_point );
    dlog.addText( Logger::TEAM,
                  __FILE__":  pass to nearest teammate (%.1f %.1f) ball_speed=%.2f reach_step=%d",
                  target_point.x, target_point.y,
                  ball_speed, ball_reach_step );

    Body_KickOneStep( target_point, ball_speed ).execute( agent );
    agent->setNeckAction( new Neck_ScanField() );

    return true;

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayIndirectFreeKick::doWaitTeammateShooter( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.teammatesFromBall().empty()
         || wm.teammatesFromBall().front()->distFromSelf() > 35.0
         || wm.teammatesFromBall().front()->pos().x < -30.0 )
    {
        if ( wm.getSetPlayCount() <= ServerParam::i().dropBallTime() - 3 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (doWaitTeammateShooter) real set play count = %d <= drop_time-3, wait...",
                          wm.getSetPlayCount() );
            Body_TurnToPoint( Vector2D( 50.0, 0.0 ) ).execute( agent );
            agent->setNeckAction( new Neck_ScanField() );
        }
        else
        {
            Vector2D target_point( ServerParam::i().pitchHalfLength(),
                                   static_cast< double >( -1 + 2 * wm.time().cycle() % 2 )
                                   * ( ServerParam::i().goalHalfWidth() - 0.8 ) );
            double ball_speed = wm.self().kickRate() * ServerParam::i().maxPower();


            agent->debugClient().addMessage( "IndKick:ForceShoot" );
            agent->debugClient().setTarget( target_point );
            dlog.addText( Logger::TEAM,
                          __FILE__":  kick to goal (%.1f %.1f) speed=%.2f",
                          target_point.x, target_point.y,
                          ball_speed );

            Body_KickOneStep( target_point, ball_speed ).execute( agent );
            agent->setNeckAction( new Neck_ScanField() );
        }
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayIndirectFreeKick::doKickToTeammateNearestToGoal( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const double goal_y
        = std::min( wm.ball().pos().absY() * 0.8, ServerParam::i().goalHalfWidth() * 0.8 )
        * sign( wm.ball().pos().y );
    const Vector2D goal( ServerParam::i().pitchHalfLength(), goal_y );
    const Segment2D shoot_segment( wm.ball().pos(), goal );

    dlog.addLine( Logger::TEAM,
                  shoot_segment.origin(), shoot_segment.terminal(), "#F00" );

    double min_dist = 100000.0;
    const PlayerObject * receiver = static_cast< const PlayerObject * >( 0 );

    for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromBall().begin(),
              end = wm.teammatesFromBall().end();
          t != end;
          ++t )
    {
        if ( (*t)->posCount() > 5 ) continue;
        if ( (*t)->distFromBall() > 10.0 ) continue;
        if ( (*t)->pos().x > wm.offsideLineX() ) continue;

        double dist = shoot_segment.dist( (*t)->pos() );
        if ( dist < min_dist )
        {
            min_dist = dist;
            receiver = (*t);
        }
    }

    Vector2D target_point = goal;
    if ( receiver )
    {
        target_point = receiver->pos();
        target_point.x += 0.4;
    }

    double target_dist = wm.ball().pos().dist( target_point );
    double ball_speed = calc_first_term_geom_series_last( 1.8, // end speed
                                                          target_dist,
                                                          ServerParam::i().ballDecay() );
    ball_speed = std::min( ball_speed, wm.self().kickRate() * ServerParam::i().maxPower() );

    agent->debugClient().addMessage( "IndKick:ForcePass%.3f", ball_speed );
    agent->debugClient().setTarget( target_point );
    dlog.addText( Logger::TEAM,
                  __FILE__":  pass to nearest teammate (%.1f %.1f) speed=%.2f",
                  target_point.x, target_point.y,
                  ball_speed );


    Body_KickOneStep( target_point, ball_speed ).execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
    agent->addSayMessage( new BallMessage( agent->effector().queuedNextBallPos(),
                                           agent->effector().queuedNextBallVel() ) );
}

namespace {
/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_avoid_circle_point( const WorldModel & wm,
                        Vector2D point )
{
    const ServerParam & SP = ServerParam::i();

    const double circle_r
        = wm.gameMode().type() == GameMode::BackPass_
        ? SP.goalAreaLength() + 0.5
        : SP.centerCircleR() + 0.5;
    const double circle_r2 = std::pow( circle_r, 2 );

    dlog.addText( Logger::TEAM,
                  __FILE__": (get_avoid_circle_point) point=(%.1f %.1f)",
                  point.x, point.y );


    if ( point.x < -SP.pitchHalfLength() + 3.0
         && point.absY() < SP.goalHalfWidth() )
    {
        while ( point.x < wm.ball().pos().x
                && point.x > - SP.pitchHalfLength()
                && wm.ball().pos().dist2( point ) < circle_r2 )
        {
            //point.x -= 0.2;
            point.x = ( point.x - SP.pitchHalfLength() ) * 0.5 - 0.01;
            dlog.addText( Logger::TEAM,
                          __FILE__": adjust x (%.1f %.1f)",
                          point.x, point.y );
        }
    }

    if ( point.x < -SP.pitchHalfLength() + 0.5
         && point.absY() < SP.goalHalfWidth() + 0.5
         && wm.self().pos().x < -SP.pitchHalfLength()
         && wm.self().pos().absY() < SP.goalHalfWidth() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_avoid_circle_point) ok. already in our goal",
                      point.x, point.y );
        return point;
    }

    if ( wm.ball().pos().dist2( point ) < circle_r2 )
    {
        Vector2D rel = point - wm.ball().pos();
        rel.setLength( circle_r );
        point = wm.ball().pos() + rel;

        dlog.addText( Logger::TEAM,
                      __FILE__": (get_avoid_circle_point) circle contains target. adjusted=(%.2f %.2f)",
                      point.x, point.y );
    }

    return Bhv_SetPlay::get_avoid_circle_point( wm, point );
}
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayIndirectFreeKick::doOffenseMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D target_point = getOffenseMovePoint( agent );

    double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );

    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 0.4 ) dist_thr = 0.4;

    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( Body_GoToPoint( target_point,
                         dist_thr,
                         dash_power,
                         -1.0, // speed
                         100 // cycle
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "IndFK:OffenseMove:Go" );
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    }
    else
    {
        //
        // already there
        //

        const PlayerObject * kicker = wm.getTeammateNearestToBall( 5 );

        dlog.addText( Logger::TEAM,
                      __FILE__": (doOffenseMove) setplay_count=%d", wm.getSetPlayCount() );

        if ( wm.gameMode().type() == GameMode::BackPass_
             || wm.gameMode().type() == GameMode::FoulCharge_
             || wm.gameMode().type() == GameMode::FoulPush_
             || ! kicker
             || kicker->distFromBall() > kicker->playerTypePtr()->kickableArea() + 0.2 )
        {
            if ( wm.getSetPlayCount() % 4 == 0 )
            {
                agent->debugClient().addMessage( "IndFK:OffenseMove:CheckBall1" );
                Bhv_NeckBodyToBall().execute( agent );
            }
            else
            {
                agent->debugClient().addMessage( "IndFK:OffenseMove:CheckGoal1" );
                Body_TurnToPoint( ServerParam::i().theirTeamGoalPos() ).execute( agent );

                const AbstractPlayerObject * goalie = wm.getTheirGoalie();

                if ( ! goalie
                     || goalie->posCount() <= 0
                     || goalie->isGhost() )
                {
                    Vector2D face_point = ( wm.getSetPlayCount() % 4 == 2
                                            ? Vector2D( ServerParam::i().pitchHalfLength(),
                                                        +ServerParam::i().goalHalfWidth() )
                                            : wm.getSetPlayCount() % 4 == 3
                                            ? Vector2D( ServerParam::i().pitchHalfLength(),
                                                        -ServerParam::i().goalHalfWidth() )
                                            : ServerParam::i().theirTeamGoalPos() );
                    agent->setNeckAction( new Neck_TurnToPoint( face_point ) );
                }
                else
                {
                    agent->setNeckAction( new Neck_TurnToGoalieOrScan( -1 ) );
                }
            }
        }
        else
        {
            if ( wm.getSetPlayCount() <= 1 )
            {
                agent->debugClient().addMessage( "IndFK:OffenseMove:CheckGoal2" );
                Body_TurnToPoint( ServerParam::i().theirTeamGoalPos() ).execute( agent );
                agent->setNeckAction( new Neck_TurnToGoalieOrScan( -1 ) );
            }
            else
            {
                agent->debugClient().addMessage( "IndFK:OffenseMove:CheckBall2" );
                Bhv_NeckBodyToBall().execute( agent );
            }

            if ( wm.gameMode().type() == GameMode::IndFreeKick_ )
            {
                agent->setIntention( new IntentionOurIndirectFreeKickStay() );
            }
        }
    }

    //
    // say
    //

    if ( target_point.x > 36.0
         && ! wm.self().staminaModel().capacityIsEmpty()
         && ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.7
              || wm.self().staminaCapacity() < ServerParam::i().staminaCapacity() * Strategy::get_remaining_time_rate( wm )
              || wm.self().pos().dist( target_point ) > wm.ball().pos().dist( target_point ) * 0.2 + 3.0 )
         )
    {
        agent->debugClient().addMessage( "SayWait" );
        agent->addSayMessage( new WaitRequestMessage() );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SetPlayIndirectFreeKick::getOffenseMovePoint( const PlayerAgent * agent )
{
    static GameTime s_update_time( -1, 0 );
    static Vector2D s_target_point;

    const WorldModel & wm = agent->world();

    if ( s_update_time == wm.time() )
    {
        return s_target_point;
    }
    s_update_time = wm.time();

    const ServerParam & SP = ServerParam::i();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    if ( home_pos.dist2( SP.theirTeamGoalPos() ) > std::pow( 20.0, 2 )
         || home_pos.x < wm.ball().pos().x - 1.0 )
    {
        s_target_point = home_pos;
        if ( home_pos.dist2( wm.ball().pos() ) < std::pow( 10.0, 2 )  )
        {
            s_target_point = wm.ball().pos() + ( home_pos - wm.ball().pos() ).setLengthVector( 6.5 );
        }
        return s_target_point;
    }

    //
    // find the shortest shoot path point
    //

    const Vector2D shoot_point = getShortestShootPathPoint( agent );
    if ( shoot_point.isValid() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(getOffenseMovePoint) shoot point (5.2f %.2f)",
                      shoot_point.x, shoot_point.y );
        s_target_point = shoot_point;
        return shoot_point;
    }


    Vector2D target_point = home_pos;
    //
    // find the target point using voronoi diagram
    //
    Vector2D voronoi_point = getOffenseVoronoiTargetPoint( agent, home_pos );
    if ( voronoi_point.isValid() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(getOffenseMovePoint) voronoi point (%.2f %.2f)",
                      voronoi_point.x, voronoi_point.y );
        target_point = voronoi_point;
    }

    //
    // adjust the target point to the shoot course
    //
    target_point = adjustTargetPointOnShootCourse( agent, target_point );
    if ( target_point.x > wm.offsideLineX() - 0.5 )
    {
        target_point.x = wm.offsideLineX() - 0.5;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(getOffenseMovePoint) target=(%.2f %.2f)",
                  target_point.x, target_point.y );

    s_target_point = target_point;
    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SetPlayIndirectFreeKick::getShortestShootPathPoint( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    const Segment2D goal_segment( Vector2D( +SP.pitchHalfLength(),
                                            -SP.goalHalfWidth() + 0.5 ),
                                  Vector2D( +SP.pitchHalfLength(),
                                            +SP.goalHalfWidth() - 0.5 ) );

    const Vector2D nearest_goal_pos = goal_segment.nearestPoint( wm.ball().pos() );

    double kick_speed = SP.ballSpeedMax();
    const PlayerObject * kicker = wm.getTeammateNearestToBall( 5 );
    if ( kicker )
    {
        kick_speed = kicker->playerTypePtr()->kickPowerRate() * SP.maxPower();
    }

    double goal_dist_thr
        = kick_speed * ( 1.0 + SP.ballDecay() + std::pow( SP.ballDecay(), 2 ) ) // first kick move (max 3 step)
        + SP.ballSpeedMax() * 0.9 ; // second kick (shoot) move
    //if ( goal_dist_thr > 9.0 ) goal_dist_thr = 9.0;

    const double nearest_goal_pos_dist = nearest_goal_pos.dist( wm.ball().pos() );

    dlog.addText( Logger::TEAM,
                  __FILE__":(getShortestShootPathPoint) nearest_goal_pos=(%.2f %.2f)",
                  nearest_goal_pos.x, nearest_goal_pos.y );
    dlog.addText( Logger::TEAM,
                  __FILE__":(getShortestShootPathPoint) goal_dist_thr=%.3f dist=%.3f",
                  goal_dist_thr, nearest_goal_pos_dist );
    dlog.addText( Logger::TEAM,
                  __FILE__":(getShortestShootPathPoint) kick_speed=%.3f",
                  kick_speed );

    if ( nearest_goal_pos_dist > goal_dist_thr )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(getShortestShootPathPoint) impossible" );
        return Vector2D::INVALIDATED;
    }

    Vector2D from_goal = wm.ball().pos() - nearest_goal_pos;
    from_goal.setLength( std::max( SP.ballSpeedMax() - 0.7, SP.tackleDist() + 0.3 ) );

    Vector2D shoot_point = nearest_goal_pos + from_goal;

    dlog.addText( Logger::TEAM,
                  __FILE__":(getShortestShootPathPoint) raw shoot point (%.2f %.2f)",
                  shoot_point.x, shoot_point.y );

    Vector2D shift_vec = ( nearest_goal_pos.y < 0.0
                           ? from_goal.rotatedVector( -90.0 )
                           : from_goal.rotatedVector( +90.0 ) );
    shift_vec.setLength( wm.self().playerType().playerSize()
                         + SP.ballSize()
                         + 0.2 );
    shoot_point += shift_vec;

    dlog.addText( Logger::TEAM,
                  __FILE__":(getShortestShootPathPoint) shift (%.2f %.2f) => shoot point (%.2f %.2f)",
                  shift_vec.x, shift_vec.y,
                  shoot_point.x, shoot_point.y );

    dlog.addText( Logger::TEAM,
                  __FILE__":(getShortestShootPathPoint) offside_x=%.2f",
                  wm.offsideLineX() );

    if ( shoot_point.x > wm.offsideLineX() - 0.5 )
    {
        return Vector2D::INVALIDATED;
    }

    double min_dist2 = 1000000.0;
    int best_unum = Unum_Unknown;
    for ( int unum = 1; unum <= 11; ++unum )
    {
        if ( kicker
             && kicker->unum() == unum )
        {
            continue;
        }

        const AbstractPlayerObject * p = wm.ourPlayer( unum );
        if ( ! p )
        {
            continue;
        }

        Vector2D pos = ( Strategy::i().getPosition( unum ) + p->pos() ) * 0.5;
        double d2 = pos.dist2( shoot_point );
        if ( d2 < min_dist2 )
        {
            min_dist2 = d2;
            best_unum = unum;
        }
    }

    if ( wm.self().unum() != best_unum )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(getShortestShootPathPoint) shooter teammate=%d",
                      best_unum );
        return Vector2D::INVALIDATED;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(getShortestShootPathPoint) shooter self" );
    return shoot_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SetPlayIndirectFreeKick::getOffenseVoronoiTargetPoint( const PlayerAgent * agent,
                                                           const Vector2D & home_pos )
{
    static std::vector< Vector2D > s_voronoi_candidates;
    s_voronoi_candidates.clear();

    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    FieldAnalyzer::i().positioningVoronoiDiagram().getPointsOnSegments( 1.0, // min length
                                                                        30, // max division
                                                                        &s_voronoi_candidates );
    Vector2D best_pos = Vector2D::INVALIDATED;
    double best_score = -1000000.0;
    for ( std::vector< Vector2D >::const_iterator p = s_voronoi_candidates.begin(),
              end = s_voronoi_candidates.end();
          p != end;
          ++p )
    {
        if ( p->x < home_pos.x - 1.0 ) continue;
        if ( home_pos.x > SP.pitchHalfLength() - 5.0 )
        {
            if ( p->dist2( home_pos ) > std::pow( 1.0, 2 ) ) continue;
        }
        else
        {
            if ( p->dist2( home_pos ) > std::pow( 5.0, 2 ) ) continue;
        }
        if ( p->x > wm.offsideLineX() - 1.0 ) continue;

        dlog.addRect( Logger::TEAM,
                      p->x - 0.1, p->y - 0.1, 0.2, 0.2, "#0F0" );

        bool exist_opponent = false;
        const Segment2D ball_line( wm.ball().pos(), *p );
        for ( PlayerObject::Cont::const_iterator o = wm.opponents().begin(), o_end = wm.opponents().end();
              o != o_end;
              ++o )
        {
            if ( std::fabs( (*o)->pos().x - p->x ) < 10.0
                 && std::fabs( (*o)->pos().y - p->y ) < 10.0
                 && ball_line.dist( (*o)->pos() ) < SP.tackleDist() )
            {
                exist_opponent = true;
                break;
            }
        }

        if ( exist_opponent ) continue;

        double score = 1.0 / ( std::max( p->dist( SP.theirTeamGoalPos() ), 0.1 )
                               * std::max( p->dist( home_pos ), 0.1 )
                               * std::max( p->dist( wm.self().pos() ), 0.1 ) );

        if ( score > best_score )
        {
            best_pos = *p;
            best_score = score;
        }
    }

    if ( best_pos.isValid() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(getOffenseVoronoiTargetPoint) voronoi position=(%.1f %.1f)",
                      best_pos.x, best_pos.y );
        return best_pos;
    }

    return Vector2D::INVALIDATED;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SetPlayIndirectFreeKick::adjustTargetPointOnShootCourse( const PlayerAgent * agent,
                                                             const Vector2D & target_point )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    double kick_speed = SP.ballSpeedMax();
    const PlayerObject * kicker = wm.getTeammateNearestToBall( 5 );
    if ( kicker )
    {
        kick_speed = kicker->playerTypePtr()->kickPowerRate() * SP.maxPower();
    }

    const Segment2D goal_segment( Vector2D( SP.pitchHalfLength(), -SP.goalHalfWidth() + 0.5 ),
                                  Vector2D( SP.pitchHalfLength(), +SP.goalHalfWidth() - 0.5 ) );
    const double max_shoot_move
        = inertia_n_step_distance( SP.ballSpeedMax(), 3, SP.ballDecay() )
        - 0.2;

    dlog.addText( Logger::TEAM,
                  __FILE__":(adjustTargetPointOnShootCourse) max_shoot_move = %.3f",
                  max_shoot_move );

    Vector2D best_shoot_point( 10000.0, 0.0 );
    for ( int i = 1; i <= 3; ++i )
    {
        const double ball_move = calc_sum_geom_series( kick_speed,
                                                       SP.ballDecay(),
                                                       i )
            + wm.self().playerType().playerSize()
            + SP.ballSize();

        for ( int j = -17; j <= 17; ++j )
        {
            Vector2D point = wm.ball().pos() + Vector2D::from_polar( ball_move, 5.0 * j );

            if ( point.x > SP.pitchHalfLength() - 1.0 ) continue;
            if ( point.x > wm.offsideLineX() - 1.0 ) continue;
            if ( point.dist2( target_point ) > std::pow( 3.0, 2 ) ) continue;

            double opponent_dist = wm.getDistOpponentNearestTo( point, 5 );
            double teammate_dist = wm.getDistTeammateNearestTo( point, 5 );

            if ( opponent_dist < SP.tackleDist() ) continue;
            if ( teammate_dist < 3.0 ) continue;

            if ( goal_segment.dist( point ) > max_shoot_move )
            {
                // dlog.addText( Logger::TEAM,
                //               "step=%d angle=%.0f : pos(%.2f %.2f) over max_shoot_move dist=%.3f",
                //               i, 5.0*j,
                //               point.x, point.y, goal.dist( point ) );
                continue;
            }

            if ( best_shoot_point.dist2( target_point ) > point.dist2( target_point ) )
            {
                // dlog.addText( Logger::TEAM,
                //               "step=%d angle=%.0f : pos(%.2f %.2f) updated",
                //               i, 5.0*j,
                //               point.x, point.y );
                best_shoot_point = point;
            }
        }
    }

    if ( best_shoot_point.x < 52.5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(adjustTargetPointOnShootCourse) shoot point=(%.2f %.2f)",
                      best_shoot_point.x, best_shoot_point.y );
        return best_shoot_point;
    }

    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayIndirectFreeKick::doDefenseMove( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
    Vector2D adjusted_point = target_point;
    if ( ! wm.gameMode().isServerCycleStoppedMode() )
    {
        get_avoid_circle_point( wm, target_point );
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": their kick adjust target to (%.1f %.1f)->(%.1f %.1f) ",
                  target_point.x, target_point.y,
                  adjusted_point.x, adjusted_point.y );

    double dash_power = wm.self().getSafetyDashPower( SP.maxDashPower() );

    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    if ( adjusted_point != target_point
         && wm.ball().pos().dist( target_point ) > 10.0
         && wm.self().inertiaFinalPoint().dist( adjusted_point ) < dist_thr )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": reverted to the first target point" );
        adjusted_point = target_point;
    }

    {
        const double collision_dist
            = wm.self().playerType().playerSize()
            + SP.goalPostRadius()
            + 0.2;

        Vector2D goal_post_l( -SP.pitchHalfLength() + SP.goalPostRadius(),
                              -SP.goalHalfWidth() - SP.goalPostRadius() );
        Vector2D goal_post_r( -SP.pitchHalfLength() + SP.goalPostRadius(),
                              +SP.goalHalfWidth() + SP.goalPostRadius() );
        double dist_post_l = wm.self().pos().dist( goal_post_l );
        double dist_post_r = wm.self().pos().dist( goal_post_r );

        const Vector2D & nearest_post = ( dist_post_l < dist_post_r
                                          ? goal_post_l
                                          : goal_post_r );
        double dist_post = std::min( dist_post_l, dist_post_r );

        if ( dist_post < collision_dist + wm.self().playerType().realSpeedMax() + 0.5 )
        {
            Circle2D post_circle( nearest_post, collision_dist );
            Segment2D move_line( wm.self().pos(), adjusted_point );

            if ( post_circle.intersection( move_line, NULL, NULL ) > 0 )
            {
                AngleDeg post_angle = ( nearest_post - wm.self().pos() ).th();
                if ( nearest_post.y < wm.self().pos().y )
                {
                    adjusted_point = nearest_post;
                    adjusted_point += Vector2D::from_polar( collision_dist + 0.1, post_angle - 90.0 );
                    // Vector2D rel = adjusted_point - wm.self().pos();
                    // rel.rotate( -45.0 );
                    // adjusted_point = wm.self().pos() + rel;
                }
                else
                {
                    adjusted_point = nearest_post;
                    adjusted_point += Vector2D::from_polar( collision_dist + 0.1, post_angle + 90.0 );
                    // Vector2D rel = adjusted_point - wm.self().pos();
                    // rel.rotate( +45.0 );
                    // adjusted_point = wm.self().pos() + rel;
                }

                dist_thr = 0.05;
                dlog.addText( Logger::TEAM,
                              __FILE__": adjust to avoid goal post. (%.2f %.2f)",
                              adjusted_point.x, adjusted_point.y );
            }
        }
    }

    agent->debugClient().addMessage( "IndFKMove" );
    agent->debugClient().setTarget( adjusted_point );
    agent->debugClient().addCircle( adjusted_point, dist_thr );

    if ( ! Body_GoToPoint( adjusted_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Body_TurnToBall().execute( agent );
        dlog.addText( Logger::TEAM,
                      __FILE__":  their kick. turn to ball" );
    }

    agent->setNeckAction( new Neck_TurnToBall() );
}
