// -*-c++-*-

/*!
  \file neck_chase_ball.cpp
  \brief turn neck with attention to ball persistently
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA, Hidehisa AKIYAMA

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

#include "neck_chase_ball.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball.h>
#include <rcsc/action/view_synch.h>
#include <rcsc/action/view_normal.h>
#include <rcsc/action/view_wide.h>

#include <rcsc/math_util.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
Neck_ChaseBall::Neck_ChaseBall()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Neck_ChaseBall::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ACTION,
                  __FILE__ ": Neck_ChaseBall" );
    agent->debugClient().addMessage( "NeckChaseBall" );

    const ServerParam & param = ServerParam::i();
    const WorldModel & wm = agent->world();

    const Vector2D next_self_pos = agent->effector().queuedNextMyPos();
    const AngleDeg next_self_body = agent->effector().queuedNextMyBody();
    const Vector2D next_ball_pos = agent->effector().queuedNextBallPos();

    const double narrow_half_width = ViewWidth::width( ViewWidth::NARROW ) * 0.5;
    const double normal_half_width = ViewWidth::width( ViewWidth::NORMAL ) * 0.5;
    const double next_view_half_width = agent->effector().queuedNextViewWidth().width() * 0.5;

    const AngleDeg angle_diff = ( next_ball_pos - next_self_pos ).th() - next_self_body;

    const SeeState & see_state = agent->seeState();
    const int see_cycles = see_state.cyclesTillNextSee() + 1;

    const bool in_visible_area = ( next_self_pos.dist2( next_ball_pos ) < std::pow( param.visibleDistance() - 0.2, 2 ) );
    const bool can_see_next_cycle_narrow = ( angle_diff.abs() < param.maxNeckAngle() + narrow_half_width );
    const bool can_see_next_cycle = ( angle_diff.abs() < param.maxNeckAngle() + next_view_half_width );

    dlog.addText( Logger::ACTION,
                  __FILE__": see_cycles = %d",
                  see_cycles );
    dlog.addText( Logger::ACTION,
                  __FILE__": will_be_in_visible_area = %s",
                  (in_visible_area ? "true" : "false") );
    dlog.addText( Logger::ACTION,
                  __FILE__": can_see_next_cycle = %s",
                  (can_see_next_cycle ? "true" : "false") );
    dlog.addText( Logger::ACTION,
                  __FILE__": can_see_next_cycle_narrow = %s",
                  (can_see_next_cycle_narrow ? "true" : "false") );

    const int teammate_reach_cycle = wm.interceptTable()->teammateReachCycle();
    const int opponent_reach_cycle = wm.interceptTable()->opponentReachCycle();
    const int predict_step = std::min( opponent_reach_cycle,
                                       teammate_reach_cycle );

    dlog.addText( Logger::ACTION,
                  __FILE__": predict_step = %d", predict_step );

    if ( predict_step < 1 )
    {
        dlog.addText( Logger::ACTION,
                      __FILE__": may be kicked, set view to synch" );

        View_Synch().execute( agent );
    }
    else if ( in_visible_area
              || can_see_next_cycle )
    {
        if ( see_cycles != 1
             && can_see_next_cycle_narrow )
        {
            dlog.addText( Logger::ACTION,
                          __FILE__": set view to synch" );

            View_Synch().execute( agent );
        }
        else
        {
            dlog.addText( Logger::ACTION,
                          __FILE__": view untouched" );
        }
    }
    else
    {
        if ( see_cycles >= 2
             && can_see_next_cycle_narrow )
        {
            dlog.addText( Logger::ACTION,
                          __FILE__": set view to narrow" );

            View_Synch().execute( agent );
        }
        else if ( angle_diff.abs() < normal_half_width - 5.0 )
        {
            dlog.addText( Logger::ACTION,
                          __FILE__": set view to normal" );

            View_Normal().execute( agent );
        }
        else
        {
            dlog.addText( Logger::ACTION,
                          __FILE__": set view to wide" );

            View_Wide().execute( agent );
        }
    }

    if ( ! can_see_next_cycle
         || ( wm.ball().seenPosCount() <= 0
              && wm.ball().seenVelCount() <= 1
              && predict_step >= 4 ) )
    {
        dlog.addText( Logger::ACTION,
                      __FILE__": ScanField" );
        Neck_ScanField().execute( agent );
        return true;
    }

    double target_angle = bound( - param.maxNeckAngle(),
                                 angle_diff.degree(),
                                 + param.maxNeckAngle() );

    dlog.addText( Logger::ACTION,
                  __FILE__": target_angle = %.0f angle_diff = %.0f",
                  target_angle, angle_diff.degree() );
    agent->doTurnNeck( target_angle - wm.self().neck() );

    return true;
}
