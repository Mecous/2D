// -*-c++-*-

/*!
  \file bhv_hold_ball.h
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

#ifndef BHV_HOLD_BALL_H
#define BHV_HOLD_BALL_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

#include <utility>

namespace rcsc {
class PlayerObject;
class WorldModel;
}

class ActionChainGraph;


class Bhv_HoldBall
    : public rcsc::SoccerBehavior {
private:

    const ActionChainGraph & M_chain_graph;

public:

    Bhv_HoldBall();

    bool execute( rcsc::PlayerAgent * agent );

private:

    bool doTurnTo( rcsc::PlayerAgent * agent,
                   const int target_unum,
                   const rcsc::Vector2D & target_point );
    bool doKeepBall( rcsc::PlayerAgent * agent,
                     const int target_unum,
                     const rcsc::Vector2D & target_point );

    std::pair< int, rcsc::Vector2D > getChainTarget( const rcsc::WorldModel & wm );

public:

    static
    rcsc::Vector2D get_keep_ball_vel( const rcsc::WorldModel & wm );

    static
    int predict_opponents_reach_step( const rcsc::WorldModel & wm,
                                      const int ball_step,
                                      const rcsc::Vector2D & ball_pos );


    static
    int predict_opponents_reach_step( const rcsc::WorldModel & wm,
                                      const rcsc::Vector2D & ball_next1,
                                      const rcsc::Vector2D & ball_next2 );

    static
    int predict_opponent_reach_step( const rcsc::PlayerObject * opponent,
                                     const int ball_step,
                                     const rcsc::Vector2D & ball_pos );

};

#endif
