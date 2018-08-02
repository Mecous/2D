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

#include "bhv_side_half_attack_move.h"

#include "strategy.h"
#include "field_analyzer.h"

#include "bhv_side_half_cross_move.h"
#include "bhv_block_ball_owner.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/body_turn_to_angle.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>

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
Bhv_SideHalfAttackMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_SideHalfAttackMove" );

    if ( doIntercept( agent ) )
    {
        return true;
    }

    if ( Bhv_SideHalfCrossMove().execute( agent ) )
    {
        return true;
    }

    if ( doGetBall( agent ) )
    {
        return true;
    }

    doShootAreaMove( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfAttackMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    dlog.addText( Logger::ROLE,
                  __FILE__": (doIntercept) self=%d, mate=%d, opp=%d",
                  self_min, mate_min, opp_min );

    bool intercept = false;

    if ( ! wm.kickableTeammate()
         && mate_min > 1
         && self_min < 4 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) ok (1)" );
        intercept = true;
    }
    else if ( self_min < mate_min )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) ok (2)" );
        intercept = true;
    }
    else if ( self_min <= mate_min && self_min > 5 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) ok (3)" );
        intercept = true;
    }
    else if ( self_min < 20
              && self_min < mate_min
              && self_min < opp_min + 20
              && wm.ball().pos().absY() > 19.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) ok (4)" );
        intercept = true;
    }

    if ( intercept )
    {
        if ( self_min >= 3 )
        {
            Vector2D ball_pos = wm.ball().inertiaPoint( self_min );
            if ( ball_pos.x < 36.0
                 && ( wm.kickableOpponent()
                      || opp_min <= self_min - 5 ) )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__": (doIntercept) false: opponent has ball" );
                intercept = false;
            }
        }
    }

    if ( intercept )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) intercept" );
        agent->debugClient().addMessage( "SH:Attack:Intercept" );

        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doIntercept) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfAttackMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( opp_min < 3
         && opp_min < mate_min - 2
         && ( opp_min <= self_min - 5
             || opp_min == 0 )
         && wm.self().stamina() > ServerParam::i().staminaMax() * 0.6
         && wm.ball().distFromSelf() < 15.0
         && wm.ball().pos().dist( home_pos ) < 10.0 )
    {
        Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opp_min );

        dlog.addText( Logger::TEAM,
                      __FILE__": (doGetBall) try. self=%d, mate=%d, opp=%d opp_ball_pos=(%.2f %.2f)",
                      self_min, mate_min, opp_min,
                      opponent_ball_pos.x, opponent_ball_pos.y );

        const AbstractPlayerObject * other_blocker = FieldAnalyzer::get_blocker( wm, opponent_ball_pos );
        if ( other_blocker )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (doGetBall) other blocker. try intercept" );
            if ( Body_Intercept( true ).execute( agent ) )
            {
                agent->debugClient().addMessage( "SHOffMove:Block:Intercept" );
                agent->setNeckAction( new Neck_TurnToBallOrScan( -1 ) );
                return true;
            }
        }

        Rect2D bounding_rect = Rect2D::from_center( home_pos, 30.0, 30.0 );
        if ( Bhv_BlockBallOwner( new Rect2D( bounding_rect ) ).execute( agent ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (doGetBall) block ball owner" );
            //agent->debugClient().addMessage( "Attack" );
            return true;
        }
        dlog.addText( Logger::TEAM,
                      __FILE__": (doGetBall) failed" );

        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (doGetBall) no block situation" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfAttackMove::doShootAreaMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    const Vector2D target_point = getShootAreaTargetPoint( agent, home_pos );

    const double dash_power = getShootAreaDashPower( agent, target_point );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    dlog.addText( Logger::ROLE,
                  __FILE__": (doShootAreaMove) go to cross point (%.2f, %.2f)",
                  target_point.x, target_point.y );

    agent->debugClient().addMessage( "SH:AttackMoveP%.0f", dash_power );
#ifndef DO_LOCAL_POSITIONING
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );
#endif

    if ( wm.self().pos().x > target_point.x + dist_thr*0.5
         && std::fabs( wm.self().pos().x - target_point.x ) < 3.0
         && wm.self().body().abs() < 10.0 )
    {
        agent->debugClient().addMessage( "Back" );
        double back_dash_power
            = wm.self().getSafetyDashPower( -dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doShootAreaMove) Back Move" );
        agent->doDash( back_dash_power );
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
        return true;
    }

    if ( ! Body_GoToPoint( target_point, dist_thr, dash_power,
                           -1.0, // dash speed
                           5, // cycle
                           true, // save recovery
                           20.0 // dir thr
                           ).execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doShootAreaMove. already target point. turn to front" );
        Body_TurnToAngle( 0.0 ).execute( agent );
    }

    if ( wm.self().pos().x > 35.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doShootAreaMove. Neck to Goalie or Scan" );
        agent->setNeckAction( new Neck_TurnToGoalieOrScan( 2 ) );
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": doShootAreaMove. Neck to Ball or Scan" );
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SideHalfAttackMove::getShootAreaTargetPoint( PlayerAgent * agent,
                                                 const Vector2D & home_pos )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point = home_pos;

    int mate_min = wm.interceptTable()->teammateReachCycle();
    Vector2D ball_pos = ( mate_min < 10
                          ? wm.ball().inertiaPoint( mate_min )
                          : wm.ball().pos() );
    bool opposite_side = false;
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    if ( ( ball_pos.y > 0.0
           && position_type == Position_Left )
         || ( ball_pos.y <= 0.0
              && position_type == Position_Right )
         || ( ball_pos.y * home_pos.y < 0.0
                                        && home_pos.absY() > 5.0
              && position_type == Position_Center )
         )
    {
        opposite_side = true;
    }

    if ( opposite_side
         && ball_pos.x > 40.0 // very chance
         && 6.0 < ball_pos.absY()
         && ball_pos.absY() < 15.0 )
    {
        Circle2D goal_area( Vector2D( 47.0, 0.0 ),
                            7.0 );

        if ( ! wm.existTeammateIn( goal_area, 10, true ) )
        {
            agent->debugClient().addMessage( "GoToCenterCross" );

            target_point.x = 47.0;

            if ( wm.self().pos().x > wm.offsideLineX() - 0.5
                 && target_point.x > wm.offsideLineX() - 0.5 )
            {
                target_point.x = wm.offsideLineX() - 0.5;
            }

            if ( ball_pos.absY() < 9.0 )
            {
                target_point.y = 0.0;
            }
            else
            {
                target_point.y = ( ball_pos.y > 0.0
                                   ? 1.0 : -1.0 );
            }

            dlog.addText( Logger::ROLE,
                          __FILE__": ShootAreaMove. center cross point" );
        }
        else
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": ShootAreaMove. exist teammate in cross point" );
        }
    }

    // consider near opponent
    {
        double opp_dist = 200.0;
        const PlayerObject * nearest_opp
            = wm.getOpponentNearestTo( target_point, 10, &opp_dist );

        if ( nearest_opp
             && opp_dist < 2.0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": ShootAreaMove. Point Blocked. old target=(%.2f, %.2f)",
                          target_point.x, target_point.y );
            agent->debugClient().addMessage( "AvoidOpp" );
            //if ( nearest_opp->pos().x + 3.0 < wm.offsideLineX() - 1.0 )
            if ( nearest_opp->pos().x + 2.0 < wm.offsideLineX() - 1.0  )
            {
                //target_point.x = nearest_opp->pos().x + 3.0;
                target_point.x = nearest_opp->pos().x + 2.0;
            }
            else
            {
                target_point.x = nearest_opp->pos().x - 2.0;
#if 1
                if ( std::fabs( wm.self().pos().y - target_point.y ) < 1.0 )
                {
                    target_point.x = nearest_opp->pos().x - 3.0;
                }
#endif
            }
        }

    }

    // consider goalie
    if ( target_point.x > 45.0 )
    {
        const AbstractPlayerObject * opp_goalie = wm.getTheirGoalie();
        if ( opp_goalie
             && opp_goalie->distFromSelf() < wm.ball().distFromSelf() )
        {
            Line2D cross_line( ball_pos, target_point );
            if (  cross_line.dist( opp_goalie->pos() ) < 3.0 )
            {
                agent->debugClient().addMessage( "AvoidGK" );
                agent->debugClient().addLine( ball_pos, target_point );
                dlog.addText( Logger::ROLE,
                              __FILE__": ShootAreaMove. Goalie is on cross line. old target=(%.2f %.2f)",
                              target_point.x, target_point.y );

                Line2D move_line( wm.self().pos(), target_point );
                target_point.x -= 2.0;
            }
        }
    }

    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_SideHalfAttackMove::getShootAreaDashPower( PlayerAgent * agent,
                                               const Vector2D & target_point )
{
    const WorldModel & wm = agent->world();

    static bool s_recover = false;

    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        return std::min( ServerParam::i().maxDashPower(),
                         wm.self().stamina() + wm.self().playerType().extraStamina() );
    }

    double dash_power = ServerParam::i().maxDashPower();

    // recover check
    // check X buffer & stamina
    if ( wm.self().stamina() < ServerParam::i().effortDecThrValue() + 500.0 )
    {
        if ( wm.self().pos().x > 30.0 )
        {
            s_recover = true;
            dlog.addText( Logger::ROLE,
                          __FILE__": recover on" );
        }
    }
    else if ( wm.self().stamina() > ServerParam::i().effortIncThrValue() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": recover off" );
        s_recover = false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( opp_min < self_min - 2
         && opp_min < mate_min - 5 )
    {
        dash_power = std::max( ServerParam::i().maxDashPower() * 0.2,
                               wm.self().playerType().staminaIncMax() * 0.8 );
    }
    else if ( wm.ball().pos().x > wm.self().pos().x )
    {
        // keep max power
        dash_power = ServerParam::i().maxDashPower();
    }
    else if ( wm.ball().pos().x > 41.0
              && wm.ball().pos().absY() < ServerParam::i().goalHalfWidth() + 5.0 )
    {
        // keep max power
        dash_power = ServerParam::i().maxDashPower();
    }
    else if ( s_recover )
    {
        dash_power = wm.self().playerType().staminaIncMax() * 0.6;
        dlog.addText( Logger::ROLE,
                      __FILE__": recovering" );
    }
    else if ( wm.interceptTable()->teammateReachCycle() <= 1
              && wm.ball().pos().x > 33.0
              && wm.ball().pos().absY() < 7.0
              && wm.ball().pos().x < wm.self().pos().x
              && wm.self().pos().x < wm.offsideLineX()
              && wm.self().pos().absY() < 9.0
              && std::fabs( wm.ball().pos().y - wm.self().pos().y ) < 3.5
              && std::fabs( target_point.y - wm.self().pos().y ) > 5.0 )
    {
        dash_power = wm.self().playerType()
            .getDashPowerToKeepSpeed( 0.3, wm.self().effort() );
        dash_power = std::min( ServerParam::i().maxDashPower() * 0.75,
                               dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__": slow for cross. power=%.1f",
                      dash_power );
    }
    else
    {
        dash_power = ServerParam::i().maxDashPower() * 0.75;
        dlog.addText( Logger::ROLE,
                      __FILE__": ball is far" );
    }

    return dash_power;
}
