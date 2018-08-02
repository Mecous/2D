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

#ifndef FIELD_EVALUATOR_2013_H
#define FIELD_EVALUATOR_2013_H

#include "field_evaluator.h"
#include "predict_state.h"

#include <boost/shared_ptr.hpp>

#include <vector>

namespace rcsc {
class AbstractPlayerObject;
class Vector2D;
}

class ActionStatePair;

class FieldEvaluator2013
    : public FieldEvaluator {
private:

public:
    FieldEvaluator2013();

    virtual
    ~FieldEvaluator2013();

    virtual
    double evaluate( const PredictState & first_state,
                     const std::vector< ActionStatePair > & path );

    virtual
    double getFirstActionPenalty( const PredictState & first_state,
                                  const ActionStatePair & first_pair );

private:

    double evaluateImpl( const PredictState & first_state,
                         const std::vector< ActionStatePair > & path ) const;
    double getFirstActionPenaltyImpl( const PredictState & first_state,
                                      const ActionStatePair & first_pair );


    double getStateValue( const std::vector< ActionStatePair > & path ) const;

    //
    // evaluation rules
    //
    double getBallPositionValue( const std::vector< ActionStatePair > & path ) const;
    double getShootChanceValue( const std::vector< ActionStatePair > & path ) const;
    double getFrontSpaceValue( const std::vector< ActionStatePair > & path ) const;
    double getOverDefenseLineValue( const std::vector< ActionStatePair > & path ) const;
    double getOpponentGoalDistanceValue( const std::vector< ActionStatePair > & path ) const;
    double getOurGoalDistanceValue( const std::vector< ActionStatePair > & path ) const;
    double getBallPositionForOurGoalieValue( const std::vector< ActionStatePair > & path ) const;
    double getOverAttackLineValue( const std::vector< ActionStatePair > & path ) const;
    double getCongestionValue( const std::vector< ActionStatePair > & path ) const;
    double getPassCountValue( const std::vector< ActionStatePair > & path ) const;
    //
    // additional penalty
    //

    double getLengthPenalty( const PredictState & first_state,
                             const std::vector< ActionStatePair > & path ) const;

    double getOpponentDistancePenalty( const std::vector< ActionStatePair > & path ) const;

    double getActionTypePenalty( const PredictState & first_state,
                                 const std::vector< ActionStatePair > & path ) const;

};

#endif
