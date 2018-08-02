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

#include "bhv_mid_fielder_free_move.h"

#include "strategy.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_turn_to_ball.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

using namespace rcsc;


namespace {

double
get_congestion( const WorldModel & wm,
                const Vector2D & pos )
{
    double value = 0.0;
    for ( PlayerObject::Cont::const_iterator p = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          p != end;
          ++p )
    {
        if ( (*p)->distFromSelf() > 20.0 ) break;

        double d2 = (*p)->pos().dist2( pos );
        if ( d2 > std::pow( 5.0, 2 ) ) continue;

        value += 1.0 / d2;
    }

    return value;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_MidFielderFreeMove::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.6 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(execute) no stamina" );
        return false;
    }

    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( opponent_step < teammate_step )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(execute) not our ball" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    Vector2D target_point = home_pos;

    double best_congestion = get_congestion( wm, target_point );
    double last_dir = 0.0;
    for ( int i = 0; i < 3; ++i )
    {
        Vector2D best_pos = target_point;
        double best_dir = 0.0;
        for ( double dir = 0.0; dir < 359.0; dir += 360.0/8.0 )
        {
            if ( std::fabs( AngleDeg::normalize_angle( dir - last_dir ) - 180.0 ) < 1.0e-3 )
            {
                continue;
            }

            Vector2D add_vec = Vector2D::polar2vector( 1.0, dir );
            Vector2D tmp_pos = target_point + add_vec;
            double congestion = get_congestion( wm, tmp_pos );
            if ( congestion < best_congestion - 1.0e-5 )
            {
                best_congestion = congestion;
                best_pos = tmp_pos;
                best_dir = dir;
            }
        }

        last_dir = best_dir;
        if ( best_pos == target_point )
        {
            break;
        }

        // dlog.addRect( Logger::ROLE,
        //               best_pos.x - 0.1, best_pos.y - 0.1, 0.2, 0.2, "#00F" );
        target_point = best_pos;
    }

    if ( home_pos == target_point )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (execute) target point not updated" );
        return false;
    }


    const double dash_power = Strategy::get_normal_dash_power( wm );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    dlog.addText( Logger::ROLE,
                  __FILE__":(execute)" );
    agent->debugClient().addMessage( "MidFreeMove:%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point, dist_thr, dash_power ).execute( agent ) )
    {
        Body_TurnToBall().execute( agent );
    }

    int count_thr = 0;
    if ( wm.self().viewWidth() == ViewWidth::NORMAL )
    {
        count_thr = 1;
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( count_thr ) );

    return true;
}
