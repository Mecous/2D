
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


#include "position_analyzer.h"


#include <iostream>
#include <fstream>
#include <cstdlib>
#include <iomanip>
#include <ctime>


#include <boost/random.hpp>


/*-------------------------------------------------------------------*/
/*!

 */

PositionAnalyzer::PositionAnalyzer()
{
    //  M_eta = 0.9;
    //  M_alpha = 0.3;
    M_loop = 20000;

    M_num_input = 16;
    M_num_teacher = 2;
    M_num_output = 2;

    M_pitch_length = 105.0 + 10.0;
    M_pitch_width = 68.0 + 10.0;

    size_t t1 = 0;
    M_Sirms_x.setModuleName( t1++, "ball_x" );
    M_Sirms_x.setModuleName( t1++, "ball_y" );
    M_Sirms_x.setModuleName( t1++, "ball_velx" );
    M_Sirms_x.setModuleName( t1++, "ball_vely" );
    M_Sirms_x.setModuleName( t1++, "self_x" );
    M_Sirms_x.setModuleName( t1++, "self_y" );
    M_Sirms_x.setModuleName( t1++, "self_velx" );
    M_Sirms_x.setModuleName( t1++, "self_vely" );
    M_Sirms_x.setModuleName( t1++, "enemy_x" );
    M_Sirms_x.setModuleName( t1++, "enemy_y" );
    M_Sirms_x.setModuleName( t1++, "enemy_velx" );
    M_Sirms_x.setModuleName( t1++, "enemy_vely" );
    M_Sirms_x.setModuleName( t1++, "mate_x" );
    M_Sirms_x.setModuleName( t1++, "mate_y" );
    M_Sirms_x.setModuleName( t1++, "mate_velx" );
    M_Sirms_x.setModuleName( t1++, "mate_vely" );
    size_t t2 = 0;
    M_Sirms_y.setModuleName( t2++, "ball_x" );
    M_Sirms_y.setModuleName( t2++, "ball_y" );
    M_Sirms_y.setModuleName( t2++, "ball_velx" );
    M_Sirms_y.setModuleName( t2++, "ball_vely" );
    M_Sirms_y.setModuleName( t2++, "self_x" );
    M_Sirms_y.setModuleName( t2++, "self_y" );
    M_Sirms_y.setModuleName( t2++, "self_velx" );
    M_Sirms_y.setModuleName( t2++, "self_vely" );
    M_Sirms_y.setModuleName( t2++, "enemy_x" );
    M_Sirms_y.setModuleName( t2++, "enemy_y" );
    M_Sirms_y.setModuleName( t2++, "enemy_velx" );
    M_Sirms_y.setModuleName( t2++, "enemy_vely" );
    M_Sirms_y.setModuleName( t2++, "mate_x" );
    M_Sirms_y.setModuleName( t2++, "mate_y" );
    M_Sirms_y.setModuleName( t2++, "mate_velx" );
    M_Sirms_y.setModuleName( t2++, "mate_vely" );

    //  Net M_net( M_alpha, M_eta );
    //  randomize();

}
/*-------------------------------------------------------------------*/
/*!

 */

void
PositionAnalyzer::addTrainData( std::vector< double > &tmp )
{
    M_train.push_back( tmp );
}


/*-------------------------------------------------------------------*/
/*!

 */


void
PositionAnalyzer::randomize()
{
    static boost::mt19937 gen( std::time( 0 ) );
    boost::uniform_real<> dst( -0.5, 0.5 );
    boost::variate_generator< boost::mt19937 &, boost::uniform_real<> >
        rng( gen, dst );

    //M_net.randomize( rng );



}


/*-------------------------------------------------------------------*/
/*!

 */

void
PositionAnalyzer::normalization( const std::vector< double > & train_data )
{
    M_input_data.resize( M_num_input, 0.0 );

    const size_t train_data_size = train_data.size();

    for ( size_t i = 0; i < train_data_size; ++i )
    {
        if ( i == train_data_size - 2 )
        {
            M_teacher_data.resize( M_num_teacher, 0.0 );
            M_teacher_data[0] = std::max( 0.0,
                                          std::min( train_data[i] / M_pitch_length + 0.5,
                                                    1.0 ) );
        }
        else if ( i == train_data_size - 1 )
        {
            M_teacher_data[1] = std::max( 0.0,
                                          std::min( train_data[i] / M_pitch_width + 0.5,
                                                    1.0 ) );
        }
        else if ( i % 4 == 0 )
        {
            M_input_data[i] = std::max( 0.0,
                                        std::min( train_data[i] / M_pitch_length + 0.5,
                                                  1.0 ) );
        }
        else if ( i % 4 == 1 )
        {
            M_input_data[i] = std::max( 0.0,
                                        std::min( train_data[i] / M_pitch_width + 0.5,
                                                  1.0 ) );
        }
        else
        {
            M_input_data[i] = std::max( 0.0,
                                        std::min( train_data[i] / 4.0 + 0.5,
                                                  1.0 ) );
            //std::cout << M_input_data[i] << std::endl;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */

#if 1
rcsc::Vector2D
PositionAnalyzer::dinormalization( )
{
    double result_x = ( M_output_data[0] - 0.5 ) * M_pitch_length;
    double result_y = ( M_output_data[1] - 0.5 ) * M_pitch_width;

    return rcsc::Vector2D( result_x, result_y );

}

#endif

/*-------------------------------------------------------------------*/
/*!

 */
#if 0
void
PositionAnalyzer::calcResultPos()
{
    //   M_result_pos.clear();

    std::ofstream ofs;
    ofs.open("data/place.csv");

    if ( M_train.empty() ) return;

    for( std::vector< train_data >::iterator it = M_train.begin();
         it != M_train.end();
         ++it )
    {

        rcsc::Vector2D place( it->agent_x, it->agent_y );
        M_output_data.resize( M_num_output, 0.0 );

        normalization( it );
        M_output_data[0] = M_Sirms_x.calculateOutput( M_input_data );
        M_output_data[1] = M_Sirms_y.calculateOutput( M_input_data );
        //M_net.propagate( M_input, M_output );

        rcsc::Vector2D result = dinormalization();

        ofs << place.x <<","<< place.y << "," << result.x << "," << result.y << std::endl;
        // M_result_pos.push_back( tmp );
    }


}
#endif
/*-------------------------------------------------------------------*/
/*!

 */
bool
PositionAnalyzer::executeTrain()
{
    std::ofstream ofs;

    ofs.open("result.csv");

    if ( M_train.empty() ) return false;

    for ( int n = 0; n < M_loop; ++n )
    {
        double error = 0.0;

        double max_error = 0.0;
        double min_error = 1000000.0;
        double ave_error = 0.0;

        for( std::vector< std::vector< double > >::iterator it = M_train.begin();
             it != M_train.end();
             ++it )
        {
            normalization( *it );
            M_output_data.resize( M_num_output, 0.0 );


            double pre_output_x = M_Sirms_x.calculateOutput( M_input_data );
            double pre_output_y = M_Sirms_y.calculateOutput( M_input_data );
            M_Sirms_x.train( M_teacher_data[0], pre_output_x );
            M_Sirms_y.train( M_teacher_data[1], pre_output_y );

            M_output_data[0] = M_Sirms_x.calculateOutput( M_input_data );
            M_output_data[1] = M_Sirms_y.calculateOutput( M_input_data );
            //std::cout << M_input_data[11] << std::endl;


            double e = 0.0;
            e += std::pow( ( M_output_data[0] - M_teacher_data[0] ) * M_pitch_length , 2 );
            e += std::pow( ( M_output_data[1] - M_teacher_data[1] ) * M_pitch_width , 2 );

            e = std::pow( e, 0.50 );


            if( e > max_error ) max_error = e;
            if( e < min_error ) min_error = e;

            error += e;
        }

        int count = M_train.size();

        error *= 1.0;
        ave_error = error / count ;

        ofs << std::setprecision(14) << error <<","<< max_error
            <<","<< min_error << "," << ave_error <<  std::endl;

        std::cout << n << std::endl;
    }

    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
#if 1
rcsc::Vector2D
PositionAnalyzer::evaluateAnalyzedPos( rcsc::Vector2D ball_pos,
                                       rcsc::Vector2D ball_vel,
                                       rcsc::Vector2D self_pos,
                                       rcsc::Vector2D self_vel,
                                       rcsc::Vector2D enemy_pos,
                                       rcsc::Vector2D enemy_vel,
                                       rcsc::Vector2D mate_pos,
                                       rcsc::Vector2D mate_vel )

{
    M_input_data.resize( M_num_input, 0.0 );


    M_input_data[0] = std::max( 0.0,
                                std::min( ball_pos.x / M_pitch_length + 0.5,
                                          1.0 ) );

    M_input_data[1] = std::max( 0.0,
                                std::min( ball_pos.y / M_pitch_width + 0.5,
                                          1.0 ) );

    M_input_data[2] = std::max( 0.0,
                                std::min( ball_vel.x / M_pitch_width + 0.5,
                                          1.0 ) );

    M_input_data[3] = std::max( 0.0,
                                std::min( ball_vel.y / M_pitch_width + 0.5,
                                          1.0 ) );

    M_input_data[4] = std::max( 0.0,
                                std::min( self_pos.x / M_pitch_length + 0.5,
                                          1.0 ) );

    M_input_data[5] = std::max( 0.0,
                                std::min( self_pos.y / M_pitch_width + 0.5,
                                          1.0 ) );

    M_input_data[6] = std::max( 0.0,
                                std::min( self_vel.x / M_pitch_width + 0.5,
                                          1.0 ) );

    M_input_data[7] = std::max( 0.0,
                                std::min( self_vel.y / M_pitch_width + 0.5,
                                          1.0 ) );

    M_input_data[8] = std::max( 0.0,
                                std::min( enemy_pos.x / M_pitch_length + 0.5,
                                          1.0 ) );

    M_input_data[9] = std::max( 0.0,
                                std::min( enemy_pos.y / M_pitch_width + 0.5,
                                          1.0 ) );

    M_input_data[10] = std::max( 0.0,
                                 std::min( enemy_vel.x / M_pitch_width + 0.5,
                                           1.0 ) );

    M_input_data[11] = std::max( 0.0,
                                 std::min( enemy_vel.y / M_pitch_width + 0.5,
                                           1.0 ) );

    M_input_data[12] = std::max( 0.0,
                                 std::min( mate_pos.x / M_pitch_length + 0.5,
                                           1.0 ) );

    M_input_data[13] = std::max( 0.0,
                                 std::min( mate_pos.y / M_pitch_width + 0.5,
                                           1.0 ) );

    M_input_data[14] = std::max( 0.0,
                                 std::min( mate_vel.x / M_pitch_width + 0.5,
                                           1.0 ) );

    M_input_data[15] = std::max( 0.0,
                                 std::min( mate_vel.y / M_pitch_width + 0.5,
                                           1.0 ) );




    M_output_data.resize( M_num_output, 0.0 );
    //readSirmParameter();
    M_output_data[0] = M_Sirms_x.calculateOutput( M_input_data );
    M_output_data[1] = M_Sirms_y.calculateOutput( M_input_data );

    //M_net.propagate( M_input, M_output );
    rcsc::Vector2D result = dinormalization();
    //std::cout << M_output_data[0] << std::endl;
    // return result.dist( real_agent_pos );
    return result;
}
#endif
/*-------------------------------------------------------------------*/
/*!

 */

bool
PositionAnalyzer::printSirmParameter ()
{
    /*
      std::ofstream ofs;
      switch ( unum )
      {
      case 1:
      ofs.open("data/bpn_evaluator/neural1.txt");
      break;
      case 2:
      ofs.open("data/bpn_evaluator/neural2.txt");
      break;
      case 3:
      ofs.open("data/bpn_evaluator/neural3.txt");
      break;
      case 4:
      ofs.open("data/bpn_evaluator/neural4.txt");
      break;
      case 5:
      ofs.open("data/bpn_evaluator/neural5.txt");
      break;
      case 6:
      ofs.open("data/bpn_evaluator/neural6.txt");
      break;
      case 7:
      ofs.open("data/bpn_evaluator/neural7.txt");
      break;
      case 8:
      ofs.open("data/bpn_evaluator/neural8.txt");
      break;
      case 9:
      ofs.open("data/bpn_evaluator/neural9.txt");
      break;
      case 10:
      ofs.open("data/bpn_evaluator/neural10.txt");
      break;
      case 11:
      ofs.open("data/bpn_evaluator/neural11.txt");
      break;
      default:
      return false;
      }
    */
    std::string dirpath1("data/sirm_evaluator/sirm_x");
    std::string dirpath2("data/sirm_evaluator/sirm_y");

    M_Sirms_x.saveParameters( dirpath1 );
    M_Sirms_y.saveParameters( dirpath2 );
    // M_net.print( ofs );


    return true;
}



/*-------------------------------------------------------------------*/
/*!

 */

bool
PositionAnalyzer::readSirmParameter ()
{
    /*
      std::ifstream ifs;
      switch ( unum )
      {
      case 1:
      ifs.open("data/bpn_evaluator/neural1.txt");
      break;
      case 2:
      ifs.open("data/bpn_evaluator/neural2.txt");
      break;
      case 3:
      ifs.open("data/bpn_evaluator/neural3.txt");
      break;
      case 4:
      ifs.open("data/bpn_evaluator/neural4.txt");
      break;
      case 5:
      ifs.open("data/bpn_evaluator/neural5.txt");
      break;
      case 6:
      ifs.open("data/bpn_evaluator/neural6.txt");
      break;
      case 7:
      ifs.open("data/bpn_evaluator/neural7.txt");
      break;
      case 8:
      ifs.open("data/bpn_evaluator/neural8.txt");
      break;
      case 9:
      ifs.open("data/bpn_evaluator/neural9.txt");
      break;
      case 10:
      ifs.open("data/bpn_evaluator/neural10.txt");
      break;
      case 11:
      ifs.open("data/bpn_evaluator/neural11.txt");
      break;
      default:
      return false;
      }
    */
    std::string dirpath1("data/sirms_for_one-to-one/sirm_evaluator/sirm_x");
    std::string dirpath2("data/sirms_for_one-to-one/sirm_evaluator/sirm_y");
    //std::string dirpath1("/home/scom/rcss/helios_for_work/src/player/data/sirm_evaluator/sirm_x");
    //std::string dirpath2("/home/scom/rcss/helios_for_work/src/player/data/sirm_evaluator/sirm_y");

    M_Sirms_x.loadParameters( dirpath1 );
    M_Sirms_y.loadParameters( dirpath2 );
    // M_net.read( ifs );


    return true;
}
