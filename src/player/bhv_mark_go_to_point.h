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

#ifndef BHV_MARK_GO_TO_POINT_H
#define BHV_MARK_GO_TO_POINT_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/common/player_type.h>
#include <rcsc/geom/vector_2d.h>

namespace rcsc {
class AbstractPlayerObject;
}

class Bhv_MarkGoToPoint
    : public rcsc::SoccerBehavior {
private:
    const rcsc::AbstractPlayerObject * M_mark_target;
    rcsc::Vector2D M_target_point; //! move target point
    const double M_dist_thr; //! distance threshold to the target point
    bool M_save_recovery;
    bool M_back_mode;
public:

    Bhv_MarkGoToPoint( const rcsc::AbstractPlayerObject * mark_target,
                       const rcsc::Vector2D & point,
                       const double dist_thr )
        : M_mark_target( mark_target ),
          M_target_point( point ),
          M_dist_thr( dist_thr ),
          M_save_recovery( true ),
          M_back_mode( false )
      { }

    /*!
      \brief execute action
      \param agent pointer to the agent itself
      \return true if action is performed
    */
    bool execute( rcsc::PlayerAgent * agent );

private:

    void checkGoalPost( const rcsc::PlayerAgent * agent );
    //void updateTargetPoint( const rcsc::PlayerAgent * agent );

    bool doOmniDash( rcsc::PlayerAgent * agent );

    bool doGoToTurn( rcsc::PlayerAgent * agent );

    bool doDash( rcsc::PlayerAgent * agent );


    void doMarkTurn( rcsc::PlayerAgent * agent );

    void setTurnNeck( rcsc::PlayerAgent * agent );
};

#endif
