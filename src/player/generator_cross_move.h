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

#ifndef GENERATOR_CROSS_MOVE_H
#define GENERATOR_CROSS_MOVE_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class AbstractPlayerObject;
class WorldModel;
}

class GeneratorCrossMove {
public:

    struct TargetPoint {
        int id_;
        rcsc::Vector2D pos_;
        int self_move_step_;
        double value_;

        TargetPoint()
            : id_( -1 ),
              pos_( rcsc::Vector2D::INVALIDATED ),
              self_move_step_( 1000 ),
              value_( -1000000.0 )
          { }

        TargetPoint( const int id,
                     const rcsc::Vector2D & pos )
            : id_( id ),
              pos_( pos ),
              self_move_step_( 1000 ),
              value_( -1000000.0 )
          { }

        struct Sorter {
            bool operator()( const TargetPoint & lhs,
                             const TargetPoint & rhs ) const
              {
                  return lhs.value_ > rhs.value_;
              }
        };
    };

private:

    rcsc::GameTime M_update_time;

    TargetPoint M_best_point;
    std::vector< TargetPoint > M_target_points;

    rcsc::GameTime M_previous_time;
    TargetPoint M_previous_best_point;

    // private for singleton
    GeneratorCrossMove();

    // not used
    GeneratorCrossMove( const GeneratorCrossMove & );
    GeneratorCrossMove & operator=( const GeneratorCrossMove & );

public:

    static
    GeneratorCrossMove & instance();

    const TargetPoint & bestPoint( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_best_point;
      }

    const std::vector< TargetPoint > & targetPoints( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_target_points;
      }

private:

    void clear();
    void generate( const rcsc::WorldModel & wm );

    void createTargetPoints( const rcsc::WorldModel & wm );
    bool existOtherReceiver( const rcsc::WorldModel & wm,
                             const rcsc::Vector2D & pos );
    bool existPassCourse( const rcsc::WorldModel & wm,
                          const rcsc::Vector2D & first_ball_pos,
                          const TargetPoint & target_point );

    void evaluate( const rcsc::WorldModel & wm );

};

#endif
