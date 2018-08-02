// -*-c++-*-

/*!
  \file defense_system.cpp
  \brief team defense system manager Source File
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA, Hiroki SHIMORA

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

#include "defense_system.h"

#include "strategy.h"
#include "mark_analyzer.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_turn_to_angle.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/neck_turn_to_point.h>
#include <rcsc/action/view_synch.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

#include <rcsc/geom/vector_2d.h>
#include <rcsc/geom/matrix_2d.h>

// #define DEBUG_PRINT_BLOCK

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

*/
double
DefenseSystem::get_defender_dash_power( const WorldModel & wm,
                                        const Vector2D & home_pos )
{
    static bool S_recover_mode = false;

    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        S_recover_mode = false;
        return std::min( ServerParam::i().maxDashPower(),
                         wm.self().stamina() + wm.self().playerType().extraStamina() );
    }
    else if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.5 )
    {
        S_recover_mode = true;
    }
    else if ( wm.self().stamina()
              > ServerParam::i().staminaMax() * 0.85 )
    {
        S_recover_mode = false;
    }

    const double ball_xdiff
        //= wm.ball().pos().x - home_pos.x;
        = wm.ball().pos().x - wm.self().pos().x;

    const PlayerType & ptype = wm.self().playerType();
    const double my_inc = ptype.staminaIncMax() * wm.self().recovery();

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    double dash_power;
    if ( S_recover_mode )
    {
#if 1
        // 2009-06-29
        if ( opp_min <= mate_min + 1
             && wm.self().pos().x < home_pos.x - 10.0 )
        {
            dlog.addText( Logger::TEAM,
                                __FILE__": (get_defender)dash_power) recover mode, but opponent ball. max power" );
            dash_power = ServerParam::i().maxDashPower();
        }
        else
#endif
        if ( wm.ourDefenseLineX() > wm.self().pos().x )
        {
            dlog.addText( Logger::TEAM,
                                __FILE__": get_dash_power. correct DF line & recover" );
            dash_power = my_inc;
        }
        else if ( ball_xdiff < 5.0 )
        {
            if ( mate_min <= opp_min - 4 )
            {
                dash_power = my_inc;
            }
            else
            {
                dash_power = ServerParam::i().maxDashPower();
            }
        }
        else if ( ball_xdiff < 10.0 )
        {
            dash_power = ServerParam::i().maxDashPower();
            dash_power *= 0.7;
            //dash_power
            //    = ptype.getDashPowerToKeepSpeed( 0.7, wm.self().effort() );
        }
        else if ( ball_xdiff < 20.0 )
        {
            dash_power = std::max( 0.0, my_inc - 10.0 );
        }
        else // >= 20.0
        {
            dash_power = std::max( 0.0, my_inc - 20.0 );
        }

        dlog.addText( Logger::TEAM,
                            __FILE__": get_dash_power. recover mode dash_power= %.1f",
                            dash_power );

        return dash_power;
    }

    // normal case

#if 1
    // added 2006/06/11 03:34
    if ( wm.ball().pos().x > 0.0
         && wm.self().pos().x < home_pos.x
         && wm.self().pos().x > wm.ourDefenseLineX() - 0.05 ) // 20080712
    {
        double power_for_max_speed = ptype.getDashPowerToKeepMaxSpeed( wm.self().effort() );
        double defense_dash_dist = wm.self().pos().dist( Vector2D( -48.0, 0.0 ) );
        int cycles_to_reach = ptype.cyclesToReachDistance( defense_dash_dist );
        int available_dash_cycles = ptype.getMaxDashCyclesSavingRecovery( power_for_max_speed,
                                                                          wm.self().stamina(),
                                                                          wm.self().recovery() );
        if ( available_dash_cycles < cycles_to_reach )
        {
            dlog.addText( Logger::TEAM,
                                __FILE__": get_dash_power. keep stamina for defense back dash,"
                                " power_for_max=%.1f"
                                " dash_dist=%.1f, reach_cycle=%d, dashable_cycle=%d",
                                power_for_max_speed,
                                defense_dash_dist, cycles_to_reach, available_dash_cycles );
            dash_power = std::max( 0.0, my_inc - 20.0 );
            return dash_power;
        }
    }
#endif

#if 1
    if ( wm.ourDefenseLineX() < wm.self().pos().x - 2.0
         && wm.self().pos().x < home_pos.x
         && wm.ball().pos().x > home_pos.x + 20.0 )
    {
        dash_power = my_inc * 0.5;
        dlog.addText( Logger::TEAM,
                            __FILE__": get_dash_power. over defense line. power=%.1f",
                            dash_power );
    }
    else
#endif
    if ( wm.self().pos().x < -30.0
         && wm.ourDefenseLineX() > wm.self().pos().x )
    {
        dash_power = ServerParam::i().maxDashPower();
        dlog.addText( Logger::TEAM,
                            __FILE__": get_dash_power. correct dash power for the defense line. power=%.1f",
                            dash_power );
    }
    else if ( home_pos.x < wm.self().pos().x )
    {
        dash_power = ServerParam::i().maxDashPower();
        dlog.addText( Logger::TEAM,
                            __FILE__": get_dash_power. max power to go to the behind home position. power=%.1f",
                            dash_power );
    }
    else if ( ball_xdiff > 20.0 )
    {
        dash_power = my_inc;
        dlog.addText( Logger::TEAM,
                            __FILE__": get_dash_power. correct dash power to save stamina(1). power=%.1f",
                            dash_power );
    }
    else if ( ball_xdiff > 10.0 )
    {
        dash_power = ServerParam::i().maxDashPower();
        dash_power *= 0.6;
        //dash_power = mytype.getDashPowerToKeepSpeed( 0.6, wm.self().effort() );
        dlog.addText( Logger::TEAM,
                            __FILE__": get_dash_power. correct dash power to save stamina(2). power=%.1f",
                            dash_power );
    }
    else if ( ball_xdiff > 5.0 )
    {
        dash_power = ServerParam::i().maxDashPower();
        dash_power *= 0.85;
        //dash_power = mytype.getDashPowerToKeepSpeed( 0.85, wm.self().effort() );
        dlog.addText( Logger::TEAM,
                            __FILE__": get_dash_power. correct dash power to save stamina(3). power=%.1f",
                            dash_power );
    }
    else
    {
        dash_power = ServerParam::i().maxDashPower();
        dlog.addText( Logger::TEAM,
                            __FILE__": get_dash_power. max power. power=%.1f",
                            dash_power );
    }

    return dash_power;
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
DefenseSystem::get_block_opponent_trap_point( const WorldModel & wm )
{
    int opp_min = wm.interceptTable()->opponentReachCycle();
    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

#ifdef DEBUG_PRINT_BLODK
    dlog.addText( Logger::BLOCK,
                  __FILE__": opponent trap pos (%.3f %.3f)",
                  opp_trap_pos.x, opp_trap_pos.y );
#endif

    const PlayerObject * opp = wm.interceptTable()->fastestOpponent();
    if ( opp )
    {
        Vector2D tmp_trap_pos = opp_trap_pos;
        if ( opp_min == 0 )
        {
            if ( opp->bodyCount() <= 1 )
            {
                tmp_trap_pos = opp->inertiaPoint( 2 )
                    + Vector2D::polar2vector( 0.4, opp->body() );
#ifdef DEBUG_PRINT_BLODK
                dlog.addText( Logger::BLOCK,
                              __FILE__": adjust opponent trap point to the body direction(1) (%.3f %.3f)",
                              tmp_trap_pos.x, tmp_trap_pos.y );
#endif
            }
            else
            {
                tmp_trap_pos = opp->inertiaPoint( 10 );
#ifdef DEBUG_PRINT_BLODK
                dlog.addText( Logger::BLOCK,
                              __FILE__": adjust opponent trap point to the inertia point (%.3f %.3f)",
                              tmp_trap_pos.x, tmp_trap_pos.y );
#endif
            }
        }
        else if ( opp_min <= 1
                  && opp->distFromSelf() < ServerParam::i().visibleDistance()
                  && opp->bodyCount() <= 1 )
        {
            tmp_trap_pos += Vector2D::polar2vector( 0.4, opp->body() );
#ifdef DEBUG_PRINT_BLOCK
            dlog.addText( Logger::BLOCK,
                          __FILE__": adjust opponent trap point to the body direction(2) (%.3f %.3f)",
                          tmp_trap_pos.x, tmp_trap_pos.y );
#endif
        }

        Vector2D center( -44.0, 0.0 );
        if ( tmp_trap_pos.dist2( center ) < opp_trap_pos.dist2( center ) )
        {
#ifdef DEBUG_PRINT_BLOCK
            dlog.addText( Logger::BLOCK,
                          __FILE__": change opponent trap pos (%.3f %.3f) -> (%.3f %.3f)",
                          opp_trap_pos.x, opp_trap_pos.y,
                          tmp_trap_pos.x, tmp_trap_pos.y );
#endif
            opp_trap_pos = tmp_trap_pos;
        }
    }

    return opp_trap_pos;
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
DefenseSystem::get_block_center_point( const WorldModel & wm )
{
    Vector2D opp_trap_pos = get_block_opponent_trap_point( wm );

#if 1
    // 2013-06-16
    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    if ( opponent
         && opponent->bodyCount() <= 1
         && opponent->isKickable()
         && -44.0 < opp_trap_pos.x
         && opp_trap_pos.x < -25.0 )
    {
        const Segment2D goal_segment( Vector2D( -ServerParam::i().pitchHalfLength(),
                                                -ServerParam::i().goalHalfWidth() ),
                                      Vector2D( -ServerParam::i().pitchHalfLength(),
                                                +ServerParam::i().goalHalfWidth() ) );
        const Segment2D opponent_move( opp_trap_pos,
                                       opp_trap_pos + Vector2D::from_polar( 500.0, opponent->body() ) );
        Vector2D intersection = goal_segment.intersection( opponent_move, false );
        if ( intersection.isValid() )
        {
            dlog.addText( Logger::BLOCK,
                          __FILE__": body line (%.2f %.2f)",
                          intersection.x, intersection.y );
            return intersection;
        }
    }

    // if ( opponent
    //      && opponent->ballReachStep() > 1 )
    // {
    //     const Segment2D goal_segment( Vector2D( -ServerParam::i().pitchHalfLength(),
    //                                             -ServerParam::i().goalHalfWidth() ),
    //                                   Vector2D( -ServerParam::i().pitchHalfLength(),
    //                                             +ServerParam::i().goalHalfWidth() ) );
    //     const Vector2D opponent_pos = opponent->pos() + opponent->vel();
    //     const Segment2D opponent_move( opp_trap_pos,
    //                                    opp_trap_pos + ( opp_trap_pos - opponent_pos ).setLengthVector( 500.0 ) );
    //     Vector2D intersection = goal_segment.intersection( opponent_move, false );
    //     if ( intersection.isValid() )
    //     {
    //         dlog.addText( Logger::BLOCK,
    //                       __FILE__": move line (%.2f %.2f)",
    //                       intersection.x, intersection.y );
    //         return intersection;
    //     }
    // }

#endif

    //
    // searth the best point
    //
    Vector2D center_pos( -44.0, 0.0 );
    if ( opp_trap_pos.x < -38.0
         && opp_trap_pos.absY() < 7.0 )
    {
        center_pos.x = -52.5;
        // 2009-06-17
        center_pos.y = -2.0 * sign( wm.self().pos().y );
#ifdef DEBUG_PRINT_BLOCK
        dlog.addText( Logger::BLOCK,
                      __FILE__": center (1) (%.1f %.1f)",
                      center_pos.x, center_pos.y );
#endif
    }
    else if ( opp_trap_pos.x < -38.0
              && opp_trap_pos.absY() < 9.0 )
    {
        center_pos.x = std::min( -49.0, opp_trap_pos.x - 0.2 );
        //center_pos.y = opp_trap_pos.y * 0.6;
        Vector2D goal_pos( - ServerParam::i().pitchHalfLength(),
                           ServerParam::i().goalHalfWidth() * 0.5 );
        if ( opp_trap_pos.y > 0.0 )
        {
            goal_pos.y *= -1.0;
        }

        Line2D opp_line( opp_trap_pos, goal_pos );
        center_pos.y = opp_line.getY( center_pos.x );
        if ( center_pos.y == Line2D::ERROR_VALUE )
        {
            center_pos.y = opp_trap_pos.y * 0.6;
        }
#ifdef DEBUG_PRINT_BLOCK
        dlog.addText( Logger::BLOCK,
                      __FILE__": center (2) (%.1f %.1f)",
                      center_pos.x, center_pos.y );
#endif
    }
    else if ( opp_trap_pos.x < -38.0
              && opp_trap_pos.absY() < 12.0 )
    {
        //center_pos.x = -50.0;
        center_pos.x = std::min( -46.5, opp_trap_pos.x - 0.2 );
        //center_pos.y = 2.5 * sign( opp_trap_pos.y );
        //center_pos.y = 6.5 * sign( opp_trap_pos.y );
        //center_pos.y = opp_trap_pos.y * 0.8;
        center_pos.y = 0.0;
#ifdef DEBUG_PRINT_BLOCK
        dlog.addText( Logger::BLOCK,
                      __FILE__": center (3) (%.1f %.1f)",
                      center_pos.x, center_pos.y );
#endif
    }
    else if ( opp_trap_pos.x < -30.0
              && 2.0 < opp_trap_pos.absY()
              && opp_trap_pos.absY() < 8.0 )
    {
        center_pos.x = -50.0;
        center_pos.y = opp_trap_pos.y * 0.9;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::BLOCK,
                      __FILE__": center (4) (%.1f %.1f)",
                      center_pos.x, center_pos.y );
#endif
    }
    else if ( opp_trap_pos.absY() > 25.0 )
    {
        center_pos.x = -44.0;
        if ( opp_trap_pos.x < -36.0 )
        {
            center_pos.y = 5.0 * sign( opp_trap_pos.y );
#ifdef DEBUG_PRINT_BLOCK
            dlog.addText( Logger::BLOCK,
                          __FILE__": center (5.1) (%.1f %.1f)",
                          center_pos.x, center_pos.y );
#endif
        }
        else
        {
            center_pos.y = 20.0 * sign( opp_trap_pos.y );
#ifdef DEBUG_PRINT_BLOCK
            dlog.addText( Logger::BLOCK,
                          __FILE__": center (5.2) (%.1f %.1f)",
                          center_pos.x, center_pos.y );
#endif
        }
    }
    else if ( opp_trap_pos.absY() > 20.0 )
    {
        center_pos.x = -44.0;
        if ( opp_trap_pos.x > -18.0 )
        {
            center_pos.y = 10.0 * sign( opp_trap_pos.y );
        }
        else
        {
            center_pos.y = 5.0 * sign( opp_trap_pos.y );
        }
#ifdef DEBUG_PRINT_BLOCK
        dlog.addText( Logger::BLOCK,
                      __FILE__": center (6) (%.1f %.1f)",
                      center_pos.x, center_pos.y );
#endif
    }
    else if ( opp_trap_pos.absY() > 15.0 )
    {
        center_pos.x = -44.0;
        if ( opp_trap_pos.x > -18.0 )
        {
            center_pos.y = 10.0 * sign( opp_trap_pos.y );
        }
        else
        {
            center_pos.y = 5.0 * sign( opp_trap_pos.y );
        }
#ifdef DEBUG_PRINT_BLOCK
        dlog.addText( Logger::BLOCK,
                      __FILE__": center (7) (%.1f %.1f)",
                      center_pos.x, center_pos.y );
#endif
    }
#ifdef DEBUG_PRINT_BLOCK
    else
    {
        dlog.addText( Logger::BLOCK,
                      __FILE__": center(default) (%.1f %.1f)",
                      center_pos.x, center_pos.y );
    }
#endif

    return center_pos;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
DefenseSystem::mark_go_to_point( PlayerAgent * agent,
                                 const AbstractPlayerObject * mark_target,
                                 const Vector2D & target_point,
                                 const double dist_thr,
                                 const double dash_power,
                                 const double dir_thr )
{
    Vector2D adjusted_point = target_point;
    double omni_dash_thr = 0.5;

#if 0
    const WorldModel & wm = agent->world();

    Vector2D inertia_pos = wm.self().inertiaPoint( 100 );
    AngleDeg target_angle = ( target_point - inertia_pos ).th();
    if ( ( target_angle - wm.self().body() ).abs() > dir_thr )
    {
        Vector2D target_rel = target_point - inertia_pos;
        target_rel.rotate( -wm.self().body() );
        if ( target_rel.x > 2.0
             && target_rel.absY() < 2.0 )
        {
            omni_dash_thr = 2.0;
            target_rel.y = 0.0;
            adjusted_point = inertia_pos + target_rel.rotate( wm.self().body() );
            dlog.addText( Logger::MARK,
                          __FILE__":(mark_go_to_point) adjust (%.2f %.2f)->(%.2f %.2f)",
                          target_point.x, target_point.y,
                          adjusted_point.x, adjusted_point.y );
        }
    }
#endif

    if ( Body_GoToPoint( adjusted_point, dist_thr, dash_power,
                         -1.0, // dash speed
                         100, // cycle
                         true,
                         dir_thr,
                         omni_dash_thr
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "MarkGo%.1f", dash_power );
        dlog.addText( Logger::MARK,
                      __FILE__":(mark_go_to_point) (%.2f %.2f)->(%.2f %.2f) dash_power=%.1f dist_thr=%.2f",
                      target_point.x, target_point.y,
                      adjusted_point.x, adjusted_point.y,
                      dash_power,
                      dist_thr );

        DefenseSystem::mark_turn_neck( agent, mark_target );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
DefenseSystem::mark_turn_to( PlayerAgent * agent,
                             const AbstractPlayerObject * mark_target )
{
    const WorldModel & wm = agent->world();

    const int min_step = wm.interceptTable()->opponentReachCycle();

    const Vector2D ball_pos = wm.ball().inertiaPoint( min_step );
    const Vector2D my_pos = wm.self().inertiaPoint( min_step );
    const AngleDeg ball_angle = ( ball_pos - my_pos ).th();

#if 1

    AngleDeg target_angle = wm.ball().angleFromSelf();
    if ( my_pos.y < 0.0 ) target_angle += 90.0;
    if ( my_pos.y > 0.0 ) target_angle -= 90.0;

    for ( double x = -50.0; x < my_pos.x; x += 5.0 )
    {
        Vector2D pos( x, 0.0 );
        AngleDeg body_angle = ( pos - my_pos ).th();

        if ( ( body_angle - ball_angle ).abs() < 90.0 )
        {
            dlog.addText( Logger::MARK,
                          __FILE__":(mark_turn_to) found angle" );
            target_angle = body_angle;
            break;
        }
    }

    Body_TurnToAngle( target_angle ).execute( agent );
    agent->debugClient().addMessage( "Mark:TurnTo%.0f",
                                     target_angle.degree() );
    dlog.addText( Logger::MARK,
                  __FILE__":(mark_turn_to) target_angle=%.1f, ball_angle=%.1f",
                  target_angle.degree(), ball_angle.degree() );
#else

    Vector2D mark_vel = mark_target->vel() / mark_target->playerTypePtr()->playerDecay();
    Vector2D mark_pos = mark_target->pos() + ( mark_vel * min_step );

    AngleDeg ball_angle = ( ball_pos - my_pos ).th();
    AngleDeg mark_angle = ( mark_pos - my_pos ).th();

    AngleDeg target_angle = AngleDeg::bisect( ball_angle, mark_angle );

    if ( ( target_angle - ball_angle ).abs() > 90.0 )
    {
        target_angle += 180.0;
    }

    Body_TurnToAngle( target_angle ).execute( agent );
    agent->debugClient().addMessage( "Mark:TurnTo%.0f",
                                     target_angle.degree() );
    dlog.addText( Logger::MARK,
                  __FILE__":(mark_turn_to) %.1f, ball_angle=%.1f mark_angle=%.1f mark_pos=(%.1f %.1f)",
                  target_angle.degree(),
                  ball_angle.degree(),
                  mark_angle.degree(), mark_pos.x, mark_pos.y );
#endif
    DefenseSystem::mark_turn_neck( agent, mark_target );
}

namespace {

void
mark_neck_find_mark_target( PlayerAgent * agent,
                            const AbstractPlayerObject * mark_target )
{

    if ( mark_target->ghostCount() > 0 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(mark_neck_find_mark_target) check ghost mark target" );
        agent->debugClient().addMessage( "Mark:NeckGhost" );

        agent->setViewAction( new View_Wide() );
        agent->setNeckAction( new Neck_ScanField() );
        return;
    }

    if ( mark_target->distFromSelf() > ServerParam::i().visibleDistance() - 0.2 )
    {
        const WorldModel & wm = agent->world();

        const int opponent_step = wm.interceptTable()->opponentReachStep();

        const double max_neck = ServerParam::i().maxNeckAngle();

        AngleDeg next_body = agent->effector().queuedNextSelfBody();
        Vector2D mark_target_pos = mark_target->pos() + mark_target->vel();
        AngleDeg mark_target_angle = ( mark_target_pos - agent->effector().queuedNextSelfPos() ).th();
        double angle_diff = ( next_body - mark_target_angle ).abs();

        ViewWidth view_width = ViewWidth::ILLEGAL;

        dlog.addText( Logger::MARK,
                      __FILE__":(mark_neck_find_mark_target) next_body=%.1f",
                      next_body.degree() );
        dlog.addText( Logger::MARK,
                      __FILE__":(mark_neck_find_mark_target) mark_target=(%.2f %.2f)",
                      mark_target_pos.x, mark_target_pos.y );
        dlog.addText( Logger::MARK,
                      __FILE__":(mark_neck_find_mark_target) mark_target_angle=%.1f",
                      mark_target_angle.degree() );
        dlog.addText( Logger::MARK,
                      __FILE__":(mark_neck_find_mark_target) angle_diff=%.1f",
                      angle_diff );

        if ( angle_diff < max_neck + ViewWidth::width( ViewWidth::NARROW ) * 0.5 - 10.0 )
        {
            dlog.addText( Logger::MARK,
                          __FILE__":(mark_neck_find_mark_target) view narrow" );
            view_width = ViewWidth::NARROW;
        }
        else if ( ( opponent_step >= 3
                    || wm.ball().distFromSelf() > 13.0 )
                  && angle_diff < max_neck + ViewWidth::width( ViewWidth::NORMAL ) * 0.5 - 10.0 )
        {
            dlog.addText( Logger::MARK,
                          __FILE__":(mark_neck_find_mark_target) view normal" );
            view_width = ViewWidth::NORMAL;
        }
        else if ( ( opponent_step >= 4
                    || wm.ball().distFromSelf() > 20.0 ) )
        {
            dlog.addText( Logger::MARK,
                          __FILE__":(mark_neck_find_mark_target) view wide" );
            view_width = ViewWidth::WIDE;
        }

        if ( view_width != ViewWidth::ILLEGAL )
        {
            agent->setViewAction( new View_ChangeWidth( view_width ) );
        }
    }

    agent->setNeckAction( new Neck_TurnToPlayerOrScan( mark_target, -1 ) );
}

}

/*-------------------------------------------------------------------*/
/*!

*/
void
DefenseSystem::mark_turn_neck( PlayerAgent * agent,
                               const AbstractPlayerObject * mark_target )
{
    const WorldModel & wm = agent->world();

    agent->setViewAction( new View_Synch() );

    if ( mark_target->posCount() > 0 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(mark_turn_neck) check mark target(1), posCount=%d",
                      mark_target->posCount() );
        agent->debugClient().addMessage( "Mark:NeckTarget1" );

        mark_neck_find_mark_target( agent, mark_target );
        return;
    }

    if ( mark_target->unumCount() >= 2
         && ! wm.kickableOpponent() )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(mark_turn_neck) check mark target(2), unumCount=%d",
                      mark_target->unumCount() );
        agent->debugClient().addMessage( "Mark:NeckTarget2" );

        mark_neck_find_mark_target( agent, mark_target );
        return;
    }

    if ( mark_target->unumCount() >= 5 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(mark_turn_neck) check mark target(3), unumCount=%d",
                      mark_target->unumCount() );
        agent->debugClient().addMessage( "Mark:NeckTarget3" );

        mark_neck_find_mark_target( agent, mark_target );
        return;
    }

    if ( wm.kickableOpponent()
         || wm.interceptTable()->opponentReachCycle() <= 1 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(mark_turn_neck) check ball force" );

        const Vector2D ball_next = agent->effector().queuedNextBallPos();
        const Vector2D my_next = agent->effector().queuedNextSelfPos();

        if ( wm.ball().posCount() <= 0
             && ! wm.kickableOpponent()
             && ! wm.kickableTeammate()
             && my_next.dist( ball_next ) < ServerParam::i().visibleDistance() - 0.2 )
        {
            dlog.addText( Logger::ACTION,
                          __FILE__": in visible distance." );
            agent->debugClient().addMessage( "Mark:NeckBallVisible" );
            agent->setNeckAction( new Neck_TurnToPlayerOrScan( mark_target, -1 ) );
            return;
        }

        const AngleDeg my_next_body = agent->effector().queuedNextSelfBody();
        const double next_view_width = agent->effector().queuedNextViewWidth().width();

        if ( ( ( ball_next - my_next ).th() - my_next_body ).abs()
             > ServerParam::i().maxNeckAngle() + next_view_width * 0.5 + 2.0 )
        {
            dlog.addText( Logger::ACTION,
                          __FILE__": never face to ball" );
            agent->debugClient().addMessage( "Mark:NeckBallFailed" );
            agent->setNeckAction( new Neck_TurnToPlayerOrScan( mark_target, -1 ) );
            return;
        }

        dlog.addText( Logger::ACTION,
                      __FILE__": turn neck to ball" );
        agent->debugClient().addMessage( "Mark:NeckBallForce" );
        agent->setNeckAction( new Neck_TurnToBallOrScan( -1 ) );
        return;
    }

    if ( wm.ball().seenPosCount() >= 3
         || ( wm.kickableOpponent()
              && wm.ball().distFromSelf() < 10.0 ) )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(mark_turn_neck) check ball" );
        agent->debugClient().addMessage( "Mark:NeckBall" );

        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
        return;
    }

    if ( mark_target->ghostCount() > 1 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(mark_turn_neck) check mark target, ghostCount>=2" );
        agent->debugClient().addMessage( "Mark:NeckGhost2" );

        agent->setNeckAction( new Neck_ScanField() );
        return;
    }

    if ( mark_target->ghostCount() > 0 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(mark_turn_neck) check mark target, ghostCount>0" );
        agent->debugClient().addMessage( "Mark:NeckGhost1" );

        agent->setNeckAction( new Neck_TurnToPlayerOrScan( mark_target, -1 ) );
        return;
    }

    dlog.addText( Logger::MARK,
                  __FILE__":(mark_turn_neck) check mark target, normal" );
    agent->debugClient().addMessage( "Mark:NeckMark" );

    agent->setNeckAction( new Neck_TurnToPlayerOrScan( mark_target, 0 ) );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
DefenseSystem::is_midfielder_block_situation( const WorldModel & wm )
{
    const AbstractPlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(is_midfielder_block_situation) false: no opponent" );
        return false;
    }

    //const int self_step = wm.interceptTable()->selfReachStep();
    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( wm.kickableTeammate()
         || teammate_step == 0
         || teammate_step < opponent_step - 1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(is_midfielder_block_situation) false: our ball" );
        return false;
    }

    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    //
    // check mark target
    //

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

    if ( mark_target
         && mark_target->unum() != opponent->unum() )
    {
        if ( opponent->pos().x > mark_target->pos().x + 5.0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(is_midfielder_block_situation) false: keep marking" );
            return false;
        }
    }

    if ( ! mark_target
         || mark_target->unum() != opponent->unum() )
    {
        const double self_dist = wm.self().pos().dist( opponent_ball_pos );

        for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(), end = wm.teammates().end();
              t != end;
              ++t )
        {
            if ( (*t)->goalie() ) continue;
            if ( (*t)->tackleCount() <= ServerParam::i().tackleCycles() + 2 ) continue;
            if ( (*t)->unum() == Unum_Unknown ) continue;

            const AbstractPlayerObject * target = MarkAnalyzer::i().getTargetOf( (*t)->unum() );
            if ( target
                 && target->unum() == opponent->unum() )
            {
                double d = (*t)->pos().dist( opponent_ball_pos );
                if ( d < 3.0
                     || d < self_dist + 2.0 )
                {
                    dlog.addText( Logger::ROLE,
                                  __FILE__":(is_midfielder_block_situation) false: opponent %d has marker %d",
                                  opponent->unum(), (*t)->unum() );
                    return false;
                }
            }

            if ( ! target
                 && (*t)->pos().dist( opponent_ball_pos ) < self_dist - 3.0 )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__":(is_midfielder_block_situation) false: opponent %d has other blocker %d",
                              opponent->unum(), (*t)->unum() );
                return false;
            }
        }
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(is_midfielder_block_situation) true" );

    return true;
}
