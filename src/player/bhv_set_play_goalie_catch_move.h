// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Yosuke Narimoto

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

#ifndef BHV_SET_PLAY_GOALIE_CATCH_MOVE_H
#define BHV_SET_PLAY_GOALIE_CATCH_MOVE_H

#include <rcsc/game_time.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/player/soccer_action.h>

// our goalie catch move

class Bhv_SetPlayGoalieCatchMove
    : public rcsc::SoccerBehavior {
public:

    Bhv_SetPlayGoalieCatchMove()
      { }

    bool execute( rcsc::PlayerAgent * agent );

private:

    void doMove( rcsc::PlayerAgent * agent );
    void doFirstMove( rcsc::PlayerAgent * agent );
    void doSecondMove( rcsc::PlayerAgent * agent );
    rcsc::Vector2D getFirstMoveTarget( const rcsc::PlayerAgent * agent );
    rcsc::Vector2D getSecondMoveTarget( const rcsc::PlayerAgent * agent );
    int getMoveStartCount();

    void doMoveOld( rcsc::PlayerAgent * agent );

};

#endif
