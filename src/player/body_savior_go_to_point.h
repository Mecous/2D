// -*-c++-*-

/*!
  \file body_savior_go_to_point.h
  \brief run to specified point
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

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

#ifndef BODY_SAVIOR_GO_TO_POINT_H
#define BODY_SAVIOR_GO_TO_POINT_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

namespace rcsc {
class PlayerAgent;
}

/*!
  \class Body_SaviorGoToPoint
*/
class Body_SaviorGoToPoint
    : public rcsc::BodyAction {

private:
    const rcsc::Vector2D M_target_point;
    const double M_max_error;
    const double M_max_power;
    const bool M_use_back_dash;
    const bool M_force_back_dash;
    const bool M_emergency_mode;
    const bool M_look_ball;

    static rcsc::GameTime S_cache_time;
    static rcsc::Vector2D S_cache_target_point;
    static double S_cache_max_error;
    static double S_cache_max_power;
    static bool S_cache_use_back_dash;
    static bool S_cache_force_back_dash;
    static bool S_cache_emergency_mode;
    static int S_turn_count;


public:
    /*!
      \brief constructor
     */
    Body_SaviorGoToPoint( const rcsc::Vector2D & target_point,
                          const double max_error = 1.0,
                          const double max_power = 100.0,
                          const bool use_back_dash = true,
                          const bool force_back_dash = false,
                          const bool emergency_mode = false,
                          const bool look_ball = false );

    /*!
      \brief do go to ball
      \param agent agent pointer to the agent itself
      \return true with action, false if already reached to target point
     */
    bool execute( rcsc::PlayerAgent * agent );

private:

    void doAdjustDash( rcsc::PlayerAgent * agent );

    bool doMoveLookBall( rcsc::PlayerAgent * agent );

    bool tryOmniDash( rcsc::PlayerAgent * agent,
                      const bool back_dash );

};

#endif
