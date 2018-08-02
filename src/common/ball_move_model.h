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

#ifndef BALL_MOVE_MODEL_H
#define BALL_MOVE_MODEL_H

#include <string>
#include <vector>

/*!
  \class BallMoveModel
  \brief ball movement model
 */
class BallMoveModel {
private:

    //! standard deviation table
    typedef std::vector< double > StdDev;

    //! table for each first speed
    std::vector< StdDev > M_standard_deviations;

public:

    /*!
      \brief default constructor.
     */
    BallMoveModel();


    /*!
      \brief read table data from file.
      \param filepath input file path
      \return result.
     */
    bool init( const std::string & filepath );

private:

    /*!
      \brief read data from input stream
      \param is reference to the input stream
      \return result status
     */
    bool read( std::istream & is );

    /*!
      \brief read header from input stream
      \param is reference to the input stream
      \return result status
     */
    bool readHeader( std::istream & is );

    /*!
      \brief read data tables from input stream
      \param is reference to the input stream
      \return result status
     */
    bool readBody( std::istream & is );

    /*!
      \brief read data table from input stream
      \param is reference to the input stream
      \param first_speed
      \return result status
     */
    bool readTable( std::istream & is,
                    const double first_speed );

public:

    /*!
      \brief get the probability that ball is located beyond the distance from the theoretical position.
      \param first_speed ball first speed
      \param step considered step value
      \param error_dist distance from the theoretical position.
     */
    double probability( const double first_speed,
                        const int step,
                        const double error_dist ) const;

};

#endif
