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

#include "bhv_block_ball_owner.h"

#include "field_analyzer.h"
#include "mark_analyzer.h"

#include "strategy.h"

#include "neck_check_ball_owner.h"

#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_turn_to_angle.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/soccer_math.h>
#include <rcsc/geom/segment_2d.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>


using namespace rcsc;

int Bhv_BlockBallOwner::S_wait_count = 0;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BlockBallOwner::execute( PlayerAgent * agent )
{
    if ( ! M_bounding_region )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Bhv_BlockBallOwner no region" );
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(execute) rect=(%.1f %.1f)(%.1f %.1f)",
                  M_bounding_region->left(), M_bounding_region->top(),
                  M_bounding_region->right(), M_bounding_region->bottom() );

    const WorldModel & wm = agent->world();

    //
    // get block point
    //
    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();

    Vector2D block_point = getBlockPoint( wm, opponent );

    if ( ! block_point.isValid() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(execute) no block point" );
        return false;
    }

    if ( ! M_bounding_region->contains( block_point ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(execute) out of bounds" );
        return false;
    }

    //
    // action
    //
    if ( ! doBodyAction( agent, opponent, block_point ) )
    {
        return false;
    }

    agent->setNeckAction( new Neck_CheckBallOwner() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BlockBallOwner::doBodyAction( PlayerAgent * agent,
                                  const PlayerObject * opponent,
                                  const Vector2D & block_point )
{
    const WorldModel & wm = agent->world();

    if ( opponent->distFromSelf() < 10.0
         && opponentIsBlocked( wm, opponent, block_point ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(doBodyAction) opponent is blocked. intercept." );
        agent->debugClient().addMessage( "Blocked:GetBall" );
        Body_Intercept( true ).execute( agent );
        return true;
    }

    double dist_thr = 1.0;

    agent->debugClient().addMessage( "Block" );
    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, dist_thr );

    double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );

    if ( Body_GoToPoint( block_point,
                         dist_thr,
                         dash_power,
                         -1.0, // dash speed
                         1, // cycle
                         true, // save recovery
                         15.0 // dir_thr
                         ).execute( agent ) )
    {
        S_wait_count = 0;
        agent->debugClient().addMessage( "GoTo%.0f",
                                         dash_power );
        dlog.addText( Logger::TEAM,
                      __FILE__":(doBodyAction) GoToPoint (%.1f %.1f) power=%.1f",
                      block_point.x, block_point.y,
                      dash_power );
        return true;
    }

    ++S_wait_count;

    dlog.addText( Logger::TEAM,
                  __FILE__":(doBodyAction) wait_count=%d",
                  S_wait_count );
    if ( S_wait_count >= 10 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(doBodyAction) waited. attack to the ball" );
        agent->debugClient().addMessage( "WaitOver:Intercept" );
        Body_Intercept( true ).execute( agent );
        return true;
    }

    AngleDeg body_angle = wm.ball().angleFromSelf() + 90.0;
    if ( body_angle.abs() < 90.0 )
    {
        body_angle += 180.0;
    }

    agent->debugClient().addMessage( "TurnTo%.0f",
                                     body_angle.degree() );
    dlog.addText( Logger::TEAM,
                  __FILE__":(doBodyAction) turn to anble=%.1f",
                  body_angle.degree() );

    Body_TurnToAngle( body_angle ).execute( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BlockBallOwner::opponentIsBlocked( const WorldModel & wm,
                                       const PlayerObject * opponent,
                                       const Vector2D & block_point )
{
    if ( ! opponent )
    {
        return false;
    }

    Vector2D opponent_pos = opponent->pos() + opponent->vel();
    const AbstractPlayerObject * blocker = FieldAnalyzer::get_blocker( wm,
                                                                       opponent_pos,
                                                                       block_point );
    if ( blocker )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (opponentIsBlocked) blocker candidate %d",
                      blocker->unum() );

        Vector2D blocker_pos = blocker->pos() + blocker->vel();
        if ( blocker_pos.dist2( opponent_pos ) < wm.self().pos().dist2( opponent_pos ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (opponentIsBlocked) exist blocker (1)" );
            return true;
        }
    }

    if ( Strategy::i().opponentType() == Strategy::Type_Gliders )
    {

    }
    else
    {
        const AbstractPlayerObject * marker = MarkAnalyzer::instance().getMarkerOf( opponent->unum() );
        if ( marker
             && marker->unum() != wm.self().unum() )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (opponentIsBlocked) marker %d",
                          marker->unum() );

            if ( marker->pos().dist2( ServerParam::i().ourTeamGoalPos() )
                 < opponent->pos().dist2( ServerParam::i().ourTeamGoalPos() ) )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": (opponentIsBlocked) exist blocker (2)" );
                return true;
            }
        }
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (opponentIsBlocked) no blocker" );

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_BlockBallOwner::getBlockPoint( const WorldModel & wm,
                                   const PlayerObject * opponent )
{
    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(getBlockPoint). no opponent" );
        return Vector2D::INVALIDATED;
    }

    //
    // estimate opponent info
    //
    Vector2D opp_pos = opponent->pos() + opponent->vel();
    Vector2D opp_target( -45.0, opp_pos.y * 0.9 );

    dlog.addText( Logger::TEAM,
                  __FILE__":(getBlockPoint) opp_pos=(%.1f %.1f) opp_target=(%.1f %.1f)",
                  opp_pos.x, opp_pos.y,
                  opp_target.x, opp_target.y );

    AngleDeg opp_body;
    if ( opponent->bodyCount() == 0
         || ( opponent->bodyCount() <= 3
              && opp_body.abs() > 150.0 ) )
    {
        opp_body = opponent->body();
        dlog.addText( Logger::TEAM,
                      __FILE__":(getBlockPoint) set opponent body %.0f",
                      opp_body.degree() );
    }
    else
    {
        opp_body = ( opp_target - opp_pos ).th();
        dlog.addText( Logger::TEAM,
                      __FILE__":(getBlockPoint) set opp target point(%.1f %.1f) as opp body %.0f",
                      opp_target.x, opp_target.y,
                      opp_body.degree() );

    }

    //
    // check intersection with current body line
    //
    const Vector2D opp_unit_vec = Vector2D::polar2vector( 1.0, opp_body );

    opp_target = opp_pos + opp_unit_vec * 10.0;

    dlog.addText( Logger::TEAM,
                  __FILE__":(getBlockPoint) set opp_target=(%.1f %.1f)",
                  opp_target.x, opp_target.y );

    Segment2D target_segment( opp_pos, //opp_pos + opp_unit_vec * 1.0,
                              opp_pos + opp_unit_vec * 3.0 ); //opp_pos + opp_unit_vec * 6.0 );

    Vector2D my_pos = wm.self().pos() + wm.self().vel();
    Segment2D my_move_segment( my_pos,
                               my_pos + Vector2D::polar2vector( 100.0, wm.self().body() ) );
    Vector2D intersection = my_move_segment.intersection( target_segment, true );

    //
    // set block point
    //
    Vector2D block_point( -50.0, 0.0 );

    if ( target_segment.dist( my_pos ) < 1.0 )
    {
        block_point = opp_pos + opp_unit_vec * 0.8;
        dlog.addText( Logger::TEAM,
                      __FILE__":(getBlockPoint) on block line" );
    }
    else if ( intersection.isValid() )
    {
        block_point = intersection;
        dlog.addText( Logger::TEAM,
                      __FILE__":(getBlockPoint) intersection" );
    }
    else
    {
        // block_point = opp_pos + opp_unit_vec * 4.0;
        block_point = opp_pos + opp_unit_vec * 1.5;
        dlog.addText( Logger::TEAM,
                      __FILE__":(getBlockPoint) no intersection" );
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(getBlockPoint) opponent=%d (%.1f, %.1f) block_point=(%.1f %.1f)",
                  opponent->unum(),
                  opponent->pos().x, opponent->pos().y,
                  block_point.x, block_point.y );

    return block_point;
}
