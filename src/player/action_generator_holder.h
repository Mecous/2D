// -*-c++-*-

/*!
  \file action_generator_holder.h
  \brief action generator holder Header File
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

#ifndef ACTION_GENERATOR_HOLDER_H
#define ACTION_GENERATOR_HOLDER_H

#include "action_generator.h"

#include <vector>

namespace rcsc {
class WorldModel;
}

class ActionStatePair;
class PredictState;

/*!
  \class ActionGeneratorHolder
  \brief action generator holder
 */
class ActionGeneratorHolder {
public:

    typedef boost::shared_ptr< ActionGeneratorHolder > Ptr; //!< pointer type
    typedef boost::shared_ptr< const ActionGeneratorHolder > ConstPtr; //!< const pointer type

private:

    //! container of generator instance
    std::vector< ActionGenerator::ConstPtr > M_generators;

public:

    /*!
      \brief register new action genrator instance
      \param g new generator instance
     */
    void addGenerator( const ActionGenerator * g )
      {
          if ( g )
          {
              M_generators.push_back( ActionGenerator::ConstPtr( g ) );
          }
      }

    /*!
      \brief generate action-state pair
      \param result container variable to store the generated action-state pair
      \param state the last state to generate new action-state pair
      \param wm current world model as the initial state
      \param path chain of action-state pair from the initial state to the last state
     */
    void generate( std::vector< ActionStatePair > * result,
                   const PredictState & state,
                   const rcsc::WorldModel & wm,
                   const std::vector< ActionStatePair > & path ) const
      {
          for ( std::vector< ActionGenerator::ConstPtr >::const_iterator g = M_generators.begin();
                g != M_generators.end();
                ++g )
          {
              (*g)->generate( result, state, wm, path );
          }
      }
};

#endif
