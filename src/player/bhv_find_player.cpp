// -*-c++-*-

/*!
  \file bhv_find_player.cpp
  \brief search specified player by body and neck
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

#include "bhv_find_player.h"

#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/body_turn_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/abstract_player_object.h>
#include <rcsc/player/soccer_intention.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/math_util.h>

#include <vector>
#include <algorithm>
#include <cmath>

using namespace rcsc;

// class IntentionFindPlayer
//     : public SoccerIntention {
// private:
// public:
// };

/*-------------------------------------------------------------------*/
/*!

 */
Bhv_FindPlayer::Bhv_FindPlayer( const AbstractPlayerObject * target_player,
                                const int count_threshold )
    : M_target_player( target_player ),
      M_count_threshold( count_threshold )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_FindPlayer::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ACTION,
                  __FILE__": Bhv_FindPlayer" );

    if ( ! M_target_player )
    {
        dlog.addText( Logger::ACTION,
                      __FILE__": NULL" );
        return false;
    }

    dlog.addText( Logger::ACTION,
                  __FILE__": target player side=%c unum=%d (%.2f %.2f) seenCount=%d posCount=%d threshold=%d",
                  ( M_target_player->side() == LEFT ? 'L' :
                    M_target_player->side() == RIGHT ? 'R' :
                    'N' ),
                  M_target_player->unum(),
                  M_target_player->pos().x, M_target_player->pos().y,
                  M_target_player->seenPosCount(),
                  M_target_player->posCount(),
                  M_count_threshold );


    if ( M_target_player->seenPosCount() <= M_count_threshold
         && M_target_player->posCount() <= M_count_threshold )
    {
        dlog.addText( Logger::ACTION,
                      __FILE__": already found" );

        return false;
    }

    const WorldModel & wm = agent->world();

    if ( M_target_player->isGhost() )
    {
        dlog.addText( Logger::ACTION,
                      __FILE__": target player is ghost." );
        agent->debugClient().addMessage( "FindPlayer:Ghost" );
        return Bhv_ScanField().execute( agent );
    }

    //
    // search the best angle
    //
    const Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    const Vector2D player_pos = M_target_player->pos() + M_target_player->vel();

    //
    // create candidate body target points
    //

    std::vector< Vector2D > body_points;
    body_points.reserve( 16 );

    const double max_x = ServerParam::i().pitchHalfLength() - 7.0;
    const double y_step = player_pos.absY() / 5.0;
    const double max_y = player_pos.absY() + y_step - 0.001;
    const double y_sign = sign( player_pos.y );

    // current body dir
    body_points.push_back( next_self_pos + Vector2D::polar2vector( 10.0, wm.self().body() ) );

    // on the static x line
    for ( double y = 0.0; y < max_y; y += y_step )
    {
        body_points.push_back( Vector2D( max_x, y * y_sign ) );
    }

    // on the static y line
    if ( next_self_pos.x < max_x )
    {
        for ( double x_rate = 0.9; x_rate >= 0.0; x_rate -= 0.1 )
        {
            double x = std::min( max_x,
                                 max_x * x_rate + next_self_pos.x * ( 1.0 - x_rate ) );
            body_points.push_back( Vector2D( x, player_pos.y ) );
        }
    }
    else
    {
        body_points.push_back( Vector2D( max_x, player_pos.y ) );
    }

    //
    // evaluate candidate points
    //

    const double next_view_half_width = agent->effector().queuedNextViewWidth().width() * 0.5;
    const double max_turn = wm.self().playerType().effectiveTurn( ServerParam::i().maxMoment(),
                                                                  wm.self().vel().r() );
    const AngleDeg player_angle = ( player_pos - next_self_pos ).th();
    const double neck_min = ServerParam::i().minNeckAngle() - next_view_half_width + 10.0;
    const double neck_max = ServerParam::i().maxNeckAngle() + next_view_half_width - 10.0;

    Vector2D best_point = Vector2D::INVALIDATED;

    const std::vector< Vector2D >::const_iterator p_end = body_points.end();
    for ( std::vector< Vector2D >::const_iterator p = body_points.begin();
          p != p_end;
          ++p )
    {
        AngleDeg target_body_angle = ( *p - next_self_pos ).th();
        double turn_moment_abs = ( target_body_angle - wm.self().body() ).abs();

        dlog.addText( Logger::ACTION,
                      "____ body_point=(%.1f %.1f) angle=%.1f moment=%.1f",
                      p->x, p->y,
                      target_body_angle.degree(),
                      turn_moment_abs );

        if ( turn_moment_abs > max_turn )
        {
            dlog.addText( Logger::ACTION,
                          "____ xxxx cannot turn by 1 step" );
            continue;
        }

        double angle_diff = ( player_angle - target_body_angle ).abs();
        if ( neck_min < angle_diff
             && angle_diff < neck_max )
        {
            best_point = *p;

            dlog.addText( Logger::ACTION,
                          "____ oooo can turn and look" );
            break;
        }

        dlog.addText( Logger::ACTION,
                      "____ xxxx cannot look" );
    }

    if ( ! best_point.isValid() )
    {
        // best_point.assign( max_x * 0.7 + wm.self().pos().x * 0.3,
        //                    wm.self().pos().y * 0.9 );
        best_point = player_pos;
        dlog.addText( Logger::ACTION,
                      __FILE__": assign player pos" );
    }


    dlog.addText( Logger::ACTION,
                  __FILE__": turn to (%.1f %.1f)",
                  best_point.x, best_point.y );
    Body_TurnToPoint( best_point ).execute( agent );
    agent->debugClient().addLine( next_self_pos, best_point );

    agent->setNeckAction( new Neck_TurnToPlayerOrScan( M_target_player, M_count_threshold ) );
    return true;
}
