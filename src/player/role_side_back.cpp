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

#include "role_side_back.h"

#include "strategy.h"
#include "defense_system.h"

#include "bhv_chain_action.h"

#include "bhv_basic_move.h"
#include "bhv_tactical_intercept.h"

#include "bhv_side_back_aggressive_cross_block.h"
#include "bhv_side_back_danger_move.h"
#include "bhv_side_back_defensive_move.h"
#include "bhv_side_back_offensive_move.h"

#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

using namespace rcsc;

const std::string RoleSideBack::NAME( "SideBack" );

/*-------------------------------------------------------------------*/
/*!

 */
namespace {
rcss::RegHolder role = SoccerRole::creators().autoReg( &RoleSideBack::create,
                                                       RoleSideBack::NAME );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
RoleSideBack::execute( PlayerAgent * agent )
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
RoleSideBack::doKick( PlayerAgent * agent )
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
RoleSideBack::doMove( PlayerAgent * agent )
{
#ifdef USE_TACTICAL_INTERCEPT
    if ( Bhv_TacticalIntercept().execute( agent ) )
    {
        return;
    }
#endif

    const WorldModel & wm = agent->world();

    int ball_step = 1000;
    ball_step = std::min( ball_step, wm.interceptTable()->teammateReachCycle() );
    ball_step = std::min( ball_step, wm.interceptTable()->opponentReachCycle() );
    ball_step = std::min( ball_step, wm.interceptTable()->selfReachCycle() );

    Vector2D ball_pos = wm.ball().inertiaPoint( ball_step );

    PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );


    switch ( Strategy::get_ball_area( wm ) ) {
    case BA_CrossBlock:
        if ( ( ball_pos.y < 0.0
               && position_type != Position_Right )
             || ( ball_pos.y > 0.0
                  && position_type != Position_Left ) )
        {
            Bhv_SideBackAggressiveCrossBlock().execute( agent );
        }
        else
        {
            Bhv_SideBackDefensiveMove().execute( agent );
        }
        break;
    case BA_Danger:
        Bhv_SideBackDangerMove().execute( agent );
        break;

    case BA_DribbleBlock:
    case BA_DefMidField:
        Bhv_SideBackDefensiveMove().execute( agent );
        break;
    case BA_DribbleAttack:
    case BA_OffMidField:
        Bhv_SideBackOffensiveMove().execute( agent );
        break;
    case BA_Cross:
    case BA_ShootChance:
        Bhv_SideBackOffensiveMove().execute( agent );
        break;
    default:
        dlog.addText( Logger::ROLE,
                      __FILE__": unknown ball area" );
        Bhv_BasicMove().execute( agent );
        break;
    }
}
