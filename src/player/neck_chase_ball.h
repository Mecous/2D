// -*-c++-*-

/*!
  \file neck_chase_ball.h
  \brief turn neck with attention to ball persistently
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

#ifndef NECK_CHASE_BALL_H
#define NECK_CHASE_BALL_H

#include <rcsc/player/soccer_action.h>

/*!
  \class Neck_ChaseBall
  \brief turn neck with attention to ball only
*/
class Neck_ChaseBall
    : public rcsc::NeckAction {

public:
    /*!
      \brief constructor
     */
    Neck_ChaseBall();

    /*!
      \brief do turn neck
      \param agent agent pointer to the agent itself
      \return always true
     */
    bool execute( rcsc::PlayerAgent * agent );

    /*!
      \brief create cloned object
      \return create pointer to the cloned object
    */
    rcsc::NeckAction * clone() const
      {
          return new Neck_ChaseBall();
      }
};


#endif
