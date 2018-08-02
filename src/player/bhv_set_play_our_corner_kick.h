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

#ifndef BHV_SET_PLAY_OUR_CORNER_KICK_H
#define BHV_SET_PLAY_OUR_CORNER_KICK_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/player/soccer_action.h>

class Bhv_SetPlayOurCornerKick
    : public rcsc::SoccerBehavior {
public:

    Bhv_SetPlayOurCornerKick()
      { }

    bool execute( rcsc::PlayerAgent * agent );

private:

    void doKicker( rcsc::PlayerAgent * agent );
    bool doKickerWait( rcsc::PlayerAgent * agent );
    bool doKickerTurnToBall( rcsc::PlayerAgent * agent );

    void setTurnNeckToReceiver( rcsc::PlayerAgent * agent );

    bool doKickerNormalPass( rcsc::PlayerAgent * agent );
    bool doKickerNearestPass( rcsc::PlayerAgent * agent );
    void doKickerFinal( rcsc::PlayerAgent * agent );

    void doMove( rcsc::PlayerAgent * agent );

    void doDefaultMove( rcsc::PlayerAgent * agent );
    bool do1stMove( rcsc::PlayerAgent * agent );
    bool do2ndMove( rcsc::PlayerAgent * agent,
                    const rcsc::Vector2D & target_point );

    bool is2ndMovePeriod( const rcsc::PlayerAgent * agent,
                          const rcsc::Vector2D & target_point );

    rcsc::Vector2D get1stMovePoint( const rcsc::PlayerAgent * agent );
    rcsc::Vector2D get2ndMovePoint( const rcsc::PlayerAgent * agent );

};

#endif
