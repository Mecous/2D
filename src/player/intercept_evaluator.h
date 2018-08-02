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

#ifndef INTERCEPT_EVALUATOR_H
#define INTERCEPT_EVALUATOR_H

#include <boost/shared_ptr.hpp>

namespace rcsc {
class InterceptInfo;
class WorldModel;
}

/*!
  \class InterceptEvaluator
  \brief abstract evaluator function class
 */
class InterceptEvaluator {
public:

    typedef boost::shared_ptr< InterceptEvaluator > Ptr; //! pointer type

    struct Evaluation {
        const rcsc::InterceptInfo * info_;
        double value_;
    };


protected:

    /*!
      \brief protected constructor
     */
    InterceptEvaluator()
      { }

public:

    /*!
      \brief virtual destructor
     */
    virtual
    ~InterceptEvaluator()
      { }


    /*!
      \brief pure virtual function.
      \return evaluation value
     */
    virtual
    double evaluate( const rcsc::WorldModel & wm,
                     const rcsc::InterceptInfo & action ) = 0;


};

#endif
