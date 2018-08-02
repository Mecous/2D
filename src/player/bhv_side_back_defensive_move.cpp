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

#include "bhv_side_back_defensive_move.h"

#include "strategy.h"
#include "mark_analyzer.h"
#include "defense_system.h"
#include "field_analyzer.h"

#include "bhv_side_back_block_ball_owner.h"
#include "bhv_side_back_mark_move.h"
#include "bhv_defender_mark_move.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>

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
Bhv_SideBackDefensiveMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_SideBackDefensiveMove" );

    //
    // intercept
    //
    if ( doIntercept( agent ) )
    {
        return true;
    }

    //
    // attack to the ball owner
    //
    if ( doAttackBallOwner( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": attack to ball owner" );
        return true;
    }

    //
    // emergency move
    //
    if ( doEmergencyMove( agent ) )
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
Bhv_SideBackDefensiveMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.kickableTeammate()
         || wm.kickableOpponent() )
    {
        return false;
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min > mate_min + 2 )
    {
        return false;
    }

    const InterceptInfo info = Body_Intercept::get_best_intercept( wm, true );
    if ( ! info.isValid() )
    {
        return false;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__"(doIntercept) 1st intercept turn=%d dash=%d",
                  info.turnStep(), info.dashStep() );

    const double ball_speed = wm.ball().vel().r() * std::pow( ServerParam::i().ballDecay(), opp_min );

    bool intercept = false;

    if ( self_min <= opp_min + 1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true(1)" );
        intercept = true;
    }
    else if ( opp_min >= 8 && self_min <= opp_min + 2 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true(2)" );
        intercept = true;
    }
    else if ( ( ( opp_min >= 3 && self_min <= opp_min + 4 )
                || ( opp_min >= 2 && self_min <= opp_min + 3 ) )
              && ball_speed < 0.8 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true(3)" );
        intercept = true;
    }

    if ( intercept
         && opp_min <= self_min - 1
         && ball_speed > 1.45 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": xxx cancel intercept. ball_speed=%f", ball_speed );
        intercept = false;
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
        agent->debugClient().addMessage( "SB:Def:Intercept" );
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
Bhv_SideBackDefensiveMove::doAttackBallOwner( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    if ( ! fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doAttackBallOwner) no opponent" );
        return false;
    }

    //const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( mate_min < opp_min )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doAttackBallOwner) ball owner is teammate" );
        return false;
    }

    const Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    {
        // 2010-06-18

        const AbstractPlayerObject * blocker = FieldAnalyzer::get_blocker( wm, opp_trap_pos );
        if ( blocker
             && blocker->unum() != wm.self().unum() )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doAttackBallOwner) exist other blocker=%d (%.1f %.1f)",
                          blocker->unum(),
                          blocker->pos().x, blocker->pos().y );
            return false;
        }
    }

    //
    // check mark target
    //

    const MarkAnalyzer & mark_analyzer = MarkAnalyzer::i();

    const AbstractPlayerObject * mark_target = mark_analyzer.getTargetOf( wm.self().unum() );
    const AbstractPlayerObject * free_attacker = static_cast< AbstractPlayerObject * >( 0 );

    if ( mark_target )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doAttackBallOwner) mark_target= %d (%.1f %.1f)",
                      mark_target->unum(),
                      mark_target->pos().x, mark_target->pos().y );

        //
        // find other free attacker
        //
        for ( AbstractPlayerObject::Cont::const_iterator o = wm.theirPlayers().begin(),
                  end = wm.theirPlayers().end();
              o != end;
              ++o )
        {
            const AbstractPlayerObject * marker = mark_analyzer.getMarkerOf( *o );
            if ( marker ) continue; // exist other marker
            if ( (*o)->pos().x > mark_target->pos().x + 10.0 ) continue; // no attacker
            if ( ( position_type == Position_Left
                   && (*o)->pos().y < mark_target->pos().y + 10.0 )
                 || ( position_type == Position_Right
                      && (*o)->pos().y > mark_target->pos().y - 10.0 )
                 || ( position_type == Position_Center
                      && std::fabs( (*o)->pos().y - mark_target->pos().y ) < 10.0 )
                 )
            {
                free_attacker = *o;
                break;
            }
        }
    }


    if ( free_attacker )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doAttackBallOwner) exist free_ataccker= %d (%.1f %.1f)",
                      free_attacker->unum(),
                      free_attacker->pos().x, free_attacker->pos().y );
        return false;
    }

    //
    //
    //

    const double min_y = ( position_type == Position_Right
                           ? -10.0
                           : -33.5 );
    const double max_y = ( position_type == Position_Left
                           ? +10.0
                           : +33.5 );

    if ( fastest_opp == mark_target )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doAttackBallOwner) fastest_opp == mark_target" );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doAttackBallOwner) opponent=%d ball_pos=(%.1f %.1f)",
                      mark_target->unum(),
                      opp_trap_pos.x, opp_trap_pos.y );

        dlog.addText( Logger::ROLE,
                      __FILE__": (doAttackBallOwner) try GetBall" );

        Rect2D bounding_rect( Vector2D( -51.0, min_y ),
                              Vector2D( 0.0, max_y ) ); // Vector2D( -15.0, max_y ) );
        if ( Bhv_SideBackBlockBallOwner( bounding_rect ).execute( agent ) )
        {
            agent->debugClient().addMessage( "SB:Block1" );
            return true;
        }

        dlog.addText( Logger::ROLE,
                      __FILE__": (doAttackBallOwner) failed GetBall" );

        Vector2D target_segment_origin( -45.0, -2.0 );
        Vector2D target_segment_terminal( -45.0, 20.0 );

        if ( ( opp_trap_pos.absY() < wm.self().pos().absY()
               || opp_trap_pos.y * wm.self().pos().y < 0.0 )
             && opp_trap_pos.absY() < 20.0 )
        {
            target_segment_terminal.y = 2.0;
            dlog.addText( Logger::ROLE,
                          __FILE__": (doAttackBallOwner) target(1)" );
        }
        else if ( opp_trap_pos.absY() > 23.0 )
        {
            target_segment_terminal.y = 20.0;
            dlog.addText( Logger::ROLE,
                          __FILE__": (doAttackBallOwner) target(2)" );
        }
        else if ( opp_trap_pos.absY() > 16.0 )
        {
            target_segment_terminal.y = 14.0;
            dlog.addText( Logger::ROLE,
                          __FILE__": (doAttackBallOwner) target(3)" );
        }
        else if ( opp_trap_pos.absY() > 7.0 )
        {
            target_segment_terminal.y = 7.0;
            dlog.addText( Logger::ROLE,
                          __FILE__": (doAttackBallOwner) target(4)" );
        }
        else
        {
            target_segment_terminal.y = 4.0;
            dlog.addText( Logger::ROLE,
                          __FILE__": (doAttackBallOwner) target(5)" );
        }

        if ( position_type == Position_Left )
        {
            target_segment_origin.y *= -1.0;
            target_segment_terminal.y *= -1.0;
        }

        const Segment2D target_segment( target_segment_origin,
                                        target_segment_terminal );
        Vector2D target_point = ( target_segment.origin() + target_segment.terminal() ) * 0.5;

        dlog.addLine( Logger::ROLE,
                      target_segment.origin(), target_segment.terminal(), "#00F" );

        if ( wm.self().body().abs() > 95.0 )
        {
            const Line2D body_line( wm.self().inertiaFinalPoint(), wm.self().body() );
            Vector2D intersect_point = target_segment.intersection( body_line );
            if ( intersect_point.isValid() )
            {
                target_point = intersect_point;
                dlog.addText( Logger::ROLE,
                              __FILE__": (doAttackBallOwner) update to the intersection point" );
            }
        }


        double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
        if ( dist_thr < 0.5 ) dist_thr = 0.5;

        double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );

        dlog.addText( Logger::ROLE,
                      __FILE__": (doAttackBallOwner) fastest_opp == mark_target. emergecy mode"
                      " target=(%.1f %.1f) dist_thr=%f dash_power=%.1f",
                      target_point.x, target_point.y,
                      dist_thr, dash_power );
        agent->debugClient().addMessage( "SB:Def:GetBall:Emergency" );
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );

        doGoToPoint( agent, target_point, dist_thr, dash_power, 18.0 );

        if ( opp_min >= 3 )
        {
            agent->setViewAction( new View_Wide() );
        }

        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }

    //
    //
    //

    if ( ! mark_target )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doAttackBallOwner) no mark target. ball_owner=%d (%.1f %.1f) trap_pos=(%.1f %.1f)",
                      fastest_opp->unum(),
                      fastest_opp->pos().x, fastest_opp->pos().y,
                      opp_trap_pos.x, opp_trap_pos.y );

        if ( opp_trap_pos.x < home_pos.x + 10.0
             && ( std::fabs( opp_trap_pos.y - home_pos.y ) < 10.0
                  || ( position_type == Position_Left
                       && opp_trap_pos.y < home_pos.y )
                  || ( position_type == Position_Right
                       && opp_trap_pos.y > home_pos.y ) )
             )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doAttackBallOwner) no mark target. attack " );
            Rect2D bounding_rect( Vector2D( -50.0, min_y ),
                                  Vector2D( -15.0, max_y ) ); //Vector2D( -5.0, max_y ) );
            if ( Bhv_SideBackBlockBallOwner( bounding_rect ).execute( agent ) )
            {
                agent->debugClient().addMessage( "SB:Block2" );
                return true;
            }
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDefensiveMove::doEmergencyMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    if ( ! fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doEmergencyMove) no opponent" );
        return false;
    }

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( mate_min + 4 <= opp_min )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doEmergencyMove) ball owner is teammate" );
        return false;
    }

    const Vector2D next_opp_pos = fastest_opp->pos() + fastest_opp->vel();
    const Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    const Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    StaminaModel stamina;
    int self_step_to_home_pos = FieldAnalyzer::predict_self_reach_cycle( wm, home_pos, 1.0, 0, true, &stamina );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doEmergencyMove) home=(%.1f %.1f) next_self=(%.1f %.1f) step=%d  stamina=%.1f",
                  home_pos.x, home_pos.y,
                  next_self_pos.x, next_self_pos.y,
                  self_step_to_home_pos, stamina.stamina() );

    int self_step_to_opp_trap_pos = FieldAnalyzer::predict_self_reach_cycle( wm, opp_trap_pos, 1.0, 0, true, &stamina );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doEmergencyMove) opp_step=%d opp_trap_pos=(%.1f %.1f) next_opp=(%.1f %.1f) self_step=%d",
                  opp_min,
                  opp_trap_pos.x, opp_trap_pos.y,
                  next_opp_pos.x, next_opp_pos.y,
                  self_step_to_opp_trap_pos );

#if 1
    {
        int teammate_count = 0;
        const double self_ball_dist2 = wm.self().inertiaPoint( opp_min ).dist2( opp_trap_pos );
        for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromBall().begin(),
                  end = wm.teammatesFromBall().end();
              t != end;
              ++t )
        {
            if ( (*t)->pos().dist2( opp_trap_pos ) < self_ball_dist2 )
            {
                ++teammate_count;
                if ( teammate_count >= 2 )
                {
                    dlog.addText( Logger::ROLE,
                                  __FILE__": (doEmergencyMove) skip. exist other teammates." );
                    return false;
                }
            }
        }
    }
#endif

    bool emergency_mode = false;

    if ( opp_trap_pos.x < next_self_pos.x + 3.0
         && next_opp_pos.x < next_self_pos.x + 3.0
         && opp_min < self_step_to_home_pos
         && opp_min <= self_step_to_opp_trap_pos + 1
         //&& std::fabs( opp_trap_pos.y - next_self_pos.y ) > 1.0 // 2009-06-22 removed
         //&& opp_trap_pos.absY() < next_self_pos.absY() + 1.0 // 2009-06-22 added
         && ( opp_trap_pos.absY() < 7.0
              || ( position_type == Position_Left && opp_trap_pos.y < 0.0 )
              || ( position_type == Position_Right && opp_trap_pos.y > 0.0 )
              )
         )
    {
        emergency_mode = true;
    }

    if ( emergency_mode )
    {
        Vector2D target_point( -48.0, 7.0 );

        if ( opp_trap_pos.absY() > 23.0 )
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

        double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );
        double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
        if ( dist_thr < 0.5 ) dist_thr = 0.5;

        dlog.addText( Logger::ROLE,
                      __FILE__": (doEmergencyMove) emergecy move."
                      " target=(%.1f %.1f) dist_thr=%f",
                      target_point.x, target_point.y,
                      dist_thr );
        agent->debugClient().addMessage( "SB:Def:Emergency" );
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );

        doGoToPoint( agent, target_point, dist_thr, dash_power, 20.0 );

        if ( opp_min >= 3 )
        {
            agent->setViewAction( new View_Wide() );
        }

        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doEmergencyMove) no emergency" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDefensiveMove::doMark( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ( ! wm.kickableOpponent()
           && mate_min <= 1
           && wm.lastKickerSide() == wm.ourSide() )
         || mate_min <= opp_min - 5 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (doMark) no mark situation." );
        return false;
    }

    if ( wm.ourDefenseLineX() < - 36.0 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (doMark) side back mark." );
        if ( Bhv_SideBackMarkMove().execute( agent ) )
        {
            return true;
        }
    }
    else
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (doMark) defender mark." );
        if ( Bhv_DefenderMarkMove().execute( agent ) )
        {
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideBackDefensiveMove::doNormalMove( PlayerAgent * agent )
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
    double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "SB:Def:Normal" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    doGoToPoint( agent, target_point, dist_thr, dash_power, 12.0 );

    agent->setNeckAction( new Neck_CheckBallOwner() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideBackDefensiveMove::doGoToPoint( PlayerAgent * agent,
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

    agent->debugClient().addMessage( "TurnTo%.0f",
                                     target_angle.degree() );
    dlog.addText( Logger::ROLE,
                  __FILE__": TurnToAngle %.1f",
                  target_angle.degree() );
}
