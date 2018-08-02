// -*-c++-*-

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

#ifndef RCSC_PLAYER_ACTION_STATE_PAIR_H
#define RCSC_PLAYER_ACTION_STATE_PAIR_H

#include <boost/shared_ptr.hpp>

#include <vector>

class CooperativeAction;
class PredictState;

/*!
  \class ActionStatePair
  \brief a pair of a cooperative action and predicted state after the action is performed.
*/
class ActionStatePair {
private:

    static int S_index_count;

    int M_index;

    boost::shared_ptr< const CooperativeAction > M_action; //!< action object
    boost::shared_ptr< const PredictState > M_state; //!< the predicted state after M_action is performed.
    double M_value;
    double M_penalty; //!< used for first action and its result state

    // not used
    ActionStatePair();

public:

    static void reset_index_count();

    /*!
      \brief copy constructor
     */
    ActionStatePair( const ActionStatePair & rhs )
        : M_index( rhs.M_index ),
          M_action( rhs.M_action ),
          M_state( rhs.M_state ),
          M_value( rhs.M_value ),
          M_penalty( rhs.M_penalty )
      { }

    /*!
      \brief substitution operatorc
     */
    ActionStatePair & operator=( const ActionStatePair & rhs )
      {
          if ( this != &rhs )
          {
              this->M_index = rhs.M_index;
              this->M_action = rhs.M_action;
              this->M_state = rhs.M_state;
              this->M_value = rhs.M_value;
              this->M_penalty = rhs.M_penalty;
          }
          return *this;
      }

    ActionStatePair( const CooperativeAction * action,
                     const PredictState * state )
        : M_index( ++S_index_count ),
          M_action( action ),
          M_state( state ),
          M_value( 0.0 ),
          M_penalty( 0.0 )
      { }

    ActionStatePair( const boost::shared_ptr< const CooperativeAction > & action,
                     const PredictState * state )
        : M_index( ++S_index_count ),
          M_action( action ),
          M_state( state ),
          M_value( 0.0 ),
          M_penalty( 0.0 )
      { }

    ActionStatePair( const CooperativeAction * action,
                     const boost::shared_ptr< const PredictState > & state )
        : M_index( ++S_index_count ),
          M_action( action ),
          M_state( state ),
          M_value( 0.0 ),
          M_penalty( 0.0 )
      { }

    ActionStatePair( const boost::shared_ptr< const CooperativeAction > & action,
                     const boost::shared_ptr< const PredictState > & state )
        : M_index( ++S_index_count ),
          M_action( action ),
          M_state( state ),
          M_value( 0.0 ),
          M_penalty( 0.0 )
      { }


    void setValue( const double value )
      {
          M_value = value;
      }


    void setPenalty( const double val )
      {
          M_penalty = val;
      }

    //

    int index() const
      {
          return M_index;
      }

    const CooperativeAction & action() const
      {
          return *M_action;
      }

    const PredictState & state() const
      {
          return *M_state;
      }

    double value() const
      {
          return M_value;
      }

    double penalty() const
      {
          return M_penalty;
      }

};

#endif
