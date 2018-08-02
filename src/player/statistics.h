// -*-c++-*-

/*!
  \file statistics.h
  \brief statistic data holder Header File
*/

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

#ifndef STATISTICS_H
#define STATISTICS_H

#include "periodic_callback.h"

#include <boost/cstdint.hpp>

class Statistics {
private:

    int M_total_action_search_count;
    boost::int64_t M_total_action_evaluate_size;
    int M_max_action_evaluate_size;
    int M_min_action_evaluate_size;
    double M_total_action_search_msec;
    double M_max_action_search_msec;

    //! callback functions called in handleActionStart()
    PeriodicCallback::Cont M_pre_action_callbacks;
    //! callback functions called in handleActionEnd()
    PeriodicCallback::Cont M_post_action_callbacks;

    // not used
    Statistics( const Statistics & );
    Statistics & operator=( const Statistics & );

    // private for singleton
    Statistics();

public:

    static
    Statistics & instance();

    //
    //
    //

    /*!
      \brief register pre-action callback
      \param ptr callback object
     */
    void addPreActionCallback( const PeriodicCallback::Ptr & ptr );

    /*!
      \brief register post-action callback
      \param ptr callback object
     */
    void addPostActionCallback( const PeriodicCallback::Ptr & ptr );


    void processPreActionCallbacks();
    void processPostActionCallbacks();

    //
    //
    //

    void setActionSearchData( const int evaluate_size,
                              const double elapsed_msec );


    int totalActionSearchCount() const { return M_total_action_search_count; }
    boost::int64_t totalActionSearchSize() const { return M_total_action_evaluate_size; }
    int maxActionSearchSize() const { return M_max_action_evaluate_size; }
    int minActionSearchSize() const { return M_min_action_evaluate_size; }
    double averageActionSearchSize() const
      {
          //return M_average_action_evaluate_size;
          return static_cast< double >( M_total_action_evaluate_size )
              /  static_cast< double >( M_total_action_search_count );
      }
    double maxActionSearchMSec() const { return M_max_action_search_msec; }
    double averageActionSearchMSec() const
      {
          return M_total_action_search_msec / M_total_action_search_count;
      }

};

#endif
