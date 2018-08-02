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

#ifndef AGENT2D_ROLE_GOALIE_H
#define AGENT2D_ROLE_GOALIE_H

#include "soccer_role.h"

#include "move_simulator.h"

#include <rcsc/geom/vector_2d.h>

class RoleGoalie
    : public SoccerRole {
private:

    bool M_opponent_shoot_situation;
    bool M_goal_post_block_situation;
    bool M_dangerous_situation;
    bool M_ball_moving_to_our_goal;
    bool M_despair_situation;

    rcsc::Vector2D M_opponent_ball_pos;
    // rcsc::Vector2D M_block_center;

public:

    static const std::string NAME;

    RoleGoalie();

    ~RoleGoalie()
      {
          //std::cerr << "delete RoleGoalie" << std::endl;
      }

    virtual
    const char * shortName() const
      {
          return "GK";
      }

    virtual
    bool execute( rcsc::PlayerAgent * agent );


    static
    const
    std::string & name()
      {
          return NAME;
      }

    static
    SoccerRole::Ptr create()
      {
          SoccerRole::Ptr ptr( new RoleGoalie() );
          return ptr;
      }

private:
    void judgeSituation( rcsc::PlayerAgent * agent );
    void setDefaultNeckAndViewAction( rcsc::PlayerAgent * agent );

    bool doCatchIfPossible( rcsc::PlayerAgent * agent );
    bool doKickIfPossible( rcsc::PlayerAgent * agent );
    bool doTackleIfNecessary( rcsc::PlayerAgent * agent );
    bool doChaseBallIfNessary( rcsc::PlayerAgent * agent );
    bool doFindBallIfNecessary( rcsc::PlayerAgent * agent );

    void doTurnLookBall( rcsc::PlayerAgent * agent,
                         const rcsc::AngleDeg & target_body_angle );
    bool doTurnIfOnTheBlockLine( rcsc::PlayerAgent * agent,
                                 const rcsc::Vector2D & target_pos,
                                 const double dist_tolerance,
                                 const double angle_tolerance );
    bool doTurnIfNecessaryAndPossible( rcsc::PlayerAgent * agent,
                                       const rcsc::Vector2D & target_pos,
                                       const double dist_tolerance,
                                       const double angle_tolerance );
    bool doMove( rcsc::PlayerAgent * agent );

    bool doChaseBall( rcsc::PlayerAgent * agent );

    rcsc::Vector2D getMoveTargetPosition( rcsc::PlayerAgent * agent );
    double getMoveDashPower( rcsc::PlayerAgent * agent,
                             const rcsc::Vector2D & target_pos );
    double getMoveTargetTolerance( rcsc::PlayerAgent * agent,
                                   const rcsc::Vector2D & target_pos );

    bool doGoToPoint( rcsc::PlayerAgent * agent,
                      const rcsc::Vector2D & target_pos,
                      const double tolerance );

    bool doTurnDash( rcsc::PlayerAgent * agent,
                     const MoveSimulator::Result & result );
};


#endif
