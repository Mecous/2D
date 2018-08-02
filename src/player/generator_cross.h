// -*-c++-*-

/*!
  \file generator_cross.h
  \brief cross pass generator Header File
*/

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

#ifndef GENERATOR_CROSS_H
#define GENERATOR_CROSS_H

#include "act_pass.h"

#include <rcsc/player/abstract_player_object.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class PlayerObject;
class WorldModel;
}


class GeneratorCross {
public:

    struct Course {
        CooperativeAction::Ptr action_;
        double value_;

        explicit
        Course( const CooperativeAction::Ptr & a )
            : action_( a ),
              value_( 0.0 )
          { }
    };

    typedef std::vector< Course > Cont;

private:

    int M_total_count;

    const rcsc::AbstractPlayerObject * M_passer; //!< estimated passer
    rcsc::Vector2D M_first_point;

    rcsc::AbstractPlayerObject::Cont M_receiver_candidates;
    rcsc::AbstractPlayerObject::Cont M_opponents;

    Cont M_dash_line_courses;
    Cont M_courses;


    // private for singleton
    GeneratorCross();

    // not used
    GeneratorCross( const GeneratorCross & );
    GeneratorCross & operator=( const GeneratorCross & );

public:

    static
    GeneratorCross & instance();

    void generate( const rcsc::WorldModel & wm );


    const Cont & dashLineCourses( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_dash_line_courses;
      }

    const Cont & courses( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_courses;
      }

private:

    void clear();

    void updatePasser( const rcsc::WorldModel & wm );
    void updateReceivers( const rcsc::WorldModel & wm );
    void updateOpponents( const rcsc::WorldModel & wm );

    void createCourses( const rcsc::WorldModel & wm );
    void evaluateCourses( const rcsc::WorldModel & wm );

    void createCrossOnDashLine( const rcsc::WorldModel & wm,
                                const rcsc::AbstractPlayerObject * receiver );
    void createCross( const rcsc::WorldModel & wm,
                      const rcsc::AbstractPlayerObject * receiver );

    CooperativeAction::SafetyLevel getSafetyLevel( const double first_ball_speed,
                                                   const rcsc::AngleDeg & ball_move_angle,
                                                   const int n_kick,
                                                   const int ball_step );
    CooperativeAction::SafetyLevel getOpponentSafetyLevel( const rcsc::AbstractPlayerObject * opponent,
                                                           const rcsc::Vector2D & first_ball_vel,
                                                           const rcsc::AngleDeg & ball_move_angle,
                                                           const int n_kick,
                                                           const int ball_step );

};

#endif
