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

#include "bhv_defensive_half_offensive_move.h"

#include "strategy.h"
#include "defense_system.h"
#include "mark_analyzer.h"

#include "bhv_basic_move.h"
#include "bhv_attacker_offensive_move.h"
#include "bhv_mid_fielder_free_move.h"
#include "bhv_defensive_half_avoid_mark_move.h"
#include "bhv_get_ball.h"
#include "bhv_block_ball_owner.h"
#include "neck_check_ball_owner.h"
#include "neck_offensive_intercept_neck.h"

#include "wall_break_move/bhv_defensive_half_wall_break_move.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>

#include <rcsc/action/body_intercept.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfOffensiveMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_DefensiveHalfOffensiveMove" );

    const WorldModel & wm = agent->world();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    if ( home_pos.x > 35.0
         && home_pos.absY() < 20.0 )
    {
        agent->debugClient().addMessage( "DH:Off:Attacker" );
        Bhv_AttackerOffensiveMove( false ).execute( agent );
        return true;
    }

    if ( doIntercept( agent ) )
    {
        return true;
    }

    if ( doGetBall( agent ) )
    {
        return true;
    }


     //WallBreaker
    if ( Strategy::i().opponentType() == Strategy::Type_CYRUS )
    {
        if ( Bhv_DefensiveHalfWallBreakMove().execute( agent ) )
        {
            return true;
        }
    }


    // if ( doAvoidMark( agent ) )
    // {
    //     return true;
    // }

    // if ( doFreeMove( agent ) )
    // {
    //     return true;
    // }

    doNormalMove( agent );

    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfOffensiveMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    {
        const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
        if ( fastest_opp )
        {
            opp_min += bound( 0, fastest_opp->posCount() - 2, 5 );
        }
    }

    bool intercept = false;

    if ( ! wm.kickableTeammate()
         //&& self_min <= mate_min + 1
         //&& self_min <= opp_min + opp_penalty
         && self_min <= std::min( opp_min * 1.5, opp_min + 8.0 )
         && self_min <= std::min( mate_min * 1.5, mate_min + 6.0 )
         )
    {
        intercept = true;
    }

    if ( ! intercept
         && ! wm.kickableTeammate()
         && ! wm.kickableOpponent()
         && self_min <= mate_min
         && self_min <= opp_min + 3 )
    {
        intercept = true;
    }

    if ( intercept )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doIntercept) true" );
        agent->debugClient().addMessage( "DH:Off:Intercept" );
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
Bhv_DefensiveHalfOffensiveMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const PlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    if ( ! fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no opponent" );
        return false;
    }

    if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.4 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no stamina" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );


    //
    // block ball owner opponent
    //
    if ( fastest_opp
         && opp_min < self_min // - 2
         && opp_min < mate_min
         && ( ( position_type == Position_Center
                && std::fabs( opp_trap_pos.y - home_pos.y ) < 10.0 )
              || ( position_type == Position_Left
                   && home_pos.y - 15.0 < opp_trap_pos.y
                   && opp_trap_pos.y < home_pos.y + 8.0 )
              || ( position_type == Position_Right
                   && home_pos.y - 8.0 < opp_trap_pos.y
                   && opp_trap_pos.y < home_pos.y + 15.0 ) )
         && opp_trap_pos.x < home_pos.x + 10.0
         && opp_trap_pos.dist( home_pos ) < 18.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) exist block target opponent" );

        //
        // check other blocker
        //
        {
            const Vector2D opponent_pos = DefenseSystem::get_block_opponent_trap_point( wm );
            const Vector2D center_pos = DefenseSystem::get_block_center_point( wm );
            const AngleDeg block_angle = ( center_pos - opponent_pos ).th();
            //const Sector2D block_area( opponent_pos, 1.0, 15.0, block_angle - 20.0, block_angle + 20.0 );

            bool exist = false;
            for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromBall().begin(),
                      end = wm.teammatesFromBall().end();
                  t != end;
                  ++t )
            {
                if ( (*t)->goalie() ) continue;
                if ( (*t)->isTackling() ) continue;
                if ( (*t)->ghostCount() >= 2 ) continue;
                if ( (*t)->unumCount() >= 10 ) continue;
                if ( (*t)->posCount() >= 5 ) continue;

                double dist = (*t)->pos().dist( opponent_pos );
                if ( dist > 10.0 ) continue;

                AngleDeg angle = ( (*t)->pos() - opponent_pos ).th();
                double dir_diff = ( block_angle - angle ).abs();
                if ( dir_diff < 20.0 )
                {
                    dlog.addText( Logger::ROLE,
                                  __FILE__": (doGetBall) exist other blocker. dir_diff=%1.f", dir_diff );
                    exist = true;
                    break;
                }

                double line_dist = dist * AngleDeg::sin_deg( dir_diff );
                if ( line_dist < (*t)->playerTypePtr()->kickableArea() - 0.2 )
                {
                    dlog.addText( Logger::ROLE,
                                  __FILE__": (doGetBall) exist other blocker. dir_diff=%1.f line_dist=%.2f",
                                  dir_diff, line_dist );
                    exist = true;
                    break;
                }
                // if ( block_area.contains( (*t)->pos() ) )
                // {
                //     exist = true;
                //     break;
                // }
            }

            if ( exist )
            {
                //Vector2D target_point = opponent_pos + ( center_pos - opponent_pos ).setLengthVector( 0.7 );
                Vector2D target_point = ( wm.kickableOpponent()
                                          ? fastest_opp->pos() + fastest_opp->vel()
                                          : opp_trap_pos );
                double dash_power = Strategy::get_normal_dash_power( wm );

                double dist_thr = wm.ball().distFromSelf() * 0.1;
                if ( dist_thr < 1.0 ) dist_thr = 1.0;

                if ( wm.self().inertiaFinalPoint().dist2( target_point ) < std::pow( dist_thr, 2 ) )
                {
                    // target_point = opponent_pos;
                    dist_thr = 0.4;
                }

                dlog.addText( Logger::ROLE,
                              __FILE__": (doGetBall) pos=(%.2f %.2f) dash_power=%.1f dist_thr=%.3f",
                              home_pos.x, home_pos.y,
                              dash_power,
                              dist_thr );
                agent->debugClient().addMessage( "DH:Off:BlockMove:%.0f", dash_power );
                agent->debugClient().setTarget( target_point );
                agent->debugClient().addCircle( target_point, dist_thr );

                if ( ! Body_GoToPoint( target_point, dist_thr, dash_power, 1, true, 20.0 ).execute( agent ) )
                {
                    Body_TurnToAngle( 0.0 ).execute( agent );
                }

                agent->setNeckAction( new rcsc::Neck_TurnToBall() );
                return true;
            }
        }

        //
        // try block
        //

        Vector2D top_left( -52.0, home_pos.y - 15.0 );
        //Vector2D bottom_right( home_pos.x + 5.0, home_pos.y + 15.0 );
        Vector2D bottom_right( home_pos.x + 10.0, home_pos.y + 15.0 );

        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) try to get ball" );
        if ( Bhv_GetBall( Rect2D( top_left, bottom_right ) ).execute( agent ) )
        {
            agent->debugClient().addMessage( "DH:Off:GetBall" );
            return true;
        }

        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) try to block ball owner" );
        if ( Bhv_BlockBallOwner( new Rect2D( top_left, bottom_right )
                                 ).execute( agent ) )
        {
            agent->debugClient().addMessage( "DH:Off:Block" );
            return true;
        }
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGetBall) false" );

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfOffensiveMove::doAvoidMark( PlayerAgent * agent )
{
    return Bhv_DefensiveHalfAvoidMarkMove().execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfOffensiveMove::doFreeMove( PlayerAgent * agent )
{
    return Bhv_MidFielderFreeMove().execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_DefensiveHalfOffensiveMove::doNormalMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D target_point = home_pos;

    const double dash_power = Strategy::get_normal_dash_power( wm );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    dlog.addText( Logger::ROLE,
                  __FILE__": (doNormalMove) pos=(%.2f %.2f) dash_power=%.1f dist_thr=%.3f",
                  home_pos.x, home_pos.y,
                  dash_power,
                  dist_thr );
    agent->debugClient().addMessage( "DH:OffNormalMove:%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point, dist_thr, dash_power ).execute( agent ) )
    {
        Body_TurnToAngle( 0.0 ).execute( agent );
    }

    if ( wm.kickableOpponent()
         && wm.ball().distFromSelf() < 18.0 )
    {
        agent->setNeckAction( new Neck_TurnToBall() );
    }
    else
    {
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    }
}
