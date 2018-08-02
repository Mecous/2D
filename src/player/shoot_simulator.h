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

#ifndef SHOOT_SIMULATOR_H
#define SHOOT_SIMULATOR_H

#include "predict_player_object.h"

#include <rcsc/player/abstract_player_object.h>
#include <rcsc/geom/vector_2d.h>

class ShootSimulator {
private:
public:

    //
    // TODO: intercept simulation
    //

    static
    rcsc::Vector2D get_our_team_near_goal_post_pos( const rcsc::Vector2D & point );

    static
    rcsc::Vector2D get_our_team_far_goal_post_pos( const rcsc::Vector2D & point );

    static
    rcsc::Vector2D get_their_team_near_goal_post_pos( const rcsc::Vector2D & point );

    static
    rcsc::Vector2D get_their_team_far_goal_post_pos( const rcsc::Vector2D & point );


    static
    void get_dist_from_our_goal_post( const rcsc::Vector2D & point,
                                      double * near_post_dist,
                                      double * far_post_dist );
    static
    double get_dist_from_our_near_goal_post( const rcsc::Vector2D & point );

    static
    double get_dist_from_our_far_goal_post( const rcsc::Vector2D & point );


    static
    void get_dist_from_their_goal_post( const rcsc::Vector2D & point,
                                        double * near_post_dist,
                                        double * far_post_dist );
    static
    double get_dist_from_their_near_goal_post( const rcsc::Vector2D & point );

    static
    double get_dist_from_their_far_goal_post( const rcsc::Vector2D & point );

    static
    bool is_ball_moving_to_our_goal( const rcsc::Vector2D & ball_pos,
                                     const rcsc::Vector2D & ball_vel,
                                     const double post_buffer );
    static
    bool is_ball_moving_to_their_goal( const rcsc::Vector2D & ball_pos,
                                       const rcsc::Vector2D & ball_vel,
                                       const double post_buffer );



    static
    bool can_shoot_from( const bool is_self,
                         const rcsc::Vector2D & pos,
                         const rcsc::AbstractPlayerObject::Cont & opponents,
                         const int valid_opponent_threshold );

    static
    bool opponent_can_shoot_from( const rcsc::Vector2D & pos,
                                  const rcsc::AbstractPlayerObject::Cont & teammates,
                                  const int valid_teammate_threshold,
                                  const double shoot_dist_threshold = -1.0,
                                  const double shoot_angle_threshold = -1.0,
                                  const double teammate_dist_threshold = -1.0,
                                  double * max_angle_diff_result = static_cast< double * >( 0 ),
                                  const bool calculate_detail = false );
};



#endif
