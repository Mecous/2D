// -*-c++-*-

/*!
  \file statistics.cpp
  \brief statistic data holder Source File
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "statistics.h"

#include <algorithm>

/*-------------------------------------------------------------------*/
/*!

 */
Statistics::Statistics()
    : M_total_action_search_count( 0 ),
      M_max_action_evaluate_size( 0 ),
      M_min_action_evaluate_size( 10000000 ),
      M_total_action_search_msec( 0.0 ),
      M_max_action_search_msec( 0.0 )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
Statistics &
Statistics::instance()
{
    static Statistics s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Statistics::addPreActionCallback( const PeriodicCallback::Ptr & ptr )
{
    M_pre_action_callbacks.push_back( ptr );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Statistics::addPostActionCallback( const PeriodicCallback::Ptr & ptr )
{
    M_post_action_callbacks.push_back( ptr );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Statistics::processPreActionCallbacks()
{
    std::for_each( M_pre_action_callbacks.begin(),
                   M_pre_action_callbacks.end(),
                   &PeriodicCallback::call_execute );
    M_pre_action_callbacks.erase( std::remove_if( M_pre_action_callbacks.begin(),
                                                  M_pre_action_callbacks.end(),
                                                  &PeriodicCallback::is_finished ),
                                  M_pre_action_callbacks.end() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Statistics::processPostActionCallbacks()
{
    std::for_each( M_post_action_callbacks.begin(),
                   M_post_action_callbacks.end(),
                   &PeriodicCallback::call_execute );
    M_post_action_callbacks.erase( std::remove_if( M_post_action_callbacks.begin(),
                                                   M_post_action_callbacks.end(),
                                                   &PeriodicCallback::is_finished ),
                                   M_post_action_callbacks.end() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Statistics::setActionSearchData( const int evaluate_size,
                                 const double elapsed_msec )
{
    M_total_action_search_count += 1;
    M_total_action_evaluate_size += evaluate_size;

    if ( M_max_action_evaluate_size < evaluate_size )
    {
        M_max_action_evaluate_size = evaluate_size;
    }
    if ( M_min_action_evaluate_size > evaluate_size )
    {
        M_min_action_evaluate_size = evaluate_size;
    }

    M_total_action_search_msec += elapsed_msec;
    if ( M_max_action_search_msec < elapsed_msec )
    {
        M_max_action_search_msec = elapsed_msec;
    }

    // double r
    //     = static_cast< double >( M_total_action_search_count - 1 )
    //     / static_cast< double >( M_total_action_search_count );

    // M_average_action_search_size
    //     = r * M_average_action_search_size
    //     + size / static_cast< double >( M_total_action_search_count );
}
