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

#include "opponent_coordination_analyzer.h"

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/coach/coach_agent.h>
#include <rcsc/coach/coach_world_model.h>

using namespace rcsc;

#define DEBUG_ANALYZE_COORDINATION

/*-------------------------------------------------------------------*/
/*!

 */
OpponentCoordinationAnalyzer::OpponentCoordinationAnalyzer()
{
    for ( int i = 0; i < 11; ++i )
    {
        for( int j = 0; j < 12; ++j )
        {
            M_coordination_matrix[i][j] = 0;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentCoordinationAnalyzer::analyze( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": HeliosCoach::analyzeOpponentCoordination()" );

    const CoachWorldModel & wm = agent->world();

    static bool s_last_nobody_owns_ball = false;

    const CoachPlayerObject * nearest_player = wm.getPlayerNearestTo( wm.ball().pos() );

    if ( ! nearest_player )
    {
        return false;
    }


    const int nearest_unum = nearest_player->unum();

    // update the coordination matrix
    if ( wm.gameMode().type() == GameMode::PlayOn
         && nearest_player->playerTypePtr()->kickableArea()
         > wm.ball().pos().dist( nearest_player->pos() )
         && nearest_player->side() == wm.theirSide() )
    {
        if( wm.lastKickerSide() == wm.theirSide()
            && s_last_nobody_owns_ball == true )
        {
#ifdef DEBUG_ANALYZE_COORDINATION
            Vector2D n_pos_1( nearest_player->pos().x - 0.2
                              , nearest_player->pos().y - 0.2 );
            Vector2D n_pos_2( nearest_player->pos().x + 0.2
                              , nearest_player->pos().y + 0.2 );
            Vector2D l_pos_1( wm.opponent( wm.lastKickerUnum() )->pos().x - 0.2
                              , wm.opponent( wm.lastKickerUnum() )->pos().y - 0.2 );
            Vector2D l_pos_2( wm.opponent( wm.lastKickerUnum() )->pos().x + 0.2
                              , wm.opponent( wm.lastKickerUnum() )->pos().y + 0.2 );
            agent->debugClient().addLine( n_pos_1, l_pos_1, "#0A0" );
            agent->debugClient().addLine( n_pos_2, l_pos_2, "#0A0" );
#endif

            M_coordination_matrix[wm.lastKickerUnum() - 1][nearest_unum - 1]++;
        }
    }
    else if ( nearest_player->playerTypePtr()->kickableArea()
              > wm.ball().pos().dist( nearest_player->pos() )
              && nearest_player->side() == wm.ourSide()
              && nearest_player->goalie() )  // our goalie took care of their shoot
    {
        if( wm.lastKickerSide() == wm.theirSide()
            && s_last_nobody_owns_ball == true )
        {
#ifdef DEBUG_ANALYZE_COORDINATION
            Vector2D n_pos_1( nearest_player->pos().x - 0.2
                              , nearest_player->pos().y - 0.2 );
            Vector2D n_pos_2( nearest_player->pos().x + 0.2
                              , nearest_player->pos().y + 0.2 );
            Vector2D l_pos_1( wm.opponent( wm.lastKickerUnum() )->pos().x - 0.2
                              , wm.opponent( wm.lastKickerUnum() )->pos().y - 0.2 );
            Vector2D l_pos_2( wm.opponent( wm.lastKickerUnum() )->pos().x + 0.2
                              , wm.opponent( wm.lastKickerUnum() )->pos().y + 0.2 );
            agent->debugClient().addLine( n_pos_1, l_pos_1, "#0A0" );
            agent->debugClient().addLine( n_pos_2, l_pos_2, "#0A0" );
#endif

            M_coordination_matrix[wm.lastKickerUnum() - 1][11]++;
        }
    }
    else if ( wm.gameMode().type() == GameMode::AfterGoal_
              && wm.gameMode().side() == wm.theirSide() )
    {
        dlog.addText( Logger::TEAM,
                      "%s:%d: wm.lastKickerUnum = %d, s_last_nobody_owns_ball = %d"
                      , __FILE__, __LINE__, wm.lastKickerUnum(), s_last_nobody_owns_ball );

        if( wm.lastKickerSide() == wm.theirSide()
            && s_last_nobody_owns_ball == true )
        {
            M_coordination_matrix[wm.lastKickerUnum() - 1][11]++;
        }
    }

    // update the ball handling status
    if ( nearest_player->playerTypePtr()->kickableArea()
         < wm.ball().pos().dist( nearest_player->pos() ) )
    {
        s_last_nobody_owns_ball = true;
    }
    else
    {
        s_last_nobody_owns_ball = false;
    }


    // display the coordination matrix
#if 0
    std::cout << std::endl;
    for ( int i = 0; i < 11; ++i )
    {
        std::cout << "unum[" << i << "] ";
        for ( int j = 0; j < 12; ++j )
        {
            std::cout << M_coordination_matrix[i][j] << " ";
        }
        std::cout << std::endl;
    }
#endif

    dlog.addText( Logger::TEAM,
                  "%s:%d: Coordination Matrix:"
                  , __FILE__, __LINE__ );
    for ( int i = 0; i < 11; ++i )
    {
        int sum = 0;
        std::string msg;
        msg.reserve( 128 );
        char buf[9];
        snprintf( buf, 9, "unum[%d] ", i + 1 );
        msg += buf;

        for ( int j = 0; j < 12; ++j )
        {
            char buf[5];
            snprintf( buf, 5, "%d ",
                      M_coordination_matrix[i][j] );
            msg += buf;
            sum += M_coordination_matrix[i][j];
        }
        dlog.addText( Logger::TEAM, "%s: %d", msg.c_str(), sum );
    }



    // currently message format is not determined
    // doSendFreeform( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentCoordinationAnalyzer::doSendFreeform( CoachAgent * agent )
{
    const CoachWorldModel & wm = agent->world();

    // send the opponent formation information in a freeform format
    if ( wm.time().cycle() > 200
         && agent->config().useFreeform()
         && wm.canSendFreeform()
         )
    {
        std::string msg;
        msg.reserve( 128 );

        msg = "(opponent_coordination ";

        msg += ")";

        //agent->doSayFreeform( msg );

        std::cout << agent->config().teamName()
                  << " Coach: "
                  << wm.time()
                  << " send freeform " << msg
                  << std::endl;
        agent->debugClient().addMessage( msg );
    }

    return true;
}

/*-------------------------------------------------------------------*/
