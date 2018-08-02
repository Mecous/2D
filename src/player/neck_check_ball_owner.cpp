// -*-c++-*-

/*!
  \file neck_check_ball_owner.cpp
  \brief change_view and turn_neck to look the ball owner.
*/

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

#include "neck_check_ball_owner.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/view_synch.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Neck_CheckBallOwner::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__ ": Neck_CheckBallOwner" );
    agent->debugClient().addMessage( "CheckBallOwner" );

    const WorldModel & wm = agent->world();

    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min =  wm.interceptTable()->opponentReachCycle();
    int min_step = std::min( mate_min, opp_min );

    const PlayerObject * fastest = ( mate_min < opp_min
                                     ? wm.interceptTable()->fastestTeammate()
                                     : wm.interceptTable()->fastestOpponent() );

    if ( ! fastest
         || min_step > 3
         || fastest->distFromBall() > 4.0 )
    {
        if ( fastest
             && wm.ball().posCount() < 3
             && fastest->seenPosCount() > 0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__ ": player %d (%.1f %.1f) or scan",
                          fastest->unum(),
                          fastest->pos().x, fastest->pos().y );
            Neck_TurnToPlayerOrScan( fastest, 0 ).execute( agent );
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__ ": ball or scan" );
            Neck_TurnToBallOrScan( -1 ).execute( agent );
        }
    }
    else
    {
        //
        // if necessary, execute change_view
        //

        doChangeView( agent );

        int count_thr = -1;
        if ( opp_min >= 2
             || fastest->distFromSelf() > 10.0 )
        {
            count_thr = 0;
        }

        Neck_TurnToBallAndPlayer( fastest, count_thr ).execute( agent );
    }
    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
void
Neck_CheckBallOwner::doChangeView( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D goal_pos( - ServerParam::i().pitchHalfLength(), 0.0 );
    const Vector2D ball_pos = wm.ball().inertiaPoint( 1 );

    if ( ball_pos.dist2( goal_pos ) < std::pow( 20.0, 2 ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": ball is in shootable area. force synch view" );
        View_Synch().execute( agent );
        return;
    }

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min =  wm.interceptTable()->opponentReachCycle();

    const PlayerObject * fastest = ( mate_min < opp_min
                                     ? wm.interceptTable()->fastestTeammate()
                                     : wm.interceptTable()->fastestOpponent() );
    if ( ! fastest )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doChangeView) no fastest player." );
        return;
    }

    const Vector2D my_pos = agent->effector().queuedNextMyPos();
    const AngleDeg my_body = agent->effector().queuedNextMyBody();

    const Vector2D player_pos = fastest->pos() + fastest->vel();
    const AngleDeg ball_angle = ( ball_pos - my_pos ).th();
    const AngleDeg player_angle = ( player_pos - my_pos ).th();

    const AngleDeg ball_rel_angle = ball_angle - my_body;
    const AngleDeg player_rel_angle = player_angle - my_body;

    dlog.addText( Logger::TEAM,
                  __FILE__": ball=(%.1f %.1f)%.0f"
                  " player=(%.1f %.1f)%.0f my_body=%.0f",
                  ball_pos.x, ball_pos.y, ball_angle.degree(),
                  player_pos.x, player_pos.y, player_angle.degree(),
                  my_body.degree() );

    ViewWidth view_width = ViewWidth::ILLEGAL;

    if ( my_pos.dist( ball_pos ) < ServerParam::i().visibleDistance() - 1.0
         && my_pos.dist( player_pos ) < ServerParam::i().visibleDistance() - 1.0 )
    {
        view_width = ViewWidth::NARROW;
    }

    //
    // check the possibility of simultaneous looking
    //
    if ( view_width == ViewWidth::ILLEGAL )
    {
        for ( ViewWidth w = ViewWidth::NARROW;
              w != ViewWidth::ILLEGAL;
              ++w )
        {
            double half_width = ViewWidth::width( w ) * 0.5;
            if ( ball_rel_angle.abs() < ServerParam::i().maxNeckAngle() + half_width - 2.0
                 && player_rel_angle.abs() < ServerParam::i().maxNeckAngle() + half_width - 2.0 )
            {
                view_width = w;
                dlog.addText( Logger::TEAM,
                              __FILE__": change view width(1) to %d",
                              w.type() );
                break;
            }
        }
    }

    //
    // check the possibility of the ball looking
    //
    if ( view_width == ViewWidth::ILLEGAL )
    {
        for ( ViewWidth w = ViewWidth::NARROW;
              w != ViewWidth::ILLEGAL;
              ++w )
        {
            double half_width = ViewWidth::width( w ) * 0.5;
            if ( ball_rel_angle.abs() < ServerParam::i().maxNeckAngle() + half_width - 2.0 )
            {
                view_width = w;
                dlog.addText( Logger::TEAM,
                              __FILE__": change view width(2) to %d",
                              w.type() );
                break;
            }
        }
    }

    if ( view_width == ViewWidth::ILLEGAL )
    {
        view_width = ViewWidth::WIDE;
    }

    if ( view_width.type() == ViewWidth::NARROW )
    {
        View_Synch().execute( agent );
    }
    else
    {
        agent->doChangeView( view_width );
    }
}
