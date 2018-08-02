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

#ifndef HELIOS_BHV_GET_BALL_H
#define HELIOS_BHV_GET_BALL_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/geom/rect_2d.h>

namespace rcsc {
class WorldModel;
}

// added 2008-07-03

class Bhv_GetBall
    : public rcsc::SoccerBehavior {
private:

    rcsc::Rect2D M_bounding_rect;
    rcsc::Vector2D M_center_pos;

public:

    struct Param {
        rcsc::Vector2D point_; //!< target point
        int turn_; //!< estimated turn steps
        int cycle_; //!< estimated reach steps
        double stamina_; //!< left stamina

        Param()
            : point_( rcsc::Vector2D::INVALIDATED ),
              turn_( 1000 ),
              cycle_( 1000 ),
              stamina_( 0.0 )
          { }
    };

    explicit
    Bhv_GetBall( const rcsc::Rect2D & bounding_rect,
                 const rcsc::Vector2D & center_pos = rcsc::Vector2D::INVALIDATED )
        : M_bounding_rect( bounding_rect ),
          M_center_pos( center_pos )
      { }

    bool execute( rcsc::PlayerAgent * agent );



    static
    void simulate( const rcsc::WorldModel & wm,
                   const rcsc::Vector2D & opponent_trap_pos,
                   const rcsc::Vector2D & center_pos,
                   const rcsc::Rect2D & bounding_rect,
                   const double & dist_thr,
                   const bool save_recovery,
                   Param * param );
    static
    void simulateNoTurn( const rcsc::WorldModel & wm,
                         const rcsc::Vector2D & opponent_trap_pos,
                         const rcsc::Vector2D & center_pos,
                         const rcsc::Rect2D & bounding_rect,
                         const double & dist_thr,
                         const bool save_recovery,
                         Param * param );
    static
    int predictSelfReachCycle( const rcsc::WorldModel & wm,
                               const rcsc::Vector2D & target_point,
                               const double & dist_thr,
                               const bool save_recovery,
                               int * turn_step,
                               double * stamina );

};

#endif
