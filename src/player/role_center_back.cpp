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

#include "role_center_back.h"

#include "strategy.h"
#include "mark_analyzer.h"
#include "defense_system.h"

#include "bhv_chain_action.h"

#include "bhv_center_back_danger_move.h"
#include "bhv_center_back_defensive_move.h"
#include "bhv_center_back_offensive_move.h"

#include "bhv_tactical_intercept.h"
#include "bhv_defender_basic_block_move.h"

#include "bhv_basic_move.h"

#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_go_to_point_look_ball.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>

#include <rcsc/formation/formation.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

using namespace rcsc;

const std::string RoleCenterBack::NAME( "CenterBack" );

/*-------------------------------------------------------------------*/
/*!

 */
namespace {
rcss::RegHolder role = SoccerRole::creators().autoReg( &RoleCenterBack::create,
                                                       RoleCenterBack::NAME );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleCenterBack::execute( PlayerAgent * agent )
{
    bool kickable = agent->world().self().isKickable();
    if ( agent->world().kickableTeammate()
         && agent->world().teammatesFromBall().front()->distFromBall()
         < agent->world().ball().distFromSelf() )
    {
        kickable = false;
    }

    if ( kickable )
    {
        doKick( agent );
    }
    else
    {
        doMove( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleCenterBack::doKick( PlayerAgent * agent )
{
    if ( Bhv_ChainAction().execute( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (execute) do chain action" );
        agent->debugClient().addMessage( "ChainAction" );
        return;
    }

    Body_HoldBall().execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleCenterBack::doMove( PlayerAgent * agent )
{
#ifdef USE_TACTICAL_INTERCEPT
    if ( Bhv_TacticalIntercept().execute( agent ) )
    {
        return;
    }
#endif

    switch ( Strategy::get_ball_area( agent->world() ) ) {
    case BA_Danger:
    case BA_CrossBlock:
        Bhv_CenterBackDangerMove().execute( agent );
        break;
    case BA_DefMidField:
    case BA_DribbleBlock:
        Bhv_CenterBackDefensiveMove().execute( agent );
        break;
    case BA_DribbleAttack:
    case BA_OffMidField:
    case BA_Cross:
    case BA_ShootChance:
        Bhv_CenterBackOffensiveMove().execute( agent );
        //doBasicMove( agent );
        break;
    default:
        dlog.addText( Logger::ROLE,
                      __FILE__": unknown ball pos" );
        Bhv_BasicMove().execute( agent );
        break;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleCenterBack::doBasicMove( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": doBasicMove" );

    const WorldModel & wm = agent->world();

    //--------------------------------------------------
    // check intercept chance
    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ! wm.kickableTeammate()
         && mate_min > 0 )
    {
        int intercept_state = 0;
        if ( self_min <= 1 && opp_min >= 1 )
        {
            intercept_state = 1;
        }
        else if ( self_min <= 2
                  && opp_min >= 3 )
        {
            intercept_state = 2;
        }
        else if ( self_min <= 3
                  && opp_min >= 4 )
        {
            intercept_state = 3;
        }
        else if ( self_min < 20
                  && self_min < mate_min
                  && ( self_min <= opp_min - 1
                       && opp_min >= 2 ) )
        {
            intercept_state = 4;
        }
        else if ( opp_min >= 2
                  && self_min <= opp_min + 1
                  && self_min <= mate_min )
        {
            intercept_state = 5;
        }

        Vector2D intercept_pos = wm.ball().inertiaPoint( self_min );
        if ( self_min < 30
             && wm.self().pos().x < -20.0
             && intercept_pos.x < wm.self().pos().x + 1.0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": doBasicMove reset intercept state %d",
                          intercept_state );
            intercept_state = 0;

            if ( self_min <= opp_min + 1
                 && opp_min >= 2
                 && self_min <= mate_min )
            {
                intercept_state = 9;
            }
            else if ( self_min <= opp_min - 3
                      && self_min <= mate_min )
            {
                intercept_state = 10;
            }
            else if ( self_min <= opp_min - 3
                      && self_min <= mate_min )
            {
                intercept_state = 12;
            }
            else if ( self_min <= 1 )
            {
                intercept_state = 13;
            }
        }

        if ( intercept_state != 0 )
        {
            // chase ball
            dlog.addText( Logger::ROLE,
                          __FILE__": doBasicMove intercept. state=%d",
                          intercept_state );
            agent->debugClient().addMessage( "CBBasicMove:Intercept%d",
                                             intercept_state );
            Body_Intercept().execute( agent );
            const PlayerObject * opp = ( wm.opponentsFromBall().empty()
                                         ? NULL
                                         : wm.opponentsFromBall().front() );
            if ( opp && opp->distFromBall() < 2.0 )
            {
                agent->setNeckAction( new Neck_TurnToBall() );
            }
            else
            {

                if ( opp_min >= self_min + 3 )
                {
                    agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
                }
                else
                {
                    agent->setNeckAction( new Neck_DefaultInterceptNeck
                                          ( new Neck_TurnToBallOrScan( 0 ) ) );
                }
            }
            return;
        }
    }

    // 2009-07-03
    if ( opp_min <= self_min
         && opp_min <= mate_min )
    {
        Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );
        if ( opp_trap_pos.x < 12.0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doBasicMove) switch to the defensive move" );
            //Bhv_CenterBackDefensiveMove().execute( agent );
            Bhv_CenterBackDefensiveMove().execute( agent );
            return;
        }
    }

    /*--------------------------------------------------------*/

    double dist_thr = 0.5;
    Vector2D target_point = getBasicMoveTarget( agent, &dist_thr );

    // decide dash power
    double dash_power = DefenseSystem::get_defender_dash_power( wm, target_point );

    //double dist_thr = wm.ball().pos().dist( target_point ) * 0.1;
    //if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "CB:basic%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    dlog.addText( Logger::ROLE,
                  __FILE__": doBasicMove go to (%.1f %.1f) dist_thr=%.2f power=%.1f",
                  target_point.x, target_point.y,
                  dist_thr,
                  dash_power );

    if ( wm.ball().pos().x < -35.0 )
    {
        if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.6
             && wm.self().pos().x < target_point.x - 4.0
                                    && wm.self().pos().dist( target_point ) > dist_thr )
        {
            Bhv_GoToPointLookBall( target_point,
                                   dist_thr,
                                   dash_power
                                   ).execute( agent );
            return;
        }
    }

    doGoToPoint( agent, target_point, dist_thr, dash_power,
                 15.0 ); // dir_thr

    if ( opp_min <= 3
         || wm.ball().distFromSelf() < 10.0 )
    {
        agent->setNeckAction( new Neck_TurnToBall() );
    }
    else
    {
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
RoleCenterBack::getBasicMoveTarget( PlayerAgent * agent,
                                    double * dist_thr )
{
    const WorldModel & wm = agent->world();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const bool is_left_side = ( Strategy::i().getPositionType( wm.self().unum() ) == Position_Left );
    const bool is_right_side = ( Strategy::i().getPositionType( wm.self().unum() ) == Position_Right );

    Vector2D target_point = home_pos;

    *dist_thr = wm.ball().pos().dist( target_point ) * 0.1;
    if ( *dist_thr < 0.5 ) *dist_thr = 0.5;

    // get mark target player

    //if ( wm.ball().pos().x > home_pos.x + 15.0 )
    if ( wm.ball().pos().x > 10.0 )
    {
        const AbstractPlayerObject * target_opponent = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

        if ( target_opponent )
        {
            if ( target_opponent->pos().x > wm.ourDefenseLineX() + 10.0
                 || target_opponent->pos().x > home_pos.x + 10.0
                 || target_opponent->pos().x < home_pos.x - 10.0
                 || std::fabs( target_opponent->pos().y - home_pos.y ) > 10.0
                 || ( is_left_side && target_opponent->pos().y < home_pos.y - 2.0 )
                 || ( is_right_side && target_opponent->pos().y > home_pos.y + 2.0 ) )
            {
                target_opponent = static_cast< const AbstractPlayerObject * >( 0 );
            }
        }

        if ( ! target_opponent )
        {
            double min_dist2 = 100000.0;
            for ( PlayerObject::Cont::const_iterator it = wm.opponentsFromSelf().begin(),
                      end = wm.opponentsFromSelf().end();
                  it != end;
                  ++it )
            {
                if ( (*it)->pos().x > wm.ourDefenseLineX() + 10.0 ) continue;
                if ( (*it)->pos().x > home_pos.x + 10.0 ) continue;
                if ( (*it)->pos().x < home_pos.x - 10.0 ) continue;
                if ( std::fabs( (*it)->pos().y - home_pos.y ) > 10.0 ) continue;
                if ( is_left_side && (*it)->pos().y < home_pos.y - 2.0 ) continue;
                if ( is_right_side && (*it)->pos().y > home_pos.y + 2.0 ) continue;

                double d2 = (*it)->pos().dist2( home_pos );
                if ( d2 < min_dist2 )
                {
                    min_dist2 = d2;
                    target_opponent = *it;
                }
                break;
            }
        }

        if ( target_opponent )
        {
            target_point.x = target_opponent->pos().x - 2.0;
            if ( target_point.x > home_pos.x )
            {
                target_point.x = home_pos.x;
            }

            target_point.y
                = target_opponent->pos().y
                + ( target_opponent->pos().y > home_pos.y
                    ? -1.0
                        : 1.0 );

            *dist_thr = std::min( 1.0,
                                  wm.ball().pos().dist( target_point ) * 0.1 );

            dlog.addText( Logger::ROLE,
                          __FILE__": (getBasicMoveTarget)"
                          "  block opponent front. dist_thr=%.2f",
                          *dist_thr );
            agent->debugClient().addMessage( "BlockOpp" );
        }
    }

    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleCenterBack::doGoToPoint( PlayerAgent * agent,
                             const Vector2D & target_point,
                             const double & dist_thr,
                             const double & dash_power,
                             const double & dir_thr )
{

    if ( Body_GoToPoint( target_point,
                         dist_thr,
                         dash_power,
                         -1.0, // dash speed
                         100, // cycle
                         true, // stamina save
                         dir_thr
                         ).execute( agent ) )
    {
        return;
    }

    const WorldModel & wm = agent->world();

    AngleDeg body_angle;
    if ( wm.ball().pos().x < -30.0 )
    {
        body_angle = wm.ball().angleFromSelf() + 90.0;
        if ( wm.ball().pos().x < -45.0 )
        {
            if (  body_angle.degree() < 0.0 )
            {
                body_angle += 180.0;
            }
        }
        else if ( body_angle.degree() > 0.0 )
        {
            body_angle += 180.0;
        }
    }
    else // if ( std::fabs( wm.self().pos().y - wm.ball().pos().y ) > 4.0 )
    {
        //body_angle = wm.ball().angleFromSelf() + ( 90.0 + 20.0 );
        body_angle = wm.ball().angleFromSelf() + 90.0;
        if ( wm.ball().pos().x > wm.self().pos().x + 15.0 )
        {
            if ( body_angle.abs() > 90.0 )
            {
                body_angle += 180.0;
            }
        }
        else
        {
            if ( body_angle.abs() < 90.0 )
            {
                body_angle += 180.0;
            }
        }
    }
    /*
      else
      {
      body_angle = ( wm.ball().pos().y < wm.self().pos().y
      ? -90.0
      : 90.0 );
      }
    */

    Body_TurnToAngle( body_angle ).execute( agent );

}
