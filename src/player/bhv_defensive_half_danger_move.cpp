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

#include "bhv_defensive_half_danger_move.h"

#include "strategy.h"
#include "mark_analyzer.h"

#include "bhv_get_ball.h"
#include "bhv_mid_fielder_mark_move.h"
#include "bhv_defensive_half_avoid_mark_move.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/arm_point_to_point.h>

#include <rcsc/action/body_intercept.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/soccer_intention.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfDangerMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_DefensiveHalfDangerMove" );

    if ( doIntercept( agent ) )
    {
        return true;
    }

    if ( doGetBall( agent ) )
    {
        return true;
    }

    if ( doMark( agent ) )
    {
        return true;
    }

    // if ( doAvoidMark( agent ) )
    // {
    //     return true;
    // }

    doDefaultMove( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfDangerMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.kickableTeammate() )
    {
        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min > mate_min + 2 )
    {
        return false;
    }
    // if ( wm.kickableOpponent()
    //      && self_min >= 2 )
    // {
    //     return false;
    // }

    const double ball_speed = wm.ball().vel().r() * std::pow( rcsc::ServerParam::i().ballDecay(), opp_min );

    bool intercept = false;
    if ( self_min <= 3
         || self_min <= opp_min + 3
         || ( ball_speed < 0.8
              && opp_min >= 3
              && self_min <= opp_min + 5 )
         )
    {
        intercept = true;
    }

#if 0
    if ( intercept
         && opp_min <= self_min - 2 )
    {
        Vector2D opponent_trap_ball_pos = wm.ball().inertiaPoint( opp_min );
        Vector2D opponent_trap_self_pos = wm.self().inertiaPoint( opp_min );

        Vector2D self_trap_ball_pos = wm.ball().inertiaPoint( self_min );
        Vector2D self_trap_self_pos = wm.self().inertiaPoint( self_min );

        AngleDeg opponent_trap_ball_angle = ( opponent_trap_ball_pos - opponent_trap_self_pos ).th() - wm.self().body();
        AngleDeg self_trap_ball_angle = ( self_trap_ball_pos - self_trap_self_pos ).th() - wm.self().body();

        if ( opponent_trap_ball_angle.abs() > 90.0
             || self_trap_ball_angle.abs() > 90.0
             || opponent_trap_self_pos.dist( opponent_trap_ball_pos ) * opponent_trap_ball_angle.sin()
             > wm.self().playerType().kickableArea() - 0.2
             || self_trap_self_pos.dist( self_trap_ball_pos ) * self_trap_ball_angle.sin()
             > wm.self().playerType().kickableArea() - 0.2 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doIntercept) xxx cancel intercept" );
            intercept = false;
        }
    }
#endif

    if ( intercept )
    {
        agent->debugClient().addMessage( "DH:Danger:Intercept" );
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept" );
        Body_Intercept().execute( agent );

        if ( opp_min >= self_min + 3 )
        {
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        }
        else
        {
            agent->setNeckAction( new Neck_DefaultInterceptNeck() );
        }

        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfDangerMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    const AbstractPlayerObject * first_opponent = wm.interceptTable()->fastestOpponent();
    if ( ! first_opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no opponent" );
        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( wm.kickableTeammate()
         || mate_min < opp_min
         || self_min <= opp_min )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGetBall) no GetBall situation" );
        return false;
    }

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );
    if ( mark_target
         && mark_target != first_opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGetBall) mark target is not the fastest player." );
        return false;
    }

#if 1
    // 2009-07-04

    //
    // check other blocker
    //
    {
        const Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );
        const Vector2D mid_point = opp_trap_pos * 0.9 + Vector2D( -50.0, 0.0 ) * 0.1;
        const PlayerObject * other_blocker = static_cast< PlayerObject * >( 0 );
        double my_dist = wm.self().pos().dist( mid_point );
        double min_dist = my_dist;

        for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromSelf().begin(),
                  end = wm.teammatesFromSelf().end();
              t != end;
              ++t )
        {
            if ( (*t)->posCount() >= 10 ) continue;
            // if ( (*t)->pos().x > opp_trap_pos.x - 1.0 ) continue;

            double d = (*t)->pos().dist( mid_point );
            if ( d < min_dist )
            {
                min_dist = d;
                other_blocker = *t;
            }
        }

        if ( my_dist > min_dist * 1.2 )
        {
            if ( other_blocker )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__": (doGetBall) exist other blocker=%d (%.1f %.1f)",
                              other_blocker->unum(),
                              other_blocker->pos().x, other_blocker->pos().y );
            }
            return false;
        }
    }
#endif

    //
    // get ball
    //
    Rect2D bounding_rect;
    if ( position_type == Position_Left )
    {
        bounding_rect
            = Rect2D( Vector2D( home_pos.x - 10.0, home_pos.y - 15.0 ),
                      Vector2D( home_pos.x + 6.0, home_pos.y + 9.5 ) );
    }
    else if ( position_type == Position_Right )
    {
        bounding_rect
            = Rect2D( Vector2D( home_pos.x - 10.0, home_pos.y - 9.5 ),
                      Vector2D( home_pos.x + 6.0, home_pos.y + 15.0 ) );
    }
    else
    {
        bounding_rect
            = Rect2D( Vector2D( home_pos.x - 10.0, home_pos.y - 15.0 ),
                      Vector2D( home_pos.x + 6.0, home_pos.y + 15.0 ) );
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": try GetBall. rect=(%.1f %.1f)(%.1f %.1f)",
                  bounding_rect.left(), bounding_rect.top(),
                  bounding_rect.right(), bounding_rect.bottom() );
    if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
    {
        agent->debugClient().addMessage( "DH:Danger:GetBall" );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": xxx failed GetBall" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfDangerMove::doMark( rcsc::PlayerAgent * agent )
{
   const WorldModel & wm = agent->world();

    const int teammate_step = wm.interceptTable()->teammateReachCycle();
    int opponent_step = wm.interceptTable()->opponentReachCycle();

    const PlayerObject * o = wm.getOpponentNearestToBall( 5 );
    if ( o
         && o->distFromBall() < ( o->playerTypePtr()->kickableArea()
                                  + o->distFromSelf() * 0.05
                                  + wm.ball().distFromSelf() * 0.05 ) )
    {
        opponent_step = 0;
    }


    if ( opponent_step <= teammate_step + 1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doMark)" );
        if ( Bhv_MidFielderMarkMove().execute( agent ) )
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
bool
Bhv_DefensiveHalfDangerMove::doAvoidMark( PlayerAgent * agent )
{
    if ( agent->world().ball().pos().x > -36.0 )
    {
        return Bhv_DefensiveHalfAvoidMarkMove().execute( agent );
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doAvoidMark) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfDangerMove::doDefaultMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    Vector2D trap_pos = wm.ball().inertiaPoint( std::min( mate_min, opp_min ) );
    double dash_power = ServerParam::i().maxDashPower();

    //
    // decide dash_power
    //
    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        dash_power = std::min( ServerParam::i().maxDashPower(),
                               wm.self().stamina() + wm.self().playerType().extraStamina() );
    }
    else if ( wm.kickableTeammate() )
    {
        if ( wm.self().pos().dist( trap_pos ) > 10.0 )
        {
            if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.8 )
            {
                dash_power = ( wm.self().playerType().staminaIncMax()
                               * wm.self().recovery() );
                dlog.addText( Logger::ROLE,
                              __FILE__": dash_power, teammate kickable, stamina save" );
            }
        }
    }
    else if ( trap_pos.x < wm.self().pos().x ) // ball is behind
    {
        dash_power *= 0.9;
        dlog.addText( Logger::ROLE,
                      __FILE__": dash_power, trap_pos is behind. trap_pos=(%.1f %.1f)",
                      trap_pos.x, trap_pos.y );
    }
    else if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.8 )
    {
        dash_power *= 0.8;
        dlog.addText( Logger::ROLE,
                      __FILE__": dash_power, enough stamina" );
    }
    else
    {
        dash_power = ( wm.self().playerType().staminaIncMax()
                       * wm.self().recovery()
                       * 0.9 );
        dlog.addText( Logger::ROLE,
                      __FILE__": dash_power, default" );
    }

    // save recovery
    dash_power = wm.self().getSafetyDashPower( dash_power );

    //
    // register action
    //

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    dlog.addText( Logger::ROLE,
                  __FILE__": go to home (%.1f %.1f) dist_thr=%.3f. dash_power=%.1f",
                  home_pos.x, home_pos.y,
                  dist_thr,
                  dash_power );

    agent->debugClient().setTarget( home_pos );
    agent->debugClient().addCircle( home_pos, dist_thr );

    if ( Body_GoToPoint( home_pos, dist_thr, dash_power,
                         -1.0, // dash speed
                         1 // 1 step
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "CH:DefMove:Go%.0f", dash_power );
    }
    else
    {
        AngleDeg body_angle = 0.0;
        if ( wm.ball().angleFromSelf().abs() > 80.0 )
        {
            body_angle = ( wm.ball().pos().y > wm.self().pos().y
                           ? 90.0
                           : -90.0 );
        }
        Body_TurnToAngle( body_angle ).execute( agent );
        agent->debugClient().addMessage( "CH:DefMove:Turn%.0f",
                                         body_angle.degree() );
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );

    return true;
}
