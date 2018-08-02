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

#include "bhv_sweeper_defensive_move.h"

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
Bhv_SweeperDefensiveMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_SweeperDefensiveMove" );

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

    doNormalMove( agent );
    return true;

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SweeperDefensiveMove::doIntercept( PlayerAgent * agent )
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

    const PlayerObject * first_teammate = wm.interceptTable()->fastestTeammate();
    const PlayerObject * first_opponent = wm.interceptTable()->fastestOpponent();
    const int pos_count = ( first_teammate
                            ? first_teammate->posCount()
                            : 1000 );
    const double ball_speed = wm.ball().vel().r() * std::pow( ServerParam::i().ballDecay(), opp_min );

    bool intercept = false;

    if ( self_min <= mate_min + 1
         && self_min <= opp_min + 1 )
    {
        if ( mate_min < self_min
             && mate_min <= 2
             && opp_min >= 5
             && ( ! first_opponent
                  || first_opponent->distFromBall() > 5.0 ) )
        {

        }
        else
        {
            intercept = true;
            dlog.addText( Logger::ROLE,
                          __FILE__": intercept true (1)" );
        }
    }

    if ( ! intercept
         && opp_min >= 2
         && ( mate_min > 1 || pos_count > 0 )
         && ! wm.kickableTeammate()
         && self_min <= opp_min + 3
         && self_min <= mate_min + 4 )
    {
        intercept = true;
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true (2)" );
    }

    if ( ! intercept
         && opp_min >= 3
         && ( mate_min > 1 || pos_count > 0 )
         && ! wm.kickableTeammate()
         && self_min <= opp_min + 4
         && self_min <= mate_min * 1.3
         && ball_speed < 0.8 )
    {
        intercept = true;
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true (3)" );
    }

    if ( ! intercept
         && opp_min >= 4
         && ( mate_min > 1 || pos_count > 0 )
         && ! wm.kickableTeammate()
         && self_min <= opp_min + 5
         && self_min <= mate_min + 2
         && ball_speed < 0.8 )
    {
        intercept = true;
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true (4)" );
    }

    if ( intercept
         && first_teammate
         && opp_min >= 5
         && self_min <= 3
         && self_min >= mate_min )
    {
        Vector2D ball_pos = wm.ball().inertiaPoint( mate_min );
        double self_dist = wm.self().pos().dist( ball_pos );
        double teammate_dist = first_teammate->pos().dist( ball_pos );
        if ( teammate_dist < self_dist * 0.8 )
        {
            intercept = false;
            dlog.addText( Logger::ROLE,
                          __FILE__": intercept cancel. self_dist=%.3f teammate_dist=%.3f",
                          self_dist, teammate_dist );
        }
    }


    if ( intercept )
    {
        agent->debugClient().addMessage( "SW:Def:Intercept" );
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
Bhv_SweeperDefensiveMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    bool get_ball_situation = false;

    if ( opp_min < mate_min
         && opp_min < self_min )
    {
        if ( ( std::fabs( opp_trap_pos.y - home_pos.y ) < 4.0
               || ( std::fabs( opp_trap_pos.y - home_pos.y ) < 6.0
                    && opp_trap_pos.absY() < 15.0 )
               || opp_trap_pos.absY() < 5.0
               || ( opp_trap_pos.absY() < home_pos.absY()
                    && opp_trap_pos.y * home_pos.y > 0.0 )
               )
             && opp_trap_pos.x < home_pos.x + 10.0
             && wm.self().pos().x < opp_trap_pos.x + 12.0
                                    && wm.self().pos().x > opp_trap_pos.x - 10.0
             )
        {
            get_ball_situation = true;
        }
    }

    if ( ! get_ball_situation )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no get ball situation" );
        return false;
    }

    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    const PlayerObject * nearest_to_home_opp = wm.getOpponentNearestTo( home_pos, 10, NULL );

    if ( nearest_to_home_opp
         && nearest_to_home_opp != fastest_opp
         && nearest_to_home_opp->pos().x < opp_trap_pos.x + 2.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) exist other attacker %d (%.1f %.1f)",
                      nearest_to_home_opp->unum(),
                      nearest_to_home_opp->pos().x, nearest_to_home_opp->pos().y );
        return false;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGetBall) try Bhv_GetBall" );

    PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    double max_x = std::min( home_pos.x + 4.0, wm.self().pos().x + 1.0 );
    double min_y = ( position_type == Position_Left
                     ? home_pos.y - 5.0
                     : home_pos.y - 8.0 );
    double max_y = ( position_type == Position_Left
                     ? home_pos.y + 8.0
                     : home_pos.y + 5.0 );
    //double max_x = home_pos.x + 2.0;
    Rect2D bounding_rect( Vector2D( -50.0, min_y ),
                          Vector2D( max_x, max_y ) );
    if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SW:Def:GetBall" );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) done Bhv_GetBall" );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGetBall) could not find the position." );

#if 1
    // 2009-06-24
    Vector2D base_point( - ServerParam::i().pitchHalfLength() - 5.0, 0.0 );

    Vector2D rel_vec = ( opp_trap_pos - base_point );
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
void
Bhv_SweeperDefensiveMove::doNormalMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->teammateReachStep();

    Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    Vector2D target_point = home_pos;
#if 1
    if ( wm.ourDefenseLineX() < -35.0
         && ( opponent_step <= teammate_step - 5
              || opponent_step == 0
              || wm.kickableOpponent() ) )
    {
        for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
                  end = wm.opponentsFromSelf().end();
              o != end;
              ++o )
        {
            if ( (*o)->isGhost() ) continue;
            if ( (*o)->posCount() >= 10 ) continue;
            if ( wm.ourDefenseLineX() - 0.5< (*o)->pos().x
                 && (*o)->pos().x < target_point.x )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__":(doNormalMove) adjust to opponent[%d] (%.2f %.2f)",
                              (*o)->unum(),
                              target_point.x, target_point.y );
                //target_point.x = std::max( home_pos.x - 2.0, (*o)->pos().x - 0.5 );
                target_point.x = std::max( -48.0, (*o)->pos().x + 0.5 );
            }
        }
    }
#endif

    //
    // avoid teammate
    //
    if ( teammate_step < opponent_step )
    {
        const Vector2D ball_pos = wm.ball().inertiaPoint( teammate_step );
        const Vector2D my_inertia = wm.self().inertiaFinalPoint();
        const Segment2D move_segment( my_inertia, target_point );

        if ( move_segment.dist( ball_pos ) < ServerParam::i().defaultKickableArea() * 3.0 )
        {
            Vector2D projection_point = Line2D( my_inertia, target_point ).projection( ball_pos );
            if ( projection_point.isValid() )
            {
                Vector2D new_target = target_point;
                Vector2D add_vec = projection_point - ball_pos;
                if ( add_vec.r2() < 1.0e-6 )
                {
                    add_vec = target_point - my_inertia;
                    add_vec.rotate( 90.0 );
                    add_vec.setLength( ServerParam::i().defaultKickableArea() * 3.0 );
                    new_target = ball_pos + add_vec;
                }
                else
                {
                    add_vec.setLength( ServerParam::i().defaultKickableArea() * 3.0 );
                    new_target = ball_pos + add_vec;
                }

                dlog.addText( Logger::ROLE,
                              __FILE__":(doNormalMove) adjust target (%.1f %.1f)->(%.1f %.1f)",
                              target_point.x, target_point.y,
                              new_target.x, new_target.y );
                agent->debugClient().addMessage( "Adjust" );

                target_point = new_target;
            }
        }
    }

    double dash_power = DefenseSystem::get_defender_dash_power( wm, target_point );
    //double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.1;
    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "SW:Def:Normal" );

    doGoToPoint( agent, target_point, dist_thr, dash_power, 12.0 );

    agent->setNeckAction( new Neck_CheckBallOwner() );

#if 1
    // 2012-06-21
    if ( teammate_step >= 3
         && opponent_step >= 5 )
    {
        const PlayerObject * t = wm.getTeammateNearestToBall( 5 );
        const PlayerObject * o = wm.getOpponentNearestToBall( 5 );
        if ( t && o
             && t->distFromBall() > ( t->playerTypePtr()->kickableArea()
                                   + t->distFromSelf() * 0.05
                                   + wm.ball().distFromSelf() * 0.05 )
             && o->distFromBall() > ( o->playerTypePtr()->kickableArea()
                                   + o->distFromSelf() * 0.05
                                      + wm.ball().distFromSelf() * 0.05 ) )
        {
            if ( teammate_step >= 4
                 && opponent_step >= 6 )
            {
                dlog.addText( Logger::ROLE, __FILE__":(doNormalMove) view wide" );
                agent->setViewAction( new View_Wide() );
            }
            else
            {
                dlog.addText( Logger::ROLE, __FILE__":(doNormalMove) view normal" );
                agent->setViewAction( new View_Normal() );
            }
        }
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SweeperDefensiveMove::doGoToPoint( PlayerAgent * agent,
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
        agent->debugClient().addMessage( "SW:Def:Go%.1f", dash_power );
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
