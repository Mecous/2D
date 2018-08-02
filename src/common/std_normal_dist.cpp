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

#include "std_normal_dist.h"

#include <boost/math/distributions/normal.hpp>

/*-------------------------------------------------------------------*/
/*!

 */
const StdNormalDist &
StdNormalDist::i()
{
    static StdNormalDist s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
StdNormalDist::StdNormalDist()
{
    boost::math::normal n( 0.0, 1.0 );

    for ( double x = 0.0; x < 5.0; x += 0.01 )
    {
        double c = boost::math::cdf( n, x );
        if ( c > 0.999 )
        {
            break;
        }

        M_probability_densities.push_back( boost::math::pdf( n, x ) );
        M_cumulative_densities.push_back( c );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
double
StdNormalDist::pdf( const double standard_deviation,
                    const double value ) const
{
#if 0
    //
    // need linear interpolation?
    //
    double v = value / standard_deviation * 100.0;
    double cv = std::ceil( v );
    double fv = std::floor( v );

    double crate = ( c - fv ) / ( cv - fv );

    size_t cidx = static_cast< size_t >( cv );
    size_t fidx = static_cast< size_t >( fv );

    double cpdf = ( cidx >= M_probabiity_densities.size()
                    ? 0.0
                    : M_probability_densities[cidx] );
    double fpdf = ( fidx >= M_probabiity_densities.size()
                    ? 0.0
                    : M_probability_densities[fidx] );

    return cpdf * crate + fpdf * ( 1.0 - crate );
#endif

    size_t idx = static_cast< size_t >( std::ceil( value / standard_deviation * 100.0 ) );

    if ( idx >= M_probability_densities.size() )
    {
        return 0.0;
    }

    return M_probability_densities[idx];
}

/*-------------------------------------------------------------------*/
/*!

 */
double
StdNormalDist::cdf( const double standard_deviation,
                    const double value ) const
{
#if 0
    //
    // need linear interpolation?
    //
    double v = value / standard_deviation * 100.0;
    double cv = std::ceil( v );
    double fv = std::floor( v );

    double crate = ( c - fv ) / ( cv - fv );

    size_t cidx = static_cast< size_t >( cv );
    size_t fidx = static_cast< size_t >( fv );

    double ccdf = ( cidx >= M_cumulative_densities.size()
                    ? 0.0
                    : M_cumulative_densities[cidx] );
    double fcdf = ( fidx >= M_cumulativedensities.size()
                    ? 0.0
                    : M_cumulative_densities[fidx] );

    return cpdf * crate + fpdf * ( 1.0 - crate );
#endif

    size_t idx = static_cast< size_t >( std::ceil( value / standard_deviation * 100.0 ) );

    if ( idx >= M_cumulative_densities.size() )
    {
        return 1.0;
    }

    return M_cumulative_densities[idx];
}
