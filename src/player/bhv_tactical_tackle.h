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

#ifndef BHV_TACTICAL_TACKLE_H
#define BHV_TACTICAL_TACKLE_H

#include <rcsc/player/soccer_action.h>

namespace rcsc {
class PlayerObject;
}

class Bhv_TacticalTackle
    : public rcsc::SoccerBehavior {
private:

    double M_success_probability;
    bool M_tackle_situation;
    bool M_use_foul;
    bool M_opponent_ball;
    bool M_ball_moving_to_our_goal;
    bool M_tackle_shoot;
    const rcsc::PlayerObject * M_blocker;


public:
    Bhv_TacticalTackle()
        : M_success_probability( 0.0 ),
          M_tackle_situation( false ),
          M_use_foul( false ),
          M_opponent_ball( false ),
          M_ball_moving_to_our_goal( false ),
          M_blocker( static_cast< const rcsc::PlayerObject * >( 0 ) )
      { }

    bool execute( rcsc::PlayerAgent * agent );

private:

    void update( const rcsc::PlayerAgent * agent );
    void updateBlocker( const rcsc::PlayerAgent * agent );

    double getMinProbability( const rcsc::PlayerAgent * agent );
    bool doTackle( rcsc::PlayerAgent * agent );

    bool doPrepareTackle( rcsc::PlayerAgent * agent );

};

#endif
