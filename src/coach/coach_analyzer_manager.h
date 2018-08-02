// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA and Tomoharu NAKASHIMA

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

#ifndef COACH_ANALYZER_MANAGER_H
#define COACH_ANALYZER_MANAGER_H

#include <rcsc/game_time.h>
#include <rcsc/types.h>
#include <rcsc/coach/coach_world_model.h>
#include <utility>

class CoachAnalyzerManager {
public:
    enum RoleType {
        FORWARD = 1,
        MIDFIELDER = 0,
        OTHER = -1,
        UNKNOWN = -2,
    };

private:
    bool M_goalie_unum_changed;
    bool M_cornerkick_analyzed;
    int M_our_goalie_unum;
    int M_their_goalie_unum;
    int M_marked_count;
    int M_agent2d_count;

    RoleType M_opponent_formation[11];
    rcsc::GameTime M_opponent_formation_time;

    //! the uniform number of first and second mark target opponent for all teammates. Unum_Unknown means no mark target.
    int M_mark_assignment[11][2];

    std::string M_opponent_nickname;

    std::string M_opponent_cornerkick_formation;

    std::string M_opponent_data_dir;

private:

    // private for singleton
    CoachAnalyzerManager();

    // not used
    CoachAnalyzerManager( const CoachAnalyzerManager & );
    CoachAnalyzerManager & operator=( const CoachAnalyzerManager & );
public:

    static
    CoachAnalyzerManager & instance();

    static CoachAnalyzerManager & i()
      {
          return instance();
      }

    bool init( const std::string & opponent_data_dir );

    //
    // goalie unum
    //
    void setGoalieUnum( const int our_unum,
                        const int their_unum );
    void clearGoalieUnumChanged() { M_goalie_unum_changed = false; }
    bool goalieUnumChanged() const { return M_goalie_unum_changed; }
    int ourGoalieUnum() const { return M_our_goalie_unum; }
    int theirGoalieUnum() const { return M_their_goalie_unum; }

    //
    // opponent formation
    //
    void setOpponentFormationTime( const rcsc::GameTime & t )
      {
          M_opponent_formation_time = t;
      }

    const rcsc::GameTime & opponentFormationTime() const
      {
          return M_opponent_formation_time;
      }

    void setOpponentFormation( const int unum,
                               const RoleType type )
      {
          if ( 1 <= unum && unum <= 11 )
          {
              M_opponent_formation[unum-1] = type;
          }
      }

    RoleType opponentFormation( const int unum ) const
      {
          return ( 1 <= unum && unum <= 11
                   ? M_opponent_formation[unum-1]
                   : UNKNOWN );
      }

    //
    // mark
    //
    void setMarkAssignment( const int our_unum,
                            const int first_target_unum,
                            const int second_target_unum );
    std::pair< int, int > markTargets( const int our_unum )
      {
          if ( our_unum < 1 || 11 < our_unum )
          {
              return std::make_pair( rcsc::Unum_Unknown, rcsc::Unum_Unknown );
          }
          return std::make_pair( M_mark_assignment[our_unum - 1][0],
                                 M_mark_assignment[our_unum - 1][1] );
      }

    bool analyzeOpponentNickname( const rcsc::CoachWorldModel & wm );

    std::string opponentNickname() const
      {
          return M_opponent_nickname;
      }

    std::string opponentDataDir() const
      {
          return M_opponent_data_dir;
      }

    ///analyze cornerkick///
    void analyzeOpponentCornerKickFormation( const rcsc::CoachWorldModel & wm );
    void clearCornerKickAnalyzed() { M_cornerkick_analyzed = false; }
    bool cornerKickAnalyzed() const { return M_cornerkick_analyzed; }
    std::string opponentCornerKickFormation() const
      {
          return M_opponent_cornerkick_formation;
      }

};

#endif
