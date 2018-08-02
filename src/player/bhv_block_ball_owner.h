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

#ifndef HELIOS_BHV_BLOCK_BALL_OWNER_H
#define HELIOS_BHV_BLOCK_BALL_OWNER_H

#include <boost/scoped_ptr.hpp>

#include <rcsc/geom/vector_2d.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/player/soccer_action.h>

namespace rcsc {
class PlayerObject;
class WorldModel;
}

// added 2008-07-06

class Bhv_BlockBallOwner
    : public rcsc::SoccerBehavior {
private:

    static int S_wait_count;

    // TODO: change to Region2D
    boost::scoped_ptr< rcsc::Rect2D > M_bounding_region;

public:
    explicit
    Bhv_BlockBallOwner( rcsc::Rect2D * bounding_region )
        : M_bounding_region( bounding_region )
      { }

    bool execute( rcsc::PlayerAgent * agent );

private:

    rcsc::Vector2D getBlockPoint( const rcsc::WorldModel & wm,
                                  const rcsc::PlayerObject * opponent );

    bool doBodyAction( rcsc::PlayerAgent * agent,
                       const rcsc::PlayerObject * opponent,
                       const rcsc::Vector2D & block_point );
    /*!
      \brief check if the target opponent is blocked by other teammate
      \param opponent target teammate
      \return true if target opponent is blocked by other teammate
     */
    bool opponentIsBlocked( const rcsc::WorldModel & wm,
                            const rcsc::PlayerObject * opponent,
                            const rcsc::Vector2D & block_point );
};

#endif
