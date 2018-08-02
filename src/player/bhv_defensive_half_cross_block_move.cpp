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

#include "bhv_defensive_half_cross_block_move.h"

#include "strategy.h"
#include "defense_system.h"
#include "mark_analyzer.h"

#include "bhv_get_ball.h"
#include "bhv_defender_mark_move.h"
#include "bhv_mid_fielder_mark_move.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

#include <limits>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfCrossBlockMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_DefensiveHalfCrossBlockMove" );

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

    //
    // mark opponent attacker
    //
    if ( doMark( agent ) )
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
Bhv_DefensiveHalfCrossBlockMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min <= opp_min + 1
         && self_min <= mate_min + 1
         && ! wm.kickableTeammate() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) performed" );
        agent->debugClient().addMessage( "CH:CrossBlock:Intercept" );
        Body_Intercept().execute( agent );
        if ( opp_min >= self_min + 3 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doIntercept) offensive turn_neck" );
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        }
        else
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doIntercept) default turn_neck" );
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
Bhv_DefensiveHalfCrossBlockMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int self_step = wm.interceptTable()->selfReachStep();
    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( wm.kickableTeammate()
         || opponent_step > teammate_step
         || opponent_step > self_step )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) our ball" );
        return false;
    }

    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    if ( ! fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no opponent" );
        return false;
    }

    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

    if ( ! mark_target
         || mark_target->unum() != fastest_opp->unum() )
    {
        if ( opponent_ball_pos.x > -30.0
             || opponent_ball_pos.dist( home_pos ) > 7.0
             || opponent_ball_pos.absY() > 13.0
             )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doGetBall) no get ball situation" );
            return false;
        }
    }

    //
    // search other blocker
    //
    bool exist_blocker = false;

    const double my_dist = wm.self().pos().dist( opponent_ball_pos );
    for ( PlayerObject::Cont::const_iterator p = wm.teammatesFromBall().begin(),
              end = wm.teammatesFromBall().end();
          p != end;
          ++p )
    {
        if ( (*p)->goalie() ) continue;
        if ( (*p)->isGhost() ) continue;
        if ( (*p)->posCount() >= 3 ) continue;
        if ( (*p)->pos().x > fastest_opp->pos().x ) continue;
        if ( (*p)->pos().x > opponent_ball_pos.x ) continue;
        if ( (*p)->pos().dist( opponent_ball_pos ) > my_dist ) continue;

        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) exist other blocker %d (%.1f %.1f)",
                      (*p)->unum(),
                      (*p)->pos().x, (*p)->pos().y );
        exist_blocker = true;
        break;
    }

    //
    // try intercept
    //
    if ( exist_blocker )
    {
        int self_step = wm.interceptTable()->selfReachCycle();
        Vector2D self_ball_pos = wm.ball().inertiaPoint( self_step );
        if ( self_step <= 10
             && self_ball_pos.dist( home_pos ) < 10.0 )
        {
            agent->debugClient().addMessage( "CH:CrossBlock:GetBall:Intercept" );
            Body_Intercept().execute( agent );
            if ( opponent_step >= self_step + 3 )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__": (doIntercept) offensive turn_neck" );
                agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
            }
            else
            {
                dlog.addText( Logger::ROLE,
                              __FILE__": (doIntercept) default turn_neck" );
                agent->setNeckAction( new Neck_DefaultInterceptNeck
                                      ( new Neck_TurnToBallOrScan( 0 ) ) );
            }
            return true;
        }

        return false;
    }

    //
    // try get ball
    //
    double max_x = -34.0;
    Rect2D bounding_rect( Vector2D( -60.0, home_pos.y - 6.0 ),
                          Vector2D( max_x, home_pos.y + 6.0 ) );
    if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
    {
        agent->debugClient().addMessage( "CH:CrossBlock:GetBall" );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) performed" );
        return true;
    }

    if ( ! mark_target
         || fastest_opp->unum() != mark_target->unum() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) false" );
        return false;
    }


    if ( ! wm.kickableOpponent()
         && self_step <= opponent_step + 1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) force mode. intercept" );
        agent->debugClient().addMessage( "DH:CrossBrock:ForceIntercept" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }


    Vector2D target_point = opponent_ball_pos;
    double dist_tolerance = std::max( 0.5, wm.ball().distFromSelf() * 0.1 );
    double dash_power = ServerParam::i().maxPower();
    double dir_tolerance = 2.0;

    if ( target_point.absY() > 23.0 ) target_point.y = 20.0;
    else if ( target_point.absY() > 18.0 ) target_point.y = 15.0;
    else if ( target_point.x > 13.0 ) target_point.x = 8.0;
    else if ( target_point.x > 8.0 ) target_point.x = 5.0;
    else target_point.y = 0.0;

    if ( opponent_ball_pos.y < 0.0 ) target_point.y *= -1.0;

    if ( target_point.x > 40.0 ) target_point.x = 40.0;

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGetBall) force move. target=(%.2f %.2f)",
                  target_point.x, target_point.y );
    agent->debugClient().addMessage( "DH:Block:ForceMove" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_tolerance );

    if ( ! Body_GoToPoint( target_point, dist_tolerance, dash_power,
                           -1.0, 100, true, // speed, cycke, recovery
                           dir_tolerance ).execute( agent ) )
    {
        Body_TurnToBall().execute( agent );
    }

    agent->setNeckAction( new Neck_CheckBallOwner() );

    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfCrossBlockMove::doMark( PlayerAgent * agent )
{
    //return Bhv_DefenderMarkMove().execute( agent );
    return Bhv_MidFielderMarkMove().execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_DefensiveHalfCrossBlockMove::doNormalMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );

    double dash_power = DefenseSystem::get_defender_dash_power( wm, target_point );
    double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    doGoToPoint( agent, target_point, dist_thr, dash_power, 12.0 );

    agent->setNeckAction( new Neck_CheckBallOwner() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_DefensiveHalfCrossBlockMove::doGoToPoint( PlayerAgent * agent,
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
        agent->debugClient().addMessage( "CH:CrossBlock:Go%.1f", dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGoToPoint) (%.1f %.1f) dash_power=%.1f dist_thr=%.2f",
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

    agent->debugClient().addMessage( "CH:CrossBlock:TurnTo%.0f",
                                     target_angle.degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doGoToPoint) turn to angle=%.1f",
                  target_angle.degree() );
}
