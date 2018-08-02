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

#include "bhv_center_back_danger_move.h"

#include "strategy.h"
#include "defense_system.h"
#include "mark_analyzer.h"
#include "field_analyzer.h"
#include "move_simulator.h"

#include "bhv_basic_move.h"
#include "bhv_find_player.h"
#include "bhv_get_ball.h"
#include "bhv_tackle_intercept.h"

#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_go_to_point_look_ball.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackDangerMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_CenterBackDangerMove" );

    //
    // intercept
    //
    if ( doIntercept( agent ) )
    {
        return true;
    }

    if ( doTackleIntercept( agent ) )
    {
        return true;
    }

    //
    // get ball
    //
    if ( doGetBall( agent ) )
    {
        return true;
    }

    //
    // mark
    //
    if ( doMarkMove( agent ) )
    {
        return true;
    }

    //
    // block shoot
    //
    if ( doBlockShoot( agent ) )
    {
        return true;
    }

    // if ( doGetBall2nd( agent ) )
    // {
    //     return true;
    // }

    //
    // normal move
    //
    doNormalMove( agent );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackDangerMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.kickableTeammate()
         || wm.kickableOpponent() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) exist ball kicker" );
        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    const double ball_speed = wm.ball().vel().r() * std::pow( ServerParam::i().ballDecay(), opp_min );

    bool intercept = false;

    if ( self_min <= opp_min + 1
         && self_min <= mate_min + 1 )
    {
        intercept = true;
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) true(1)" );
    }

    if ( ! intercept
         && self_min <= opp_min + 2
         && self_min <= mate_min + 1
         && ball_speed < 0.5 )
    {
        intercept = true;
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) true(2)" );
    }

    if ( self_min >= opp_min + 1
         && ball_speed > 1.5 )
    {
        intercept = false;
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) cancel" );
    }

    if ( intercept )
    {
        agent->debugClient().addMessage( "CB:Danger:Intercept" );
        Body_Intercept().execute( agent );

        if ( wm.ball().vel().x > 0.0
             && opp_min >= self_min + 3 )
        {
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        }
        else
        {
            agent->setNeckAction( new Neck_DefaultInterceptNeck
                                  ( new Neck_TurnToBallOrScan( 0 ) ) );
        }

        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackDangerMove::doTackleIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( FieldAnalyzer::is_ball_moving_to_our_goal( wm )
         || wm.ball().pos().dist2( ServerParam::i().ourTeamGoalPos() ) < std::pow( 15.0, 2 ) )
    {
        if ( Bhv_TackleIntercept().execute( agent ) )
        {
            agent->debugClient().addMessage( "CB:TackleIntercept" );
            return true;
        }
    }

    return false;
}

namespace {
/*-------------------------------------------------------------------*/
/*!

 */
bool
is_block_situation( const WorldModel & wm )
{
    const AbstractPlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(is_block_situation) false: no opponent" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    const int opponent_step = wm.interceptTable()->opponentReachStep();
    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    if ( wm.kickableTeammate()
         || opponent_ball_pos.x > -30.0
         || opponent_ball_pos.dist2( home_pos ) > std::pow( 16.0, 2 )
         || opponent_ball_pos.absY() > 20.0
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (is_block_situation) false: no get ball situation" );
        return false;
    }

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );
    if ( mark_target
         && mark_target->unum() == opponent->unum() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (is_block_situation) true: first opponent is the mark target" );
        return true;
    }

    if ( mark_target
         && ( mark_target->pos().dist( ServerParam::i().ourTeamGoalPos() )
              < opponent->pos().dist( ServerParam::i().ourTeamGoalPos() ) ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (is_block_situation) false: keep mark" );
        return false;
    }

    if ( opponent_ball_pos.dist2( ServerParam::i().ourTeamGoalPos() ) > std::pow( 20.0, 2 ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (is_block_situation) false: safe goal distance" );
        return false;
    }

    const Triangle2D goal_triangle( opponent_ball_pos,
                                    Vector2D( -ServerParam::i().pitchHalfLength(),
                                              -ServerParam::i().goalHalfWidth() ),
                                    Vector2D( -ServerParam::i().pitchHalfLength(),
                                              +ServerParam::i().goalHalfWidth() ) );

    const AbstractPlayerObject * marker = MarkAnalyzer::i().getMarkerOf( opponent->unum() );
    if ( marker
         && ( goal_triangle.contains( marker->pos() )
              || marker->pos().dist2( Vector2D( -45.0, 0.0 ) ) < opponent->pos().dist2( Vector2D( -45.0, 0.0 ) )
              //|| marker->pos().dist2( opponent->pos() ) < std::pow( 2.5, 2 ) )
              )
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (is_block_situation) false: opponent has other marker %d",
                      marker->unum() );
        return false;
    }

    int player_count = 0;
    for ( PlayerObject::Cont::const_iterator p = wm.teammates().begin(),
              end = wm.teammates().end();
          p != end;
          ++p )
    {
        if ( (*p)->goalie() ) continue;
        if ( (*p)->isGhost() ) continue;
        if ( (*p)->posCount() >= 10 ) continue;

        if ( goal_triangle.contains( (*p)->pos() ) )
        {
            ++player_count;
        }
    }

    if ( player_count >= 2 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (is_block_situation) false: maybe exist other blocker" );
        return false;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (is_block_situation) true" );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
is_goal_left_blocked( const WorldModel & wm )
{
    const Vector2D opponent_ball_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );
    const Triangle2D left_triangle( ServerParam::i().ourTeamGoalPos(),
                                    Vector2D( -ServerParam::i().pitchHalfLength(),
                                              -ServerParam::i().goalHalfWidth() ),
                                    opponent_ball_pos );
    size_t n_players = wm.countTeammatesIn( left_triangle, 5, true );
    if ( n_players >= 2 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(is_goal_left_blocked) true: exist two or more teammates in triangle" );
        return true;
    }

    // const Vector2D left_mid_pos( -ServerParam::i().pitchHalfLength(),
    //                              -ServerParam::i().goalHalfWidth()*0.5 );
    // const Segment2D mid_line( opponent_ball_pos, left_mid_pos );
    // for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(), end = wm.teammates().end();
    //       t != end;
    //       ++t )
    // {
    // }

    dlog.addText( Logger::ROLE,
                  __FILE__":(is_goal_left_blocked) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
is_goal_right_blocked( const WorldModel & wm )
{
    const Vector2D opponent_ball_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );
    const Triangle2D right_triangle( ServerParam::i().ourTeamGoalPos(),
                                     Vector2D( -ServerParam::i().pitchHalfLength(),
                                               +ServerParam::i().goalHalfWidth() ),
                                     opponent_ball_pos );
    size_t n_players = wm.countTeammatesIn( right_triangle, 5, true );
    if ( n_players >= 2 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(is_goal_right_blocked) true: exist two or more teammates in triangle" );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(is_goal_right_blocked) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_block_center_pos( const WorldModel & wm )
{
    //
    // estimate opponent ball position
    //

    const Vector2D opponent_ball_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );

    if ( opponent_ball_pos.absY() > ServerParam::i().goalHalfWidth() )
    {
        return Vector2D::INVALIDATED;
    }

#if 1
    const bool goal_left_blocked = is_goal_left_blocked( wm );
    const bool goal_right_blocked = is_goal_right_blocked( wm );

    if ( goal_left_blocked && ! goal_right_blocked )
    {
        const double goal_y = ServerParam::i().goalHalfWidth() - 0.5 - wm.self().playerType().kickableArea();
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_block_center_pos) block right side. y=%.2f", goal_y );
        return Vector2D( -ServerParam::i().pitchHalfLength(), goal_y );
    }
    if ( ! goal_left_blocked && goal_right_blocked )
    {
        const double goal_y = -ServerParam::i().goalHalfWidth() + 0.5 + wm.self().playerType().kickableArea();
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_block_center_pos) block left side. y=%.2f", goal_y );
        return Vector2D( -ServerParam::i().pitchHalfLength(), goal_y );
    }

#endif

    //
    // check our goalie
    //

    const AbstractPlayerObject * goalie = wm.getOurGoalie();
    if ( ! goalie
         || goalie->pos().x > ServerParam::i().ourPenaltyAreaLineX()
         || goalie->pos().absY() > ServerParam::i().goalHalfWidth() + 3.0 )
    {
        return Vector2D::INVALIDATED;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(get_block_center_pos) detect our goalie" );

    //
    // decide position to support our goalie
    //

    Vector2D center_pos = Vector2D::INVALIDATED;

    const Vector2D goal_l( -ServerParam::i().pitchHalfLength(),
                           -ServerParam::i().goalHalfWidth() );
    const Vector2D goal_r( -ServerParam::i().pitchHalfLength(),
                           +ServerParam::i().goalHalfWidth() );

    AngleDeg goalie_angle = ( goalie->pos() - opponent_ball_pos ).th();
    AngleDeg goal_l_angle = ( goal_l - opponent_ball_pos ).th();
    AngleDeg goal_r_angle = ( goal_r - opponent_ball_pos ).th();
    double diff_l = ( goalie_angle - goal_l_angle ).abs();
    double diff_r = ( goalie_angle - goal_r_angle ).abs();
    AngleDeg mid_angle = ( diff_l > diff_r
                           ? AngleDeg::bisect( goal_l_angle, goalie_angle )
                           : AngleDeg::bisect( goal_r_angle, goalie_angle ) );
    if ( mid_angle.abs() < 90.0 ) mid_angle += 180.0;

    dlog.addText( Logger::ROLE,
                  __FILE__":(get_block_center_pos) goalie_angle=%.1f goal_l_angle=%.1f goal_r_angle=%.1f",
                  goalie_angle.degree(), goal_l_angle.degree(), goal_r_angle.degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__":(get_block_center_pos) diff_l=%.3f diff_r=%.3f",
                  diff_l, diff_r );
    dlog.addText( Logger::ROLE,
                  __FILE__":(get_block_center_pos) mid_angle = %.1f", mid_angle.degree() );

    Line2D block_line( opponent_ball_pos, mid_angle );
    double center_x = -ServerParam::i().pitchHalfLength() + 1.0;
    double center_y = block_line.getY( center_x );

    if ( center_y != Line2D::ERROR_VALUE )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_block_center_pos) set center pos (%.2f %.2f)",
                      center_x, center_y );
        center_pos.assign( center_x, center_y );

        Segment2D block_segment( opponent_ball_pos, center_pos );
        if ( block_segment.contains( goalie->pos() )
             && block_segment.dist( goalie->pos() ) < goalie->playerTypePtr()->maxCatchableDist() + 0.5 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_block_center_pos) exist goalie on block line" );
            center_pos = Vector2D::INVALIDATED;
        }
    }

    return center_pos;
}
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackDangerMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( ! is_block_situation( wm ) )
    {
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    const double max_x = -36.0; // home_pos.x + 4.0;
    Rect2D bounding_rect( Vector2D( -60.0, home_pos.y - 12.0 ),
                          Vector2D( max_x, home_pos.y + 12.0 ) );
    // 2009-06-26
    if ( wm.ball().pos().x > -36.0 )
    {
        bounding_rect = Rect2D( Vector2D( -60.0, home_pos.y - 10.0 ),
                                Vector2D( wm.ball().pos().x, home_pos.y + 10.0 ) );
    }

    const int self_step = wm.interceptTable()->selfReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();
    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    // 2013-06-01
    Vector2D center_pos = get_block_center_pos( wm );

    //
    // block
    //

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGetBall) try get ball" );
    if ( Bhv_GetBall( bounding_rect, center_pos ).execute( agent ) )
    {
        return true;
    }

    //
    // force intercept
    //
    if ( opponent_ball_pos.x < -45.0
         && opponent_ball_pos.absY() < 10.0
         && self_step <= opponent_step + 3 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) force intercept" );
        if ( Body_Intercept().execute( agent ) )
        {
            agent->setNeckAction( new Neck_CheckBallOwner() );
            dlog.addText( Logger::ROLE,
                          __FILE__": (doGetBall) done" );
            return true;
        }
    }

    //
    // force move
    //
    Vector2D target_point( -47.0, 0.0 );
    if ( opponent_ball_pos.absY() > 5.0 )
    {
        target_point.y = 3.0 * sign( opponent_ball_pos.y );
    }

    double dist_tolerance = 0.5;
    if ( Body_GoToPoint( target_point, dist_tolerance, ServerParam::i().maxDashPower()
                         -1.0, // dash speed
                         1, true, 15.0
                         ).execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) force move. target=(%.2f %.2f)",
                      target_point.x, target_point.y );
    }
    else
    {
        Body_TurnToBall().execute( agent );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) force move. turn to ball" );

    }

    agent->setNeckAction( new Neck_CheckBallOwner() );

    agent->debugClient().addMessage( "CB:Emergency" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_tolerance );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackDangerMove::doMarkMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( mate_min <= opp_min - 5
         && wm.ball().vel().x > -0.1 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(doMarkMove) no mark situation." );
        return false;
    }

    //
    // search mark target opponent
    //
    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

    if ( ! mark_target )
    {
        // not found
        dlog.addText( Logger::ROLE,
                      __FILE__":(doMarkMove) mark not found" );
        return false;
    }

    const AbstractPlayerObject * fastest_opponent = wm.interceptTable()->fastestOpponent();
    if ( mark_target == fastest_opponent )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(doMarkMove) mark target is the first attacker." );
        return false;
    }

    dlog.addText( Logger::MARK,
                  __FILE__":(doMarkMove) mark target ghostCount=%d",
                  mark_target->ghostCount() );

    if ( mark_target->ghostCount() >= 5 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(doMarkMove) mark target is ghost." );
        return false;
    }

    if ( mark_target->ghostCount() >= 2 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(doMarkMove) find player." );
        Bhv_FindPlayer( mark_target, 0 ).execute( agent );
        return true;
    }


    Vector2D move_point = getMarkMoveTargetPoint( agent, mark_target );

    if ( ! move_point.isValid() )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(doMarkMove) illegal move point." );
        return false;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doMarkMove) mark point=(%.1f %.1f)",
                  move_point.x, move_point.y );

    double dash_power = ServerParam::i().maxDashPower();

    double dist_thr = wm.ball().distFromSelf() * 0.025;
    if ( dist_thr < 0.3 ) dist_thr = 0.3;

    agent->debugClient().addMessage( "CB:Mark" );
    agent->debugClient().setTarget( move_point );
    agent->debugClient().addCircle( move_point, dist_thr );


    dlog.addText( Logger::MARK,
                  __FILE__":(doMarkMove) pos=(%.2f %.2f) dist_thr=%.3f dash_power=%.1f",
                  move_point.x, move_point.y,
                  dist_thr, dash_power );

    if ( ! DefenseSystem::mark_go_to_point( agent, mark_target,
                                            move_point, dist_thr, dash_power, 20.0 ) )
    {
        DefenseSystem::mark_turn_to( agent, mark_target );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_CenterBackDangerMove::getMarkMoveTargetPoint( PlayerAgent * agent,
                                                  const AbstractPlayerObject * mark_target )
{
    const WorldModel & wm = agent->world();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PlayerType * ptype = mark_target->playerTypePtr();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    const double additional_move = std::min( 5, mark_target->posCount() ) * ptype->realSpeedMax();

    Vector2D mark_target_pos = mark_target->inertiaFinalPoint();

    if ( std::fabs( mark_target_pos.y - home_pos.y ) > 20.0 + additional_move
         || mark_target_pos.dist2( home_pos ) > std::pow( 20.0 + additional_move, 2 )
         || mark_target_pos.x > home_pos.x + 12.0 + additional_move )
    {
        dlog.addText( Logger::ROLE | Logger::MARK,
                      __FILE__":(getMarkMoveTargetPoint) too far mark target (%.2f %.2f) y_diff=%.3f dist=%.3f",
                      mark_target_pos.x, mark_target_pos.y,
                      std::fabs( mark_target_pos.y - home_pos.y ),
                      mark_target_pos.dist( home_pos ) );
        return Vector2D::INVALIDATED;
    }

    const Vector2D ball_pos = wm.ball().inertiaPoint( opp_min );

    Vector2D move_point = mark_target_pos;

    if ( mark_target->velCount() <= 1
         && mark_target->vel().th().abs() > 70.0
         && mark_target->vel().r2() > std::pow( ptype->realSpeedMax() * ptype->playerDecay() * 0.5, 2 ) )
    {
        Vector2D add_vec = mark_target->vel();
        add_vec /= ptype->playerDecay();
        add_vec *= 3.0;
        move_point += add_vec;

        move_point.x -= 0.8;

        dlog.addText( Logger::ROLE | Logger::MARK,
                      __FILE__":(getMarkMoveTargetPoint) mark_target running" );
        dlog.addText( Logger::MARK,
                      __FILE__":(getMarkMoveTargetPoint) mark_target=%d(%.2f %.2f) add_vec(%.3f %.3f) -> (%.2f %.2f)",
                      mark_target->unum(),
                      mark_target_pos.x, mark_target_pos.y,
                      add_vec.x, add_vec.y,
                      move_point.x, move_point.y );
    }
    else
    {
        move_point += mark_target->vel();
        move_point.x -= 2.0;

        dlog.addText( Logger::ROLE | Logger::MARK,
                      __FILE__":(getMarkMoveTargetPoint) x adjust default" );
        dlog.addText( Logger::ROLE | Logger::MARK,
                      __FILE__":(getMarkMoveTargetPoint) mark_target=%d(%.2f %.2f) x-=4  -> (%.2f %.2f)",
                      mark_target->unum(),
                      mark_target_pos.x, mark_target_pos.y,
                      move_point.x, move_point.y );
    }


    const double y_diff = ball_pos.y - move_point.y;

    dlog.addText( Logger::ROLE | Logger::MARK,
                  __FILE__":(getMarkMoveTargetPoint) mark_target=%d(%.1f %.1f)->(%.1f %.1f) base_target=(%.1f %.1f)",
                  mark_target->unum(),
                  mark_target->pos().x, mark_target->pos().y,
                  mark_target_pos.x, mark_target_pos.y,
                  move_point.x, move_point.y );

    if ( move_point.x < -36.0
         && move_point.absY() < 16.0 )
    {
        dlog.addText( Logger::ROLE | Logger::MARK,
                      __FILE__":(getMarkMoveTargetPoint) y adjust(0) in penalty area." );

        const AngleDeg ball_to_target_angle = ( mark_target_pos - ball_pos ).th();
        if ( ball_to_target_angle.abs() < 120.0 )
        {
            dlog.addText( Logger::ROLE | Logger::MARK,
                          __FILE__":(getMarkMoveTargetPoint) y adjust(0) in penalty area. block cross" );
            move_point.y += 0.1 * sign( y_diff );
        }
    }
    else if ( std::fabs( y_diff ) < 3.0 )
    {
        dlog.addText( Logger::ROLE | Logger::MARK,
                      __FILE__":(getMarkMoveTargetPoint) y adjust(1) no adjust" );
    }
    else if ( std::fabs( y_diff ) < 7.0 )
    {
        move_point.y += 0.45 * sign( y_diff );
        //move_point.y += 0.75 * sign( y_diff );
        dlog.addText( Logger::ROLE | Logger::MARK,
                      __FILE__":(getMarkMoveTargetPoint) y adjust(2) %f", 0.45 * sign( y_diff ) );
    }
    else
    {
        move_point.y += 0.9 * sign( y_diff );
        //move_point.y += 1.25 * sign( y_diff );
        dlog.addText( Logger::ROLE | Logger::MARK,
                      __FILE__":(getMarkMoveTargetPoint) y adjust(3) %f", 0.9 * sign( y_diff ) );
    }


    const double min_x = ( ball_pos.x > 0.0
                           ? -15.0
                           : ball_pos.x > -12.5
                           ? -23.5
                           : ball_pos.x > -21.5
                           ? -30.0
                           : ball_pos.x > -27.5
                           ? -38.5
                           : -40.0 );

    if ( move_point.x < min_x )
    {
        dlog.addText( Logger::ROLE | Logger::MARK,
                      __FILE__":(getMarkMoveTargetPoint) x adjust (2) min_x=%.1f",
                      min_x);
        move_point.x = min_x;

        if ( mark_target_pos.x < -36.5 )
        {
            dlog.addText( Logger::ROLE | Logger::MARK,
                          __FILE__":(getMarkMoveTargetPoint) x adjust in penalty area." );
            move_point.x = mark_target_pos.x - 0.4;
        }
        else if ( move_point.x > mark_target_pos.x + 0.6 )
        {
            dlog.addText( Logger::ROLE | Logger::MARK,
                          __FILE__":(getMarkMoveTargetPoint) x adjust. adjust to opponent x."
                          " min_x=%.2f target_x=%.2f mark_target_x=%.2f",
                          min_x, move_point.x, mark_target_pos.x );
            move_point.x = mark_target_pos.x + 0.6;
        }
    }

    if ( move_point.x > home_pos.x )
    {
#if 1
        dlog.addText( Logger::ROLE | Logger::MARK,
                      __FILE__":(getMarkMoveTargetPoint) x adjust to home. move_x=%.2f home_x=%.2f",
                      move_point.x, home_pos.x );
        move_point.x = ( home_pos.x + move_point.x ) * 0.5;
#else
        if ( move_point.x < -36.5 )
        {
            dlog.addText( Logger::ROLE | Logger::MARK,
                          __FILE__":(getMarkMoveTargetPoint) x adjust, to home, in penalty area" );
            move_point.x = std::min( home_pos.x + 3.0, move_point.x );
        }
        else
        {
            dlog.addText( Logger::ROLE | Logger::MARK,
                          __FILE__":(getMarkMoveTargetPoint) x adjust (%.2f %.2f) to home_x=(%.2f %.2f)",
                          move_point.x, move_point.y,
                          home_pos.x, home_pos.y );
            move_point.x = home_pos.x;
        }
#endif
    }

    //
    // adjust y for the home position
    //
#if 0
    // 2013-06-09
    if ( std::fabs( home_pos.y - move_point.y ) > 10.0 )
    {
        move_point.y = home_pos.y + ( move_point.y - home_pos.y )
            * 10.0 / std::fabs( home_pos.y - move_point.y );
        dlog.addText( Logger::ROLE | Logger::MARK,
                      __FILE__":(getMarkMoveTargetPoint) adjust y to home_pos.y %.2f", move_point.y );

    }
#else
    // 2016-06-27
    const double y_thr = 5.0;
    if ( std::fabs( home_pos.y - move_point.y ) > y_thr )
    {
        const double dir
            = ( move_point.y - home_pos.y )
            / std::fabs( home_pos.y - move_point.y );
        move_point.y = home_pos.y + y_thr * dir;

        dlog.addText( Logger::ROLE | Logger::MARK,
                      __FILE__":(getMarkMoveTargetPoint) adjust y to home_pos.y %.2f", move_point.y );

    }
#endif

    return move_point;
}

namespace {
/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_shoot_block_goal_pos( const WorldModel & wm,
                          const Vector2D & opponent_ball_pos )
{
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    Vector2D goal_base_point( - ServerParam::i().pitchHalfLength(), 0.0 );
    if ( position_type == Position_Left )
    {
        if ( opponent_ball_pos.y < 0.0 )
        {
            goal_base_point.y = -1.0;
        }
        else
        {
            goal_base_point.y = -ServerParam::i().goalHalfWidth() + 0.5;
        }
    }
    else if ( position_type == Position_Right )
    {
        if ( opponent_ball_pos.y > 0.0 )
        {
            goal_base_point.y = +1.0;
        }
        else
        {
            goal_base_point.y = ServerParam::i().goalHalfWidth() - 0.5;
        }
    }
    else
    {
        if ( opponent_ball_pos.y > 0.0 )
        {
            goal_base_point.y = -ServerParam::i().goalHalfWidth() + 1.5;
        }
        else
        {
            goal_base_point.y = +ServerParam::i().goalHalfWidth() - 1.5;
        }
    }

    return goal_base_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_block_shoot_pos_2012( PlayerAgent * agent,
                          const Vector2D & opponent_ball_pos )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D goal_base_point = get_shoot_block_goal_pos( wm, opponent_ball_pos );

    Vector2D block_point( -47.0, 0.0 );

    Line2D block_line( opponent_ball_pos, goal_base_point );
    double tmp_x = block_line.getX( home_pos.y );

    if ( - ServerParam::i().pitchHalfLength() + 0.5 < tmp_x
         && tmp_x < home_pos.x + 1.0 )
    {
        block_point.x = tmp_x;
        dlog.addText( Logger::ROLE,
                      __FILE__": (get_block_shoot_pos_2012) update x to %.3f",
                      tmp_x );
    }

    block_point.y = block_line.getY( block_point.x );

    dlog.addText( Logger::ROLE,
                  __FILE__": (get_block_shoot_pos_2012) target=(%.3f %.3f)",
                  block_point.x, block_point.y );

    block_point.y += ( goal_base_point.y < 0.0
                       ? + wm.self().playerType().kickableArea()
                       : - wm.self().playerType().kickableArea() );

    dlog.addText( Logger::ROLE,
                  __FILE__": (get_block_shoot_pos_2012) y adjusted (%.3f %.3f)",
                  block_point.x, block_point.y );
    dlog.addLine( Logger::ROLE,
                  opponent_ball_pos, goal_base_point, "#F00" );

    return block_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_block_shoot_pos_2013( PlayerAgent * agent,
                          const Vector2D & opponent_ball_pos,
                          MoveSimulator::Result * result )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D goal_base_point = get_shoot_block_goal_pos( wm, opponent_ball_pos );

    Vector2D block_point( -47.0, 0.0 );

    Line2D block_line( opponent_ball_pos, goal_base_point );
    double tmp_x = block_line.getX( home_pos.y );

    if ( - ServerParam::i().pitchHalfLength() + 0.5 < tmp_x
         && tmp_x < home_pos.x + 1.0 )
    {
        block_point.x = tmp_x;
        dlog.addText( Logger::ROLE,
                      __FILE__": (get_block_shoot_pos_2013) update x to %.2f",
                      tmp_x );
    }

    block_point.y = block_line.getY( block_point.x );

    dlog.addText( Logger::ROLE,
                  __FILE__": (get_block_shoot_pos_2013) target=(%.2f %.2f)",
                  block_point.x, block_point.y );

    block_point.y += ( goal_base_point.y < 0.0
                       ? + wm.self().playerType().kickableArea()
                       : - wm.self().playerType().kickableArea() );

    dlog.addText( Logger::ROLE,
                  __FILE__": (get_block_shoot_pos_2013) y adjusted (%.2f %.2f)",
                  block_point.x, block_point.y );
    dlog.addLine( Logger::ROLE,
                  opponent_ball_pos, goal_base_point, "#F00" );


    const int opponent_step = wm.interceptTable()->opponentReachStep();
    const double dist_step = 0.5;
    const double max_dist = opponent_ball_pos.dist( block_point );
    const Vector2D unit_vec = ( block_point - opponent_ball_pos ).setLengthVector( 1.0 );
    const double tolerance = 0.5;

    for ( double dist = 2.5; dist < max_dist; dist += dist_step )
    {
        const Vector2D pos = opponent_ball_pos + unit_vec * dist;
        const int ball_step = ServerParam::i().ballMoveStep( ServerParam::i().ballSpeedMax(), dist );

        const int max_step = opponent_step + ball_step + 1;

        if ( MoveSimulator::self_can_reach_after_turn_dash( wm, pos, tolerance, false,
                                                            max_step, result ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (get_block_shoot_pos_2013) found forwad. step=%d(o:%d b:%d) pos=(%.2f %.2f)",
                          max_step, opponent_step, ball_step, pos.x, pos.y );
            block_point = pos;
            break;
        }

        if ( MoveSimulator::self_can_reach_after_turn_dash( wm, pos, tolerance, true,
                                                            max_step, result ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (get_block_shoot_pos_2013) found back. step=%d(o:%d b:%d) pos=(%.2f %.2f)",
                          max_step, opponent_step, ball_step, pos.x, pos.y );
            block_point = pos;
            break;
        }

        int omni_step = MoveSimulator::simulate_self_omni_dash( wm, pos, tolerance, max_step, result );
        if ( omni_step >= 0
             && omni_step <= max_step )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (get_block_shoot_pos_2013) found omni. step=%d(o:%d b:%d) pos=(%.2f %.2f)",
                          max_step, opponent_step, ball_step, pos.x, pos.y );
            block_point = pos;
            break;
        }
    }

    return block_point;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackDangerMove::doBlockShoot( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int teammate_step = wm.interceptTable()->teammateReachCycle();
    const int opponent_step = wm.interceptTable()->opponentReachCycle();

    if ( teammate_step <= opponent_step - 3 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) our ball" );
        return false;
    }

    if ( wm.self().pos().x > -36.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) my position is not good for shoot block" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D opponent_ball_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );

    if ( opponent_ball_pos.x < home_pos.x )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) failed" );
        return false;
    }

    MoveSimulator::Result result;
    //const Vector2D block_point = get_block_shoot_pos_2012( agent, opponent_ball_pos );
    const Vector2D block_point = get_block_shoot_pos_2013( agent, opponent_ball_pos, &result );

    double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );
    double dist_thr = std::max( 0.4, wm.self().pos().dist( opponent_ball_pos ) * 0.1 );

    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, 1.0 );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doBlockShoot) result turn=%d dash=%d",
                  result.turn_step_, result.dash_step_ );

    if ( result.turn_step_ > 0 )
    {
        agent->debugClient().addMessage( "CB:Danger:BlockShoot:GoTurn" );
        agent->doTurn( result.turn_moment_ );
    }
    else if ( result.dash_step_ > 0 )
    {
        agent->debugClient().addMessage( "CB:Danger:BlockShoot:GoDash" );
        agent->doDash( result.dash_power_, result.dash_dir_ );
    }
    else if ( Body_GoToPoint( block_point,
                              dist_thr,
                              dash_power,
                              -1.0, // dash speed
                              1, // cycle
                              true, // save recovery
                              15.0 // dir thr
                              ).execute( agent ) )
    {
        agent->debugClient().addMessage( "CB:Danger:BlockShoot:Go%.0f",
                                         dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) go to (%.2f %.2f)",
                      block_point.x, block_point.y );
    }
    else
    {
        Vector2D face_point( wm.self().pos().x, wm.ball().pos().y );

        agent->debugClient().addMessage( "CB:Danger:BlockShoot:TurnTo" );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) already there" );
        Body_TurnToPoint( face_point ).execute( agent );
    }

    agent->setNeckAction( new Neck_CheckBallOwner() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackDangerMove::doGetBall2nd( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );
    if ( ! fastest_opp
         || ! mark_target )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall2nd) no target" );
        return false;
    }

    if ( mark_target != fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall2nd) mark target is not the fastest player." );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D ball_pos = wm.ball().inertiaPoint( wm.interceptTable()->opponentReachStep() );

    if ( home_pos.dist2( ball_pos ) > std::pow( 10.0, 2 ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall2nd) too far." );
        return false;
    }

    agent->debugClient().addMessage( "CB:Danger:GetBall2nd" );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doGetBall2nd) try intercept." );

    Body_Intercept().execute( agent );
    agent->setNeckAction( new Neck_TurnToBall() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_CenterBackDangerMove::doNormalMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    Vector2D next_ball_pos = wm.ball().pos() + wm.ball().vel();


    const double ball_xdiff = next_ball_pos.x  - next_self_pos.x;

    if ( ball_xdiff > 10.0
         && ( wm.kickableTeammate()
              || mate_min < opp_min - 1
              || self_min < opp_min - 1 )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doNormalMove) ball is front and our team keep ball" );
        Bhv_BasicMove().execute( agent );
        return;
    }

    double dash_power = DefenseSystem::get_defender_dash_power( wm, home_pos );

    double dist_thr = next_self_pos.dist( next_ball_pos ) * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "CB:Danger:Normal" );
    agent->debugClient().setTarget( home_pos );
    agent->debugClient().addCircle( home_pos, dist_thr );
    dlog.addText( rcsc::Logger::TEAM,
                  __FILE__": go home (%.1f %.1f) dist_thr=%.2f power=%.1f",
                  home_pos.x, home_pos.y,
                  dist_thr,
                  dash_power );

    doGoToPoint( agent, home_pos, dist_thr, dash_power, 12.0 );

    //agent->setNeckAction( new rcsc::Neck_TurnToBallOrScan() );
    agent->setNeckAction( new Neck_CheckBallOwner() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_CenterBackDangerMove::doGoToPoint( PlayerAgent * agent,
                                       const Vector2D & target_point,
                                       const double & dist_thr,
                                       const double & dash_power,
                                       const double & dir_thr )
{
    const WorldModel & wm = agent->world();

    if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                         -1.0, // dash speed
                         1, // 1 step
                         true, // save recovery
                         dir_thr
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Go%.1f", dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__": GoToPoint (%.1f %.1f) dash_power=%.1f dist_thr=%.2f",
                      target_point.x, target_point.y,
                      dash_power,
                      dist_thr );
        return;
    }

    // already there

    int min_step = std::min( wm.interceptTable()->teammateReachCycle(),
                             wm.interceptTable()->opponentReachCycle() );

    Vector2D ball_pos = wm.ball().inertiaPoint( min_step );
    Vector2D self_pos = wm.self().inertiaFinalPoint();
    AngleDeg ball_angle = ( ball_pos - self_pos ).th();

    AngleDeg target_angle = ball_angle + 90.0;
    if ( ball_pos.x < -47.0 )
    {
        if ( target_angle.abs() > 90.0 )
        {
            target_angle += 180.0;
        }
    }
    else
    {
        if ( target_angle.abs() < 90.0 )
        {
            target_angle += 180.0;
        }
    }

    Body_TurnToAngle( target_angle ).execute( agent );

    agent->debugClient().addMessage( "TurnTo%.0f",
                                     target_angle.degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__": TurnToAngle %.1f",
                  target_angle.degree() );
}
