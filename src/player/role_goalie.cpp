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

#include "role_goalie.h"

#include "strategy.h"
#include "field_analyzer.h"
#include "action_chain_holder.h"
#include "move_simulator.h"

#include "bhv_chain_action.h"
#include "bhv_clear_ball.h"
#include "bhv_deflecting_tackle.h"
#include "bhv_tactical_tackle.h"
#include "bhv_tackle_intercept.h"
#include "body_savior_go_to_point.h"
#include "neck_chase_ball.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_stop_dash.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/view_synch.h>
#include <rcsc/action/view_wide.h>
#include <rcsc/action/view_normal.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/segment_2d.h>
#include <rcsc/geom/matrix_2d.h>

using namespace rcsc;

const std::string RoleGoalie::NAME( "Goalie" );

/*-------------------------------------------------------------------*/
/*!

 */
namespace {
rcss::RegHolder role = SoccerRole::creators().autoReg( &RoleGoalie::create,
                                                       RoleGoalie::name() );


const Sector2D shootable_sector( Vector2D( -58.0, 0.0 ),
                                 0.0, 20.0,
                                 -42.5, 42.5 );

/*-------------------------------------------------------------------*/
/*!

 */
bool
is_ball_moving_to_our_goal( const WorldModel & wm )
{
    if ( wm.self().isKickable()
         || wm.kickableTeammate()
         || wm.kickableOpponent() )
    {
        return false;
    }

    const Segment2D goal( Vector2D( -ServerParam::i().pitchHalfLength(),
                                    -ServerParam::i().goalHalfWidth() - 1.5 ),
                          Vector2D( -ServerParam::i().pitchHalfLength(),
                                    +ServerParam::i().goalHalfWidth() + 1.5 ) );
    const Segment2D ball_move( wm.ball().pos(), wm.ball().inertiaFinalPoint() );

    return goal.intersectsExceptEndpoint( ball_move );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
is_opponent_shoot_situation( const WorldModel & wm )
{
    const int self_step = wm.interceptTable()->selfReachStep();
    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( opponent_step > self_step + 1
         || opponent_step > teammate_step + 2 )
    {
        return false;
    }

    //const Vector2D ball_reach_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );
    const Vector2D ball_reach_pos
        = FieldAnalyzer::get_field_bound_predict_ball_pos( wm, opponent_step, 0.5 );

    return ServerParam::i().ourTeamGoalPos().dist2( ball_reach_pos ) < std::pow( 18.0, 2 );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
is_goal_post_block_situation( const WorldModel & wm )
{
    //const Vector2D ball_reach_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );
    const int opponent_step = wm.interceptTable()->opponentReachStep();
    const Vector2D ball_reach_pos
        = FieldAnalyzer::get_field_bound_predict_ball_pos( wm, opponent_step, 0.5 );

    if ( ball_reach_pos.absY() > ServerParam::i().goalHalfWidth() - 0.5
         && ball_reach_pos.dist2( ServerParam::i().ourTeamGoalPos() ) < std::pow( 13.5, 2 ) )
    {
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
is_dangerous_situation( const WorldModel & wm )
{
    const int self_step = wm.interceptTable()->selfReachStep();
    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( opponent_step > self_step + 1
         || opponent_step > teammate_step + 2 )
    {
        return false;
    }

    //const Vector2D ball_reach_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );
    const Vector2D ball_reach_pos
        = FieldAnalyzer::get_field_bound_predict_ball_pos( wm, opponent_step, 0.5 );

    if ( ! shootable_sector.contains( ball_reach_pos ) )
    {
        return false;
    }

    //
    // check other blocker teammate
    //

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
is_despair_situation( const WorldModel & wm )
{
    const int self_step = wm.interceptTable()->selfReachStep();
    const Vector2D self_intercept_pos = wm.ball().inertiaPoint( self_step );

    return ( self_intercept_pos.x < -ServerParam::i().pitchHalfLength()
             && is_ball_moving_to_our_goal( wm ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_block_center_point( const WorldModel & wm,
                        const Vector2D & ball_reach_pos )
{
    const ServerParam & SP = ServerParam::i();

    //
    // if exist other blocker teammate, shrink my block area
    //

    Vector2D left_side( -SP.pitchHalfLength(), -SP.goalHalfWidth() );
    Vector2D right_side( -SP.pitchHalfLength(), +SP.goalHalfWidth() );
    Triangle2D left_triangle( ball_reach_pos, left_side, SP.ourTeamGoalPos() );
    Triangle2D right_triangle( ball_reach_pos, right_side, SP.ourTeamGoalPos() );

    for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(), end = wm.teammates().end();
          t != end;
          ++t )
    {
        if ( left_triangle.contains( (*t)->pos() ) )
        {
            Line2D left_line( ball_reach_pos, (*t)->pos() );
            left_side = Vector2D( -SP.pitchHalfLength(), left_line.getY( -SP.pitchHalfLength() ) + 1.0 );
            left_triangle = Triangle2D( ball_reach_pos, left_side, SP.ourTeamGoalPos() );
        }
        else if( right_triangle.contains( (*t)->pos() ) )
        {
            Line2D right_line( ball_reach_pos, (*t)->pos() );
            right_side = Vector2D( -SP.pitchHalfLength(), right_line.getY( -SP.pitchHalfLength() ) - 1.0 );
            right_triangle = Triangle2D( ball_reach_pos, right_side, SP.ourTeamGoalPos() );
        }
    }

    // dlog.addTriangle( Logger::ROLE, left_triangle, "#00F" );
    // dlog.addTriangle( Logger::ROLE, right_triangle, "#00F" );

    Vector2D result_pos = SP.ourTeamGoalPos();

    double step_ball_to_left = SP.ballMoveStep( SP.ballSpeedMax(), ball_reach_pos.dist( left_side ) );
    if ( step_ball_to_left < 1.0
         || 30.0 < step_ball_to_left )
    {
        step_ball_to_left = 30.0;
    }

    double step_ball_to_right = SP.ballMoveStep( SP.ballSpeedMax(), ball_reach_pos.dist( right_side ) );
    if ( step_ball_to_right < 1.0
         || 30.0 < step_ball_to_right )
    {
        step_ball_to_right = 30.0;
    }

    result_pos.y = SP.goalHalfWidth()
        - SP.goalWidth() * ( step_ball_to_right / ( step_ball_to_left + step_ball_to_right ) );

    return result_pos;
}

/*-------------------------------------------------------------------*/
/*!

 */
AngleDeg
get_target_body_angle( const Vector2D & self_pos,
                       const Vector2D & opponent_ball_pos )
{
    AngleDeg target_body_angle = ( opponent_ball_pos - self_pos ).th();
    double body_angle_rotation = ( self_pos.y > 0.0
                                   ? +90.0
                                   : -90.0 );

    dlog.addText( Logger::ROLE,
                  __FILE__":(get_target_body_angle) base_angle=%.1f rotation=%.1f",
                  target_body_angle.degree(), body_angle_rotation );

    if ( self_pos.x < -45.0 )
    {
        body_angle_rotation *= -1.0;
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_target_body_angle) inverse rotation %.1f",
                      body_angle_rotation );
    }

    if ( self_pos.x < -ServerParam::i().pitchHalfLength() + ServerParam::i().catchableArea()
         && self_pos.absY() > ServerParam::i().goalHalfWidth() )
    {
        if ( self_pos.y > 0.0 )
        {
            body_angle_rotation -= 20.0;
        }
        else
        {
            body_angle_rotation += 20.0;
        }
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_target_body_angle) adjust rotation %.1f",
                      body_angle_rotation );
    }

    target_body_angle += body_angle_rotation;

    if ( opponent_ball_pos.absY() < 4.0 )
    {
        if ( target_body_angle.abs() < 90.0 )
        {
            AngleDeg adjusted_angle = ( self_pos.y < 0.0 ? 90.0 : 90.0 );
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_target_body_angle) check adjusted angle %.1f",
                          adjusted_angle.degree() );
            if ( ( target_body_angle - adjusted_angle ).abs() < 20.0 )
            {
                target_body_angle = adjusted_angle;
                dlog.addText( Logger::ROLE,
                              __FILE__":(get_target_body_angle) adjust to angle %.1f",
                              adjusted_angle.degree() );
            }
        }
    }

    return target_body_angle;
}
}

/*-------------------------------------------------------------------*/
/*!

 */
RoleGoalie::RoleGoalie()
    : M_opponent_shoot_situation( false ),
      M_goal_post_block_situation( false ),
      M_dangerous_situation( false ),
      M_despair_situation( false ),
      M_opponent_ball_pos( 0.0, 0.0 )
      //M_block_center( ServerParam::i().ourTeamGoalPos() )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleGoalie::execute( PlayerAgent * agent )
{
    judgeSituation( agent );

    setDefaultNeckAndViewAction( agent );

    if ( doCatchIfPossible( agent ) )
    {
        return true;
    }

    if ( doKickIfPossible( agent ) )
    {
        return true;
    }

    if ( doTackleIfNecessary( agent ) )
    {
        return true;
    }

    if ( doChaseBallIfNessary( agent ) )
    {
        return true;
    }

    if ( doFindBallIfNecessary( agent ) )
    {
        return true;
    }

    if ( doMove( agent ) )
    {
        return true;
    }

    agent->debugClient().addMessage( "GK:NoAction" );
    dlog.addText( Logger::ROLE,
                  __FILE__": (execute) no action" );
    return agent->doTurn( 0.0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleGoalie::judgeSituation( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    //M_opponent_ball_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );
    const int opponent_step = wm.interceptTable()->opponentReachStep();
    M_opponent_ball_pos
        = FieldAnalyzer::get_field_bound_predict_ball_pos( wm, opponent_step, 0.5 );

    dlog.addText( Logger::ROLE,
                  "(Goalie::judgeSituation) reach_step: self=%d teammate=%d opponent=%d",
                  wm.interceptTable()->selfReachStep(),
                  opponent_step,
                  wm.interceptTable()->opponentReachStep() );
    dlog.addText( Logger::ROLE,
                  "(Goalie::judgeSituation) opponent_ball_pos=(%.2f %.2f)",
                  M_opponent_ball_pos.x, M_opponent_ball_pos.y );
    dlog.addRect( Logger::ROLE,
                  M_opponent_ball_pos.x - 0.1, M_opponent_ball_pos.y - 0.1, 0.2, 0.2, "#F0F", true );

    if ( is_goal_post_block_situation( wm ) )
    {
        M_goal_post_block_situation = true;
        M_opponent_shoot_situation = true;
        agent->debugClient().addMessage( "PostBlock" );
        dlog.addText( Logger::ROLE,
                      "(Goalie::judgeSituation) goal_post_block_situation" );
    }

    if ( is_opponent_shoot_situation( wm ) )
    {
        M_opponent_shoot_situation = true;
        agent->debugClient().addMessage( "OpponentShoot" );
        dlog.addText( Logger::ROLE,
                      "(Goalie::judgeSituation) opponent_shoot_situation" );
    }

    dlog.addSector( Logger::ROLE,
                    shootable_sector, "#F00" );
    if ( is_dangerous_situation( wm ) )
    {
        M_dangerous_situation = true;
        agent->debugClient().addMessage( "Dangerous" );
        dlog.addText( Logger::ROLE,
                      "(Goalie::judgeSituation) dangerous_situation" );
    }

    if ( is_ball_moving_to_our_goal( wm ) )
    {
        M_ball_moving_to_our_goal = true;
        agent->debugClient().addMessage( "BallToOurGoal" );
        dlog.addText( Logger::ROLE,
                      "(Goalie::judgeSituation) ball_moving_to_our_goal" );
    }

    if ( is_despair_situation( wm ) )
    {
        M_despair_situation = true;
        agent->debugClient().addMessage( "Despair" );
        dlog.addText( Logger::ROLE,
                      "(Goalie::judgeSituation) despair_situation" );
    }

    // M_block_center = get_block_center_point( wm, M_opponent_ball_pos );
    // dlog.addRect( Logger::ROLE,
    //               M_block_center.x - 0.25, M_block_center.y - 0.25, 0.5, 0.5, "#F00", true );

    // dlog.addLine( Logger::ROLE,
    //               M_opponent_ball_pos, M_block_center, "#F00" );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleGoalie::setDefaultNeckAndViewAction( PlayerAgent * agent )
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
                          __FILE__":(setDefaultNeckAndViewAction) neck chase ball" );
            agent->setNeckAction( new Neck_ChaseBall() );
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
RoleGoalie::doCatchIfPossible( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    const double MAX_SELF_BALL_ERROR = 0.5;

    const bool catchable_situation = ( wm.self().goalie()
                                       && wm.lastKickerSide() != wm.ourSide() // not back pass
                                       && ( wm.gameMode().type() == GameMode::PlayOn
                                            || wm.gameMode().type() == GameMode::PenaltyTaken_ )
                                       && wm.time().cycle() >= agent->effector().getCatchTime().cycle() + SP.catchBanCycle()
                                       && ( wm.ball().pos().x < ( SP.ourPenaltyAreaLineX()
                                                                  + SP.ballSize() * 2
                                                                  - MAX_SELF_BALL_ERROR )
                                            && wm.ball().pos().absY()  < ( SP.penaltyAreaHalfWidth()
                                                                           + SP.ballSize() * 2
                                                                           - MAX_SELF_BALL_ERROR ) ) );

    if ( catchable_situation
         && wm.self().catchProbability() > 0.99
         //&& ( ! wm.kickableTeammate() || wm.kickableOpponent() )
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doCatchIfPossible) doCatchIfPossible prob=%.3f",
                      wm.self().catchProbability() );
        agent->debugClient().addMessage( "Catch" );
        agent->setNeckAction( new Neck_TurnToBall() );
        agent->setViewAction( new View_Synch() );
        return agent->doCatch();
    }

    //
    // TODO: consider catch probability
    //


    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleGoalie::doKickIfPossible( PlayerAgent * agent )
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

    //
    // if exist near opponent, clear the ball
    //
    if ( wm.kickableOpponent()
         || wm.interceptTable()->opponentReachCycle() < 4 )
    {
        Bhv_ClearBall().execute( agent );
        agent->setNeckAction( new Neck_TurnToBall() );
        agent->setViewAction( new View_Synch() );

        agent->debugClient().addMessage( "Clear1" );
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
                      __FILE__":(doKickIfPossible) try pass. target = (%.2f, %.2f)",
                      target_point.x, target_point.y );

        if ( wm.self().pos().absY() > ServerParam::i().penaltyAreaHalfWidth() - 5.0
             && ( target_point.y * wm.self().pos().y < 0.0 // opposite side
                  || target_point.absY() < wm.self().pos().absY() + 5.0 // center direction
                  || target_point.x < wm.self().pos().x + 5.0 ) )  // no forward pass
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doKickIfPossible) cancel pass. clear" );
            agent->debugClient().addMessage( "SideForceClear" );

            Bhv_ClearBall().execute( agent );
            return true;
        }
    }

    //
    // do chain action
    //
    if ( first_action.type() != CooperativeAction::Hold )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doKickIfPossible) try action sequence" );
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
        agent->debugClient().addMessage( "Clear2" );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doKickIfPossible) no action, default clear ball" );
        return true;
    }

    return false;

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleGoalie::doTackleIfNecessary( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    if ( M_despair_situation )
    {
        const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
        const bool next_step_in_goal = ( ball_next.x < -SP.pitchHalfLength() + 0.2 );

        dlog.addText( Logger::ROLE,
                      __FILE__":(doTackleIfNecessary) despair situation. try tackle. current_prob=%.3f",
                      wm.self().tackleProbability() );

        if ( ! next_step_in_goal
             && wm.self().tackleProbability() < 0.9 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) ball is not in the goal in next step,"
                          " ball_next=(%.2f, %.2f)",
                          ball_next.x, ball_next.y );

            if ( Bhv_TackleIntercept().execute( agent ) )
            {
                dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) despair situation. tackle intercept" );
                agent->debugClient().addMessage( "GK:TackleIntercept" );
                return true;
            }
        }

        if ( Bhv_DeflectingTackle( EPS, true ).execute( agent ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTackleIfNecessary) despair situation. done deflecting tackle" );
            agent->debugClient().addMessage( "GK:DeflectTackle" );
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
            agent->debugClient().addMessage( "GK:TacticalTackle" );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleGoalie::doChaseBallIfNessary( PlayerAgent * agent )
{
    static GameTime s_last_chase_time( -1, 0 );

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

    int opponent_step_penalty = -1;
    if ( s_last_chase_time.cycle() + 1 == wm.time().cycle() )
    {
        opponent_step_penalty = 0;
    }

    if ( M_despair_situation )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doChaseBallIfNecessary) despair situation." );
        agent->debugClient().addMessage( "GK:DespairChase" );

        if ( doChaseBall( agent ) )
        {
            s_last_chase_time = wm.time();
            return true;
        }
    }

    if ( wm.gameMode().type() == GameMode::PlayOn
         && ! wm.kickableTeammate() )
    {
        if ( self_reach_cycle <= opponent_reach_cycle + opponent_step_penalty
             && ( ( self_reach_cycle + 5 <= teammate_reach_cycle
                    || self_intercept_point.absY() < SP.penaltyAreaHalfWidth() - 1.0 )
                  && shrinked_penalty_area.contains( self_intercept_point ) )
             && ( self_reach_cycle <= teammate_reach_cycle
                  || ( shrinked_penalty_area.contains( self_intercept_point )
                       && self_reach_cycle <= teammate_reach_cycle + 3 ) ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doChaseBallIfNecessary) normal chase." );
            agent->debugClient().addMessage( "GK:Chase" );

            if ( doChaseBall( agent ) )
            {
                s_last_chase_time = wm.time();
                return true;
            }
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
RoleGoalie::doChaseBall( PlayerAgent * agent )
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
    const Vector2D predict_ball_pos
        = FieldAnalyzer::get_field_bound_predict_ball_pos( wm, self_reach_cycle, 1.0 );
    const double tolerance = std::max( SP.catchableArea(), SP.tackleDist() );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doChaseBall) goal line intercept point = [%.1f, %.1f]",
                  predict_ball_pos.x, predict_ball_pos.y );

    if ( doGoToPoint( agent,
                      predict_ball_pos,
                      tolerance ) )
    {
        agent->setNeckAction( new Neck_ChaseBall() );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doChaseBall) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleGoalie::doFindBallIfNecessary( PlayerAgent * agent )
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
        agent->debugClient().addMessage( "GK:FindBall" );
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
void
RoleGoalie::doTurnLookBall( PlayerAgent * agent,
                            const AngleDeg & target_body_angle )
{
    const WorldModel & wm = agent->world();

    const double turn_moment = ( target_body_angle - wm.self().body() ).degree();
    const double max_turn = wm.self().playerType().effectiveTurn( ServerParam::i().maxMoment(),
                                                                  wm.self().vel().r() );

    dlog.addText( Logger::ROLE,
                  __FILE__":(doTurnLookBall) turn_moment=%.1f max_turn=%.1f",
                  turn_moment, max_turn );

    if ( std::fabs( turn_moment ) <= max_turn )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doTurnLookBall) 1 turn" );
        agent->doTurn( turn_moment );
        return;
    }

    const AngleDeg next_body_angle = wm.self().body() + ( max_turn * sign( turn_moment ) );
    const Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    const Vector2D next_ball_pos = wm.ball().pos() + wm.ball().vel();
    const AngleDeg next_ball_angle = ( next_ball_pos - next_self_pos ).th();

    if ( ( next_ball_angle - next_body_angle ).abs() < 95.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doTurnLookBall) 2 or more turns. normal" );
        agent->doTurn( turn_moment );
        return;
    }

    const AngleDeg reverse_body_angle = wm.self().body() + ( max_turn * sign( -turn_moment ) );
    if ( ( next_ball_angle - reverse_body_angle ).abs() < 95.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doTurnLookBall) 2 or more turns. reverse" );
        double command_moment = ( turn_moment > 0.0
                                  ? -ServerParam::i().maxMoment()
                                  : +ServerParam::i().maxMoment() );
        agent->doTurn( command_moment );
        return;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doTurnLookBall) 2 or more turns. final" );

    agent->doTurn( turn_moment );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleGoalie::doTurnIfOnTheBlockLine( PlayerAgent * agent,
                                    const Vector2D & target_pos,
                                    const double dist_tolerance,
                                    const double angle_tolerance )
{
    const WorldModel & wm = agent->world();

    if ( M_opponent_ball_pos.x > -30.0
         || M_opponent_ball_pos.dist2( ServerParam::i().ourTeamGoalPos() ) > std::pow( 25.0, 2 ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doTurnIfOnTheBlockLine) false: far opponent ball pos" );
        return false;
    }

    const Line2D block_line( M_opponent_ball_pos, target_pos );
    const Vector2D self_final_pos = wm.self().inertiaFinalPoint();
    const Segment2D self_move_segment( wm.self().pos(), self_final_pos );

    if ( block_line.dist( self_final_pos ) > dist_tolerance
         && ! self_move_segment.intersects( block_line ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doTurnIfOnTheBlockLine) false: not on the block line" );
        return false;
    }

    if ( self_final_pos.x > M_opponent_ball_pos.x )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doTurnIfOnTheBlockLine) false: on the line, but illegal x" );
        return false;
    }

    if ( self_final_pos.dist( target_pos ) > 3.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doTurnIfOnTheBlockLine) false: on the line, but too far" );
        return false;
    }

    const AngleDeg normal_angle = get_target_body_angle( target_pos, M_opponent_ball_pos );
    const AngleDeg reverse_angle = normal_angle + 180.0;

    const double normal_diff = ( normal_angle - wm.self().body() ).abs();
    const double reverse_diff = ( reverse_angle - wm.self().body() ).abs();

    if ( std::min( normal_diff, reverse_diff ) < angle_tolerance )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doTurnIfOnTheBlockLine) false: already facing" );
        return false;
    }

    const AngleDeg target_body_angle = ( normal_diff < reverse_diff
                                         ? normal_angle
                                         : reverse_angle );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doTurnIfOnTheBlockLine) turn to %.1f",
                  target_body_angle.degree() );

    doTurnLookBall( agent, target_body_angle );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleGoalie::doTurnIfNecessaryAndPossible( PlayerAgent * agent,
                                          const Vector2D & target_pos,
                                          const double dist_tolerance,
                                          const double angle_tolerance )
{
    const WorldModel & wm = agent->world();

    const Vector2D self_next = wm.self().pos() + wm.self().vel();
    if ( self_next.dist2( target_pos ) > std::pow( dist_tolerance, 2 ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doTurnIfNecessaryAndPossible) false: over run" );
        return false;
    }

    const AngleDeg normal_angle = get_target_body_angle( wm.self().inertiaFinalPoint(),
                                                         M_opponent_ball_pos );
    const AngleDeg reverse_angle = normal_angle + 180.0;

    const double normal_diff = ( normal_angle - wm.self().body() ).abs();
    const double reverse_diff = ( reverse_angle - wm.self().body() ).abs();

    dlog.addText( Logger::ROLE,
                  __FILE__":(doTurnIfNecessaryAndPossible) angle_tolerance=%.1f",
                  angle_tolerance );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doTurnIfNecessaryAndPossible) target_angle=%.1f(%.1f) current=%.1f diff=%.1f(%.1f)",
                  normal_angle.degree(), reverse_angle.degree(),
                  wm.self().body().degree(),
                  normal_diff, reverse_diff );

    // try turn if necessary and possible
    if ( normal_diff < angle_tolerance )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doTurnIfNecessaryAndPossible) false: already facing" );
        return false;
    }

    const double max_turn = wm.self().playerType().effectiveTurn( ServerParam::i().maxMoment(),
                                                                  wm.self().vel().r() );

    AngleDeg target_body_angle = normal_angle;

    if ( ( wm.kickableOpponent()
           || wm.interceptTable()->opponentReachStep() <= 2 )
         && wm.ball().pos().dist2( ServerParam::i().ourTeamGoalPos() ) < std::pow( 20.0, 2 ) )
    {
        if ( reverse_diff < angle_tolerance )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTurnIfNecessaryAndPossible) false: reverse facing" );
            return false;
        }

        if ( normal_diff > max_turn
             && reverse_diff < max_turn )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTurnIfNecessaryAndPossible) change to the reverse angle(1)" );
            target_body_angle = reverse_angle;
        }
        else if ( reverse_diff < normal_diff )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTurnIfNecessaryAndPossible) change to the reverse angle(2)" );
            target_body_angle = reverse_angle;
        }
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doTurnIfNecessaryAndPossible) turn to %.1f",
                  target_body_angle.degree() );

    doTurnLookBall( agent, target_body_angle );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleGoalie::doMove( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const Vector2D target_pos = getMoveTargetPosition( agent );
    const double tolerance = getMoveTargetTolerance( agent, target_pos );
    //const double dash_power = getMoveDashPower( agent, target_pos );

    agent->debugClient().setTarget( target_pos );
    agent->debugClient().addCircle( target_pos, tolerance );
    agent->debugClient().addLine( M_opponent_ball_pos, Vector2D( -SP.pitchHalfLength(), -SP.goalHalfWidth() ) );
    agent->debugClient().addLine( M_opponent_ball_pos, Vector2D( -SP.pitchHalfLength(), +SP.goalHalfWidth() ) );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doMove) target=(%.2f %.2f) tolerance=%.2f",
                  target_pos.x, target_pos.y, tolerance );

    //
    // turn
    //
    if ( doTurnIfOnTheBlockLine( agent, target_pos, tolerance, 12.5 ) )
    {
        agent->debugClient().addMessage( "GK:Turn(0)" );
        return true;
    }

    //
    // move
    //
    if ( doGoToPoint( agent,
                      target_pos,
                      tolerance ) )
    {
        agent->debugClient().addMessage( "GK:Move" );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doMove) move" );
        return true;
    }

    //
    // turn
    //
    if ( doTurnIfNecessaryAndPossible( agent, target_pos, 0.5, 12.5 ) )
    {
        agent->debugClient().addMessage( "GK:Turn(1)" );
        return true;
    }

    //
    // stop
    //
    if ( wm.ball().distFromSelf() < 30.0
         && wm.self().vel().rotatedVector( -wm.self().body() ).absX() >= 0.01 )
    {
        if ( wm.self().inertiaFinalPoint().dist2( target_pos ) > std::pow( tolerance, 2 ) )
        {
            if ( Body_StopDash( true ).execute( agent ) )
            {
                agent->debugClient().addMessage( "GK:Stop" );
                dlog.addText( Logger::ROLE,
                              __FILE__":(doMove) stop dash" );
                return true;
            }
        }
    }

    //
    // turn
    //

    if ( doTurnIfNecessaryAndPossible( agent, target_pos, 2.0, 5.0 ) )
    {
        agent->debugClient().addMessage( "GK:Turn(2)" );
        return true;
    }

    if ( ! wm.kickableOpponent()
         && wm.interceptTable()->opponentReachStep() >= 2  )
    {
        if ( doTurnIfNecessaryAndPossible( agent, target_pos, 2.0, 0.0 ) )
        {
            agent->debugClient().addMessage( "GK:Turn(3)" );
            return true;
        }
    }

    //
    //
    //

    dlog.addText( Logger::ROLE,
                  __FILE__":(doMove) failed all" );

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
RoleGoalie::getMoveTargetPosition( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    Vector2D result_pos = home_pos;

    dlog.addText( Logger::ROLE,
                  __FILE__":(getMoveTargetPosition) strategic_position=(%.2f %.2f)",
                  home_pos.x, home_pos.y );
    dlog.addCircle( Logger::ROLE,
                    result_pos, wm.self().playerType().maxCatchableDist(), "#FF0" );

    const int opponent_step = wm.interceptTable()->opponentReachStep();
    StaminaModel stamina = wm.self().staminaModel();
    //int self_step = FieldAnalyzer::predict_self_reach_cycle( wm, result_pos, 0.5, 0, true, &stamina );
    int forward_dash_step = MoveSimulator::simulate_self_turn_dash( wm, home_pos, 0.5, false, 30, NULL );
    int back_dash_step = MoveSimulator::simulate_self_turn_dash( wm, home_pos, 0.5, true, 30, NULL );
    int omni_dash_step = MoveSimulator::simulate_self_omni_dash( wm, home_pos, 0.5, 30, NULL );
    int self_step = std::min( std::min( forward_dash_step, back_dash_step ), omni_dash_step );

    dlog.addText( Logger::ROLE,
                  __FILE__":(getMoveTargetPosition) self_step=%d(f%d:b%d:o%d) opponent_step=%d",
                  self_step, opponent_step );

    agent->debugClient().addMessage( "Self%d:Opp%d", self_step, opponent_step );

    //
    // try body line target
    //
    if ( self_step > opponent_step )
    {
        Vector2D self_pos = wm.self().inertiaPoint( opponent_step );
        Line2D block_line( M_opponent_ball_pos, home_pos );
        if ( block_line.dist( self_pos ) > 1.5 )
        {
            Line2D body_line( self_pos, wm.self().body() );
            Vector2D intersection = body_line.intersection( block_line );
            if ( intersection.isValid()
                 && intersection.x < home_pos.x + 1.0
                 && intersection.x > -SP.pitchHalfLength() + 1.0 )
            {
                result_pos = intersection;
                self_step = FieldAnalyzer::predict_self_reach_cycle( wm, result_pos, 0.5, 0, true, &stamina );
                dlog.addText( Logger::ROLE,
                              __FILE__":(getMoveTargetPosition) change to body line target (%.2f %.2f)",
                              result_pos.x, result_pos.y );
                dlog.addText( Logger::ROLE,
                              __FILE__":(getMoveTargetPosition) body line target. self_step=%d opponent_step=%d",
                              self_step, opponent_step );
            }
        }
    }

    //
    // opposite side goal post
    //
    if ( self_step > opponent_step )
    {
        if ( std::fabs( result_pos.y - wm.self().pos().y ) > 2.0
             && M_opponent_ball_pos.dist2( SP.ourTeamGoalPos() ) < std::pow( 25.0, 2 )
             && ( result_pos.absY() < wm.self().pos().absY()
                  || result_pos.y * wm.self().pos().y < 0.0 ) )
        {
            if ( M_opponent_ball_pos.absY() > SP.goalHalfWidth() - 0.5
                 && wm.self().pos().y * M_opponent_ball_pos.y > 0.0 )
            {

            }
            else
            {
                dlog.addText( Logger::ROLE,
                              __FILE__":(getMoveTargetPosition) change the target to opposite side post" );
                //result_pos.x = -SP.pitchHalfLength() + 1.0;
                const Vector2D self_pos = wm.self().inertiaFinalPoint();
                result_pos.x = std::max( -SP.pitchHalfLength() + 1.0, self_pos.x );
                result_pos.x = std::min( result_pos.x, home_pos.x );

                result_pos.y = SP.goalHalfWidth() * ( wm.self().pos().y < 0.0 ? 1.0 : -1.0 );
                dlog.addText( Logger::ROLE,
                              __FILE__":(getMoveTargetPosition) new target=(%.2f %.2f)",
                              result_pos.x, result_pos.y );
                dlog.addCircle( Logger::ROLE,
                                result_pos, wm.self().playerType().maxCatchableDist(), "#990" );
            }
        }
    }

    return result_pos;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
RoleGoalie::getMoveDashPower( PlayerAgent * agent,
                              const Vector2D & target_pos )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const int self_reach_step = wm.interceptTable()->selfReachCycle();
    const int teammate_reach_step = wm.interceptTable()->teammateReachStep();
    const int opponent_reach_step = wm.interceptTable()->opponentReachStep();
    const int predict_step = std::min( opponent_reach_step,
                                       std::min( teammate_reach_step,
                                                 self_reach_step ) );
    const Vector2D ball_reach_pos = FieldAnalyzer::get_field_bound_predict_ball_pos( wm, predict_step, 0.5 );

    const double dist_diff = wm.self().pos().dist( target_pos );

    double dash_power = SP.maxDashPower();

    if ( wm.gameMode().type() != GameMode::PenaltyTaken_
         && dist_diff < 5.0
         && wm.ball().distFromSelf() > 30.0
         && wm.ball().pos().x > -20.0 )
    {
        dash_power = SP.maxDashPower() * 0.7;
        dlog.addText( Logger::ROLE,
                      __FILE__":(getMoveDashPower) (1)" );

        if ( teammate_reach_step < opponent_reach_step
             || wm.ball().pos().x >= 10.0 )
        {
            dash_power = SP.maxDashPower() * 0.3;
            dlog.addText( Logger::ROLE,
                          __FILE__":(getMoveDashPower) (2)" );
        }
    }

    if ( wm.gameMode().type() != GameMode::PenaltyTaken_ )
    {
        if ( ball_reach_pos.x < -30.0
             && std::fabs( wm.self().pos().y - target_pos.y ) > 4.0
             && std::fabs( wm.self().pos().x - target_pos.x ) < 5.0 )
        {

        }
        else if  ( ball_reach_pos.x > -10.0
                   && wm.ball().pos().x > 10.0
                   && wm.ourDefenseLineX() > -20.0
                   && std::fabs( wm.self().pos().x - target_pos.x ) < 5.0 )
        {
            dash_power = std::min( dash_power, SP.maxDashPower() * 0.5 );
            dlog.addText( Logger::ROLE,
                          __FILE__":(getMoveDashPower) (3)" );
        }
        else if ( ball_reach_pos.x > -20.0
                  && wm.ball().pos().x > +20.0
                  && wm.ourDefenseLineX() > -25.0
                  && teammate_reach_step < opponent_reach_step
                  && std::fabs( wm.self().pos().x - target_pos.x ) < 5.0 )
        {
            dash_power = std::min( dash_power, SP.maxDashPower() * 0.6 );
            dlog.addText( Logger::ROLE,
                          __FILE__":(getMoveDashPower) (4)" );
        }
    }

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
            if ( wm.ball().pos().x > ServerParam::i().ourPenaltyAreaLineX() + 10.0 )
            {
                dash_power = std::min( dash_power, wm.self().playerType().staminaIncMax() );
                dlog.addText( Logger::ROLE,
                              __FILE__":(getMoveDashPower) (5)" );
            }
        }
    }

    dash_power = wm.self().getSafetyDashPower( dash_power );

    return dash_power;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
RoleGoalie::getMoveTargetTolerance( PlayerAgent * agent,
                                    const Vector2D & target_pos )
{
    const WorldModel & wm = agent->world();

    (void)target_pos;

    double result_value = ( wm.ball().pos().dist2( ServerParam::i().ourTeamGoalPos() ) < std::pow( 20.0, 2 )
                            ? wm.ball().distFromSelf() * 0.05
                            : wm.ball().distFromSelf() * 0.1 );
    result_value = std::max( 0.3, result_value );

    return result_value;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleGoalie::doGoToPoint( PlayerAgent * agent,
                         const Vector2D & target_pos,
                         const double tolerance )
{
    dlog.addText( Logger::ROLE,
                  __FILE__":(doGoToPoint) target=(%.2f %.2f) tolerance=%.2f",
                  target_pos.x, target_pos.y, tolerance );

    const WorldModel & wm = agent->world();

    if ( wm.self().pos().dist2( target_pos ) < std::pow( tolerance, 2 )
         || ( wm.self().pos() + wm.self().vel() ).dist2( target_pos ) < std::pow( tolerance, 2 ) )
    {
        return false;
    }

    MoveSimulator::Result turn_dash_result;
    MoveSimulator::Result turn_back_dash_result;
    MoveSimulator::Result omni_dash_result;
    int turn_dash_step = MoveSimulator::simulate_self_turn_dash( wm, target_pos, tolerance, false,
                                                                 30, &turn_dash_result );
    int turn_back_dash_step = MoveSimulator::simulate_self_turn_dash( wm, target_pos, tolerance, true,
                                                                      20, &turn_back_dash_result );
    int omni_dash_step = MoveSimulator::simulate_self_omni_dash( wm, target_pos, tolerance,
                                                                 30, &omni_dash_result );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doGoToPoint) (turn_dash) step=%d (turn=%d dash=%d) stamina=%.1f",
                  turn_dash_step, turn_dash_result.turn_step_, turn_dash_result.dash_step_,
                  turn_dash_result.stamina_ );
    // dlog.addText( Logger::ROLE,
    //               __FILE__": -----> turn=%.1f dash_power=%.1f dash_dir=%.1f",
    //               turn_dash_result.turn_moment_,
    //               turn_dash_result.dash_power_, turn_dash_result.dash_dir_ );
    // dlog.addText( Logger::ROLE,
    //               __FILE__": -----> body_angle=%.1f",
    //               turn_dash_result.body_angle_.degree() );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGoToPoint) (turn_back_dash) step=%d (turn=%d dash=%d) stamina=%.1f",
                  turn_back_dash_step, turn_back_dash_result.turn_step_, turn_back_dash_result.dash_step_,
                  turn_back_dash_result.stamina_ );
    // dlog.addText( Logger::ROLE,
    //               __FILE__": -----> turn=%.1f dash_power=%.1f dash_dir=%.1f",
    //               turn_back_dash_result.turn_moment_,
    //               turn_back_dash_result.dash_power_, turn_back_dash_result.dash_dir_ );
    // dlog.addText( Logger::ROLE,
    //               __FILE__": -----> body_angle=%.1f",
    //               turn_back_dash_result.body_angle_.degree() );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGoToPoint) (omni_dash) step=%d (turn=%d dash=%d) stamina=%.1f",
                  omni_dash_step, omni_dash_result.turn_step_, omni_dash_result.dash_step_,
                  omni_dash_result.stamina_ );
    // dlog.addText( Logger::ROLE,
    //               __FILE__": -----> turn=%.1f dash_power=%.1f dash_dir=%.1f",
    //               omni_dash_result.turn_moment_,
    //               omni_dash_result.dash_power_, omni_dash_result.dash_dir_ );
    // dlog.addText( Logger::ROLE,
    //               __FILE__": -----> body_angle=%.1f",
    //               omni_dash_result.body_angle_.degree() );


    if ( wm.ball().pos().dist2( ServerParam::i().ourTeamGoalPos() ) > std::pow( 30.0, 2 )
         && M_opponent_ball_pos.dist2( ServerParam::i().ourTeamGoalPos() ) > std::pow( 30.0, 2 ) )
    {
        if ( turn_dash_step >= 0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doGoToPoint) turn forward dash. safe(1)" );
            if ( doTurnDash( agent, turn_dash_result ) )
            {
                return true;
            }
        }
    }

    const int opponent_step = wm.interceptTable()->opponentReachStep();
    const double opponent_ball_dist = wm.getDistOpponentNearestToBall( 5 );

    if ( turn_dash_step >= 0
         && opponent_step >= turn_dash_step + 2
         && opponent_ball_dist > 3.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGoToPoint) turn forward dash. safe(2) self=%d opponent=%d",
                      turn_dash_step, opponent_step );
        if ( doTurnDash( agent, turn_dash_result ) )
        {
            return true;
        }
    }

    const AngleDeg target_body_angle = get_target_body_angle( target_pos, M_opponent_ball_pos );

    const double omni_dash_angle_diff
        = std::min( ( wm.self().body() - target_body_angle ).abs(),
                    180.0 - ( wm.self().body() - target_body_angle ).abs() );
    const double turn_dash_angle_diff
        = std::min( ( turn_dash_result.body_angle_ - target_body_angle ).abs(),
                    180.0 - ( turn_dash_result.body_angle_ - target_body_angle ).abs() );
    const double turn_back_dash_angle_diff
        = std::min( ( turn_back_dash_result.body_angle_ - target_body_angle ).abs(),
                    180.0 - ( turn_back_dash_result.body_angle_ - target_body_angle ).abs() );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGoToPoint) target_body_angle=%.1f current=%.1f",
                  target_body_angle.degree(), wm.self().body().degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doGoToPoint) angle_diff: foward=%.1f back=%.1f omni=%.1f",
                  turn_dash_angle_diff, turn_back_dash_angle_diff, omni_dash_angle_diff );

    if ( omni_dash_step >= 0
         && omni_dash_step <= wm.interceptTable()->opponentReachStep()
         && omni_dash_angle_diff < 15.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGoToPoint) omni dash (1)" );
        if ( doTurnDash( agent, omni_dash_result ) )
        {
            return true;
        }
    }

    if ( turn_dash_step >= 0
         && turn_dash_step <= wm.interceptTable()->opponentReachStep()
         && turn_dash_angle_diff < 15.0 )
    {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doGoToPoint) turn forward dash (2)" );
        if ( doTurnDash( agent, turn_dash_result ) )
        {
            return true;
        }
    }

    if ( turn_back_dash_step >= 0
         && turn_back_dash_step <= wm.interceptTable()->opponentReachStep()
         && turn_back_dash_angle_diff < 15.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGoToPoint) turn back dash (1)" );
        if ( doTurnDash( agent, turn_back_dash_result ) )
        {
            return true;
        }
    }


    //
    // TODO: compare reach step & body angle for looking at the ball
    //

    const AngleDeg ball_angle = ( M_opponent_ball_pos - target_pos ).th();
    const AngleDeg next_ball_angle = ( M_opponent_ball_pos - ( wm.self().pos() + wm.self().vel() ) ).th();

    double omni_dash_score = ( omni_dash_step < 0 ? 10000 : omni_dash_step + 1 );
    double turn_dash_score = ( turn_dash_step < 0 ? 10000 : turn_dash_step + 1 );
    double turn_back_dash_score = ( turn_back_dash_step < 0 ? 10000 : turn_back_dash_step + 1 );

    if ( omni_dash_step >= 0 )
    {
        if ( ( wm.self().body() - ball_angle ).abs() < 100.0 )
        {
            omni_dash_score -= 0.1;
        }
        if ( ( wm.self().body() - next_ball_angle ).abs() < 100.0 )
        {
            omni_dash_score -= 0.1;
        }
        omni_dash_score += 0.025 * omni_dash_angle_diff;
    }

    if ( turn_dash_step >= 0 )
    {
        turn_dash_score += std::fabs( turn_dash_result.turn_moment_ ) * 0.01;
        // if ( std::fabs( turn_dash_result.turn_moment_ ) > 90.0 )
        // {
        //     turn_dash_score += std::fabs( turn_dash_result.turn_moment_ ) * 0.05;
        // }
        if ( ( turn_dash_result.body_angle_ - ball_angle ).abs() < 100.0 )
        {
            turn_dash_score -= 0.1;
        }
        if ( ( turn_dash_result.body_angle_ - next_ball_angle ).abs() < 100.0 )
        {
            turn_dash_score -= 0.1;
        }
        turn_dash_score += 0.025 * turn_dash_angle_diff;
    }

    if ( turn_back_dash_step >= 0 )
    {
        turn_back_dash_score += std::fabs( turn_back_dash_result.turn_moment_ ) * 0.015;
        // if ( std::fabs( turn_back_dash_result.turn_moment_ ) > 90.0 )
        // {
        //     turn_back_dash_score += std::fabs( turn_back_dash_result.turn_moment_ ) * 0.05;
        // }
        if ( ( turn_back_dash_result.body_angle_ - ball_angle ).abs() < 100.0 )
        {
            turn_back_dash_score -= 0.1;
        }
        if ( ( turn_back_dash_result.body_angle_ - next_ball_angle ).abs() < 100.0 )
        {
            turn_back_dash_score -= 0.1;
        }
        turn_back_dash_score += 0.025 * turn_back_dash_angle_diff;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGoToPoint) compare score: omni=%.2f forward=%.2f back=%.2f",
                  omni_dash_score, turn_dash_score, turn_back_dash_score );

    if ( omni_dash_score <= turn_dash_score
         && omni_dash_score <= turn_back_dash_score )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGoToPoint) omni dash (2)" );
        if ( doTurnDash( agent, omni_dash_result ) )
        {
            return true;
        }
    }

    if ( turn_dash_score <= omni_dash_score
         && turn_dash_score < turn_back_dash_score )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGoToPoint) turn forward dash (3)" );
        if ( doTurnDash( agent, turn_dash_result ) )
        {
            return true;
        }
    }

    if ( turn_back_dash_score <= omni_dash_score
         && turn_back_dash_score <= turn_back_dash_score )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGoToPoint) turn back dash (2)" );
        if ( doTurnDash( agent, turn_back_dash_result ) )
        {
            return true;
        }
    }


    dlog.addText( Logger::ROLE,
                  __FILE__": (doGoToPoint) SaviorGoToPoint" );

    return Body_SaviorGoToPoint( target_pos, tolerance, ServerParam::i().maxDashPower(),
                                 true, true, true ).execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleGoalie::doTurnDash( PlayerAgent * agent,
                        const MoveSimulator::Result & result )
{
    if ( result.turn_step_ > 0 )
    {
        agent->debugClient().addMessage( "Turn:%.0f", result.turn_moment_ );
        agent->doTurn( result.turn_moment_ );
        return true;
    }

    if ( result.dash_step_ > 0 )
    {
        agent->debugClient().addMessage( "Dash:%.0f:%.0f",
                                         result.dash_power_, result.dash_dir_ );
        agent->doDash( result.dash_power_, result.dash_dir_ );
        return true;
    }

    return false;
}
