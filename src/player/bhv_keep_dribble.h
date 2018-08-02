// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifndef BHV_KEEP_DRIBBLE_H
#define BHV_KEEP_DRIBBLE_H

#include "cooperative_action.h"

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

/*!
  \class Bhv_KeepDribble
  \brief dribble action that tries to keep ball kickable
*/
class Bhv_KeepDribble
    : public rcsc::SoccerBehavior {
private:
    int M_action_index;
    int M_mode;
    rcsc::Vector2D M_target_ball_pos;
    rcsc::Vector2D M_target_player_pos;
    rcsc::Vector2D M_first_ball_vel;
    double M_first_turn_moment;
    double M_first_dash_power;
    double M_first_dash_dir; // dash direction relative to player's body angle

    int M_total_step;
    int M_kick_step;
    int M_turn_step;
    int M_dash_step;

    rcsc::NeckAction::Ptr M_neck_action;
    rcsc::ViewAction::Ptr M_view_action;

public:

    explicit
    Bhv_KeepDribble( const CooperativeAction & action );

    /*!
      \brief perform dribble action
      \param agent agent pointer to the agent itself
      \return true with action, false if can't do dribble
     */
    bool execute( rcsc::PlayerAgent * agent );


private:

    void setFirstTurnNeck( rcsc::PlayerAgent * agent );

    bool doFirstKick( rcsc::PlayerAgent * agent );

    bool doKeepDashes( rcsc::PlayerAgent * agent );
    bool doKeepKickDashes( rcsc::PlayerAgent * agent );
    bool doKeepKickTurnDashes( rcsc::PlayerAgent * agent );
    bool doKeepTurnKickDashes( rcsc::PlayerAgent * agent );
    bool doKeepCollideTurnKickDashes( rcsc::PlayerAgent * agent );

};

#endif
