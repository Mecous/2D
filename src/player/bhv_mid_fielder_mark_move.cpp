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

#include "bhv_mid_fielder_mark_move.h"

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
Bhv_MidFielderMarkMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM | Logger::MARK,
                  __FILE__": Bhv_MidFielderMarkMove" );

    const WorldModel & wm = agent->world();

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( mate_min <= 1
         && mate_min <= opp_min - 2
         && wm.ball().vel().x > -0.1 )
    {
        if ( wm.ball().pos().x < -36.0
             && ( ! wm.opponentsFromBall().empty()
                  && wm.opponentsFromBall().front()->distFromBall() < 2.0 ) )
        {

        }
        else
        {
            dlog.addText( Logger::MARK,
                          __FILE__": (execute) no mark situation." );
            return false;
        }
    }

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

    if ( ! mark_target )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) no mark target." );
        return false;
    }

    dlog.addText( Logger::MARK,
                  __FILE__": (execute) mark target ghostCount()=%d",
                  mark_target->ghostCount() );

    if ( mark_target->ghostCount() >= 5 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) mark target is ghost." );
        return false;
    }

    if ( mark_target->ghostCount() >= 2
         || mark_target->posCount() >= 6 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) find player." );
        agent->debugClient().addMessage( "MF:Mark:FindPlayer" );
        Bhv_FindPlayer( mark_target, 0 ).execute( agent );
        return true;
    }

    const Vector2D target_point = getTargetPoint( agent, mark_target );

    if ( ! target_point.isValid() )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (execute) illegal target point." );
        return false;
    }

    double dash_power = ServerParam::i().maxPower();
    double dist_thr = 0.5;

    if ( wm.ball().pos().dist( target_point ) > 30.0 )
    {
        dist_thr = 1.0;
    }

    agent->debugClient().addMessage( "MF:Mark" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );
    //agent->debugClient().addLine( mark_target->pos(), mark_target_pos );

    // dlog.addLine( Logger::MARK,
    //               mark_target->pos(), mark_target_pos, "#F00" );
    dlog.addText( Logger::MARK,
                  __FILE__": (execute) pos=(%.2f %.2f) dist_thr=%.3f dash_power=%.1f",
                  target_point.x, target_point.y,
                  dist_thr, dash_power );

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
rcsc::Vector2D
Bhv_MidFielderMarkMove::getTargetPoint( PlayerAgent * agent,
                                        const AbstractPlayerObject * mark_target )
{
    const WorldModel & wm = agent->world();

    const Vector2D mark_target_pos = mark_target->pos() + mark_target->vel();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    if ( std::fabs( mark_target_pos.y - home_pos.y ) > 25.0
         //|| mark_target_pos.dist2( home_pos ) > std::pow( 20.0, 2 )
         || mark_target_pos.x > home_pos.x + 15.0 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(getTargetPoint) too far mark target (%.2f %.2f) y_diff=%.3f dist=%.3f",
                      mark_target_pos.x, mark_target_pos.y,
                      std::fabs( mark_target_pos.y - home_pos.y ),
                      mark_target_pos.dist( home_pos ) );
        return Vector2D::INVALIDATED;
    }

    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const Vector2D ball_pos = wm.ball().inertiaPoint( opp_min );

    double y_diff = ball_pos.y - mark_target_pos.y;

    Vector2D target_point = mark_target_pos;
    //target_point.x -= 1.4;
    target_point.x -= 2.0; // 2015-07-14
    if ( target_point.y > +10.0 ) target_point.y -= 0.5;
    if ( target_point.y < -10.0 ) target_point.y += 0.5;

    dlog.addText( Logger::MARK,
                  __FILE__":(getTargetPoint) mark_target=%d(%.1f %.1f)->(%.1f %.1f) base_target=(%.1f %.1f)",
                  mark_target->unum(),
                  mark_target->pos().x, mark_target->pos().y,
                  mark_target_pos.x, mark_target_pos.y,
                  target_point.x, target_point.y );

    if ( target_point.x < -36.0
         && target_point.absY() < 16.0 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(getTargetPoint) y adjust(0) in penalty area. no adjust" );
    }
    else if ( std::fabs( y_diff ) < 3.0 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(getTargetPoint) y adjust(1) no adjust" );
    }
    else if ( std::fabs( y_diff ) < 7.0 )
    {
        target_point.y += 0.45 * sign( y_diff );
        //target_point.y += 0.75 * sign( y_diff );
        dlog.addText( Logger::MARK,
                      __FILE__":(getTargetPoint) y adjust(2) %.2f", 0.45 * sign( y_diff ) );
    }
    else
    {
        target_point.y += 0.9 * sign( y_diff );
        //target_point.y += 1.25 * sign( y_diff );
        dlog.addText( Logger::MARK,
                      __FILE__":(getTargetPoint) y adjust(3) %.2f", 0.9 * sign( y_diff ) );
    }

    if ( target_point.x < wm.ourDefenseLineX() - 3.0 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(getTargetPoint) adjust target_point_x=(%.2f %.2f) to defense_line=%.2f",
                      target_point.x, target_point.y,
                      wm.ourDefenseLineX() );
        target_point.x = wm.ourDefenseLineX() - 3.0;
    }

    const double min_x = ( wm.ball().pos().x > 0.0
                           ? -15.0
                           : wm.ball().pos().x > -12.5
                           ? -23.5 //? -25.0 //? -22.0
                           : wm.ball().pos().x > -21.5
                           ? -30.0
                           : -36.5 );

    if ( target_point.x < min_x )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(getTargetPoint) adjust target_point_x(2) min_x=%.1f",
                      min_x);
        target_point.x = min_x;

        if ( target_point.x > mark_target_pos.x - 0.8 )
        {
            dlog.addText( Logger::MARK,
                          __FILE__":(getTargetPoint) adjust target_point_x. adjust to opponent x." );
            dlog.addText( Logger::MARK,
                          __FILE__"__  min_x=%.2f target_x=%.2f mark_target_x=%.2f",
                          min_x, target_point.x, mark_target_pos.x );
            target_point.x = mark_target_pos.x - 0.8;
        }
    }

    if ( mark_target->velCount() <= 2
         && mark_target->vel().x < -0.2 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(getTargetPoint) adjust target_point_x again for opponent attacker dash" );
        target_point.x -= 1.0;
    }

    return target_point;
}
