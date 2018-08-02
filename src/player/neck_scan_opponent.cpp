// -*-c++-*-

/*!
  \file neck_scan_opponent.cpp
  \brief turn_neck to find low accuracy opponent player
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

#include "neck_scan_opponent.h"

#include "neck_check_ball_owner.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

using namespace rcsc;


/*-------------------------------------------------------------------*/
/*!

 */
Neck_ScanOpponent::Neck_ScanOpponent( boost::shared_ptr< Region2D > region,
                                      NeckAction::Ptr default_neck )
    : M_region( region ),
      M_default_action( default_neck )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Neck_ScanOpponent::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const PlayerObject * opponent = wm.getOpponentNearestToBall( 5 );
    const double opp_dist = ( opponent
                              ? opponent->distFromBall()
                              : 65535.0 );

    if ( opp_min <= 1
         || opp_dist < 2.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (execute) opponent ball owner." );
        agent->debugClient().addMessage( "NeckScanOpp:Default" );
        if ( M_default_action )
        {
            M_default_action->execute( agent );
        }
        else
        {
            Neck_CheckBallOwner().execute( agent );
        }
        return false;
    }

    const Vector2D next_self_pos = agent->effector().queuedNextSelfPos();
    const AngleDeg next_self_body = agent->effector().queuedNextSelfBody();
    const double view_half_width = agent->effector().queuedNextViewWidth().width() - 5.0;
    const double min_neck = ServerParam::i().minNeckAngle() - view_half_width;
    const double max_neck = ServerParam::i().maxNeckAngle() - view_half_width;


    const PlayerObject * target_opponent = static_cast< const PlayerObject * >( 0 );
    int max_pos_count = 0;

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        if ( (*o)->ghostCount() % 3 != 1 ) continue; // Magic Number
        if ( (*o)->posCount() < max_pos_count ) continue;

        if ( M_region
             && ! M_region->contains( (*o)->pos() ) )
        {
            continue;
        }

        AngleDeg target_neck_angle = ( (*o)->pos() - next_self_pos ).th() - next_self_body;

        if ( min_neck < target_neck_angle.degree()
             && target_neck_angle.degree() < max_neck )
        {
            target_opponent = *o;
            max_pos_count = (*o)->posCount();
        }
    }

    if ( ! target_opponent
         || target_opponent->ghostCount() >= 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (execute) no target opponent. scan field." );
        agent->debugClient().addMessage( "NeckScanOpp:Scan" );
        Neck_ScanField().execute( agent );
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (execute) try to find target opponent %d (%.2f %.2f).",
                  target_opponent->unum(),
                  target_opponent->pos().x, target_opponent->pos().y );
    agent->debugClient().addMessage( "NeckScanOpp" );
    Neck_TurnToPlayerOrScan( target_opponent, 0 ).execute( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
NeckAction *
Neck_ScanOpponent::clone() const
{
    return new Neck_ScanOpponent( M_region, M_default_action );
}
