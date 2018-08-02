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

#ifndef BHV_SET_PLAY_H
#define BHV_SET_PLAY_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/player/soccer_action.h>

#include <vector>

namespace rcsc {
class WorldModel;
}

class Bhv_SetPlay
    : public rcsc::SoccerBehavior {
public:

    Bhv_SetPlay()
      { }

    bool execute( rcsc::PlayerAgent * agent );

    static
    rcsc::Vector2D get_mark_point( const rcsc::PlayerAgent * agent,
                                   int opp_min );

    static
    rcsc::Vector2D get_avoid_circle_point( const rcsc::WorldModel & world,
                                           const rcsc::Vector2D & target_point );

    static
    double get_set_play_dash_power( const rcsc::PlayerAgent * agent );

    static
    bool is_kicker( const rcsc::PlayerAgent * agent );

    static
    bool is_fast_restart_situation( const rcsc::PlayerAgent * agent );

    static
    bool is_delaying_tactics_situation( const rcsc::PlayerAgent * agent );

    static
    int estimateOppMin(  const rcsc::WorldModel & world );

    static
    rcsc::Vector2D predictPlayerPos( const rcsc::WorldModel & world,
                                     int opp_min,
                                     int unum );

    static
    int find_kicker( const rcsc::WorldModel & world,
                     int opp_min );

    static
    int find_second_kicker( const rcsc::WorldModel & world,
                            int opp_min,
                            int unum_possible_kicker );

    static
    bool createRecieverCandidate( const rcsc::WorldModel & world,
                                  int opp_min,
                                  int unum_possible_kicker,
                                  int unum_possible_2nd_kicker,
                                  std::vector<int> & candidate_reciever );
    static
    rcsc::Vector2D decide_reciever( const rcsc::WorldModel & wm,
                                    int opp_min,
                                    std::vector<int> & candidate_reciever,
                                    int unum_possible_2nd_kicker );

    static
    bool createPasserCandidate( const rcsc::WorldModel & world,
                                int target_unum,
                                std::vector<int> & candidate_passer );
    static
    rcsc::Vector2D decide_passer( const rcsc::WorldModel & wm,
                                  std::vector<int> & candidate_passer,
                                  int target_unum );


    static
    rcsc::Vector2D get_substitute_point( const rcsc::PlayerAgent * agent,
                                         int opp_min );


private:
    void doBasicTheirSetPlayMove( rcsc::PlayerAgent * agent );

    void doMarkTheirSetPlayMove( rcsc::PlayerAgent * agent );
};

#endif
