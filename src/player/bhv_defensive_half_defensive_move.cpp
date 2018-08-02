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

#include "bhv_defensive_half_defensive_move.h"

#include "strategy.h"
#include "mark_analyzer.h"
#include "defense_system.h"
#include "move_simulator.h"

#include "bhv_block_ball_owner.h"
#include "bhv_mid_fielder_mark_move.h"
#include "bhv_mid_fielder_free_move.h"
#include "bhv_defensive_half_avoid_mark_move.h"
#include "bhv_get_ball.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/body_intercept.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_turn_to_ball.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

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
Bhv_DefensiveHalfDefensiveMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_DefensiveHalfDefensiveMove" );

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

    // if ( doAttackBallOwner( agent ) )
    // {
    //     return false;
    // }

    // if ( doAvoidMark( agent ) )
    // {
    //     return true;
    // }

    // if ( doFreeMove( agent ) )
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
Bhv_DefensiveHalfDefensiveMove::doIntercept( PlayerAgent * agent )
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

    if ( mate_min == 1
         && self_min > 1 )
    {
        return false;
    }

    if ( mate_min == 2
         && self_min > 2 )
    {
        return false;
    }

    if ( self_min <= 2
         || ( self_min <= opp_min + 3
              && opp_min >= 1 )
         || ( wm.ball().vel().r() * std::pow( rcsc::ServerParam::i().ballDecay(), opp_min ) < 0.8
              && opp_min >= 3
              && self_min <= opp_min + 5 )
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept" );
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
Bhv_DefensiveHalfDefensiveMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( ! DefenseSystem::is_midfielder_block_situation( wm ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGetBall) no block situation" );
        return false;
    }

    //
    // force chase
    //
    const int self_step = wm.interceptTable()->selfReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( ! wm.kickableOpponent()
         && self_step <= opponent_step + 1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) force mode. intercept" );
        agent->debugClient().addMessage( "DH:Block:ForceIntercept" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }

    //
    // block
    //
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    Rect2D bounding_rect;
    if ( position_type == Position_Left )
    {
        bounding_rect
            = Rect2D( Vector2D( -52.5, home_pos.y - 25.0 ),
                      Vector2D( home_pos.x + 10.0, home_pos.y + 25.0 ) );
    }
    else if ( position_type == Position_Right )
    {
        bounding_rect
            = Rect2D( Vector2D( -52.5, home_pos.y - 25.0 ),
                      Vector2D( home_pos.x + 10.0, home_pos.y + 25.0 ) );
    }
    else
    {
        bounding_rect
            = Rect2D( Vector2D( -52.5, home_pos.y - 25.0 ),
                      Vector2D( home_pos.x + 10.0, home_pos.y + 25.0 ) );
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (GetBall) rect=(%.1f %.1f)(%.1f %.1f)",
                  bounding_rect.left(), bounding_rect.top(),
                  bounding_rect.right(), bounding_rect.bottom() );
    if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
    {
        agent->debugClient().addMessage( "DH:GetBall" );
        return true;
    }

    //
    // force block
    //
    if ( Bhv_BlockBallOwner( new Rect2D( bounding_rect ) ).execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) force mode. block" );
        agent->debugClient().addMessage( "DH:ForceBlock" );

        return true;
    }

    //
    // force move
    //

    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    Vector2D target_point = opponent_ball_pos;
    double dist_tolerance = std::max( 0.5, wm.ball().distFromSelf() * 0.1 );
    double dash_power = ServerParam::i().maxPower();
    double dir_tolerance = 25.0;

    if ( opponent_ball_pos.x > 3.0 ) target_point.x = 0.0;
    else if ( opponent_ball_pos.x > -2.0 ) target_point.x = -5.0;
    else if ( opponent_ball_pos.x > -7.0 ) target_point.x = -10.0;
    else if ( opponent_ball_pos.x > -12.0 ) target_point.x = -15.0;
    else if ( opponent_ball_pos.x > -17.0 ) target_point.x = -20.0;
    else if ( opponent_ball_pos.x > -22.0 ) target_point.x = -25.0;
    else if ( opponent_ball_pos.x > -27.0 ) target_point.x = -30.0;
    else if ( opponent_ball_pos.x > -32.0 ) target_point.x = -35.0;
    else if ( opponent_ball_pos.x > -37.0 ) target_point.x = -40.0;
    else target_point.x = -45.0;

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
Bhv_DefensiveHalfDefensiveMove::doMark( PlayerAgent * agent )
{
    static GameTime s_last_time( -100, 0 );

    const WorldModel & wm = agent->world();

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( opp_min <= mate_min
         // || ( s_last_time.cycle() == wm.time().cycle() - 1
         //      && ( opp_min < mate_min + 4
         //           || wm.getDistOpponentNearestToBall( 3 ) < 2.0 ) )
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doMark)" );
        if ( Bhv_MidFielderMarkMove().execute( agent ) )
        {
            s_last_time = wm.time();
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
Bhv_DefensiveHalfDefensiveMove::doAttackBallOwner( PlayerAgent * agent )
{
    if ( Strategy::i().opponentType() != Strategy::Type_Gliders )
    {
        return false;
    }

    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doAttackBallOwner) false: no opponent" );
        return false;
    }

    const AbstractPlayerObject * marker = MarkAnalyzer::i().getMarkerOf( opponent );

    if ( ! marker )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doAttackBallOwner) false: my marker" );
        return false;
    }

    if ( marker
         && marker->unum() == wm.self().unum() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doAttackBallOwner) false: my mark target" );
        return false;
    }

    const int t_step = wm.interceptTable()->teammateReachStep();
    const int o_step = wm.interceptTable()->opponentReachStep();

    if ( wm.kickableTeammate()
         || t_step == 0
         || t_step < o_step -1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doAttackBallOwner) false: our ball" );
        return false;
    }

    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( o_step );

    //
    // find the teammate nearest to the ball (excludes marker & goalie)
    //
    const AbstractPlayerObject * nearest = static_cast< const AbstractPlayerObject * >( 0 );
    double nearest_dist = 100000.0;

    for ( AbstractPlayerObject::Cont::const_iterator t = wm.ourPlayers().begin(), end = wm.ourPlayers().end();
          t != end;
          ++t )
    {
        if ( *t == marker
             || (*t)->goalie() )
        {
            continue;
        }

        double dist = (*t)->pos().dist( opponent_ball_pos );
        if ( dist < nearest_dist )
        {
            nearest_dist = dist;
            nearest = *t;
        }
    }

    if ( ! nearest )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doAttackBallOwner) false: no candidate" );
        return false;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doAttackBallOwner) marker=%d nearest=%d",
                  marker->unum(), nearest->unum() );

    if ( nearest->unum() != wm.self().unum() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doAttackBallOwner) false: exist other attacker" );
        return false;
    }

    //
    // calculate target point
    //

    const Vector2D virtual_target( -46.0, 0.0 );
    const double max_dist = opponent_ball_pos.dist( virtual_target );

    Vector2D target_point = virtual_target;

    for ( int i = 0; i < 20; ++i )
    {
        const double rate = i / 20.0;
        target_point = opponent_ball_pos + ( virtual_target - opponent_ball_pos ) * rate;

        const int self_step = MoveSimulator::simulate_self_turn_dash( wm, target_point, 1.5, false, 20, NULL );

        if ( self_step < 0 )
        {
            continue;
        }


        const double opponent_move_dist = max_dist * rate;
        const int opponent_step
            = opponent->ballReachStep()
            + 3 // kick + turn +...
            + opponent->playerTypePtr()->cyclesToReachDistance( opponent_move_dist );

        dlog.addText( Logger::ROLE,
                      "-- (%.1f %.1f) self_step=%d opp_step=%d",
                      target_point.x, target_point.y,
                      self_step, opponent_step );

        if ( self_step <= opponent_step * 1.1 )
        {
            break;
        }
    }


    dlog.addText( Logger::ROLE,
                  __FILE__":(doAttackBallOwner) target=(%.1f %.1f)",
                  target_point.x, target_point.y );

    double dist_tolerance = std::max( 0.5, wm.ball().distFromSelf() * 0.1 );
    double dash_power = ServerParam::i().maxPower();
    double dir_tolerance = 25.0;

    agent->debugClient().addMessage( "DH:AttackBall" );
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
Bhv_DefensiveHalfDefensiveMove::doFreeMove( PlayerAgent * agent )
{
    return Bhv_MidFielderFreeMove().execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfDefensiveMove::doAvoidMark( PlayerAgent * agent )
{
    return Bhv_DefensiveHalfAvoidMarkMove().execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfDefensiveMove::doDefaultMove( PlayerAgent * agent )
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
                              __FILE__":(doDefaultMove) dash_power, teammate kickable, stamina save" );
            }
        }
    }
    else if ( trap_pos.x < wm.self().pos().x ) // ball is behind
    {
        dash_power *= 0.9;
        dlog.addText( Logger::ROLE,
                      __FILE__":(doDefaultMove) dash_power, trap_pos is behind. trap_pos=(%.1f %.1f)",
                      trap_pos.x, trap_pos.y );
    }
    else if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.8 )
    {
        dash_power *= 0.8;
        dlog.addText( Logger::ROLE,
                      __FILE__":(doDefaultMove) dash_power, enough stamina" );
    }
    else
    {
        dash_power = ( wm.self().playerType().staminaIncMax()
                       * wm.self().recovery()
                       * 0.9 );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doDefaultMove) dash_power, default" );
    }

    // save recovery
    dash_power = wm.self().getSafetyDashPower( dash_power );

    //
    // register action
    //

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    dlog.addText( Logger::ROLE,
                  __FILE__":(doDefaultMove) go to home (%.1f %.1f) dist_thr=%.3f. dash_power=%.1f",
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
        agent->debugClient().addMessage( "CH:Default:Go%.0f", dash_power );
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
        agent->debugClient().addMessage( "CH:Default:Turn%.0f",
                                         body_angle.degree() );
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );

    return true;
}
