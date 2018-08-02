// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa Akiyama

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

#ifndef BHV_SET_PLAY_AVOID_MARK_MOVE_H
#define BHV_SET_PLAY_AVOID_MARK_MOVE_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

// our set play move

class Bhv_SetPlayAvoidMarkMove
    : public rcsc::SoccerBehavior {
public:

    Bhv_SetPlayAvoidMarkMove()
      { }

    bool execute( rcsc::PlayerAgent * agent );

private:
    void doSay( rcsc::PlayerAgent * agent,
                const rcsc::Vector2D & target_point );

    bool isWaitPeriod( const rcsc::PlayerAgent * agent );
    bool checkRecoverSituation( const rcsc::PlayerAgent * agent );
    bool existOtherReceiver( const rcsc::PlayerAgent * agent );
    bool existOpponent( const rcsc::PlayerAgent * agent );
    rcsc::Vector2D getTargetPointVoronoi( const rcsc::PlayerAgent * agent );
    rcsc::Vector2D getTargetPointOnCircle( const rcsc::PlayerAgent * agent );

};

#endif
