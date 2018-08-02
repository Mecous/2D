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

#ifndef BHV_SIDE_HALF_CROSS_MOVE_H
#define BHV_SIDE_HALF_CROSS_MOVE_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

namespace rcsc {
class WorldModel;
}

class Bhv_SideHalfCrossMove
    : public rcsc::SoccerBehavior {
private:

public:
    Bhv_SideHalfCrossMove()
      { }

    bool execute( rcsc::PlayerAgent * agent );

private:
    bool doIntercept( rcsc::PlayerAgent * agent );
    bool doGoToSpace( rcsc::PlayerAgent * agent,
                      const rcsc::Vector2D & base_target_point );
    void doNormalMove( rcsc::PlayerAgent * agent,
                       const rcsc::Vector2D & target_point );

    bool doCheckCrossPoint( rcsc::PlayerAgent * agent );

    rcsc::Vector2D getTargetPoint( rcsc::PlayerAgent * agent );
    rcsc::Vector2D getAvoidOffsidePosition( const rcsc::WorldModel & wm,
                                            const rcsc::Vector2D & old_target );
    double getDashPower( rcsc::PlayerAgent * agent,
                         const rcsc::Vector2D & target_point );
};

#endif
