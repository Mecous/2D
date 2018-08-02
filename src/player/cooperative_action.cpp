// -*-c++-*-

/*!
  \file cooperative_action.cpp
  \brief cooperative action type Source File.
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 3 of the License, or (at your option) any later version.

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

#include "cooperative_action.h"

using namespace rcsc;

const double CooperativeAction::ERROR_ANGLE = -360.0;

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::CooperativeAction( const Type type,
                                      const int player_unum,
                                      const Vector2D & target_ball_pos,
                                      const int duration_time,
                                      const char * description )
    : M_type( type ),
      M_index( -1 ),
      M_mode( 0 ),
      M_player_unum( player_unum ),
      M_target_player_unum( Unum_Unknown ),
      M_target_ball_pos( target_ball_pos ),
      M_target_player_pos( Vector2D::INVALIDATED ),
      M_target_body_angle( ERROR_ANGLE ),
      M_first_ball_vel( Vector2D::INVALIDATED ),
      M_first_turn_moment( 0.0 ),
      M_first_dash_power( 0.0 ),
      M_first_dash_dir( 0.0 ),
      M_duration_time( duration_time ),
      M_kick_count( 0 ),
      M_turn_count( 0 ),
      M_dash_count( 0 ),
      M_safety_level( Safe ),
      M_description( description )
{

}
