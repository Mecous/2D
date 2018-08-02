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

#include "bhv_set_play_free_kick.h"

#include "strategy.h"

#include "action_chain_holder.h"
#include "action_chain_graph.h"
#include "cooperative_action.h"

#include "bhv_set_play.h"
#include "bhv_set_play_avoid_mark_move.h"
#include "bhv_prepare_set_play_kick.h"
#include "bhv_go_to_static_ball.h"
#include "bhv_chain_action.h"
#include "bhv_clear_ball.h"
#include "neck_turn_to_receiver.h"

#include "intention_wait_after_set_play_kick.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_clear_ball.h>
#include <rcsc/action/body_pass.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>
#include <rcsc/action/view_synch.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/say_message_builder.h>

#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/circle_2d.h>
#include <rcsc/math_util.h>

using namespace rcsc;


/*-------------------------------------------------------------------*/
/*!

 */
namespace {
const int kicker_wait_time_thr = 8;
}

/*-------------------------------------------------------------------*/
/*!
  execute action
*/
bool
Bhv_SetPlayFreeKick::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_SetPlayFreeKick" );
    //---------------------------------------------------
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
Bhv_SetPlayFreeKick::doKick( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    dlog.addText( Logger::TEAM,
                  __FILE__": (doKick)" );
    //
    // go to the ball position
    //
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

    if ( wm.getSetPlayCount() < ServerParam::i().dropBallTime() - 1 )
    {
        //
        // turn to ball
        //
        const AngleDeg angle_diff = wm.ball().angleFromSelf() - wm.self().body();
        if ( angle_diff.abs() > 2.0 )
        {
            agent->debugClient().addMessage( "FreeKick:TurnToBall" );
            dlog.addText( Logger::TEAM,
                          __FILE__":(doKick) turn to ball. angle=%.1f angle_diff=%.3f",
                          wm.ball().angleFromSelf().degree(), angle_diff.abs() );

            if ( wm.seeTime() != wm.time() )
            {
                agent->doTurn( 0.0 );
                agent->setViewAction( new View_Synch() );
            }
            else
            {
                agent->doTurn( angle_diff );
            }
            agent->setNeckAction( new Neck_ScanField() );
            doTurnNeckToReceiver( agent );
            return;
        }

        if ( wm.self().viewWidth() != ViewWidth::WIDE )
        {
            agent->debugClient().addMessage( "FreeKick:ViewWide" );
            agent->doTurn( 0.0 );
            agent->setViewAction( new View_Wide() );
            doTurnNeckToReceiver( agent );
            return;
        }

        if ( wm.seeTime() != wm.time() )
        {
            agent->debugClient().addMessage( "FreeKick:CheckReceiver" );
            agent->doTurn( 0.0 );
            doTurnNeckToReceiver( agent );
            return;
        }
    }

    //
    // pass
    //
    if ( Bhv_ChainAction().execute( agent ) )
    {
        agent->debugClient().addMessage( "FreeKick:Pass" );
        agent->setIntention( new IntentionWaitAfterSetPlayKick() );
        return;
    }

    //
    // kick to the nearest teammate
    //
    if ( doKickToNearestTeammate( agent ) )
    {
        return;
    }

    //
    // clear
    //

    agent->debugClient().addMessage( "FreeKick:Clear" );
    dlog.addText( Logger::TEAM,
                  __FILE__":(doKick)  clear" );

    Bhv_ClearBall().execute( agent );
    //agent->setNeckAction( new Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayFreeKick::doKickWait( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    dlog.addText( Logger::TEAM,
                  __FILE__": (doKickWait)" );

    if ( wm.getSetPlayCount() >= ServerParam::i().dropBallTime() - kicker_wait_time_thr )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) set_play_count=%d", wm.getSetPlayCount() );
        return false;
    }

    if ( wm.time().stopped() != 0 )
    {
        agent->doTurn( 90.0 );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( Bhv_SetPlay::is_fast_restart_situation( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) fast restart" );
        return false;
    }

    agent->debugClient().addMessage( "FreeKick:Wait" );
    dlog.addText( Logger::TEAM,
                  __FILE__": (doKickWait) wait" );
    //
    // body action
    //
    if ( wm.getSetPlayCount() >= ServerParam::i().dropBallTime() - kicker_wait_time_thr - 4 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) wait turn to ball" );
        AngleDeg angle_diff = wm.ball().angleFromSelf() - wm.self().body();
        agent->doTurn( angle_diff );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) wait turn 90" );
        agent->doTurn( 90.0 );
    }

    //
    // neck action
    //
    if ( wm.getSetPlayCount() >= ServerParam::i().dropBallTime() - kicker_wait_time_thr - 1 )
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
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayFreeKick::doKickToNearestTeammate( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const PlayerObject * nearest_teammate = wm.getTeammateNearestToSelf( 10, false ); // without goalie
    if ( ! nearest_teammate
         || nearest_teammate->distFromSelf() > 20.0
         || ( nearest_teammate->pos().x < -30.0
              && nearest_teammate->distFromSelf() > 10.0 ) )
    {
        return false;
    }

    const double max_ball_speed = wm.self().kickRate() * ServerParam::i().maxPower();

    Vector2D target_point = nearest_teammate->inertiaFinalPoint();

    if ( nearest_teammate->seenPosCount() > 1 )
    {
        agent->debugClient().addMessage( "FreeKick:ForcePass:Look" );
        agent->debugClient().setTarget( target_point );
        dlog.addText( Logger::TEAM,
                      __FILE__":  force pass. check target=%d (%.1f %.1f)",
                      nearest_teammate->unum(),
                      target_point.x, target_point.y );
        Bhv_NeckBodyToPoint( target_point ).execute( agent );
        return true;
    }

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

    agent->debugClient().addMessage( "FreeKick:ForcePass%.3f", ball_speed );
    agent->debugClient().setTarget( target_point );
    dlog.addText( Logger::TEAM,
                  __FILE__":  force pass. target=(%.1f %.1f) speed=%.2f reach_step=%d",
                  target_point.x, target_point.y,
                  ball_speed, ball_reach_step );

    Body_KickOneStep( target_point, ball_speed ).execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
    if ( nearest_teammate->unum() != Unum_Unknown )
    {
        Vector2D ball_vel = agent->effector().queuedNextBallVel();
        agent->addSayMessage( new PassMessage( nearest_teammate->unum(),
                                               target_point,
                                               agent->effector().queuedNextBallPos(),
                                               ball_vel ) );
    }
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayFreeKick::doTurnNeckToReceiver( PlayerAgent * agent )
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
Bhv_SetPlayFreeKick::doMove( PlayerAgent * agent )
{
    static GameTime s_last_adjusted_time( -1, 0 );

    const WorldModel & wm = agent->world();

    dlog.addText( Logger::TEAM,
                  __FILE__": (doMove)" );

    if ( Bhv_SetPlayAvoidMarkMove().execute( agent ) )
    {
        return;
    }

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
    target_point.x = std::min( target_point.x, wm.offsideLineX() - 0.5 );

    if ( target_point.dist2( wm.ball().pos() ) < std::pow( 10.0, 2 )
         || s_last_adjusted_time.cycle() == wm.time().cycle() - 1 )
    {
        Vector2D adjusted = wm.ball().pos() + ( target_point - wm.ball().pos() ).setLengthVector( 7.0 );
        s_last_adjusted_time = wm.time();
        dlog.addText( Logger::TEAM,
                      __FILE__": (doMove) adjust position (%.1f %.1f) -> (%.1f %.1f)",
                      target_point.x, target_point.y,
                      adjusted.x, adjusted.y );
        target_point = adjusted;
        target_point.x = std::min( target_point.x, wm.offsideLineX() - 0.5 );
    }

    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
    if ( wm.ball().distFromSelf() < 10.0 )
    {
        dash_power = wm.self().getSafetyDashPower( wm.self().playerType().staminaIncMax() * 0.9 );
    }

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "SetplayMove" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( Body_GoToPoint( target_point,
                         dist_thr,
                         dash_power ).execute( agent ) )
    {
        agent->setNeckAction( new Neck_TurnToBallOrScan( 2 ) );
    }
    else
    {
        // already there
        if ( wm.getSetPlayCount() < ServerParam::i().dropBallTime() - kicker_wait_time_thr + 3
             || wm.time().stopped() > 0
             || ! Bhv_SetPlay::is_fast_restart_situation( agent ) )
        {
            // wait period
            if ( wm.getSetPlayCount() % 4 == 1 )
            {
                agent->debugClient().addMessage( "SetplayCheckBall" );
                Body_TurnToBall().execute( agent );
                agent->setNeckAction( new Neck_TurnToBallOrScan( -1 ) );
            }
            else
            {
                agent->doTurn( 90.0 );
                if ( wm.self().pos().x > 30.0 )
                {
                    agent->debugClient().addMessage( "SetplayGoalieOrScan" );
                    agent->setNeckAction( new Neck_TurnToGoalieOrScan( 0 ) );
                }
                else
                {
                    agent->debugClient().addMessage( "SetplayScan" );
                    agent->setNeckAction( new Neck_ScanField() );
                }
            }
        }
        else
        {
            Body_TurnToBall().execute( agent );
            agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
        }
    }

    if ( wm.self().pos().dist( target_point ) > std::max( wm.ball().pos().dist( target_point ) * 0.2, dist_thr ) + 6.0 )
    {
        agent->debugClient().addMessage( "SayWaitFar" );
        agent->addSayMessage( new WaitRequestMessage() );
    }
    else if ( ! wm.self().staminaModel().capacityIsEmpty()
              && ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.7
                   || wm.self().staminaCapacity() < ServerParam::i().staminaCapacity() * Strategy::get_remaining_time_rate( wm )
                   || wm.self().inertiaFinalPoint().dist( target_point ) > wm.ball().pos().dist( target_point ) * 0.2 + 3.0 ) )
    {
        agent->debugClient().addMessage( "SayWaitRecover" );
        agent->addSayMessage( new WaitRequestMessage() );
    }
}
