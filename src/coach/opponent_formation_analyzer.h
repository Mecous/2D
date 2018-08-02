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

#ifndef OPPONENT_FORMATION_ANALYZER_H
#define OPPONENT_FORMATION_ANALYZER_H

#include "abstract_coach_analyzer.h"
#include "coach_analyzer_manager.h"

#include <vector>

class OpponentFormationAnalyzer
    : public AbstractCoachAnalyzer {
public:

private:

    long M_cycle_last_sent;
    int M_count_modified;

    int M_count_forward[11];
    int M_count_midfielder[11];
    int M_count_defender[11];
    CoachAnalyzerManager::RoleType M_opponent_formation[11];
    bool M_formation_modified[11];

public:

    OpponentFormationAnalyzer();

    ~OpponentFormationAnalyzer()
      { }


    bool analyze( rcsc::CoachAgent * agent );

    bool saveOpponentData();
    bool loadOpponentData();

private:

    bool doPlayOn( rcsc::CoachAgent * agent );

    bool doSendFreeform( rcsc::CoachAgent * agent );


    /*!
      \brief return the uniform number list of opponent forward players
     */
    void findForwardOpponents( rcsc::CoachAgent * agent,
                               std::vector< int > & unum_list );

    /*!
      \brief return the uniform number list of opponent midfielders
     */
    void findMidfielderOpponents( rcsc::CoachAgent * agent,
                                  const std::vector< int > & unum_fw,
                                  std::vector< int > & unum_list );

    /*!
      \brief return the uniform number list of offensive opponent players
     */
    void findOffensiveOpponents( rcsc::CoachAgent * agent,
                                 std::vector< int > & unum_list );

    /*!
      \brief
     */
    bool doReportToAnalyzerManager( rcsc::CoachAgent * agent );

};

#endif
