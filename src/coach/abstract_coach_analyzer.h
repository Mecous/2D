// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Tomoharu NAKASHIMA, Hidehisa AKIYAMA

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

#ifndef ABSTRACT_COACH_ANALYZER_H
#define ABSTRACT_COACH_ANALYZER_H

#include <boost/shared_ptr.hpp>

namespace rcsc {
class CoachAgent;
}

class AbstractCoachAnalyzer {
public:

    typedef boost::shared_ptr< AbstractCoachAnalyzer > Ptr;

private:

protected:

    AbstractCoachAnalyzer()
      { }

public:

    virtual
    ~AbstractCoachAnalyzer()
      { }


    virtual
    bool analyze( rcsc::CoachAgent * agent ) = 0;

    virtual
    bool loadOpponentData()
      {
          return true;
      }

    virtual
    bool saveOpponentData()
      {
          return true;
      }

};

#endif
