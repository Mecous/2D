// -*-c++-*-

/*!
  \file role_savior.cpp
  \brief aggressive goalie
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

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

#include "role_savior.h"

#include "bhv_savior.h"

#include <rcsc/player/world_model.h>
#include <rcsc/game_mode.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

using namespace rcsc;

const std::string RoleSavior::NAME( "Savior" );

/*-------------------------------------------------------------------*/
/*!

 */
namespace {
rcss::RegHolder role = SoccerRole::creators().autoReg( &RoleSavior::create,
                                                       RoleSavior::NAME );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
RoleSavior::execute( rcsc::PlayerAgent * agent )
{
    return Bhv_Savior().execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
RoleSavior::acceptExecution( const WorldModel & wm )
{
    const GameMode mode = wm.gameMode();

    switch( mode.type() ) {
    case GameMode::PlayOn:
    case GameMode::KickIn_:
    case GameMode::CornerKick_:
    case GameMode::OffSide_:
    case GameMode::FoulCharge_:
    case GameMode::BeforeKickOff:
    case GameMode::KickOff_:
    case GameMode::AfterGoal_:
    case GameMode::PenaltyTaken_:
        return true;
        break;

    case GameMode::GoalKick_:
    case GameMode::GoalieCatch_:
    case GameMode::BackPass_:
        return mode.side() != wm.ourSide();
        break;

    case GameMode::FreeKick_:
        //return wm.ball().pos().x > ServerParam::i().ourPenaltyAreaLineX();
        return true;

        break;
    default:
        break;
    }

    return false;
}
