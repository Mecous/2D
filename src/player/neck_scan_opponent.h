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

#ifndef NECK_SCAN_OPPONENT_H
#define NECK_SCAN_OPPONENT_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/region_2d.h>

#include <boost/shared_ptr.hpp>

class ActionChainGraph;

/*!
  \brief trying turn_neck to the lowest accuracy opponent in the input region.
 */
class Neck_ScanOpponent
    : public rcsc::NeckAction {
private:

    boost::shared_ptr< rcsc::Region2D > M_region;
    rcsc::NeckAction::Ptr M_default_action;

public:

    Neck_ScanOpponent( boost::shared_ptr< rcsc::Region2D > region,
                       rcsc::NeckAction::Ptr default_neck );

    bool execute( rcsc::PlayerAgent * agent );

    rcsc::NeckAction * clone() const;
};

#endif
