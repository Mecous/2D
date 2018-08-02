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

#include "role_defensive_half.h"

#include "strategy.h"

#include "bhv_basic_move.h"
#include "bhv_tactical_intercept.h"
#include "bhv_chain_action.h"


#include "bhv_defensive_half_cross_block_move.h"
#include "bhv_defensive_half_danger_move.h"
#include "bhv_defensive_half_defensive_move.h"
#include "bhv_defensive_half_offensive_move.h"

#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

using namespace rcsc;

const std::string RoleDefensiveHalf::NAME( "DefensiveHalf" );

/*-------------------------------------------------------------------*/
/*!

 */
namespace {
rcss::RegHolder role = SoccerRole::creators().autoReg( &RoleDefensiveHalf::create,
                                                       RoleDefensiveHalf::NAME );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleDefensiveHalf::execute( PlayerAgent * agent )
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
RoleDefensiveHalf::doKick( PlayerAgent * agent )
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
RoleDefensiveHalf::doMove( PlayerAgent * agent )
{
#ifdef USE_TACTICAL_INTERCEPT
    if ( Bhv_TacticalIntercept().execute( agent ) )
    {
        return;
    }
#endif

    switch ( Strategy::get_ball_area( agent->world() ) ) {
    case BA_CrossBlock:
        Bhv_DefensiveHalfCrossBlockMove().execute( agent );
        break;
    case BA_Danger:
        Bhv_DefensiveHalfDangerMove().execute( agent );
        break;
    case BA_DribbleBlock:
    case BA_DefMidField:
        Bhv_DefensiveHalfDefensiveMove().execute( agent );
        break;
    case BA_DribbleAttack:
        Bhv_DefensiveHalfOffensiveMove().execute( agent );
        break;
    case BA_OffMidField:
        Bhv_DefensiveHalfOffensiveMove().execute( agent );
        break;
    case BA_Cross:
        Bhv_DefensiveHalfOffensiveMove().execute( agent );
        break;
    case BA_ShootChance:
        Bhv_DefensiveHalfOffensiveMove().execute( agent );
        break;
    default:
        dlog.addText( Logger::ROLE,
                            __FILE__": unknown ball area" );
        Bhv_BasicMove().execute( agent );
        break;
    }
}
