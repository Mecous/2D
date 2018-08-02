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

#ifndef INTENAION_SETPLAY_MOVe_H
#define INTENAION_SETPLAY_MOVe_H

#include <rcsc/game_time.h>
#include <rcsc/geom/vector_2d.h>

#include <rcsc/player/soccer_intention.h>

class IntentionSetplayMove
    : public rcsc::SoccerIntention {
private:
    const rcsc::Vector2D M_target_point; // global coordinate
    int M_step;

    rcsc::GameTime M_last_time;

public:
    IntentionSetplayMove( const rcsc::Vector2D & target_point,
                          const int max_step,
                          const rcsc::GameTime & start_time );

    bool finished( const rcsc::PlayerAgent * agent );

    bool execute( rcsc::PlayerAgent * agent );

};

#endif
