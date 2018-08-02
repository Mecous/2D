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

#include "bhv_side_back_block_ball_owner_dribble.h"

#include "strategy.h"
#include "move_simulator.h"

#include "neck_check_ball_owner.h"

#include "position_analyzer.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_turn_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/view_synch.h>

#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/player_agent.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/circle_2d.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/soccer_math.h>
#include <rcsc/timer.h>

using namespace rcsc;

// #define DEBUG_PROFILE
// #define DEBUG_PRINT
// #define DEBUG_PRINT_2013

namespace {

inline
void
debug_paint_target( const int count,
                    const Vector2D & pos )
{
    char msg[8];
    snprintf( msg, 8, "%d", count );
    dlog.addMessage( Logger::ROLE,
                     pos, msg );
    dlog.addRect( Logger::ROLE,
                  pos.x - 0.1, pos.y - 0.1, 0.2, 0.2,
                  "#F00" );
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_opponent_ball_pos( const WorldModel & wm )
{
    const int opponent_step = wm.interceptTable()->opponentReachCycle();
    const Vector2D ball_pos = wm.ball().inertiaPoint( opponent_step );
    Vector2D opponent_ball_pos = ball_pos;

    dlog.addText( Logger::ROLE,
                  __FILE__":(get_opponent_ball_pos) opponent_step=%d inertia_ball_pos=(%.2f %.2f)",
                  opponent_step,
                  opponent_ball_pos.x, opponent_ball_pos.y );

    if ( wm.kickableOpponent() )
    {
        const PlayerObject * o = wm.kickableOpponent();
	
        if ( wm.ball().seenVelCount() <= 1
             && o->bodyCount() <= 1 )
        {
            AngleDeg ball_vel_angle = wm.ball().seenVel().th();
            if ( ( ball_vel_angle - o->body() ).abs() < 45.0
                 && wm.ball().seenVel().r2() > std::pow( 0.5, 2 ) )
            {
                opponent_ball_pos = wm.ball().pos()
                    + wm.ball().seenVel() * std::pow( ServerParam::i().ballDecay(), 3 );
                dlog.addText( Logger::ROLE,
                              __FILE__":(get_opponent_ball_pos) kickable opponent. seen ball vel and opponent body" );
                dlog.addText( Logger::ROLE,
                              __FILE__":(get_opponent_ball_pos) opponent_ball_pos=(%.2f %.2f)",
                              opponent_ball_pos.x, opponent_ball_pos.y );
                return opponent_ball_pos;
            }
        }

        if ( o->bodyCount() <= 1
             && o->velCount() <= 1
             && o->vel().r2() < std::pow( 0.2, 2 ) )
        {
            opponent_ball_pos = o->pos() + o->vel();
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_opponent_ball_pos) kickable opponent. stopped" );
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_opponent_ball_pos) opponent_ball_pos=(%.2f %.2f)",
                          opponent_ball_pos.x, opponent_ball_pos.y );
            return opponent_ball_pos;
	}
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(get_opponent_ball_pos) no kickable opponent" );
    dlog.addText( Logger::ROLE,
                  __FILE__":(get_opponent_ball_pos) opponent_ball_pos=(%.2f %.2f)",
                  opponent_ball_pos.x, opponent_ball_pos.y );
    return opponent_ball_pos;
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
get_center_point( const WorldModel & wm,
                  const Vector2D & opponent_ball_pos )
{
#if 0
    // 2012-06-16
    const AbstractPlayerObject * fastest_opponent = wm.interceptTable()->fastestOpponent();
    if ( fastest_opponent
         && fastest_opponent->bodyCount() == 0 )
    {
        if ( opponent_ball_pos.x < -35.0 )
        {
            AngleDeg base_angle = ( Vector2D( -45.0, 0.0 ) - opponent_ball_pos ).th();
            if ( ( base_angle - fastest_opponent->body() ).abs() < 30.0 )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__":(get_center_point) center body(1)" );
                return opponent_ball_pos + Vector2D::from_polar( 30.0, fastest_opponent->body() );
            }
        }
        else
        {
            if ( fastest_opponent->body().abs() > 140.0 )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__":(get_center_point) center body(2)" );
                return opponent_ball_pos + Vector2D::from_polar( 30.0, fastest_opponent->body() );
            }
        }

    }
#endif

#if 1
    // 2013-06-07
    const PlayerObject * fastest_opponent = wm.interceptTable()->fastestOpponent();
    if ( fastest_opponent
         && fastest_opponent->isKickable()
         && fastest_opponent->bodyCount() == 0
         && fastest_opponent->body().abs() > 100.0 )
    {
        const Vector2D opponent_pos = fastest_opponent->inertiaFinalPoint();
        const Circle2D circle( Vector2D( -46.0, 0.0 ), 7.0 );
        const Ray2D body_ray( opponent_pos, fastest_opponent->body() );
        Vector2D sol1, sol2;
        int n_sol = circle.intersection( body_ray, &sol1, &sol2 );
        if ( n_sol > 0 )
        {
            Vector2D pos = sol1;
            if ( n_sol > 1 )
            {
                pos = ( sol1 + sol2 ) * 0.5;
            }
            dlog.addLine( Logger::ROLE, opponent_pos, sol1, "#00F" );
            dlog.addCircle( Logger::ROLE, circle, "#00F" );
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_center_point) on body line. n_sol=%d pos=(%.2f %.2f)",
                          n_sol, pos.x, pos.y );
            return pos;
        }
    }
#endif

    //
    // searth the best point
    //
    Vector2D center_pos( -44.0, 0.0 );
    if ( opponent_ball_pos.x < -38.0
         && opponent_ball_pos.absY() < 7.0 )
    {
        center_pos.x = -52.5;
        // 2009-06-17
        center_pos.y = -2.0 * sign( wm.self().pos().y );
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_center_point) (1) (%.1f %.1f)",
                      center_pos.x, center_pos.y );
    }
    else if ( opponent_ball_pos.x < -38.0
              && opponent_ball_pos.absY() < 9.0 )
    {
        center_pos.x = std::min( -49.0, opponent_ball_pos.x - 0.2 );
        //center_pos.y = opponent_ball_pos.y * 0.6;
        Vector2D goal_pos( - ServerParam::i().pitchHalfLength(),
                           ServerParam::i().goalHalfWidth() * 0.5 );
        if ( opponent_ball_pos.y > 0.0 )
        {
            goal_pos.y *= -1.0;
        }

        Line2D opp_line( opponent_ball_pos, goal_pos );
        center_pos.y = opp_line.getY( center_pos.x );
        if ( center_pos.y == Line2D::ERROR_VALUE )
        {
            center_pos.y = opponent_ball_pos.y * 0.6;
        }
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_center_point) (2) (%.1f %.1f)",
                      center_pos.x, center_pos.y );
    }
    else if ( opponent_ball_pos.x < -38.0
              && opponent_ball_pos.absY() < 12.0 )
    {
        //center_pos.x = -50.0;
        center_pos.x = std::min( -46.5, opponent_ball_pos.x - 0.2 );
        //center_pos.y = 2.5 * sign( opponent_ball_pos.y );
        //center_pos.y = 6.5 * sign( opponent_ball_pos.y );
        //center_pos.y = opponent_ball_pos.y * 0.8;
        center_pos.y = 0.0;
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_center_point) (3) (%.1f %.1f)",
                      center_pos.x, center_pos.y );
    }
    else if ( opponent_ball_pos.x < -30.0
              && 2.0 < opponent_ball_pos.absY()
              && opponent_ball_pos.absY() < 8.0 )
    {
        center_pos.x = -50.0;
        center_pos.y = opponent_ball_pos.y * 0.9;
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_center_point) (4) (%.1f %.1f)",
                      center_pos.x, center_pos.y );
    }
    else if ( opponent_ball_pos.absY() > 25.0 )
    {
        if ( opponent_ball_pos.x < -15.0 )
        {
            center_pos.y = 5.0 * sign( opponent_ball_pos.y );
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_center_point) (5) (%.1f %.1f)",
                          center_pos.x, center_pos.y );
        }
        else
        {
            center_pos.y = 20.0 * sign( opponent_ball_pos.y );
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_center_point) (6) (%.1f %.1f)",
                          center_pos.x, center_pos.y );
        }
    }
    else if ( opponent_ball_pos.absY() > 20.0 )
    {
        center_pos.x = -44.0;
        if ( opponent_ball_pos.x > -18.0 )
        {
            //center_pos.y = 10.0 * sign( opponent_ball_pos.y );
            if ( Strategy::i().opponentType() == Strategy::Type_WrightEagle )
            {
                center_pos.y = 15.0 * sign( opponent_ball_pos.y );
            }
            else
            {
                center_pos.y = 10.0 * sign( opponent_ball_pos.y );
            }
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_center_point) (7) (%.1f %.1f)",
                          center_pos.x, center_pos.y );
        }
        else
        {
            center_pos.y = 5.0 * sign( opponent_ball_pos.y );
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_center_point) (8) (%.1f %.1f)",
                          center_pos.x, center_pos.y );
        }
    }
    else if ( opponent_ball_pos.absY() > 15.0 )
    {
        center_pos.x = -44.0;
        if ( opponent_ball_pos.x > -18.0 )
        {
            //center_pos.y = 10.0 * sign( opponent_ball_pos.y );
            if ( Strategy::i().opponentType() == Strategy::Type_WrightEagle )
            {
                center_pos.y = 13.0 * sign( opponent_ball_pos.y );
            }
            else
            {
                center_pos.y = 10.0 * sign( opponent_ball_pos.y );
            }
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_center_point) (9) (%.1f %.1f)",
                          center_pos.x, center_pos.y );
        }
        else
        {
            center_pos.y = 5.0 * sign( opponent_ball_pos.y );
            dlog.addText( Logger::ROLE,
                          __FILE__":(get_center_point) (10) (%.1f %.1f)",
                          center_pos.x, center_pos.y );
        }
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(get_center_point) default (%.1f %.1f)",
                      center_pos.x, center_pos.y );
    }

    return center_pos;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackBlockBallOwnerDribble::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_SideBackBlockBallOwner" );

    const WorldModel & wm = agent->world();

    //
    // check ball owner
    //
    if ( wm.interceptTable()->teammateReachCycle() == 0
         || wm.kickableTeammate() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": teammate owns the ball." );
        return false;
    }

    //
    // intercept
    //
    if ( doIntercept( agent ) )
    {
        return true;
    }

    //
    // block move
    //
    if ( doBlockMove2013( agent ) )
    {
        if ( wm.ball().distFromSelf() < 4.0 )
        {
            agent->setViewAction( new View_Synch() );
        }
        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackBlockBallOwnerDribble::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int self_step = wm.interceptTable()->selfReachCycle();
    int teammate_step = wm.interceptTable()->teammateReachCycle();
    int opponent_step = wm.interceptTable()->opponentReachCycle();

    //
    // intercept
    //
    if ( self_step <= teammate_step + 1
         && self_step <= opponent_step )
    {
        agent->debugClient().addMessage( "SB:Block:Intercept1" );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doIntercept) get ball intercept" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    if ( self_step <= 3
         && wm.ball().pos().x < -ServerParam::i().pitchHalfLength() + 3.0
         && wm.ball().pos().absY() < ServerParam::i().penaltyAreaHalfWidth() )
    {
        agent->debugClient().addMessage( "SB:Block:Intercept2" );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doIntercept) get ball intercept back" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackBlockBallOwnerDribble::doBlockMove2013( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doBlockMove2013) no opponent" );
        return false;
    }
    const Vector2D opponent_ball_pos = get_opponent_ball_pos( wm );


    const Vector2D center_pos = ( M_center_pos.isValid()
                                  ? M_center_pos
                                  : get_center_point( wm, opponent_ball_pos ) );
    if ( M_center_pos.isValid() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doBlockMove2013) given center=(%.2f %.2f)", center_pos.x, center_pos.y );
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doBlockMove2013) calculated center=(%.2f %.2f)", center_pos.x, center_pos.y );
    }

    dlog.addLine( Logger::ROLE,
                  opponent_ball_pos, center_pos, "#F00" );

    Vector2D target_point = opponent_ball_pos;
    double dist_thr = 0.5;
    bool save_recovery = true;

    if ( opponent_ball_pos.absY() - wm.self().pos().absY() > 8.0 )
    {
	target_point.x -= 8.0;
    }
    else if ( opponent_ball_pos.x - wm.self().pos().x < -2.0
	      && opponent_ball_pos.absY() - wm.self().pos().absY() > 0.0 )
    {
	target_point = wm.self().pos();
	target_point.x = -40.0;
    }
    else
    {
	target_point.x -= 4.0;
    }


    if ( Body_GoToPoint( target_point,
			 dist_thr,
			 ServerParam::i().maxDashPower(),
			 -1.0,
			 2,
			 //std::max( 2, param.cycle_ ),
			 save_recovery,
			 15.0, // dir threshold
                         1.0, // omni dash threshold
                         false // no back dash
                         ).execute( agent ) )
    {
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );
        agent->debugClient().addMessage( "SB:DribbleBlock:Go1" );
        dlog.addText( Logger::ROLE,
                      __FILE__": go to point(1) (%.1f %.1f) thr=%.3f cycle=%d",
                      target_point.x, target_point.y,
                      dist_thr,
                      2 );

	return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SideBackBlockBallOwnerDribble::simulateTurnDashOrOmniDash2013( const WorldModel & wm,
                                                            const Vector2D & opponent_ball_pos,
                                                            const Vector2D & center_pos,
                                                            MoveSimulator::Result * result_move )
{
    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(simulateTurnDashOrOmniDash2013) no opponent" );
        return Vector2D::INVALIDATED;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D inertia_self_pos = wm.self().inertiaFinalPoint();

    bool on_body_line = false;
    if ( opponent->bodyCount() <= 1 )
    {
        Segment2D opponent_move_line( opponent_ball_pos,
                                      opponent_ball_pos + Vector2D::from_polar( 100.0, opponent->body() ) );
        if ( opponent_move_line.contains( wm.self().pos() )
             && opponent_move_line.dist( inertia_self_pos ) < wm.self().playerType().kickableArea() * 0.5 )
        {
            on_body_line = true;
        }
    }

    //
    // simulation loop
    //

    int opponent_turn_step = 0;
    {
        AngleDeg opponent_move_angle = ( center_pos - opponent_ball_pos ).th();
        AngleDeg opponent_body_angle = opponent->body();
        if ( opponent->bodyCount() >= 2 )
        {
            if ( ! opponent->isKickable() )
            {
                Vector2D ball_pos = wm.ball().inertiaPoint( wm.interceptTable()->opponentReachStep() );
                opponent_body_angle = ( ball_pos - opponent->pos() ).th();
            }
            else
            {
                opponent_body_angle = opponent_move_angle;
            }
        }

        if ( ( opponent_body_angle - opponent_move_angle ).abs() > 90.0 )
        {
            opponent_turn_step = 2;
        }
        else if ( ( opponent_body_angle - opponent_move_angle ).abs() > 15.0 )
        {
            opponent_turn_step = 1;
        }
    }
#ifdef DEBUG_PRINT_2013
    dlog.addText( Logger::ROLE,
                  __FILE__":(simulateTurnDashOrOmniDash2013) estimated opponent turn step %d",
                  opponent_turn_step );
#endif

    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -wm.self().body() );

    const double max_x = ServerParam::i().pitchHalfLength() - 1.0;
    const double max_y = ServerParam::i().pitchHalfWidth() - 1.0;
    const double min_length = ( opponent_ball_pos.x < -36.0
                                ? 1.0 // 0.3
                                : 2.0 );
    const double max_length = std::min( 30.0, opponent_ball_pos.dist( center_pos ) ) + 1.0;
    const Vector2D unit_vec = ( center_pos - opponent_ball_pos ).setLengthVector( 1.0 );

#ifdef DEBUG_PRINT_2013
    dlog.addText( Logger::ROLE,
                  __FILE__":(simulateTurnDashOrOmniDash2013) start simulation min_length=%.1f",
                  min_length );
#endif

    Vector2D result_point = Vector2D::INVALIDATED;
    MoveSimulator::Result move;

    double dist_step = 0.3;
    for ( double len = min_length; len < max_length; len += dist_step )
    {
        if ( len >= 16.0 ) dist_step = 3.0;
        else if ( len >= 10.0 ) dist_step = 2.0;
        else if ( len >= 5.0 ) dist_step = 1.0;
        else if ( len >= 1.5 ) dist_step = 0.5;

        const Vector2D target_point = opponent_ball_pos + unit_vec * len;

#ifdef DEBUG_PRINT_2013
        dlog.addRect( Logger::ROLE,
                      target_point.x - 0.05, target_point.y - 0.05, 0.1, 0.1, "#0F0" );
#endif
        if ( target_point.absX() > max_x
             || target_point.absY() > max_y
             || ! M_bounding_rect.contains( target_point ) )
        {
#ifdef DEBUG_PRINT_2013
            dlog.addText( Logger::ROLE,
                          "len=%.2f pos(%.2f %.2f) out of bounds",
                          len, target_point.x, target_point.y );
#endif
            continue;
        }

        if ( ! on_body_line
             && target_point.x > home_pos.x
             && target_point.x > inertia_self_pos.x
             && ( target_point - inertia_self_pos ).th().abs() < 50.0 )
        {
#ifdef DEBUG_PRINT_2013
            dlog.addText( Logger::ROLE,
                          "len=%.2f skip",
                          len, target_point.x, target_point.y );
#endif
            continue;
        }

        const double opponent_move_dist = len - 0.5; //opponent_ball_pos.dist( target_point );
        const int opponent_reach_step
            = opponent->ballReachStep()
            + opponent->playerTypePtr()->cyclesToReachDistance( opponent_move_dist )
            + 1
            + opponent_turn_step
            + static_cast< int >( std::floor( opponent_move_dist * 0.2 ) ); // virtual kick count

        const Vector2D self_pos = wm.self().inertiaPoint( opponent_reach_step );
        const double self_move_dist = self_pos.dist( target_point ) ;
#ifdef DEBUG_PRINT_2013
        dlog.addText( Logger::ROLE,
                      "len=%.2f pos(%.2f %.2f) opponent(move=%.2f step=%d)",
                      len, target_point.x, target_point.y, opponent_move_dist, opponent_reach_step );
#endif
        if ( wm.self().playerType().cyclesToReachDistance( self_move_dist ) > opponent_reach_step )
        {
#ifdef DEBUG_PRINT_2013
            dlog.addText( Logger::ROLE,
                          "len=%.2f XX never reach", len );
#endif
            continue;
        }

        const double tolerance = std::max( 0.5, opponent_move_dist * 0.08 );

        int turn_dash_step = MoveSimulator::simulate_self_turn_dash( wm, target_point, tolerance, false,
                                                                     opponent_reach_step, &move );
        if ( turn_dash_step >= 0
             && ( turn_dash_step <= opponent_reach_step - 2
                  || ( turn_dash_step <= 3 && turn_dash_step <= opponent_reach_step - 1 )
                  || ( turn_dash_step <= 2 && turn_dash_step <= opponent_reach_step ) ) )
        {
            result_point = target_point;
#ifdef DEBUG_PRINT_2013
            dlog.addText( Logger::ROLE,
                          "len=%.2f OK turn dash step=%d(t:%d d:%d)",
                          len, turn_dash_step, move.turn_step_, move.dash_step_ );
#endif
        }

        if ( turn_dash_step >= 0
             && turn_dash_step > opponent_reach_step - 2
             && move.turn_step_ > 0 )
        {
            const Vector2D target_rel = rotate_matrix.transform( target_point - wm.self().pos() );
            if ( target_rel.x > -2.0
                 && target_rel.absY() < 2.0 )
            {
                int omni_step = MoveSimulator::simulate_self_omni_dash( wm, target_point,
                                                                        tolerance,
                                                                        opponent_reach_step,
                                                                        &move );
                if ( omni_step >= 0
                     && ( turn_dash_step < 0 || omni_step <= turn_dash_step )
                     && ( omni_step <= opponent_reach_step - 2
                          || ( omni_step <= 3 && omni_step <= opponent_reach_step - 1 )
                          || ( omni_step <= 2 && omni_step <= opponent_reach_step ) ) )
                {
#ifdef DEBUG_PRINT_2013
                    dlog.addText( Logger::ROLE,
                                  "len=%.2f OK omni dash step=%d",
                                  len, omni_step );
#endif
                    result_point = target_point;
                }
            }
        }

        if ( result_point.isValid() )
        {
            *result_move = move;
            break;
        }
    }

    return result_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SideBackBlockBallOwnerDribble::simulateDashOnly2013( const rcsc::WorldModel & wm,
                                                  const rcsc::Vector2D & opponent_ball_pos,
                                                  const rcsc::Vector2D & center_pos,
                                                  const rcsc::Vector2D & turn_dash_target_point,
                                                  MoveSimulator::Result * result_move )
{
    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(simulateDashOnly2013) no opponent" );
        return Vector2D::INVALIDATED;
    }

    const Segment2D block_segment( opponent_ball_pos, center_pos );
    const Vector2D self_pos = wm.self().inertiaFinalPoint();
    const Line2D self_move_line( self_pos, wm.self().body() );

    const Vector2D target_point = block_segment.intersection( self_move_line );

    if ( ! target_point.isValid()
         || target_point.dist2( turn_dash_target_point ) > std::pow( 3.0, 2 )
         || ! M_bounding_rect.contains( target_point )
         || ( ( target_point - wm.self().pos() ).th() - wm.self().body() ).abs() > 5.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (simulateDashOnly2013) no intersection" );
        return Vector2D::INVALIDATED;
    }

    const double opponent_move_dist = opponent_ball_pos.dist( target_point );
    const int opponent_reach_step
            = opponent->ballReachStep()
            + opponent->playerTypePtr()->cyclesToReachDistance( opponent_move_dist )
            + 1
            + static_cast< int >( std::floor( opponent_move_dist * 0.1 ) ); // virtual kick count

    const double tolerance = std::max( 0.5, opponent_move_dist * 0.08 );

    int self_step = MoveSimulator::simulate_self_turn_dash( wm,
                                                            target_point,
                                                            tolerance,
                                                            false,
                                                            opponent_reach_step,
                                                            result_move );

    if ( self_step >= 0
         && self_step < opponent_reach_step )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (simulateDashOnly2013) OK (%.2f %.2f) self_step=%d opponent_step=%d",
                      target_point.x, target_point.y, self_step, opponent_reach_step );
        return target_point;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (simulateDashOnly2013) XX not found" );
    return Vector2D::INVALIDATED;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackBlockBallOwnerDribble::doWait( PlayerAgent * agent,
                                    const Param & param,
                                    const Segment2D & block_line )
{
    const WorldModel & wm = agent->world();

    if ( param.point_.x < -35.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doWait) NG target_x = %.2f", param.point_.x );
        return false;
    }

    if ( param.cycle_ >= 2 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doWait) NG cycle = %d", param.cycle_ );
        return false;
    }

    const Vector2D my_inertia = wm.self().inertiaFinalPoint();

    if ( block_line.contains( my_inertia )
         && block_line.dist( my_inertia ) < 0.5 )
    {
        Body_TurnToPoint( param.point_ ).execute( agent );

        if ( wm.ball().distFromSelf() < 4.0 )
        {
            agent->setViewAction( new View_Synch() );
        }
        agent->setNeckAction( new Neck_CheckBallOwner() );

        agent->debugClient().setTarget( param.point_ );
        agent->debugClient().addMessage( "SB:Block:Wait" );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doWait) wait on block line" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideBackBlockBallOwnerDribble::simulateDashes( const WorldModel & wm,
                                            const Vector2D & opponent_ball_pos,
                                            const Vector2D & center_pos,
                                            const Rect2D & bounding_rect,
                                            const double & dist_thr,
                                            const bool save_recovery,
                                            Param * param )
{
    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();

    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(simulate) no fastest opponent" );
        return;
    }

    const Segment2D block_segment( opponent_ball_pos, center_pos );
    const Line2D my_move_line( wm.self().pos(), wm.self().body() );

    const Vector2D intersect_point = block_segment.intersection( my_move_line );
    if ( ! intersect_point.isValid()
         || ! bounding_rect.contains( intersect_point )
         || ( ( intersect_point - wm.self().pos() ).th() - wm.self().body() ).abs() > 5.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (simulateDashes) no intersection" );
        return;
    }

    {
        //const Vector2D ball_pos = wm.ball().inertiaPoint( wm.interceptTable()->opponentReachCycle() );

        if ( param->point_.isValid() // already solution exists
             && param->point_.dist2( intersect_point ) > std::pow( 3.0, 2 )
             //|| my_move_line.dist( ball_pos ) > wm.self().playerType().kickableArea() + 0.3 )
             )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (simulateDashes) intersection too far" );
            return;
        }
    }

    const PlayerType * opponent_type = opponent->playerTypePtr();
    const double opponent_dist = std::max( 0.0,
                                           opponent_ball_pos.dist( intersect_point )
                                           - opponent_type->kickableArea() * 0.8 );
    const int opponent_step
        = wm.interceptTable()->opponentReachCycle()
        + opponent_type->cyclesToReachDistance( opponent_dist )
        + 1; // magic number

    const int max_step = std::min( param->cycle_ + 2, opponent_step );

    dlog.addText( Logger::ROLE,
                  __FILE__": (simulateDashes) opponent_trap=(%.1f %.1f)",
                  opponent_ball_pos.x, opponent_ball_pos.y );
    dlog.addText( Logger::ROLE,
                  __FILE__": (simulateDashes) intersection=(%.1f %.1f) opponent_step=%d",
                  intersect_point.x, intersect_point.y,
                  opponent_step );

    dlog.addText( Logger::ROLE,
                  __FILE__": (simulateDashes) max_step=%d param_step=%d",
                  max_step, param->cycle_ );
    //
    //
    //
    for ( int n_dash = 1; n_dash <= max_step; ++n_dash )
    {
        const Vector2D inertia_pos = wm.self().inertiaPoint( n_dash );
        const double target_dist = inertia_pos.dist( intersect_point );

        const int dash_step = wm.self().playerType().cyclesToReachDistance( target_dist - dist_thr*0.5 );

        if ( dash_step <= n_dash )
        {
            StaminaModel stamina = wm.self().staminaModel();
            stamina.simulateDashes( wm.self().playerType(),
                                    n_dash,
                                    ServerParam::i().maxDashPower() );
            if ( save_recovery
                 && stamina.recovery() < ServerParam::i().recoverInit() )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__": (simulateDashes) dash=%d recover decay",
                              n_dash );
            }
            else
            {
                dlog.addText( Logger::ROLE,
                              __FILE__": (simulateDashes) dash=%d found stamina=%.1f",
                              n_dash, stamina.stamina() );
                debug_paint_target( -1, intersect_point );

                param->point_ = intersect_point;
                param->turn_ = 0;
                param->cycle_ = n_dash;
                param->stamina_ = stamina.stamina();
            }

            break;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_SideBackBlockBallOwnerDribble::predictSelfReachCycle( const WorldModel & wm,
                                                   const Vector2D & target_point,
                                                   const double & dist_thr,
                                                   const bool save_recovery,
                                                   int * turn_step,
                                                   double * stamina )
{
    const ServerParam & param = ServerParam::i();
    const PlayerType & self_type = wm.self().playerType();
    const double max_moment = param.maxMoment();
    const double recover_dec_thr = param.staminaMax() * param.recoverDecThr();

    const double first_my_speed = wm.self().vel().r();

    for ( int cycle = 0; cycle < 100; ++cycle )
    {
        const Vector2D inertia_pos = wm.self().inertiaPoint( cycle );
        double target_dist = ( target_point - inertia_pos ).r();
        if ( target_dist - dist_thr > self_type.realSpeedMax() * cycle )
        {
            continue;
        }

        int n_turn = 0;
        int n_dash = 0;

        AngleDeg target_angle = ( target_point - inertia_pos ).th();
        double my_speed = first_my_speed;

        //
        // turn
        //
        double angle_diff = ( target_angle - wm.self().body() ).abs();
        double turn_margin = 180.0;
        if ( dist_thr < target_dist )
        {
            turn_margin = std::max( 15.0,
                                    AngleDeg::asin_deg( dist_thr / target_dist ) );
        }

        while ( angle_diff > turn_margin )
        {
            angle_diff -= self_type.effectiveTurn( max_moment, my_speed );
            my_speed *= self_type.playerDecay();
            ++n_turn;
        }

        StaminaModel stamina_model = wm.self().staminaModel();

#if 0
        // TODO: stop dash
        if ( n_turn >= 3 )
        {
            Vector2D vel = wm.self().vel();
            vel.rotate( - wm.self().body() );
            Vector2D stop_accel( -vel.x, 0.0 );

            double dash_power
                = stop_accel.x / ( self_type.dashPowerRate() * my_effort );
            double stop_stamina = stop_dash_power;
            if ( stop_dash_power < 0.0 )
            {
                stop_stamina *= -2.0;
            }

            if ( save_recovery )
            {
                if ( stop_stamina > my_stamina - recover_dec_thr )
                {
                    stop_stamina = std::max( 0.0, my_stamina - recover_dec_thr );
                }
            }
            else if ( stop_stamina > my_stamina )
            {
                stop_stamina = my_stamina;
            }

            if ( stop_dash_power < 0.0 )
            {
                dash_power = -stop_stamina * 0.5;
            }
            else
            {
                dash_power = stop_stamina;
            }

            stop_accel.x = dash_power * self_type.dashPowerRate() * my_effort;
            vel += stop_accel;
            my_speed = vel.r();
        }
#endif

        AngleDeg dash_angle = wm.self().body();
        if ( n_turn > 0 )
        {
            angle_diff = std::max( 0.0, angle_diff );
            dash_angle = target_angle;
            if ( ( target_angle - wm.self().body() ).degree() > 0.0 )
            {
                dash_angle -= angle_diff;
            }
            else
            {
                dash_angle += angle_diff;
            }

            stamina_model.simulateWaits( self_type, n_turn );
        }

        //
        // dash
        //

        Vector2D my_pos = inertia_pos;
        Vector2D vel = wm.self().vel() * std::pow( self_type.playerDecay(), n_turn );
        while ( n_turn + n_dash < cycle
                && target_dist > dist_thr )
        {
            double dash_power = std::min( param.maxDashPower(),
                                          stamina_model.stamina() + self_type.extraStamina() );
            if ( save_recovery
                 && stamina_model.stamina() - dash_power < recover_dec_thr )
            {
                dash_power = std::max( 0.0, stamina_model.stamina() - recover_dec_thr );
            }

            Vector2D accel = Vector2D::polar2vector( dash_power
                                                     * self_type.dashPowerRate()
                                                     * stamina_model.effort(),
                                                     dash_angle );
            vel += accel;
            double speed = vel.r();
            if ( speed > self_type.playerSpeedMax() )
            {
                vel *= self_type.playerSpeedMax() / speed;
            }

            my_pos += vel;
            vel *= self_type.playerDecay();

            stamina_model.simulateDash( self_type, dash_power );

            target_dist = my_pos.dist( target_point );
            ++n_dash;
        }

        if ( target_dist <= dist_thr
             || inertia_pos.dist2( target_point ) < inertia_pos.dist2( my_pos ) )
        {
            if ( turn_step )
            {
                *turn_step = n_turn;
            }

            if ( stamina )
            {
                *stamina = stamina_model.stamina();
            }

            //             dlog.addText( Logger::ROLE,
            //                           "____ cycle=%d n_turn=%d n_dash=%d stamina=%.1f",
            //                           cycle,
            //                           n_turn, n_dash,
            //                           my_stamina );
            return n_turn + n_dash;
        }
    }

    return 1000;
}
