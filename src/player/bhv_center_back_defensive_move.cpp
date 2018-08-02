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

#include "bhv_center_back_defensive_move.h"

#include "strategy.h"
#include "mark_analyzer.h"
#include "defense_system.h"

#include "bhv_defender_mark_move.h"
#include "bhv_get_ball.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>
#include <rcsc/action/view_synch.h>

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
Bhv_CenterBackDefensiveMove::execute( PlayerAgent * agent )
{
    //
    // intercept
    //
    if ( doIntercept( agent ) )
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

    if ( doMark( agent ) )
    {
        return true;
    }

    doNormalMove( agent );
    return true;

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackDefensiveMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.kickableTeammate() )
    {
        return false;
    }

    // chase ball
    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    const double ball_speed = wm.ball().vel().r() * std::pow( ServerParam::i().ballDecay(), opp_min );


    bool intercept = false;

    if ( self_min <= mate_min + 1
         && self_min <= opp_min + 1 )
    {
        intercept = true;
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true (1)" );
    }

    if ( ! intercept
         && opp_min >= 2
         && self_min <= opp_min + 1
         && self_min <= mate_min + 1 )
    {
        intercept = true;
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true (2)" );
    }

    if ( ! intercept
         && opp_min >= 3
         && self_min <= opp_min + 1
         && self_min <= mate_min * 1.3
         && ball_speed < 0.8 )
    {
        intercept = true;
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true (3)" );
    }

    if ( ! intercept
         && opp_min >= 4
         && self_min <= opp_min + 2
         && self_min <= mate_min + 2
         && ball_speed < 0.8 )
    {
        intercept = true;
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true (4)" );
    }

    if ( self_min >= opp_min + 1
         &&  ball_speed > 1.5 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept cancel" );
        intercept = false;
    }

    if ( intercept )
    {
        agent->debugClient().addMessage( "CB:Def:Intercept" );
        Body_Intercept().execute( agent );

        if ( opp_min >= self_min + 3 )
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
Bhv_CenterBackDefensiveMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    if ( ! fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no opponent" );
        return false;
    }

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );
    if ( mark_target
         && mark_target != fastest_opp
         && mark_target->distFromSelf() < fastest_opp->distFromSelf() + 2.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) mark target is not the fastest player." );
        return false;
    }

    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( teammate_step < opponent_step )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) our ball" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    if ( std::fabs( opponent_ball_pos.y - home_pos.y ) > 20.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no get ball situation (1)" );
        return false;
    }

    if ( Strategy::i().opponentType() == Strategy::Type_Gliders )
    {

    }
    else if ( opponent_ball_pos.absY() > 23.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no get ball situation (2)" );
        return false;
    }

    // const AbstractPlayerObject * nearest_to_home_opp = wm.getOpponentNearestTo( home_pos, 10, NULL );
    // if ( nearest_to_home_opp
    //      && nearest_to_home_opp != fastest_opp
    //      && nearest_to_home_opp->pos().x < ball_pos.x + 2.0 )
    // {
    //     dlog.addText( Logger::ROLE,
    //                   __FILE__": (doGetBall) exist other attacker %d (%.1f %.1f)",
    //                   nearest_to_home_opp->unum(),
    //                   nearest_to_home_opp->pos().x, nearest_to_home_opp->pos().y );
    //     return false;
    // }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGetBall) try Bhv_GetBall" );

    const double max_x = ( Strategy::i().opponentType() == Strategy::Type_WrightEagle
                           || Strategy::i().opponentType() == Strategy::Type_Oxsy )
        ? std::min( home_pos.x + 4.0, wm.self().pos().x + 1.0 )
        : home_pos.x + 4.0;
    const double min_y = home_pos.y - 20.0;
    const double max_y = home_pos.y + 20.0;

    const Rect2D bounding_rect( Vector2D( -50.0, min_y ),
                                Vector2D( max_x, max_y ) );
    if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
    {
        agent->debugClient().addMessage( "CB:Def:GetBall" );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) done Bhv_GetBall" );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGetBall) could not find the position." );

#if 1
    // 2009-06-24
    Vector2D base_point( - ServerParam::i().pitchHalfLength() - 5.0, 0.0 );

    Vector2D rel_vec = ( opponent_ball_pos - base_point );
    Vector2D target_point = base_point + rel_vec.setLengthVector( std::min( 10.0, rel_vec.r() - 1.0 ) );

    double dist_thr = wm.self().playerType().kickableArea();
    if ( Body_GoToPoint( target_point,
                         dist_thr, // reduced dist thr
                         ServerParam::i().maxDashPower(),
                         -1.0, // dash speed
                         100,
                         true,
                         15.0 // dir threshold
                         ).execute( agent ) )
    {
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );
        agent->debugClient().addMessage( "GetBall:EmergencyGoTo" );
        dlog.addText( Logger::BLOCK,
                      __FILE__": emergency (%.1f %.1f)",
                      target_point.x, target_point.y )
            ;
        if ( wm.ball().distFromSelf() < 4.0 )
        {
            agent->setViewAction( new View_Synch() );
        }
        agent->setNeckAction( new Neck_CheckBallOwner() );

        return true;
    }
#endif
    return false;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackDefensiveMove::doMark( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    bool mark = false;
    if ( opponent_step <= teammate_step + 2 )
    {
        mark = true;
        dlog.addText( Logger::ROLE,
                      __FILE__":(doMark) opponent may intercept the ball" );
    }

    if ( ! mark )
    {
        const PlayerObject * nearest_opponent = wm.getOpponentNearestToBall( 3, false );
        if ( nearest_opponent
             && nearest_opponent->distFromBall() < ServerParam::i().tackleDist() )
        {
            mark = true;
            dlog.addText( Logger::ROLE,
                          __FILE__":(doMark) nearest opponent may be tacklable" );
        }
    }

    if ( mark )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doMark) try" );
        if ( Bhv_DefenderMarkMove().execute( agent ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doMark) done" );
            return true;
        }
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doMark) failed" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_CenterBackDefensiveMove::doNormalMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    Vector2D target_point = home_pos;

    if ( wm.ourDefenseLineX() < target_point.x )
    {
        for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
                  end = wm.opponentsFromSelf().end();
              o != end;
              ++o )
        {
            if ( (*o)->isGhost() ) continue;
            if ( (*o)->posCount() >= 10 ) continue;
            if ( wm.ourDefenseLineX() < (*o)->pos().x
                 && (*o)->pos().x < target_point.x )
            {
                target_point.x = std::max( home_pos.x - 2.0, (*o)->pos().x - 0.5 );
            }
        }
    }

    double dash_power = DefenseSystem::get_defender_dash_power( wm, target_point );
    double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "CB:Def:Normal" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    doGoToPoint( agent, target_point, dist_thr, dash_power, 12.0 );

    agent->setNeckAction( new Neck_CheckBallOwner() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_CenterBackDefensiveMove::doGoToPoint( PlayerAgent * agent,
                                          const Vector2D & target_point,
                                          const double & dist_thr,
                                          const double & dash_power,
                                          const double & dir_thr )
{
    const WorldModel & wm = agent->world();

    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                         -1.0, // dash speed
                         1, // 1 step
                         true, // save recovery
                         dir_thr
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "CB:Def:Go%.1f", dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__": GoToPoint (%.1f %.1f) dash_power=%.1f dist_thr=%.2f",
                      target_point.x, target_point.y,
                      dash_power,
                      dist_thr );
        return;
    }

    // already there

    Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    Vector2D my_final = wm.self().inertiaFinalPoint();
    AngleDeg ball_angle = ( ball_next - my_final ).th();

    AngleDeg target_angle;
    if ( ball_next.x < -30.0 )
    {
        target_angle = ball_angle + 90.0;
        if ( ball_next.x < -45.0 )
        {
            if ( target_angle.degree() < 0.0 )
            {
                target_angle += 180.0;
            }
        }
        else
        {
            if ( target_angle.degree() > 0.0 )
            {
                target_angle += 180.0;
            }
        }
    }
    else
    {
        target_angle = ball_angle + 90.0;
        if ( ball_next.x > my_final.x + 15.0 )
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
    }

    Body_TurnToAngle( target_angle ).execute( agent );

    agent->debugClient().addMessage( "SB:Def:TurnTo%.0f",
                                     target_angle.degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doGoToPoint) turn to angle=%.1f",
                  target_angle.degree() );
}
