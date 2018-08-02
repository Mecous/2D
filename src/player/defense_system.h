// -*-c++-*-

/*!
  \file defense_system.h
  \brief team defense system manager Header File
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

#ifndef DEFENSE_SYSTEM_H
#define DEFENSE_SYSTEM_H

namespace rcsc {
class AbstractPlayerObject;
class PlayerAgent;
class Vector2D;
class WorldModel;
}

class DefenseSystem {
private:

public:

    static
    double get_defender_dash_power( const rcsc::WorldModel & wm,
                                    const rcsc::Vector2D & home_pos );

    static
    rcsc::Vector2D get_block_opponent_trap_point( const rcsc::WorldModel & wm );

    static
    rcsc::Vector2D get_block_center_point( const rcsc::WorldModel & wm );

    //
    // mark behaviors
    //
    static
    bool mark_go_to_point( rcsc::PlayerAgent * agent,
                           const rcsc::AbstractPlayerObject * mark_target,
                           const rcsc::Vector2D & target_point,
                           const double dist_thr,
                           const double dash_power,
                           const double dir_thr );
    static
    void mark_turn_to( rcsc::PlayerAgent * agent,
                       const rcsc::AbstractPlayerObject * mark_target );
    static
    void mark_turn_neck( rcsc::PlayerAgent * agent,
                         const rcsc::AbstractPlayerObject * mark_target );

    //
    //
    //

    static
    bool is_midfielder_block_situation( const rcsc::WorldModel & wm );

};

#endif
