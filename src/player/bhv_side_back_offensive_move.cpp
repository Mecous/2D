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

#include "bhv_side_back_offensive_move.h"

#include "strategy.h"
#include "mark_analyzer.h"
#include "defense_system.h"
#include "field_analyzer.h"

#include "bhv_get_ball.h"
#include "bhv_defender_mark_move.h"
#include "bhv_side_back_block_ball_owner.h"
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
Bhv_SideBackOffensiveMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_SideBackOffensiveMove" );

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
Bhv_SideBackOffensiveMove::doIntercept( PlayerAgent * agent )
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

    const PlayerObject * teammate = wm.interceptTable()->fastestTeammate();
    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();

    if ( self_min > mate_min + 2 )
    {
        return false;
    }

    const Vector2D self_ball_pos = wm.ball().inertiaPoint( self_min );
    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opp_min );

    bool intercept = false;

    if ( self_min <= opp_min )
    {
        intercept = true;
    }
    else if ( //( ( opp_min >= 3 && self_min <= opp_min + 4 )
              //  || ( opp_min >= 2 && self_min <= opp_min + 3 ) ) &&
             self_ball_pos.dist2( opponent_ball_pos ) < std::pow( ServerParam::i().tackleDist() - 0.2, 2 ) )
    {
        intercept = true;
    }

    if ( intercept )
    {
        agent->debugClient().addMessage( "SB:Off:Intercept" );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doIntercept) self=%d t=%d,count=%d o=%d,count=%d",
                      self_min,
                      mate_min, ( teammate ? teammate->posCount() : -1 ),
                      opp_min, ( opponent ? opponent->posCount() : -1 ) );
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
                  __FILE__":(doIntercept) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackOffensiveMove::doAttackBallOwner( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockBallOwner) no opponent" );
        return false;
    }

    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( teammate_step < opponent_step )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockBallOwner) ball owner is teammate" );
        return false;
    }

    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    //
    // check mark target
    //
    {
        const AbstractPlayerObject * mark_target =  MarkAnalyzer::i().getTargetOf( wm.self().unum() );

        if ( mark_target )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doBlockBallOwner) opponent=%d mark_target=%d",
                          opponent->unum(), mark_target->unum() );
        }

        if ( ! mark_target
             || mark_target->unum() != opponent->unum() )
        {
            const double self_dist = opponent_ball_pos.dist2( wm.self().pos() );
            const AngleDeg opponent_goal_angle = ( ServerParam::i().ourTeamGoalPos() - opponent_ball_pos ).th();

            const AbstractPlayerObject * marker = MarkAnalyzer::i().getMarkerOf( opponent );
            if ( marker
                 && ( marker->pos().dist( opponent_ball_pos ) < self_dist + 3.0
                      || ( ( marker->pos() - opponent_ball_pos ).th() - opponent_goal_angle ).abs() < 25.0 )
                 )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__": (doBlockBallOwner) false: exist other marker" );
                return false;
            }

            for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(), end = wm.teammates().end();
                  t != end;
                  ++t )
            {
                if ( (*t)->goalie() ) continue;
                if ( (*t)->posCount() > 20 ) continue;
                if ( (*t)->isTackling() ) continue;
                if ( (*t)->unum() == Unum_Unknown ) continue;

                double d = (*t)->pos().dist( opponent_ball_pos );
                if ( d < self_dist
                     || ( ( (*t)->pos() - opponent_ball_pos ).th() - opponent_goal_angle ).abs() < 20.0 )
                {
                    dlog.addText( Logger::ROLE,
                                  __FILE__": (doBlockBallOwner) false: exist other teammate" );
                    return false;
                }
            }
        }
    }

    //
    //
    //

    dlog.addText( Logger::ROLE,
                  __FILE__": (doBlockBallOwner) opponent=%d ball_pos=(%.1f %.1f)",
                  opponent->unum(),
                  opponent_ball_pos.x, opponent_ball_pos.y );

    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );
    const double min_y = ( position_type == Position_Right
                           ? -10.0
                           : -33.5 );
    const double max_y = ( position_type == Position_Left
                           ? +10.0
                           : +33.5 );

    Rect2D bounding_rect( Vector2D( -51.0, min_y ),
                          Vector2D( 0.0, max_y ) ); // Vector2D( -15.0, max_y ) );
    if ( Bhv_SideBackBlockBallOwner( bounding_rect ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SB:Block1" );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doBlockBallOwner) false" );

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackOffensiveMove::doMarkMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( mate_min <= opp_min - 3 )
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
Bhv_SideBackOffensiveMove::doNormalMove( PlayerAgent * agent )
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

    agent->debugClient().addMessage( "SB:Off:Normal" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    doGoToPoint( agent, target_point, dist_thr, dash_power, 12.0 );

    const AbstractPlayerObject * mark_target =  MarkAnalyzer::i().getTargetOf( wm.self().unum() );
    if ( mark_target )
    {
        DefenseSystem::mark_turn_neck( agent, mark_target );
    }
    else
    {
        agent->setNeckAction( new Neck_CheckBallOwner() );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideBackOffensiveMove::doGoToPoint( PlayerAgent * agent,
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
    }
    else
    {
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
}
