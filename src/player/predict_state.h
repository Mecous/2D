// -*-c++-*-

/*!
  \file predict_state.h
  \brief predicted field state class Header File
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA, Hidehisa AKIYAMA

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

#ifndef RCSC_PLAYER_PREDICT_STATE_H
#define RCSC_PLAYER_PREDICT_STATE_H

#include "predict_player_object.h"
#include "predict_ball_object.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/abstract_player_object.h>
#include <rcsc/player/player_predicate.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/vector_2d.h>

#include <boost/shared_ptr.hpp>

#include <algorithm>

class PredictState {
public:
    static const int VALID_PLAYER_THRESHOLD;

    typedef boost::shared_ptr< PredictState > Ptr; //!< pointer type alias
    typedef boost::shared_ptr< const PredictState > ConstPtr; //!< const pointerp type alias

private:
    rcsc::GameMode M_game_mode;
    rcsc::GameTime M_current_time;
    unsigned long M_spent_time;

    PredictBallObject M_ball;

    PredictPlayerObject::ConstPtr M_self;
    PredictPlayerObject::Cont M_our_players;
    PredictPlayerObject::Cont M_their_players;

    PredictPlayerObject::ConstPtr M_ball_holder;

    double M_offside_line_x;
    double M_our_defense_line_x;
    double M_our_offense_player_line_x;
    double M_their_defense_player_line_x;

    // not used
    PredictState & operator=( const PredictState & );

public:


    /*!
      \brief The default constructor creates an empty state.

      \todo  The default constructor is used only by FieldEvaluatorPrinter. This should be set to private.
     */
    PredictState();

    /*!
      \brief The copy constructor.
      \param rhs source instance.

      \todo  The copy constructor is used only by FieldEvaluatorPrinter. This should be set to private.
     */
    PredictState( const PredictState & rhs );

    /*!
      \brief This constructor is used to create the root state of the search tree.
      \param wm current world model instance.
     */
    explicit
    PredictState( const rcsc::WorldModel & wm );

    /*!
      \brief This constructor is used to generate Hold actions.
      \param rhs previous state
      \param append_spent_time expected duration steps to perform the action
     */
    PredictState( const PredictState & rhs,
                  const unsigned long append_spent_time );

    /*!
      \brief This constructor is used to generate Pass or Dribble type actions.
      \param rhs previous state
      \param append_spent_time expected duration steps to perform the action
      \param ball_holder_unum the expected next ball holder.
      \param ball_and_holder_pos the expected ball receive point.
     */
    PredictState( const PredictState & rhs,
                  const unsigned long append_spent_time,
                  const int ball_holder_unum,
                  const rcsc::Vector2D & ball_and_holder_pos );

    /*!
      \brief This constructor is used to generate Clear or Shoot type actions. The ball holder is not changed.
      \param rhs previous state
      \param append_spent_time expected duration steps that the ball reaches the target point.
      \param ball_pos predicted ball position
     */
    PredictState( const PredictState & rhs,
                  const unsigned long append_spent_time,
                  const rcsc::Vector2D & ball_pos );

    //
    //
    //

    /*!
      \brief update the ball holder and offense/defense lines
     */
    void update();
private:
    void updateBallHolder();
    void predictPlayerPositions();
    void updateLines();
public:
    void setGameMode( const rcsc::GameMode & mode );
    void setCurrentTime( const rcsc::GameTime & time );
    void setSpentTime( const unsigned long spent_time );

    void setBall( const rcsc::Vector2D & pos,
                  const rcsc::Vector2D & vel );
    void setBall( const rcsc::Vector2D & pos );

    void setSelf( PredictPlayerObject::Ptr ptr );
    void setSelfPos( const rcsc::Vector2D & pos );

    void setPlayer( PredictPlayerObject::Ptr ptr );
private:
    void setTeammate( PredictPlayerObject::Ptr ptr );
    void setOpponent( PredictPlayerObject::Ptr ptr );
    void setUnknownPlayer( PredictPlayerObject::Ptr ptr );
public:
    void setPlayerPos( const rcsc::SideID side,
                       const int unum,
                       const rcsc::Vector2D & pos );
    void setBallHolderUnum( const int ball_holder_unum );

    //
    //
    //

    const rcsc::GameMode & gameMode() const { return M_game_mode; }
    const rcsc::GameTime & currentTime() const { return M_current_time; }
    unsigned long spentTime() const { return M_spent_time; }

    const PredictBallObject & ball() const { return M_ball; }
    const rcsc::AbstractPlayerObject & self() const { return *M_self; }
    const PredictPlayerObject::Cont & ourPlayers() const { return M_our_players; }
    const PredictPlayerObject::Cont & theirPlayers() const { return M_their_players; }
    const rcsc::AbstractPlayerObject & ballHolder() const { return *M_ball_holder; }
    const rcsc::AbstractPlayerObject * ourPlayer( const int unum ) const;
    const rcsc::AbstractPlayerObject * theirPlayer( const int unum ) const;
    const rcsc::AbstractPlayerObject * getOurGoalie() const;

    const rcsc::AbstractPlayerObject * getPlayerNearestTo( const PredictPlayerObject::Cont & players,
                                                           const rcsc::Vector2D & point,
                                                           const int count_thr,
                                                           double * dist_to_point ) const;
    const rcsc::AbstractPlayerObject * getOurPlayerNearestTo( const rcsc::Vector2D & point,
                                                              const int count_thr,
                                                              double * dist_to_point ) const
      {
          return getPlayerNearestTo( M_our_players, point, count_thr, dist_to_point );
      }

    const rcsc::AbstractPlayerObject * getOpponentNearestTo( const rcsc::Vector2D & point,
                                                             const int count_thr,
                                                             double * dist_to_point ) const
      {
          return getPlayerNearestTo( M_their_players, point, count_thr, dist_to_point );
      }

    rcsc::AbstractPlayerObject::Cont getPlayers( const rcsc::PlayerPredicate * predicate ) const;

    void getPlayers( rcsc::AbstractPlayerObject::Cont & cont,
                     const rcsc::PlayerPredicate * predicate ) const;

    double offsideLineX() const { return M_offside_line_x; }
    double ourDefenseLineX() const { return M_our_defense_line_x; }
    double ourOffensePlayerLineX() const { return M_our_offense_player_line_x; }
    double theirDefensePlayerLineX() const { return M_their_defense_player_line_x; }

    std::ostream & print( std::ostream & os ) const;
};

#endif
