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

#ifndef RECEIVE_SIMULATOR_H
#define RECEIVE_SIMULATOR_H

#include "cooperative_action.h"

#include <vector>

namespace rcsc {
class PlayerObject;
class WorldModel;
}

class GeneratorReceiveMove {
public:

    struct ReceivePoint {
        rcsc::Vector2D pos_;
        int self_move_step_;
        CooperativeAction::SafetyLevel safety_level_;

        ReceivePoint( const rcsc::Vector2D & pos,
                      const int self_step,
                      const CooperativeAction::SafetyLevel level )
            : pos_( pos ),
              self_move_step_( self_step ),
              safety_level_( level )
          { }
    };


private:

    rcsc::GameTime M_update_time;
    int M_total_count;

    const rcsc::PlayerObject * M_passer;
    int M_last_passer_unum;
    rcsc::Vector2D M_first_ball_pos;

    std::vector< ReceivePoint > M_receive_points_direct_pass;
    std::vector< ReceivePoint > M_receive_points_leading_pass;
    std::vector< ReceivePoint > M_receive_points_through_pass;

    std::vector< ReceivePoint > M_receive_points_around_home;

    // private for singleton
    GeneratorReceiveMove();

    // not used
    GeneratorReceiveMove( const GeneratorReceiveMove & );
    GeneratorReceiveMove & operator=( const GeneratorReceiveMove & );

public:

    static
    GeneratorReceiveMove & instance();

    void generate( const rcsc::WorldModel & wm );

    const std::vector< ReceivePoint > & getReceivePointsDirectPass( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_receive_points_direct_pass;
      }

    const std::vector< ReceivePoint > & getReceivePointsAroundHome( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_receive_points_around_home;
      }

    const std::vector< ReceivePoint > & getReceivePointsThroughPass( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_receive_points_through_pass;
      }

private:

    void clear();

    void updatePasser( const rcsc::WorldModel & wm );
    void generateImpl( const rcsc::WorldModel & wm );
    void createDirectPass( const rcsc::WorldModel & wm );
    void createLeadingPass( const rcsc::WorldModel & wm );
    void createAroundHomePosition( const rcsc::WorldModel & wm );
    void createThroughPass( const rcsc::WorldModel & wm );

    CooperativeAction::SafetyLevel getSafetyLevel( const rcsc::WorldModel & wm,
                                                   const rcsc::Vector2D & receive_point,
                                                   const double first_ball_speed,
                                                   const int ball_move_step );

};



#endif
