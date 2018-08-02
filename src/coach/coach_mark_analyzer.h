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

#ifndef COACH_MARK_ANALYZER_H
#define COACH_MARK_ANALYZER_H

#include "abstract_coach_analyzer.h"

#include <rcsc/types.h>

class CoachMarkAnalyzer
    : public AbstractCoachAnalyzer {
private:

    int M_nearest_matrix[11][11]; /*!< M_nearest_matrix[i][j] represents how many times
                                    teammate[i] is the nearest to opponent[j]
                                    plus how many times opponent[j] is the nearest to teammate[i]
                                   */

    int M_nearest_matrix_setplay[11][11]; /*!< M_nearest_matrix_setplay[i][j] represents how many times
                                    teammate[i] is the nearest to opponent[j] plus how many times
                                    opponent[j] is the nearest to teammate[i] in setplay modes
                                   */

    int M_nearest_sum[11]; /*! total number of nearest counts for agent[i] */
    int M_nearest_sum_setplay[11];
    int M_opponent_unum_to_mark[11];
    int M_second_opponent_unum_to_mark[11];
    int M_candidate_opponent_unum_to_mark[11];

    bool M_do_marking[11];
    bool M_is_defender[11];

public:

    CoachMarkAnalyzer();

    ~CoachMarkAnalyzer()
      { }


    bool analyze( rcsc::CoachAgent * agent );
    bool saveOpponentData();
    bool loadOpponentData();

private:

    bool doPlayOn( rcsc::CoachAgent * agent );
    bool doSetPlayTheirBall( rcsc::CoachAgent * agent );

    void updateMarkingFlag();

    void reportToManager();

    bool doSendFreeform( rcsc::CoachAgent * agent );

    /*!
      \brief return the side where the ball is kickable now
     */
    rcsc::SideID checkBallKickableSide( rcsc::CoachAgent * agent );

    /*!
      \brief check which player is nearest to which player
     */
    bool checkNearestPlayer( rcsc::CoachAgent * agent );

    /*!
      \brief check which player is nearest to which player in setplay modes
     */
    bool checkNearestPlayerSetPlay( rcsc::CoachAgent * agent );

    /*!
      \brief find an opponent to mark for each teammate.
     */
    bool findOpponentToMark( rcsc::CoachAgent * agent );

    /*!
      \brief find an opponent to mark for each defender teammate.
     */
    bool findOpponentToMarkForDefenders( rcsc::CoachAgent * agent );

    /*!
      \brief find an opponent to mark for each defender teammate.
     */
    bool findOpponentToMarkForTheOthers( rcsc::CoachAgent * agent );

    /*!
      \brief find the second opponent to mark for each teammate.
     */
    bool findSecondOpponentToMark();// rcsc::CoachAgent * agent );

    /*!
      \brief find an opponent to mark for each teammate for set play.
     */
    bool findOpponentToMarkSetPlay( rcsc::CoachAgent * agent );

    /*!
      \brief find an opponent to mark for each teammate for set play.
     */
    bool findOpponentToMarkSetPlayForDefenders( rcsc::CoachAgent * agent );

    /*!
      \brief find an opponent to mark for each teammate for set play.
     */
    bool findOpponentToMarkSetPlayForTheOthers( rcsc::CoachAgent * agent );

    /*!
      \brief find the second opponent to mark for each teammate for set play.
     */
    bool findSecondOpponentToMarkSetPlay();// rcsc::CoachAgent * agent );

    /*!
      \brief
     */
    bool acceptCandidateTargets( rcsc::CoachAgent * agent );

    /*!
      \brief return the number of different targets between the candidate and last sent.
     */
    int checkDifference( rcsc::CoachAgent * agent );

    /*!
      /brief check if the current situation is to mark or not
     */
    bool isMarkingSituation( rcsc::CoachAgent * agent );

};

#endif
