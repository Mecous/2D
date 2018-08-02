// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Tomoharu NAKASHIMA

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

#ifndef OPPONENT_WALL_DETECTOR_H
#define OPPONENT_WALL_DETECTOR_H

#include "abstract_coach_analyzer.h"
#include "coach_analyzer_manager.h"

#include <rcsc/player/player_object.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/stamina_model.h>
#include <rcsc/coach/coach_agent.h>


#include <vector>

class OpponentWallDetector
    : public AbstractCoachAnalyzer {

public:

private:

    int M_count_over_x;
    int M_count_play_on;

    long M_cycle;
    long M_stopped;

    double M_var_offside_integrate;
    double M_ball_x_integrate;
    double M_var_x_integrate;

    double M_count_front_opponent;

    std::string M_pre_defense_type;

public:

    OpponentWallDetector();

    ~OpponentWallDetector()
      { }

    bool analyze( rcsc::CoachAgent * agent );

private:

    bool doPlayOn( rcsc::CoachAgent * agent );

    bool resetParameters( rcsc::CoachAgent * agent );

    bool updateOpponentCoordinate( rcsc::CoachAgent * agent );

    bool calcBallX( rcsc::CoachAgent * agent );
    bool calcVarX( rcsc::CoachAgent * agent );
    bool calcVarOffside( rcsc::CoachAgent * agent );
    bool countFrontOpponent( rcsc::CoachAgent * agent );


    std::string decideWall();


    bool doSendFreeform( rcsc::CoachAgent * agent );

};

#endif
