// -*-c++-*-

/*!
  \file bhv_chain_pass.cpp
  \brief search the pass receiver player and perform pass kick if possible
*/

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

#include "bhv_pass.h"

#include "strategy.h"
#include "action_chain_holder.h"
#include "action_chain_graph.h"
#include "field_analyzer.h"
#include "generator_pass.h"

#include "bhv_hold_ball.h"
#include "neck_turn_to_receiver.h"

#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/body_stop_ball.h>
#include <rcsc/action/body_turn_to_point.h>
#include <rcsc/action/neck_turn_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/view_synch.h>
#include <rcsc/action/kick_table.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/abstract_player_object.h>
#include <rcsc/player/soccer_intention.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/math_util.h>

#include <vector>
#include <algorithm>
#include <cmath>

// #define DEBUG_PRINT

using namespace rcsc;


class IntentionFindReceiver
    : public SoccerIntention {
private:
    int M_step;
    int M_receiver_unum;
    Vector2D M_receive_point;

public:

    IntentionFindReceiver( const int receiver_unum,
                           const Vector2D & receive_point )
        : M_step( 0 ),
          M_receiver_unum( receiver_unum ),
          M_receive_point( receive_point )
      { }

    bool finished( const PlayerAgent * agent );

    bool execute( PlayerAgent * agent );

private:

};


class IntentionWaitAfterPassKick
    : public SoccerIntention {
private:
    int M_count;

public:

    IntentionWaitAfterPassKick()
        : M_count( 0 )
      { }

    bool finished( const PlayerAgent * agent )
      {
          if ( ++M_count > 1 )
          {
              return true;
          }

          if ( ! agent->world().self().isKickable() )
          {
              dlog.addText( Logger::TEAM,
                            __FILE__": (finished) no kickable" );
              return true;
          }

          return false;
      }

    bool execute( PlayerAgent * agent )
      {
          dlog.addText( Logger::TEAM,
                        __FILE__": (finished) pass kick wait" );
          agent->debugClient().addMessage( "PasKick:Wait" );
          agent->doTurn( 0.0 );
          agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
          return true;
      }

private:

};

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionFindReceiver::finished( const PlayerAgent * agent )
{
    ++M_step;

    dlog.addText( Logger::TEAM,
                  __FILE__": (finished) step=%d",
                  M_step );

    if ( M_step >= 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) time over" );
        return true;
    }

    const WorldModel & wm = agent->world();

    //
    // check kickable
    //

    if ( ! wm.self().isKickable() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) no kickable" );
        return true;
    }

    //
    // check receiver
    //

    const AbstractPlayerObject * receiver = wm.ourPlayer( M_receiver_unum );

    if ( ! receiver )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) NULL receiver" );
        return true;
    }

    if ( receiver->unum() == wm.self().unum() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) receiver is self" );
        return true;
    }

    if ( receiver->posCount() <= 0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) already seen" );
        return true;
    }

    //
    // check opponent
    //

    if ( wm.kickableOpponent() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) exist kickable opponent" );
        return true;
    }

    if ( wm.interceptTable()->opponentReachCycle() <= 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) opponent may be kickable" );
        return true;
    }

    //
    // check next kickable
    //

    double kickable2 = std::pow( wm.self().playerType().kickableArea()
                                 - wm.self().vel().r() * ServerParam::i().playerRand()
                                 - wm.ball().vel().r() * ServerParam::i().ballRand()
                                 - 0.15,
                                 2 );
    Vector2D self_next = wm.self().pos() + wm.self().vel();
    Vector2D ball_next = wm.ball().pos() + wm.ball().vel();

    if ( self_next.dist2( ball_next ) > kickable2 )
    {
        // unkickable if turn is performed.
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) unkickable at next cycle" );
        return true;
    }

    return false;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionFindReceiver::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const AbstractPlayerObject * receiver = wm.ourPlayer( M_receiver_unum );

    if ( ! receiver )
    {
        return false;
    }

    const Vector2D next_self_pos = agent->effector().queuedNextMyPos();
    const double next_view_width = agent->effector().queuedNextViewWidth().width() * 0.5;

    const Vector2D receiver_pos = receiver->pos() + receiver->vel();
    const AngleDeg receiver_angle = ( receiver_pos - next_self_pos ).th();

    Vector2D face_point = ( receiver_pos + M_receive_point ) * 0.5;
    AngleDeg face_angle = ( face_point - next_self_pos ).th();

    double rate = 0.5;
    while ( ( face_angle - receiver_angle ).abs() > next_view_width - 10.0 )
    {
        rate += 0.1;
        face_point
            = receiver_pos * rate
            + M_receive_point * ( 1.0 - rate );
        face_angle = ( face_point - next_self_pos ).th();

        if ( rate > 0.999 )
        {
            break;
        }
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (IntentionFindReceiver::execute) facePoint=(%.1f %.1f) faceAngle=%.1f",
                  face_point.x, face_point.y,
                  face_angle.degree() );
    agent->debugClient().addMessage( "IntentionFindReceiver%.0f",
                                     face_angle.degree() );
    Body_TurnToPoint( face_point ).execute( agent );
    agent->setNeckAction( new Neck_TurnToPoint( face_point ) );

    //
    // say
    //
    if ( agent->config().useCommunication()
         && M_receiver_unum != Unum_Unknown )
    {
        dlog.addText( Logger::ACTION | Logger::TEAM,
                      __FILE__": (IntentionFindReceiver::execute) set pass communication." );
        agent->debugClient().addMessage( "Say:pass" );
        agent->addSayMessage( new PassMessage( M_receiver_unum,
                                               M_receive_point,
                                               agent->effector().queuedNextBallPos(),
                                               Vector2D( 0.0, 0.0 ) ) );
    }

    return true;
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Pass::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_Pass" );

    const WorldModel & wm = agent->world();

    if ( ! wm.self().isKickable() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": no kickable" );
        return false;
    }

    //
    // validate the pass
    //
    const CooperativeAction & pass =  ActionChainHolder::i().graph().bestFirstAction();

    if ( pass.type() != CooperativeAction::Pass )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": action(%d) is not a pass type.",
                      pass.type() );
        return false;
    }

    const AbstractPlayerObject * receiver = wm.ourPlayer( pass.targetPlayerUnum() );

    if ( ! receiver )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": NULL receiver." );

        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": pass receiver unum=%d (%.1f %.1f)",
                  receiver->unum(),
                  receiver->pos().x, receiver->pos().y );

    if ( wm.self().unum() == receiver->unum() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": receiver is my self." );
        return false;
    }

    // if ( receiver->isGhost() )
    // {
    //     dlog.addText( Logger::TEAM,
    //                   __FILE__": receiver is a ghost." );
    //     return false;
    // }

    if ( wm.gameMode().type() != GameMode::PlayOn )
    {
        doPassKick( agent, pass );
        return true;
    }

    //
    // estimate kick step
    //

    if ( pass.kickCount() == 1
         && receiver->seenPosCount() <= 3 )
    {
        const Vector2D home_pos = Strategy::i().getPosition( receiver->unum() );
        if ( receiver->seenPosCount() <= 1
             || ( home_pos.dist( receiver->pos() )
                  < std::max( 3.0, wm.ball().pos().dist( home_pos ) * 0.1 ) ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": 1 step kick." );
            doPassKick( agent, pass );
            return true;
        }
        else
        {
            agent->debugClient().addMessage( "ReceiverMoving" );
            dlog.addText( Logger::TEAM,
                          __FILE__": 1 step kick, but receiver may be moving." );
        }
    }

    //
    // trying to turn body and/or neck to the pass receiver
    //

    if ( doCheckReceiver( agent, pass ) )
    {
        doSayPass( agent, pass );

        agent->debugClient().addCircle( wm.self().pos(), 3.0 );
        agent->debugClient().addCircle( wm.self().pos(), 5.0 );
        agent->debugClient().addCircle( wm.self().pos(), 10.0 );

        return true;
    }

    //
    // pass kick
    //

    doPassKick( agent, pass );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Pass::doPassKick( PlayerAgent * agent,
                      const CooperativeAction & pass )
{
    const WorldModel & wm = agent->world();

    const double first_ball_speed = pass.firstBallVel().r();
    // const AbstractPlayerObject * receiver = wm.ourPlayer( pass.targetPlayerUnum() );

    agent->debugClient().setTarget( pass.targetPlayerUnum() );
    agent->debugClient().setTarget( pass.targetBallPos() );
    dlog.addText( Logger::TEAM,
                  __FILE__" (Bhv_Pass) pass to "
                  "%d receive_pos=(%.1f %.1f) ball_vel=(%.2f %.2f) speed=%f",
                  pass.targetPlayerUnum(),
                  pass.targetBallPos().x, pass.targetBallPos().y,
                  pass.firstBallVel().x, pass.firstBallVel().y,
                  first_ball_speed );

    // if ( pass.kickCount() == 1
    //      && wm.gameMode().type() == GameMode::PlayOn
    //      && receiver
    //      && receiver->pos().x > wm.offsideLineX() )
    // {
    //     agent->debugClient().addMessage( "Pass:OffsideHold" );
    //     dlog.addText( Logger::TEAM,
    //                   __FILE__": (execute) avoid offside. hold ball." );
    //     Body_HoldBall( true,
    //                    pass.targetBallPos(),
    //                    pass.targetBallPos() ).execute( agent );
    // }
    // else
    if ( pass.kickCount() == 1
         || wm.gameMode().type() != GameMode::PlayOn )
    {
        agent->debugClient().addMessage( "Pass:1Step" );
        Body_KickOneStep( pass.targetBallPos(), first_ball_speed ).execute( agent );
        //agent->setIntention( new IntentionWaitAfterPassKick() );
        doSayPass( agent, pass );
    }
    else
    {
        agent->debugClient().addMessage( "Pass:MultiStep" );
        Body_SmartKick( pass.targetBallPos(),
                        first_ball_speed,
                        first_ball_speed * 0.96,
                        3 ).execute( agent );
        doSayPass( agent, pass );
    }
    agent->setNeckAction( new Neck_TurnToReceiver() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_Pass::kickStep( const WorldModel & wm,
                    const CooperativeAction & pass )
{
    Vector2D max_vel
        = KickTable::calc_max_velocity( ( pass.targetBallPos() - wm.ball().pos() ).th(),
                                        wm.self().kickRate(),
                                        wm.ball().vel() );

    // dlog.addText( Logger::TEAM,
    //               __FILE__": (kickStep) maxSpeed=%.3f passSpeed=%.3f",
    //               max_vel.r(), pass.firstBallSpeed() );
    double speed2 = pass.firstBallVel().r2();

    if ( max_vel.r2() >= speed2 )
    {
        return 1;
    }

    if ( speed2 > 2.5*2.5 ) // Magic Number
    {
        return 3;
    }

    return 2;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Pass::doCheckReceiver( PlayerAgent * agent,
                           const CooperativeAction & pass )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * receiver = wm.ourPlayer( pass.targetPlayerUnum() );

    if ( ! receiver )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doCheckReceiver) no receiver." );
        return false;
    }

    if ( receiver->seenPosCount() == 0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doCheckReceiver) receiver already seen." );
        return false;
    }

#if 1
    //
    // check opponent interfare
    //

    if ( //! GeneratorPrecisePass::can_see_only_turn_neck( wm, *receiver ) &&
        ! GeneratorPass::can_check_receiver_without_opponent( wm ) )
    {
        return false;
    }
#endif

    //
    // set view mode
    //
    agent->setViewAction( new View_Synch() );

    //const double next_view_half_width = agent->effector().queuedNextViewWidth().width() * 0.5;
    const double view_max = SP.maxNeckAngle() + ViewWidth::width( ViewWidth::NARROW ) * 0.5 - 10.0;

    //
    // check if turn_neck is necessary or not
    //
    const Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    const Vector2D player_pos = receiver->pos() + receiver->vel();
    const AngleDeg player_angle = ( player_pos - next_self_pos ).th();

    const double angle_diff = ( player_angle - wm.self().body() ).abs();

    if ( angle_diff > view_max )
    {
        //
        // need turn
        //

        dlog.addText( Logger::TEAM,
                      __FILE__": (doCheckReceiver) need turn. angle_diff=%.1f view_max=%.1f",
                      angle_diff, view_max );

        //
        // TODO: check turn moment?
        //


        //
        // stop the ball
        //

        const double next_ball_dist = next_self_pos.dist( wm.ball().pos() + wm.ball().vel() );
        const double noised_kickable_area = wm.self().playerType().kickableArea()
            - wm.ball().vel().r() * SP.ballRand()
            - wm.self().vel().r() * SP.playerRand()
            - 0.15;

        dlog.addText( Logger::TEAM,
                      __FILE__": (doCheckReceiver) next_ball_dist=%.3f"
                      " noised_kickable=%.3f(kickable=%.3f)",
                      next_ball_dist,
                      noised_kickable_area,
                      wm.self().playerType().kickableArea() );

        if ( next_ball_dist > noised_kickable_area )
        {
            if ( doKeepBall( agent, pass ) )
            {
                return true;
            }

            if ( Body_StopBall().execute( agent ) )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": (doCheckReceiver) stop the ball" );
                agent->debugClient().addMessage( "PassKickFind:StopBall" );
                agent->debugClient().setTarget( receiver->unum() );

                return true;
            }

            dlog.addText( Logger::TEAM,
                          __FILE__": (doCheckReceiver) Could not stop the ball???" );
        }

        //
        // turn to receiver
        //

        doTurnBodyNeckToReceiver( agent, pass );
        return true;
    }

    //
    // can see the receiver without turn
    //
    dlog.addText( Logger::TEAM,
                  __FILE__": (doCheckReceiver) can see receiver[%d](%.2f %.2f)"
                  " angleDiff=%.1f view_max%.1f",
                  pass.targetPlayerUnum(),
                  player_pos.x, player_pos.y,
                  angle_diff, view_max );

    if ( ( pass.kickCount() == 1
           && receiver->seenPosCount() >= 3 )
         || receiver->isGhost() )
    {
        agent->debugClient().addMessage( "Pass:FindHold" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doCheckReceiver) hold ball." );

        Body_HoldBall( true,
                       pass.targetBallPos(),
                       pass.targetBallPos() ).execute( agent );
        agent->setNeckAction( new Neck_TurnToReceiver() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Pass::doKeepBall( PlayerAgent * agent,
                      const CooperativeAction & pass )
{
    const WorldModel & wm = agent->world();

    //
    // try collision kick
    //

    const PlayerObject * nearest_opponent = ( wm.opponentsFromSelf().empty()
                                              ? static_cast< const PlayerObject * >( 0 )
                                              : wm.opponentsFromSelf().front() );

    if ( ! nearest_opponent
         || nearest_opponent->distFromSelf() > ( ServerParam::i().tackleDist()
                                                 + wm.self().playerType().playerSize()
                                                 + ServerParam::i().ballSize()
                                                 + nearest_opponent->playerTypePtr()->realSpeedMax() * 2.0 ) )
    {
        const PlayerType & ptype = wm.self().playerType();
        Vector2D self_next = wm.self().pos() + wm.self().vel();
        Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
        Vector2D kick_accel = self_next - ball_next;
        double kick_accel_len = kick_accel.r();
        if ( ServerParam::i().maxPower() * wm.self().kickRate()
             > kick_accel_len - ptype.playerSize() - ServerParam::i().ballSize() )
        {
            double kick_power = std::min( kick_accel_len / wm.self().kickRate(),
                                          ServerParam::i().maxPower() );
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (doKeepBall) collision kick. kick_power=%.1f",
                          kick_power );
            agent->debugClient().addMessage( "PassKickFind:KeepBallCollide" );
            agent->debugClient().setTarget( pass.targetPlayerUnum() );

            agent->doKick( kick_power, kick_accel.th() - wm.self().body() );
            agent->setNeckAction( new Neck_TurnToReceiver() );

            dlog.addText( Logger::TEAM,
                          __FILE__": (doKeepBall) register intention" );
            agent->setIntention( new IntentionFindReceiver( pass.targetPlayerUnum(),
                                                            pass.targetBallPos() ) );
            return true;
        }
    }

    //
    // try hold ball
    //

    const Vector2D ball_vel = Bhv_HoldBall::get_keep_ball_vel( wm );

    if ( ! ball_vel.isValid() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKeepBall) no candidate." );

        return false;
    }

    const Vector2D kick_accel = ball_vel - wm.ball().vel();
    const double kick_power = kick_accel.r() / wm.self().kickRate();
    const AngleDeg kick_angle = kick_accel.th() - wm.self().body();

    if ( kick_power > ServerParam::i().maxPower() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKeepBall) over kick power" );
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (doKeepBall) "
                  " ballVel=(%.2f %.2f)"
                  " kickPower=%.1f kickAngle=%.1f",
                  ball_vel.x, ball_vel.y,
                  kick_power,
                  kick_angle.degree() );

    agent->debugClient().addMessage( "PassKickFind:KeepBall" );
    agent->debugClient().setTarget( pass.targetPlayerUnum() );
    agent->debugClient().setTarget( wm.ball().pos()
                                    + ball_vel
                                    + ball_vel * ServerParam::i().ballDecay() );

    agent->doKick( kick_power, kick_angle );
    agent->setNeckAction( new Neck_TurnToReceiver() );

    //
    // set turn intention
    //

    dlog.addText( Logger::TEAM,
                  __FILE__": (doKeepBall) register intention" );
    agent->setIntention( new IntentionFindReceiver( pass.targetPlayerUnum(),
                                                    pass.targetBallPos() ) );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Pass::doTurnBodyNeckToReceiver( PlayerAgent * agent,
                                    const CooperativeAction & pass )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * receiver = wm.ourPlayer( pass.targetPlayerUnum() );

    const double next_view_half_width = agent->effector().queuedNextViewWidth().width() * 0.5;
    const double view_min = SP.minNeckAngle() - next_view_half_width + 10.0;
    const double view_max = SP.maxNeckAngle() + next_view_half_width - 10.0;

    //
    // create candidate body target points
    //

    std::vector< Vector2D > body_points;
    body_points.reserve( 16 );

    const Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    const Vector2D receiver_pos = receiver->pos() + receiver->vel();
    const AngleDeg receiver_angle = ( receiver_pos - next_self_pos ).th();

    const double max_x = SP.pitchHalfLength() - 7.0;
    const double min_x = ( max_x - 10.0 < next_self_pos.x
                           ? max_x - 10.0
                           : next_self_pos.x );
    const double max_y = std::max( SP.pitchHalfLength() - 5.0,
                                   receiver_pos.absY() );
    const double y_step = std::max( 3.0, max_y / 5.0 );
    const double y_sign = sign( receiver_pos.y );

    // on the static x line (x = max_x)
    for ( double y = 0.0; y < max_y + 0.001; y += y_step )
    {
        body_points.push_back( Vector2D( max_x, y * y_sign ) );
    }

    // on the static y line (y == receiver_pos.y)
    for ( double x_rate = 0.9; x_rate >= 0.0; x_rate -= 0.1 )
    {
        double x = std::min( max_x,
                             max_x * x_rate + min_x * ( 1.0 - x_rate ) );
        body_points.push_back( Vector2D( x, receiver_pos.y ) );
    }

    //
    // evaluate candidate points
    //

    const double max_turn
        = wm.self().playerType().effectiveTurn( SP.maxMoment(),
                                                wm.self().vel().r() );
    Vector2D best_point = Vector2D::INVALIDATED;
    double min_turn = 360.0;

    for ( std::vector< Vector2D >::const_iterator p = body_points.begin(),
              p_end = body_points.end();
          p != p_end;
          ++p )
    {
        AngleDeg target_body_angle = ( *p - next_self_pos ).th();
        double turn_moment_abs = ( target_body_angle - wm.self().body() ).abs();

#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      "____ body_point=(%.1f %.1f) angle=%.1f moment=%.1f",
                      p->x, p->y,
                      target_body_angle.degree(),
                      turn_moment_abs );
#endif

        double angle_diff = ( receiver_angle - target_body_angle ).abs();
        if ( view_min < angle_diff
             && angle_diff < view_max )
        {
            if ( turn_moment_abs < max_turn )
            {
                best_point = *p;
#ifdef DEBUG_PRINT
                dlog.addText( Logger::TEAM,
                              "____ oooo can turn and look" );
#endif
                if ( turn_moment_abs < min_turn )
                {
                    best_point = *p;
                    min_turn = turn_moment_abs;
#ifdef DEBUG_PRINT
                    dlog.addText( Logger::TEAM,
                                  "____ ---- update candidate point min_turn=%.1f",
                                  min_turn );
#endif
                }
            }
        }
#ifdef DEBUG_PRINT
        else
        {
            dlog.addText( Logger::TEAM,
                          "____ xxxx cannot look" );
        }
#endif
    }

    if ( ! best_point.isValid() )
    {
        best_point = pass.targetBallPos();
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      __FILE__": (doTurnBodyNeckToPoint) could not find the target point." );
#endif
    }

    //
    // perform the action
    //
    agent->debugClient().addMessage( "PassKickFind:Turn" );
    agent->debugClient().setTarget( receiver->unum() );

    dlog.addText( Logger::TEAM,
                  __FILE__": (doTurnBodyNeckToReceiver)"
                  " receiver=%d receivePos=(%.2f %.2f)"
                  " turnTo=(%.2f %.2f)",
                  pass.targetPlayerUnum(),
                  pass.targetBallPos().x, pass.targetBallPos().y,
                  best_point.x, best_point.y );
    agent->debugClient().addLine( next_self_pos, best_point );

    Body_TurnToPoint( best_point ).execute( agent );
    agent->setNeckAction( new Neck_TurnToReceiver() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_Pass::doSayPass( PlayerAgent * agent,
                     const CooperativeAction & pass )
{
    const int receiver_unum = pass.targetPlayerUnum();
    const Vector2D receive_pos = pass.targetBallPos();

    if ( agent->config().useCommunication()
         && receiver_unum != Unum_Unknown
         //&& ! agent->effector().queuedNextBallKickable()
         )
    {
        const AbstractPlayerObject * receiver = agent->world().ourPlayer( receiver_unum );
        if ( ! receiver
             // || receiver->pos().x > agent->world().offsideLineX()
             )
        {
            dlog.addText( Logger::ACTION | Logger::TEAM,
                          __FILE__": (doSayPass) null receiver or over offside line." );
            return;
        }

        dlog.addText( Logger::ACTION | Logger::TEAM,
                      __FILE__": (doSayPass) set pass communication." );

        Vector2D target_buf( 0.0, 0.0 );

        agent->debugClient().addMessage( "Say:pass" );

        Vector2D ball_vel( 0.0, 0.0 );
        if ( ! agent->effector().queuedNextBallKickable() )
        {
            ball_vel = agent->effector().queuedNextBallVel();
        }

        agent->addSayMessage( new PassMessage( receiver_unum,
                                               receive_pos + target_buf,
                                               agent->effector().queuedNextBallPos(),
                                               ball_vel ) );
    }
}
