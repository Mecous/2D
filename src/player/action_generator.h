// -*-c++-*-

/*!
  \file action_generator.h
  \brief abstract action generator Header File
*/

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

#ifndef ACTION_GENERATOR_H
#define ACTION_GENERATOR_H

#include <boost/shared_ptr.hpp>

#include <vector>

namespace rcsc {
class WorldModel;
}

class ActionStatePair;
class PredictState;

/*!
  \class ActionGenerator
  \brief an abstract class to generate chain of action-state pair
*/
class ActionGenerator {
public:

    typedef boost::shared_ptr< ActionGenerator > Ptr;
    typedef boost::shared_ptr< const ActionGenerator > ConstPtr;

private:

    // not used
    ActionGenerator( const ActionGenerator & );
    ActionGenerator & operator=( const ActionGenerator & );

protected:

    ActionGenerator()
      { }

public:

    virtual
    ~ActionGenerator()
      { }

    /*!
      \brief genarate action-state pair
      \param result container variable to store the generated action-state pair
      \param state the last state to generate new action-state pair
      \param wm current world model as the initial state
      \param path chain of action-state pair from the initial state to the last state
     */
    virtual
    void generate( std::vector< ActionStatePair > * result,
                   const PredictState & state,
                   const rcsc::WorldModel & wm,
                   const std::vector< ActionStatePair > & path ) const = 0;
};

#endif
