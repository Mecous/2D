// -*-c++-*-

/*!
  \file predict_player_object.cpp
  \brief predicted player class Source File
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

#include "predict_player_object.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
PredictPlayerObject::PredictPlayerObject( const rcsc::SideID side,
                                          const int unum,
                                          const int player_type,
                                          const bool is_self,
                                          const rcsc::Vector2D & pos,
                                          const rcsc::AngleDeg body_dir,
                                          const bool goalie )
    : AbstractPlayerObject( -1 ),
      M_valid( true ),
      M_is_self( is_self ),
      M_ghost_count( 0 )
{
    M_side = side;
    M_unum = unum;
    M_unum_count = 0;
    M_goalie = goalie;

    M_player_type = PlayerTypeSet::i().get( player_type );
    if ( ! M_player_type )
    {
        M_player_type = PlayerTypeSet::i().get( Hetero_Unknown );
    }

    M_pos = pos;
    M_pos_count = 0;

    M_seen_pos = pos;
    M_seen_pos_count = 0;

    M_heard_pos = pos;
    M_heard_pos_count = 0;

    M_vel = Vector2D( 0.0, 0.0 );
    M_vel_count = 0;

    M_seen_vel = M_vel;
    M_seen_vel_count = 0;

    M_body = body_dir;
    M_body_count = 0;

    M_face = 0.0;
    M_face_count = 0;

    M_dist_from_ball = 0.0;
}

/*-------------------------------------------------------------------*/
/*!

 */
PredictPlayerObject::Ptr
PredictPlayerObject::clone() const
{
    PredictPlayerObject::Ptr ptr ( new PredictPlayerObject( *this ) );
    return ptr;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PredictPlayerObject::copyPlayerInfo( const AbstractPlayerObject & pl )
{
    M_valid = true;
    M_is_self = pl.isSelf();
    M_ghost_count = pl.ghostCount();

    M_side = pl.side();
    M_unum = pl.unum();
    M_unum_count = pl.unumCount();
    M_goalie = pl.goalie();

    M_player_type = pl.playerTypePtr();

    M_pos = pl.pos();
    M_pos_count = pl.posCount();

    M_seen_pos = pl.seenPos();
    M_seen_pos_count = pl.seenPosCount();

    M_heard_pos = pl.heardPos();
    M_heard_pos_count = pl.heardPosCount();

    M_vel = pl.vel();
    M_vel_count = pl.velCount();

    M_seen_vel = pl.seenVel();
    M_seen_vel_count = pl.seenVelCount();

    M_body = pl.body();
    M_body_count = pl.bodyCount();

    M_face = pl.face();
    M_face_count = pl.faceCount();

    M_dist_from_ball = pl.distFromBall();
}
