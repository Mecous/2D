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

#ifndef BHV_DEFENDER_MARK_MOVE_H
#define BHV_DEFENDER_MARK_MOVE_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/player/soccer_action.h>

namespace rcsc {
class AbstractPlayerObject;
}

// added 2009-06-22

class Bhv_DefenderMarkMove
    : public rcsc::SoccerBehavior {
public:

    bool execute( rcsc::PlayerAgent * agent );

private:

    bool isValidMarkTarget( const rcsc::PlayerAgent * agent );

    // add 2016-05-29
    bool doMoveFindPlayer( rcsc::PlayerAgent * agent,
                           const rcsc::Vector2D & move_target );

    rcsc::Vector2D getTargetPointOld( const rcsc::PlayerAgent * agent );
    rcsc::Vector2D getTargetPoint2016( const rcsc::PlayerAgent * agent );

};

#endif
