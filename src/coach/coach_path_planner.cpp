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

#include "coach_path_planner.h"

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/coach/coach_agent.h>
#include <rcsc/coach/coach_world_model.h>

using namespace rcsc;

#define DEBUG_PRINT
// #define DEBUG_DRAW_PLAYER_CHAINS

#define NUMBER_OF_NEIGHBORS 3

/*-------------------------------------------------------------------*/
/*!

 */
CoachPathPlanner::CoachPathPlanner()
{
    M_path.resize( NUMBER_OF_NEIGHBORS );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
CoachPathPlanner::analyze( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__": (analyze)" );

    const CoachWorldModel & wm = agent->world();

    M_path.clear();

    if ( wm.gameMode().type() == GameMode::PlayOn )
    {
        doPlayOn( agent );
    }
    else if ( wm.gameMode().isTheirSetPlay( wm.ourSide() ) )
    {
        doSetPlayTheirBall( agent );
    }
    else if ( wm.gameMode().isOurSetPlay( wm.ourSide() ) )
    {
        doSetPlayOurBall( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
CoachPathPlanner::doPlayOn( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__": (doPlayOn)" );

    const CoachWorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    int our_unum_nearest_to_ball = Unum_Unknown;
    double min_dist2 = std::pow( 10.0 * ( SP.pitchLength() + SP.pitchWidth() ), 2.0 );

    const CoachPlayerObject * fastest_player = wm.currentState().fastestInterceptPlayer();
    Vector2D intercept_point = wm.ball().pos();
    if ( fastest_player )
    {
        intercept_point = wm.ball().inertiaPoint( fastest_player->ballReachStep() );
    }

    for ( CoachPlayerObject::Cont::const_iterator it = wm.teammates().begin();
          it != wm.teammates().end();
          ++it )
    {
        const CoachPlayerObject * our_player = *it;
        if ( ! our_player ) continue;
        if ( our_player->goalie() ) continue;

        double dist2 = our_player->pos().dist2( intercept_point );
        if ( dist2 < min_dist2 )
        {
            min_dist2 = dist2;
            our_unum_nearest_to_ball = our_player->unum();
        }
    }

    std::vector< int > first_path;
    first_path.push_back( our_unum_nearest_to_ball );
    M_path.push_back( first_path );

    appendNextLevelMates( agent );
    appendNextLevelMates( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
CoachPathPlanner::doSetPlayTheirBall( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__": (doSetPlayTheirBall)" );

    const CoachWorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    int our_unum_nearest_to_ball = Unum_Unknown;
    double min_dist2 = std::pow( 10.0 * ( SP.pitchLength() + SP.pitchWidth() ), 2.0 );

    for ( CoachPlayerObject::Cont::const_iterator it = wm.teammates().begin();
          it != wm.teammates().end();
          ++it )
    {
        const CoachPlayerObject * our_player = *it;
        if ( ! our_player ) continue;
        if ( our_player->goalie() ) continue;

        double dist2 = our_player->pos().dist2( wm.ball().pos() );
        if ( dist2 < min_dist2 )
        {
            min_dist2 = dist2;
            our_unum_nearest_to_ball = our_player->unum();
        }
    }

    std::vector< int > first_path;
    first_path.push_back( our_unum_nearest_to_ball );
    M_path.push_back( first_path );

    appendNextLevelMates( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
CoachPathPlanner::doSetPlayOurBall( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__": (doSetPlayOurBall)" );

    const CoachWorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    int our_unum_nearest_to_ball = Unum_Unknown;
    double min_dist2 = std::pow( 10.0 * ( SP.pitchLength() + SP.pitchWidth() ), 2.0 );

    for ( CoachPlayerObject::Cont::const_iterator it = wm.teammates().begin();
          it != wm.teammates().end();
          ++it )
    {
        const CoachPlayerObject * our_player = *it;
        if ( ! our_player ) continue;
        if ( our_player->goalie() ) continue;

        double dist2 = our_player->pos().dist2( wm.ball().pos() );
        if ( dist2 < min_dist2 )
        {
            min_dist2 = dist2;
            our_unum_nearest_to_ball = our_player->unum();
        }
    }

    std::vector< int > first_path;
    first_path.push_back( our_unum_nearest_to_ball );
    M_path.push_back( first_path );

    appendNextLevelMates( agent );
    appendNextLevelMates( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
CoachPathPlanner::appendNextLevelMates( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__": (appendNextLevelMates)" );

    const CoachWorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    std::vector< std::vector< int > > new_path;

    for ( std::vector< std::vector< int > >::iterator it_path = M_path.begin();
          it_path != M_path.end();
          ++it_path )
    {
        dlog.addText( Logger::ANALYZER,
                      __FILE__":(%d) unum of player_tail is %d",
                      __LINE__, (*it_path).back() );

        const int unum_tail = (*it_path).back();
        const CoachPlayerObject * player_tail = wm.teammate( unum_tail );
        if ( ! player_tail ) continue;


        std::vector< int > unum_neighbors;

        for ( int i = 0; i < NUMBER_OF_NEIGHBORS; ++i )
        {
            double min_dist2 = std::pow( 10.0 * ( SP.pitchLength() + SP.pitchWidth() ), 2.0 );
            int unum_min = Unum_Unknown;
            for ( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin();
                  it_mate != wm.teammates().end();
                  ++it_mate )
            {
                const CoachPlayerObject * our_player = *it_mate;
                if ( ! our_player ) continue;
                if ( our_player->goalie() ) continue;

                const int unum_mate = our_player->unum();

                bool used = false;
                for ( std::vector< int >::iterator it = unum_neighbors.begin();
                      it != unum_neighbors.end();
                      ++it )
                {
                    if ( (*it) == unum_mate )
                    {
                        used = true;
                        break;
                    }
                }
                if ( used ) continue;

                for ( std::vector< int >::iterator it_it_path = (*it_path).begin();
                      it_it_path != (*it_path).end();
                      ++it_it_path )
                {
                    if ( (*it_it_path) == unum_mate )
                    {
                        used = true;
                        break;
                    }
                }
                if ( used ) continue;

                double dist2 = our_player->pos().dist2( player_tail->pos() );
                if ( dist2 < min_dist2 )
                {
                    min_dist2 = dist2;
                    unum_min = unum_mate;
                }
            }

            if ( unum_min != Unum_Unknown )
            {
                unum_neighbors.push_back( unum_min );

                dlog.addText( Logger::ANALYZER,
                              __FILE__":(%d) Adding Player %d -> Player %d",
                              __LINE__, unum_tail, unum_min );
            }
        }

        for ( std::vector< int >::iterator it = unum_neighbors.begin();
              it != unum_neighbors.end();
              ++it )
        {
            std::vector< int > tmp_intvector( *it_path );

            tmp_intvector.push_back( *it );

            new_path.push_back( tmp_intvector );

#ifdef DEBUG_DRAW_PLAYER_CHAINS
            dlog.addText( Logger::ANALYZER,
                          __FILE__":(%d) Drawing Player %d -> Player %d",
                          __LINE__, unum_tail, *it );
            agent->debugClient().addLine( player_tail->pos(), wm.teammate( *it )->pos(), "#AAA" );
#endif
        }
    }

    M_path.clear();
    for ( std::vector< std::vector< int > >::iterator it_new_path = new_path.begin();
          it_new_path != new_path.end();
          ++it_new_path )
    {
        M_path.push_back( *it_new_path );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

/*-------------------------------------------------------------------*/
/*!

 */
