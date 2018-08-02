// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_clear_ball.h"

#include "generator_clear.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/soccer_intention.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_ClearBall::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__"(execute)" );

    const WorldModel & wm = agent->world();

    CooperativeAction::Ptr action = GeneratorClear::instance().getBestAction( wm );

    if ( ! action )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__" not found" );
        Body_HoldBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return false;
    }

    const double first_ball_speed = action->firstBallVel().r();

    agent->debugClient().addMessage( "Clear" );
    agent->debugClient().setTarget( action->targetBallPos() );
    dlog.addText( Logger::TEAM,
                  __FILE__" clear to (%.2f %.2f) speed=%.3f",
                  action->targetBallPos().x, action->targetBallPos().y,
                  first_ball_speed );

    if ( action->kickCount() == 1 )
    {
        Body_KickOneStep( action->targetBallPos(),
                          first_ball_speed ).execute( agent );
    }
    else
    {
        Body_SmartKick( action->targetBallPos(),
                        first_ball_speed,
                        first_ball_speed * 0.96,
                        2 ).execute( agent ); // 2 kick
    }
    agent->setNeckAction( new Neck_ScanField() );

    return true;
}
