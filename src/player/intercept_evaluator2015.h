// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa Akiyama

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

#ifndef INTERCEPT_EVALUATOR_2015_H
#define INTERCEPT_EVALUATOR_2015_H

#include "intercept_evaluator.h"

#include <rcsc/geom/vector_2d.h>

/*!
  \class InterceptEvaluator2015
  \brief abstract evaluator function class
 */
class InterceptEvaluator2015
    : public InterceptEvaluator {
private:
    int M_count;
    const bool M_save_recovery;

public:

    explicit
    InterceptEvaluator2015( const bool save_recovery = true );

    ~InterceptEvaluator2015();


    virtual
    double evaluate( const rcsc::WorldModel & wm,
                     const rcsc::InterceptInfo & action );

private:

    void addShootSpotValue( const rcsc::Vector2D & ball_pos,
                            double * value );
    void addOpponentStepValue( const rcsc::WorldModel & wm,
                               const rcsc::InterceptInfo & action,
                               double * value );
    void addTeammateStepValue( const rcsc::WorldModel & wm,
                               const rcsc::InterceptInfo & action,
                               double * value );
    void addTurnMomentValue( const rcsc::WorldModel & wm,
                             const rcsc::InterceptInfo & action,
                             double * value );
    void addTurnPenalty( const rcsc::WorldModel & wm,
                         const rcsc::InterceptInfo & action,
                         double * value );
    void addMoveDistValue( const rcsc::WorldModel & wm,
                           const rcsc::InterceptInfo & action,
                           double * value );
    void addBallDistValue( const rcsc::WorldModel & wm,
                           const rcsc::InterceptInfo & action,
                           double * value );
    void addBallSpeedPenalty( const rcsc::WorldModel & wm,
                              const rcsc::InterceptInfo & action,
                              double * value );
};

#endif
