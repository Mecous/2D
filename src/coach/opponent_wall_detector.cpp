// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Tomoharu NAKASHIMA

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

#include "opponent_wall_detector.h"
#include "coach_strategy.h"

#include "default_freeform_message.h"

#include <algorithm>
#include <vector>
#include <map>
#include <string>


#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/coach/coach_agent.h>
#include <rcsc/coach/coach_world_model.h>
#include <rcsc/timer.h>

using namespace rcsc;


//MAGIC numbers
#define STARTING_POINT 500
#define VAR_OFFSIDE_THRESHOLD 240.5
#define BALL_X_THRESHOLD 7.0
#define VAR_X_THRESHOLD 75.0
#define FRONT_OPPONENTS 3.5


/*-------------------------------------------------------------------*/
/*!

 */
OpponentWallDetector::OpponentWallDetector()
    : M_count_over_x( 0 ),
      M_count_play_on( 0 ),
      M_cycle( 0 ),
      M_stopped( 0 ),
      M_var_offside_integrate( 0 ),
      M_ball_x_integrate( 0 ),
      M_var_x_integrate( 0 ),
      M_count_front_opponent( 0 ),
      M_pre_defense_type( "unknown" )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentWallDetector::analyze( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "%s:%d: OpponentWallDetector::analyze",
                  __FILE__, __LINE__ );

    const CoachWorldModel & wm = agent->world();

    std::string defense_type;

    if ( resetParameters( agent ) )
    {
         std::cout << agent->config().teamName() << " coach: " << agent->world().time()
                   << " reset parameter" << std::endl;
    }


    if ( M_cycle != wm.time().cycle()
         && wm.gameMode().type() == GameMode::PlayOn )
    {
        doPlayOn( agent );
    }

    if ( M_count_play_on > STARTING_POINT
         && ( ( M_cycle != wm.time().cycle()
                && wm.gameMode().type() == GameMode::PlayOn )
              || ( M_stopped != wm.time().stopped()
                   && wm.gameMode().type() != GameMode::PlayOn ) ) )
    {
        defense_type = decideWall();

        if ( defense_type != M_pre_defense_type )
        {
            if ( doSendFreeform( agent ) )
            {
                M_pre_defense_type = defense_type;
            }
        }
    }

    M_cycle = wm.time().cycle();
    M_stopped = wm.time().stopped();

#if 0
    if ( M_count_play_on == 500 )
    {
        std::cout << "WallDetector [" << wm.time().cycle() << "]"
                  << std::endl
                  << "M_var_offside_integrate : " << M_var_offside_integrate
                  << std::endl
                  << "M_ball_x_integrate : " << M_ball_x_integrate
                  << std::endl
                  << "M_var_x_integrate : " << M_var_x_integrate
                  << std::endl
                  << "M_count_front_opponent : " << M_count_front_opponent
                  << std::endl;
    }
#endif

    return true;

}


/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentWallDetector::doPlayOn( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "OpponentWallDetector::doPlayOn" );

    const CoachWorldModel & wm = agent->world();

    ++M_count_play_on;

    calcBallX( agent );
    countFrontOpponent( agent );

    if ( wm.ball().pos().x > -25.0 )
    {
        ++M_count_over_x;
        calcVarOffside( agent );
        calcVarX( agent );
    }


    return true;

}

/*-------------------------------------------------------------------*/
/*!

 */

bool
OpponentWallDetector::resetParameters( CoachAgent * agent )
{

    dlog.addText( Logger::TEAM,
                  "OpponentWallDetector::resetParameter" );

    const CoachWorldModel & wm = agent->world();

    if ( wm.gameMode().type() == GameMode::AfterGoal_
         && M_stopped == 0 )
    {

        M_var_offside_integrate = 0.0;
        M_ball_x_integrate = 0.0;
        M_var_x_integrate = 0.0;
        M_count_front_opponent = 0.0;

        M_count_play_on = 0;
        M_count_over_x = 0;

        M_pre_defense_type = "unknown";

        doSendFreeform( agent );

        return true;

    }

    return false;


}









/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentWallDetector::calcBallX( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "OpponentWallDetector::calcBallX" );

    const CoachWorldModel & wm = agent->world();

    M_ball_x_integrate = ( M_ball_x_integrate * ( M_count_play_on - 1 ) + wm.ball().pos().x ) / M_count_play_on;

    /* test
    std::cout << "ball:"<< wm.ball().pos().x
              << "," << wm.ball().pos().y << std::endl;
    */

    return true;



}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentWallDetector::calcVarX( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "OpponentWallDetector::calcVarX" );

    const CoachWorldModel & wm = agent->world();

    int count_player = 0;

    double ave_x = 0.0;
    double tmp_value = 0.0;

    for ( int i = 0; i < 11; ++i )
    {
        const CoachPlayerObject * opp = wm.opponent( i + 1 );

        if ( !opp ) continue;
        if ( opp->goalie() ) continue;

        ave_x += opp->pos().x;
        count_player++;

    }

    ave_x /= count_player;

    for ( int i = 0; i < 11; ++i )
    {
        const CoachPlayerObject * opp = wm.opponent( i + 1 );

        if ( !opp ) continue;
        if ( opp->goalie() ) continue;

        tmp_value += ( opp->pos().x - ave_x ) * ( opp->pos().x - ave_x );

    }

    tmp_value /= (double)count_player;

    M_var_x_integrate = ( M_var_x_integrate * ( M_count_over_x - 1 ) + tmp_value ) / M_count_over_x;

    return true;
}





/*-------------------------------------------------------------------*/

/*!

 */


bool
OpponentWallDetector::calcVarOffside( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "OpponentWallDetector::calcVarOffside" );


    const CoachWorldModel & wm = agent->world();

    int count_player = 0;

    double tmp_value = 0.0;

    for ( int i = 0; i < 11; ++i )
    {
        const CoachPlayerObject * opp = wm.opponent( i + 1 );

        if ( !opp ) continue;
        if ( opp->goalie() ) continue;

        count_player++;

        tmp_value += ( opp->pos().x - wm.offsideLineXForLeft() ) * ( opp->pos().x - wm.offsideLineXForLeft() );

    }

    tmp_value = tmp_value / (double) count_player;

    M_var_offside_integrate = ( M_var_offside_integrate * ( M_count_over_x - 1 ) + tmp_value ) / M_count_over_x;
    //std::cout << M_var_offside_integrate << std::endl;

    return true;

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentWallDetector::countFrontOpponent( rcsc::CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "OpponentWallDetector::countFrontOpponent" );

    const CoachWorldModel & wm = agent->world();

    int count_front_opponent = 0;

    for ( int i = 0; i < 11; ++i )
    {
        const CoachPlayerObject * opp = wm.opponent( i + 1 );

        if ( !opp ) continue;
        if ( opp->goalie() ) continue;

        const Vector2D left_side( ServerParam::i().pitchHalfLength(), - ServerParam::i().pitchHalfWidth() );

        const Vector2D right_side( ServerParam::i().pitchHalfLength(), ServerParam::i().pitchHalfWidth() );

        const Triangle2D opp_wall_area( ( wm.ball().pos() ),
                                     left_side,
                                     right_side );

        if ( opp_wall_area.contains( opp->pos() ) ) ++count_front_opponent;

    }

    //std::cout << count_front_opponent << std::endl;

    M_count_front_opponent = ( M_count_front_opponent * ( M_count_play_on - 1 ) + count_front_opponent ) / M_count_play_on;


    return true;

}







/*-------------------------------------------------------------------*/
/*!

 */


std::string
OpponentWallDetector::decideWall()
{

    dlog.addText( Logger::TEAM,
                  "OpponentWallDetector::decideWall" );

    int wall_count = 0;

    if ( M_var_offside_integrate < VAR_OFFSIDE_THRESHOLD ) wall_count++;
    if ( M_ball_x_integrate > BALL_X_THRESHOLD ) wall_count++;
    if ( M_var_x_integrate < VAR_X_THRESHOLD ) wall_count++;
    if ( M_count_front_opponent > FRONT_OPPONENTS ) wall_count++;

    if ( wall_count > 2 ) return "wall";

    //else if ( wall_count > 0 ): return "others"

    return "default";


}



/*-------------------------------------------------------------------*/
/*!

 */


bool
OpponentWallDetector::doSendFreeform( CoachAgent * agent )
{

    /*
      format:
      "   "
      ->

      (say (freeform "(wd ...)" ) )



    */


    dlog.addText( Logger::TEAM,
                  __FILE__": OpponentWallDetector::doSendFreeform" );


    const CoachWorldModel & wm = agent->world();

    std::string defense_type;

    defense_type = decideWall();

    if ( agent->config().useFreeform()
         && wm.canSendFreeform()
         //&& ( wm.time().cycle() - M_cycle_last_sent > 500 )
         )
    {

        boost::shared_ptr< FreeformMessage > ptr( new OpponentWallDetectorMessage( defense_type ) );

        agent->addFreeformMessage( ptr );

        std::cout << agent->config().teamName() << " coach: " << agent->world().time()
                  << " send freeform ";

        std::cout << "opponent_wall " << std::endl;

        std::string msg;

        msg.reserve( 128 );
        ptr->append( msg );
        agent->debugClient().addMessage( msg );
    }
    else
    {
        return false;
    }

    return true;
}
