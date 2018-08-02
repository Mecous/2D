// -*-c++-*-

/*!
  \file generator_omni_dribble.h
  \brief omni direction dribble course generator Header File
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

#ifndef GENERATOR_OMNI_DRIBBLE_H
#define GENERATOR_OMNI_DRIBBLE_H

#include "cooperative_action.h"

#include <rcsc/player/abstract_player_object.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class PlayerObject;
class WorldModel;
}

class GeneratorOmniDribble {
private:

    rcsc::GameTime M_update_time;
    int M_total_count;

    rcsc::Vector2D M_first_ball_pos;
    rcsc::Vector2D M_first_ball_vel;

    std::vector< CooperativeAction::Ptr > M_courses;

    // private for singleton
    GeneratorOmniDribble();

    // not used
    GeneratorOmniDribble( const GeneratorOmniDribble & );
    GeneratorOmniDribble & operator=( const GeneratorOmniDribble & );
public:

    static
    GeneratorOmniDribble & instance();

    void generate( const rcsc::WorldModel & wm );

    const std::vector< CooperativeAction::Ptr > & courses( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_courses;
      }

private:

    void clear();

    void generateImpl( const rcsc::WorldModel & wm );

    void simulateKickDashes( const rcsc::WorldModel & wm,
                             const double dash_dir );

    void createSelfCache( const rcsc::WorldModel & wm,
                          const double dash_dir,
                          const int max_dash,
                          std::vector< rcsc::Vector2D > & self_cache );

    bool checkExecutable( const rcsc::WorldModel & wm,
                          const int n_dash,
                          const std::vector< rcsc::Vector2D > & self_cache,
                          const rcsc::Vector2D & last_ball_pos );

    CooperativeAction::SafetyLevel getSafetyLevel( const rcsc::WorldModel & wm,
                                                   const int n_dash,
                                                   const rcsc::Vector2D & last_ball_pos );
    CooperativeAction::SafetyLevel getOpponentSafetyLevel( const int n_dash,
                                                           const rcsc::Vector2D & last_ball_pos,
                                                           const rcsc::PlayerObject * opponent,
                                                           int * result_step );
};

#endif
