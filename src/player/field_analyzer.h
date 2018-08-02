// -*-c++-*-

/*!
  \file field_analyzer.h
  \brief miscellaneous field analysis class Header File
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

#ifndef FIELD_ANALYZER_H
#define FIELD_ANALYZER_H

#include "player_graph.h"
#include "predict_state.h"

#include "ball_move_model.h"

#include <rcsc/formation/formation.h>
#include <rcsc/player/player_object.h>
#include <rcsc/geom/voronoi_diagram.h>
#include <rcsc/geom/vector_2d.h>
#include <list>
#include <cmath>

namespace rcsc {
class AbstractPlayerObject;
class PlayerType;
class ServerParam;
class StaminaModel;
class WorldModel;
}

class PassChecker;

class FieldAnalyzer {
public:
    enum {
        SHOOT_AREA_X_DIVS = 14,
        SHOOT_AREA_Y_DIVS = 21,
    };

private:

    rcsc::VoronoiDiagram M_target_voronoi_diagram;
    std::vector< rcsc::Vector2D > M_voronoi_target_points;

    rcsc::VoronoiDiagram M_positioning_voronoi_diagram;

    PlayerGraph M_our_players_graph;

    BallMoveModel M_ball_move_model;

    const rcsc::AbstractPlayerObject * M_our_shoot_blocker;

    double M_shoot_point_values[SHOOT_AREA_X_DIVS][SHOOT_AREA_Y_DIVS];

    rcsc::Formation::ConstPtr M_our_formation;

    bool M_exist_opponent_man_marker;
    bool M_exist_opponent_pass_line_marker;

    std::list< double > M_playon_offside_lines;
    double M_offside_line_speed;

    FieldAnalyzer();
public:

    static
    FieldAnalyzer & instance();

    static
    const FieldAnalyzer & i()
      {
          return instance();
      }

    bool init( const std::string & ball_table_file );


    const BallMoveModel & ballMoveModel() const
      {
          return M_ball_move_model;
      }

    const rcsc::VoronoiDiagram & targetVoronoiDiagram() const
      {
          return M_target_voronoi_diagram;
      }

    const std::vector< rcsc::Vector2D > & voronoiTargetPoints() const
      {
          return M_voronoi_target_points;
      }

    const rcsc::VoronoiDiagram & positioningVoronoiDiagram() const
      {
          return M_positioning_voronoi_diagram;
      }

    const PlayerGraph & ourPlayersGraph() const
      {
          return M_our_players_graph;
      }

    const rcsc::AbstractPlayerObject * ourShootBlocker() const
      {
          return M_our_shoot_blocker;
      }


    rcsc::Formation::ConstPtr ourFormation() const
        {
            return M_our_formation;
        }

    void setOurFormation( rcsc::Formation::ConstPtr ptr )
      {
          M_our_formation = ptr;
      }

    bool existOpponentManMarker() const
      {
          return M_exist_opponent_man_marker;
      }
    bool existOpponentPassLineMarker() const
      {
          return M_exist_opponent_pass_line_marker;
      }

    void update( const rcsc::WorldModel & wm );

private:

    void updateVoronoiDiagram( const rcsc::WorldModel & wm );
    void updatePlayerGraph( const rcsc::WorldModel & wm );
    void updateOurShootBlocker( const rcsc::WorldModel & wm );
    void updateShootPointValues( const rcsc::WorldModel & wm );
    void updateOpponentManMarker( const rcsc::WorldModel & wm );
    void updateOpponentPassLineMarker( const rcsc::WorldModel & wm );
    void updateOffsideLines( const rcsc::WorldModel & wm );

    void debugPrintMovableRange( const rcsc::WorldModel & wm );
    void debugPrintShootPositions();
    void debugPrintTargetVoronoiDiagram();
    void debugPrintPositioningVoronoiDiagram();

public:
    inline
    static
    int estimate_min_reach_cycle( const rcsc::Vector2D & player_pos,
                                  const double control_area,
                                  const double player_speed_max,
                                  const rcsc::Vector2D & target_point,
                                  const rcsc::AngleDeg & target_move_angle )
      {
          rcsc::Vector2D target_to_player = ( player_pos - target_point ).rotatedVector( -target_move_angle );
          if ( target_to_player.x < -control_area )
          {
              return -1;
          }
          double dist = std::max( 0.0, target_to_player.absY() - control_area );
          return std::max( 1, static_cast< int >( std::floor( dist / player_speed_max ) ) );
      }

    static
    double estimate_virtual_dash_distance( const rcsc::AbstractPlayerObject * player );

    static
    int predict_player_turn_cycle( const rcsc::PlayerType * player_type,
                                   const rcsc::AngleDeg & player_body,
                                   const double & player_speed,
                                   const double & target_dist,
                                   const rcsc::AngleDeg & target_angle,
                                   const double & dist_thr,
                                   const bool use_back_dash );

    static
    int predict_self_reach_cycle( const rcsc::WorldModel & wm,
                                  const rcsc::Vector2D & target_point,
                                  const double & dist_thr,
                                  const int wait_cycle,
                                  const bool save_recovery,
                                  rcsc::StaminaModel * stamina );

    /*!
      \param dist_thr kickable_area or catchable_area
      \param penalty_distance result of estimate_virtual_dash_distance()
      \param wait_cycle penalty of kick, tackle or observation delay.
     */
    static
    int predict_player_reach_cycle( const rcsc::AbstractPlayerObject * player,
                                    const rcsc::Vector2D & target_point,
                                    const double & dist_thr,
                                    const double & penalty_distance,
                                    const int body_count_thr,
                                    const int default_n_turn,
                                    const int wait_cycle,
                                    const bool use_back_dash );
    static
    int predict_kick_count( const rcsc::WorldModel & wm,
                            const rcsc::AbstractPlayerObject * kicker,
                            const double & first_ball_speed,
                            const rcsc::AngleDeg & ball_move_angle );


    static
    int get_pass_count( const PredictState & state,
                        const PassChecker & pass_checker,
                        const double first_ball_speed,
                        int max_count = -1 );

    static
    double get_congestion( const PredictState & wm,
                           const rcsc::Vector2D & point,
                           const int opponent_additional_chase_time = 0 );

    static
    const rcsc::AbstractPlayerObject * get_blocker( const rcsc::WorldModel & wm,
                                                    const rcsc::Vector2D & opponent_pos );

    static
    const rcsc::AbstractPlayerObject * get_blocker( const rcsc::WorldModel & wm,
                                                    const rcsc::Vector2D & opponent_pos,
                                                    const rcsc::Vector2D & base_pos );

    static
    rcsc::Vector2D get_field_bound_predict_ball_pos( const rcsc::WorldModel & wm,
                                                     const int predict_step,
                                                     const double shrink_offset );
    static
    rcsc::Vector2D get_field_bound_opponent_ball_pos( const rcsc::WorldModel & wm );

    static
    bool is_ball_moving_to_our_goal( const rcsc::WorldModel & wm );

    static
    bool is_ball_moving_to_their_goal( const rcsc::WorldModel & wm );

    static
    double get_tackle_probability_after_dash( const rcsc::WorldModel & wm,
                                              double * result_dash_power,
                                              double * result_dash_dir );
    static
    double get_tackle_probability_after_turn( const rcsc::WorldModel & wm );
};

#endif
