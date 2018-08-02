// -*-c++-*-

/*!
  \file act_dribble.h
  \brief dribble action object Header File.
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 3 of the License, or (at your option) any later version.

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

#ifndef ACT_DRIBBLE_H
#define ACT_DRIBBLE_H

#include "cooperative_action.h"

#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>
#include <rcsc/types.h>

#include <boost/shared_ptr.hpp>

class ActDribble
    : public CooperativeAction {
public:

    enum {
        KICK_TURN_DASHES,
        OMNI_KICK_DASHES,
        KEEP_DASHES,
        KEEP_KICK_DASHES,
        KEEP_KICK_TURN_DASHES,
        KEEP_TURN_KICK_DASHES,
        KEEP_COLLIDE_TURN_KICK_DASHES,
    };

private:

    ActDribble( const int unum,
                const rcsc::Vector2D & target_ball_pos,
                const int duration_time,
                const char * description );

public:

    /*!
      \brief create normal (kick->turn->dash) dribble
     */
    static
    CooperativeAction::Ptr create_normal( const int unum,
                                          const rcsc::Vector2D & target_ball_pos,
                                          const double target_body_angle,
                                          const rcsc::Vector2D & first_ball_vel,
                                          const int kick_count,
                                          const int turn_count,
                                          const int dash_count,
                                          const char * description );

    /*!
      \param unum player's uniform number
      \param target_ball_pos ball reach point
      \param target_player_pos player reach point
      \param first_dash_power max dash power for the first dash
      \param first_dash_dir first dash direction. if the value is ERROR_ANGLE, dash direction is set to 0
      \param dash_count total dash count
      \param description description message
     */
    static
    CooperativeAction::Ptr create_dash_only( const int unum,
                                             const rcsc::Vector2D & target_ball_pos,
                                             const rcsc::Vector2D & target_player_pos,
                                             const double target_body_angle,
                                             const rcsc::Vector2D & first_ball_vel,
                                             const double first_dash_power,
                                             const double first_dash_dir,
                                             const int dash_count,
                                             const char * description );

    /*!
      \brief create omni (kick->dash) dribble
     */
    static
    CooperativeAction::Ptr create_omni( const int unum,
                                        const rcsc::Vector2D & target_ball_pos,
                                        const rcsc::Vector2D & target_player_pos,
                                        const double target_body_angle,
                                        const rcsc::Vector2D & first_ball_vel,
                                        const double dash_power,
                                        const double dash_dir,
                                        const int dash_count,
                                        const char * description );
};

#endif
