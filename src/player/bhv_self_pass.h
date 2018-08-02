// -*-c++-*-

/*!
  \file bhv_self_pass.h
  \brief pass to self action.
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

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

#ifndef HELIOS_BHV_SELF_PASS_H
#define HELIOS_BHV_SELF_PASS_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

#include <vector>

namespace rcsc {
class WorldModel;
}

/*!
  \class Bhv_SelfPass
  \brief pass to self
 */
class Bhv_SelfPass
    : public rcsc::SoccerBehavior {
private:

public:

    bool execute( rcsc::PlayerAgent * agent );

private:

    bool doKickDashes( rcsc::PlayerAgent * agent );
    bool doKickTurnDashes( rcsc::PlayerAgent * agent,
                           const rcsc::Vector2D & target_point );

    void createSelfCache( rcsc::PlayerAgent * agent,
                          const int n_turn,
                          const int n_dash,
                          const rcsc::AngleDeg & accel_angle,
                          std::vector< rcsc::Vector2D > & self_state );
    bool checkOpponent( rcsc::PlayerAgent * agent,
                        const int n_turn,
                        const int n_dash,
                        const rcsc::Vector2D & receive_pos );
    bool canKick( const rcsc::WorldModel & wm,
                  const int n_turn,
                  const int n_dash,
                  const rcsc::Vector2D & receive_pos );
    bool doKick( rcsc::PlayerAgent * agent,
                 const int n_turn,
                 const int n_dash,
                 const rcsc::Vector2D & receive_pos );
};

#endif
