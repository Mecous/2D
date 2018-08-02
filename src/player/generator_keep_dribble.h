// -*-c++-*-

/*!
  \file generator_keep_dribble.h
  \brief keep dribble generator Header File
*/

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

#ifndef GENERATOR_KEEP_DRIBBLE_H
#define GENERATOR_KEEP_DRIBBLE_H

#include "cooperative_action.h"

#include <rcsc/player/abstract_player_object.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/geom/matrix_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class PlayerObject;
class StaminaModel;
class WorldModel;
}

class GeneratorKeepDribble {
public:
    struct Candidate {
        CooperativeAction::Ptr action_;
        double opponent_dist_;
        double self_move_dist_;
    };

private:

    struct State {
        rcsc::Vector2D ball_pos_;
        rcsc::Vector2D self_pos_;
        double dash_power_; //!< previous dash power that results this state
        double dash_dir_; //!< previous dash direction that results this state

        State( const rcsc::Vector2D & ball_pos,
               const rcsc::Vector2D & self_pos,
               const double dash_power,
               const double dash_dir )
            : ball_pos_( ball_pos ),
              self_pos_( self_pos ),
              dash_power_( dash_power ),
              dash_dir_( dash_dir )
          { }

        State( const rcsc::Vector2D & ball_pos,
               const rcsc::Vector2D & self_pos )
            : ball_pos_( ball_pos ),
              self_pos_( self_pos ),
              dash_power_( 0 ),
              dash_dir_( 0 )
          { }
    };

    struct Result {
        std::vector< State > states_;
        CooperativeAction::Ptr action_;

        Result()
          {
              states_.reserve( 10 );
          }

        void add( const rcsc::Vector2D & ball_pos,
                  const rcsc::Vector2D & self_pos,
                  const double dash_power,
                  const double dash_dir )
          {
              states_.push_back( State( ball_pos, self_pos, dash_power, dash_dir ) );
          }
        void add( const rcsc::Vector2D & ball_pos,
                  const rcsc::Vector2D & self_pos )
          {
              states_.push_back( State( ball_pos, self_pos ) );
          }
    };

    rcsc::GameTime M_update_time;
    int M_total_count;

    CooperativeAction::Ptr M_dash_only_best;
    CooperativeAction::Ptr M_kick_dashes_best;
    std::vector< CooperativeAction::Ptr > M_kick_dashes_candidates;

    std::vector< Candidate > M_candidates;

    std::vector< boost::shared_ptr< Result > > M_results;

    // private for singleton
    GeneratorKeepDribble();

    // not used
    GeneratorKeepDribble( const GeneratorKeepDribble & );
    GeneratorKeepDribble & operator=( const GeneratorKeepDribble & );
public:

    static
    GeneratorKeepDribble & instance();

    void generate( const rcsc::WorldModel & wm );


    CooperativeAction::Ptr getDashOnlyBest( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_dash_only_best;
      }

    CooperativeAction::Ptr getKickDashesBest( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_kick_dashes_best;
      }

    const std::vector< CooperativeAction::Ptr > & getKickDashesCandidates( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_kick_dashes_candidates;
      }

    const std::vector< Candidate > & candidates( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_candidates;
      }

private:

    void clear();
    void debugPrintResults( const rcsc::WorldModel & wm );

    void generateImpl( const rcsc::WorldModel & wm );

    bool simulateOneDash( const rcsc::WorldModel & wm,
                          const int dash_count,
                          const rcsc::Vector2D & ball_pos,
                          rcsc::Vector2D * self_pos,
                          rcsc::Vector2D * self_vel,
                          rcsc::StaminaModel * stamina_model,
                          double * result_dash_power,
                          double * result_dash_dir,
                          const rcsc::Matrix2D & rot_to_body );
    bool simulateDashes( const rcsc::WorldModel & wm );

    bool simulateKickDashes( const rcsc::WorldModel & wm );
    void simulateKick2Dashes( const rcsc::WorldModel & wm );

    CooperativeAction::Ptr simulateKickDashesImpl( const rcsc::WorldModel & wm,
                                                   const std::vector< rcsc::Vector2D > & self_cache,
                                                   const rcsc::Vector2D & first_self_pos,
                                                   const rcsc::Vector2D & first_ball_pos,
                                                   const rcsc::Vector2D & first_ball_rel_pos,
                                                   const rcsc::Vector2D & first_ball_rel_vel,
                                                   const size_t turn_step_after_kick,
                                                   const rcsc::Matrix2D & rotation_matrix );

    bool simulateTurnKickDashes( const rcsc::WorldModel & wm,
                                 const rcsc::AngleDeg & dash_angle );

    void simulateKickTurnsDashes( const rcsc::WorldModel & wm,
                                  const int n_turn,
                                  const rcsc::AngleDeg & dash_angle );
    void simulateCollideTurnKickDashes( const rcsc::WorldModel & wm,
                                        const rcsc::AngleDeg & dash_angle );

    void createSelfCacheRelative( const rcsc::PlayerType & ptype,
                                  const int kick_count,
                                  const int turn_count,
                                  const int dash_count,
                                  const rcsc::Vector2D & first_rel_vel,
                                  const rcsc::StaminaModel & first_stamina_model,
                                  std::vector< rcsc::Vector2D > & self_cache );
    void eraseIllegalSelfCache( const rcsc::Vector2D & first_self_pos,
                                const rcsc::Matrix2D & inverse_matrix,
                                std::vector< rcsc::Vector2D > & self_cache );

    CooperativeAction::SafetyLevel getSafetyLevel( const rcsc::WorldModel & wm,
                                                   const rcsc::Vector2D & ball_pos,
                                                   const int step );

};

#endif
