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

#include "bhv_go_to_static_ball.h"

#include "bhv_set_play.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_GoToStaticBall::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const PlayerType & ptype = wm.self().playerType();

    const double dir_margin_wait = 20.0;
    const double dir_margin_move = 10.0;
    const double dist_tolerance = ptype.playerSize() + ServerParam::i().ballSize() + 0.08;

    const AngleDeg angle_diff = ( wm.ball().pos() - wm.self().pos() ).th() - M_ball_place_angle;

    dlog.addText( Logger::TEAM,
                  __FILE__": angle_diff=%.1f dir_margin_wait=%.1f dir_margin_move=%.1f",
                  angle_diff.abs(), dir_margin_wait, dir_margin_move );
    dlog.addText( Logger::TEAM,
                  __FILE__": ball_dist=%.3f  dist_tolerance=%.3f",
                  wm.ball().distFromSelf(), dist_tolerance );

    if ( angle_diff.abs() < dir_margin_wait
         && wm.ball().distFromSelf() < dist_tolerance )
    {
        // already reach
        return false;
    }

    double dash_power = 20.0;
    double dash_speed = -1.0;
    if ( angle_diff.abs() > dir_margin_move
         || wm.ball().distFromSelf() > 2.0 )
    {
        dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
        dlog.addText( Logger::TEAM,
                      __FILE__": normal dash_power = %.2f", dash_power );
    }
    else
    {
        Vector2D ball_rel = ( wm.ball().pos() - wm.self().pos() ).rotatedVector( -wm.self().body() );
        double move_x = ball_rel.x - ptype.playerSize() - ServerParam::i().ballSize();
        double vel_x = move_x
            * ( 1.0 - ptype.playerDecay() )
            / ( 1.0 - std::pow( ptype.playerDecay(), 2 ) );

        Vector2D self_vel = wm.self().vel().rotatedVector( -wm.self().body() );
        double required_accel = vel_x - self_vel.x;
        dash_power = required_accel / wm.self().dashRate();
        dash_power = wm.self().getSafetyDashPower( dash_power );

        dlog.addText( Logger::TEAM,
                      __FILE__": close ball dash_power = %.2f", dash_power );
    }

    // it is necessary to go to sub target point
    if ( angle_diff.abs() > dir_margin_move )
    {
        const Vector2D sub_target = wm.ball().pos() + Vector2D::polar2vector( 2.0, M_ball_place_angle + 180.0 );
        dlog.addText( Logger::TEAM,
                            __FILE__": go to subtarget=(%.2f %.2f)",
                            sub_target.x, sub_target.y );
        Body_GoToPoint( sub_target,
                        0.1,
                        dash_power,
                        dash_speed ).execute( agent );
    }
    // dir diff is small. go to ball
    else
    {
        // need to adjust the body direction
        if ( ( wm.ball().angleFromSelf() - wm.self().body() ).abs() > 1.5 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": turn to ball. angle_diff=%.1f",
                          ( wm.ball().angleFromSelf() - wm.self().body() ).abs() );
            Body_TurnToBall().execute( agent );
        }
        // dash to ball
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": dash to ball" );
            agent->doDash( dash_power );
        }
    }

    agent->setNeckAction( new Neck_ScanField() );

    return true;
}
