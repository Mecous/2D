// -*-c++-*-

/*!
  \file generator_self_pass.h
  \brief self pass generator Header File
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

#ifndef GENERATOR_SELF_PASS_H
#define GENERATOR_SELF_PASS_H

#include "cooperative_action.h"

#include <rcsc/player/abstract_player_object.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class PlayerObject;
class WorldModel;
}

class GeneratorSelfPass {
private:

    rcsc::GameTime M_update_time;
    int M_total_count;

    std::vector< CooperativeAction::Ptr > M_courses;

    // private for singleton
    GeneratorSelfPass();

    // not used
    GeneratorSelfPass( const GeneratorSelfPass & );
    GeneratorSelfPass & operator=( const GeneratorSelfPass & );
public:

    static
    GeneratorSelfPass & instance();

    void generate( const rcsc::WorldModel & wm );

    const std::vector< CooperativeAction::Ptr > & courses( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_courses;
      }

private:

    void clear();
    void debugPrintCandidates( const rcsc::WorldModel & wm );

    void createCourses( const rcsc::WorldModel & wm );

    void createSelfCache( const rcsc::WorldModel & wm,
                          const rcsc::AngleDeg & dash_angle,
                          const int n_kick,
                          const int n_turn,
                          const int n_dash,
                          std::vector< rcsc::Vector2D > & self_cache );
    rcsc::Vector2D simulateSelfPos( const rcsc::WorldModel & wm,
                                    const rcsc::AngleDeg & dash_angle,
                                    const int n_kick,
                                    const int n_turn,
                                    const int n_dash );

    bool canKickOneStep( const rcsc::WorldModel & wm,
                         const int n_turn,
                         const int n_dash,
                         const rcsc::Vector2D & receive_pos );

    CooperativeAction::SafetyLevel getSafetyLevel( const rcsc::WorldModel & wm,
                                                   const int n_kick,
                                                   const int n_turn,
                                                   const int n_dash,
                                                   const rcsc::Vector2D & ball_first_vel,
                                                   const rcsc::Vector2D & receive_pos );
    CooperativeAction::SafetyLevel getOpponentSafetyLevel( const rcsc::WorldModel & wm,
                                                           const rcsc::PlayerObject * opponent,
                                                           const int n_kick,
                                                           const int n_turn,
                                                           const int n_dash,
                                                           const rcsc::Vector2D & ball_first_vel,
                                                           const rcsc::AngleDeg & ball_move_angle );

};

#endif
