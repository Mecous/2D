
// -*-c++-*-

/*
 *Copyright:

 Copyright (C) opuSCOM

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

#ifndef POSITION_ANALYZER_H
#define POSITION_ANALYZER_H

//#include <rcsc/ann/bpn1.h>

#include "defensive_sirms_model.h"

#include <rcsc/geom/vector_2d.h>

#include <vector>


#define MIDDLE 10

struct train_data
{
  /*double ball_x;
    double ball_y;
    double ball_velx;
    double ball_vely;*/

    double self_x;
    double self_y;
    double self_velx;
    double self_vely;

    double enemy_x;
    double enemy_y;
    double enemy_velx;
    double enemy_vely;

    double reach_pos_x;
    double reach_pos_y;

};
/*
struct position_data
{
    int M_cycle;
    std::vector< rcsc::Vector2D >M_opponent_pos;
    rcsc::Vector2D ball_pos;

};
*/

class PositionAnalyzer {
private:
    //    typedef rcsc::BPNetwork1< 5, MIDDLE, 2 > Net;
    double M_pitch_length;
    double M_pitch_width;

    DefensiveSIRMsModel M_Sirms_x;
    DefensiveSIRMsModel M_Sirms_y;

    int M_num_input;
    int M_num_teacher;
    int M_num_output;


    double M_eta;
    double M_alpha;
    double M_loop;


    std::vector< std::vector< double > > M_train;

    std::vector< double > M_input_data;
    std::vector< double > M_teacher_data;
    std::vector< double > M_output_data;
    /*
    Net M_net;

    Net::input_array M_input;
    Net::output_array M_teacher;
    Net::output_array M_output;

    */
    void randomize();

    void normalization( const std::vector< double > & train_data );

    rcsc::Vector2D dinormalization( );
    //std::vector< double > dinormalization( );

public:

    PositionAnalyzer();

    //    std::vector< rcsc::Vector2D > M_result_pos;

    void addTrainData( std::vector< double > & tmp );


    bool executeTrain ();

    //    void calcResultPos ( int unum );

    rcsc::Vector2D evaluateAnalyzedPos ( rcsc::Vector2D ball_pos,
					 rcsc::Vector2D ball_vel,
					 rcsc::Vector2D self_pos,
					 rcsc::Vector2D self_vel,
					 rcsc::Vector2D enemy_pos,
					 rcsc::Vector2D enemy_vel,
					 rcsc::Vector2D mate_pos,
					 rcsc::Vector2D mate_vel );

  //std::vector< double > evaluateAnalyzedPos( std::vector< double > & train_data );

    bool printSirmParameter ();
    bool readSirmParameter ();

};

#endif
