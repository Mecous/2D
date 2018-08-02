// -*-c++-*-

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

#ifndef RCSC_PLAYER_FIELD_EVALUATOR_H
#define RCSC_PLAYER_FIELD_EVALUATOR_H

/////////////////////////////////////////////////////////////////////

#include <boost/shared_ptr.hpp>

#include <vector>

namespace rcsc {
class WorldModel;
}

class PredictState;
class ActionStatePair;

/*!
  \class FieldEvaluator
  \brief abstract field evaluator function object class
*/
class FieldEvaluator {
public:

    typedef boost::shared_ptr< FieldEvaluator > Ptr; //!< pointer type alias

protected:

    /*!
      \brief protected constructor to inhibit instantiation of this class
     */
    FieldEvaluator()
      { }

public:

    /*!
      \brief virtual destructor
     */
    virtual
    ~FieldEvaluator()
      { }

    virtual
    bool isValid() const
      {
          return true;
      }

    /*!
      \brief evaluation function
      \return evaluated value.
     */
    virtual
    double evaluate( const PredictState & first_state,
                     const std::vector< ActionStatePair > & path ) = 0;

    virtual
    double getFirstActionPenalty( const PredictState & /* first_state */,
                                  const ActionStatePair & /* first_pair */ )
      {
          return 0.0;
      }

    /*
      \brief write debug information to dlog
      \param evaluator field evaluator instance
      \param wm world model instance
     */
    void writeDebugLog( const rcsc::WorldModel & wm ) const;
};


#endif
