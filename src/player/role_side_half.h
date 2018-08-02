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

#ifndef AGENT2D_ROLE_SIDE_HALF_H
#define AGENT2D_ROLE_SIDE_HALF_H

#include "soccer_role.h"

class RoleSideHalf
    : public SoccerRole {
private:

public:

    static const std::string NAME;

    RoleSideHalf()
      { }

    ~RoleSideHalf()
      { }

    virtual
    const char * shortName() const
      {
          return "SH";
      }

    virtual
    bool execute( rcsc::PlayerAgent * agent );

    static
    const std::string & name()
      {
          return NAME;
      }

    static
    SoccerRole::Ptr create()
      {
          SoccerRole::Ptr ptr( new RoleSideHalf() );
          return ptr;
      }
private:

    void doKick( rcsc::PlayerAgent * agent );
    void doMove( rcsc::PlayerAgent * agent );

public:

    static
    bool do_get_ball_2013( rcsc::PlayerAgent * agent );

};


#endif
