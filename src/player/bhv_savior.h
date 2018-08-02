// -*-c++-*-

/*!
  \file bhv_savior.h
  \brief aggressive goalie behavior
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

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

#ifndef BHV_SAVIOR_H
#define BHV_SAVIOR_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/player/abstract_player_object.h>
#include <rcsc/math_util.h>

namespace rcsc {
class PlayerAgent;
class WorldModel;
class Vector2D;
class Rect2D;
class AngleDeg;
}


class Bhv_Savior
    : public rcsc::SoccerBehavior {
private:

    bool M_dangerous;
    bool M_aggressive;
    bool M_emergent_advance;
    bool M_goal_line_positioning;
public:

    Bhv_Savior()
        : M_dangerous( false ),
          M_aggressive( false ),
          M_emergent_advance( false ),
          M_goal_line_positioning( false )
      { }

    bool execute( rcsc::PlayerAgent * agent );

private:

    bool doPlayOnMove( rcsc::PlayerAgent * agent );

    bool doKickOffMove( rcsc::PlayerAgent * agent,
                        const double max_error = 1.0 );

    bool doPenaltyKick( rcsc::PlayerAgent * agent );

    //
    //
    //

    void setDefaultNeckAndViewAction( rcsc::PlayerAgent * agent );

    bool doCatchIfPossible( rcsc::PlayerAgent * agent );

    bool doKickIfPossible( rcsc::PlayerAgent * agent );

    bool doTackleIfNecessary( rcsc::PlayerAgent * agent );

    bool doChaseBallIfNessary( rcsc::PlayerAgent * agent );

    bool doFindBallIfNecessary( rcsc::PlayerAgent * agent );

    bool doPositioning( rcsc::PlayerAgent * agent );

    bool doGoToPoint( rcsc::PlayerAgent * agent,
                      const rcsc::Vector2D & target_point,
                      const double max_error = 1.0,
                      const double max_power = 100.0,
                      const bool use_back_dash = true,
                      const bool force_back_dash = false,
                      const bool emergency_mode = false,
                      const bool look_ball = false );

    bool doChaseBall( rcsc::PlayerAgent * agent );

    bool doGoalLinePositioning( rcsc::PlayerAgent * agent,
                                const rcsc::Vector2D & target_position,
                                const double low_priority_x_position_error,
                                const double max_x_position_error,
                                const double max_y_position_error,
                                const double dash_power );


    rcsc::Vector2D getOpponentBallPosition( const rcsc::WorldModel & wm );

    rcsc::Vector2D getBasicPosition( rcsc::PlayerAgent * agent );

    rcsc::Vector2D getBasicPositionFromBall( const rcsc::WorldModel & wm,
                                             const rcsc::Vector2D & ball_pos,
                                             const double base_dist,
                                             const rcsc::Vector2D & self_pos );

    rcsc::Vector2D getGoalLinePositioningTarget( const rcsc::WorldModel & wm,
                                                 const double goal_x_dist,
                                                 const rcsc::Vector2D & ball_pos );

    double getGoalLinePositioningXFromGoal( const rcsc::WorldModel & wm,
                                            const rcsc::Vector2D & ball_pos );

    bool isGoalLinePositioningSituationBase( const rcsc::WorldModel & wm,
                                             const rcsc::Vector2D & ball_pos );
    bool isEmergentOneToOneSituation( const rcsc::WorldModel & wm,
                                      const rcsc::Vector2D & ball_pos );

    rcsc::AngleDeg getDirFromOurGoal( const rcsc::Vector2D & pos );

    double getTackleProbability( const rcsc::Vector2D & body_relative_ball );

    rcsc::Vector2D getSelfNextPosWithDash( const rcsc::WorldModel & wm,
                                           const double dash_power );

    double getSelfNextTackleProbabilityWithDash( const rcsc::WorldModel & wm,
                                                 const double dash_power );

    double getSelfNextTackleProbabilityWithTurn( const rcsc::WorldModel & wm );

    bool opponentCanShootFrom( const rcsc::WorldModel & wm,
                               const rcsc::Vector2D & pos,
                               const long valid_teammate_threshold = 20 );

    bool canCatchAtNearPost( const rcsc::WorldModel & wm,
                             const rcsc::Vector2D & pos,
                             const rcsc::Vector2D & ball_pos );

    bool canBlockShootFrom( const rcsc::WorldModel & wm,
                            const rcsc::Vector2D & pos );

    rcsc::Vector2D getFieldBoundPredictBallPos( const rcsc::WorldModel & wm,
                                                const int predict_step,
                                                const double shrink_offset );

};

#endif
