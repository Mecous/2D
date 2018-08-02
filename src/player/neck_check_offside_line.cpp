// -*-c++-*-

/*!
  \file neck_check_offside_line.cpp
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

#include "neck_check_offside_line.h"

#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/view_synch.h>
#include <rcsc/action/view_normal.h>
#include <rcsc/action/view_wide.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

#include <rcsc/math_util.h>

// #define DEBUG_PRINT

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Neck_CheckOffsideLine::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__ ": Neck_CheckOffsideLine count=%d",
                  agent->world().offsideLineCount() );

    const double face_angle = get_best_angle( agent );
    if ( face_angle == -360.0 )
    {
        //Neck_ScanField().execute( agent );
        //return true;
        return false;
    }

    const AngleDeg self_body = agent->effector().queuedNextSelfBody();
    const AngleDeg neck_moment = AngleDeg( face_angle ) - self_body - agent->world().self().neck();

    dlog.addText( Logger::TEAM,
                  __FILE__ ":(execute) neck_moment=%.1f",
                  neck_moment.degree() );
    agent->debugClient().addMessage( "NeckCheckOffside%.0f", face_angle );

    agent->doTurnNeck( neck_moment );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Neck_CheckOffsideLine::get_best_angle( const PlayerAgent * agent )
{
    static GameTime s_update_time( -1, 0 );
    static double s_cached_result = -360.0;

    const WorldModel & wm = agent->world();

    if ( s_update_time == wm.time() )
    {
        return s_cached_result;
    }

    const Vector2D self_pos = agent->effector().queuedNextSelfPos();
    const AngleDeg self_body = agent->effector().queuedNextSelfBody();
    const double view_width = agent->effector().queuedNextViewWidth().width();

    const double min_neck = ServerParam::i().minNeckAngle();
    const double max_neck = ServerParam::i().maxNeckAngle();
    const int neck_divs = 36;
    const double neck_step = ( max_neck - min_neck ) / neck_divs;

    const double factor = 1.0 / ( 2.0 * std::pow( 5.0, 2 ) );

    double best_value = 0.0;
    double best_angle = -360.0;

    for ( int i = 0; i <= neck_divs; ++i )
    {
        const double neck = min_neck + neck_step * i;
        const AngleDeg face = self_body + neck;
        const AngleDeg left = face - ( view_width*0.5 - 5.0 );
        const AngleDeg right = face + ( view_width*0.5 - 5.0 );
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      "====================================" );
        dlog.addText( Logger::TEAM,
                      "%d: face=%.0f(body=%.0f neck=%.0f) left=%.1f right=%.1f",
                      i, face.degree(), self_body.degree(), neck, left.degree(), right.degree() );
#endif
        double value = 0.0;
        for ( PlayerObject::Cont::const_iterator o = wm.opponents().begin(),
                  end = wm.opponents().end();
              o != end;
              ++o )
        {
            if ( (*o)->ghostCount() >= 2 ) continue;

            Vector2D pos = (*o)->pos() + (*o)->vel();
            AngleDeg angle = ( pos - self_pos ).th();

            if ( ! angle.isRightOf( left )
                 || ! angle.isLeftOf( right ) )
            {
                continue;
            }

            double v = std::exp( -std::pow( wm.theirDefensePlayerLineX() - (*o)->pos().x, 2 ) * factor )
                * ( (*o)->posCount() + 1 );
            value += v;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          "%d: opponent[%d] value=%f(%f)", i, (*o)->unum(), v, value );
#endif
        }

        if ( value > best_value )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          "<<<< %d: update %f", i, value );
#endif
            best_value = value;
            best_angle = face.degree();
        }
    }

    s_update_time = wm.time();
    s_cached_result = best_angle;

    dlog.addText( Logger::TEAM,
                  __FILE__":(get_best_angle) %.1f", best_angle );
    return s_cached_result;
}
