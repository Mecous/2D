// -*-c++-*-

/*!
  \file generator_pass.h
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

#ifndef GENRATOR_PASS_H
#define GENRATOR_PASS_H

#include "act_pass.h"

#include <rcsc/player/abstract_player_object.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class PlayerObject;
class WorldModel;
}

/*!
  \class GeneratorPass
  \brief pass action generator
 */
class GeneratorPass {
public:

    struct Receiver {
        const rcsc::AbstractPlayerObject * player_;
        rcsc::Vector2D pos_;
        rcsc::Vector2D vel_;
        rcsc::Vector2D inertia_pos_;
        double speed_;
        double penalty_distance_;
        int penalty_step_;
        rcsc::AngleDeg angle_from_ball_;
    private:
        Receiver();
    public:
        Receiver( const rcsc::AbstractPlayerObject * p,
                  const rcsc::Vector2D & first_ball_pos );
    };

    typedef std::vector< Receiver > ReceiverCont;

    struct Opponent {
        const rcsc::AbstractPlayerObject * player_;
        rcsc::Vector2D pos_;
        rcsc::Vector2D vel_;
        double speed_;
        double bonus_distance_;
    private:
        Opponent();
    public:
        explicit
        Opponent( const rcsc::AbstractPlayerObject * p );
    };

    typedef std::vector< Opponent > OpponentCont;

private:

    rcsc::GameTime M_update_time;
    int M_total_count;
    int M_pass_type;

    const rcsc::AbstractPlayerObject * M_passer; //!< estimated passer player
    rcsc::GameTime M_start_time; //!< pass action start time
    rcsc::Vector2D M_first_point; //!< first ball point

    ReceiverCont M_receiver_candidates;
    OpponentCont M_opponents;

    int M_direct_size;
    int M_leading_size;
    int M_through_size;

    std::vector< CooperativeAction::Ptr > M_direct_pass[11];
    std::vector< CooperativeAction::Ptr > M_leading_pass[11];
    std::vector< CooperativeAction::Ptr > M_through_pass[11];
    std::vector< CooperativeAction::Ptr > M_courses;


    // private for singleton
    GeneratorPass();

    // not used
    GeneratorPass( const GeneratorPass & );
    GeneratorPass & operator=( const GeneratorPass & );

public:

    static
    GeneratorPass & instance();

    void generate( const rcsc::WorldModel & wm );

    const std::vector< CooperativeAction::Ptr > & courses( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_courses;
      }

private:

    void clear();

    /*!
      \brief estimate passer player and set it to M_passer.
      \param wm world model reference
     */
    void updatePasser( const rcsc::WorldModel & wm );

    void updateReceivers( const rcsc::WorldModel & wm );
    void updateOpponents( const rcsc::WorldModel & wm );

    void createCourses( const rcsc::WorldModel & wm );

    void createDirectPass( const rcsc::WorldModel & wm ,
                           const Receiver & receiver );
    void createLeadingPass( const rcsc::WorldModel & wm ,
                            const Receiver & receiver );

    void createThroughPass( const rcsc::WorldModel & wm ,
                            const Receiver & receiver );

    void createPassCommon( const rcsc::WorldModel & wm,
                           const Receiver & receiver,
                           const rcsc::Vector2D & receive_point,
                           const int min_step,
                           const int max_step,
                           const double & min_first_ball_speed,
                           const double & max_first_ball_speed,
                           const double & min_receive_ball_speed,
                           const double & max_receive_ball_speed,
                           const double & ball_move_dist,
                           const rcsc::AngleDeg & ball_move_angle,
                           const char * description );

    const Receiver * getNearestReceiver( const rcsc::Vector2D & pos );


    int predictReceiverReachStep( const Receiver & receiver,
                                  const rcsc::Vector2D & pos,
                                  const bool use_penalty );

    CooperativeAction::SafetyLevel getSafetyLevel( const rcsc::WorldModel & wm,
                                                   const rcsc::Vector2D & first_ball_pos,
                                                   const double first_ball_speed,
                                                   const rcsc::AngleDeg & ball_move_angle,
                                                   const rcsc::Vector2D & receive_point,
                                                   const int n_kick,
                                                   const int ball_move_step );
    CooperativeAction::SafetyLevel getOpponentSafetyLevel( const rcsc::WorldModel & wm,
                                                           const Opponent & opponent,
                                                           const rcsc::Vector2D & first_ball_pos,
                                                           const rcsc::Vector2D & first_ball_vel,
                                                           const double first_ball_speed,
                                                           const rcsc::AngleDeg & ball_move_angle,
                                                           const rcsc::Vector2D & receive_point,
                                                           const int n_kick,
                                                           const int ball_move_step );

public:

    static
    bool can_check_receiver_without_opponent( const rcsc::WorldModel & wm );

    static
    bool can_see_only_turn_neck( const rcsc::WorldModel & wm,
                                 const rcsc::AbstractPlayerObject & receiver );

};

#endif
