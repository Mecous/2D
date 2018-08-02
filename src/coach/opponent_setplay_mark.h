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

//#ifdef OPPONENT_SETPLAY_MARK_H
#define OPPONENT_SETPLAY_MARK_H

#include "abstract_coach_analyzer.h"
#include "coach_analyzer_manager.h"

#include <rcsc/player/player_object.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/stamina_model.h>
#include <rcsc/coach/coach_agent.h>


#include <vector>


//#define MAX_V 3000

class OpponentSetplayMark
    : public AbstractCoachAnalyzer {
private:

    enum OpponentType {
        Type_Gliders,
        Type_Oxsy,
        Type_WrightEagle,
        Type_Normal,
    };

    struct edge {
        int to, cap, cost, rev_index;

        edge()
            : to( 0 ),
              cap( 0 ),
              cost( 0 ),
              rev_index( 0 )
          { }

        edge( const int t,
              const int ca,
              const int co,
              const int rev )
            : to( t ),
              cap( ca ),
              cost( co ),
              rev_index( rev )
          { }
    };


    int M_time_o;
    int M_time_s;
    int M_max_cycle;

    std::map< std::string, OpponentType > M_opp_type_map;
    OpponentType M_current_opp_type;

    /*! M_candidate_marking_opponent[j] shows the unum of our player that are marked by Opponent j
     */
    int M_candidate_unum_marking_setplay[11];
    int M_pre_candidate_unum_setplay[11];
    ///////////

public:

    OpponentSetplayMark();

    ~OpponentSetplayMark()
      { }

    bool analyze( rcsc::CoachAgent * agent );

    int markTargets( int unum );



private:

    /*!
      \brief find an opponent to mark for each teammate for set play by greedy.
    */
    bool findOpponentToMark( rcsc::CoachAgent * agent );

    /*!
      \brief find an opponent to mark for each teammate for set play by hungalian method.
    */
    bool assignedMarker( rcsc::CoachAgent * agent,
                         int opp_min,
                         std::vector< int > & our_unum,
                         std::vector< int > & their_unum );

    void assignedSetPlayMarker( rcsc::CoachAgent * agent,
                                int opp_min,
                                std::vector< int > & our_unum,
                                std::vector< int > & their_unum );

    bool createCandidates( const rcsc::CoachWorldModel & wm,
                           int opp_min,
                           std::vector< int > & our_unum,
                           std::vector< int > & our_setplay_marker,
                           std::vector< int > & their_unum,
                           std::vector< int > & their_defender_unum );

    bool createCandidates( const rcsc::CoachWorldModel & wm,
                           int opp_min,
                           std::vector< int > & our_unum,
                           std::vector< int > & their_unum );

    rcsc::Vector2D predictOpponentPos( const rcsc::CoachWorldModel & wm,
                                       int opp_min,
                                       int unum );

    int estimateOppMin( const rcsc::CoachWorldModel & wm );

    void compareCandidate( const rcsc::CoachWorldModel & wm,
                           int opp_min );

    void add_edge( int from,
                   int to,
                   int cap,
                   int cost,
                   std::vector< std::vector<OpponentSetplayMark::edge> > & G );

    int min_cost_flow( int s,
                       int t,
                       int f,
                       std::vector< std::vector<OpponentSetplayMark::edge> > & G );

# if 0

  static
    int predict_self_reach_cycle( const rcsc::CoachPlayerObject *p,
                                  const rcsc::Vector2D & target_point,
                                  const double & dist_thr,
                                  const int wait_cycle,
                                  const bool save_recovery,
                                  rcsc::StaminaModel  stamina );

    static
    int predict_player_turn_cycle( const rcsc::PlayerType * player_type,
                                   const rcsc::AngleDeg & player_body,
                                   const double & player_speed,
                                   const double & target_dist,
                                   const rcsc::AngleDeg & target_angle,
                                   const double & dist_thr,
                                   const bool use_back_dash );

#endif
};

//#endif
