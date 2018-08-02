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

#include "bhv_sweeper_danger_move.h"

#include "strategy.h"
#include "defense_system.h"
#include "field_analyzer.h"
#include "move_simulator.h"

#include "bhv_basic_move.h"
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
#include <rcsc/geom/line_2d.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/geom/matrix_2d.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

using namespace rcsc;

namespace {

//
const double g_shoot_block_base_x = -47.0;


/*-------------------------------------------------------------------*/
/*!

 */
int
get_move_step_to_stop_shoot( const WorldModel & wm,
                             const int opponent_step,
                             const Vector2D & first_ball_pos,
                             const Vector2D & target_pos,
                             Vector2D * block_pos )
{
    const PlayerType & ptype = wm.self().playerType();

    MoveSimulator::Result result;

    Vector2D ball_pos = first_ball_pos;
    Vector2D ball_vel = ( target_pos - ball_pos ).setLengthVector( ServerParam::i().ballSpeedMax() );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    double min_dist2 = 1000000.0;
    Vector2D best_pos;
    int best_step = -1;

    for ( int step = 1; step <= 20; ++step )
    {
        ball_pos += ball_vel;
        if ( ball_pos.x < -ServerParam::i().pitchHalfLength() - 0.1 )
        {
            return -1;
        }
        ball_vel *= ServerParam::i().ballDecay();

        dlog.addText( Logger::ROLE,
                      __FILE__": (get_move_step_to_stop_shoot) step=%d ball=(%.2f %.2f)",
                      step, ball_pos.x, ball_pos.y );
        dlog.addRect( Logger::ROLE,
                      ball_pos.x - 0.05, ball_pos.y - 0.05, 0.1, 0.1, "#0F0" );

        if ( MoveSimulator::self_can_reach_after_turn_dash( wm, ball_pos,
                                                            ptype.kickableArea(),
                                                            false, // forward dash
                                                            step + opponent_step,
                                                            &result ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (get_move_step_to_stop_shoot) step=%d ok forward, result_step=%d",
                          step, step + opponent_step );
            double d2 = home_pos.dist2( ball_pos );
            if ( d2 < min_dist2 )
            {
                min_dist2 = d2;
                best_pos = ball_pos;
                best_step = step + opponent_step;
            }
        }
        else if ( MoveSimulator::self_can_reach_after_turn_dash( wm, ball_pos,
                                                                 ptype.kickableArea(),
                                                                 true, // back dash
                                                                 step + opponent_step,
                                                                 &result ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (get_move_step_to_stop_shoot) step=%d ok back, result_step=%d",
                          step, step + opponent_step );
            double d2 = home_pos.dist2( ball_pos );
            if ( d2 < min_dist2 )
            {
                min_dist2 = d2;
                best_pos = ball_pos;
                best_step = step + opponent_step;
            }
        }
    }

    if ( block_pos )
    {
        *block_pos = best_pos;
    }

    return best_step;
}

/*-------------------------------------------------------------------*/
/*!

 */
Line2D
get_shoot_block_line( const WorldModel & wm )
{
    const Vector2D goal_l( -ServerParam::i().pitchHalfLength(), -ServerParam::i().goalHalfWidth() + 0.5 );
    const Vector2D goal_r( -ServerParam::i().pitchHalfLength(), +ServerParam::i().goalHalfWidth() - 0.5 );

    const Vector2D opponent_ball_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );

    const AngleDeg base_angle_l = ( goal_l - opponent_ball_pos ).th();
    const AngleDeg base_angle_r = ( goal_r - opponent_ball_pos ).th();
    const Line2D base_line_l( opponent_ball_pos, goal_l );
    const Line2D base_line_r( opponent_ball_pos, goal_r );

    const Line2D block_line_l( goal_l + Vector2D::from_polar( wm.self().playerType().kickableArea() + 0.3,
                                                              base_angle_l - 90.0 ),
                               base_angle_l );
    const Line2D block_line_r( goal_r + Vector2D::from_polar( wm.self().playerType().kickableArea() + 0.3,
                                                              base_angle_r + 90.0 ),
                               base_angle_r );
#if 0
    //
    // debug
    //
    {
        dlog.addLine( Logger::ROLE, opponent_ball_pos, ServerParam::i().ourTeamGoalPos(), "#F00" );
        dlog.addLine( Logger::ROLE, opponent_ball_pos, goal_l, "#F00" );
        dlog.addLine( Logger::ROLE, opponent_ball_pos, goal_r, "#F00" );

        Vector2D proj1 = block_line_l.projection( goal_l );
        Vector2D proj2 = block_line_l.projection( opponent_ball_pos );
        if ( proj1.isValid() && proj2.isValid() )
        {
            dlog.addLine( Logger::ROLE, proj1, proj2, "#800" );
        }
        proj1 = block_line_r.projection( goal_r );
        proj2 = block_line_r.projection( opponent_ball_pos );
        if ( proj1.isValid() && proj2.isValid() )
        {
            dlog.addLine( Logger::ROLE, proj1, proj2, "#800" );
        }
    }
#endif

    //
    // return the line
    //

    const AbstractPlayerObject * goalie = wm.getOurGoalie();

    const AngleDeg goalie_angle = ( goalie
                                    ? ( goalie->pos() - opponent_ball_pos ).th()
                                    : 0.0 );
    if ( opponent_ball_pos.x < g_shoot_block_base_x )
    {
        if ( goalie )
        {
            if ( ( goalie_angle - base_angle_l ).abs() > ( goalie_angle - base_angle_r ).abs() )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__":(get_shoot_block_line) goal line defense. detect goalie.  left base line" );
                return base_line_l;
            }
            else
            {
                dlog.addText( Logger::ROLE,
                              __FILE__":(get_shoot_block_line) goal line defense. detect goalie. right base line" );
                return base_line_r;
            }
        }
    }

    //
    //
    //
    if ( goalie )
    {
        if ( goalie->pos().y > 2.0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_shoot_block_line) detect goalie. y>+2. block left line(1)" );
            return block_line_l;
        }
        if ( goalie->pos().y < -2.0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_shoot_block_line) detect goalie. y<-2. block right line(1)" );
            return block_line_r;
        }

        if ( ( goalie_angle - base_angle_l ).abs() > ( goalie_angle - base_angle_r ).abs() )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_shoot_block_line) detect goalie. block left line" );
            return block_line_l;
        }
        else
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_shoot_block_line) detect goalie. block right line" );
            return block_line_r;
        }
    }

    //
    //
    //
    if ( opponent_ball_pos.y > 0.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_shoot_block_line) no goalie. block left line" );
        return block_line_l;
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_shoot_block_line) no goalie. block right line" );
        return block_line_r;
    }
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
opponent_is_blocked( const WorldModel & wm,
                     const PlayerObject * opponent,
                     const Vector2D & ball_pos )
{
    const Segment2D goal_line( ball_pos, ServerParam::i().ourTeamGoalPos() );
    const Vector2D goal_l( -ServerParam::i().pitchHalfLength(), -ServerParam::i().goalHalfWidth() );
    const Vector2D goal_r( -ServerParam::i().pitchHalfLength(), +ServerParam::i().goalHalfWidth() );
    const AngleDeg goal_l_angle = ( goal_l - ball_pos ).th();
    const AngleDeg goal_r_angle = ( goal_r - ball_pos ).th();

    for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(), end = wm.teammates().end();
          t != end;
          ++t )
    {
        if ( (*t)->goalie() ) continue;
        if ( (*t)->posCount() >= 3 ) continue;
        if ( (*t)->pos().x > opponent->pos().x ) continue;

        const AngleDeg angle_from_ball = ( (*t)->pos() - ball_pos ).th();
        if ( angle_from_ball.isWithin( goal_r_angle, goal_l_angle )
             || goal_line.dist( (*t)->pos() ) < (*t)->playerTypePtr()->kickableArea() )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (opponent_is_blocked) blocked by %d", (*t)->unum() );
            return true;
        }
    }

    return false;
}
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SweeperDangerMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_SweeperDangerMove" );

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
    // prepare shoot block
    //
    if ( doPrepareBlockShoot( agent ) )
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
    // block shoot
    //
    if ( doBlockShoot( agent ) )
    {
        return true;
    }

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
Bhv_SweeperDangerMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.kickableTeammate()
         || wm.kickableOpponent() )
    {
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
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true(1)" );
        intercept = true;

    }

    if ( ! intercept
         && self_min <= opp_min + 2
         && self_min <= mate_min + 1
         && ball_speed < 0.5 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true(2)" );
        intercept = true;
    }

#if 1
    if ( intercept
         && opp_min < self_min )
    {
        MoveSimulator::Result result;
        Vector2D ball_pos = wm.ball().inertiaPoint( opp_min );

        if ( ! MoveSimulator::self_can_reach_after_turn_dash( wm, ball_pos,
                                                              wm.self().playerType().kickableArea() + 0.3,
                                                              false, // forward dash
                                                              opp_min,
                                                              &result ) )
        {
            intercept = false;
        }
        else
        {
            if ( result.turn_step_ > 0 )
            {
                agent->debugClient().addMessage( "SW:Danger:Intercept:MoveTurn" );
                agent->doTurn( result.turn_moment_ );
                agent->setNeckAction( new Neck_DefaultInterceptNeck
                                      ( new Neck_TurnToBallOrScan( 0 ) ) );
                return true;
            }

            if ( result.dash_step_ > 0 )
            {
                agent->debugClient().addMessage( "SW:Danger:Intercept:MoveDash" );
                agent->doDash( result.dash_power_, result.dash_dir_ );
                agent->setNeckAction( new Neck_DefaultInterceptNeck
                                      ( new Neck_TurnToBallOrScan( 0 ) ) );
                return true;
            }
        }
    }
#endif

    if ( intercept )
    {
        agent->debugClient().addMessage( "SW:Danger:Intercept" );
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
Bhv_SweeperDangerMove::doTackleIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( FieldAnalyzer::is_ball_moving_to_our_goal( wm )
         || ( wm.interceptTable()->opponentReachStep() >= 2
              && wm.ball().pos().dist2( ServerParam::i().ourTeamGoalPos() ) < std::pow( 15.0, 2 ) ) )
    {
        if ( Bhv_TackleIntercept().execute( agent ) )
        {
            agent->debugClient().addMessage( "SW:TackleIntercept" );
            return true;
        }
    }
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SweeperDangerMove::doPrepareBlockShoot( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D opponent_ball_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    Vector2D opposite_side_target;
    opposite_side_target.x = -ServerParam::i().pitchHalfLength();
    opposite_side_target.y = ( opponent_ball_pos.y > 0.0
                               ? -ServerParam::i().goalHalfWidth() + 2.0
                               : +ServerParam::i().goalHalfWidth() - 2.0 );

    dlog.addText( Logger::ROLE,
                  __FILE__":(doPrepareBlockShoot) opponent_step=%d",
                  opponent_step );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doPrepareBlockShoot) first=(%.2f %.2f) target=(%.2f %.2f)",
                  opponent_ball_pos.x, opponent_ball_pos.y,
                  opposite_side_target.x, opposite_side_target.y );

    Vector2D block_pos;
    int step = get_move_step_to_stop_shoot( wm, opponent_step, opponent_ball_pos, opposite_side_target,
                                            &block_pos );
    if ( step < 0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPrepareBlockShoot) cannot stop the shoot" );
        return false;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doPrepareBlockShoot) can stop the shoot. step=%d pos=(%.2f %.2f)",
                  step, block_pos.x, block_pos.y );

    double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );
    double tolerance = std::max( 0.4, wm.self().pos().dist( opponent_ball_pos ) * 0.07 );

    if ( Body_GoToPoint( block_pos,
                         tolerance,
                         dash_power ).execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doPrepareBlockShoot) prepare shoot block. target=(%.2f %.2f)",
                      block_pos.x, block_pos.y );
        agent->debugClient().addMessage( "SW:PrepareBlock" );
        agent->debugClient().setTarget( block_pos );
        agent->debugClient().addCircle( block_pos, tolerance );
        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SweeperDangerMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    if ( ! fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no opponent" );
        return false;
    }

    int opp_min = wm.interceptTable()->opponentReachCycle();
    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    if ( wm.kickableTeammate()
         || opp_trap_pos.x > -36.0
         || opp_trap_pos.dist( home_pos ) > 7.0
         || opp_trap_pos.absY() > 9.0
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no get ball situation" );
        return false;
    }

    if ( opponent_is_blocked( wm, fastest_opp, opp_trap_pos ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) opponent %d is blocked", fastest_opp->unum() );
        return false;
    }


    Rect2D bounding_rect( Vector2D( -60.0, home_pos.y - 4.0 ),
                          Vector2D( -36.0, home_pos.y + 4.0 ) );

#if 1
    // 2009-06-26
    if ( wm.ball().pos().x > -36.0 )
    {
        bounding_rect = Rect2D( Vector2D( -60.0, home_pos.y - 10.0 ),
                                Vector2D( wm.ball().pos().x, home_pos.y + 10.0 ) );
    }
#endif


    Vector2D center_pos = Vector2D::INVALIDATED;
    // 2013-06-08
    {
        const Vector2D opponent_ball_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );

        const double goal_x = -ServerParam::i().pitchHalfLength();
        const double goal_w = ServerParam::i().goalHalfWidth();
        const Vector2D goal_l( goal_x, -goal_w + 0.5 );
        const Vector2D goal_r( goal_x, +goal_w - 0.5 );

        const Line2D goal_l_line( opponent_ball_pos, goal_l );
        const Line2D goal_r_line( opponent_ball_pos, goal_r );

        double block_x_l = -47.0;
        double block_x_r = -47.0;
        double block_y_l = goal_l_line.getY( block_x_l );
        double block_y_r = goal_r_line.getY( block_x_r );
        if ( block_y_l == Line2D::ERROR_VALUE
             || std::fabs( block_y_l ) > goal_w - 1.0 )
        {
            block_x_l = goal_x;
            block_y_l = -goal_w + 1.0;
        }
        if ( block_y_r == Line2D::ERROR_VALUE
             || std::fabs( block_y_r ) > goal_w - 1.0 )
        {
            block_x_r = goal_x;
            block_y_r = +goal_w - 1.0;
        }

        block_y_l += wm.self().playerType().kickableArea();
        block_y_r -= wm.self().playerType().kickableArea();

        Vector2D block_point_l( block_x_l, block_y_l );
        Vector2D block_point_r( block_x_r, block_y_r );

        {
            Vector2D unit_vec = ( block_point_l - opponent_ball_pos ).setLengthVector( 1.0 );
            for ( int i = 0; i < 5 && block_point_l.x > goal_x; ++i )
            {
                block_point_l += unit_vec;
            }
            if ( block_point_l.x < goal_x ) block_point_l -= unit_vec;
        }
        {
            Vector2D unit_vec = ( block_point_r - opponent_ball_pos ).setLengthVector( 1.0 );
            for ( int i = 0; i < 5 && block_point_r.x > goal_x; ++i )
            {
                block_point_r += unit_vec;
            }
            if ( block_point_r.x < goal_x ) block_point_r -= unit_vec;
        }

        const AngleDeg block_angle_l = ( block_point_l - opponent_ball_pos ).th();
        const AngleDeg block_angle_r = ( block_point_r - opponent_ball_pos ).th();


        if ( 0 )
        {
            dlog.addLine( Logger::ROLE, opponent_ball_pos, goal_l, "#F00" );
            dlog.addLine( Logger::ROLE, opponent_ball_pos, goal_r, "#F00" );
            dlog.addLine( Logger::ROLE, opponent_ball_pos, block_point_l, "#0FF" );
            dlog.addLine( Logger::ROLE, opponent_ball_pos, block_point_r, "#0FF" );
        }

        if ( 0 )
        {
            Vector2D shoot_block_point = getShootBlockPoint( agent );
            Line2D shoot_block_line( opponent_ball_pos, shoot_block_point );
            Vector2D shoot_block_on_goal( goal_x, shoot_block_line.getY( goal_x ) );
            dlog.addLine( Logger::ROLE, opponent_ball_pos, shoot_block_on_goal, "#FF0" );
        }

        center_pos = ( opponent_ball_pos.y > 0.0
                       ? block_point_l
                       : block_point_r );

        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) opposite side target (%.2f %.2f)",
                      center_pos.x, center_pos.y );
        const AbstractPlayerObject * goalie = wm.getOurGoalie();
        if ( goalie
             && goalie->pos().absY() > 3.0 )
        {
            const AngleDeg goalie_angle = ( goalie->pos() - opponent_ball_pos ).th();
            if ( ( goalie_angle - block_angle_l ).abs() > ( goalie_angle - block_angle_r ).abs() )
            {
                center_pos = block_point_l;
            }
            else
            {
                center_pos = block_point_r;
            }
            dlog.addText( Logger::ROLE,
                          __FILE__": (doGetBall) support goalie (%.2f %.2f)",
                          center_pos.x, center_pos.y );
        }
    }

    if ( Bhv_GetBall( bounding_rect, center_pos ).execute( agent ) )
    {
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SweeperDangerMove::doBlockShoot( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( mate_min <= opp_min - 3 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) our ball" );
        return false;
    }

    if ( wm.self().pos().x > 36.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) false: my position is not good for shoot block" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D opponent_ball_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );

    if ( opponent_ball_pos.x < home_pos.x )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) false: opponent ball pos is back" );
        return false;
    }

    Vector2D block_point = getShootBlockPoint( agent );
    //double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );
    double dist_thr = std::max( 0.4, wm.self().pos().dist( opponent_ball_pos ) * 0.07 );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doBlockShoot) block point (%.3f %.3f)",
                  block_point.x, block_point.y );
    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, dist_thr );

    //
    // turn
    //
    if ( wm.self().pos().dist2( block_point ) < std::pow( dist_thr,2 )
         || ( wm.self().pos() + wm.self().vel() ).dist2( block_point ) < std::pow( dist_thr, 2 ) )
    {
        AngleDeg face_angle = ( wm.ball().pos().y > 0.0
                                ? wm.ball().angleFromSelf() - 90.0
                                : wm.ball().angleFromSelf() + 90.0 );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) already block goal(1). turn to angle %.1f",
                      face_angle.degree() );
        agent->debugClient().addMessage( "SW:Danger:BlockShoot:Turn" );
        Body_TurnToAngle( face_angle ).execute( agent );
        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }

    //
    // move
    //

    MoveSimulator::Result forward_dash_move;
    MoveSimulator::Result back_dash_move;
    MoveSimulator::Result omni_dash_move;

    int forward_dash_step = MoveSimulator::simulate_self_turn_dash( wm, block_point, dist_thr, false, 30,
                                                                    &forward_dash_move );
    int back_dash_step = MoveSimulator::simulate_self_turn_dash( wm, block_point, dist_thr, true, 30,
                                                                 &back_dash_move );
    int omni_dash_step = MoveSimulator::simulate_self_omni_dash( wm, block_point, dist_thr, 30,
                                                                 &omni_dash_move );

    if ( forward_dash_step < 0 ) forward_dash_step = 1000;
    if ( back_dash_step < 0 ) back_dash_step = 1000;
    if ( omni_dash_step < 0 ) omni_dash_step = 1000;

    dlog.addText( Logger::ROLE,
                  __FILE__": (doBlockShoot) move step: forward=%d back=%d omni=%d",
                  forward_dash_step, back_dash_step, omni_dash_step );

    MoveSimulator::Result best_move = forward_dash_move;

    if ( omni_dash_step <= forward_dash_step
         && omni_dash_step <= back_dash_step )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) select omni dash" );
        best_move = omni_dash_move;
    }
    else if ( forward_dash_step <= back_dash_step
              && forward_dash_step <= omni_dash_step )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) select forward dash" );
        best_move = forward_dash_move;
    }
    else if ( back_dash_step <= forward_dash_step
              && back_dash_step <= omni_dash_step )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) select back dash" );
        best_move = back_dash_move;
    }

    if ( best_move.turn_step_ > 0 )
    {
        agent->debugClient().addMessage( "SW:Danger:BlockShoot:Turn:%.0f", best_move.turn_moment_ );
        agent->doTurn( best_move.turn_moment_ );
    }
    else if ( best_move.dash_step_ > 0 )
    {
        agent->debugClient().addMessage( "SW:Danger:BlockShoot:Dash:%.0f,%.0f",
                                         best_move.dash_power_, best_move.dash_dir_ );
        agent->doDash( best_move.dash_power_, best_move.dash_dir_ );
    }
    else
    {
        if ( Body_GoToPoint( block_point,
                             dist_thr,
                             ServerParam::i().maxDashPower(),
                             -1.0, // dash speed
                             2, //1, // cycle
                             true, // save recovery
                             15.0 // dir thr
                             // omni dash threshold
                             ).execute( agent ) )
        {
            agent->debugClient().addMessage( "SW:Danger:BlockShoot:Go" );
        }
        else
        {
            AngleDeg face_angle = ( wm.ball().pos().y > 0.0
                                    ? wm.ball().angleFromSelf() - 90.0
                                    : wm.ball().angleFromSelf() + 90.0 );
            dlog.addText( Logger::ROLE,
                          __FILE__": (doBlockShoot) already block goal(2). turn to angle %.1f",
                          face_angle.degree() );
            agent->debugClient().addMessage( "SW:Danger:BlockShoot:Turn" );
            Body_TurnToAngle( face_angle ).execute( agent );
        }
    }

    agent->setNeckAction( new Neck_CheckBallOwner() );
    return true;
}

namespace {
/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_shoot_block_point_2012( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const Vector2D goal_l( -SP.pitchHalfLength(), -SP.goalHalfWidth() + 0.5 );
    const Vector2D goal_r( -SP.pitchHalfLength(), +SP.goalHalfWidth() - 0.5 );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    const int opponent_step = wm.interceptTable()->opponentReachStep();
    Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    if ( wm.kickableOpponent()
         || opponent_step == 0 )
    {
        const PlayerObject * fastest_opponent = wm.interceptTable()->fastestOpponent();
        if ( fastest_opponent
             && fastest_opponent->posCount() <= wm.ball().posCount() )
        {
            opponent_ball_pos = fastest_opponent->inertiaFinalPoint();
        }
    }

    //Vector2D goal_base_point( -SP.pitchHalfLength(), 0.0 );

    dlog.addLine( Logger::ROLE, opponent_ball_pos, goal_l, "#F00" );
    dlog.addLine( Logger::ROLE, opponent_ball_pos, SP.ourTeamGoalPos(), "#F00" );
    dlog.addLine( Logger::ROLE, opponent_ball_pos, goal_r, "#F00" );

    const AngleDeg goal_angle = ( SP.ourTeamGoalPos() - opponent_ball_pos ).th();
    const AngleDeg half_angle_l = AngleDeg::bisect( ( goal_l - opponent_ball_pos ).th(), goal_angle );
    const AngleDeg half_angle_r = AngleDeg::bisect( goal_angle, ( goal_r - opponent_ball_pos ).th() );
    AngleDeg block_angle = ( opponent_ball_pos.y > 0.0
                             ? half_angle_l
                             : half_angle_r );
    const AbstractPlayerObject * goalie = wm.getOurGoalie();
    if ( goalie
         && goalie->pos().absY() > SP.goalHalfWidth() + 5.0 )
    {
        block_angle = ( opponent_ball_pos.y > 0.0
                        ? half_angle_r
                        : half_angle_l );
    }

    const Line2D block_line( opponent_ball_pos, block_angle );

    Vector2D block_point( home_pos.x, 0.0 );
    double tmp_x = block_line.getX( home_pos.y );

    dlog.addText( Logger::ROLE,
                  __FILE__":(getShootBlockPoint) block point original= (%.3f %.3f)",
                  block_point.x, block_point.y );

    if ( -SP.pitchHalfLength() + 0.5 < tmp_x
         && tmp_x < home_pos.x + 1.0 )
    {
        block_point.x = tmp_x;
        dlog.addText( Logger::ROLE,
                      __FILE__":(getShootBlockPoint) update x to %.3f", tmp_x );
    }

    block_point.y = block_line.getY( block_point.x );
    dlog.addText( Logger::ROLE,
                  __FILE__":(getShootBlockPoint) block point modified= (%.3f %.3f)",
                  block_point.x, block_point.y );

    // block_point.y += ( goal_base_point.y < 0.0
    //                    ? + wm.self().playerType().kickableArea()
    //                    : - wm.self().playerType().kickableArea() );

    Vector2D my_inertia = wm.self().inertiaFinalPoint();
    if ( block_line.dist( my_inertia ) > 1.0 )
    {
        Ray2D my_body_ray( my_inertia, wm.self().body() );
        Vector2D ray_intersection = my_body_ray.intersection( block_line );
        if ( ray_intersection.isValid() )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doBlockGoal) body ray intersection = (%.3f %.3f)",
                          ray_intersection.x, ray_intersection.y );
            if ( ray_intersection.dist2( block_point ) < std::pow( 2.0, 2 ) )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__": (doBlockGoal) updated to body ray intersection" );
                block_point = ray_intersection;
            }
        }
    }

    return block_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_shoot_block_point_2013( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    const Vector2D opponent_ball_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );

    const Line2D block_line = get_shoot_block_line( wm );

    double block_x = ( opponent_ball_pos.absY() < 1.0
                       ? home_pos.x
                       : g_shoot_block_base_x );
    double block_y = block_line.getY( block_x );

    if ( block_y == Line2D::ERROR_VALUE )
    {
        block_y = home_pos.y;
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_shoot_block_point_2013) could not get legal y value" );
    }
    dlog.addText( Logger::ROLE,
                  __FILE__":(get_shoot_block_point_2013) block point base=(%.2f %.2f)",
                  block_x, block_y );
    //
    // debug
    //
    if ( 0 )
    {
        double start_y = block_line.getY( opponent_ball_pos.x );
        double end_y = block_line.getY( -52.5 );
        if ( start_y != Line2D::ERROR_VALUE
             && end_y != Line2D::ERROR_VALUE )
        {
            dlog.addLine( Logger::ROLE,
                          opponent_ball_pos.x, start_y, -52.5, end_y, "#00F" );
        }
        else
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_shoot_block_point_2013) cannot paint the legal line" );
        }
    }

    if ( block_x > opponent_ball_pos.x
         || std::fabs( block_y ) > 3.0
         // || std::fabs( block_y ) > opponent_ball_pos.absY()
         )
    {
        double tmp_x = block_line.getX( home_pos.y );
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_shoot_block_point_2013) tmp_x=%.2f", tmp_x );

        if ( -SP.pitchHalfLength() + 0.5 < tmp_x
             && tmp_x < home_pos.x + 2.0 )
        {
            block_x = tmp_x;
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_shoot_block_point_2013) adjust x to %.2f", block_x );
        }

        block_y = block_line.getY( tmp_x );
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_shoot_block_point_2013) new_y=%.2f", block_y );

        if ( block_y == Line2D::ERROR_VALUE
             || std::fabs( block_y ) > SP.goalHalfWidth() )
        {
            block_y = home_pos.y;
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_shoot_block_point_2013) set y to home.y=%.2f", block_y );
        }

        dlog.addText( Logger::ROLE,
                      __FILE__":(get_shoot_block_point_2013) block point adjusted=(%.2f %.2f)",
                      block_x, block_y );
    }

    Vector2D block_point( block_x, block_y );

    Vector2D my_inertia = wm.self().inertiaFinalPoint();
    if ( block_line.dist( my_inertia ) > 1.0 )
    {
        Ray2D my_body_ray( my_inertia, wm.self().body() );
        Vector2D ray_intersection = my_body_ray.intersection( block_line );
        if ( ray_intersection.isValid() )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doBlockGoal) body ray intersection = (%.3f %.3f)",
                          ray_intersection.x, ray_intersection.y );
            if ( ray_intersection.dist2( block_point ) < std::pow( 2.0, 2 ) )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__": (doBlockGoal) updated to body ray intersection " );
                block_point = ray_intersection;
            }
        }
    }

    return block_point;
}
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SweeperDangerMove::getShootBlockPoint( PlayerAgent * agent )
{
    return get_shoot_block_point_2013( agent );
    //return get_shoot_block_point_2012( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SweeperDangerMove::doNormalMove( PlayerAgent * agent )
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

    agent->debugClient().addMessage( "CG:Danger:Normal" );
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
Bhv_SweeperDangerMove::doGoToPoint( PlayerAgent * agent,
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
