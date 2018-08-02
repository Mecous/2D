// -*-c++-*-

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

#ifndef SIMPLE_PASS_CHECKER_H
#define SIMPLE_PASS_CHECKER_H

#include "pass_checker.h"

class SimplePassChecker
    : public PassChecker {

public:
    /*!
      \brief constructor
    */
    SimplePassChecker()
      { }

    /*!
      \brief estimate success probability of specified pass
      \param state world model
      \param passer passer player
      \param receiver receiver player
      \param receive_point planned pass receive point
      \param first_ball_speed first ball speed
      \return success probability
    */
    virtual
    double operator()( const PredictState & state,
                       const rcsc::AbstractPlayerObject * passer,
                       const rcsc::AbstractPlayerObject * receiver,
                       const rcsc::Vector2D & receive_point,
                       const double & first_ball_speed,
                       const int ball_step ) const
      {
          return check( state, passer, receiver, receive_point, first_ball_speed, ball_step );
      }


    /*!
      \brief estimate success probability of specified pass (static method, called by operator())
      \param state world model
      \param passer passer player
      \param receiver receiver player
      \param receive_point planned pass receive point
      \param first_ball_speed first ball speed
      \return success probability
    */
    static
    double check( const PredictState & state,
                  const rcsc::AbstractPlayerObject * passer,
                  const rcsc::AbstractPlayerObject * receiver,
                  const rcsc::Vector2D & receive_point,
                  const double & first_ball_speed,
                  const int ball_step );

};

#endif
