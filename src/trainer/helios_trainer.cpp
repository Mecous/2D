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
#include "config.h"
#endif

#include "helios_trainer.h"

#include "options.h"

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <math.h>
#include <sstream>

#include <rcsc/trainer/trainer_command.h>
#include <rcsc/trainer/trainer_config.h>
#include <rcsc/coach/coach_world_model.h>
#include <rcsc/common/abstract_client.h>
#include <rcsc/common/player_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/server_param.h>
#include <rcsc/param/param_map.h>
#include <rcsc/param/cmd_line_parser.h>
#include <rcsc/random.h>

/*-------------------------------------------------------------------*/
/*!

 */
HeliosTrainer::HeliosTrainer()
    : TrainerAgent()
{

}

/*-------------------------------------------------------------------*/
/*!

*/
HeliosTrainer::~HeliosTrainer()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
HeliosTrainer::initImpl( rcsc::CmdLineParser & cmd_parser )
{
    bool result = rcsc::TrainerAgent::initImpl( cmd_parser );

#if 0
    rcsc::ParamMap my_params;

    std::string formation_conf;
    my_map.add()
        ( &conf_path, "fconf" )
        ;

    cmd_parser.parse( my_params );
#endif

    if ( cmd_parser.failed() )
    {
        std::cerr << "coach: ***WARNING*** detected unsupported options: ";
        cmd_parser.print( std::cerr );
        std::cerr << std::endl;
    }

    if ( ! result )
    {
        return false;
    }

    M_count = 0;
    M_time = 3100;
    M_isSetplay = true;

    const Options & opt = Options::i();

    if ( ! readSetplayCoordinate( opt.TestSetplayDir() ) )
    {
        std::cerr << "***ERROR*** Failed to read test setplay coordinate. directory=["
                  << opt.TestSetplayDir() << "]" << std::endl;
        return false;
    }

    //////////////////////////////////////////////////////////////////
    // Add your code here.
    //////////////////////////////////////////////////////////////////

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosTrainer::actionImpl()
{
    if ( world().teamNameLeft().empty() )
    {
        doTeamNames();
        return;
    }

    //////////////////////////////////////////////////////////////////
    // Add your code here.

    doSetplay();
    //sampleAction();
    //recoverForever();
    //doSubstitute();
    doKeepaway();
}


/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosTrainer::handleInitMessage()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosTrainer::handleServerParam()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosTrainer::handlePlayerParam()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosTrainer::handlePlayerType()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosTrainer::sampleAction()
{
    // sample training to test a ball interception.

    static int s_state = 0;
    static int s_wait_counter = 0;

    static rcsc::Vector2D s_last_player_move_pos;

    if ( world().existKickablePlayer() )
    {
        s_state = 1;
    }

    switch ( s_state ) {
    case 0:
        // nothing to do
        break;
    case 1:
        // exist kickable left player

        // recover stamina
        doRecover();
        // move ball to center
        doMoveBall( rcsc::Vector2D( 0.0, 0.0 ),
                    rcsc::Vector2D( 0.0, 0.0 ) );
        // change playmode to play_on
        doChangeMode( rcsc::PM_PlayOn );
        {
            // move player to random point
            rcsc::UniformReal uni01( 0.0, 1.0 );
            rcsc::Vector2D move_pos
                = rcsc::Vector2D::polar2vector( 15.0, //20.0,
                                                rcsc::AngleDeg( 360.0 * uni01() ) );
            s_last_player_move_pos = move_pos;

            doMovePlayer( config().teamName(),
                          1, // uniform number
                          move_pos,
                          move_pos.th() - 180.0 );
        }
        // change player type
        {
            static int type = 0;
            doChangePlayerType( world().teamNameLeft(), 1, type );
            type = ( type + 1 ) % rcsc::PlayerParam::i().playerTypes();
        }

        doSay( "move player" );
        s_state = 2;
        std::cout << "trainer: actionImpl init episode." << std::endl;
        break;
    case 2:
        ++s_wait_counter;
        if ( s_wait_counter > 3
             && ! world().playersLeft().empty() )
        {
            // add velocity to the ball
            //rcsc::UniformReal uni_spd( 2.7, 3.0 );
            //rcsc::UniformReal uni_spd( 2.5, 3.0 );
            rcsc::UniformReal uni_spd( 2.3, 2.7 );
            //rcsc::UniformReal uni_ang( -50.0, 50.0 );
            rcsc::UniformReal uni_ang( -10.0, 10.0 );
            rcsc::Vector2D velocity
                = rcsc::Vector2D::polar2vector( uni_spd(),
                                                s_last_player_move_pos.th()
                                                + uni_ang() );
            doMoveBall( rcsc::Vector2D( 0.0, 0.0 ),
                        velocity );
            s_state = 0;
            s_wait_counter = 0;
            std::cout << "trainer: actionImpl start ball" << std::endl;
        }
        break;

    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosTrainer::recoverForever()
{
    if ( world().playersLeft().empty() )
    {
        return;
    }

    if ( world().time().stopped() == 0
         && world().time().cycle() % 50 == 0 )
    {
        // recover stamina
        doRecover();
    }
#if 1
    if ( world().time().stopped() == 0
         && world().time().cycle() % 100 == 1
         && ! world().teamNameLeft().empty() )
    {
        static int type = 0;
        doChangePlayerType( world().teamNameLeft(), 1, type );
        type = ( type + 1 ) % rcsc::PlayerParam::i().playerTypes();
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosTrainer::doSubstitute()
{
    static bool s_substitute = false;
    if ( ! s_substitute
         && world().time().cycle() == 0
         && world().time().stopped() >= 10 )
    {
        std::cerr << "trainer " << world().time() << " team name = "
                  << world().teamNameLeft()
                  << std::endl;

        if ( ! world().teamNameLeft().empty() )
        {
            rcsc::UniformSmallInt uni( 0, rcsc::PlayerParam::i().ptMax() );
            doChangePlayerType( world().teamNameLeft(),
                                1,
                                uni() );

            s_substitute = true;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosTrainer::doKeepaway()
{
    if ( world().trainingTime() == world().time() )
    {
        std::cerr << "trainer: "
                  << world().time()
                  << " training time." << std::endl;
    }

}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosTrainer::doSetplay()
{

    static int s_state = 0;

    if ( world().existKickablePlayer() )
    {
        s_state = 1;
    }

    switch ( s_state )
    {
    case 0:
        // nothing to do
        break;
    case 1:
        // move ball and player

        if( world().theirTeamName() == "WrightEagle"
            && world().ball().pos().y * M_ball_pos[ M_count ][1] < 0
            && M_time <= world().time().cycle() - 45
            && !M_isSetplay )
            {
                doMoveBall( rcsc::Vector2D( M_ball_pos[ M_count ][0],
                                            M_ball_pos[ M_count ][1] ),
                            rcsc::Vector2D( 0.0, 0.0 ) );
                //break;
            }

        if( !M_isSetplay
            &&  M_time <= world().time().cycle() - 51 )
        {

            if( M_count  > (int)M_ball_pos.size() )
            {
                s_state = 0;
                break;
            }

            M_time = world().time().cycle();
            M_isSetplay = true;

            // change mode

            if( M_ball_pos[ M_count ][0] >= 50.9
                && std::fabs( M_ball_pos[ M_count ][1] ) >= 33.0 )
                doChangeMode( rcsc::PM_CornerKick_Left );
            else if( std::fabs( M_ball_pos[ M_count ][1] ) >= 34.0 )
                doChangeMode( rcsc::PM_KickIn_Left );
            else if ( M_ball_pos[ M_count ][0] >= 51.0
                      && std::fabs( M_ball_pos[ M_count ][1] ) <= 10.0 )
                doChangeMode( rcsc::PM_GoalKick_Left );
            else
                doChangeMode( rcsc::PM_Foul_Charge_Right );

            // recover stamina

            doRecover();
            // move ball

            doMoveBall( rcsc::Vector2D( M_ball_pos[ M_count ][0],
                                        M_ball_pos[ M_count ][1] ),
                        rcsc::Vector2D( 0.0, 0.0 ) );

            // move player

            for( int i = 1; i <= 11; ++i )
            {

                doMovePlayer( world().teamNameLeft(),
                              i, // uniform number
                              rcsc::Vector2D( M_mate_pos[ M_count ][i-1][0],
                                              M_mate_pos[ M_count ][i-1][1] )  );

                doMovePlayer( world().teamNameRight(),
                              i, // uniform number
                              rcsc::Vector2D( M_opp_pos[ M_count ][i-1][0],
                                              M_opp_pos[ M_count ][i-1][1] )  );

            }

            M_count += 1;

        }

        if( M_isSetplay
            && world().gameMode().type() == rcsc::GameMode::PlayOn )
        {
            M_time = world().time().cycle();
            M_isSetplay = false;
        }
        else if( world().gameMode().type() != rcsc::GameMode::PlayOn )
        {
            M_time = world().time().cycle();
        }

    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
HeliosTrainer::readSetplayCoordinate( const std::string & coordinate_dir )
{

    double x,y;
    int unum;
    char ball[5];

    std::vector< double > tmp;
    std::vector< std::vector< double > > pos_tmp;

    std::string our_coordinate_file = coordinate_dir + "our_coordinate.txt";
    std::string their_coordinate_file = coordinate_dir + "their_coordinate.txt";

    std::ifstream our_coordinate( our_coordinate_file.c_str() );
    if ( ! our_coordinate.is_open() )
    {
        std::cerr << __FILE__ << ':'
                  << "(readOurCoordinate)"
                  << " ***ERROR*** could not open the file ["
                  << our_coordinate_file << "]" << std::endl;
        return false;
    }

    std::ifstream their_coordinate( their_coordinate_file.c_str() );
    if ( ! their_coordinate.is_open() )
    {
        std::cerr << __FILE__ << ':'
                  << "(readTheirCoordinate)"
                  << " ***ERROR*** could not open the file ["
                  << their_coordinate_file << "]" << std::endl;
        return false;
    }

    std::string line_buf;
    while( std::getline( our_coordinate, line_buf ) )
    {
        if ( line_buf.empty()
             || line_buf[0] == '#')
        {
            continue;
        }

        if ( line_buf[0] == 'b')
        {
            if( std::sscanf( line_buf.data(),
                             "%4s %lf %lf",
                             ball, &x, &y ) != 3 )
            {
                std::cerr << __FILE__ << ':'
                          << "(readBallCoordinate)"
                          << " *** ERROR *** could not read the line "
                          << '[' << line_buf << ']' << std::endl;
                return false;
            }
            tmp.push_back( x );
            tmp.push_back( y );
            M_ball_pos.push_back( tmp );
            tmp.clear();
        }
        else
        {
            if( std::sscanf( line_buf.data(),
                             "%d %lf %lf",
                             &unum, &x, &y ) != 3 )
            {
                std::cerr << __FILE__ << ':'
                          << "(readOurCoordinate)"
                          << " *** ERROR *** could not read the line "
                          << '[' << line_buf << ']' << std::endl;
                return false;
            }
            tmp.push_back( x );
            tmp.push_back( y );
            pos_tmp.push_back( tmp );

            if( unum == 11 )
            {
                M_mate_pos.push_back( pos_tmp );
                pos_tmp.clear();
            }

            tmp.clear();
        }
    }

    while( std::getline( their_coordinate , line_buf ) )
    {
        if ( line_buf.empty()
             || line_buf[0] == '#'
             || line_buf[0] == 'b')
        {
            continue;
        }

        if( std::sscanf( line_buf.data(),
                     "%d %lf %lf",
                     &unum, &x, &y ) != 3 )
        {
            std::cerr << __FILE__ << ':'
                      << "(readTheirCoordinate)"
                      << " *** ERROR *** could not read the line "
                      << '[' << line_buf << ']' << std::endl;
            return false;
        }

        tmp.push_back( x );
        tmp.push_back( y );
        pos_tmp.push_back( tmp );
        if( unum == 11 )
        {
                M_opp_pos.push_back( pos_tmp );
                pos_tmp.clear();
        }
        tmp.clear();
    }

    return true;
}
