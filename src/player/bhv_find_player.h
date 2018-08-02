// -*-c++-*-

/*!
  \file bhv_find_player.h
  \brief search specified player by body and neck
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

#ifndef BHV_FIND_PLAYER_H
#define BHV_FIND_PLAYER_H

#include <rcsc/player/soccer_action.h>

namespace rcsc {
class AbstractPlayerObject;
}

/*!
  \class Bhv_FindPlayer
  \brief search specified player by body and neck
*/
class Bhv_FindPlayer
    : public rcsc::SoccerBehavior {
public:
    static const int FORCE = -1;

private:
    //! target player to find
    const rcsc::AbstractPlayerObject * M_target_player;

    //! threshold of step counts to checking found or not
    const int M_count_threshold;

public:
    /*!
      \brief constructor
      \param target_player target player to find
      \param count_threshold threshold of step counts to checking found or not
     */
    Bhv_FindPlayer( const rcsc::AbstractPlayerObject * target_player,
                    const int count_threshold = 0 );

    /*!
      \brief do find player
      \param agent agent pointer to the agent itself
      \return true with action, false if target player already found
     */
    bool execute( rcsc::PlayerAgent * agent );
};

#endif
