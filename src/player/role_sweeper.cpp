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

#include "role_sweeper.h"

#include "strategy.h"

#include "bhv_chain_action.h"

#include "bhv_basic_move.h"
#include "bhv_sweeper_danger_move.h"
#include "bhv_sweeper_defensive_move.h"

#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/common/logger.h>

using namespace rcsc;

const std::string RoleSweeper::NAME( "Sweeper" );

/*-------------------------------------------------------------------*/
/*!

 */
namespace {
rcss::RegHolder role = SoccerRole::creators().autoReg( &RoleSweeper::create,
                                                       RoleSweeper::NAME );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
RoleSweeper::execute( PlayerAgent * agent )
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
RoleSweeper::doKick( PlayerAgent * agent )
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
RoleSweeper::doMove( rcsc::PlayerAgent * agent )
{
#ifdef USE_TACTICAL_INTERCEPT
    if ( Bhv_TacticalIntercept().execute( agent ) )
    {
        return;
    }
#endif

    switch ( Strategy::get_ball_area( agent->world() ) ) {
    case BA_Danger:
        Bhv_SweeperDangerMove().execute( agent );
        break;
    case BA_CrossBlock:
    case BA_DefMidField:
    case BA_DribbleBlock:
        Bhv_SweeperDefensiveMove().execute( agent );
        break;
    case BA_DribbleAttack:
    case BA_OffMidField:
    case BA_Cross:
    case BA_ShootChance:
        Bhv_BasicMove().execute( agent );
        break;
    default:
        dlog.addText( Logger::ROLE,
                      __FILE__": unknown ball pos" );
        Bhv_BasicMove().execute( agent );
        break;
    }
}
