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

#include "opponent_defense_mark_or_zone_decider.h"
#include "coach_analyzer_manager.h"

#include <algorithm>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/coach/coach_agent.h>
#include <rcsc/coach/coach_world_model.h>

using namespace rcsc;

namespace {
// MAGIC numbers
const double OPPONENT_DEFENCE_LIMIT = 25.0;
const double MARKING_MARGIN2 = 3.0*3.0;
const double UPPER_VELOCITY_FOR_STAYING = 0.5;
}

/*-------------------------------------------------------------------*/
/*!

 */
OpponentDefenseMarkOrZoneDecider::OpponentDefenseMarkOrZoneDecider()
{
    for ( int i = 0; i < 11; ++i )
    {
        for ( int j = 0; j < 11; ++j )
        {
            M_opponent_marking_matrix[i][j] = 0;
        }
    }

    for ( int i = 0; i < 11; ++i )
    {
        std::vector< rcsc::Vector2D > vc;
        M_stay_position.push_back( vc );
    }

    for ( int j = 0; j < 11; ++j )
    {
        M_candidate_unum_marking[j] = Unum_Unknown;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentDefenseMarkOrZoneDecider::analyze( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "%s:%d: OpponentDefenseMarkOrZoneDecider::analyze",
                  __FILE__, __LINE__ );

    const CoachWorldModel & wm = agent->world();

    if ( wm.gameMode().type() == GameMode::PlayOn )
    {
        doPlayOn( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentDefenseMarkOrZoneDecider::drawCandidateMarking( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "OpponentDefenseMarkOrZoneDecider::drawCandidateMarkingOpponent" );

    const CoachWorldModel & wm = agent->world();

    for ( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
              end_opp = wm.opponents().end();
          it_opp != end_opp;
          ++it_opp )
    {
        const CoachPlayerObject * p_opp = * it_opp;
        if ( ! p_opp ) continue;
        if ( p_opp->goalie() ) continue;

        int unum_opp = p_opp->unum();
        int marked_mate_unum = M_candidate_unum_marking[unum_opp - 1];

        const CoachPlayerObject * p_mate = wm.teammate( marked_mate_unum );
        if ( ! p_mate ) continue;

        agent->debugClient().addLine( p_opp->pos(), p_mate->pos(), "#008" );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentDefenseMarkOrZoneDecider::doPlayOn( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "OpponentDefenseMarkOrZoneDecider::doPlayOn" );

    const CoachWorldModel & wm = agent->world();

    if ( wm.ball().pos().x < OPPONENT_DEFENCE_LIMIT ) // MAGIC number
    {
        dlog.addText( Logger::TEAM,
                      "%d: Ball is not deep enough in their area. will not analyze.", __LINE__ );
        return false;
    }

    if ( wm.lastKickerSide() == wm.ourSide() )
    {
        doPlayOnOurBall( agent );

        drawCandidateMarking( agent );
    }
    else if ( wm.lastKickerSide() == wm.theirSide() )
    {
        doPlayOnTheirBall( agent );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      "%d: Unknown which team the ball is governed by. will not analyze.", __LINE__ );
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentDefenseMarkOrZoneDecider::doPlayOnOurBall( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "OpponentDefenseMarkOrZoneDecider::doPlayOnOurSide" );

    //const CoachWorldModel & wm = agent->world();

    checkMarkingByDistance( agent );

    checkZoning( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentDefenseMarkOrZoneDecider::checkZoning( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "OpponentDefenseMarkOrZoneDecider::checkZoning" );

    const CoachWorldModel & wm = agent->world();

    std::vector< int > unum_other_players;

    for ( int unum = 1; unum <= 11; ++unum )
    {
        CoachAnalyzerManager::RoleType rt;
        rt = CoachAnalyzerManager::i().opponentFormation( unum );

        if ( rt == CoachAnalyzerManager::OTHER )
        {
            const CoachPlayerObject * p = wm.opponent( unum );
            if ( ! p ) continue;

            unum_other_players.push_back( p->unum() );
        }
    }

    for ( std::vector< int >::iterator it = unum_other_players.begin();
          it != unum_other_players.end();
          ++it )
    {
        const CoachPlayerObject * p = wm.opponent( *it );
        if ( ! p ) continue;

        // in the future, the condition will be:
        // if ( the current velocity is lower than the previous velocity )
        if ( p->vel().r() < UPPER_VELOCITY_FOR_STAYING )  // MAGIC number
        {
            M_stay_position[p->unum() - 1].push_back( p->pos() );
        }
    }

    // doKMeansForStayPositions

    // mergeTwoClusterCentersIfPossible

    // statisticalTestforZoning


    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentDefenseMarkOrZoneDecider::checkMarkingByDistance( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "OpponentDefenseMarkOrZoneDecider::checkMarkingByDistance" );

    const CoachWorldModel & wm = agent->world();
    const ServerParam & sp = ServerParam::i();

    std::vector< int > opp_unum_nearest_to_mate( 11, Unum_Unknown );
    std::vector< int > mate_unum_nearest_to_opp( 11, Unum_Unknown );

    for ( int j = 0; j < 11; ++j )
    {
        M_candidate_unum_marking[j] = Unum_Unknown;
    }

    // For now this function checks all opponent players.
    // This check should be done for opponent defenders
    // identified by OpponentFormationAnalyzer
    for ( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
              end_opp = wm.opponents().end();
          it_opp != end_opp;
          ++it_opp )
    {
        const CoachPlayerObject * p_opp = * it_opp;
        if ( ! p_opp ) continue;
        if ( p_opp->goalie() ) continue;

        int unum_opp = p_opp->unum();

        double min_dist2 = ( sp.pitchWidth() + sp.pitchLength() ) * 10.0;
        min_dist2 *= min_dist2;
        int unum_nearest_mate = -1;
        for ( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
                  end_mate = wm.teammates().end();
              it_mate != end_mate;
              ++it_mate )
        {
            const CoachPlayerObject * p_mate = * it_mate;
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;

            double dist2 = p_opp->pos().dist2( p_mate->pos() );
            if ( dist2 < min_dist2 )
            {
                min_dist2 = dist2;
                unum_nearest_mate = p_mate->unum();
            }
        }

        if ( unum_nearest_mate != -1
             && min_dist2 < MARKING_MARGIN2 )
        {
            mate_unum_nearest_to_opp[unum_opp - 1] = unum_nearest_mate;
        }
    }

    for ( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
              end_mate = wm.teammates().end();
          it_mate != end_mate;
          ++it_mate )
    {
        const CoachPlayerObject * p_mate = * it_mate;
        if ( ! p_mate ) continue;
        if ( p_mate->goalie() ) continue;

        int unum_mate = p_mate->unum();

        double min_dist2 = ( sp.pitchWidth() + sp.pitchLength() ) * 10.0;
        min_dist2 *= min_dist2;
        int unum_nearest_opp = -1;
        for ( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
                  end_opp = wm.opponents().end();
              it_opp != end_opp;
              ++it_opp )
        {
            const CoachPlayerObject * p_opp = * it_opp;
            if ( ! p_opp ) continue;
            if ( p_opp->goalie() ) continue;

            double dist2 = p_mate->pos().dist2( p_opp->pos() );
            if ( dist2 < min_dist2 )
            {
                min_dist2 = dist2;
                unum_nearest_opp = p_opp->unum();
            }
        }

        if ( unum_nearest_opp != -1
             && min_dist2 < MARKING_MARGIN2 )
        {
            opp_unum_nearest_to_mate[unum_mate - 1] = unum_nearest_opp;
        }
    }

    for ( int i = 0; i < 11; ++i )
    {
        dlog.addText( Logger::TEAM, "%d: opp_unum_nearest_to_mate[%d] = %d",
                      __LINE__, i, opp_unum_nearest_to_mate[i] );
    }
    for ( int j = 0; j < 11; ++j )
    {
        dlog.addText( Logger::TEAM, "%d: mate_unum_nearest_to_opp[%d] = %d",
                      __LINE__, j, mate_unum_nearest_to_opp[j] );
    }

    for ( int i = 0; i < 11; ++i )
    {
        int opp_unum = opp_unum_nearest_to_mate[i];
        if ( opp_unum == Unum_Unknown ) continue;

        if ( i + 1 == mate_unum_nearest_to_opp[opp_unum - 1] )
        {
            M_opponent_marking_matrix[i][opp_unum - 1] += 1;
            M_candidate_unum_marking[opp_unum - 1] = i + 1;

            dlog.addText( Logger::TEAM, "%d: Oppnent %d seems to mark Our %d",
                          __LINE__, opp_unum, i + 1 );
        }
    }


    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentDefenseMarkOrZoneDecider::doPlayOnTheirBall( CoachAgent * /*agent*/ )
{
    dlog.addText( Logger::TEAM,
                  "OpponentDefenseMarkOrZoneDecider::doPlayOnTheirSide" );

    //const CoachWorldModel & wm = agent->world();



    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentDefenseMarkOrZoneDecider::doSendFreeform( CoachAgent * /*agent*/ )
{
    /*
      format:
      "(mark (1 -1) (2 -1) (3 -1) (4 -1) (5 -1) (6 0) (7 0) (8 0) (9 1) (10 1) (11 1))"
      ->
      (say (freeform "(mark ...)"))

      (teammate_unum opponent_unum_to_mark)
      opponent_unum_to_mark is -1 if the analyzer could not find an appropriate opponent to mark
    */

#if 0
    dlog.addText( Logger::TEAM, "OpponentDefenseMarkOrZoneDecider::doSendFreeForm" );

    if ( ! agent->config().useFreeform() )
    {
        dlog.addText( Logger::TEAM, "%d: useFreeform is false", __LINE__ );
        return false;
    }

    if ( ! agent->world().canSendFreeform() )
    {
        dlog.addText( Logger::TEAM, "%d: canSendFreeform is false", __LINE__ );
        return false;
    }

    if ( agent->world().time().cycle() < 200 )
    {
        return false;
    }

    std::string msg;
    msg.reserve( 128 );

    msg = "(mark ";

    for ( int unum = 1; unum <= 11; ++unum )
    {
        char buf[8];
        snprintf( buf, 8, "(%d %d)",
                  unum, M_candidate_opponent_unum_to_mark[unum - 1] );
        msg += buf;
    }

    msg += ")";

    agent->doSayFreeform( msg );

    std::cout << agent->config().teamName()
              << " Coach: "
              << agent->world().time()
              << " send freeform " << msg
              << std::endl;

    agent->debugClient().addMessageString( msg );
    dlog.addText( Logger::TEAM,
                  __FILE__": mark %s", msg.c_str() );

#endif

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
