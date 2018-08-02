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

#ifndef COACH_PATH_PLANNER_H
#define COACH_PATH_PLANNER_H

#include "abstract_coach_analyzer.h"
#include "coach_analyzer_manager.h"

#include <vector>

class CoachPathPlanner
    : public AbstractCoachAnalyzer {
public:

private:

    std::vector< std::vector< int > > M_path;

public:

    CoachPathPlanner();

    ~CoachPathPlanner()
      { }


    bool analyze( rcsc::CoachAgent * agent );

    // bool saveOpponentData();
    // bool loadOpponentData();

private:

    /*!
     */
    bool doPlayOn( rcsc::CoachAgent * agent );

    /*!
     */
    bool doSetPlayTheirBall( rcsc::CoachAgent * agent );

    /*!
     */
    bool doSetPlayOurBall( rcsc::CoachAgent * agent );

    /*!
     */
    bool appendNextLevelMates( rcsc::CoachAgent * agent );
};

#endif
