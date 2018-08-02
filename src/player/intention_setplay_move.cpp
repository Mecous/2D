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

#include "intention_setplay_move.h"

#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/body_intercept.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

*/
IntentionSetplayMove::IntentionSetplayMove( const Vector2D & target_point,
                                            const int max_step,
                                            const GameTime & start_time )
    : M_target_point( target_point ),
      M_step( max_step ),
      M_last_time( start_time )
{

}

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionSetplayMove::finished( const PlayerAgent * agent )
{
    if ( M_step <= 0 )
    {
        dlog.addText( Logger::TEAM,
                            __FILE__":(finished) time over" );
        return true;
    }

    const WorldModel & wm = agent->world();

    if ( wm.gameMode().type() == GameMode::PlayOn )
    {
        dlog.addText( Logger::TEAM,
                            __FILE__":(finished) playon" );
        return true;
    }

    if ( wm.self().isKickable() )
    {
        dlog.addText( Logger::TEAM,
                            __FILE__":(finished) kickable" );
        return true;
    }

    if ( M_last_time.cycle() < wm.time().cycle() - 1 )
    {
        dlog.addText( Logger::TEAM,
                            __FILE__":(finished) strange time." );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionSetplayMove::execute( PlayerAgent * agent )
{
    if ( M_step <= 0 )
    {
        dlog.addText( Logger::TEAM,
                            __FILE__":(execute) time over" );
        return false;
    }

    const WorldModel & wm = agent->world();
    const int time_thr = 8 - ( 1 + 3 ); // kicker wait - ( turn + view )
    const int time_till_kick = std::max( 0,
                                         ServerParam::i().dropBallTime() - time_thr - wm.getSetPlayCount() );

    M_step -= 1;
    M_last_time = wm.time();

    double dash_power = wm.self().staminaModel().getSafetyDashPower( wm.self().playerType(),
                                                                     ServerParam::i().maxDashPower(),
                                                                     500.0 );

    dlog.addText( Logger::TEAM,
                  __FILE__":(execute) time till kick = %d", time_till_kick );


    if ( time_till_kick > 0
         && wm.self().pos().x < wm.offsideLineX() - 0.5
         && M_target_point.x > wm.offsideLineX() - 0.5 )
    {
        Line2D move_line( wm.self().pos(), M_target_point );
        double y = move_line.getY( wm.offsideLineX() - 0.5 );
        Vector2D intersection( wm.offsideLineX(), y );
        Vector2D self_pos = wm.self().inertiaPoint( time_till_kick );
        double move_dist = self_pos.dist( intersection );
        int move_step = wm.self().playerType().cyclesToReachDistance( move_dist );
        if ( move_step < time_till_kick )
        {
            dash_power = 1.0;
            dlog.addText( Logger::TEAM,
                          __FILE__":(execute) save dash power to avoid offside" );
        }
    }

    if ( wm.self().pos().x > wm.offsideLineX() - 0.5
         && M_target_point.x > wm.offsideLineX() - 0.5 )
    {
        dash_power = 0.01;
        dlog.addText( Logger::TEAM,
                      __FILE__":(execute) over offside line" );
    }

    double dist_thr = 0.5;

    dlog.addText( Logger::TEAM,
                  __FILE__": (execute) dash_power=%.1f", dash_power );
    agent->debugClient().addMessage( "IntentionSetPlayMove" );
    agent->debugClient().setTarget( M_target_point );
    agent->debugClient().addCircle( M_target_point, dist_thr );

    if ( ! Body_GoToPoint( M_target_point, dist_thr, dash_power ).execute( agent ) )
    {
        Body_TurnToBall().execute( agent );
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    return true;
}
