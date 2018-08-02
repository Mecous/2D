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

#ifndef PASS_CHECKER_H
#define PASS_CHECKER_H

namespace rcsc {

class AbstractPlayerObject;
class Vector2D;

}

class PredictState;

/*!
  \class PassChecker
  \brief abstract pass checker that estimates pass success probability.
*/
class PassChecker {
protected:

    /*!
      \brief protected constructor.
     */
    PassChecker()
      { }

public:

    /*!
      \brief virtual destructor.
     */
    virtual
    ~PassChecker()
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
                       const int ball_step ) const = 0;
};

#endif
