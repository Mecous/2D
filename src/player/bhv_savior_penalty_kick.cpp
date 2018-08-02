// -*-c++-*-

/*!
  \file bhv_savior_penalty_kick.cpp
  \brief aggressive goalie behavior
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

#include "action_chain_holder.h"
#include "cooperative_action.h"

#include "bhv_savior_penalty_kick.h"
#include "bhv_deflecting_tackle.h"
#include "bhv_clear_ball.h"

#include "bhv_chain_action.h"

#include "shoot_simulator.h"

#include "neck_chase_ball.h"

#include <rcsc/action/bhv_neck_body_to_ball.h>
#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_turn_to_angle.h>
#include <rcsc/action/body_turn_to_point.h>
#include <rcsc/action/body_stop_dash.h>
//#include <rcsc/action/body_clear_ball.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/view_synch.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/penalty_kick_state.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/geom/angle_deg.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/math_util.h>

#include <algorithm>
#include <cmath>
#include <cstdio>


# define VISUAL_DEBUG
# define USE_GOALIE_GO_TO_POINT
#include "body_savior_go_to_point.h"

//
// each setting switch
//

/*
 * uncomment this to use old condition for decision which positioning mode,
 * normal positioning or goal line positioning
 */
#define DO_AGGRESSIVE_POSITIONING



/*
 * do goal line positioning instead of normal move if ball is side
 */
//#define DO_GOAL_LINE_POSITIONING_AT_SIDE


/*
 * uncomment this to use old condition for decision which positioning mode,
 * normal positioning, goal line positioning, goal parallel positioning
 */
//#define PENALTY_SHOOTOUT_BLOCK_IN_PENALTY_AREA
//#define PENALTY_SHOOTOUT_GOAL_PARALLEL_POSITIONING


/*
 * if you don't want to use new conditions for "SideChase", please uncomment.
 */
//#define USE_EXTENDED_SIDE_CHASE



using namespace rcsc;

namespace {
static const double EMERGENT_ADVANCE_DIR_DIFF_THRESHOLD = 3.0; //degree
static const double SHOOT_ANGLE_THRESHOLD = 10.0; // degree
static const double SHOOT_DIST_THRESHOLD = 40.0; // meter
static const int SHOOT_BLOCK_TEAMMATE_POS_COUNT_THRESHOLD = 20; // steps
}

/*---------------------------------------------------------------------------------*/
/*!

 */

bool
Bhv_SaviorPenaltyKick::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": (execute)" );

    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

#ifdef VISUAL_DEBUG
    if ( opponentCanShootFrom( wm, wm.ball().pos(), 20 ) )
    {
        agent->debugClient().addLine( wm.ball().pos(),
                                      Vector2D( SP.ourTeamGoalLineX(),
                                                -SP.goalHalfWidth() ) );
        agent->debugClient().addLine( wm.ball().pos(),
                                      Vector2D( SP.ourTeamGoalLineX(),
                                                +SP.goalHalfWidth() ) );
    }
#endif


    switch( wm.gameMode().type() )
    {
    case GameMode::PlayOn:
    case GameMode::KickIn_:
    case GameMode::OffSide_:
    case GameMode::CornerKick_:
        {
            return this->doPlayOnMove( agent );
        }

    case GameMode::GoalKick_:
        {
            if ( wm.gameMode().side() == wm.self().side() )
            {
                const Vector2D pos( SP.ourTeamGoalLineX() + 2.5, 0.0 );

                Body_GoToPoint( pos, 0.5, SP.maxPower() ).execute( agent )
                    || Body_StopDash( true ).execute( agent )
                    || Body_TurnToAngle( 0.0 ).execute( agent );

                agent->setNeckAction( new Neck_ScanField() );

                return true;
            }
            else
            {
                return this->doPlayOnMove( agent );
            }
            break;
        }

    case GameMode::BeforeKickOff:
    case GameMode::AfterGoal_:
    case GameMode::KickOff_:
        {
            const Vector2D pos( SP.ourTeamGoalLineX(), 0.0 );

            agent->setNeckAction( new Neck_ScanField );

            return Body_SaviorGoToPoint( pos ).execute( agent )
                || Bhv_ScanField().execute( agent );
        }

    case GameMode::PenaltyTaken_:
        {
            const PenaltyKickState * pen = wm.penaltyKickState();

            if ( pen->currentTakerSide() == wm.ourSide() )
            {
                return Bhv_ScanField().execute( agent );
            }
            else
            {
                if ( this->doPenaltyKickMove( agent ) )
                {
                    agent->setNeckAction( new Neck_ChaseBall );
                    agent->setViewAction( new View_Synch );
                    return true;
                }
            }

            break;
        }

    default:
        return this->doPlayOnMove( agent );
        break;
    }

    return false;
}

/*=======================================================================*/
/*!

 */

bool
Bhv_SaviorPenaltyKick::doPenaltyKickMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    //
    // set some parameters for thinking
    //
    const int self_reach_cycle = wm.interceptTable()->selfReachCycle();
    const int opponent_reach_cycle = wm.interceptTable()->opponentReachCycle();
    const int predict_step = std::min( opponent_reach_cycle, self_reach_cycle );
    const Vector2D self_intercept_point = wm.ball().inertiaPoint( self_reach_cycle );
    const Vector2D ball_pos = getFieldBoundPredictBallPos( wm, predict_step, 0.5 );

    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickMove) self pos = [%f, %f], body dir = %f",
                  wm.self().pos().x, wm.self().pos().y,
                  wm.self().body().degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickMove) self reach cycle = %d",
                  self_reach_cycle );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickMove) opponent reach cycle = %d",
                  opponent_reach_cycle );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickMove) current ball pos = [%f, %f]",
                  wm.ball().pos().x, wm.ball().pos().y );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickMove) current_ball_vel = [%f, %f]",
                  wm.ball().vel().x, wm.ball().vel().y );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickMove) predict step = %d",
                  predict_step );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickMove) predict ball pos = [%f, %f]",
                  ball_pos.x, ball_pos.y );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickMove) ball dist = %f",
                  wm.ball().distFromSelf() );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickMove) self intercept cycle = %d, point = (%f, %f)",
                  self_reach_cycle,
                  self_intercept_point.x, self_intercept_point.y );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickMove) tackle probability = %f",
                  wm.self().tackleProbability() );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickMove) catch probability = %f",
                  wm.self().catchProbability() );

    //
    // set default neck action
    //
    setDefaultNeckAction( agent, true, predict_step );


    //
    // if catchable, do catch
    //
    if ( doCatchIfPossible( agent ) )
    {
        return true;
    }

    //
    // if kickable, do kick
    //
    if ( wm.self().isKickable() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPenaltyKickMove) doKick",
                      wm.self().tackleProbability() );
        return this->doKick( agent );
    }

    agent->debugClient().addMessage( "ball dist=%f", wm.ball().distFromSelf() );


    //
    // set parameters
    //
    const Rect2D shrinked_penalty_area
        ( Vector2D( SP.ourTeamGoalLineX(),
                    - (SP.penaltyAreaHalfWidth() - 1.0) ),
          Size2D( SP.penaltyAreaLength() - 1.0,
                  (SP.penaltyAreaHalfWidth() - 1.0) * 2.0 ) );

    const bool is_shoot_ball
        = ShootSimulator::is_ball_moving_to_our_goal( wm.ball().pos(),
                                                      wm.ball().vel(),
                                                      1.0 );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickMove) is_shoot_ball = %s",
                  ( is_shoot_ball ? "true" : "false" ) );

#ifdef VISUAL_DEBUG
    if ( is_shoot_ball )
    {
        agent->debugClient().addLine( wm.self().pos() + Vector2D( -2.0, -2.0 ),
                                      wm.self().pos() + Vector2D( -2.0, +2.0 ) );
    }
#endif

    const bool is_despair_situation
        = ( is_shoot_ball && self_intercept_point.x <= SP.ourTeamGoalLineX() );



    //
    // tackle
    //
    if ( doTackleIfNecessary( agent, is_despair_situation ) )
    {
        return true;
    }


    //
    // chase ball
    //
    if ( doChaseBallIfNessary( agent,
                               true, // is_penalty_kick_mode,
                               is_despair_situation,
                               self_reach_cycle,
                               6000,  // teammate_reach_cycle,
                               opponent_reach_cycle,
                               self_intercept_point,
                               shrinked_penalty_area ) )
    {
        return true;
    }


    //
    // check ball
    //
    if ( doFindBallIfNecessary( agent, opponent_reach_cycle ) )
    {
        return true;
    }


    //
    // positioning
    //
    if ( doPenaltyKickPositioning( agent, ball_pos,
                                   is_despair_situation ) )
    {
        return true;
    }


    //
    // default behavior
    //
    agent->debugClient().addMessage( "Savior:NoAction" );
    dlog.addText( Logger::ROLE,
                  __FILE__ ":(doPenaltyKickMove) no action" );
    agent->doTurn( 0.0 );
    return true;
}

/*=======================================================================*/
/*!

 */

bool
Bhv_SaviorPenaltyKick::doPlayOnMove( PlayerAgent * agent,
                                     const bool is_penalty_kick_mode )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    //
    // set some parameters for thinking
    //
    const int self_reach_cycle = wm.interceptTable()->selfReachCycle();
    const int teammate_reach_cycle = wm.interceptTable()->teammateReachCycle();
    const int opponent_reach_cycle = wm.interceptTable()->opponentReachCycle();
    const int predict_step = std::min( opponent_reach_cycle,
                                       std::min( teammate_reach_cycle,
                                                 self_reach_cycle ) );
    const Vector2D self_intercept_point = wm.ball().inertiaPoint( self_reach_cycle );
    const Vector2D ball_pos = getFieldBoundPredictBallPos( wm, predict_step, 0.5 );
#if 0
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPlayOnMove) self pos = [%f, %f], body dir = %f",
                  wm.self().pos().x, wm.self().pos().y,
                  wm.self().body().degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPlayOnMove) self reach cycle = %d",
                  self_reach_cycle );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPlayOnMove) teammate reach cycle = %d",
                  teammate_reach_cycle );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPlayOnMove) opponent reach cycle = %d",
                  opponent_reach_cycle );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPlayOnMove) current ball pos = [%f, %f]",
                  wm.ball().pos().x, wm.ball().pos().y );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPlayOnMove) current_ball_vel = [%f, %f]",
                  wm.ball().vel().x, wm.ball().vel().y );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPlayOnMove) predict step = %d",
                  predict_step );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPlayOnMove) predict ball pos = [%f, %f]",
                  ball_pos.x, ball_pos.y );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPlayOnMove) ball dist = %f",
                  wm.ball().distFromSelf() );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPlayOnMove) self intercept cycle = %d, point = (%f, %f)",
                  self_reach_cycle,
                  self_intercept_point.x, self_intercept_point.y );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPlayOnMove) tackle probability = %f",
                  wm.self().tackleProbability() );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPlayOnMove) catch probability = %f",
                  wm.self().catchProbability() );
#endif

    //
    // set default neck action
    //
    setDefaultNeckAction( agent, is_penalty_kick_mode, predict_step );


    //
    // if catchable, do catch
    //
    if ( doCatchIfPossible( agent ) )
    {
        return true;
    }


    //
    // if kickable, do kick
    //
    if ( wm.self().isKickable() )
    {
        if ( ! wm.kickableTeammate()
             || wm.ball().distFromSelf() < wm.teammatesFromBall().front()->distFromBall() )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPlayOnMove) doKick",
                          wm.self().tackleProbability() );
            return this->doKick( agent );
        }
    }


    agent->debugClient().addMessage( "ball dist=%f", wm.ball().distFromSelf() );


    //
    // set parameters
    //
    const Rect2D shrinked_penalty_area
        ( Vector2D( SP.ourTeamGoalLineX(),
                    - (SP.penaltyAreaHalfWidth() - 1.0) ),
          Size2D( SP.penaltyAreaLength() - 1.0,
                  (SP.penaltyAreaHalfWidth() - 1.0) * 2.0 ) );

    const bool is_shoot_ball
        = ShootSimulator::is_ball_moving_to_our_goal( wm.ball().pos(),
                                                      wm.ball().vel(),
                                                      1.0 );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPlayOnMove) is_shoot_ball = %s",
                  ( is_shoot_ball ? "true" : "false" ) );

#ifdef VISUAL_DEBUG
    if ( is_shoot_ball )
    {
        agent->debugClient().addLine( wm.self().pos() + Vector2D( -2.0, -2.0 ),
                                      wm.self().pos() + Vector2D( -2.0, +2.0 ) );
    }
#endif

    const bool is_despair_situation
        = ( is_shoot_ball && self_intercept_point.x <= SP.ourTeamGoalLineX() );



    //
    // tackle
    //
    if ( doTackleIfNecessary( agent, is_despair_situation ) )
    {
        return true;
    }


    //
    // chase ball
    //
    if ( doChaseBallIfNessary( agent,
                               is_penalty_kick_mode,
                               is_despair_situation,
                               self_reach_cycle,
                               teammate_reach_cycle,
                               opponent_reach_cycle,
                               self_intercept_point,
                               shrinked_penalty_area ) )
    {
        return true;
    }


    //
    // check ball
    //
    if ( doFindBallIfNecessary( agent, opponent_reach_cycle ) )
    {
        return true;
    }


    //
    // positioning
    //
    if ( doPositioning( agent, ball_pos,
                        is_penalty_kick_mode, is_despair_situation ) )
    {
        return true;
    }


    //
    // default behavior
    //
    agent->debugClient().addMessage( "Savior:NoAction" );
    dlog.addText( Logger::ROLE,
                  __FILE__ ":(doPlayOnMove) no action" );
    agent->doTurn( 0.0 );
    return true;
}

/*-----------------------------------------------------------------------*/
/*!

 */

void
Bhv_SaviorPenaltyKick::setDefaultNeckAction( rcsc::PlayerAgent * agent,
                                  const bool is_penalty_kick_mode,
                                  const int predict_step )
{
    const WorldModel & wm = agent->world();

    if ( is_penalty_kick_mode )
    {
        agent->setNeckAction( new Neck_ChaseBall() );
    }
    else if ( wm.ball().pos().x > 0.0 )
    {
        agent->setNeckAction( new Neck_ScanField() );
    }
    else if ( wm.ball().seenPosCount() == 0 )
    {
        if ( predict_step >= 10 )
        {
            agent->setNeckAction( new Neck_ScanField() );
        }
        else if ( predict_step > 3 )
        {
            agent->setNeckAction( new Neck_TurnToBall() );
        }
        else
        {
            agent->setNeckAction( new Neck_ChaseBall() );
        }
    }
    else
    {
        agent->setNeckAction( new Neck_ChaseBall() );
    }
}

/*-----------------------------------------------------------------------*/
/*!

 */

bool
Bhv_SaviorPenaltyKick::doPenaltyKickPositioning( rcsc::PlayerAgent * agent,
                                                 const Vector2D & ball_pos,
                                                 const bool is_despair_situation )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    const Vector2D & current_ball_pos = wm.ball().pos();
    const int opponent_reach_cycle = wm.interceptTable()->opponentReachCycle();

    dlog.addText( Logger::ROLE,
                  __FILE__ ":(doPenaltyKickPositioning) start" );

    const Vector2D best_position = this->getBasicPosition( agent );

    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickPositioning) best position=(%.1f %.1f)",
                  best_position.x, best_position.y );

    double max_position_error = 0.70;
    double dash_power = SP.maxDashPower();

    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickPositioning) max_error base = %f",
                  max_position_error );

    const double diff = (best_position - wm.self().pos()).r();

    if ( diff >= 10.0 )
    {
        // for speedy movement
        max_position_error = 1.9;
    }
    else if ( diff >= 5.0 )
    {
        // for speedy movement
        max_position_error = 1.0;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickPositioning) max_position_error = %.2f",
                  max_position_error );

    dash_power = wm.self().getSafetyDashPower( dash_power );

    if ( is_despair_situation )
    {
        if ( max_position_error < 1.5 )
        {
            max_position_error = 1.5;

            dlog.addText( Logger::ROLE,
                          __FILE__":(doPenaltyKickPositioning) despair_situation, "
                          "position error thr changed to %.2f",
                          max_position_error );
        }
    }


    max_position_error = 0.2;

    dlog.addText( Logger::ROLE,
                  __FILE__":(doPenaltyKickPositioning) positioning target = [%f, %f], "
                  "position_error_thr=%f, dash_power=%f",
                  best_position.x, best_position.y,
                  max_position_error, dash_power );

    const bool force_back_dash = false;

    if (  Body_SaviorGoToPoint( best_position,
                                max_position_error,
                                dash_power,
                                true,
                                force_back_dash,
                                is_despair_situation ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Savior:Positioning" );
#ifdef VISUAL_DEBUG
        agent->debugClient().setTarget( best_position );
        agent->debugClient().addCircle( best_position, max_position_error );
#endif
        dlog.addText( Logger::ROLE,
                      __FILE__ ":(doPenaltyKickPositioning) go to (%.2f %.2f) error_thr=%.3f dash_power=%.1f force_back=%d",
                      best_position.x, best_position.y,
                      max_position_error,
                      dash_power,
                      static_cast< int >( force_back_dash ) );
        return true;
    }

    //
    // emergency stop
    //
    if ( opponent_reach_cycle <= 1
         && (current_ball_pos - wm.self().pos()).r() < 10.0
         && wm.self().vel().r() >= 0.05 )
    {
        if ( Body_StopDash( true ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:EmergemcyStop" );
            dlog.addText( Logger::ROLE,
                          __FILE__ ":(doPenaltyKickPositioning) emergency stop" );
            return true;
        }
    }


    //
    // go to position with minimum error
    //
    const PlayerObject * first_opponent = wm.interceptTable()->fastestOpponent();

    if ( ( first_opponent
           && first_opponent->pos().dist( wm.self().pos() ) >= 5.0 )
         && opponent_reach_cycle >= 3 )
    {
        const double dist_thr = ( wm.gameMode().type() == GameMode::PlayOn
                                  ? 0.3
                                  : std::max( 0.3, wm.ball().distFromSelf() * 0.05 ) );

        if (  Body_SaviorGoToPoint( best_position,
                                    dist_thr,
                                    dash_power,
                                    true,
                                    false,
                                    is_despair_situation ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:TunePositioning" );
#ifdef VISUAL_DEBUG
            agent->debugClient().setTarget( best_position );
            agent->debugClient().addCircle( best_position, 0.3 );
#endif
            dlog.addText( Logger::ROLE,
                          __FILE__ ":(doPenaltyKickPositioning) go to position with minimum error. target=(%.2f %.2f) dash_power=%.1f",
                          best_position.x, best_position.y,
                          dash_power );
            return true;
        }
    }


    //
    // stop
    //
    if ( wm.self().vel().rotatedVector( - wm.self().body() ).absX() >= 0.01 )
    {
        if ( Body_StopDash( true ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:Stop" );
            dlog.addText( Logger::ROLE,
                          __FILE__ ":(doPenaltyKickPositioning) stop" );
            return true;
        }
    }


    //
    // turn body angle against ball
    //
    const Vector2D final_body_pos = wm.self().inertiaFinalPoint();

    AngleDeg target_body_angle = 0.0;

    if ( (ball_pos - final_body_pos).r() <= 5.0
         && ( wm.kickableOpponent() || wm.kickableTeammate() ) )
    {
        AngleDeg diff_body_angle = 0.0;

        if ( final_body_pos.y > 0.0 )
        {
            diff_body_angle = - 90.0;
        }
        else
        {
            diff_body_angle = + 90.0;
        }

        target_body_angle = (ball_pos - final_body_pos).th()
            + diff_body_angle;

        dlog.addText( Logger::ROLE,
                      __FILE__ ":(doPenaltyKickPositioning) target body angle = %.2f."
                      " final_body_pos = (%.2f %.2f)",
                      target_body_angle.degree(),
                      final_body_pos.x, final_body_pos.y );
    }


    if ( ( target_body_angle - wm.self().body() ).abs() > 10.0 )
    {
        if ( Body_TurnToAngle( target_body_angle ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:TurnTo%.0f",
                                             target_body_angle.degree() );
            dlog.addText( Logger::ROLE,
                          __FILE__ ":(doPenaltyKickPositioning) turn to angle %.1f",
                          target_body_angle.degree() );
            return true;
        }
    }


    const bool use_extended_side_chase = false;
    if ( use_extended_side_chase )
    {
        double dash_dir = -180.0;
        double temp = 100000.0;
        double best_effect = 100000.0;
        PlayerObject opp = * wm.opponentsFromBall().front();
        Vector2D next_my_pos = wm.self().playerType().inertiaPoint(wm.self().pos(), wm.self().vel(),1);
        //					next_my_pos =wm.self().pos() + wm.self().vel();

        //
        // if opp is kikable, self see ball inertia point
        // ,else self see current ball point.
        //
        Vector2D next_ball_pos ;
        if( opp.isKickable() )
        {
            next_ball_pos = wm.ball().pos() ;
        }
        else
        {
            next_ball_pos = wm.ball().inertiaPoint( wm.interceptTable()->opponentReachStep() ) ;
        }

#ifdef VISUAL_DEBUG

        agent -> debugClient().setTarget( next_ball_pos );
#endif

        double diff_ang = wm.self().body().degree();
        Vector2D target_pos;
        if( opp.isKickable() )
        {
            target_pos = opp.pos() ;//+ wm.ball().inertiaPoint( wm.interceptTable()->opponentReachStep() ) )/2;
        }
        else
        {
            target_pos = wm.ball().inertiaPoint( wm.interceptTable()->opponentReachStep() ) ;
        }

        std::cout<<"bhv_savior_penalty_kick.cpp : sample dash"<<std::endl;
        agent->doDash( dash_power, dash_dir );
        agent->debugClient().addMessage( "Savior:SideChase" );
        dlog.addText( Logger::ROLE,
                      __FILE__ ":(doPositioning) side chase" );
        return true;

        // Vector2D goal_pos = Vector2D( -SP.pitchHalfLength(), 0.0 );

        Vector2D goal_l( -SP.pitchHalfLength(), -SP.goalHalfWidth() );
        Vector2D goal_r( -SP.pitchHalfLength(), +SP.goalHalfWidth() );
        Line2D line_l( goal_l, target_pos );
        Line2D line_r( goal_r, target_pos );


        AngleDeg ball2post_angle_l = ( goal_l - target_pos ).th();
        AngleDeg ball2post_angle_r = ( goal_r - target_pos ).th();

        // NOTE: post_angle_r < post_angle_l
        AngleDeg line_dir = AngleDeg::bisect( ball2post_angle_r,
                                              ball2post_angle_l );
        // Vector2D move_point = Vector2D().setPolar( 2.0 , line_dir );

        Line2D line_mid( target_pos, line_dir ); //angle division into two parts equally

        Line2D goal_line( goal_l, goal_r );

        // Vector2D intersection = goal_line.intersection( line_mid );


        if( ball_pos.x < wm.self().pos().x )
        {
            Body_Intercept().execute( agent );
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPenaltyKickPositioning) emergency move" );
            agent->debugClient().addMessage( "Savior:GoalieGotoPoint(emergencymove) -> (%.1f , %.1f)  ", target_pos.x,target_pos.y);
            return true;
        }

        double discount_rate = 0.85;

        if( wm.self().distFromBall() > 17.0 )
        {
            agent->debugClient().addMessage( "Savior:GoalieGotoPoint -> (%.1f , %.1f)  ", target_pos.x,target_pos.y);
            Body_SaviorGoToPoint( getGoalieMovePos(target_pos,discount_rate),//target_point,
                                  std::max( SP.catchableArea(),
                                            SP.tackleDist() ), //max_error,
                                  60.0,			 //max_power,
                                  true, //use_back_dash,
                                  true, //force_back_dash,
                                  true//emergency_mode
                                  ).execute( agent );
            return true;

        }

        double dir_target = (wm.self().body().degree() > 0.0
                             ? +90.0    //+ (wm.ball().pos() - goal_pos).th().degree()
                             : -90.0 );//+ (wm.ball().pos() - goal_pos).th().degree() );


        if ( std::fabs( wm.self().body().degree() - dir_target ) > 3.0 )
        {
            if ( Body_TurnToAngle( dir_target ).execute( agent ) )
            {
                agent -> debugClient().addMessage( "BodyTurnToAngle : %.1f", dir_target );
                return true;
            }
        }

        double dash_power = 0.0;
        Vector2D predict_pos_2 ;
        double kick_dist = 0.0;

        const bool is_shoot_ball
            = ShootSimulator::is_ball_moving_to_our_goal( wm.ball().pos(),
                                                          wm.ball().vel(),
                                                          1.0 );
        next_my_pos = wm.self().pos();
        Vector2D next_my_vel = wm.self().vel();

        if( wm.self().playerTypePtr() )
            kick_dist = wm.self().playerTypePtr() -> kickableArea() - 0.0;

        for ( double temp_power = -100.0 ; temp_power <= 100.0 ; temp_power += 10.0 )
        {
            for ( double temp_dir = -180.0 ; temp_dir < 180.0 ; temp_dir += SP.dashAngleStep() )
            {

                if( wm.self().playerTypePtr() )
                {
                    omniDash(agent, next_my_vel ,next_my_pos,AngleDeg(temp_dir+diff_ang), temp_power);
                    predict_pos_2 = next_my_pos + next_my_vel;
                    if( std::min(line_l.dist(wm.self().pos()),line_r.dist(wm.self().pos())) <= line_mid.dist(wm.self().pos()) )
                    {
                        temp = line_mid.dist( predict_pos_2 );
                        if(is_shoot_ball)
                        {
                            Body_SaviorGoToPoint(getGoalieMovePos(target_pos, 0.9),
                                                 kick_dist,
                                                 100.0,
                                                 true,
                                                 true,
                                                 true );
                            agent -> debugClient().addMessage("emergency SaviorGoToPoint");
                            return true;
                        }
                        else
                        {
                            //not shoot ball

                            //1. to center
                            //2. to side
                            if( wm.ball().pos().y * wm.ball().vel().y < 0)
                            {
                                agent -> debugClient().addMessage("ball to center");
                                agent -> debugClient().addMessage("emergency doDash to ball");
                                agent -> doDash(  SP.maxPower(), (target_pos - wm.self().pos()).th().degree());
                                return true;
                            }
                        }
                    }


                    else if( opp.isKickable() )
                    {
                        temp=std::fabs(line_l.dist(predict_pos_2) - kick_dist )
                            +std::fabs(line_r.dist(predict_pos_2) - kick_dist );
                    }
                    else
                    {
                        temp = ( wm.ball().inertiaPoint( wm.interceptTable()->selfReachStep()) - predict_pos_2 ).r();
                    }
                    if( best_effect > temp )
                    {
                        best_effect = temp;
                        dash_dir = temp_dir;
                        dash_power = temp_power;
                    }
                }
            }
        }
        omniDash(agent, next_my_vel ,next_my_pos,AngleDeg(dash_dir + diff_ang),dash_power);
        predict_pos_2 = next_my_pos + next_my_vel;
#ifdef VISUAL_DEBUG

        for(double temp_dir = -180.0 ; temp_dir < 180.0 ; temp_dir += SP.dashAngleStep())
        {
            next_my_vel=wm.self().vel();
            next_my_pos=wm.self().pos();
            omniDash(agent, next_my_vel ,next_my_pos,AngleDeg(temp_dir-diff_ang), dash_power);
            agent -> debugClient().addLine( wm.self().pos(),next_my_pos, "yellow");
        }

        if(std::min(line_l.dist(wm.self().pos()),line_r.dist(wm.self().pos())) <= line_mid.dist(wm.self().pos()) )
        {
            agent -> debugClient().addMessage("outArea : move to (%.1f,%.1f)",target_pos.x,target_pos.y);
            agent -> debugClient().addLine( next_my_pos,line_mid.perpendicular(next_my_pos).intersection(line_mid) ,"pink");
            agent -> debugClient().setTarget(line_mid.perpendicular(next_my_pos).intersection(line_mid));
        }
        else
        {
            agent -> debugClient().addLine( next_my_pos, predict_pos_2,"pink");
            agent -> debugClient().addLine( target_pos, predict_pos_2,"pink");
            agent -> debugClient().addMessage("targetArea move to : %1.f, %1.f",target_pos.x,target_pos.y);
            agent -> debugClient().addCircle( next_my_pos, kick_dist,"blue");
            agent -> debugClient().addCircle( predict_pos_2, kick_dist,"pink");
        }
#endif

        agent->doDash( dash_power, dash_dir );
        agent->debugClient().addMessage( "Savior:ExtendedSideChase power : %.1f  dir : %.1f",
                                         dash_power,
                                         dash_dir );
        dlog.addText( Logger::ROLE,
                      __FILE__ ":(doExtendedSideChase) power : %.1f  dir : %.1f",
                      dash_power,
                      dash_dir );
    }
    else
    {
        double dash_dir;
        if ( wm.self().body().degree() > 0.0 )
        {
            dash_dir = -90.0;
        }
        else
        {
            dash_dir = +90.0;
        }
        agent->doDash( SP.maxPower(), dash_dir );
        agent->debugClient().addMessage( "Savior:SideChase" );


        dlog.addText( Logger::ROLE,
                      __FILE__ ":(doPositioning) side chase" );
        return true;
    }

    if ( Body_TurnToAngle( target_body_angle ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Savior:AdjustTurnTo%.0f",
                                              target_body_angle.degree() );
        dlog.addText( Logger::ROLE,
                           __FILE__ ":(doPositioning) adjust turn to angle %.1f",
                           target_body_angle.degree() );
        return true;
    }

    agent -> debugClient().addMessage("decision fault");
    return false;

}

/*-----------------------------------------------------------------------*/
/*!

 */

bool
Bhv_SaviorPenaltyKick::doPositioning( rcsc::PlayerAgent * agent,
                                      const Vector2D & ball_pos,
                                      const bool is_penalty_kick_mode,
                                      const bool is_despair_situation )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    const Vector2D & current_ball_pos = wm.ball().pos();
    const int opponent_reach_cycle = wm.interceptTable()->opponentReachCycle();
    const int teammate_reach_cycle = wm.interceptTable()->teammateReachCycle();

    dlog.addText( Logger::ROLE,
                  __FILE__ ":(doPositioning) start" );

    bool dangerous = false;
    bool aggressive = false;
    bool goal_line_positioning = false;

    const Vector2D best_position = this->getBasicPosition( agent );

    dlog.addText( Logger::ROLE,
                  __FILE__":(doPositioning) best position=(%.1f %.1f)",
                  best_position.x, best_position.y );
    dlog.addText( Logger::ROLE,
                  "__ %s%s%s%s",
                  dangerous ? "dangerous " : "x ",
                  aggressive ? "aggressive " : "x ",
                  goal_line_positioning ? "gline " : "x " );

#ifdef VISUAL_DEBUG
    if ( goal_line_positioning )
    {
        agent->debugClient().addLine( Vector2D
                                      ( SP.ourTeamGoalLineX() - 1.0,
                                        - SP.goalHalfWidth() ),
                                      Vector2D
                                      ( SP.ourTeamGoalLineX() - 1.0,
                                        + SP.goalHalfWidth() ) );
    }

    agent->debugClient().addLine( wm.self().pos(), best_position );
#endif


    double max_position_error = 0.70;
    double goal_line_positioning_y_max_position_error = 0.3;
    double dash_power = SP.maxDashPower();

    dlog.addText( Logger::ROLE,
                  __FILE__":(doPositioning) max_error base = %f",
                  max_position_error );

    if ( aggressive )
    {
        if ( wm.ball().distFromSelf() >= 30.0 )
        {
            max_position_error
                = bound( 2.0, 2.0 + (wm.ball().distFromSelf() - 30.0) / 20.0, 3.0 );
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) aggressive, ball is far. max_error = %f",
                          max_position_error );
        }
    }


    const double diff = (best_position - wm.self().pos()).r();

    if ( wm.ball().distFromSelf() >= 30.0
         && wm.ball().pos().x >= -20.0
         && diff < 5.0
         && ! is_penalty_kick_mode )
    {
        dash_power = SP.maxDashPower() * 0.7;

#ifdef VISUAL_DEBUG
        agent->debugClient().addLine
            ( wm.self().pos() + Vector2D( -1.0, +3.0 ),
              wm.self().pos() + Vector2D( +1.0, +3.0 ) );

        agent->debugClient().addLine
            ( wm.self().pos() + Vector2D( -1.0, -3.0 ),
              wm.self().pos() + Vector2D( +1.0, -3.0 ) );
#endif
    }

    if ( goal_line_positioning )
    {
        if ( is_penalty_kick_mode )
        {
            max_position_error = 0.7;
            goal_line_positioning_y_max_position_error = 0.5;
        }
        else
        {
            if ( std::fabs( wm.self().pos().x - best_position.x ) > 5.0 )
            {
                max_position_error = 2.0;
                goal_line_positioning_y_max_position_error = 1.5;

                dlog.addText( Logger::ROLE,
                              __FILE__":(doPositioning) goal_line_positioning thr=%.2f",
                              max_position_error );
            }

            if ( ball_pos.x > -10.0
                 && current_ball_pos.x > +10.0
                 && wm.ourDefenseLineX() > -20.0
                 && std::fabs( wm.self().pos().x - best_position.x ) < 5.0 )
            {
                // safety, save stamina
                dash_power = SP.maxDashPower() * 0.5;
                max_position_error = 2.0;
                goal_line_positioning_y_max_position_error = 1.5;

                dlog.addText( Logger::ROLE,
                              __FILE__":(doPositioning) goal_line_positioning, save stamina condition1 thr=%.2f",
                              max_position_error );
            }
            else if ( ball_pos.x > -20.0
                      && current_ball_pos.x > +20.0
                      && wm.ourDefenseLineX() > -25.0
                      && teammate_reach_cycle < opponent_reach_cycle
                      && std::fabs( wm.self().pos().x - best_position.x ) < 5.0 )
            {
                // safety, save stamina
                dash_power = SP.maxDashPower() * 0.6;
                max_position_error = 1.8;
                goal_line_positioning_y_max_position_error = 1.5;

                dlog.addText( Logger::ROLE,
                              __FILE__":(doPositioning) goal_line_positioning, save stamina condition2 thr=%.2f",
                              max_position_error );
            }
        }
    }
    else if ( dangerous )
    {
        if ( diff >= 10.0 )
        {
            // for speedy movement
            max_position_error = 1.9;
        }
        else if ( diff >= 5.0 )
        {
            // for speedy movement
            max_position_error = 1.0;
        }

        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) dangerous, thr=%.2f",
                      max_position_error );
    }
    else
    {
        if ( diff >= 10.0 )
        {
            // for speedy movement
            max_position_error = 1.5;
        }
        else if ( diff >= 5.0 )
        {
            // for speedy movement
            max_position_error = 1.0;
        }

        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) normal update, diff=%.2f, thr=%.2f",
                      diff, max_position_error );
    }

    dash_power = wm.self().getSafetyDashPower( dash_power );

    if ( is_despair_situation )
    {
        if ( max_position_error < 1.5 )
        {
            max_position_error = 1.5;

            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) despair_situation, "
                          "position error thr changed to %.2f",
                          max_position_error );
        }
    }


    //
    // update dash power by play mode
    //
    {
        const GameMode mode = wm.gameMode();

        if ( mode.type() == GameMode::KickIn_
             || mode.type() == GameMode::OffSide_
             || mode.type() == GameMode::CornerKick_
             || ( ( mode.type() == GameMode::GoalKick_
                    || mode.type() == GameMode::GoalieCatch_
                    || mode.type() == GameMode::BackPass_ )
                  && mode.side() == wm.ourSide() )
             || ( mode.type() == GameMode::FreeKick_ ) )
        {
            max_position_error = 5.0;

            if ( wm.ball().pos().x > ServerParam::i().ourPenaltyAreaLineX() + 10.0 )
            {
                dash_power = std::min( dash_power, wm.self().playerType().staminaIncMax() );
            }

            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) updated max_position_error_thr"
                          " and dash_power by game mode,"
                          " thr=%.2f, dash_power=%.2f",
                          max_position_error, dash_power );
        }
        else if ( is_penalty_kick_mode )
        {
            max_position_error = 0.2;
        }
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doPositioning) positioning target = [%f, %f], "
                  "position_error_thr=%f, dash_power=%f",
                  best_position.x, best_position.y,
                  max_position_error, dash_power );

    const bool force_back_dash = false;

    if ( goal_line_positioning
         && ! is_despair_situation )
    {
        if ( doGoalLinePositioning( agent ,
                                    best_position,
                                    2.0,
                                    max_position_error,
                                    goal_line_positioning_y_max_position_error,
                                    dash_power ) )
        {
            return true;
        }
    }
    else
    {
        if (  Body_SaviorGoToPoint( best_position,
                                    max_position_error,
                                    dash_power,
                                    true,
                                    force_back_dash,
                                    is_despair_situation ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:Positioning" );
#ifdef VISUAL_DEBUG
            agent->debugClient().setTarget( best_position );
            agent->debugClient().addCircle( best_position, max_position_error );
#endif
            dlog.addText( Logger::ROLE,
                          __FILE__ ":(doPositioning) go to (%.2f %.2f) error_thr=%.3f dash_power=%.1f force_back=%d",
                          best_position.x, best_position.y,
                          max_position_error,
                          dash_power,
                          static_cast< int >( force_back_dash ) );
            return true;
        }
    }


    //
    // emergency stop
    //
    if ( opponent_reach_cycle <= 1
         && (current_ball_pos - wm.self().pos()).r() < 10.0
         && wm.self().vel().r() >= 0.05 )
    {
        if ( Body_StopDash( true ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:EmergemcyStop" );
            dlog.addText( Logger::ROLE,
                          __FILE__ ":(doPositioning) emergency stop" );
            return true;
        }
    }


    //
    // go to position with minimum error
    //
    const PlayerObject * first_opponent = wm.interceptTable()->fastestOpponent();

    if ( ( first_opponent
           && first_opponent->pos().dist( wm.self().pos() ) >= 5.0 )
         && opponent_reach_cycle >= 3 )
    {
        if ( goal_line_positioning
             && ! is_despair_situation )
        {
            if ( doGoalLinePositioning( agent,
                                        best_position,
                                        2.0,
                                        0.3,
                                        0.3,
                                        dash_power ) )
            {
                return true;
            }
        }
        else
        {
            const double dist_thr = ( wm.gameMode().type() == GameMode::PlayOn
                                      ? 0.3
                                      : std::max( 0.3, wm.ball().distFromSelf() * 0.05 ) );

            if ( Body_SaviorGoToPoint( best_position,
                                       dist_thr,
                                       dash_power,
                                       true,
                                       false,
                                       is_despair_situation ).execute( agent ) )
            {
                agent->debugClient().addMessage( "Savior:TunePositioning" );
#ifdef VISUAL_DEBUG
                agent->debugClient().setTarget( best_position );
                agent->debugClient().addCircle( best_position, 0.3 );
#endif
                dlog.addText( Logger::ROLE,
                              __FILE__ ":(doPositioning) go to position with minimum error. target=(%.2f %.2f) dash_power=%.1f",
                              best_position.x, best_position.y,
                              dash_power );
                return true;
            }
        }
    }


    //
    // stop
    //
    if ( wm.self().vel().rotatedVector( - wm.self().body() ).absX() >= 0.01 )
    {
        if ( Body_StopDash( true ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:Stop" );
            dlog.addText( Logger::ROLE,
                          __FILE__ ":(doPositioning) stop" );
            return true;
        }
    }


    //
    // turn body angle against ball
    //
    const Vector2D final_body_pos = wm.self().inertiaFinalPoint();

    AngleDeg target_body_angle = 0.0;


    if ( ((ball_pos - final_body_pos).r() <= 5.0
          && (wm.kickableOpponent() || wm.kickableTeammate()) )
         || goal_line_positioning )
    {
        if ( goal_line_positioning )
        {
            target_body_angle = sign( final_body_pos.y ) * 90.0;
        }
        else
        {
            AngleDeg diff_body_angle = 0.0;

            if ( final_body_pos.y > 0.0 )
            {
                diff_body_angle = - 90.0;
            }
            else
            {
                diff_body_angle = + 90.0;
            }

            target_body_angle = (ball_pos - final_body_pos).th()
                + diff_body_angle;
        }

        dlog.addText( Logger::ROLE,
                      __FILE__ ":(doPositioning) target body angle = %.2f."
                      " final_body_pos = (%.2f %.2f)",
                      target_body_angle.degree(),
                      final_body_pos.x, final_body_pos.y );
    }

    if ( ( target_body_angle - wm.self().body() ).abs() > 10.0 )
    {
        if ( Body_TurnToAngle( target_body_angle ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:TurnTo%.0f",
                                             target_body_angle.degree() );
            dlog.addText( Logger::ROLE,
                          __FILE__ ":(doPositioning) turn to angle %.1f",
                          target_body_angle.degree() );
            return true;
        }
    }



    double dash_dir = 0.0;
    if ( is_penalty_kick_mode )
    {
        if ( wm.self().body().degree() > 0.0 )
        {
            dash_dir = -90.0;
        }
        else
        {
            dash_dir = +90.0;
        }
    }
    agent->doDash( SP.maxPower(), dash_dir );
    agent->debugClient().addMessage( "Savior:SideChase" );

    dlog.addText( Logger::ROLE,
                        __FILE__ ":(doPositioning) side chase" );
    return true;



    if ( Body_TurnToAngle( target_body_angle ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Savior:AdjustTurnTo%.0f",
                                              target_body_angle.degree() );
        dlog.addText( Logger::ROLE,
                           __FILE__ ":(doPositioning) adjust turn to angle %.1f",
                           target_body_angle.degree() );
        return true;
    }

    agent -> debugClient().addMessage("decision fault");

    return false;
}

/*-----------------------------------------------------------------------*/
/*!

 */

void
Bhv_SaviorPenaltyKick::omniDash( rcsc::PlayerAgent * agent,
                                 Vector2D & self_vel,
                                 Vector2D & self_pos,
                                 AngleDeg dash_angle,
                                 double dash_power )
{

    const WorldModel &wm = agent -> world();
    const rcsc::PlayerType & type = wm.self().playerType();
    double M_effort_max = type.effortMax();
    double M_dash_power_rate = type.dashPowerRate();
    double M_player_speed_max = type.playerSpeedMax();
    double M_player_decay = type.playerDecay();

    AngleDeg dir = round( dash_angle.degree() / 45 ) * 45;

    double back_dash_rate = ServerParam::i().backDashRate();
    double side_dash_rate = ServerParam::i().sideDashRate();

    double dir_rate = ( fabs( dir.degree() ) > 90
                        ? back_dash_rate - ( back_dash_rate - side_dash_rate )
                        * ( 1.0 - ( fabs( dir.degree() ) - 90 ) / 90 )
                        : side_dash_rate + ( 1.0 - side_dash_rate )
                        * ( 1.0 - fabs( dir.degree() ) / 90 ) );

    double effector_power = M_effort_max * M_dash_power_rate * dash_power * dir_rate;

    Vector2D accel( effector_power * dir.cos(),
                    effector_power * dir.sin() );

    if ( accel.r() > ServerParam::i().playerAccelMax() )
    {
        accel *= ServerParam::i().playerAccelMax() / accel.r();
    }

    self_vel += accel;
    if ( self_vel.r() > M_player_speed_max )
    {
        self_vel *= M_player_speed_max / self_vel.r();
    }

    self_pos += self_vel;
    self_vel *= M_player_decay;

    return ;

 }

/*---------------------------------------------------------------------*/
/*!
 */

Vector2D
Bhv_SaviorPenaltyKick::getGoalieMovePos( const Vector2D & ball_pos, double dist_rate)
{
    const ServerParam & SP = ServerParam::i();
    const double min_x = -SP.pitchHalfLength() + SP.catchAreaLength()*0.9;

    if ( ball_pos.x < -49.0 )
    {
        if ( ball_pos.absY() < SP.goalHalfWidth() )
        {
            return Vector2D( min_x, ball_pos.y );
        }
        else
        {
            return Vector2D( min_x,
                             sign( ball_pos.y ) * SP.goalHalfWidth() );
        }
    }

    Vector2D goal_l( -SP.pitchHalfLength(), -SP.goalHalfWidth() );
    Vector2D goal_r( -SP.pitchHalfLength(), +SP.goalHalfWidth() );

    AngleDeg ball2post_angle_l = ( goal_l - ball_pos ).th();
    AngleDeg ball2post_angle_r = ( goal_r - ball_pos ).th();

    // NOTE: post_angle_r < post_angle_l
    AngleDeg line_dir = AngleDeg::bisect( ball2post_angle_r,
                                          ball2post_angle_l );
    Vector2D move_point = Vector2D().setPolar( 2.0 , line_dir );


    Line2D line_mid( ball_pos, line_dir );
    Line2D goal_line( goal_l, goal_r );

    Vector2D intersection = goal_line.intersection( line_mid );
    if ( intersection.isValid() )
    {
        move_point = intersection + ( ball_pos - intersection) * dist_rate;
        return move_point;
    }

    return ball_pos;
}

/*---------------------------------------------------------------------*/
/*!

 */

bool
Bhv_SaviorPenaltyKick::doCatchIfPossible( rcsc::PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    const double MAX_SELF_BALL_ERROR = 0.5; // meter

    const bool can_catch_the_ball
        = ( // goalie
           wm.self().goalie()
           // not back pass
           && wm.lastKickerSide() != wm.ourSide()
           // catchable playmode
           && ( wm.gameMode().type() == GameMode::PlayOn
                || wm.gameMode().type() == GameMode::PenaltyTaken_ )
           // not in catch ban cycle
           && wm.time().cycle()
           >= agent->effector().getCatchTime().cycle() + SP.catchBanCycle()
           // ball is in penalty area
           && ( wm.ball().pos().x < ( SP.ourPenaltyAreaLineX()
                                      + SP.ballSize() * 2
                                      - MAX_SELF_BALL_ERROR )
                && wm.ball().pos().absY()  < ( SP.penaltyAreaHalfWidth()
                                               + SP.ballSize() * 2
                                               - MAX_SELF_BALL_ERROR ) )
           // // in catchable distance
           // && wm.ball().distFromSelf() < wm.self().playerType().reliableCatchableDist() - 0.01 );
           && wm.self().catchProbability() > 0.99 );


    // if catchable situation
    if ( can_catch_the_ball
         && ( ! wm.kickableTeammate() || wm.kickableOpponent() ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doCatchIfPossible) doCatch prob=%f",
                      wm.self().catchProbability() );
        agent->setNeckAction( new Neck_TurnToBall() );
        return agent->doCatch();
    }

    return false;
}

/*-------------------------------------------------------------------------------*/
/*!

 */

bool
Bhv_SaviorPenaltyKick::doTackleIfNecessary( rcsc::PlayerAgent * agent,
                                            const bool is_despair_situation )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    double tackle_prob_threshold = 0.8;

    if ( is_despair_situation )
    {
#ifdef VISUAL_DEBUG
        agent->debugClient().addLine( Vector2D
                                      ( SP.ourTeamGoalLineX() - 2.0,
                                        - SP.goalHalfWidth() ),
                                      Vector2D
                                      ( SP.ourTeamGoalLineX() - 2.0,
                                        + SP.goalHalfWidth() ) );
#endif

        tackle_prob_threshold = EPS;

        const Vector2D ball_next = wm.ball().inertiaPoint( 1 );
        const bool next_step_goal = ( ball_next.x
                                      < SP.ourTeamGoalLineX() + SP.ballRand() );

        if ( ! next_step_goal )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) next step, ball is in goal,"
                          " ball = [%f, %f], ball rand = %f = %f",
                          ball_next.x, ball_next.y,
                          SP.ballRand() );

            const double next_tackle_prob_forward
                = getSelfNextTackleProbabilityWithDash( wm, + SP.maxDashPower() );
            const double next_tackle_prob_backword
                = getSelfNextTackleProbabilityWithDash( wm, SP.minDashPower() );

            dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) next_tackle_prob_forward = %f",
                          next_tackle_prob_forward );
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) next_tackle_prob_backword = %f",
                          next_tackle_prob_backword );

            if ( next_tackle_prob_forward > wm.self().tackleProbability() )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__":(doTackleIfNecessary) dash forward expect next tackle" );

                agent->doDash( SP.maxDashPower() );

                return true;
            }
            else if ( next_tackle_prob_backword > wm.self().tackleProbability() )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__":(doTackleIfNecessary) dash backward expect next tackle" );
                agent->doDash( SP.minDashPower() );

                return true;
            }
        }

        if ( ! next_step_goal )
        {
            const double next_tackle_prob_with_turn
                = getSelfNextTackleProbabilityWithTurn( wm );

            dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) next_tackle_prob_with_turn = %f",
                          next_tackle_prob_with_turn );
            if ( next_tackle_prob_with_turn > wm.self().tackleProbability() )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__":(doTackleIfNecessary) turn to next ball position for tackling" );
                Body_TurnToPoint( ball_next, 1 ).execute( agent );
                return true;
            }
        }
    }

    if ( is_despair_situation )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doTackleIfNecessary) despair situation. try deflecting tackle" );
        if ( Bhv_DeflectingTackle( tackle_prob_threshold, true ).execute( agent ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) despair situation. done deflecting tackle" );
            agent->debugClient().addMessage( "Savior:DeflectingTackle(%f)",
                                             wm.self().tackleProbability() );
            return true;
        }
    }

    return false;
}

/*-----------------------------------------------------------------------------------*/
/*!

 */

 bool
 Bhv_SaviorPenaltyKick::doChaseBallIfNessary( PlayerAgent * agent,
                                              const bool is_penalty_kick_mode,
                                              const bool is_despair_situation,
                                              const int self_reach_cycle,
                                              const int teammate_reach_cycle,
                                              const int opponent_reach_cycle,
                                              const Vector2D & self_intercept_point,
                                              const Rect2D & shrinked_penalty_area )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    if ( is_despair_situation )
    {
#ifdef VISUAL_DEBUG
        agent->debugClient().addLine( wm.self().pos()
                                      + Vector2D( +2.0, -2.0 ),
                                      wm.self().pos()
                                      + Vector2D( +2.0, +2.0 ) );
#endif
        dlog.addText( Logger::ROLE,
                      __FILE__":(doChaseBallIfNecessary) despair situation." );
        agent->debugClient().addMessage( "Savior:DespairChase" );
        if ( this->doChaseBall( agent ) )
        {
            return true;
        }
    }


    if ( wm.gameMode().type() == GameMode::PlayOn
         && ! wm.kickableTeammate() )
    {
        if ( self_reach_cycle <= opponent_reach_cycle
             && ( ( self_reach_cycle + 5 <= teammate_reach_cycle
                    || self_intercept_point.absY() < SP.penaltyAreaHalfWidth() - 1.0 )
                  && shrinked_penalty_area.contains( self_intercept_point ) )
             && ( self_reach_cycle <= teammate_reach_cycle
                  || ( shrinked_penalty_area.contains( self_intercept_point )
                       && self_reach_cycle <= teammate_reach_cycle + 3 ) )
             )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doChaseBallIfNecessary) normal chase." );
            agent->debugClient().addMessage( "Savior:Chase" );

            if ( this->doChaseBall( agent ) )
            {
                return true;
            }
        }

#if 1
        // 2009-12-13 akiyama
        if ( self_reach_cycle + 3 < opponent_reach_cycle
             && self_reach_cycle + 2 <= teammate_reach_cycle
             && self_intercept_point.x > SP.ourPenaltyAreaLineX()
             && self_intercept_point.absY() < SP.penaltyAreaHalfWidth() - 1.0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doChaseBallIfNecessary) aggressive chase." );
            agent->debugClient().addMessage( "Savior:ChaseAggressive" );

            if ( this->doChaseBall( agent ) )
            {
                return true;
            }
        }
#endif
    }


    if ( is_penalty_kick_mode
         && ( self_reach_cycle + 1 < opponent_reach_cycle
#ifdef PENALTY_SHOOTOUT_BLOCK_IN_PENALTY_AREA
              && ( shrinked_penalty_area.contains( self_intercept_point )
                   || self_intercept_point.x <= SP.ourTeamGoalLineX() )
#endif
              ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doChaseBallIfNecessary) penalty kick mode." );
        agent->debugClient().addMessage( "Savior:ChasePenaltyKickMode" );

        if ( Body_Intercept().execute( agent ) )
        {
            agent->setNeckAction( new Neck_ChaseBall() );

            return true;
        }
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doChaseBallIfNecessary) false" );

    return false;
}

/*------------------------------------------------------------------------------*/
/*!
 */

bool
Bhv_SaviorPenaltyKick::doFindBallIfNecessary( PlayerAgent * agent,
                                              const int opponent_reach_cycle )
{
    const WorldModel & wm = agent->world();

    long ball_premise_accuracy = 2;
    bool brief_ball_check = false;

    if ( wm.self().pos().x >= -30.0 )
    {
        ball_premise_accuracy = 6;
        brief_ball_check = true;
    }
    else if ( opponent_reach_cycle >= 3 )
    {
        ball_premise_accuracy = 3;
        brief_ball_check = true;
    }

    if ( wm.ball().seenPosCount() > ball_premise_accuracy
         || ( brief_ball_check
              && wm.ball().posCount() > ball_premise_accuracy ) )
    {
        agent->debugClient().addMessage( "Savior:FindBall" );
        dlog.addText( Logger::ROLE,
                      __FILE__ ":(doFindBallIfNecessary) find ball" );
        return Bhv_NeckBodyToBall().execute( agent );
    }

    dlog.addText( Logger::ROLE,
                  __FILE__ ":(doFindBallIfNecessary) false" );
    return false;
}

/*-----------------------------------------------------------------------------*/
/*!

 */

bool
Bhv_SaviorPenaltyKick::doGoalLinePositioning( PlayerAgent * agent,
                                              const Vector2D & target_position,
                                              const double low_priority_x_position_error,
                                              const double max_x_position_error,
                                              const double max_y_position_error,
                                              const double dash_power )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    const double dist = target_position.dist( wm.self().pos() );
    const double x_diff = std::fabs( target_position.x - wm.self().pos().x );
    const double y_diff = std::fabs( target_position.y - wm.self().pos().y );

    if ( x_diff < max_x_position_error
         && y_diff < max_y_position_error )
    {
        return false;
    }

    Vector2D p = target_position;
    const bool use_back_dash = ( agent->world().ball().pos().x <= -20.0 );

    const bool x_near = ( x_diff < max_x_position_error );
    const bool y_near = ( y_diff < max_y_position_error );


#ifdef VISUAL_DEBUG
    agent->debugClient().addRectangle( Rect2D::from_center
                                       ( target_position,
                                         max_x_position_error * 2.0,
                                         max_y_position_error * 2.0 ) );
#endif

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGoalLinePositioning): "
                  "max_position_error = %f, "
                  "enlarged_max_position_error = %f, "
                  "dist = %f, "
                  "x_diff = %f, y_diff = %f, "
                  "x_near = %s, y_near = %s",
                  max_x_position_error,
                  max_y_position_error,
                  dist,
                  x_diff, y_diff,
                  ( x_near ? "true" : "false" ),
                  ( y_near ? "true" : "false" ) );

    if ( x_diff > low_priority_x_position_error )
    {
        if ( Body_SaviorGoToPoint( p,
                                   std::min( max_x_position_error,
                                             max_y_position_error ),
                                   dash_power,
                                   use_back_dash,
                                   false ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:GoalLinePositioning:normal" );
#ifdef VISUAL_DEBUG
            agent->debugClient().setTarget( p );
#endif
            dlog.addText( Logger::ROLE,
                          __FILE__ ": go to (%.2f %.2f) error=%.3f dash_power=%.1f",
                          p.x, p.y,
                          std::min( max_x_position_error,
                                    max_y_position_error ),
                          dash_power );

            return true;
        }
    }
    else if ( ! y_near )
    {
        p.assign( wm.self().pos().x, p.y );


        if (  Body_SaviorGoToPoint( p,
                                    std::min( max_x_position_error,
                                              max_y_position_error ),
                                    dash_power,
                                    true,
                                    false ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:GoalLinePositioning:AdjustY" );
#ifdef VISUAL_DEBUG
            agent->debugClient().setTarget( p );
#endif
            dlog.addText( Logger::ROLE,
                          __FILE__ ": go to (%.2f %.2f) error=%.3f dash_power=%.1f",
                          p.x, p.y, low_priority_x_position_error, dash_power );
            return true;
        }
    }
    else
    {
        const double dir_target = (wm.self().body().degree() > 0.0
                                   ? +90.0
                                   : -90.0 );

        if ( std::fabs( wm.self().body().degree() - dir_target ) > 2.0 )
        {
            if ( Body_TurnToAngle( dir_target ).execute( agent ) )
            {
                return true;
            }
        }


        double side_dash_dir = ( wm.self().body().degree() > 0.0
                                 ? +90.0
                                 : -90.0 );
        if ( wm.self().pos().x < target_position.x )
        {
            side_dash_dir *= -1.0;
        }

        const double side_dash_rate = wm.self().dashRate() * SP.dashDirRate( side_dash_dir );

        double side_dash_power = x_diff / std::max( side_dash_rate, EPS );
        side_dash_power = std::min( side_dash_power, dash_power );

        agent->doDash( side_dash_power, side_dash_dir );
        agent->debugClient().addMessage( "Savior:GoalLinePositioning:SideDash" );
        dlog.addText( Logger::ROLE,
                      __FILE__ ": goal line posisitioning side dash" );

        return true;
    }

    return false;
}


/*-------------------------------------------------------------------------------*/
/*!

 */

bool
Bhv_SaviorPenaltyKick::doKick( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();
    const double clear_y_threshold = SP.penaltyAreaHalfWidth() - 5.0;

    //
    // if exist near opponent, clear the ball
    //
    if ( wm.kickableOpponent()
         || wm.interceptTable()->opponentReachCycle() < 4 )
    {
        Bhv_ClearBall().execute( agent );
        //agent->setNeckAction( new Neck_TurnToLowConfTeammate() );

        agent->debugClient().addMessage( "clear ball" );
        dlog.addText( Logger::ROLE,
                      "opponent near or kickable, clear ball" );
        return true;
    }


    dlog.addText( Logger::ROLE,
                  __FILE__ ": update action chain graph" );


    const CooperativeAction & first_action = ActionChainHolder::i().graph().bestFirstAction();

    //
    // if side of field, clear the ball
    //
    if ( first_action.type() == CooperativeAction::Pass )
    {
        const Vector2D target_point = first_action.targetBallPos();

        dlog.addText( Logger::ROLE,
                      __FILE__": doKick() self pos = [%f, %f], "
                      "pass target = [%f, %f]",
                      wm.self().pos().x, wm.self().pos().y,
                      target_point.x, target_point.y );

        if ( wm.self().pos().absY() > clear_y_threshold
             && ! (target_point.y * wm.self().pos().y > 0.0
                   && std::fabs( target_point.y ) > (std::fabs( wm.self().pos().y ) + 5.0)
                   && target_point.x > wm.self().pos().x + 5.0 ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": doKick() self pos y %f is grater than %f,"
                          "pass target pos y = %f, clear ball",
                          wm.self().pos().absY(),
                          clear_y_threshold,
                          target_point.y );
            agent->debugClient().addMessage( "Savior:SideForceClear" );

            Bhv_ClearBall().execute( agent );
            //agent->setNeckAction( new Neck_TurnToLowConfTeammate() );
            return true;
        }
        else
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": doKick() not force clear" );
        }
    }

    //
    // do chain action
    //
    if ( first_action.type() != CooperativeAction::Hold )
    {
        if ( Bhv_ChainAction().execute( agent ) )
        {
            return true;
        }
    }


    //
    // default clear ball
    //
    {
        if ( Bhv_ClearBall().execute( agent ) )
        {
            //agent->setNeckAction( new Neck_TurnToLowConfTeammate() );

            agent->debugClient().addMessage( "no action, default clear ball" );
            dlog.addText( Logger::ROLE,
                          __FILE__": no action, default clear ball" );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------------------*/
/*!

 */

Vector2D
Bhv_SaviorPenaltyKick::getBasicPosition( PlayerAgent * agent ) const
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    const int opponent_reach_cycle = wm.interceptTable()->opponentReachCycle();
    const int self_reach_cycle = wm.interceptTable()->selfReachCycle();
    const int predict_step = std::min( opponent_reach_cycle, self_reach_cycle );

    // const Vector2D self_pos = wm.self().pos();
    const Vector2D ball_pos = getFieldBoundPredictBallPos( wm, predict_step, 0.5 );

    dlog.addText( Logger::ROLE,
                  __FILE__": (getBasicPosition) predicted ball pos = (%f,%f)",
                  ball_pos.x, ball_pos.y );

    const Rect2D penalty_area( Vector2D( SP.ourTeamGoalLineX(),
                                         - SP.penaltyAreaHalfWidth() ),
                               Size2D( SP.penaltyAreaLength(),
                                       SP.penaltyAreaWidth() ) );

    const Vector2D goal_pos = SP.ourTeamGoalPos();

    double base_dist = 14.0;

    dlog.addText( Logger::ROLE,
                  __FILE__": (getBasicPosition) base_dist=%.2f ball_dir_from_goal=%.1f",
                  base_dist, getDirFromOurGoal( ball_pos ).degree() );

    if ( ! penalty_area.contains( wm.ball().pos() ) )
    {
        if ( ( goal_pos - ball_pos ).r() > 20.0 )
        {
            base_dist = std::min( base_dist,
                                  5.0 + ( ( goal_pos - ball_pos ).r() - 20.0 ) * 0.1 );
        }
        else
        {
            base_dist = 5.0;
        }
    }

    //base_dist = (goal_pos - ball_pos).r() - 4.5;
    double ball_goal_dist = ball_pos.dist( goal_pos );
    base_dist = ball_goal_dist - 3.0;

    double self_goal_dist = wm.self().pos().dist( goal_pos );
    if ( self_goal_dist > base_dist
         && self_goal_dist < ball_goal_dist )
    {
        base_dist = self_goal_dist;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__ ": (getBasicPosition) base dist = %f", base_dist );

    return getBasicPositionFromBall( agent,
                                     ball_pos,
                                     base_dist,
                                     wm.self().pos() );
}

/*----------------------------------------------------------------------------------*/
/*!

 */

Vector2D
Bhv_SaviorPenaltyKick::getBasicPositionFromBall( PlayerAgent * agent,
                                                 const Vector2D & ball_pos,
                                                 const double base_dist,
                                                 const Vector2D & self_pos ) const
{
    const double minimum_goal_x_dist = 1.0;

    const ServerParam & SP =  ServerParam::i();

    const double goal_line_x = SP.ourTeamGoalLineX();
    const double goal_half_width = SP.goalHalfWidth();

    const double alpha = std::atan2( goal_half_width, base_dist );

    const double min_x = ( goal_line_x + minimum_goal_x_dist );

    const Vector2D goal_center = SP.ourTeamGoalPos();
    const Vector2D goal_post_plus( goal_center.x, +goal_half_width );
    const Vector2D goal_post_minus( goal_center.x, -goal_half_width );

    const Line2D line_1( ball_pos, goal_post_plus );
    const Line2D line_2( ball_pos, goal_post_minus );

    const double dist_ball_to_plus_post = ball_pos.dist( goal_post_plus );
    const double dist_ball_to_minus_post = ball_pos.dist( goal_post_minus );

    const double plus_rate = 0.5
        + 0.1 * ( dist_ball_to_plus_post / ( dist_ball_to_plus_post + dist_ball_to_minus_post ) );
    const double minus_rate = 1.0 - plus_rate;

    const AngleDeg plus_post_angle = ( goal_post_plus - ball_pos ).th();
    const AngleDeg minus_post_angle = ( goal_post_minus - ball_pos ).th();
    const double angle_diff = ( plus_post_angle - minus_post_angle ).abs();

    const AngleDeg line_dir = plus_post_angle.isLeftOf( minus_post_angle )
        ? plus_post_angle + ( angle_diff * plus_rate ) + 180.0
        : minus_post_angle + ( angle_diff * minus_rate ) + 180.0;

    // the line biased a little bit from the center of the goal.
    // the bias is calculaed from the ball position.
    const Line2D line_m( ball_pos, line_dir );

    const Line2D goal_line( goal_post_plus, goal_post_minus );

    const Vector2D goal_line_point = goal_line.intersection( line_m );
    if ( ! goal_line_point.isValid() )
    {
        if ( ball_pos.x > 0.0 )
        {
            return Vector2D( min_x, goal_post_plus.y );
        }
        else if ( ball_pos.x < 0.0 )
        {
            return Vector2D( min_x, goal_post_minus.y );
        }
        else
        {
            return Vector2D( min_x, goal_center.y );
        }
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPositionFromBall) line_dir=%.1f goal_line_point=(%.1f %.1f)",
                  line_dir.degree(),
                  goal_line_point.x, goal_line_point.y );
    dlog.addText( Logger::ROLE,
                  "__ plus_post_angle=%.3f minus_post_angle=%.1f",
                  plus_post_angle.degree(), minus_post_angle.degree() );
    dlog.addText( Logger::ROLE,
                  "__ plus_rate=%.3f angle_diff=%.1f",
                  plus_rate, angle_diff );
    dlog.addLine( Logger::ROLE,
                  ball_pos, goal_line_point,
                  "#00F" );

    // angle -> dist
    double dist_from_goal
        = ( line_1.dist( goal_line_point ) + line_2.dist( goal_line_point ) )
        / 2.0
        / std::sin( alpha );

    if ( dist_from_goal <= goal_half_width )
    {
        dist_from_goal = goal_half_width;
    }

    if ( ( ball_pos - goal_line_point ).r() + 1.5 < dist_from_goal )
    {
        dist_from_goal = ( ball_pos - goal_line_point ).r() + 1.5;
    }

    const AngleDeg current_position_line_dir = ( self_pos - goal_line_point ).th();

    const AngleDeg line_dir_error = line_dir - current_position_line_dir;

    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPositionFromBall) position angle error = %f",
                  line_dir_error.abs() );

    agent->debugClient().addMessage( "angleError = %f", line_dir_error.abs() );

    if ( line_dir_error.abs() > 5.0
         && ( goal_line_point - self_pos ).r() > 5.0 )
    {
        const double current_position_dist = ( goal_line_point - self_pos ).r();

        if ( dist_from_goal < current_position_dist )
        {
            dist_from_goal = current_position_dist;
        }

        dist_from_goal = ( goal_line_point - self_pos ).r();

        dlog.addText( Logger::ROLE,
                      __FILE__":(getBasicPositionFromBall) position angle error big,"
                      " positioning has changed to projection point, "
                      "new dist_from_goal = %f", dist_from_goal );

#ifdef VISUAL_DEBUG
        const Vector2D r = goal_line_point
            + ( ball_pos - goal_line_point ).setLengthVector( dist_from_goal );

        agent->debugClient().addLine( goal_line_point, r );
        agent->debugClient().addLine( self_pos, r );
#endif
    }

    Vector2D result = goal_line_point
        + ( ball_pos - goal_line_point ).setLengthVector( dist_from_goal );

    if ( result.x < min_x )
    {
        result.assign( min_x, result.y );
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPositionFromBall) positioning target = [%f,%f]",
                  result.x, result.y );

    const Vector2D self_inertia = agent->world().self().inertiaFinalPoint();
    const Segment2D block_segment( ball_pos, goal_line_point );
    if ( block_segment.dist( self_inertia ) > 1.0 )
    {
        if ( self_inertia.x < ball_pos.x - 2.0 )
        {
            double block_y = line_m.getY( self_inertia.x );
            if ( block_y != Line2D::ERROR_VALUE )
            {
                result.assign( self_inertia.x, block_y );
                dlog.addText( Logger::ROLE,
                              __FILE__":(getBasicPositionFromBall) adjust on block lien = [%f,%f]",
                              result.x, result.y );
            }
        }
    }

    return result;
}


/*----------------------------------------------------------------------------------------------*/
/*!

 */

Vector2D
Bhv_SaviorPenaltyKick::getGoalLinePositioningTarget( PlayerAgent * agent,
                                                     const WorldModel & wm,
                                                     const double goal_x_dist,
                                                     const Vector2D & ball_pos,
                                                     const bool is_despair_situation ) const
{
    const ServerParam & SP = ServerParam::i();

    const double goal_line_x = SP.ourTeamGoalLineX();

    const double goal_half_width = SP.goalHalfWidth();
    const Vector2D goal_center = SP.ourTeamGoalPos();
    const Vector2D goal_post_plus( goal_center.x, +goal_half_width );
    const Vector2D goal_right_post( goal_center.x, -goal_half_width );

    const AngleDeg line_dir = getDirFromOurGoal( ball_pos );
    const Line2D line_m( ball_pos, line_dir );
    const Line2D goal_line( goal_post_plus, goal_right_post );
    const Line2D target_line( goal_post_plus + Vector2D( goal_x_dist, 0.0 ),
                              goal_right_post + Vector2D( goal_x_dist, 0.0 ) );

    if ( is_despair_situation )
    {
        const double target_x = std::max( wm.self().inertiaPoint( 1 ).x,
                                          goal_line_x );

        const Line2D positioniong_line( Vector2D( target_x, -1.0 ),
                                        Vector2D( target_x, +1.0 ) );

        if ( wm.ball().vel().r() < EPS )
        {
            return wm.ball().pos();
        }

        const Line2D ball_line( ball_pos, wm.ball().vel().th() );

        const Vector2D c = positioniong_line.intersection( ball_line );

        if ( c.isValid() )
        {
            return c;
        }
    }


    //
    // set base point to ball line and positioning line
    //
    Vector2D p = target_line.intersection( line_m );

    if ( ! p.isValid() )
    {
        double target_y;

        if ( ball_pos.absY() > SP.goalHalfWidth() )
        {
            target_y = sign( ball_pos.y )
                * std::min( ball_pos.absY(),
                            SP.goalHalfWidth() + 2.0 );
        }
        else
        {
            target_y = ball_pos.y * 0.8;
        }

        p = Vector2D( goal_line_x + goal_x_dist, target_y );
    }


    const int teammate_reach_cycle = wm.interceptTable()->teammateReachCycle();
    const int opponent_reach_cycle = wm.interceptTable()->opponentReachCycle();
    const int predict_step = std::min( opponent_reach_cycle, teammate_reach_cycle );

    const Vector2D predict_ball_pos = wm.ball().inertiaPoint( predict_step );

    dlog.addText( Logger::ROLE,
                  __FILE__": (getGoalLinePositioningTarget) predict_step = %d, "
                  "predict_ball_pos = [%f, %f]",
                  predict_step, predict_ball_pos.x, predict_ball_pos.y );

    const bool can_shoot_from_predict_ball_pos = opponentCanShootFrom( wm, predict_ball_pos );

    dlog.addText( Logger::ROLE,
                  __FILE__": (getGoalLinePositioningTarget) can_shoot_from_predict_ball_pos = %s",
                  ( can_shoot_from_predict_ball_pos? "true": "false" ) );


    //
    // shift to middle of goal if catchable
    //
    const double ball_dist = ShootSimulator::get_dist_from_our_near_goal_post( predict_ball_pos );
    //const int ball_step = static_cast< int >( std::ceil( ball_dist / SP.ballSpeedMax() ) + EPS );
    const int ball_step = SP.ballMoveStep( SP.ballSpeedMax(), ball_dist );

    const double s = sign( p.y );
    const double org_p_y_abs = std::fabs( p.y );

    for( double new_y_abs = 0.0; new_y_abs < org_p_y_abs; new_y_abs += 0.5 )
    {
        const Vector2D new_p( p.x, s * new_y_abs );

        const double self_dist
            = ShootSimulator::get_dist_from_our_near_goal_post( Vector2D( p.x, s * new_y_abs ) );
        const int self_step = wm.self().playerTypePtr()->cyclesToReachDistance( self_dist );

        dlog.addText( Logger::ROLE,
                      __FILE__": check back shift y_abs = %f, self_step = %d, ball_step = %d",
                      new_y_abs, self_step, ball_step );

        if ( self_step * 1.05 + 4 < ball_step )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": shift back to [%d, %d]",
                          p.x, p.y );

# ifdef VISUAL_DEBUG
            agent->debugClient().addCircle( p, 0.5 );

            agent->debugClient().addLine( new_p + Vector2D( -1.5, -1.5 ),
                                          new_p + Vector2D( +1.5, +1.5 ) );
            agent->debugClient().addLine( new_p + Vector2D( -1.5, +1.5 ),
                                          new_p + Vector2D( +1.5, -1.5 ) );

            agent->debugClient().addLine( p, new_p );

            if ( can_shoot_from_predict_ball_pos )
            {
                agent->debugClient().addLine( Vector2D( SP.ourTeamGoalLineX() - 5.0,
                                                        rcsc::sign( wm.ball().pos().y )
                                                        * SP.goalHalfWidth() ),
                                              Vector2D( SP.ourTeamGoalLineX() + 5.0,
                                                        rcsc::sign( wm.ball().pos().y )
                                                        * SP.goalHalfWidth() ) );
            }
# endif
            p = new_p;
            break;
        }
    }

    if ( p.absY() > SP.goalHalfWidth() + 2.0 )
    {
        p.assign( p.x, sign( p.y ) * ( SP.goalHalfWidth() + 2.0 ) );
    }

    return p;
}

/*---------------------------------------------------------------------------------*/
/*!

 */

AngleDeg
Bhv_SaviorPenaltyKick::getDirFromOurGoal( const Vector2D & pos )
{
    const ServerParam & SP = ServerParam::i();

    const double goal_half_width = SP.goalHalfWidth();

    const Vector2D goal_center = SP.ourTeamGoalPos();
    const Vector2D goal_post_plus( goal_center.x, +goal_half_width );
    const Vector2D goal_post_minus( goal_center.x, -goal_half_width );

    // return AngleDeg( ( (pos - goal_post_plus).th().degree()
    //                    + (pos - goal_post_minus).th().degree() ) / 2.0 );

    const double dist_to_plus_post = pos.dist( goal_post_plus );
    const double dist_to_minus_post = pos.dist( goal_post_minus );

    const double plus_rate = dist_to_plus_post / ( dist_to_plus_post + dist_to_minus_post );
    const double minus_rate = 1.0 - plus_rate;

    const AngleDeg plus_post_angle = ( goal_post_plus - pos ).th();
    const AngleDeg minus_post_angle = ( goal_post_minus - pos ).th();
    const double angle_diff = ( plus_post_angle - minus_post_angle ).abs();

    return plus_post_angle.isLeftOf( minus_post_angle )
        ? plus_post_angle + ( angle_diff * plus_rate ) + 180.0
        : minus_post_angle + ( angle_diff * minus_rate ) + 180.0;
}

/*---------------------------------------------------------------------------------*/
/*!

 */

double
Bhv_SaviorPenaltyKick::getTackleProbability( const Vector2D & body_relative_ball )
{
    const ServerParam & SP = ServerParam::i();

    double tackle_length;

    if ( body_relative_ball.x > 0.0 )
    {
        if ( SP.tackleDist() > EPS )
        {
            tackle_length = SP.tackleDist();
        }
        else
        {
            return 0.0;
        }
    }
    else
    {
        if ( SP.tackleBackDist() > EPS )
        {
            tackle_length = SP.tackleBackDist();
        }
        else
        {
            return 0.0;
        }
    }

    if ( SP.tackleWidth() < EPS )
    {
        return 0.0;
    }


    double prob = 1.0;

    // vertical dist penalty
    prob -= std::pow( body_relative_ball.absX() / tackle_length,
                      SP.tackleExponent() );

    // horizontal dist penalty
    prob -= std::pow( body_relative_ball.absY() / SP.tackleWidth(),
                      SP.tackleExponent() );

    // don't allow negative value by calculation error.
    return std::max( 0.0, prob );
}

/*---------------------------------------------------------------------------------*/
/*!

 */

Vector2D
Bhv_SaviorPenaltyKick::getSelfNextPosWithDash( const WorldModel & wm,
                                               const double dash_power )
{
    return wm.self().inertiaPoint( 1 )
        + Vector2D::polar2vector
        ( dash_power * wm.self().playerType().dashPowerRate(),
          wm.self().body() );
}

/*---------------------------------------------------------------------------------*/
/*!

 */

double
Bhv_SaviorPenaltyKick::getSelfNextTackleProbabilityWithDash( const WorldModel & wm,
                                                             const double dash_power )
{
    return getTackleProbability( ( wm.ball().inertiaPoint( 1 )
                                   - getSelfNextPosWithDash( wm, dash_power ) )
                                 .rotatedVector( - wm.self().body() ) );
}

/*---------------------------------------------------------------------------------*/
/*!

 */

double
Bhv_SaviorPenaltyKick::getSelfNextTackleProbabilityWithTurn( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();

    const Vector2D self_next = wm.self().pos() + wm.self().vel();
    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    const Vector2D ball_rel = ball_next - self_next;
    const double ball_dist = ball_rel.r();

    if ( ball_dist > SP.tackleDist()
         && ball_dist > SP.tackleWidth() )
    {
        return 0.0;
    }

    double max_turn = wm.self().playerType().effectiveTurn( SP.maxMoment(),
                                                            wm.self().vel().r() );
    AngleDeg ball_angle = ball_rel.th() - wm.self().body();
    ball_angle = std::max( 0.0, ball_angle.abs() - max_turn );

    return getTackleProbability( Vector2D::polar2vector( ball_dist, ball_angle ) );
}


/*---------------------------------------------------------------------------------*/
/*!

 */

bool
Bhv_SaviorPenaltyKick::opponentCanShootFrom( const WorldModel & wm,
                                             const Vector2D & pos,
                                             const long valid_teammate_threshold ) const
{
    return ShootSimulator::opponent_can_shoot_from( pos,
                                                    wm.getPlayers( new TeammatePlayerPredicate( wm ) ),
                                                    valid_teammate_threshold,
                                                    SHOOT_DIST_THRESHOLD,
                                                    SHOOT_ANGLE_THRESHOLD );
}

/*---------------------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SaviorPenaltyKick::doChaseBall( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();

    const int self_reach_cycle = agent->world().interceptTable()->selfReachCycle();
    const Vector2D intercept_point = agent->world().ball().inertiaPoint( self_reach_cycle );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doChaseBall): intercept point = [%f, %f]",
                  intercept_point.x, intercept_point.y );

    if ( intercept_point.x > SP.ourTeamGoalLineX() - 1.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doChaseBall): normal intercept" );

        if ( Body_Intercept().execute( agent ) )
        {
            agent->setNeckAction( new Neck_ChaseBall() );

            return true;
        }
    }


    //
    // go to goal line
    //
    const Vector2D predict_ball_pos = getFieldBoundPredictBallPos( agent->world(),
                                                                   self_reach_cycle,
                                                                   1.0 );

    const double target_error = std::max( SP.catchableArea(),
                                          SP.tackleDist() );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doChaseBall): "
                  "goal line intercept point = [%f, %f]",
                  predict_ball_pos.x, predict_ball_pos.y );

    if (  Body_SaviorGoToPoint( predict_ball_pos,
                                target_error,
                                SP.maxPower(),
                                true,
                                true,
                                true ).execute( agent ) )
    {
        agent->setNeckAction( new Neck_ChaseBall() );

        return true;
    }

    return false;
}

/*---------------------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SaviorPenaltyKick::getFieldBoundPredictBallPos( const WorldModel & wm,
                                                    const int predict_step,
                                                    const double shrink_offset ) const
{
    const double half_len = ServerParam::i().pitchHalfLength() - shrink_offset;
    const double half_wid = ServerParam::i().pitchHalfWidth() - shrink_offset;

    const Vector2D current_pos = wm.ball().pos();
    const Vector2D predict_pos = wm.ball().inertiaPoint( predict_step );

    const Rect2D pitch_rect = Rect2D::from_center( Vector2D( 0.0, 0.0 ),
                                                   half_len*2.0, half_wid*2.0 );

    if ( pitch_rect.contains( current_pos )
         && pitch_rect.contains( predict_pos ) )
    {
        return predict_pos;
    }

    Vector2D sol1, sol2;
    int n = pitch_rect.intersection( Segment2D( current_pos, predict_pos ),
                                     &sol1, &sol2 );
    if ( n == 0 )
    {
        return predict_pos;
    }
    else if ( n == 1 )
    {
        return sol1;
    }

    return Vector2D( bound( -half_len, current_pos.x, +half_len ),
                     bound( -half_wid, current_pos.y, +half_wid ) );
}
