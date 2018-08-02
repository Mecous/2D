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

#include "bhv_set_play_kick_in.h"

#include "options.h"
#include "strategy.h"

#include "action_chain_holder.h"
#include "action_chain_graph.h"
#include "cooperative_action.h"

#include "bhv_set_play.h"
#include "bhv_set_play_avoid_mark_move.h"
#include "bhv_go_to_static_ball.h"
#include "bhv_chain_action.h"

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
    target_point.x = std::min( 45.0, wm.offsideLineX() - 1.0 );

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
Bhv_SetPlayKickIn::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__":(execute)" );

    if ( Bhv_SetPlay::is_kicker( agent ) )
    {
        doKick( agent );
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
Bhv_SetPlayKickIn::doKick( PlayerAgent * agent )
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

    if ( doKickWait( agent ) )
    {
        return;
    }

    //
    // turn to ball
    //
    {
        AngleDeg angle_diff = wm.ball().angleFromSelf() - wm.self().body();
        if ( angle_diff.abs() > 2.05
             || wm.seeTime() != wm.time() )
        {
            agent->debugClient().addMessage( "KickIn:TurnToBall" );
            dlog.addText( Logger::TEAM,
                          __FILE__":(doKick) turn to ball. angle=%.1f angle_diff=%.3f",
                          wm.ball().angleFromSelf().degree(), angle_diff.abs() );

            //Body_TurnToBall().execute( agent );
            agent->doTurn( angle_diff );
            //agent->setNeckAction( new Neck_ScanField() );
            doTurnNeckToReceiver( agent );
            agent->setViewAction( new View_Wide() );
            return;
        }
    }

    //
    // pass
    //
    if ( Bhv_ChainAction().execute( agent ) )
    {
        if ( wm.gameMode().type() == GameMode::CornerKick_
             || wm.ball().pos().x > 45.0 )
        {
            agent->setIntention( new IntentionAvoidOffsideAfterCornerKick( wm.time() ) );
        }
        else
        {
            agent->setIntention( new IntentionWaitAfterSetPlayKick() );
        }
        agent->debugClient().addMessage( "KickIn:SeqPass" );
        return;
    }

    //
    // kick to the nearest teammate
    //
    {
        const PlayerObject * receiver = wm.getTeammateNearestToBall( 10 );

        if ( receiver )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(doKick) try kicking to nearest teammate [%d]",
                          receiver->unum() );
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(doKick) error. no teammate" );
        }

        const double receiver_dist_thr = ( wm.ball().pos().x > 40.0
                                           ? 15.0
                                           : 10.0 );
        if ( receiver
             && receiver->distFromBall() < receiver_dist_thr
             && receiver->pos().absX() < ServerParam::i().pitchHalfLength()
             && receiver->pos().absY() < ServerParam::i().pitchHalfWidth() )
        {
            const double max_ball_speed = wm.self().kickRate() * ServerParam::i().maxPower();

            Vector2D target_point = receiver->inertiaFinalPoint();
            target_point.x += 0.5;

            double ball_move_dist = wm.ball().pos().dist( target_point );
            int ball_reach_step
                = static_cast< int >( std::ceil( calc_length_geom_series( max_ball_speed,
                                                                          ball_move_dist,
                                                                          ServerParam::i().ballDecay() ) ) );
            double ball_speed = 0.0;
            if ( ball_reach_step > 3 )
            {
                ball_speed = calc_first_term_geom_series( ball_move_dist,
                                                          ServerParam::i().ballDecay(),
                                                          ball_reach_step );
            }
            else
            {
                ball_speed = calc_first_term_geom_series_last( 1.4,
                                                               ball_move_dist,
                                                               ServerParam::i().ballDecay() );
                ball_reach_step
                    = static_cast< int >( std::ceil( calc_length_geom_series( ball_speed,
                                                                              ball_move_dist,
                                                                              ServerParam::i().ballDecay() ) ) );
            }

            ball_speed = std::min( ball_speed, max_ball_speed );

            agent->debugClient().addMessage( "KickIn:ForcePass%.3f", ball_speed );
            agent->debugClient().setTarget( target_point );

            dlog.addText( Logger::TEAM,
                          __FILE__":(doKick) kick to nearest teammate (%.1f %.1f) speed=%.2f",
                          target_point.x, target_point.y,
                          ball_speed );
            Body_KickOneStep( target_point,
                              ball_speed
                              ).execute( agent );
            agent->setNeckAction( new Neck_ScanField() );
            return;
        }
    }

    //
    // advance ball
    //

    if ( wm.self().pos().x < 20.0 )
    {
        agent->debugClient().addMessage( "KickIn:Advance" );

        dlog.addText( Logger::TEAM,
                      __FILE__":(doKick) advance(1)" );
        Body_AdvanceBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return;
    }

    //
    // kick to the opponent side corner
    //
    {
        agent->debugClient().addMessage( "KickIn:ForceAdvance" );

        Vector2D target_point( ServerParam::i().pitchHalfLength() - 2.0,
                               ( ServerParam::i().pitchHalfWidth() - 5.0 )
                               * ( 1.0 - ( wm.self().pos().x
                                           / ServerParam::i().pitchHalfLength() ) ) );
        if ( wm.self().pos().y < 0.0 )
        {
            target_point.y *= -1.0;
        }
        // enforce one step kick
        dlog.addText( Logger::TEAM,
                      __FILE__":(doKick) advance(2) to (%.1f, %.1f)",
                      target_point.x, target_point.y );
        Body_KickOneStep( target_point,
                          ServerParam::i().ballSpeedMax()
                          ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayKickIn::doKickWait( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const int time_thr = 8;

    if ( wm.getSetPlayCount() >= ServerParam::i().dropBallTime() - time_thr )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) set_play_count=%d, force kick mode",
                      wm.getSetPlayCount() );
        return false;
    }

    if ( Bhv_SetPlay::is_fast_restart_situation( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) fast restart" );
        return false;
    }

    agent->debugClient().addMessage( "KickIn:Wait" );

    //
    // body action
    //
    if ( wm.getSetPlayCount() >= ServerParam::i().dropBallTime() - time_thr - 4 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) wait turn to ball. angle=%.1f angle_diff=%.1f",
                      wm.ball().angleFromSelf().degree(),
                      ( wm.ball().angleFromSelf() - wm.self().body() ).abs() );
        AngleDeg angle_diff = wm.ball().angleFromSelf() - wm.self().body();
        agent->doTurn( angle_diff );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) wait turn to center" );
        Body_TurnToPoint( Vector2D( 0.0, 0.0 ) ).execute( agent );
    }

    //
    // neck action
    //
    if ( wm.getSetPlayCount() >= ServerParam::i().dropBallTime() - time_thr - 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) turn_neck check receiver" );
        doTurnNeckToReceiver( agent );
        agent->setViewAction( new View_Wide() );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) turn_neck scan" );
        agent->setNeckAction( new Neck_ScanField() );
    }

    return true;

#if 0
    if ( Bhv_SetPlay::is_delaying_tactics_situation( agent ) )
    {
        agent->debugClient().addMessage( "KickIn:Delaying" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) delaying" );

        Body_TurnToPoint( Vector2D( 0.0, 0.0 ) ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.teammatesFromBall().empty() )
    {
        agent->debugClient().addMessage( "KickIn:NoTeammate" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) no teammate" );

        Body_TurnToPoint( Vector2D( 0.0, 0.0 ) ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.getSetPlayCount() <= 3 )
    {
        agent->debugClient().addMessage( "KickIn:Wait%d", wm.getSetPlayCount() );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) wait teammates" );

        Body_TurnToBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.audioMemory().waitRequestTime().cycle() > wm.time().cycle() - 10 )
    {
        agent->debugClient().addMessage( "KickIn:WaitReqested" );
        Body_TurnToBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    const double capacity_thr = ServerParam::i().staminaCapacity() * Strategy::get_remaining_time_rate( wm );

    if ( wm.getSetPlayCount() >= 15
         && wm.seeTime() == wm.time()
         && wm.self().stamina() > ServerParam::i().staminaMax() * 0.6
         && wm.self().staminaCapacity() > capacity_thr )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) set play count = %d, force kick mode",
                      wm.getSetPlayCount() );
        return false;
    }

    if ( wm.seeTime() != wm.time()
         || wm.self().stamina() < ServerParam::i().staminaMax() * 0.9
         || wm.self().staminaCapacity() < capacity_thr )
    {
        Body_TurnToBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );

        agent->debugClient().addMessage( "KickIn:Wait%d", wm.getSetPlayCount() );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) no see or recover" );
        return true;
    }

    return false;
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayKickIn::doTurnNeckToReceiver( PlayerAgent * agent )
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
void
Bhv_SetPlayKickIn::doMove( PlayerAgent * agent )
{
    static GameTime s_previous_time( -1, 0 );
    static Vector2D s_previous_target_point = Vector2D::INVALIDATED;

    const WorldModel & wm = agent->world();

    //
    // opponent avoidance
    //

    if ( Bhv_SetPlayAvoidMarkMove().execute( agent ) )
    {
        return;
    }

    //
    // determine the target point
    //

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    Vector2D target_point = home_pos;

    if ( home_pos.dist2( wm.ball().pos() ) < std::pow( 10.0, 2 )  )
    {
        target_point = wm.ball().pos() + ( home_pos - wm.ball().pos() ).setLengthVector( 7.0 );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) adjust position (%.1f %.1f) -> (%.1f %.1f)",
                      home_pos.x, home_pos.y,
                      target_point.x, target_point.y );
    }

    target_point.x = bound( -ServerParam::i().pitchHalfLength() + 1.0,
                            target_point.x,
                            +ServerParam::i().pitchHalfLength() - 1.0 );
    target_point.y = bound( -ServerParam::i().pitchHalfWidth() + 1.0,
                            target_point.y,
                            +ServerParam::i().pitchHalfWidth() - 1.0 );

    double dist_thr = std::max( 0.4, wm.ball().distFromSelf() * 0.085 );

    if ( s_previous_time.cycle() == wm.time().cycle() - 1
         && s_previous_target_point.isValid()
         && s_previous_target_point.absY() > ServerParam::i().pitchHalfWidth() - 3.0 )
    {
        if ( s_previous_target_point.dist( target_point ) > dist_thr )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (doMove) adjust to the previous point (%.1f %.1f)->(%.1f %.1f)",
                          target_point.x, target_point.y,
                          s_previous_target_point.x, s_previous_target_point.y );
            target_point = s_previous_target_point;
        }
    }

    s_previous_time = wm.time();
    s_previous_target_point = target_point;

    agent->debugClient().addMessage( "KickInMove" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );

    dlog.addText( Logger::TEAM,
                  __FILE__": (doMove) target=(%.1f %.1f) power=%.1f dist_thr=%.3f",
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
        if ( wm.getSetPlayCount() < ServerParam::i().dropBallTime() - 8 + 3
             || wm.time().stopped() > 0 )
        {
            // wait period
            if ( wm.getSetPlayCount() % 4 == 1 )
            {
                agent->debugClient().addMessage( "KickIn:CheckBall" );
                Body_TurnToBall().execute( agent );
                agent->setNeckAction( new Neck_TurnToBallOrScan( -1 ) );
            }
            else
            {
                agent->doTurn( 90.0 );
                agent->debugClient().addMessage( "KickIn:Scan" );
                agent->setNeckAction( new Neck_ScanField() );
            }
        }
        else
        {
            Body_TurnToBall().execute( agent );
            agent->debugClient().addMessage( "KickIn:CheckBall" );
            agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
        }
    }

    //
    // say wait message
    //

    if ( ! wm.self().staminaModel().capacityIsEmpty() )
    {
        if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.7
             || wm.self().staminaCapacity() < ServerParam::i().staminaCapacity() * Strategy::get_remaining_time_rate( wm )
             || wm.self().inertiaFinalPoint().dist( home_pos ) > wm.ball().pos().dist( home_pos ) * 0.2 + 3.0 )

        {
            agent->debugClient().addMessage( "SayWait" );
            agent->addSayMessage( new WaitRequestMessage() );
        }
    }
}
