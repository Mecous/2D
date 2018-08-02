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

#include "bhv_center_back_offensive_move.h"

#include "strategy.h"
#include "mark_analyzer.h"
#include "defense_system.h"

#include "bhv_defender_mark_move.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
// #include <rcsc/action/neck_turn_to_ball_and_player.h>
#include <rcsc/action/body_intercept.h>

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
Bhv_CenterBackOffensiveMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_CenterBackOffensiveMove" );

    //
    // intercept
    //
    if ( doIntercept( agent ) )
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

    doNormalMove( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterBackOffensiveMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.kickableTeammate()
         || wm.kickableOpponent() )
    {
        return false;
    }

    const int self_step = wm.interceptTable()->selfReachCycle();
    const int teammate_step = wm.interceptTable()->teammateReachCycle();
    const int opponent_step = wm.interceptTable()->opponentReachCycle();

    const PlayerObject * teammate = wm.interceptTable()->fastestTeammate();
    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();

    if ( self_step > teammate_step + 2 )
    {
        return false;
    }

    bool intercept = false;

    if ( self_step <= opponent_step
         || ( opponent_step >= 3 && self_step <= opponent_step + 3 )
         || ( ( ( opponent_step >= 3 && self_step <= opponent_step + 4 )
                || ( opponent_step >= 2 && self_step <= opponent_step + 3 ) )
              && wm.ball().vel().r() * std::pow( ServerParam::i().ballDecay(), opponent_step ) < 0.8 )
         )
    {
        intercept = true;
    }

    if ( intercept )
    {
        agent->debugClient().addMessage( "CB:Off:Intercept" );
        dlog.addText( Logger::ROLE,
                      __FILE__": self=%d t=%d,count=%d o=%d,count=%d",
                      self_step,
                      teammate_step, ( teammate ? teammate->posCount() : -1 ),
                      opponent_step, ( opponent ? opponent->posCount() : -1 ) );
        Body_Intercept().execute( agent );

        if ( opponent_step >= self_step + 3 )
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
Bhv_CenterBackOffensiveMove::doMarkMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const int teammate_step = wm.interceptTable()->teammateReachCycle();
    const int opponent_step = wm.interceptTable()->opponentReachCycle();

    if ( teammate_step <= opponent_step - 3 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doMarkMove) our ball" );
        return false;
    }

    return Bhv_DefenderMarkMove().execute( agent );
}


/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_CenterBackOffensiveMove::doNormalMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    Vector2D target_point = home_pos;

    target_point.x += 0.7;

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
    double tolerance = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
    if ( tolerance < 0.5 ) tolerance = 0.5;

    agent->debugClient().addMessage( "CB:Off:Normal" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, tolerance );

    doGoToPoint( agent, target_point, tolerance, dash_power, 12.0 );

    agent->setNeckAction( new Neck_CheckBallOwner() );

}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_CenterBackOffensiveMove::doGoToPoint( PlayerAgent * agent,
                                          const Vector2D & target_point,
                                          const double dist_tolerance,
                                          const double dash_power,
                                          const double dir_tolerance )
{
    if ( Body_GoToPoint( target_point, dist_tolerance, dash_power,
                         -1.0, // dash speed
                         1, true, dir_tolerance
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Go%.1f", dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__" (doGoToPoint) (%.2f %.2f) dash_power=%.1f dist_tolerance=%.2f dir_tolerance=%.2f",
                      target_point.x, target_point.y,
                      dash_power,
                      dist_tolerance,
                      dir_tolerance );
        return;
    }

    // already there
    // turn to somewhere

    const WorldModel & wm = agent->world();

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
        else if ( target_angle.degree() > 0.0 )
        {
            target_angle += 180.0;
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

    agent->debugClient().addMessage( "TurnTo%.0f", target_angle.degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doGoToPoint) turn to %.1f",
                  target_angle.degree() );
}
