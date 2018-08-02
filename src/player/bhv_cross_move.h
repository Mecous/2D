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

#ifndef BHV_CROSS_MOVE_H
#define BHV_CROSS_MOVE_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

namespace rcsc {
class WorldModel;
}

class Bhv_CrossMove
    : public rcsc::SoccerBehavior {
private:

public:
    Bhv_CrossMove()
      { }

    bool execute( rcsc::PlayerAgent * agent );

private:
    bool doIntercept( rcsc::PlayerAgent * agent );
    bool doFastMove( rcsc::PlayerAgent * agent );
    bool doGoToSpace( rcsc::PlayerAgent * agent );
    bool doGetBall( rcsc::PlayerAgent * agent );
    void doNormalMove( rcsc::PlayerAgent * agent );

    double getDashPower( rcsc::PlayerAgent * agent,
                         const rcsc::Vector2D & target_point );

    rcsc::Vector2D getTargetPoint( rcsc::PlayerAgent * agent );
    double evaluateTargetPoint( const int count,
                                const rcsc::WorldModel & wm,
                                const rcsc::Vector2D & home_pos,
                                const rcsc::Vector2D & ball_pos,
                                const rcsc::Vector2D & move_pos );

public:
    static
    int opponents_in_cross_cone( const rcsc::WorldModel & wm,
                                 const rcsc::Vector2D & ball_pos,
                                 const rcsc::Vector2D & move_pos );
};

#endif
