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

#ifndef BHV_MID_FIELDER_FREE_MOVE_H
#define BHV_MID_FIELDER_FREE_MOVE_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

namespace rcsc {
class WorldModel;
}

class Bhv_MidFielderFreeMove
    : public rcsc::SoccerBehavior {
public:

    Bhv_MidFielderFreeMove()
      { }

    bool execute( rcsc::PlayerAgent * agent );

private:

    rcsc::Vector2D getTargetPoint( const rcsc::WorldModel & wm );

    bool isMarked( const rcsc::WorldModel & wm );

    double getDashPower( const rcsc::WorldModel & wm,
                         const rcsc::Vector2D & target_point );

    void doGoToPoint( rcsc::PlayerAgent * agent,
                      const rcsc::Vector2D & target_point,
                      const double dash_power );

    double evaluatePoint( const int count,
                          const rcsc::WorldModel & wm,
                          const rcsc::Vector2D & home_pos,
                          const rcsc::Vector2D & ball_pos,
                          const rcsc::Vector2D & move_pos );
};

#endif
