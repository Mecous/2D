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

#ifndef FIELD_EVALUATOR_SVMRANK_H
#define FIELD_EVALUATOR_SVMRANK_H

#include "field_evaluator.h"
#include "predict_state.h"

#include <svmrank/svm_struct_api_types.h>

#include <boost/shared_ptr.hpp>

#include <vector>

namespace rcsc {
class AbstractPlayerObject;
class Vector2D;
}

class ActionStatePair;

class FieldEvaluatorSVMRank
    : public FieldEvaluator {
private:

    svmrank::STRUCTMODEL M_model;
    svmrank::STRUCT_LEARN_PARM M_learn_param;

public:
    FieldEvaluatorSVMRank();

    virtual
    ~FieldEvaluatorSVMRank();

    virtual
    bool isValid() const;

    virtual
    double evaluate( const PredictState & first_state,
                     const std::vector< ActionStatePair > & path );

    virtual
    double getFirstActionPenalty( const PredictState & first_state,
                                  const ActionStatePair & first_pair );

private:

    void writeRankData( const int unum,
                        const rcsc::GameTime & current,
                        const double value,
                        const std::vector< double > & features );

    void createFeatureVector( const PredictState & first_state,
                              const std::vector< ActionStatePair > & path,
                              std::vector< double > & features );
    svmrank::PATTERN convertTOSVMRankPattern( const std::vector< double > & features );

    void setFirstActionFeatures( const PredictState & first_state,
                                 const std::vector< ActionStatePair > & path,
                                 std::vector< double > & features );
    void setLastActionFeatures( const PredictState & first_state,
                                const std::vector< ActionStatePair > & path,
                                std::vector< double > & features );
    void setLastStateFeatures( const std::vector< ActionStatePair > & path,
                               std::vector< double > & features );
    void setSequenceFeatures( const std::vector< ActionStatePair > & path,
                              std::vector< double > & features );

    void debugWriteFeatures( const std::vector< double > & features );
};

#endif
