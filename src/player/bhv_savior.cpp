// -*-c++-*-

/*!
  \file bhv_savior.cpp
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

#include "strategy.h"

#include "action_chain_holder.h"
#include "cooperative_action.h"

#include "bhv_savior.h"
#include "bhv_savior_penalty_kick.h"
#include "bhv_deflecting_tackle.h"
#include "bhv_tactical_tackle.h"
#include "bhv_clear_ball.h"

#include "bhv_chain_action.h"
#include "bhv_penalty_kick.h"

#include "body_savior_go_to_point.h"

#include "neck_chase_ball.h"

#include "shoot_simulator.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/player/penalty_kick_state.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/action/bhv_neck_body_to_ball.h>
#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_turn_to_angle.h>
#include <rcsc/action/body_turn_to_point.h>
#include <rcsc/action/body_stop_dash.h>
//#include <rcsc/action/body_clear_ball.h>
#include <rcsc/action/neck_turn_to_ball.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/view_synch.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/geom/angle_deg.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/math_util.h>

#include <rcsc/player/penalty_kick_state.h>

#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>


#define VISUAL_DEBUG
#define DEBUG_PRINT
#define DEBUG_PRINT_DIST_THR

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


// if you don't want to use, please comment out it.
#define USE_BHV_SAVIOR_PENALTY_KICK



/*
 * revert options, if you'd like to rollback behavior, turn enable some of
 * following options
 */
// #define REVERT_TO_CONSERVATIVE_GOAL_LINE_POSITIONING_X


using namespace rcsc;

namespace {
static const double SHOOT_ANGLE_THRESHOLD = 10.0; // degree
static const double SHOOT_DIST_THRESHOLD = 40.0; // meter
static const int SHOOT_BLOCK_TEAMMATE_POS_COUNT_THRESHOLD = 20; // steps


bool
is_despair_situation( const WorldModel & wm )
{
    const int self_step = wm.interceptTable()->selfReachStep();
    const Vector2D self_intercept_pos = wm.ball().inertiaPoint( self_step );

    return ( self_intercept_pos.x < -ServerParam::i().pitchHalfLength()
             && ShootSimulator::is_ball_moving_to_our_goal( wm.ball().pos(),
                                                            wm.ball().vel(),
                                                            1.0 ) );
}

}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": (Bhv_Savior::execute)" );

    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

#ifdef VISUAL_DEBUG
    {
        const int opp_min = wm.interceptTable()->opponentReachCycle();
        const Vector2D ball_pos = wm.ball().inertiaPoint( opp_min );
        if ( opponentCanShootFrom( wm, ball_pos, 20 ) )
        {
            agent->debugClient().addLine( ball_pos,
                                          Vector2D( SP.ourTeamGoalLineX(),
                                                    -SP.goalHalfWidth() ) );
            agent->debugClient().addLine( ball_pos,
                                          Vector2D( SP.ourTeamGoalLineX(),
                                                    +SP.goalHalfWidth() ) );
        }
    }
#endif

    switch ( wm.gameMode().type() ) {
    case GameMode::PlayOn:
    case GameMode::KickIn_:
    case GameMode::OffSide_:
    case GameMode::CornerKick_:
    case GameMode::KickOff_:
        return doPlayOnMove( agent );
        break;

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
                return doPlayOnMove( agent );
            }
        }
        break;

    case GameMode::BeforeKickOff:
    case GameMode::AfterGoal_:
        return doKickOffMove( agent, 1.0 );
        break;

    case GameMode::PenaltyTaken_:
        return doPenaltyKick( agent );
        break;

    default:
        return doPlayOnMove( agent );
        break;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::doPlayOnMove( PlayerAgent * agent )
{
#ifdef DEBUG_PRINT
    {
        const WorldModel & wm = agent->world();

        const int self_reach_cycle = wm.interceptTable()->selfReachCycle();
        const int teammate_reach_cycle = wm.interceptTable()->teammateReachCycle();
        const int opponent_reach_cycle = wm.interceptTable()->opponentReachCycle();
        const int predict_step = std::min( opponent_reach_cycle,
                                           std::min( teammate_reach_cycle,
                                                     self_reach_cycle ) );
        const Vector2D self_intercept_point = wm.ball().inertiaPoint( self_reach_cycle );
        const Vector2D ball_pos = getFieldBoundPredictBallPos( wm, predict_step, 0.5 );

        dlog.addText( Logger::ROLE,
                      __FILE__":(doPlayOnMove) self. pos=[%.2f, %.2f], body_dir=%.0f",
                      wm.self().pos().x, wm.self().pos().y, wm.self().body().degree() );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPlayOnMove) reach cycle. self=%d(%.1f %.1f), teammate=%d, opponent=%d",
                      self_reach_cycle, self_intercept_point.x, self_intercept_point.y,
                      teammate_reach_cycle, opponent_reach_cycle );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPlayOnMove) current ball. pos=[%.2f, %.2f](%d) vel=[%.2f, %.2f](%d)",
                      wm.ball().pos().x, wm.ball().pos().y, wm.ball().posCount(),
                      wm.ball().vel().x, wm.ball().vel().y, wm.ball().velCount() );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPlayOnMove) predict step=%d bpos=[%.1f, %.1f]",
                      predict_step, ball_pos.x, ball_pos.y );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPlayOnMove) tackle_prob=%.3f, catch_prob=%.3f",
                      wm.self().tackleProbability(), wm.self().catchProbability() );

        agent->debugClient().addMessage( "ball dist=%f", wm.ball().distFromSelf() );

        if ( ShootSimulator::is_ball_moving_to_our_goal( wm.ball().pos(),
                                                         wm.ball().vel(),
                                                         1.0 ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPlayOnMove) is_shoot_ball = true" );
            dlog.addRect( Logger::ROLE,
                          ball_pos.x - 0.2, ball_pos.y - 0.2, 0.4, 0.4, "#F00" );
        }
        else
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPlayOnMove) is_shoot_ball = false" );
        }
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPlayOnMove) is_despair_situation = %s",
                      ( is_despair_situation( wm ) ? "true" : "false" ) );
    }
#endif

    //
    // set default neck and view action
    //
    setDefaultNeckAndViewAction( agent );


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
    if ( doKickIfPossible( agent ) )
    {
        return true;
    }

    //
    // tackle
    //
    if ( doTackleIfNecessary( agent ) )
    {
        return true;
    }

    //
    // chase ball
    //
    if ( doChaseBallIfNessary( agent ) )
    {
        return true;
    }

    //
    // check ball
    //
    if ( doFindBallIfNecessary( agent ) )
    {
        return true;
    }

    //
    // positioning
    //
    if ( doPositioning( agent ) )
    {
        return true;
    }

    //
    // default behavior
    //
    agent->debugClient().addMessage( "Savior:NoAction" );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doPlayOnMove) no action" );
    agent->doTurn( 0.0 );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::doKickOffMove( PlayerAgent * agent,
                           const double max_error )
{
    const WorldModel & wm = agent->world();
    const Vector2D target_point( ServerParam::i().ourTeamGoalLineX(), 0.0 );

    if ( wm.self().pos().dist2( target_point ) < std::pow( max_error, 2 ) )
    {
        Bhv_ScanField().execute( agent );
    }
    else
    {
        agent->doMove( target_point.x, target_point.y );
    }

    agent->setNeckAction( new Neck_ScanField );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::doPenaltyKick( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const PenaltyKickState * pk_state = wm.penaltyKickState();

    //
    // kicker decision
    //
    if ( pk_state->isKickTaker( wm.ourSide(), wm.self().unum() ) )
    {
        Bhv_PenaltyKick().execute( agent );
        return true;
    }

    //
    // goalie decision
    //
#ifdef USE_BHV_SAVIOR_PENALTY_KICK
    Bhv_SaviorPenaltyKick().execute( agent );
#else
    if ( pk_state->currentTakerSide() == wm.ourSide() )
    {
        Bhv_ScanField().execute( agent );
    }
    else
    {
        if ( doPlayOnMove( agent ) )
        {
            agent->setNeckAction( new Neck_ChaseBall );
            agent->setViewAction( new View_Synch );
        }
    }
#endif
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_Savior::setDefaultNeckAndViewAction( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.gameMode().isPenaltyKickMode() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(setDefaultNeckAndViewAction) penalty kick mode, neck chase ball" );
        agent->setNeckAction( new Neck_ChaseBall() );
        agent->setViewAction( new View_Synch() );
    }
    else if ( wm.ball().pos().x > 0.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(setDefaultNeckAndViewAction) ball far, neck scan field" );
        agent->setNeckAction( new Neck_ScanField() );
    }
    else if ( wm.ball().seenPosCount() == 0 )
    {
        const int predict_step = std::min( wm.interceptTable()->opponentReachStep(),
                                           std::min( wm.interceptTable()->teammateReachStep(),
                                                     wm.interceptTable()->selfReachStep() ) );

        dlog.addText( Logger::ROLE,
                      __FILE__":(setDefaultNeckAndViewAction) ball seen_pos_count=0, predict_step=%d",
                      predict_step );

        if ( predict_step >= 10 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(setDefaultNeckAndViewAction) neck scan field" );
            agent->setNeckAction( new Neck_ScanField() );
        }
        else if ( predict_step > 3 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(setDefaultNeckAndViewAction) neck turn to ball" );
            agent->setNeckAction( new Neck_TurnToBall() );
        }
        else
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(setDefaultNeckAndViewAction) neck chase ball" );
            agent->setNeckAction( new Neck_ChaseBall() );
            agent->setViewAction( new View_Synch() );
        }
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(setDefaultNeckAndViewAction) defaule neck chase ball" );
        agent->setNeckAction( new Neck_ChaseBall() );
        agent->setViewAction( new View_Synch() );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::doPositioning( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const int self_reach_cycle = wm.interceptTable()->selfReachCycle();
    const int opponent_reach_cycle = wm.interceptTable()->opponentReachCycle();
    const int teammate_reach_cycle = wm.interceptTable()->teammateReachCycle();
    const int predict_step = std::min( opponent_reach_cycle,
                                       std::min( teammate_reach_cycle,
                                                 self_reach_cycle ) );
    const Vector2D ball_pos = getFieldBoundPredictBallPos( wm, predict_step, 0.5 );

    dlog.addText( Logger::ROLE,
                  __FILE__":(doPositioning) start" );

    double goal_line_positioning_y_max_position_error = 0.3;
    double dash_power = SP.maxDashPower();

    const Vector2D best_position = getBasicPosition( agent );
    // const Vector2D best_position = ( wm.gameMode().type() == GameMode::PenaltyTaken_
    //                                  ? getBasicPosition( agent )
    //                                  : Strategy::i().getPosition( wm.self().unum() ) );
    const double dist_diff = wm.self().pos().dist( best_position );

#ifdef VISUAL_DEBUG
    if ( M_emergent_advance )
    {
        const Vector2D hat_base_vec = ( best_position - wm.self().pos() ).normalizedVector();

        // draw arrow
        agent->debugClient().addLine( wm.self().pos() + hat_base_vec * 3.0,
                                      wm.self().pos() + hat_base_vec * 3.0
                                      + Vector2D::polar2vector
                                      ( 1.5, hat_base_vec.th() + 150.0 ) );
        agent->debugClient().addLine( wm.self().pos() + hat_base_vec * 3.0,
                                      wm.self().pos() + hat_base_vec * 3.0
                                      + Vector2D::polar2vector
                                      ( 1.5, hat_base_vec.th() - 150.0 ) );
        agent->debugClient().addLine( wm.self().pos() + hat_base_vec * 4.0,
                                      wm.self().pos() + hat_base_vec * 4.0
                                      + Vector2D::polar2vector
                                      ( 1.5, hat_base_vec.th() + 150.0 ) );
        agent->debugClient().addLine( wm.self().pos() + hat_base_vec * 4.0,
                                      wm.self().pos() + hat_base_vec * 4.0
                                      + Vector2D::polar2vector
                                      ( 1.5, hat_base_vec.th() - 150.0 ) );
    }

    if ( M_goal_line_positioning )
    {
        agent->debugClient().addLine( Vector2D( SP.ourTeamGoalLineX() - 1.0, -SP.goalHalfWidth() ),
                                      Vector2D( SP.ourTeamGoalLineX() - 1.0, +SP.goalHalfWidth() ) );
    }

    if ( M_dangerous ) agent->debugClient().addMessage( "Dangerous" );
    if ( M_aggressive ) agent->debugClient().addMessage( "Aggressive" );
    if ( M_emergent_advance ) agent->debugClient().addMessage( "Emergent" );
    if ( M_goal_line_positioning ) agent->debugClient().addMessage( "GLine" );

    agent->debugClient().addLine( wm.self().pos(), best_position );
#endif
#ifdef DEBUG_PRINT

    dlog.addText( Logger::ROLE,
                  __FILE__": dangerous[%c] aggressive[%c] emergent[%c] gline[%c]",
                  M_dangerous ? 'x' : '_',
                  M_aggressive ? 'x' : '_',
                  M_emergent_advance ? 'x' : '_',
                  M_goal_line_positioning ? 'x' : '_' );

    if ( M_goal_line_positioning )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) POSITIONING_MODE = GOAL_LINE_POSITIONING" );
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) POSITIONING_MODE = NOT_GOAL_LINE_POSITIONING" );
    }

#endif

    //
    // update dash power
    //
    if ( wm.gameMode().type() != GameMode::PenaltyTaken_
         && dist_diff < 5.0
         && wm.ball().distFromSelf() >= 30.0
         && wm.ball().pos().x >= -20.0 )
    {
        dash_power = SP.maxDashPower() * 0.7;

        if ( teammate_reach_cycle < opponent_reach_cycle
             || wm.ball().pos().x >= 10.0 )
        {
            dash_power = SP.maxDashPower() * 0.3;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [power] ball is far(1). power=%.1f",
                          dash_power );
#endif
        }
#ifdef DEBUG_PRINT
        else
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [power] ball is far(2). power=%.1f",
                          dash_power );
        }
#endif
#ifdef VISUAL_DEBUG
        agent->debugClient().addMessage( "BallFar" );
        // agent->debugClient().addLine( wm.self().pos() + Vector2D( -1.0, +3.0 ),
        //                               wm.self().pos() + Vector2D( +1.0, +3.0 ) );
        // agent->debugClient().addLine( wm.self().pos() + Vector2D( -1.0, -3.0 ),
        //                               wm.self().pos() + Vector2D( +1.0, -3.0 ) );
#endif
    }

    //
    // update distance threshold
    //

    double max_position_error = 0.7;
#ifdef DEBUG_PRINT_DIST_THR
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPositioning) [dist_thr] base dist_thr=%.2f",
                  max_position_error );
#endif

    if ( M_aggressive )
    {
        if ( wm.ball().distFromSelf() >= 30.0 )
        {
            max_position_error = bound( 2.0, 2.0 + ( wm.ball().distFromSelf() - 30.0 ) / 20.0, 3.0 );
#ifdef DEBUG_PRINT_DIST_THR
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr] aggressive, ball far. dist_thr=%.2f",
                          max_position_error );
#endif
        }
    }

    if ( M_emergent_advance )
    {
        if ( dist_diff >= 10.0 )
        {
            // for speedy movement
            max_position_error = 1.9;
#ifdef DEBUG_PRINT_DIST_THR
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr] emergent speedy(1). dist_thr=%.2f",
                          max_position_error );
#endif
        }
        else if ( dist_diff >= 5.0 )
        {
            // for speedy movement
            max_position_error = 1.0;
#ifdef DEBUG_PRINT_DIST_THR
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr] emergent speedy(2). dist_thr=%.2f",
                          max_position_error );
#endif
        }
#ifdef DEBUG_PRINT_DIST_THR
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) [dist_thr] emergent_advance, dist_diff=%.2f, dist_thr=%.2f",
                      dist_diff, max_position_error );
#endif
    }
    else if ( M_goal_line_positioning )
    {
        if ( wm.gameMode().type() == GameMode::PenaltyTaken_ )
        {
            max_position_error = 0.7;
            goal_line_positioning_y_max_position_error = 0.5;
#ifdef DEBUG_PRINT_DIST_THR
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr] penalty taken. dist_thr=%.2f max_y_thr=%.2f",
                          max_position_error, goal_line_positioning_y_max_position_error );
#endif
        }
        else
        {
            const double x_diff = std::fabs( wm.self().pos().x - best_position.x );
            const double y_diff = std::fabs( wm.self().pos().y - best_position.y );
#ifdef DEBUG_PRINT_DIST_THR
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr] gline. x_diff=%.2f",
                          x_diff );
#endif
            if ( x_diff > 5.0 || y_diff > 3.0 )
            {
                max_position_error = 2.0;
                goal_line_positioning_y_max_position_error = 1.5;
#ifdef DEBUG_PRINT_DIST_THR
                dlog.addText( Logger::ROLE,
                              __FILE__":(doPositioning) [dist_thr] gline. dist_thr=%.2f max_y_thr=%.2f",
                              max_position_error, goal_line_positioning_y_max_position_error );
#endif
            }

            if ( ball_pos.x < -30.0
                 && std::fabs( wm.self().pos().y - best_position.y ) > 4.0
                 && std::fabs( wm.self().pos().x - best_position.x ) < 5.0 )
            {
                max_position_error = 2.0;
                goal_line_positioning_y_max_position_error = 1.5;
#ifdef DEBUG_PRINT_DIST_THR
                dlog.addText( Logger::ROLE,
                              __FILE__":(doPositioning) [dist_thr] gline, too far x."
                              " dist_thr=%.2f, max_y_thr=%.2f",
                              max_position_error, goal_line_positioning_y_max_position_error );
#endif
            }
            else if ( ball_pos.x > -10.0
                      && wm.ball().pos().x > +10.0
                      && wm.ourDefenseLineX() > -20.0
                      && std::fabs( wm.self().pos().x - best_position.x ) < 5.0 )
            {
                // safety, save stamina
                dash_power = SP.maxDashPower() * 0.5;
                max_position_error = 2.0;
                goal_line_positioning_y_max_position_error = 1.5;
#ifdef DEBUG_PRINT_DIST_THR
                dlog.addText( Logger::ROLE,
                              __FILE__":(doPositioning) [dist_thr][power] gline, save stamina(1)."
                              " dist_thr=%.2f max_y_thr=%.2f power=%.1f",
                              max_position_error, goal_line_positioning_y_max_position_error, dash_power );
#endif
            }
            else if ( ball_pos.x > -20.0
                      && wm.ball().pos().x > +20.0
                      && wm.ourDefenseLineX() > -25.0
                      && teammate_reach_cycle < opponent_reach_cycle
                      && std::fabs( wm.self().pos().x - best_position.x ) < 5.0 )
            {
                // safety, save stamina
                dash_power = SP.maxDashPower() * 0.6;
                max_position_error = 1.8;
                goal_line_positioning_y_max_position_error = 1.5;
#ifdef DEBUG_PRINT_DIST_THR
                dlog.addText( Logger::ROLE,
                              __FILE__":(doPositioning) [dist_thr][power] gline, save stamina(2).",
                              " dist_thr=%.2f max_y_thr=%.2f power=%.1f",
                              max_position_error, goal_line_positioning_y_max_position_error, dash_power );
#endif
            }
        }
    }
    else if ( M_dangerous )
    {
        if ( dist_diff >= 10.0 )
        {
            // for speedy movement
            max_position_error = 1.9;
#ifdef DEBUG_PRINT_DIST_THR
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr] danger(1) dist_diff=%.1f dist_thr=%.2f",
                          dist_diff, max_position_error );
#endif
        }
        else if ( dist_diff >= 5.0 )
        {
            // for speedy movement
            max_position_error = 1.0;
#ifdef DEBUG_PRINT_DIST_THR
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr] danger(2) dist_diff=%.1f dist_thr=%.2f",
                          dist_diff, max_position_error );
#endif
        }
        else
        {
            max_position_error = 0.1;
#ifdef DEBUG_PRINT_DIST_THR
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr] danger(3) dist_diff=%.1f dist_thr=%.2f",
                          dist_diff, max_position_error );
#endif
        }
    }
    else
    {
        if ( dist_diff >= 10.0 )
        {
            // for speedy movement
            max_position_error = 1.5;
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr] normal(1). dist_diff=%.1f dist_thr=%.2f",
                          dist_diff, max_position_error );
        }
        else if ( dist_diff >= 5.0 )
        {
            // for speedy movement
            max_position_error = 1.0;
#ifdef DEBUG_PRINT_DIST_THR
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr] normal(2). dist_diff=%.1f dist_thr=%.2f",
                          dist_diff, max_position_error );
#endif
        }
        else
        {
            // max_position_error = std::max( 0.15, wm.ball().pos().dist( best_position ) * 0.08 );
#ifdef DEBUG_PRINT_DIST_THR
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr] normal(0), dist_diff=%.2f dist_thr=%.2f",
                          dist_diff, max_position_error );
#endif
        }
    }

    if ( is_despair_situation( wm ) )
    {
        if ( max_position_error < 1.5 )
        {
            max_position_error = 1.5;
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr] despair_situation. dist_thr=%.2f",
                          max_position_error );
        }
    }

    dash_power = wm.self().getSafetyDashPower( dash_power );

    //
    // update distance threshold & dash power based on play mode
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
            //max_position_error = 5.0;
            max_position_error = std::max( 0.5, wm.ball().distFromSelf() * 0.065 );
            goal_line_positioning_y_max_position_error = 1.0;

            if ( wm.ball().pos().x > ServerParam::i().ourPenaltyAreaLineX() + 10.0 )
            {
                dash_power = std::min( dash_power, wm.self().playerType().staminaIncMax() );
            }

            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr][power] our setplay."
                          " dist_thr=%.2f max_y_thr=%.2f power=%.1f",
                          max_position_error,
                          goal_line_positioning_y_max_position_error,
                          dash_power );
        }
    }

#ifdef DEBUG_PRINT_DIST_THR
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPositioning) positioning target = [%.2f, %.2f], ",
                  best_position.x, best_position.y );
    dlog.addText( Logger::ROLE,
                  "__current_position_diff=%.2f",
                  dist_diff );
    dlog.addText( Logger::ROLE,
                  "__max_position_error=%.2f, ",
                  max_position_error );
    dlog.addText( Logger::ROLE,
                  "__goal_line_positioning_y_max_position_error=%.2f",
                  goal_line_positioning_y_max_position_error );
    dlog.addText( Logger::ROLE,
                  "__dash_power=%.2f",
                  goal_line_positioning_y_max_position_error, dash_power );
#endif

    //
    // prepare stop
    //
    if ( opponent_reach_cycle == 0
         && dist_diff < 0.5
         && ( wm.self().pos() + wm.self().vel() ).dist2( best_position ) > std::pow( 0.5, 2 )
         && wm.self().pos().dist2( wm.ball().pos() ) < std::pow( 10.0, 2 ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) try prepare stop" );
        if ( Body_StopDash( true ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:PrepareStop" );
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) prepare stop" );
            return true;
        }
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) failed prepare stop" );
    }
#ifdef DEBUG_PRINT
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) do not prepare stop" );
    }
#endif

    if ( M_goal_line_positioning
         && ! is_despair_situation( wm ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) try normal doGoalLinePositioning" );

        if ( doGoalLinePositioning( agent ,
                                    best_position,
                                    2.0,
                                    max_position_error,
                                    goal_line_positioning_y_max_position_error,
                                    dash_power ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) done normal doGoalLinePositioning" );
            return true;
        }
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) try normal positioning" );
        if ( doGoToPoint( agent,
                          best_position,
                          max_position_error,
                          dash_power,
                          true, // back dash
                          false )  // force back dash
             )
        {
            agent->debugClient().addMessage( "Savior:Positioning" );
#ifdef VISUAL_DEBUG
            agent->debugClient().setTarget( best_position );
            agent->debugClient().addCircle( best_position, max_position_error );
#endif
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) done normal positioning" );
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) go to (%.2f %.2f) dist_thr=%.3f dash_power=%.1f",
                          best_position.x, best_position.y,
                          max_position_error,
                          dash_power );
            return true;
        }

        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) failed normal positioning" );
    }


    //
    // emergency stop
    //
    if ( opponent_reach_cycle <= 1
         && wm.self().pos().dist( wm.ball().pos() ) < 10.0
         && wm.self().vel().r() >= 0.05 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) try emergent stop" );
        if ( Body_StopDash( true ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:EmergemcyStop" );
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) done emergent stop" );
            return true;
        }
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) failed emergent stop" );
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) no emergent stop" );
    }


    //
    // go to position with minimum error
    //
    const PlayerObject * first_opponent = wm.interceptTable()->fastestOpponent();

    if ( opponent_reach_cycle <= 3
         && first_opponent
         && first_opponent->distFromSelf() <= 5.0 )
    {
        if ( M_goal_line_positioning
             && ! is_despair_situation( wm ) )
        {
            const double dist_thr = ( wm.gameMode().type() == GameMode::PlayOn
                                      || wm.gameMode().type() == GameMode::PenaltyTaken_
                                      ? 0.3
                                      : std::max( 0.3, wm.ball().distFromSelf() * 0.1 ) );
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) try doGoalLinePositioning with minimum error" );
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) [dist_thr] %.3f", dist_thr );

            if ( doGoalLinePositioning( agent,
                                        best_position,
                                        2.0,
                                        dist_thr,
                                        dist_thr,
                                        dash_power ) )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__":(doPositioning) done doGoalLinePositioning with minimum error" );
                return true;
            }

            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) failed doGoalLinePositioning with minimum error" );
        }
        else
        {
            const double dist_thr = ( wm.gameMode().type() == GameMode::PlayOn
                                      || wm.gameMode().type() == GameMode::PenaltyTaken_
                                      ? 0.3
                                      : std::max( 0.3, wm.ball().distFromSelf() * 0.1 ) );

            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) try doGoToPoint minimum error [dist_thr] %.3f", dist_thr );

            if ( doGoToPoint( agent,
                              best_position,
                              dist_thr,
                              dash_power,
                              true, // back dash
                              false ) // force back dash
                 )
            {
                agent->debugClient().addMessage( "Savior:TunePositioning" );
#ifdef VISUAL_DEBUG
                agent->debugClient().setTarget( best_position );
                agent->debugClient().addCircle( best_position, 0.3 );
#endif
                dlog.addText( Logger::ROLE,
                              __FILE__":(doPositioning) done doGoToPoint minimum error [dist_thr] %.3f",
                              dist_thr );
                return true;
            }

            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) failed doGoToPoint minimum error [dist_thr] %.3f", dist_thr );
        }
    }

    agent->debugClient().setTarget( best_position );
    agent->debugClient().addCircle( best_position, max_position_error );

    //
    // stop
    //
    if ( wm.self().vel().rotatedVector( -wm.self().body() ).absX() >= 0.01 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) try stop dash" );
        if ( Body_StopDash( true ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:Stop" );
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) done stop dist_thr=%.3f", max_position_error );
            return true;
        }
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) failed stop dash" );
    }

    //
    // turn body angle against ball
    //
    const Vector2D final_body_pos = wm.self().inertiaFinalPoint();

    AngleDeg target_body_angle = 0.0;

// #ifdef PENALTY_SHOOTOUT_BLOCK_IN_PENALTY_AREA
//     const double dist_thr = 12.0;
// #else
//     const double dist_thr = 5.0;
// #endif

    // if ( M_goal_line_positioning
    //      || ( ( wm.kickableOpponent() || wm.kickableTeammate() )
    //           && ball_pos.dist( final_body_pos ) <= dist_thr ) )
    // {
        if ( M_goal_line_positioning )
        {
            target_body_angle = sign( final_body_pos.y ) * 90.0;
        }
        else
        {
            AngleDeg diff_body_angle = 0.0;

            if ( final_body_pos.y > 0.0 )
            {
                diff_body_angle = + 90.0;
            }
            else
            {
                diff_body_angle = - 90.0;
            }

            if ( final_body_pos.x <= -45.0 )
            {
                diff_body_angle *= -1.0;
            }

            target_body_angle = ( ball_pos - final_body_pos ).th() + diff_body_angle;
        }

        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) target body angle = %.2f."
                      " final_body_pos = (%.2f %.2f)",
                      target_body_angle.degree(),
                      final_body_pos.x, final_body_pos.y );
    // }

    if ( ( target_body_angle - wm.self().body() ).abs() > 10.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) try turn to angle %.1f",
                      target_body_angle.degree() );
        if ( Body_TurnToAngle( target_body_angle ).execute( agent ) )
        {
            agent->debugClient().addMessage( "Savior:TurnTo%.0f",
                                             target_body_angle.degree() );
            dlog.addText( Logger::ROLE,
                          __FILE__":(doPositioning) done turn to angle %.1f",
                          target_body_angle.degree() );
            return true;
        }
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) failed turn to angle %.1f",
                          target_body_angle.degree() );
    }


#if ! defined( PENALTY_SHOOTOUT_BLOCK_IN_PENALTY_AREA ) && ! defined( PENALTY_SHOOTOUT_GOAL_PARALLEL_POSITIONING )
    //
    // side chase
    //
    if ( wm.gameMode().type() == GameMode::PenaltyTaken_ )
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

        agent->debugClient().addMessage( "Savior:SideDash" );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) penalty kick mode, side chase" );

        agent->doDash( SP.maxDashPower(), dash_dir );
        return true;
    }
#endif

    //
    // turn body with minimum error
    //
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPositioning) try adjust turn to angle %.1f",
                  target_body_angle.degree() );
    if ( Body_TurnToAngle( target_body_angle ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Savior:AdjustTurnTo%.0f",
                                         target_body_angle.degree() );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPositioning) adjust turn to angle %.1f",
                      target_body_angle.degree() );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doPositioning) failed all" );

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::doCatchIfPossible( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    const double MAX_SELF_BALL_ERROR = 0.5; // meter

    const bool can_catch_the_ball= ( wm.self().goalie()
                                     && wm.lastKickerSide() != wm.ourSide() // not back pass
                                     && ( wm.gameMode().type() == GameMode::PlayOn
                                          || wm.gameMode().type() == GameMode::PenaltyTaken_ )
                                     // not in catch ban cycle
                                     && wm.time().cycle() >= agent->effector().getCatchTime().cycle() + SP.catchBanCycle()
                                     // ball is in penalty area
                                     && ( wm.ball().pos().x < ( SP.ourPenaltyAreaLineX()
                                                                + SP.ballSize() * 2
                                                                - MAX_SELF_BALL_ERROR )
                                          && wm.ball().pos().absY()  < ( SP.penaltyAreaHalfWidth()
                                                                         + SP.ballSize() * 2
                                                                         - MAX_SELF_BALL_ERROR ) )
                                     && wm.self().catchProbability() > 0.99 );

    // if catchable situation
    if ( can_catch_the_ball
         && ( ! wm.kickableTeammate() || wm.kickableOpponent() ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doCatchIfPossible) doCatch prob=%.3f",
                      wm.self().catchProbability() );
        agent->setNeckAction( new Neck_TurnToBall() );
        return agent->doCatch();
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::doTackleIfNecessary( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    double tackle_prob_threshold = 0.8;

    if ( is_despair_situation( wm ) )
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

        const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
        const bool next_step_goal = ( ball_next.x < SP.ourTeamGoalLineX() );

        if ( ! next_step_goal )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) next step, ball is in goal,"
                          " ball = [%.2f, %.2f], ball rand = %f",
                          ball_next.x, ball_next.y,
                          SP.ballRand() );

            const double next_tackle_prob_forward = getSelfNextTackleProbabilityWithDash( wm, + SP.maxDashPower() );
            const double next_tackle_prob_backword = getSelfNextTackleProbabilityWithDash( wm, SP.minDashPower() );

            dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) next_tackle_prob_forward = %.3f",
                          next_tackle_prob_forward );
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) next_tackle_prob_backword = %.3f",
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
            const double next_tackle_prob_with_turn = getSelfNextTackleProbabilityWithTurn( wm );

            dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) next_tackle_prob_with_turn = %.3f",
                          next_tackle_prob_with_turn );
            if ( next_tackle_prob_with_turn > wm.self().tackleProbability() )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__"(doTackleIfNecessary) turn to next ball position for tackling" );
                Body_TurnToPoint( ball_next, 1 ).execute( agent );
                return true;
            }
        }

        dlog.addText( Logger::ROLE,
                      __FILE__":(doTackleIfNecessary) despair situation. try deflecting tackle" );

        if ( Bhv_DeflectingTackle( tackle_prob_threshold, true ).execute( agent ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) despair situation. done deflecting tackle" );
            agent->debugClient().addMessage( "Savior:DeflectingTackle(%.3f)",
                                             wm.self().tackleProbability() );
            return true;
        }
    }

    if ( wm.kickableOpponent()
         && ! wm.kickableTeammate()
         && wm.ball().pos().dist( ServerParam::i().ourTeamGoalPos() ) < 13.0
         && wm.self().tackleProbability() > 0.85 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doTackleIfNecessary) exist kickable opponent" );
        if ( Bhv_TacticalTackle().execute( agent ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) try tackle" );
            agent->debugClient().addMessage( "Savior:Tackle" );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::doChaseBallIfNessary( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const Rect2D shrinked_penalty_area( Vector2D( SP.ourTeamGoalLineX(),
                                                  - ( SP.penaltyAreaHalfWidth() - 1.0 ) ),
                                        Size2D( SP.penaltyAreaLength() - 1.0,
                                                ( SP.penaltyAreaHalfWidth() - 1.0 ) * 2.0 ) );
    const int self_reach_cycle = wm.interceptTable()->selfReachStep();
    const int teammate_reach_cycle = wm.interceptTable()->teammateReachStep();
    const int opponent_reach_cycle =  wm.interceptTable()->opponentReachStep();
    const Vector2D self_intercept_point = wm.ball().inertiaPoint( self_reach_cycle );

    if ( is_despair_situation( wm ) )
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

        if ( doChaseBall( agent ) )
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
                       && self_reach_cycle <= teammate_reach_cycle + 3 ) ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doChaseBallIfNecessary) normal chase." );
            agent->debugClient().addMessage( "Savior:Chase" );

            if ( doChaseBall( agent ) )
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
            agent->debugClient().addMessage( "Savior:AggressiveChase" );

            if ( doChaseBall( agent ) )
            {
                return true;
            }
        }
#endif
    }


    if ( wm.gameMode().type() == GameMode::PenaltyTaken_
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

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::doFindBallIfNecessary( PlayerAgent * agent )
{
    if ( agent->effector().queuedNextViewWidth() == ViewWidth::WIDE )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doFindBallIfNecessary) wide view" );
        return false;
    }

    const WorldModel & wm = agent->world();

    const int opponent_reach_cycle = wm.interceptTable()->opponentReachStep();

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
                      __FILE__":(doFindBallIfNecessary) find ball" );
        Bhv_NeckBodyToBall().execute( agent );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doFindBallIfNecessary) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::doGoToPoint( PlayerAgent * agent,
                         const Vector2D & target_point,
                         const double max_error,
                         const double max_power,
                         const bool use_back_dash,
                         const bool force_back_dash,
                         const bool emergency_mode,
                         const bool look_ball )
{
    return Body_SaviorGoToPoint( target_point,
                                 max_error,
                                 max_power,
                                 use_back_dash,
                                 force_back_dash,
                                 emergency_mode,
                                 look_ball ).execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::doGoalLinePositioning( PlayerAgent * agent,
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

    dlog.addText( Logger::ROLE,
                  __FILE__":(doGoalLinePositioning): target = [%.2f, %.2f], ",
                  target_position.x, target_position.y );
    dlog.addText( Logger::ROLE,
                  "__ low_priority_x_error=%.2f "
                  "max_x_error=%.2f "
                  "max_y_error=%.2f "
                  "dash_power=%.2f",
                  low_priority_x_position_error,
                  max_x_position_error, max_y_position_error,
                  dash_power );

    if ( x_diff < max_x_position_error
         && y_diff < max_y_position_error )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGoalLinePositioning) near target, return false" );

        return false;
    }

    //Vector2D p = target_position;
    const bool use_back_dash = ( agent->world().ball().pos().x <= -20.0 );

    const bool x_near = ( x_diff < max_x_position_error );
    const bool y_near = ( y_diff < max_y_position_error );


#ifdef VISUAL_DEBUG
    agent->debugClient().addRectangle( Rect2D::from_center( target_position,
                                                            max_x_position_error * 2.0,
                                                            max_y_position_error * 2.0 ) );
#endif

    dlog.addText( Logger::ROLE,
                  "__ dist=%.3f x_diff=%.3f y_diff=%.3f x_near=%s y_near=%s",
                  dist,
                  x_diff, y_diff,
                  ( x_near ? "true" : "false" ),
                  ( y_near ? "true" : "false" ) );
    dlog.addText( Logger::ROLE,
                  "__ use_back_dash=%s",
                  ( use_back_dash ? "true" : "false" ) );

    if ( x_diff > low_priority_x_position_error )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGoalLinePositioning): x_diff(%.2f) > low_priority_x_position_error(%.2f)",
                      x_diff, low_priority_x_position_error );

        if ( doGoToPoint( agent,
                          target_position,
                          std::max( max_x_position_error, max_y_position_error ), //std::min( max_x_position_error, max_y_position_error ),
                          dash_power,
                          use_back_dash,
                          false, // force back dash
                          false, // no emergent
                          true ) ) // look ball
        {
            agent->debugClient().addMessage( "Savior:GoalLinePositioning:normal" );
#ifdef VISUAL_DEBUG
            agent->debugClient().setTarget( target_position );
#endif
            dlog.addText( Logger::ROLE,
                          __FILE__":(doGoalLinePositioning): go to (%.2f %.2f) error=%.3f dash_power=%.1f",
                          target_position.x, target_position.y,
                          //std::min( max_x_position_error, max_y_position_error ),
                          std::max( max_x_position_error, max_y_position_error ),
                          dash_power );

            return true;
        }
    }
    else if ( ! y_near )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGoalLinePositioning) y_near is false",
                      x_diff, low_priority_x_position_error );

        if ( doGoToPoint( agent,
                          target_position,
                          std::min( max_x_position_error, max_y_position_error ),
                          dash_power,
                          use_back_dash,
                          false,  // force back dash
                          false, // no emergent
                          false // look ball
                          ) )
        {
            agent->debugClient().addMessage( "Savior:GoalLinePositioning:AdjustY" );
#ifdef VISUAL_DEBUG
            agent->debugClient().setTarget( target_position );
#endif
            dlog.addText( Logger::ROLE,
                          __FILE__":(doGoalLinePositioning) go to (%.2f %.2f) error=%.3f dash_power=%.1f",
                          target_position.x, target_position.y, low_priority_x_position_error, dash_power );
            return true;
        }
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGoalLinePositioning) goal line posisitioning side dash" );

        const AngleDeg dir_target = ( wm.self().body().degree() > 0.0
                                      ? +90.0
                                      : -90.0 );

        if ( ( wm.self().body() - dir_target ).abs() > 2.0 )
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

        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doGoalLinePositioning) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::doKickIfPossible( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( ! wm.self().isKickable()
         || ( wm.kickableTeammate()
              && wm.ball().distFromSelf() > wm.teammatesFromBall().front()->distFromBall() ) )
    {
        return false;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doKickIfPossible) start" );

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
                      __FILE__":(doKickIfPossible) opponent near or kickable, clear ball" );
        return true;
    }

    const CooperativeAction & first_action = ActionChainHolder::i().graph().bestFirstAction();

    //
    // if side of field, clear the ball
    //
    if ( first_action.type() == CooperativeAction::Pass )
    {
        const Vector2D target_point = first_action.targetBallPos();

        dlog.addText( Logger::ROLE,
                      __FILE__":(doKickIfPossible) self pos = [%.1f, %.1f], "
                      "pass target = [%.1f, %.1f]",
                      wm.self().pos().x, wm.self().pos().y,
                      target_point.x, target_point.y );

        if ( wm.self().pos().absY() > clear_y_threshold
             && ! ( target_point.y * wm.self().pos().y > 0.0
                    && target_point.absY() > wm.self().pos().absY() + 5.0
                    && target_point.x > wm.self().pos().x + 5.0 ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doKickIfPossible) self pos y %.2f is grater than %.2f,"
                          "pass target pos y = %.2f, clear ball",
                          wm.self().pos().absY(), clear_y_threshold,
                          target_point.y );
            agent->debugClient().addMessage( "Savior:SideForceClear" );

            Bhv_ClearBall().execute( agent );
            //agent->setNeckAction( new Neck_TurnToLowConfTeammate() );
            return true;
        }
        else
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doKickIfPossible) not force clear" );
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
    if ( Bhv_ClearBall().execute( agent ) )
    {
        //agent->setNeckAction( new Neck_TurnToLowConfTeammate() );

        agent->debugClient().addMessage( "no action, default clear ball" );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doKickIfPossible) no action, default clear ball" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_Savior::getOpponentBallPosition( const WorldModel & wm )
{
    const int opponent_step = wm.interceptTable()->opponentReachStep();
#if 0
    const PlayerObject * opponent = wm.interceptTable()->firstOpponent();
    Vector2D ball_pos;

    if ( opponent_step == 0
         && opponent )
    {
        ball_pos = opponent->pos() + opponent->vel();
        ball_pos += ( ServerParam::i().ourTeamGoalPos() - ball_pos ).setLengthVector( 0.5 );
        // ball_pos = ( opponent->pos() + opponent->vel() ) * 0.5;
        // ball_pos += wm.ball().pos() * 0.5;
    }
    else if ( wm.kickableOpponent() )
    {
        ball_pos = ( wm.kickableOpponent()->pos() + wm.kickableOpponent()->vel() ) * 0.5;
        ball_pos += ( ServerParam::i().ourTeamGoalPos() - ball_pos ).setLengthVector( 0.5 );
        // ball_pos = ( wm.kickableOpponent()->pos() + wm.kickableOpponent()->vel() ) * 0.5;
        // ball_pos += wm.ball().pos() * 0.5;
    }
    else if ( opponent_step == 1
              && opponent )
    {
        Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
        ball_pos = ball_next + ( opponent->pos() - ball_next ).setLengthVector( 0.5 ); // magic number
    }
    else
    {
        ball_pos = getFieldBoundPredictBallPos( wm, opponent_step, 0.5 );
    }

    return ball_pos;
#else
    return getFieldBoundPredictBallPos( wm, opponent_step, 0.5 );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_Savior::getBasicPosition( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    if ( wm.gameMode().type() == GameMode::CornerKick_
         || wm.gameMode().type() == GameMode::KickIn_ )
    {
        return Strategy::i().getPosition( wm.self().unum() );
    }

    // aggressive positioning
    const double additional_forward_positioning_max = 14.0;
    const Vector2D ball_pos = getOpponentBallPosition( wm );
    const AngleDeg ball_angle_from_our_goal = getDirFromOurGoal( ball_pos );

    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPosition) ball predict pos = (%.2f, %.2f)",
                  ball_pos.x, ball_pos.y );

    const Rect2D penalty_area( Vector2D( SP.ourTeamGoalLineX(),
                                         - SP.penaltyAreaHalfWidth() ),
                               Size2D( SP.penaltyAreaLength(),
                                       SP.penaltyAreaWidth() ) );

    const Rect2D conservative_area( Vector2D( SP.ourTeamGoalLineX(),
                                              - SP.pitchHalfWidth() ),
                                    Size2D( SP.penaltyAreaLength() + 15.0,
                                            SP.pitchWidth() ) );

    const Vector2D goal_pos = SP.ourTeamGoalPos();


    const int num_teammates_in_conservative_area
        = wm.countPlayer( new AndPlayerPredicate
                          ( new TeammatePlayerPredicate( wm ),
                            new ContainsPlayerPredicate< Rect2D >( conservative_area ) ) );

    const bool ball_is_in_conservative_area = conservative_area.contains( wm.ball().pos() );

    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPosition) conservative area: n_teammates=%d, ball=%d",
                  num_teammates_in_conservative_area,
                  ( ball_is_in_conservative_area? "in area" : "out of area" ) );

    double base_dist = 14.0;

    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPosition) base_dist=%.2f ourDefenseLineX=%.1f ball_dir_from_goal=%.1f",
                  base_dist, wm.ourDefenseLineX(), ball_angle_from_our_goal.degree() );

    M_goal_line_positioning = isGoalLinePositioningSituationBase( wm, ball_pos );

    // if ball is not in penalty area and teammates are in penalty area
    // (e.g. our clear ball succeeded), do back positioning
    if ( ! ball_is_in_conservative_area
         && num_teammates_in_conservative_area >= 2 )
    {
        if ( goal_pos.dist( ball_pos ) > 20.0 )
        {
            base_dist = std::min( base_dist,
                                  5.0 + ( goal_pos.dist( ball_pos ) - 20.0 ) * 0.1 );
        }
        else
        {
            base_dist = 5.0;
        }
    }

    M_emergent_advance = isEmergentOneToOneSituation( wm, ball_pos );
    if ( M_emergent_advance )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(getBasicPosition) emergent advance mode" );
        agent->debugClient().addMessage( "EmergentAdvance" );
    }


    if ( wm.gameMode().type() == GameMode::PenaltyTaken_ )
    {
#if defined( PENALTY_SHOOTOUT_BLOCK_IN_PENALTY_AREA )
        base_dist = 15.3;
#else
        base_dist = goal_pos.dist( ball_pos ) - 4.5;

        if ( wm.self().pos().dist( goal_pos ) > base_dist )
        {
            base_dist = wm.self().pos().dist( goal_pos );
        }
#endif

        dlog.addText( Logger::ROLE,
                      __FILE__":(getBasicPosition) penalty kick mode, new base dist = %.2f",
                      base_dist );
    }
    else if ( M_emergent_advance )
    {
        base_dist = goal_pos.dist( ball_pos ) - 3.0;
    }
    else if ( wm.ourDefenseLineX() >= - additional_forward_positioning_max )
    {
        if ( wm.self().stamina() >= SP.staminaMax() * 0.6 )
        {
            base_dist += std::min( wm.ourDefenseLineX(), 0.0 ) + additional_forward_positioning_max;
            M_aggressive = true;

            dlog.addText( Logger::ROLE,
                          __FILE__":(getBasicPosition) aggressive positioning, base_dist = %f",
                          base_dist );
            agent->debugClient().addMessage( "AggressivePositioniong" );
        }
    }
    else
    {
        M_aggressive = false;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPosition) base dist = %f", base_dist );

    M_dangerous = false;

    return getBasicPositionFromBall( wm,
                                     ball_pos,
                                     base_dist,
                                     wm.self().pos() );
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_Savior::getBasicPositionFromBall( const WorldModel & wm,
                                      const Vector2D & ball_pos,
                                      const double base_dist,
                                      const Vector2D & self_pos )
{
    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPositionFromBall) start base_dist=%.3f", base_dist );

    const double GOAL_LINE_POSITIONING_GOAL_X_DIST = getGoalLinePositioningXFromGoal( wm, ball_pos );
    const double MINIMUM_GOAL_X_DIST = 1.0;

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    dlog.addCircle( Logger::TEAM,
                    home_pos, wm.self().playerType().reliableCatchableDist(), "#0F0" );
    dlog.addCircle( Logger::TEAM,
                    home_pos, wm.self().playerType().maxCatchableDist(), "#FF0" );

    if ( ! M_emergent_advance
         && M_goal_line_positioning )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(getBasicPositionFromBall) goal line positioning" );
        return getGoalLinePositioningTarget( wm, GOAL_LINE_POSITIONING_GOAL_X_DIST, ball_pos );
    }

    const ServerParam & SP = ServerParam::i();

    const double goal_line_x = SP.ourTeamGoalLineX();
    const double goal_half_width = SP.goalHalfWidth();

    // const double alpha = std::atan2( goal_half_width, base_dist );

    const double min_x = ( goal_line_x + MINIMUM_GOAL_X_DIST );

    const Line2D line_1( ball_pos, Vector2D( -SP.pitchHalfLength(), +SP.goalHalfWidth() ) );
    const Line2D line_2( ball_pos, Vector2D( -SP.pitchHalfLength(), -SP.goalHalfWidth() ) );

    const AngleDeg line_dir = getDirFromOurGoal( ball_pos );
    const Line2D line_m( ball_pos, line_dir );

    const Line2D goal_line( Vector2D( -SP.pitchHalfLength(), 10.0 ),
                            Vector2D( -SP.pitchHalfLength(), -10.0 ) );

    const Vector2D goal_line_point = goal_line.intersection( line_m );

    if ( ! goal_line_point.isValid() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(getBasicPositionFromBall) invalid goal_line_point" );
        if ( ball_pos.x > 0.0 )
        {
            return Vector2D( min_x, +SP.goalHalfWidth() );
        }
        else if ( ball_pos.x < 0.0 )
        {
            return Vector2D( min_x, -SP.goalHalfWidth() );
        }
        else
        {
            return Vector2D( min_x, 0.0 );
        }
    }

    const AngleDeg current_position_line_dir = ( self_pos - goal_line_point ).th();
    const AngleDeg line_dir_error = line_dir - current_position_line_dir;
    const double danger_angle = 21.0;

    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPositionFromBall) line_dir=%.1f goal_line_point=(%.1f %.1f)",
                  line_dir.degree(),
                  goal_line_point.x, goal_line_point.y );
    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPositionFromBall) line_dit_thr=%.1f",
                  line_dir_error.abs() );
#ifdef VISUAL_DEBUG
    dlog.addLine( Logger::ROLE,
                  ball_pos, goal_line_point,
                  "#00F" );
#endif

#if 0
    {
        const PlayerType & ptype = wm.self().playerType();
        const Vector2D self_next = wm.self().pos() + wm.self().vel();

        Vector2D bpos_l = ball_pos;
        Vector2D bpos_r = ball_pos;
        Vector2D bvel_l = ( Vector2D( -SP.pitchHalfLength(), -SP.goalHalfWidth() + 0.2 ) - ball_pos );
        Vector2D bvel_r = ( Vector2D( -SP.pitchHalfLength(), +SP.goalHalfWidth() - 0.2 ) - ball_pos );
        bvel_l.setLength( SP.ballSpeedMax() );
        bvel_r.setLength( SP.ballSpeedMax() );

        dlog.addText( Logger::ROLE,
                      "(getBasicPositionFromBall) decide distance" );

        Vector2D nearest_point = goal_line_point;
        double min_dist2 = 1000000.0;
        for ( int i = 1; i <= 10; ++i )
        {
            bpos_l += bvel_l;
            bpos_r += bvel_r;

            if ( bpos_l.x < -SP.pitchHalfLength()
                 || bpos_r.x < -SP.pitchHalfLength() )
            {
                break;
            }

            Vector2D intersection = line_m.intersection( Line2D( bpos_l, bpos_r ) );
            if ( ! intersection.isValid() ) intersection = bpos_l;

            double dist_l = intersection.dist( bpos_l );
            double dist_r = intersection.dist( bpos_r );

            int step_l = ptype.cyclesToReachDistance( dist_l - ptype.maxCatchableDist() );
            int step_r = ptype.cyclesToReachDistance( dist_r - ptype.maxCatchableDist() );

            dlog.addRect( Logger::ROLE,
                          intersection.x - 0.05, intersection.y - 0.05, 0.1, 0.1, "#F00" );
            dlog.addText( Logger::ROLE,
                          "__ bstep=%d step_l=%d step_r=%d",
                          i, step_l, step_r );
            if ( step_l > i + 1 && step_r > i + 1 )
            {
                break;
            }

            double d2 = self_next.dist2( intersection );
            if ( d2 < min_dist2 )
            {
                nearest_point = intersection;
                min_dist2 = d2;
            }
        }

        dlog.addText( Logger::ROLE,
                      ">>> nearest point = (%.2f %.2f)",
                      nearest_point.x, nearest_point.y );
    }
#endif

#if 1
    double dist_from_goal = home_pos.dist( goal_line_point );
    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPositionFromBall) formation pos=(%.2f %.2f) dist from goal=%.3f",
                  home_pos.x, home_pos.y, dist_from_goal );
#else
    // angle -> dist
    double dist_from_goal
        = ( line_1.dist( goal_line_point ) + line_2.dist( goal_line_point ) )
        / 2.0
        / std::sin( alpha );
    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPositionFromBall) dist_from_goal=%.3f  alpha=%.1f",
                  dist_from_goal, AngleDeg::rad2deg( alpha ) );
    if ( dist_from_goal <= goal_half_width )
    {
        dist_from_goal = goal_half_width;
        dlog.addText( Logger::ROLE,
                      __FILE__":(getBasicPositionFromBall) dist_from_goal=%.3f  goal_half_width",
                      dist_from_goal );
    }
#endif

    if ( ( ball_pos - goal_line_point ).r() + 1.5 < dist_from_goal )
    {
        dist_from_goal = ( ball_pos - goal_line_point ).r() + 1.5;
        dlog.addText( Logger::ROLE,
                      __FILE__":(getBasicPositionFromBall) dist_from_goal=%.3f  goal_line_point dist?",
                      dist_from_goal );
    }

    //agent->debugClient().addMessage( "angleError=%.1f", line_dir_error.abs() );

    if ( wm.gameMode().type() != GameMode::PenaltyTaken_
         && line_dir_error.abs() > danger_angle
         && SP.ourTeamGoalPos().dist( ball_pos ) < 50.0
         && self_pos.dist( goal_line_point ) > 5.0 )
    {
        dist_from_goal *= ( 1.0 - ( line_dir_error.abs() - danger_angle ) / ( 180.0 - danger_angle ) ) / 3.0;

        dlog.addText( Logger::ROLE,
                      __FILE__":(getBasicPositionFromBall) dist_from_goal=%.3f  angle_error(1)=%.1f",
                      dist_from_goal, line_dir_error.abs() );

        if ( dist_from_goal < goal_half_width - 1.0 )
        {
            dist_from_goal = goal_half_width - 1.0;
            dlog.addText( Logger::ROLE,
                          __FILE__":(getBasicPositionFromBall) dist_from_goal=%.3f  angle_error(1-1)",
                          dist_from_goal );
        }

        M_dangerous = true;

#ifdef DO_AGGRESSIVE_POSITIONING
        if ( self_pos.x < -45.0
             && ( ball_pos - self_pos ).r() < 20.0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(getBasicPositionFromBall) change to goal line positioning" );

            M_goal_line_positioning = true;

            return getGoalLinePositioningTarget( wm, GOAL_LINE_POSITIONING_GOAL_X_DIST, ball_pos );
        }
#endif
    }
    else if ( line_dir_error.abs() > 5.0
              && ( goal_line_point - self_pos ).r() > 5.0 )
    {
        const double current_position_dist = ( goal_line_point - self_pos ).r();

        if ( dist_from_goal < current_position_dist )
        {
            dist_from_goal = current_position_dist;
        }

        dist_from_goal = ( goal_line_point - self_pos ).r();

        dlog.addText( Logger::ROLE,
                      __FILE__":(getBasicPositionFromBall) dist_from_goal=%.3f angle_error(2)=%.1f",
                      dist_from_goal, line_dir_error.abs() );

        M_dangerous = true;

#ifdef DO_GOAL_LINE_POSITIONING_AT_SIDE
        if ( self_pos.x < -45.0
             && ( ball_pos - self_pos ).r() < 20.0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(getBasicPositionFromBall) change to goal line positioning" );

            *goal_line_positioning = true;

            return getGoalLinePositioningTarget( wm, GOAL_LINE_POSITIONING_GOAL_X_DIST,
                                                 ball_pos, is_despair_situation );
        }
#endif

#ifdef VISUAL_DEBUG
        {
            const Vector2D r = goal_line_point
                + ( ball_pos - goal_line_point ).setLengthVector( dist_from_goal );

            dlog.addLine( Logger::ROLE, goal_line_point, r, "#FF1493" );
            dlog.addLine( Logger::ROLE, self_pos, r, "#FF1493" );
            // agent->debugClient().addLine( goal_line_point, r );
            // agent->debugClient().addLine( self_pos, r );
        }
#endif
    }
    else if ( wm.gameMode().type() != GameMode::PenaltyTaken_
              && line_dir_error.abs() > 10.0 )
    {
        //dist_from_goal = std::min( dist_from_goal, 14.0 );
        dist_from_goal = std::min( goal_line_point.dist( ball_pos ) * 0.7, 5.0 );

        dlog.addText( Logger::ROLE,
                      __FILE__":(getBasicPositionFromBall) dist_from_goal=%.2f angle_error(3)=%.1f",
                      dist_from_goal, line_dir_error.abs() );
    }


    Vector2D result = goal_line_point
        + ( ball_pos - goal_line_point ).setLengthVector( dist_from_goal );

    if ( result.x < min_x )
    {
        result.assign( min_x, result.y );
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPositionFromBall) positioning target = [%.2f,%.2f]",
                  result.x, result.y );

#ifdef DEBUG_PRINT
    Vector2D formation_pos = Strategy::i().getPosition( wm.self().unum() );
    dlog.addText( Logger::ROLE,
                  __FILE__":(getBasicPositionFromBall) formation = [%.2f,%.2f]",
                  formation_pos.x, formation_pos.y );
    dlog.addCircle( Logger::ROLE,
                    formation_pos, 0.5, "#0F0" );
#endif

    return result;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_Savior::getGoalLinePositioningXFromGoal( const WorldModel & wm,
                                             const Vector2D & ball_pos )
{
#ifdef PENALTY_SHOOTOUT_GOAL_PARALLEL_POSITIONING
    if ( wm.gameMode().type() == GameMode::PenaltyTaken_ )
    {
        return = 15.8;
    }
#endif

    double dist = 1.5;

#if ! defined( REVERT_TO_CONSERVATIVE_GOAL_LINE_POSITIONING_X )
    const double MAX_ADD_DIST = 15.0;
    const double THRESHOLD_X = -35.0;

    if ( wm.ourDefenseLineX() > THRESHOLD_X )
    {
        double defense_line_x_add = bound( 0.0,
                                           wm.ourDefenseLineX() - THRESHOLD_X,
                                           MAX_ADD_DIST );
        double ball_x_add = bound( 0.0,
                                   ball_pos.x,
                                   MAX_ADD_DIST );

        {
            const double d = ( defense_line_x_add * 2.0 + ball_x_add ) / 3.0;

            dist += d * 1.0;

            dlog.addText( Logger::ROLE,
                          __FILE__":(getGoalLinePositioningXFromGoal) new goal line positioning goal x dist = %.2f",
                          dist );
        }
    }
#endif

    return dist;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_Savior::getGoalLinePositioningTarget( const WorldModel & wm,
                                          const double goal_x_dist,
                                          const Vector2D & ball_pos )
{
    dlog.addText( Logger::ROLE,
                  __FILE__":(getGoalLinePositioningTarget) start " );

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

    if ( is_despair_situation( wm ) )
    {
        const double target_x = std::max( wm.self().inertiaPoint( 1 ).x,
                                          goal_line_x );

        const Line2D positioniong_line( Vector2D( target_x, -1.0 ),
                                        Vector2D( target_x, +1.0 ) );

        if ( wm.ball().vel().r() < EPS )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(getGoalLinePositioningTarget) despair_situation, "
                          "target is ball, target = [%.2f, %.2f]",
                          wm.ball().pos().x, wm.ball().pos().y );
            return wm.ball().pos();
        }

        const Line2D ball_line( ball_pos, wm.ball().vel().th() );

        const Vector2D c = positioniong_line.intersection( ball_line );

        if ( c.isValid() )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(getGoalLinePositioningTarget) despair_situation, "
                          "target is intercection, target = [%.2f, %.2f]",
                          c.x, c.y );
            return c;
        }
    }


    //
    // set base point to ball line and positioning line
    //
    Vector2D result_pos = target_line.intersection( line_m );

    if ( ! result_pos.isValid() )
    {
        double target_y;

        if ( ball_pos.absY() > SP.goalHalfWidth() )
        {
            target_y = sign( ball_pos.y ) * std::min( ball_pos.absY(), SP.goalHalfWidth() + 2.0 );
        }
        else
        {
            target_y = ball_pos.y * 0.8;
        }

        result_pos = Vector2D( goal_line_x + goal_x_dist, target_y );
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(getGoalLinePositioningTarget) base target = (%.1f, %.1f)",
                  result_pos.x, result_pos.y );

    const int teammate_reach_cycle = wm.interceptTable()->teammateReachCycle();
    const int opponent_reach_cycle = wm.interceptTable()->opponentReachCycle();
    const int predict_step = std::min( opponent_reach_cycle, teammate_reach_cycle );

    const Vector2D predict_ball_pos = wm.ball().inertiaPoint( predict_step );

    dlog.addText( Logger::ROLE,
                  __FILE__":(getGoalLinePositioningTarget) predict_step = %d, "
                  "predict_ball_pos = [%.1f, %.1f]",
                  predict_step,
                  predict_ball_pos.x, predict_ball_pos.y );

    const bool can_shoot_from_predict_ball_pos = opponentCanShootFrom( wm, predict_ball_pos );

    dlog.addText( Logger::ROLE,
                  __FILE__":(getGoalLinePositioningTarget) can_shoot_from_predict_ball_pos = %s",
                  ( can_shoot_from_predict_ball_pos? "true": "false" ) );

    //
    // shift to middle of goal if catchable
    //
    {
        const double ball_dist = ShootSimulator::get_dist_from_our_near_goal_post( predict_ball_pos );
        const int ball_step = SP.ballMoveStep( SP.ballSpeedMax(), ball_dist );

        const double orig_y_sign = sign( result_pos.y );
        //const double orig_y_abs = result_pos.absY();
        const double max_y_abs = SP.goalHalfWidth();

        for( double new_y_abs = result_pos.absY(); new_y_abs < max_y_abs; new_y_abs += 0.5 )
        {
            const Vector2D new_p( result_pos.x, orig_y_sign * new_y_abs );

            const double self_dist
                = ShootSimulator::get_dist_from_our_near_goal_post( new_p )
                - wm.self().playerTypePtr()->reliableCatchableDist();
            const int self_step = wm.self().playerTypePtr()->cyclesToReachDistance( self_dist );

            dlog.addText( Logger::ROLE,
                          __FILE__":(getGoalLinePositioningTarget) "
                          "check back shift y_abs = %.1f, self_step = %d, ball_step = %d",
                          new_y_abs, self_step, ball_step );

            //if ( self_step * 1.05 + 4 < ball_step )
            if ( self_step < ball_step )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__":(getGoalLinePositioningTarget) update to [%.2f, %.2f]",
                              new_p.x, new_p.y );
#ifdef VISUAL_DEBUG
                dlog.addCircle( Logger::ROLE, result_pos, 0.5, "#00F" );
                dlog.addLine( Logger::ROLE,
                              new_p + Vector2D( -1.5, -1.5 ), new_p + Vector2D( +1.5, +1.5 ),
                              "#00F" );
                dlog.addLine( Logger::ROLE,
                              new_p + Vector2D( -1.5, +1.5 ), new_p + Vector2D( +1.5, -1.5 ),
                              "#00F" );
                dlog.addLine( Logger::ROLE, result_pos, new_p, "#00F" );

                if ( can_shoot_from_predict_ball_pos )
                {
                    dlog.addLine( Logger::ROLE,
                                  Vector2D( SP.ourTeamGoalLineX() - 5.0,
                                            sign( wm.ball().pos().y ) * SP.goalHalfWidth() ),
                                  Vector2D( SP.ourTeamGoalLineX() + 5.0,
                                            sign( wm.ball().pos().y ) * SP.goalHalfWidth() ),
                                  "#00F" );
                }
#endif
                result_pos = new_p;
                break;
            }
        }
    }

    if ( result_pos.absY() > SP.goalHalfWidth() + 2.0 )
    {
        result_pos.assign( result_pos.x, sign( result_pos.y ) * ( SP.goalHalfWidth() + 2.0 ) );
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(getGoalLinePositioningTarget) target = [%.2f, %.2f]",
                  result_pos.x, result_pos.y );

    return result_pos;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::canCatchAtNearPost( const WorldModel & wm,
                                const Vector2D & pos,
                                const Vector2D & ball_pos )
{
    const ServerParam & SP = ServerParam::i();

    //
    // shift to middle of goal if catchable
    //
    const double ball_dist = ShootSimulator::get_dist_from_our_near_goal_post( ball_pos );
    const int ball_step = static_cast< int >( std::ceil( ball_dist / SP.ballSpeedMax() ) + EPS );

    const double self_dist = ShootSimulator::get_dist_from_our_near_goal_post( pos );
    const int self_step = wm.self().playerTypePtr()->cyclesToReachDistance( self_dist );

    if ( self_step * 1.05 + 4 < ball_step )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(canCatchAtNearPost) can catch at near post [%.1f, %.1f]",
                      pos.x, pos.y );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
AngleDeg
Bhv_Savior::getDirFromOurGoal( const Vector2D & pos )
{
    const ServerParam & SP = ServerParam::i();

    const Vector2D goal_post_plus( -SP.pitchHalfLength(), +SP.goalHalfWidth() );
    const Vector2D goal_post_minus( -SP.pitchHalfLength(), -SP.goalHalfWidth() );

    const double dist_to_plus_post = pos.dist( goal_post_plus );
    const double dist_to_minus_post = pos.dist( goal_post_minus );
#if 0
    const double plus_rate = 0.3 + 0.4 * ( dist_to_minus_post / ( dist_to_plus_post + dist_to_minus_post ) );
    const double minus_rate = 0.3 + 0.4 * ( dist_to_plus_post / ( dist_to_plus_post + dist_to_minus_post ) );
#else
    const double plus_rate = 0.25 + 0.5 * ( dist_to_plus_post / ( dist_to_plus_post + dist_to_minus_post ) );
    const double minus_rate = 0.25 + 0.5 * ( dist_to_minus_post / ( dist_to_plus_post + dist_to_minus_post ) );
#endif
    const AngleDeg plus_post_angle = ( goal_post_plus - pos ).th();
    const AngleDeg minus_post_angle = ( goal_post_minus - pos ).th();
    const double angle_diff = ( plus_post_angle - minus_post_angle ).abs();

    dlog.addText( Logger::ROLE,
                  __FILE__":(getDirFromOurGoal) plus_rate=%.3f minus_rate=%.3f",
                  plus_rate, minus_rate );

    return plus_post_angle.isLeftOf( minus_post_angle )
        ? plus_post_angle + ( angle_diff * plus_rate ) + 180.0
        : minus_post_angle + ( angle_diff * minus_rate ) + 180.0;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_Savior::getTackleProbability( const Vector2D & body_relative_ball )
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

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_Savior::getSelfNextPosWithDash( const WorldModel & wm,
                                    const double dash_power )
{
    return wm.self().inertiaPoint( 1 )
        + Vector2D::polar2vector
        ( dash_power * wm.self().playerType().dashPowerRate(),
          wm.self().body() );
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_Savior::getSelfNextTackleProbabilityWithDash( const WorldModel & wm,
                                                  const double dash_power )
{
    return getTackleProbability( ( wm.ball().inertiaPoint( 1 )
                                   - getSelfNextPosWithDash( wm, dash_power ) )
                                 .rotatedVector( - wm.self().body() ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_Savior::getSelfNextTackleProbabilityWithTurn( const WorldModel & wm )
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

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::opponentCanShootFrom( const WorldModel & wm,
                                  const Vector2D & pos,
                                  const long valid_teammate_threshold )
{
    return ShootSimulator::opponent_can_shoot_from( pos,
                                                    wm.getPlayers( new TeammatePlayerPredicate( wm ) ),
                                                    valid_teammate_threshold,
                                                    SHOOT_DIST_THRESHOLD,
                                                    SHOOT_ANGLE_THRESHOLD );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::canBlockShootFrom( const WorldModel & wm,
                               const Vector2D & pos )
{
    bool result = ! ShootSimulator::opponent_can_shoot_from( pos,
                                                             wm.getPlayers( new TeammatePlayerPredicate( wm ) ),
                                                             SHOOT_BLOCK_TEAMMATE_POS_COUNT_THRESHOLD,
                                                             SHOOT_DIST_THRESHOLD,
                                                             SHOOT_ANGLE_THRESHOLD,
                                                             -1.0 );
    dlog.addText( Logger::ROLE,
                  __FILE__":(canBlockShootFrom) pos = [%.1f, %.1f], result = %s",
                  pos.x, pos.y,
                  ( result? "true" : "false" ) );

    return result;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::doChaseBall( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const int self_reach_cycle = wm.interceptTable()->selfReachCycle();
    const Vector2D intercept_point = wm.ball().inertiaPoint( self_reach_cycle );

    dlog.addText( Logger::ROLE,
                  __FILE__":(doChaseBall) intercept point = [%.1f, %.1f]",
                  intercept_point.x, intercept_point.y );

    if ( intercept_point.x > SP.ourTeamGoalLineX() - 1.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doChaseBall) normal intercept" );

        if ( Body_Intercept().execute( agent ) )
        {
            agent->setNeckAction( new Neck_ChaseBall() );

            return true;
        }
    }


    //
    // go to goal line
    //
    const Vector2D predict_ball_pos = getFieldBoundPredictBallPos( wm, self_reach_cycle, 1.0 );
    const double target_error = std::max( SP.catchableArea(), SP.tackleDist() );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doChaseBall) goal line intercept point = [%.1f, %.1f]",
                  predict_ball_pos.x, predict_ball_pos.y );

    if ( doGoToPoint( agent,
                      predict_ball_pos,
                      target_error,
                      SP.maxPower(),
                      true, // back dash
                      true, // force back dash
                      true )  // emergent
         )
    {
        agent->setNeckAction( new Neck_ChaseBall() );

        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::isGoalLinePositioningSituationBase( const WorldModel & wm,
                                                const Vector2D & ball_pos )
{
    const double SIDE_ANGLE_DEGREE_THRESHOLD = 50.0;

    if ( wm.gameMode().type() == GameMode::PenaltyTaken_ )
    {
#ifdef PENALTY_SHOOTOUT_GOAL_PARALLEL_POSITIONING
        const ServerParam & SP = ServerParam::i();

        if ( wm.self().pos().x > SP.ourPenaltyAreaLineX() )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(isGoalLinePositioningSituationBase) "
                          "penalty kick mode parallel positioning,"
                          " over penalty area x, return false" );
            return false;
        }
        else if ( ball_pos.absY() < SP.goalHalfWidth() )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(isGoalLinePositioningSituationBase) "
                          "penalty kick mode parallel positioning, return true" );
            return true;
        }
        else
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(isGoalLinePositioningSituationBase) "
                          "penalty kick mode parallel positioning,"
                          " side of goal, return false" );
            return false;
        }
#else
        dlog.addText( Logger::ROLE,
                      __FILE__":(isGoalLinePositioningSituationBase) "
                      "penalty kick mode, return false" );
        return false;
#endif
    }

    if ( wm.ourDefenseLineX() >= -15.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (isGoalLinePositioningSituationBase) "
                      "defense line(%.1f) too forward, "
                      "no need to goal line positioning",
                      wm.ourDefenseLineX() );
        return false;
    }


    const AngleDeg ball_dir = getDirFromOurGoal( ball_pos );
    const bool is_side_angle = ( ball_dir.abs() > SIDE_ANGLE_DEGREE_THRESHOLD );

    dlog.addText( Logger::ROLE,
                  __FILE__":(isGoalLinePositioningSituationBase) "
                  "ball direction from goal = %.1f, is_side_angle = %s",
                  ball_dir.degree(), ( is_side_angle ? "true": "false" ) );

    if ( is_side_angle )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(isGoalLinePositioningSituationBase) "
                      "side angle, not goal line positioning" );
        return false;
    }

    if ( wm.ourDefenseLineX() <= -15.0
         && ball_pos.x < 5.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(isGoalLinePositioningSituationBase) "
                      "defense line(%.1f) too back, "
                      "goal line positioning, return true",
                      wm.ourDefenseLineX() );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(isGoalLinePositioningSituationBase) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_Savior::isEmergentOneToOneSituation( const WorldModel & wm,
                                         const Vector2D & ball_pos )
{
    // if opponent player will get the ball at backward of our defense line, do 1-to-1 aggressive defense

    const AbstractPlayerObject * fastest_opponent = wm.interceptTable()->fastestOpponent();

    if ( fastest_opponent
         && wm.gameMode().type() != GameMode::PenaltyTaken_
         && ! M_goal_line_positioning
         && ball_pos.x >= ServerParam::i().ourPenaltyAreaLineX() - 5.0
         && wm.interceptTable()->opponentReachCycle() < wm.interceptTable()->teammateReachCycle()
         && ball_pos.x <= wm.ourDefenseLineX() + 5.0 )
    {
        const AngleDeg ball_angle_from_our_goal = getDirFromOurGoal( ball_pos );

        if ( ball_angle_from_our_goal.abs() < 20.0
             && ( ball_angle_from_our_goal - getDirFromOurGoal( wm.self().inertiaPoint( 1 ) )).abs() < 3.0
             && opponentCanShootFrom( wm, ball_pos, 20 )
             && 0 == wm.countPlayer( new AndPlayerPredicate
                                     ( new TeammatePlayerPredicate( wm ),
                                       new XCoordinateBackwardPlayerPredicate( fastest_opponent->pos().x ),
                                       new YCoordinatePlusPlayerPredicate( fastest_opponent->pos().y - 1.0 ),
                                       new YCoordinateMinusPlayerPredicate( fastest_opponent->pos().y + 1.0 ) ) )
             )
        {
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_Savior::getFieldBoundPredictBallPos( const WorldModel & wm,
                                         const int predict_step,
                                         const double shrink_offset )
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
