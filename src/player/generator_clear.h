// -*-c++-*-

/*!
  \file generator_clear.h
  \brief clear kick generator class Header File
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

#ifndef GENERATOR_CLEAR_H
#define GENERATOR_CLEAR_H

#include "cooperative_action.h"

#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class AbstractPlayerObject;
class WorldModel;
}

/*!
  \class GeneratorClear
  \brief clear kick generator
 */
class GeneratorClear {
private:

    //! best clear course
    CooperativeAction::Ptr M_best_action;

    // private for singleton
    GeneratorClear();

    // not used
    GeneratorClear( const GeneratorClear & );
    const GeneratorClear & operator=( const GeneratorClear & );
public:

    static
    GeneratorClear & instance();

    void generate( const rcsc::WorldModel & wm );

    CooperativeAction::Ptr getBestAction( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_best_action;
      }

private:

    void clear();

    void generateImpl( const rcsc::WorldModel & wm );

    int predictOpponentReachStep( const rcsc::WorldModel & wm,
                                  const rcsc::Vector2D & first_ball_vel,
                                  const rcsc::AngleDeg & ball_move_angle,
                                  const int ball_move_step );

    int predictOpponentReachStep( const rcsc::WorldModel & wm,
                                  const rcsc::AbstractPlayerObject * opponent,
                                  const rcsc::Vector2D & first_ball_vel,
                                  const rcsc::AngleDeg & ball_move_angle,
                                  const int ball_move_step );

};

#endif
