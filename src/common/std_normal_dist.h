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

#ifndef STD_NORMAL_DIST_H
#define STD_NORMAL_DIST_H

#include <vector>

/*!
  \class StdNormalDist
  \brief standard normal distribution table
 */
class StdNormalDist {
private:

    std::vector< double > M_probability_densities; //!< probability density table
    std::vector< double > M_cumulative_densities; //!< cumulative density table


    /*!
      \brief primvate for singleton. create table instance.
     */
    StdNormalDist();

public:

    /*!
      \brief singleton interface.
      \return const reference to the singleton instance.
     */
    static
    const StdNormalDist & i();


    /*!
      \brief approximate probability density function for another normal distribution.
      \param standard_deviation standard deviation value of another distribution
      \param value input value for another distribution
      \return approximate probability density.
     */
    double pdf( const double standard_deviation,
                const double value ) const;

    /*!
      \brief approximate cumulative density function for another normal distribution.
      \param standard_deviation standard deviation value of another distribution
      \param value input value for another distribution
      \return approximate cumulative density.
     */
    double cdf( const double standard_deviation,
                const double value ) const;


};

#endif
