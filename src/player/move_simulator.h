// -*-c++-*-

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

#ifndef MOVE_SIMULATOR_H
#define MOVE_SIMULATOR_H

#include <rcsc/geom/matrix_2d.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class AbstractPlayerObject;
class WorldModel;
}

class MoveSimulator {
public:

    struct Result {
        int turn_step_;
        int dash_step_;
        double stamina_;
        double turn_moment_;
        double dash_power_;
        double dash_dir_;
        rcsc::AngleDeg body_angle_;

        Result()
            : turn_step_( -1 ),
              dash_step_( -1 ),
              stamina_( 0.0 ),
              turn_moment_( 0.0 ),
              dash_power_( 0.0 ),
              dash_dir_( 0.0 )
          { }

        void setValue( const int n_turn,
                       const int n_dash,
                       const double stamina,
                       const double turn_moment,
                       const double dash_power,
                       const double dash_dir,
                       const rcsc::AngleDeg & body_angle );
    };

private:

    static
    int simulate_turn_step( const rcsc::WorldModel & wm,
                            const rcsc::Vector2D & target_point,
                            const double tolerance,
                            const int move_step,
                            const bool back_dash,
                            rcsc::AngleDeg * result_dash_angle );

public:

    /*!
      \brief estimate total steps for going to the target point
      \return estimated step count
     */
    static
    int simulate_self_turn_dash( const rcsc::WorldModel & wm,
                                 const rcsc::Vector2D & target_point,
                                 const double tolerance,
                                 const bool back_dash,
                                 const int max_step,
                                 Result * result );

    /*!
      \brief check if the self player can reach the target point within max_step
     */
    static
    bool self_can_reach_after_turn_dash( const rcsc::WorldModel & wm,
                                         const rcsc::Vector2D & target_point,
                                         const double tolerance,
                                         const bool back_dash,
                                         const int max_step,
                                         Result * result );

    static
    int simulate_self_omni_dash( const rcsc::WorldModel & wm,
                                 const rcsc::Vector2D & target_point,
                                 const double tolerance,
                                 const int max_step,
                                 Result * result );

};

#endif
