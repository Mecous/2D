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

#ifndef OPPONENT_DEFENSE_MARK_OR_ZONE_DECIDER_H
#define OPPONENT_DEFENSE_MARK_OR_ZONE_DECIDER_H

#include "abstract_coach_analyzer.h"

#include <rcsc/geom/vector_2d.h>

#include <vector>

class OpponentDefenseMarkOrZoneDecider
    : public AbstractCoachAnalyzer {
private:

    /*!
      /brief M_nearest_matrix[i][j] represents the frequency
      that Teammate i is marked by Opponent j
     */
    int M_opponent_marking_matrix[11][11];

    std::vector< std::vector< rcsc::Vector2D > > M_stay_position;

    /*! M_candidate_marking_opponent[j] shows the unum of our player that are marked by Opponent j
     */
    int M_candidate_unum_marking[11];


public:

    OpponentDefenseMarkOrZoneDecider();

    ~OpponentDefenseMarkOrZoneDecider()
      { }


    bool analyze( rcsc::CoachAgent * agent );

private:

    /*!
      \brief
    */
    bool doPlayOn( rcsc::CoachAgent * agent );

    /*!
      \brief
    */
    bool doPlayOnOurBall( rcsc::CoachAgent * agent );

    /*!
      \brief
    */
    bool doPlayOnTheirBall( rcsc::CoachAgent * agent );

    /*!
      \brief
    */
    bool checkMarkingByDistance( rcsc::CoachAgent * agent );

    /*!
      \brief
    */
    bool checkZoning( rcsc::CoachAgent * agent );

    /*!
      \brief
    */
    bool doSendFreeform( rcsc::CoachAgent * agent );

    /*!
      \brief
    */
    bool drawCandidateMarking( rcsc::CoachAgent * agent );
};

#endif
