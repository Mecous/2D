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

#include "bhv_side_back_aggressive_cross_block.h"

#include "strategy.h"
#include "defense_system.h"
#include "field_analyzer.h"
#include "mark_analyzer.h"

#include "bhv_side_back_block_ball_owner.h"
#include "bhv_side_back_mark_move.h"
#include "bhv_defender_mark_move.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

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
Bhv_SideBackAggressiveCrossBlock::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_SideBackAggressiveCrossBlock" );

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
    // normal block
    //
    if ( doBlockNormal( agent ) )
    {
        return true;
    }

    //
    // emergency move
    //
    if ( doEmergencyMove( agent ) )
    {
        return true;
    }

    if ( doMarkMove( agent ) )
    {
        return true;
    }

    //
    // block cross
    //
    if ( doBlockCrossLine( agent ) )
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
Bhv_SideBackAggressiveCrossBlock::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    {
        const PlayerObject * teammate = wm.interceptTable()->fastestTeammate();
        if ( teammate
             && teammate->goalie() )
        {
            mate_min = wm.interceptTable()->secondTeammateReachCycle();
        }
    }

    const InterceptInfo info = Body_Intercept::get_best_intercept( wm, true );

    bool intercept = false;

    if ( ! wm.kickableTeammate()
         && self_min <= opp_min
         && self_min <= mate_min )
    {
        intercept = true;
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) intercept(1)" );
    }

    if ( ! intercept
         && ! wm.kickableTeammate()
         && 2 <= opp_min && opp_min <= 6
         //&& self_min <= opp_min + 3
         && self_min <= opp_min + 1
         && self_min <= mate_min + 5 )
    {
        intercept = true;
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) intercept(2)" );
    }

    if ( ! intercept
         && ! wm.kickableTeammate()
         && 2 <= opp_min
         //&& self_min <= opp_min + 1
         && self_min <= std::min( opp_min * 1.5, opp_min + 6.0 )
         && self_min <= std::min( mate_min * 1.5, mate_min + 6.0 ) )
    {
        intercept = true;
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) intercept(3)" );
    }

    if ( intercept
         && info.turnCycle() > 0
         && opp_min <= self_min - 1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) xxx cancel intercept. need turn" );
        intercept = false;
    }

    if ( intercept )
    {
        agent->debugClient().addMessage( "SB:ACrossBlock:Intercept" );
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

    dlog.addText( Logger::ROLE,
                  __FILE__": (doIntercept) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackAggressiveCrossBlock::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    if ( ! fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no opponent" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    int opp_min = wm.interceptTable()->opponentReachCycle();

    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );
    Vector2D next_self_pos = wm.self().pos() + wm.self().vel();

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGetBall) check opp_trap_pos=(%.2f %.2f) home_y=%.2f self_y=%.2f pos_type=%d",
                  opp_trap_pos.y,
                  home_pos.y,
                  next_self_pos.y,
                  position_type );

    if ( wm.kickableTeammate()
         || opp_trap_pos.x > -36.0
         || opp_trap_pos.dist( home_pos ) > 10.0
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no get ball situation" );
        return false;
    }

    if ( std::fabs( opp_trap_pos.y - home_pos.y ) < 10.0 // 5.0
         && ( ( position_type == Position_Left
                && opp_trap_pos.y < 0.0 )
              || ( position_type == Position_Right
                   && opp_trap_pos.y > 0.0 ) )
         //&& ( next_self_pos.x < trap_pos.x
         //|| next_self_pos.x < -30.0 )
         )
    {
        double min_y = std::max( -32.0, home_pos.y - 15.0 );
        double max_y = std::min( +32.0, home_pos.y + 15.0 );
        Rect2D bounding_rect( Vector2D( -51.5, min_y ),
                              Vector2D( home_pos.x + 4.0, max_y ) );
        agent->debugClient().addRectangle( bounding_rect );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) try to block ball owner" );

        if ( Bhv_SideBackBlockBallOwner( bounding_rect ).execute( agent ) )
        {
            agent->debugClient().addMessage( "SB:ACrossBlock:GetBall" );
            dlog.addText( Logger::ROLE,
                          __FILE__": (doGetBall) done Bhv_SideBackBlockBallOwner" );
            return true;
        }
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doIntercept) false" );
    return false;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackAggressiveCrossBlock::doBlockNormal( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();

    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockNormal) no opponent" );
        return false;
    }

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

    if ( mark_target
         && mark_target->unum() != opponent->unum() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockNormal) opponent ball holder is not a mark target" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    // const int opponent_min = wm.interceptTable()->opponentReachCycle();
    // const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_min );

    double min_y = std::max( -32.0, home_pos.y - 15.0 );
    double max_y = std::min( +32.0, home_pos.y + 15.0 );
    Rect2D bounding_rect( Vector2D( -51.5, min_y ),
                          Vector2D( home_pos.x + 4.0, max_y ) );
    agent->debugClient().addRectangle( bounding_rect );

    if ( Bhv_SideBackBlockBallOwner( bounding_rect ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SB:CrossBlock:NormalBlock" );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockNormal) done Bhv_SideBackBlockBallOwner" );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doBlockNormal) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackAggressiveCrossBlock::doEmergencyMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    if ( ! fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doEmergencyMove) false: no opponent" );
        return false;
    }

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( mate_min + 4 <= opp_min )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doEmergencyMove) false: ball owner is teammate" );
        return false;
    }

    const Vector2D next_opp_pos = fastest_opp->pos() + fastest_opp->vel();
    const Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    const Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    StaminaModel stamina;
    int self_step = FieldAnalyzer::predict_self_reach_cycle( wm, home_pos, 1.0, 0, true, &stamina );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doEmergencyMove) home=(%.1f %.1f) next_self=(%.1f %.1f) step=%d  stamina=%.1f",
                  home_pos.x, home_pos.y,
                  next_self_pos.x, next_self_pos.y,
                  self_step, stamina.stamina() );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doEmergencyMove) opp_step=%d opp_trap_pos=(%.1f %.1f) next_opp=(%.1f %.1f)",
                  opp_min,
                  opp_trap_pos.x, opp_trap_pos.y,
                  next_opp_pos.x, next_opp_pos.y );

    if ( opp_trap_pos.x < next_self_pos.x
         //&& home_pos.x < next_self_pos.x
         && opp_min < self_step
         && next_opp_pos.x < next_self_pos.x + 1.0
         && ( opp_trap_pos.absY() < 7.0
              || ( position_type == Position_Left && opp_trap_pos.y < 0.0 )
              || ( position_type == Position_Right && opp_trap_pos.y > 0.0 )
              )
         )
    {
        Vector2D target_point( -48.0, 7.0 );

        // same as Bhv_SideBackDefensiveMove
        if ( ( opp_trap_pos.absY() < wm.self().pos().absY()
               || opp_trap_pos.y * wm.self().pos().y < 0.0 )
             && opp_trap_pos.absY() < 20.0 )
        {
            target_point.y = 0.0;
        }
        else if ( opp_trap_pos.absY() > 23.0 )
        {
            target_point.y = 20.0;
        }
        else if ( opp_trap_pos.absY() > 16.0 )
        {
            target_point.y = 14.0;
        }
        else if ( opp_trap_pos.absY() > 7.0 )
        {
            target_point.y = 7.0;
        }
        else
        {
            target_point.y = 4.0;
        }

        if ( position_type == Position_Left )
        {
            target_point.y *= -1.0;
        }

        dlog.addText( Logger::ROLE,
                      __FILE__": (doEmergencyMove) target=(%.2f %.2f)",
                      target_point.x, target_point.y );


        if ( wm.self().body().abs() > 100.0 )
        {
            const Line2D my_body_line( wm.self().inertiaFinalPoint(), wm.self().body() );
            double target_x = -45.0;
            double target_y = my_body_line.getY( target_x );
            if ( position_type == Position_Left
                 && target_y < 0.0
                 && opp_trap_pos.y < target_y - 2.0 )
            {
                target_point.assign( target_x, target_y );
                dlog.addText( Logger::ROLE,
                              __FILE__": (doEmergencyMove) adjust target=(%.2f %.2f)",
                              target_point.x, target_point.y );
            }
            if ( position_type == Position_Right
                 && target_y > 0.0
                 && opp_trap_pos.y > target_y + 2.0 )
            {
                target_point.assign( target_x, target_y );
                dlog.addText( Logger::ROLE,
                              __FILE__": (doEmergencyMove) adjust target=(%.2f %.2f)",
                              target_point.x, target_point.y );
            }
        }

        double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );
        double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
        if ( dist_thr < 0.5 ) dist_thr = 0.5;

        agent->debugClient().addMessage( "SB:ACrossBlock:Emergency" );
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );

        doGoToPoint( agent, target_point, dist_thr, dash_power, 18.0 );
        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doEmergencyMove) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackAggressiveCrossBlock::doBlockCrossLine( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    int opp_min = wm.interceptTable()->opponentReachCycle();
    Vector2D trap_pos = wm.ball().inertiaPoint( opp_min );

    if ( ( position_type == Position_Left
           && trap_pos.y > 0.0 )
         || ( position_type == Position_Right
              && trap_pos.y < 0.0 )
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockCrossLine) false: opposite side" );
        return false;
    }

#if 1
    if ( trap_pos.x > -35.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockCrossLine) false: x > -35.0" );
        return false;
    }
#endif

    Vector2D block_point = home_pos;

    double dash_power = ServerParam::i().maxDashPower();
    if ( wm.self().pos().x < block_point.x + 5.0 )
    {
        dash_power -= wm.ball().distFromSelf() * 2.0;
        dash_power = std::max( 20.0, dash_power );
    }

    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 0.8 ) dist_thr = 0.8;

    dlog.addText( Logger::ROLE,
                  __FILE__": (doBlockCrossLine) target=(%.2f, %.2f) dash_power=%.1f",
                  block_point.x, block_point.y, dash_power );

    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, dist_thr );

    if ( Body_GoToPoint( block_point,
                         dist_thr,
                         dash_power,
                         -1.0, // dash speed
                         1, // cycle
                         true, // save recovery
                         15.0 // dir threshold
                         ).execute( agent )
         )
    {
        agent->debugClient().addMessage( "SB:ACrossBlock:BlockLine:Go%.0f",
                                         dash_power );
    }
    else
    {
        AngleDeg body_angle = ( wm.ball().angleFromSelf().abs() < 70.0
                                ? 0.0
                                : 180.0 );
        if ( trap_pos.x < - 47.0 )
        {
            body_angle = 0.0;
        }
        Body_TurnToAngle( body_angle ).execute( agent );
        agent->debugClient().addMessage( "SB:ACrossBlock:BlockLine:TurnTo%.0f",
                                         body_angle.degree() );
    }

    agent->setNeckAction( new Neck_CheckBallOwner() );

    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackAggressiveCrossBlock::doMarkMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int teammate_step = wm.interceptTable()->teammateReachCycle();
    const int opponent_step = wm.interceptTable()->opponentReachCycle();

    if ( ( ! wm.kickableOpponent()
           && teammate_step <= 1
           && wm.lastKickerSide() == wm.ourSide() )
         || teammate_step <= opponent_step - 5 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(doMarkMove) false: no mark situation." );
        return false;
    }

    if ( wm.ourDefenseLineX() < -36.0 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(doMarkMove) side back mark." );
        if ( Bhv_SideBackMarkMove().execute( agent ) )
        {
            return true;
        }
    }
    else
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(doMarkMove) defender mark." );
        if ( Bhv_DefenderMarkMove().execute( agent ) )
        {
            return true;
        }
    }

    dlog.addText( Logger::MARK,
                  __FILE__":(doMarkMove) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideBackAggressiveCrossBlock::doNormalMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );

    double dash_power = DefenseSystem::get_defender_dash_power( wm, target_point );
    double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doNormalMove target=(%.2f %.2f) dist_thr=%.2f power=%.1f",
                  target_point.x, target_point.y,
                  dist_thr, dash_power );

    if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                         -1.0, // dash speed
                         1, // cycle
                         true, // save recovery
                         15.0 // dir threshold
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SB:ACrossBlock:Normal:Go%.0f",
                                         dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doNormalMove) GoTo" );
    }
    else
    {
        AngleDeg body_angle = ( wm.ball().pos().y < wm.self().pos().y
                                ? -90.0
                                : 90.0 );
        agent->debugClient().addMessage( "SB:ACrossBlock:Normal:TurnTo%.0f",
                                         body_angle.degree() );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doNormalMove) Turn" );
        Body_TurnToAngle( body_angle ).execute( agent );
    }

    if ( wm.ball().distFromSelf() < 10.0
         && wm.interceptTable()->opponentReachCycle() <= 3 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doNormalMove) look at the ball" );
        agent->setNeckAction( new Neck_TurnToBall() );
    }
    else
    {
        int count_thr = -1;
        switch ( agent->effector().queuedNextViewWidth().type() ) {
        case ViewWidth::NARROW:
            count_thr = 1;
            break;
        case ViewWidth::NORMAL:
            count_thr = 2;
            break;
        case ViewWidth::WIDE:
            count_thr = 3;
            break;
        default:
            break;
        }

        dlog.addText( Logger::ROLE,
                      __FILE__": (doNormalMove) look ath the ball or scan. count_thr=%d",
                      count_thr );
        agent->setNeckAction( new Neck_TurnToBallOrScan( count_thr ) );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideBackAggressiveCrossBlock::doGoToPoint( PlayerAgent * agent,
                                               const Vector2D & target_point,
                                               const double & dist_thr,
                                               const double & dash_power,
                                               const double & dir_thr )
{
    if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                         -1.0, // dash speed
                         1, true, dir_thr
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Go%.1f", dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__": GoToPoint (%.1f %.1f) dash_power=%.1f dist_thr=%.2f",
                      target_point.x, target_point.y,
                      dash_power,
                      dist_thr );
        return;
    }

    // already there
    // turn to somewhere

    const WorldModel & wm = agent->world();

    Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    Vector2D my_final = wm.self().inertiaFinalPoint();
    AngleDeg ball_angle = ( ball_next - my_final ).th();

    AngleDeg body_angle = ( ball_angle.abs() < 70.0
                            ? 0.0
                            : 180.0 );
    if ( ball_next.x < - 47.0 )
    {
        body_angle = 0.0;
    }

    Body_TurnToAngle( body_angle ).execute( agent );

    agent->debugClient().addMessage( "TurnTo%.0f",
                                     body_angle.degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__": TurnToAngle %.1f",
                  body_angle.degree() );
}
