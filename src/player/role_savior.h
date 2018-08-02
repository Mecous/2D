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

#ifndef HELIOS_ROLE_SAVIOR_H
#define HELIOS_ROLE_SAVIOR_H

#include "soccer_role.h"

#include <rcsc/geom/vector_2d.h>
#include <rcsc/math_util.h>

namespace rcsc {
class WorldModel;
class GameMode;
}

class RoleSavior
    : public SoccerRole {
private:

public:

    static const std::string NAME;

    RoleSavior()
      { }

    ~RoleSavior()
      {
          //std::cerr << "delete RoleSavior" << std::endl;
      }

    virtual
    const char * shortName() const
      {
          return "GK";
      }

    virtual
    bool execute( rcsc::PlayerAgent * agent );

    virtual
    bool acceptExecution( const rcsc::WorldModel & wm );


    static
    const std::string & name()
      {
          return NAME;
      }

    static
    SoccerRole::Ptr create()
      {
          SoccerRole::Ptr ptr( new RoleSavior() );
          return ptr;
      }
};


#endif
