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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ball_move_model.h"

#include "std_normal_dist.h"

#include <iostream>
#include <fstream>
#include <cmath>

/*-------------------------------------------------------------------*/
/*!

*/
BallMoveModel::BallMoveModel()
{
    StdNormalDist::i();
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
BallMoveModel::init( const std::string & filepath )
{
    std::ifstream fin( filepath.c_str() );

    if ( ! fin.is_open() )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": ERROR could not read the ball table file. "
                  << filepath << std::endl;
        return false;
    }

    return read( fin );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
BallMoveModel::read( std::istream & is )
{
    M_standard_deviations.clear();

    if ( ! readHeader( is ) )
    {
        return false;
    }

    if ( ! readBody( is ) )
    {
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
BallMoveModel::readHeader( std::istream & is )
{
    std::string line_buf;
    while ( std::getline( is, line_buf ) )
    {
        if ( line_buf.empty()
             || line_buf[0] == '#'
             || line_buf[0] == ';' )
        {
            continue;
        }

        if ( line_buf.compare( 0, 5, "cycle", 5 ) != 0 )
        {
            std::cerr << __FILE__ << ' ' << __LINE__
                      << ": ERROR could not read the file header line ["
                      << line_buf << ']' << std::endl;
            return false;
        }

        break;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
BallMoveModel::readBody( std::istream & is )
{
    size_t current_speed_idx = 0;

    std::string line_buf;
    while ( std::getline( is, line_buf ) )
    {
        if ( line_buf.empty()
             || line_buf[0] == '#'
             || line_buf[0] == ';' )
        {
            continue;
        }

        if ( line_buf[0] == 'f' )
        {
            double first_speed = -1.0;
            if ( std::sscanf( line_buf.c_str(),
                              " firstSpeed %lf ",
                              &first_speed ) != 1 )
            {
                std::cerr << __FILE__ << ' ' << __LINE__
                          << ": ERROR could not read the firstSpeed line ["
                          << line_buf << ']' << std::endl;
                return false;
            }

            size_t idx = static_cast< size_t >( rint( first_speed * 10.0 ) );
            if ( idx != current_speed_idx + 1 )
            {
                std::cerr << __FILE__ << ' ' << __LINE__
                          << ": ERROR unexpected firstSpeed line ["
                          << line_buf << "] idx=" << idx << std::endl;
                return false;
            }

#if 1
            if ( ! M_standard_deviations.empty()
                 && M_standard_deviations.back().size() != 50 )
            {
                std::cerr << __FILE__ << ' ' << __LINE__
                          << ": ERROR illegal table size at index="
                          << current_speed_idx << std::endl;
                return false;
            }
#endif

            current_speed_idx = idx;
            M_standard_deviations.push_back( StdDev() );

            continue;
        }

        int step = 0;
        double theoretical_dist;
        double theoretical_max_dist;
        double average_dist;
        double standard_deviation;

        if ( std::sscanf( line_buf.c_str(),
                          " %d %lf %lf %lf %lf ",
                          &step,
                          &theoretical_dist, &theoretical_max_dist,
                          &average_dist, &standard_deviation ) != 5 )
        {
            std::cerr << __FILE__ << ' ' << __LINE__
                      << ": ERROR could not read the data line ["
                      << line_buf << ']' << std::endl;
            return false;
        }

        StdDev & stddev = M_standard_deviations.back();

        if ( (int)stddev.size() != step - 1 )
        {
            std::cerr << __FILE__ << ' ' << __LINE__
                      << ": ERROR unexpected step ["
                      << line_buf << ']' << std::endl;
            return false;
        }

        if ( standard_deviation < 0.0 )
        {
            std::cerr << __FILE__ << ' ' << __LINE__
                      << ": ERROR illegal standard deviation value ["
                      << line_buf << ']' << std::endl;
            return false;
        }

        stddev.push_back( standard_deviation );
    }

#if 1
    if ( ! M_standard_deviations.empty()
         && M_standard_deviations.back().size() != 50 )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": ERROR illegal table size at index="
                  << current_speed_idx << std::endl;
        return false;
    }
#endif

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
double
BallMoveModel::probability( const double first_speed,
                            const int step,
                            const double error_dist ) const
{
    if ( M_standard_deviations.empty() )
    {
        return 0.0;
    }

    size_t speed_idx = std::min( static_cast< size_t >( std::ceil( first_speed * 100.0 ) ) - 1,
                                 M_standard_deviations.size() - 1 );

    const StdDev & stddev = M_standard_deviations[speed_idx];

    size_t step_idx = std::min( static_cast< size_t >( step ) - 1,
                                stddev.size() - 1 );

    return 1.0 - StdNormalDist::i().cdf( stddev[step_idx], error_dist );
}
