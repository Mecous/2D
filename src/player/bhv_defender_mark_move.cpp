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

#include "bhv_defender_mark_move.h"

#include "strategy.h"
#include "mark_analyzer.h"
#include "defense_system.h"

#include "bhv_find_player.h"
#include "neck_check_ball_owner.h"

#include <rcsc/action/basic_actions.h>
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
Bhv_DefenderMarkMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM | Logger::MARK,
                  __FILE__": Bhv_DefenderMarkMove" );

    const WorldModel & wm = agent->world();

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( mate_min <= 1
         && mate_min <= opp_min - 2
         && wm.ball().vel().x > -0.1 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) no mark situation." );
        return false;
    }

    //
    // check mark target player
    //

    if ( ! isValidMarkTarget( agent ) )
    {
        return false;
    }

    //
    // decide move target point
    //
    const Vector2D target_point = getTargetPointOld( agent );

    if ( ! target_point.isValid() )
    {
        return false;
    }

    //
    // move & find player
    //
    // if ( doMoveFindPlayer( agent, target_point ) )
    // {
    //     return true;
    // }

    //
    // move to the target point
    //

    double dash_power = ServerParam::i().maxPower();
    double dist_thr = 0.5;

    if ( //wm.self().pos().x < target_point.x
        wm.ball().pos().dist( target_point ) > 30.0 )
    {
        dist_thr = 2.0;
    }

    agent->debugClient().addMessage( "Def:Mark" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );
    //agent->debugClient().addLine( mark_target->pos(), mark_target_pos );

    dlog.addText( Logger::MARK,
                  __FILE__": (execute) pos=(%.2f %.2f) dist_thr=%.3f dash_power=%.1f",
                  target_point.x, target_point.y,
                  dist_thr, dash_power );

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

    if ( ! DefenseSystem::mark_go_to_point( agent, mark_target,
                                            target_point, dist_thr, dash_power, 20.0 ) )
    {
        DefenseSystem::mark_turn_to( agent, mark_target );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_DefenderMarkMove::isValidMarkTarget( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * first_opponent = wm.interceptTable()->fastestOpponent();
    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

    if ( ! mark_target )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) no mark target." );
        return false;
    }

    if ( mark_target
         && first_opponent
         && mark_target->unum() == first_opponent->unum() )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) mark target is ball owner." );
        return false;
    }

    dlog.addText( Logger::MARK,
                  __FILE__": (execute) mark target ghostCount=%d, posCount=%d",
                  mark_target->ghostCount(), mark_target->posCount() );

    if ( mark_target->ghostCount() >= 5 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) mark target is ghost." );
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_DefenderMarkMove::doMoveFindPlayer( PlayerAgent * agent,
                                        const Vector2D & move_target )
{
    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

    if ( ! mark_target )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (doMoveFindPlayer) null target" );
        return false;
    }

    if ( mark_target->ghostCount() < 2
         && mark_target->posCount() < 3 )
    {
        return false;
    }

    if ( move_target.x < wm.self().pos().x - 2.0 )
    {
        return false;
    }

    agent->debugClient().setTarget( move_target );
    agent->debugClient().addCircle( move_target, 1.0 );

    dlog.addText( Logger::MARK,
                  __FILE__": (doMoveFindPlayer) target=%d", mark_target->unum() );

    agent->setViewAction( new View_Wide() );

    if ( ! DefenseSystem::mark_go_to_point( agent, mark_target,
                                            move_target,
                                            0.5,
                                            ServerParam::i().maxDashPower(),
                                            20.0 ) )
    {
        DefenseSystem::mark_turn_to( agent, mark_target );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
Bhv_DefenderMarkMove::getTargetPointOld( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

    if ( ! mark_target )
    {
        return Vector2D::INVALIDATED;
    }

    Vector2D mark_target_pos = mark_target->pos() + mark_target->vel();
    mark_target_pos.x -= 1.0;
    if ( mark_target_pos.y > +10.0 ) mark_target_pos.y -= 0.5;
    if ( mark_target_pos.y < -10.0 ) mark_target_pos.y += 0.5;

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

#if 1
    if ( std::fabs( mark_target_pos.y - home_pos.y ) > 25.0
         || mark_target_pos.dist2( home_pos ) > std::pow( 30.0, 2 )
         || mark_target_pos.x > home_pos.x + 15.0 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) too far mark target (%.2f %.2f) y_diff=%.3f dist=%.3f",
                      mark_target_pos.x, mark_target_pos.y,
                      std::fabs( mark_target_pos.y - home_pos.y ),
                      mark_target_pos.dist( home_pos ) );
        return Vector2D::INVALIDATED;
    }
#endif

    const int opponent_step = wm.interceptTable()->opponentReachStep();
    const Vector2D ball_pos = wm.ball().inertiaPoint( opponent_step );

    double y_diff = ball_pos.y - mark_target_pos.y;

    Vector2D target_point = mark_target_pos;
    //target_point.x -= 3.5;
    target_point.x -= 4.0; // 2009-12-14

    dlog.addText( Logger::MARK,
                  __FILE__": (execute) mark_target=%d(%.1f %.1f)->(%.1f %.1f) base_target=(%.1f %.1f)",
                  mark_target->unum(),
                  mark_target->pos().x, mark_target->pos().y,
                  mark_target_pos.x, mark_target_pos.y,
                  target_point.x, target_point.y );

    if ( mark_target->velCount() <= 2
         && mark_target->vel().x < -0.2 )
    {
        target_point.x -= 1.0;
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) adjust x for opponent dash (%.1f %.1f)",
                      target_point.x, target_point.y );
    }

    if ( Strategy::i().opponentType() == Strategy::Type_Gliders )
    {

    }
    else
    {
        if ( target_point.x < wm.ourDefenseLineX() - 3.0 )
        {
            dlog.addText( Logger::MARK,
                          __FILE__": (execute) adjust target_point_x=(%.2f %.2f) to defense_line=%.2f",
                          target_point.x, target_point.y,
                          wm.ourDefenseLineX() );
            target_point.x = wm.ourDefenseLineX() - 3.0;
        }
    }

    const double min_x = ( wm.ball().pos().x > 0.0
                           ? -20.0 //-15.0
                           : wm.ball().pos().x > -12.5
                           ? -27.0 //-23.5 //? -25.0 //? -22.0
                           : wm.ball().pos().x > -21.5
                           ? -30.0
                           : -36.5 );

    if ( target_point.x < min_x )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) adjust target_point_x(2) min_x=%.1f",
                      min_x);
        target_point.x = min_x;

        if ( target_point.x > mark_target_pos.x + 0.6 )
        {
            dlog.addText( Logger::MARK,
                          __FILE__": (execute) adjust target_point_x. adjust to opponent x."
                          " min_x=%.2f target_x=%.2f mark_target_x=%.2f",
                          min_x, target_point.x, mark_target_pos.x );
            target_point.x = mark_target_pos.x + 0.6;
        }
    }

    if ( target_point.x > home_pos.x )
    {
        if ( ball_pos.x < -30.0
             && target_point.dist2( ServerParam::i().ourTeamGoalPos() ) < std::pow( 20.0, 2 ) )
        {
            dlog.addText( Logger::MARK,
                          __FILE__": (execute) adjust target_x>home_x, but danger" );
        }
        else
        {
            dlog.addText( Logger::MARK,
                          __FILE__": (execute) adjust target_point_x=(%.2f %.2f) to home_x=(%.2f %.2f)",
                          target_point.x, target_point.y,
                          home_pos.x, home_pos.y );
            target_point.x = home_pos.x;
#if 1
            // 2012-06-08
            if ( target_point.x < mark_target->pos().x - 8.0 )
            {
                double r = std::exp( -std::pow( mark_target_pos.x - 8.0 - target_point.x, 2 )
                                     / ( 2.0 * std::pow( 3.0, 2 ) ) );
                target_point.y = target_point.y * r + home_pos.y * ( 1.0 - r );
            }
#endif
        }
    }


    if ( target_point.x < -36.0
         && target_point.absY() < 16.0 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) y adjust(0) in penalty area. no adjust" );
    }
    else if ( std::fabs( y_diff ) < 3.0 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) y adjust(1) no adjust" );
    }
    else if ( std::fabs( y_diff ) < 7.0 )
    {
        target_point.y += 0.45 * sign( y_diff );
        //target_point.y += 0.75 * sign( y_diff );
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) y adjust(2) %f", 0.45 * sign( y_diff ) );
    }
    else if ( mark_target_pos.absY() > ServerParam::i().penaltyAreaHalfWidth() ) // 2012-04-25
    {
        //Line2D mark_line( mark_target_pos, ServerParam::i().ourTeamGoalPos() );
        const double mark_base_y
            = sign( mark_target_pos.y )
            * std::max( ( mark_target_pos.absY() - 5.0 ), // magic number
                        0.0 );
        const Vector2D mark_base_pos( -ServerParam::i().pitchHalfLength(),
                                      mark_base_y );
        Line2D mark_line( mark_target_pos, mark_base_pos );
        double new_y = mark_line.getY( target_point.x );
#if 1
        // 2015-07-14
        if ( mark_target->velCount() <= 2
             && mark_target->vel().x < -0.2 )
        {
            AngleDeg vel_angle = mark_target->vel().th();
            dlog.addText( Logger::MARK,
                          __FILE__": (execute) vel angle %.1f", vel_angle.degree() );

            if ( vel_angle.abs() > 130.0 )
            {
                if ( ( mark_target_pos.y < 0.0 && vel_angle.degree() < 0.0 )
                     || ( mark_target_pos.y > 0.0 && vel_angle.degree() > 0.0 ) )
                {
                    dlog.addText( Logger::MARK,
                                  __FILE__": (execute) align vel angle %.1f", vel_angle.degree() );
                    vel_angle = 180.0;
                }

                mark_line.assign( mark_target_pos, vel_angle );
                new_y = mark_line.getY( target_point.x );
                if ( new_y != Line2D::ERROR_VALUE )
                {
                    new_y += 1.0 * sign( y_diff );
                    dlog.addText( Logger::MARK,
                                  __FILE__": (execute) align to vel line (%.1f %.1f)",
                                  target_point.x, new_y );
                }
            }
        }
#endif
        if ( new_y != Line2D::ERROR_VALUE )
        {
            target_point.y = new_y;
            dlog.addText( Logger::MARK,
                          __FILE__": (execute) y adjust(3-1) (%.1f %.1f)",
                          target_point.x, target_point.y );
        }
        else
        {
            target_point.y += 1.5 * sign( y_diff );
            dlog.addText( Logger::MARK,
                          __FILE__": (execute) y adjust(3-2) (%.1f %.1f)",
                          target_point.x, target_point.y );
        }
    }
    else
    {
        target_point.y += 0.9 * sign( y_diff );
        //target_point.y += 1.25 * sign( y_diff );
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) y adjust(4) %f", target_point.y );
    }

#if 0
    // 2012-06-10
    // Do NOT use the following code. bad effect for midfielders
    if ( wm.ourDefenseLineX() + 3.0 < target_point.x )
    {
        double new_x = ( target_point.x + wm.ourDefenseLineX() ) * 0.5;
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) adjust to defense line (%.2f %.2f) -> (%.2f %.2f)",
                      target_point.x, target_point.y, new_x, target_point.y );
        target_point.x = new_x;
    }
#endif

    dlog.addLine( Logger::MARK,
                  mark_target->pos(), mark_target_pos, "#F00" );

    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
Bhv_DefenderMarkMove::getTargetPoint2016( const PlayerAgent * agent )
{
    (void)agent;
    return Vector2D::INVALIDATED;
}
