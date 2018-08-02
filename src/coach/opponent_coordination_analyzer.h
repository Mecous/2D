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

#ifndef OPPONENT_COORDINATION_ANALYZER_H
#define OPPONENT_COORDINATION_ANALYZER_H

#include "abstract_coach_analyzer.h"

#include <vector>

class OpponentCoordinationAnalyzer
    : public AbstractCoachAnalyzer {
private:

    int M_coordination_matrix[11][12];


public:

    OpponentCoordinationAnalyzer();

    ~OpponentCoordinationAnalyzer()
      { }


    bool analyze( rcsc::CoachAgent * agent );

private:

    bool doSendFreeform( rcsc::CoachAgent * agent );

};

#endif
