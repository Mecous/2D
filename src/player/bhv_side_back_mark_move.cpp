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

#include "bhv_side_back_mark_move.h"

#include "strategy.h"
#include "mark_analyzer.h"
#include "defense_system.h"

#include "bhv_find_player.h"
#include "neck_check_ball_owner.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/view_synch.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackMarkMove::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( mate_min <= 2
         && mate_min <= opp_min - 6 )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (execute) no mark situation." );
        return false;
    }

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

    if ( ! mark_target )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (execute) no mark target." );
        return false;
    }

    dlog.addText( Logger::MARK | Logger::ROLE,
                  __FILE__": (execute) mark target ghostCount=%d",
                  mark_target->ghostCount() );

    if ( mark_target->ghostCount() >= 5 )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (execute) mark target is ghost." );
        return false;
    }

#if 1
    if ( mark_target->ghostCount() >= 2
         || mark_target->posCount() >= 3 )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (execute) find player." );
        agent->debugClient().addMessage( "SB:Mark:Find" );
        Bhv_FindPlayer( mark_target, 0 ).execute( agent );
        return true;
    }
#else
    if ( mark_target->ghostCount() >= 2
         || mark_target->posCount() >= 3 )
    {
        if ( doMoveFindPlayer( agent ) )
        {
            return true;
        }
    }
#endif

    const AbstractPlayerObject * fastest_opponent = wm.interceptTable()->fastestOpponent();
    if ( fastest_opponent == mark_target )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (execute) mark target is the first opponent attakcer." );
        return false;
    }

    Vector2D move_point = getTargetPoint( wm );

    if ( ! move_point.isValid() )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (execute) move target point not found." );
        return false;
    }

    double dash_power = ServerParam::i().maxPower();

    double dist_thr = 0.5;

    if ( wm.ball().pos().dist( move_point ) > 30.0 )
    {
        dist_thr = 2.0;
    }

    agent->debugClient().addMessage( "SB:Mark" );
    agent->debugClient().setTarget( move_point );
    agent->debugClient().addCircle( move_point, dist_thr );
    //agent->debugClient().addLine( mark_target->pos(), mark_target_pos );

    dlog.addText( Logger::MARK | Logger::ROLE,
                  __FILE__": (execute) pos=(%.2f %.2f) dist_thr=%.3f dash_power=%.1f",
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
bool
Bhv_SideBackMarkMove::doMoveFindPlayer( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );
    if ( ! mark_target )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (doMoveFindPlayer) no mark target." );
        return false;
    }

    if ( mark_target->ghostCount() < 2
         && mark_target->posCount() < 3 )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (doMoveFindPlayer) mark target is available." );
        return false;
    }

    Vector2D mark_target_move_target( -ServerParam::i().pitchHalfLength() + 2.0, 0.0 );
    if ( mark_target->pos().absY() > ServerParam::i().goalHalfWidth() - 3.0 )
    {
        mark_target_move_target.y = ServerParam::i().goalHalfWidth() - 3.0;
        mark_target_move_target.y *= sign( mark_target->pos().y );
    }

    Vector2D virtual_move = ( mark_target_move_target - mark_target->pos() );
    virtual_move.setLength( mark_target->playerTypePtr()->realSpeedMax() * 0.8 * mark_target->posCount() );

    Vector2D target_point = mark_target->pos() + virtual_move;

    if ( target_point.x < mark_target_move_target.x )
    {
        target_point = mark_target_move_target;
    }

    dlog.addText( Logger::MARK | Logger::ROLE,
                  __FILE__": (doMoveFindPlayer) target point (%.2f %.2f)",
                  target_point.x, target_point.y );

    agent->debugClient().addMessage( "SB:Mark:Find" );
    agent->debugClient().setTarget( target_point );

    if ( Body_GoToPoint( target_point, 2.0,
                         ServerParam::i().maxDashPower(),
                         -1.0, // dash speed
                         100, // cycle
                         true,
                         15.0 ).execute( agent ) )
    {
        agent->setViewAction( new View_Wide() );
        agent->setNeckAction( new Neck_ScanField() );
    }
    else
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (doMoveFindPlayer) scan field." );
        Bhv_ScanField().execute( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SideBackMarkMove::getTargetPoint( const WorldModel & wm )
{
    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

    if ( ! mark_target )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) no mark target." );
        return Vector2D::INVALIDATED;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PlayerType * ptype = mark_target->playerTypePtr();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    Vector2D mark_target_pos = mark_target->inertiaFinalPoint();

    if ( std::fabs( mark_target_pos.y - home_pos.y ) > 25.0
         || ( mark_target_pos.x > home_pos.x + 12.0
              && mark_target_pos.x > 0.0 ) )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) too far (%.2f %.2f) x_diff=%.1f y_diff=%.1f dist=%.2f",
                      mark_target_pos.x, mark_target_pos.y,
                      mark_target_pos.x - home_pos.x,
                      std::fabs( mark_target_pos.y - home_pos.y ),
                      mark_target_pos.dist( home_pos ) );
        return Vector2D::INVALIDATED;
    }

    const Vector2D ball_pos = wm.ball().inertiaPoint( opp_min );

    Vector2D move_point = mark_target_pos;

    const AngleDeg mark_target_run_dir = mark_target->vel().th();
    const double mark_target_speed = mark_target->vel().r();
    const double fast_run_speed = std::max( 0.1, ptype->realSpeedMax() * ptype->playerDecay() * 0.5 );

    dlog.addText( Logger::MARK | Logger::ROLE,
                  __FILE__": (getTargetPoint) ball_pos=(%.1f %.1f)",
                  ball_pos.x, ball_pos.y );
    dlog.addText( Logger::MARK | Logger::ROLE,
                  __FILE__": (getTargetPoint) check run. vel_count=%d dir=%.1f speed=%.3f thr=%.3f",
                  mark_target->velCount(),
                  mark_target_run_dir.degree(),
                  mark_target_speed, fast_run_speed );
    bool detect_fast_run = false;

    if ( mark_target->velCount() <= 1
         && mark_target_run_dir.abs() > 40.0
         && mark_target_speed > fast_run_speed )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) mark_target %d fast running" );
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) mark_target=%d speed=%.3f(%.3f) dir=%.1f",
                      mark_target->unum(),
                      mark_target_speed, mark_target_speed / ptype->playerDecay(),
                      mark_target_run_dir.degree() );

        Vector2D add_vec = mark_target->vel();
        add_vec /= ptype->playerDecay();

        if ( mark_target->bodyCount() == 0 )
        {
            add_vec = Vector2D::from_polar( ptype->realSpeedMax(), mark_target->body() );
            dlog.addText( Logger::MARK | Logger::ROLE,
                          __FILE__": (getTargetPoint) change to body line" );
        }

        if ( mark_target_pos.x < wm.self().pos().x + 0.5
             || ( mark_target->pos().y < 0.0 && mark_target->vel().y > 0.1 )
             || ( mark_target->pos().y > 0.0 && mark_target->vel().y < -0.1 ) )
        {
            add_vec *= 10.0;
        }

        move_point += add_vec;
        move_point.x -= 0.8;

        detect_fast_run = true;

        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) add_vec=(%.3f %.3f) len=%.3f dir=%.1f -> (%.2f %.2f)",
                      mark_target->unum(),
                      add_vec.x, add_vec.y, add_vec.r(), add_vec.th().degree(),
                      move_point.x, move_point.y );

    }
    else if ( mark_target_pos.x < -40.0 )
    {
        move_point += mark_target->vel();
        move_point.x -= 1.0;

        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) mark_target danger(1)" );
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) mark_target=%d(%.2f %.2f) x-=1  -> (%.2f %.2f)",
                      mark_target->unum(),
                      mark_target_pos.x, mark_target_pos.y,
                      move_point.x, move_point.y );
    }
    else if ( mark_target_pos.x < -35.0 )
    {
        move_point += mark_target->vel();
        move_point.x -= 1.8;

        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) mark_target danger(2)" );
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) mark_target=%d(%.2f %.2f) x-=1  -> (%.2f %.2f)",
                      mark_target->unum(),
                      mark_target_pos.x, mark_target_pos.y,
                      move_point.x, move_point.y );
    }
    else
    {
        move_point += mark_target->vel();
        move_point.x -= 4.0;

        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) mark_target default" );
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) mark_target=%d(%.2f %.2f) x-=4  -> (%.2f %.2f)",
                      mark_target->unum(),
                      mark_target_pos.x, mark_target_pos.y,
                      move_point.x, move_point.y );
    }

#if 1
    // 2012-06-10
    if ( ! detect_fast_run
         && mark_target->bodyCount() <= 1 )
    {
        move_point += Vector2D::from_polar( 0.5, mark_target->body() );
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) add body dir (%.2f %.2f)",
                      move_point.x, move_point.y );
    }
#endif

    const double y_diff = mark_target_pos.y - ball_pos.y;

    dlog.addText( Logger::MARK | Logger::ROLE,
                  __FILE__": (getTargetPoint) base_move_point=(%.2f %.2f)",
                  move_point.x, move_point.y );
    dlog.addRect( Logger::MARK | Logger::ROLE,
                  move_point.x - 0.2, move_point.y - 0.2, 0.4, 0.4 );

    if ( std::fabs( y_diff ) < 3.0 )
    {
        if ( move_point.absY() > 3.0 )
        {
            dlog.addText( Logger::MARK | Logger::ROLE,
                          __FILE__": (getTargetPoint) y adjust(1-1) small adjust" );
            move_point.y -= 0.4 * sign( move_point.y );
        }
        else
        {
            dlog.addText( Logger::MARK | Logger::ROLE,
                          __FILE__": (getTargetPoint) y adjust(1-2) no adjust" );
        }
    }
    else if ( std::fabs( y_diff ) < 7.0 )
    {
        move_point.y -= 0.4 * sign( y_diff );
        //move_point.y += 0.75 * sign( y_diff );
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) y adjust(2) y=%.2f", move_point.y );
    }
    else
    {
        if ( move_point.x < -36.0 )
        {
            if ( move_point.absY() < 6.0 )
            {
                move_point.y -= 0.8 * sign( y_diff );
                dlog.addText( Logger::MARK | Logger::ROLE,
                              __FILE__": (getTargetPoint) y adjust in penalty area(1) y=%.2f", move_point.y );
            }
            else
            {
                //move_point.y -= 1.5 * sign( y_diff );
                move_point.y -= 2.0 * sign( y_diff );
                dlog.addText( Logger::MARK | Logger::ROLE,
                              __FILE__": (getTargetPoint) y adjust in penalty area(2) y=%.2f", move_point.y );
            }
            // if ( ball_angle.abs() > 15.0 )
            // {
            //     move_point.y -= 1.0 * sign( y_diff );
            //     dlog.addText( Logger::MARK | Logger::ROLE,
            //                   __FILE__": (getTargetPoint) y adjust in penalty area(2)" );
            // }
        }
        else
        {
            move_point.y -= 0.2 * sign( y_diff );
            dlog.addText( Logger::MARK | Logger::ROLE,
                          __FILE__": (getTargetPoint) y adjust(4) y=%.2f", move_point.y );
        }
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

    if ( move_point.x < min_x
         && mark_target->pos().x > wm.self().pos().x )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) x adjust(2) base=%.1f -> min_x=%.1f",
                      move_point.x, min_x);
        Vector2D my_inertia = wm.self().inertiaFinalPoint();
        Line2D move_line( my_inertia, move_point );
        move_point.x = min_x;

        if ( mark_target_pos.x < -40.0 )
        {
            dlog.addText( Logger::MARK | Logger::ROLE,
                          __FILE__": (getTargetPoint) x adjust in penalty area." );
            move_point.x = mark_target_pos.x - 0.4;
        }
        else if ( move_point.x > mark_target_pos.x + 0.6 )
        {
            dlog.addText( Logger::MARK | Logger::ROLE,
                          __FILE__": (getTargetPoint) x adjust adjust to opponent x."
                          " min_x=%.2f target_x=%.2f mark_target_x=%.2f",
                          min_x, move_point.x, mark_target_pos.x );
            move_point.x = mark_target_pos.x + 0.6;
        }
#if 0
        double new_y = move_line.getY( move_point.x );
        if ( new_y != Line2D::ERROR_VALUE
             && ( move_point.y - new_y ) * ( my_inertia.y - new_y ) < 0.0 )
        {
            dlog.addText( Logger::MARK | Logger::ROLE,
                          __FILE__": (getTargetPoint) y adjust on the move line(1). old_y=%.2f new_y=%.2f",
                          move_point.y, new_y );
            move_point.y = new_y;
        }
#endif
    }

    if ( move_point.x > home_pos.x
         && move_point.x > -36.0 )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint) x adjust (%.2f %.2f) to home_x=(%.2f %.2f)",
                      move_point.x, move_point.y,
                      home_pos.x, home_pos.y );
#if 0
        Vector2D my_inertia = wm.self().inertiaFinalPoint();
        Line2D move_line( my_inertia, move_point );
        double new_y = move_line.getY( home_pos.x );
        if ( new_y != Line2D::ERROR_VALUE
             && ( move_point.y - new_y ) * ( my_inertia.y - new_y ) < 0.0 )
        {
            dlog.addText( Logger::MARK | Logger::ROLE,
                          __FILE__": (getTargetPoint) y adjust on the move line(2). old_y=%.2f new_y=%.2f",
                          move_point.y, new_y );
            move_point.y = new_y;
        }
#endif
        move_point.x = home_pos.x;
    }

    return move_point;
}


/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SideBackMarkMove::getTargetPoint2013( const WorldModel & wm )
{
    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

    if ( ! mark_target )
    {
        dlog.addText( Logger::MARK | Logger::ROLE,
                      __FILE__": (getTargetPoint2013) no mark target player" );
        return Vector2D::INVALIDATED;
    }

#if 0
    const PlayerType * ptype = mark_target->playerTypePtr();
    const Vector2D target_player_pos = mark_target->inertiaFinalPoint();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    // if ( std::fabs( target_player_pos.y - home_pos.y ) > 15.0
    //      || target_player_pos.dist2( home_pos ) > std::pow( 20.0, 2 )
    //      || ( target_player_pos.x > home_pos.x + 12.0
    //           && target_player_pos.x > 0.0 ) )
    // {
    //     dlog.addText( Logger::MARK | Logger::ROLE,
    //                   __FILE__": (getTargetPoint) too far (%.2f %.2f) x_diff=%.1f y_diff=%.1f dist=%.2f",
    //                   target_player_pos.x, target_player_pos.y,
    //                   target_player_pos.x - home_pos.x,
    //                   std::fabs( target_player_pos.y - home_pos.y ),
    //                   target_player_pos.dist( home_pos ) );
    //     return Vector2D::INVALIDATED;
    // }

    const int opponent_step = wm.interceptTable()->opponentReachCycle();
    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    const AngleDeg mark_target_vel_dir = mark_target->vel().th();
    const double mark_target_speed = mark_target->vel().r();
    const double fast_run_speed = std::max( 0.1, ptype->realSpeedMax() * ptype->playerDecay() * 0.5 );
#endif

    return Vector2D::INVALIDATED;
}
