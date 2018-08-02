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

#include "bhv_offensive_half_offensive_move.h"

#include "strategy.h"

#include "bhv_basic_move.h"
#include "bhv_attacker_offensive_move.h"
#include "bhv_mid_fielder_free_move.h"
#include "bhv_get_ball.h"
#include "bhv_block_ball_owner.h"
#include "neck_check_ball_owner.h"
#include "neck_offensive_intercept_neck.h"

#include "wall_break_move/bhv_offensive_half_wall_break_move.h"

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
Bhv_OffensiveHalfOffensiveMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_OffensiveHalfOffensiveMove" );

    const WorldModel & wm = agent->world();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    if ( home_pos.x > 30.0
         && home_pos.absY() < 7.0 )
    {
        agent->debugClient().addMessage( "OH:Off:Attacker" );
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
        if ( Bhv_OffensiveHalfWallBreakMove().execute( agent ) )
        {
            return true;
        }
    }

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
Bhv_OffensiveHalfOffensiveMove::doIntercept( PlayerAgent * agent )
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
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true (1)" );
        intercept = true;
    }

    if ( ! intercept
         && ! wm.kickableTeammate()
         && ! wm.kickableOpponent()
         && self_min <= mate_min
         && self_min <= opp_min + 3 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true (2)" );
        intercept = true;
    }

#if 0
    // 2012-06-11
    if ( ! intercept
         && ! wm.kickableTeammate()
         && self_min <= mate_min
         && self_min <= opp_min + 2 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept true (3)" );
        intercept = true;
    }
#endif

    if ( self_min == 1
         && mate_min == 1
         && opp_min > 1 )
    {
        Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
        if ( wm.self().pos().dist( ball_next )
             < wm.interceptTable()->fastestTeammate()->pos().dist( ball_next ) )
        {
            dlog.addText( Logger::INTERCEPT,
                          __FILE__": intercept step=1 true" );
            intercept = true;
        }
        else
        {
            dlog.addText( Logger::INTERCEPT,
                          __FILE__": intercept step=1 false" );
            intercept = false;
        }
    }

    if ( intercept )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept" );
        agent->debugClient().addMessage( "OH:Off:Intercept" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": intercept false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfOffensiveMove::doGetBall( PlayerAgent * agent )
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
        Vector2D top_left( -52.0, home_pos.y - 15.0 );
        Vector2D bottom_right( home_pos.x + 5.0, home_pos.y + 15.0 );
        dlog.addText( Logger::ROLE,
                      __FILE__": try block ball owner" );
        if ( Bhv_GetBall( Rect2D( top_left, bottom_right ) ).execute( agent ) )
        {
            agent->debugClient().addMessage( "OH:Off:GetBall" );
            return true;
        }

        if ( Bhv_BlockBallOwner( new Rect2D( top_left, bottom_right )
                                 ).execute( agent ) )
        {
            agent->debugClient().addMessage( "OH:Off:Block" );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfOffensiveMove::doFreeMove( PlayerAgent * agent )
{
    return Bhv_MidFielderFreeMove().execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_OffensiveHalfOffensiveMove::doNormalMove( PlayerAgent * agent )
{
    //Bhv_BasicMove().execute( agent );

    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D target_point = home_pos;

    const double dash_power = Strategy::get_normal_dash_power( wm );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    dlog.addText( Logger::ROLE,
                  __FILE__": OH:OffNormalMove" );
    agent->debugClient().addMessage( "OH:OffNormalMove:%.0f", dash_power );
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
        int count_thr = 0;
        if ( wm.self().viewWidth() == ViewWidth::NORMAL )
        {
            count_thr = 1;
        }

        agent->setNeckAction( new Neck_TurnToBallOrScan( count_thr ) );
    }
}
