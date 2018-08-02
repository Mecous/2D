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

#include "coach_mark_analyzer.h"
#include "coach_analyzer_manager.h"
#include "coach_strategy.h"

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/coach/coach_agent.h>
#include <rcsc/coach/coach_world_model.h>

#include <string>
#include <cstdio>

using namespace rcsc;

#define DEBUG_PRINT
#define DEBUG_DRAW_MARK_TARGET

/*-------------------------------------------------------------------*/
/*!

 */
CoachMarkAnalyzer::CoachMarkAnalyzer()
{
    for ( int i = 0; i < 11; ++i )
    {
        for ( int j = 0; j < 11; ++j )
        {
            M_nearest_matrix[i][j] = 0;
            M_nearest_matrix_setplay[i][j] = 0;
        }

        M_nearest_sum[i] = 0;
        M_nearest_sum_setplay[i] = 0;
    }

    for ( int i = 0; i < 11; ++i )
    {
        M_opponent_unum_to_mark[i] = Unum_Unknown;
        M_candidate_opponent_unum_to_mark[i] = Unum_Unknown;
        M_second_opponent_unum_to_mark[i] = Unum_Unknown;
    }

    for ( int i = 0; i < 11; ++i )
    {
        if ( i == 0 || i == 1 )
        {
            M_do_marking[i] = false;
        }
        else
        {
            M_do_marking[i] = true;
        }

        if ( 1 <= i && i <= 5 )
        {
            M_is_defender[i] = true;
        }
        else
        {
            M_is_defender[i] = false;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
CoachMarkAnalyzer::analyze( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__": (analyze)" );

    const CoachWorldModel & wm = agent->world();

    updateMarkingFlag();

    if ( wm.gameMode().type() == GameMode::PlayOn )
    {
        doPlayOn( agent );
    }
    else if ( wm.gameMode().isTheirSetPlay( wm.ourSide() ) )
    {
        doSetPlayTheirBall( agent );
    }

    if ( wm.time().cycle() > 200
         && checkDifference( agent ) > 0 )
    {
#if 0
        if ( doSendFreeform( agent ) )
        {
            acceptCandidateTargets( agent );
        }
#else
        acceptCandidateTargets( agent );
#endif
        reportToManager();
    }

#ifdef DEBUG_DRAW_MARK_TARGET

    if ( isMarkingSituation( agent ) )
    {
        for( CoachPlayerObject::Cont::const_iterator it = wm.teammates().begin(),
                 end = wm.teammates().end();
             it != end;
             ++it )
        {
            const CoachPlayerObject * p = *it;
            if ( ! p ) continue;
            if ( p->goalie() ) continue;

            int p_unum = p->unum();
            if ( M_opponent_unum_to_mark[p_unum - 1] != Unum_Unknown )
            {
                const CoachPlayerObject * p_tgt = wm.opponent( M_opponent_unum_to_mark[p_unum - 1] );
                if( ! p_tgt ) continue;

                agent->debugClient().addLine( p_tgt->pos(), p->pos(), "#AA0" );
            }
        }
    }
#endif

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CoachMarkAnalyzer::updateMarkingFlag()
{
    for ( int unum = 1; unum <= 11; ++unum )
    {
        M_do_marking[unum-1] = CoachStrategy::i().isMarkerType( unum );
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ANALYZER,
                      __FILE__":(updateMarkingFlag) %d %s",
                      unum, M_do_marking[unum-1] ? "on" : "off" );
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CoachMarkAnalyzer::reportToManager()
{
    for ( int i = 0; i < 11; ++i )
    {
        CoachAnalyzerManager::instance().setMarkAssignment( i + 1,
                                                            M_candidate_opponent_unum_to_mark[i],
                                                            M_second_opponent_unum_to_mark[i] );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
int
CoachMarkAnalyzer::checkDifference( CoachAgent * agent )
{
    const CoachWorldModel & wm = agent->world();

    int n_modified = 0;
    for( CoachPlayerObject::Cont::const_iterator it = wm.teammates().begin(),
             end = wm.teammates().end();
         it != end;
         ++it )
    {
        if ( ! *it ) continue;
        if ( (*it)->goalie() ) continue;

        int i = (*it)->unum() - 1;
        if ( M_opponent_unum_to_mark[i] != M_candidate_opponent_unum_to_mark[i] )
        {
            ++n_modified;
        }
    }

    return n_modified;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
CoachMarkAnalyzer::acceptCandidateTargets( CoachAgent * agent )
{
    const CoachWorldModel & wm = agent->world();

    for( CoachPlayerObject::Cont::const_iterator it = wm.teammates().begin(),
             end = wm.teammates().end();
         it != end;
         ++it )
    {
        if ( ! *it ) continue;
        if ( (*it)->goalie() ) continue;

        int i = (*it)->unum() - 1;
        if ( M_opponent_unum_to_mark[i] != M_candidate_opponent_unum_to_mark[i] )
        {
            M_opponent_unum_to_mark[i] = M_candidate_opponent_unum_to_mark[i];
        }

    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
CoachMarkAnalyzer::doPlayOn( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__":(doPlayOn)" );

    checkNearestPlayer( agent );

    if ( isMarkingSituation( agent ) )
    {
        findOpponentToMarkForDefenders( agent );
        findOpponentToMarkForTheOthers( agent );
        findSecondOpponentToMark();
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
CoachMarkAnalyzer::findSecondOpponentToMark() //  CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__":(findSecondOpponentToMark)" );

    // const CoachWorldModel & wm = agent->world();

    for ( int i = 0; i < 11; ++i )
    {
        M_second_opponent_unum_to_mark[i] = Unum_Unknown;
    }

    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;

        int second_max_opp_unum = Unum_Unknown;
        int second_max_cnt = -1;
        for ( int j = 0; j < 11; ++j )
        {
            if ( j + 1 == M_candidate_opponent_unum_to_mark[i] )
                continue;

            if ( second_max_cnt < M_nearest_matrix[i][j] )
            {
                second_max_opp_unum = j + 1;
                second_max_cnt = M_nearest_matrix[i][j];
            }
        }

        if ( second_max_cnt != -1 )
        {
            M_second_opponent_unum_to_mark[i] = second_max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current second mark: %d %d %d %d %d %d %d %d %d %d %d",
                  M_second_opponent_unum_to_mark[0],
                  M_second_opponent_unum_to_mark[1],
                  M_second_opponent_unum_to_mark[2],
                  M_second_opponent_unum_to_mark[3],
                  M_second_opponent_unum_to_mark[4],
                  M_second_opponent_unum_to_mark[5],
                  M_second_opponent_unum_to_mark[6],
                  M_second_opponent_unum_to_mark[7],
                  M_second_opponent_unum_to_mark[8],
                  M_second_opponent_unum_to_mark[9],
                  M_second_opponent_unum_to_mark[10]
                  );

    return true;
}

/*------------------------------------------------------------------------*/
/*!

 */

bool
CoachMarkAnalyzer::findOpponentToMark( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__":(findOpponentToMark)" );

    const CoachWorldModel & wm = agent->world();

    for ( int i = 0; i < 11; ++i )
    {
        M_candidate_opponent_unum_to_mark[i] = Unum_Unknown;
    }

    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;

        int max_opp_unum = Unum_Unknown;
        int max_cnt = 0;
        for ( int j = 0; j < 11; ++j )
        {
            CoachAnalyzerManager::RoleType role_type
                = CoachAnalyzerManager::i().opponentFormation( j + 1 );
            if ( role_type == CoachAnalyzerManager::OTHER )
                continue;

            if ( max_cnt < M_nearest_matrix[i][j] )
            {
                max_opp_unum = j + 1;
                max_cnt = M_nearest_matrix[i][j];
            }
        }

        if ( max_cnt != 0 )
        {
            M_candidate_opponent_unum_to_mark[i] = max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(1): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // duplication check
    // only the currently nearest player to the target is valid.
    // the other players that are assined to the target are canceled.
    for( CoachPlayerObject::Cont::const_iterator it = wm.opponents().begin(),
             end = wm.opponents().end();
         it != end;
         ++it )
    {
        const CoachPlayerObject * p = *it;
        if ( ! p ) continue;
        if ( p->goalie() ) continue;

        CoachAnalyzerManager::RoleType role_type
            = CoachAnalyzerManager::i().opponentFormation( p->unum() );
        if ( role_type == CoachAnalyzerManager::OTHER )
            continue;

        int unum_tgt = p->unum();
        int max_ratio_index = -1;
        double max_ratio = 0.0;
        for ( int i = 0; i < 11; ++i )
        {
            const CoachPlayerObject * p_mate = wm.teammate( i + 1 );
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;
            if ( M_candidate_opponent_unum_to_mark[i] != unum_tgt )
                continue;

            double ratio = (double)M_nearest_matrix[i][unum_tgt - 1]
                / M_nearest_sum[i];
            if( ratio > max_ratio )
            {
                max_ratio = ratio;
                max_ratio_index = i;
            }
        }

        if ( max_ratio_index != -1 )
        {
            for ( int i = 0; i < 11; ++i )
            {
                if ( M_candidate_opponent_unum_to_mark[i] == unum_tgt
                     && i != max_ratio_index )
                {
                    M_candidate_opponent_unum_to_mark[i] = Unum_Unknown;
                }
            }
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(2): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // mark target is determined according to the nearest matrix
    // for those whose target is not determined yet.
    // The procedure is almost the same as the first step.
    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;
        if ( M_candidate_opponent_unum_to_mark[i] != Unum_Unknown )
            continue;

        int max_opp_unum = Unum_Unknown;
        int max_cnt = 0;
        for ( int j = 0; j < 11; ++j )
        {
            CoachAnalyzerManager::RoleType role_type
                = CoachAnalyzerManager::i().opponentFormation( j + 1 );
            if ( role_type == CoachAnalyzerManager::OTHER )
                continue;

            bool used = false;
            for ( int i2 = 0; i2 < 11; ++i2 )
            {
                if ( M_candidate_opponent_unum_to_mark[i2] == j + 1 )
                {
                    used = true;
                    break;
                }
            }
            if ( used == true )
            {
                continue;
            }

            if ( max_cnt < M_nearest_matrix[i][j] )
            {
                max_opp_unum = j + 1;
                max_cnt = M_nearest_matrix[i][j];
            }
        }

        if ( max_cnt != 0 )
        {
            M_candidate_opponent_unum_to_mark[i] = max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(3): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // for our players whose target is still unknown
    // the target is determined as the current nearest opponent player
    for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
             end_opp = wm.opponents().end();
         it_opp != end_opp;
         ++it_opp )
    {
        const CoachPlayerObject * p_opp = *it_opp;
        if ( ! p_opp ) continue;
        if ( p_opp->goalie() ) continue;
        int opp_unum = p_opp->unum();

        CoachAnalyzerManager::RoleType role_type
            = CoachAnalyzerManager::i().opponentFormation( p_opp->unum() );
        if ( role_type == CoachAnalyzerManager::OTHER )
            continue;

        // check if the player is already considered as a candidate mark target
        bool used = false;
        for ( int i = 0; i < 11; ++i )
        {
            if ( M_candidate_opponent_unum_to_mark[i] == opp_unum )
            {
                used = true;
                break;
            }
        }
        if ( used == true )
        {
            continue;
        }

        dlog.addText( Logger::ANALYZER,
                      __FILE__":(findOpponentToMark) No teammate is assigned for Opponent %d",
                      opp_unum );

        double min_dist2 = ( ServerParam::i().pitchLength() + ServerParam::i().pitchWidth() ) * 10.0;
        min_dist2 *= min_dist2;
        int mate_unum_nearest = -1;
        for( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
                 end_mate = wm.teammates().end();
             it_mate != end_mate;
             ++it_mate )
        {
            const CoachPlayerObject * p_mate = *it_mate;
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;
            int mate_unum = p_mate->unum();

            dlog.addText( Logger::ANALYZER,
                          __FILE__":(findOpponentToMark) M_c_o_u_t_m[%d] = %d",
                          mate_unum - 1,
                          M_candidate_opponent_unum_to_mark[mate_unum - 1] );

            if ( ! M_do_marking[mate_unum - 1] ) continue;
            if ( M_candidate_opponent_unum_to_mark[mate_unum - 1] != Unum_Unknown )
            {
                continue;
            }

            double dist2 = p_opp->pos().dist2( p_mate->pos() );
            if ( min_dist2 > dist2 )
            {
                min_dist2 = dist2;
                mate_unum_nearest = mate_unum;
            }
        }

        if ( mate_unum_nearest != -1 )
        {
            M_candidate_opponent_unum_to_mark[mate_unum_nearest - 1] = opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark: %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    return true;
}

/*------------------------------------------------------------------------*/
/*!

 */

bool
CoachMarkAnalyzer::findOpponentToMarkForDefenders( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__":(findOpponentToMarkForDefenders)" );

    const CoachWorldModel & wm = agent->world();

    // It is assumed that players with unum 2-5 are defenders.
    for ( int i = 0; i < 11; ++i )
    {
        M_candidate_opponent_unum_to_mark[i] = Unum_Unknown;
    }

    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;
        if ( ! M_is_defender[i] ) continue;

        int max_opp_unum = Unum_Unknown;
        int max_cnt = 0;
        for ( int j = 0; j < 11; ++j )
        {
            CoachAnalyzerManager::RoleType role_type
                = CoachAnalyzerManager::i().opponentFormation( j + 1 );
            if ( role_type == CoachAnalyzerManager::OTHER )
                continue;

            if ( max_cnt < M_nearest_matrix[i][j] )
            {
                max_opp_unum = j + 1;
                max_cnt = M_nearest_matrix[i][j];
            }
        }

        if ( max_cnt != 0 )
        {
            M_candidate_opponent_unum_to_mark[i] = max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(1): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // duplication check
    // only the currently nearest player to the target is valid.
    // the other players that are assined to the target are canceled.
    for( CoachPlayerObject::Cont::const_iterator it = wm.opponents().begin(),
             end = wm.opponents().end();
         it != end;
         ++it )
    {
        const CoachPlayerObject * p = *it;
        if ( ! p ) continue;
        if ( p->goalie() ) continue;

        CoachAnalyzerManager::RoleType role_type
            = CoachAnalyzerManager::i().opponentFormation( p->unum() );
        if ( role_type == CoachAnalyzerManager::OTHER )
            continue;

        int unum_tgt = p->unum();
        int max_ratio_index = -1;
        double max_ratio = 0.0;
        for ( int i = 0; i < 11; ++i )
        {
            const CoachPlayerObject * p_mate = wm.teammate( i + 1 );
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;
            if ( M_candidate_opponent_unum_to_mark[i] != unum_tgt )
                continue;

            double ratio = (double)M_nearest_matrix[i][unum_tgt - 1]
                / M_nearest_sum[i];
            if( ratio > max_ratio )
            {
                max_ratio = ratio;
                max_ratio_index = i;
            }
        }

        if ( max_ratio_index != -1 )
        {
            for ( int i = 0; i < 11; ++i )
            {
                if ( M_candidate_opponent_unum_to_mark[i] == unum_tgt
                     && i != max_ratio_index )
                {
                    M_candidate_opponent_unum_to_mark[i] = Unum_Unknown;
                }
            }
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(2): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // mark target is determined according to the nearest matrix
    // for those whose target is not determined yet.
    // The procedure is almost the same as the first step.
    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;
        if ( ! M_is_defender[i] ) continue;
        if ( M_candidate_opponent_unum_to_mark[i] != Unum_Unknown )
            continue;

        int max_opp_unum = Unum_Unknown;
        int max_cnt = 0;
        for ( int j = 0; j < 11; ++j )
        {
            CoachAnalyzerManager::RoleType role_type
                = CoachAnalyzerManager::i().opponentFormation( j + 1 );
            if ( role_type == CoachAnalyzerManager::OTHER )
                continue;

            bool used = false;
            for ( int i2 = 0; i2 < 11; ++i2 )
            {
                if ( M_candidate_opponent_unum_to_mark[i2] == j + 1 )
                {
                    used = true;
                    break;
                }
            }
            if ( used == true )
            {
                continue;
            }

            if ( max_cnt < M_nearest_matrix[i][j] )
            {
                max_opp_unum = j + 1;
                max_cnt = M_nearest_matrix[i][j];
            }
        }

        if ( max_cnt != 0 )
        {
            M_candidate_opponent_unum_to_mark[i] = max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(3): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // for our players whose target is still unknown
    // the target is determined as the current nearest opponent player
    for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
             end_opp = wm.opponents().end();
         it_opp != end_opp;
         ++it_opp )
    {
        const CoachPlayerObject * p_opp = *it_opp;
        if ( ! p_opp ) continue;
        if ( p_opp->goalie() ) continue;
        int opp_unum = p_opp->unum();

        CoachAnalyzerManager::RoleType role_type
            = CoachAnalyzerManager::i().opponentFormation( p_opp->unum() );
        if ( role_type == CoachAnalyzerManager::OTHER )
            continue;

        // check if the player is already considered as a candidate mark target
        bool used = false;
        for ( int i = 0; i < 11; ++i )
        {
            if ( M_candidate_opponent_unum_to_mark[i] == opp_unum )
            {
                used = true;
                break;
            }
        }
        if ( used == true )
        {
            continue;
        }

        dlog.addText( Logger::ANALYZER,
                      __FILE__":(findOpponentToMark) No teammate is assigned for Opponent %d",
                      opp_unum );

        double min_dist2 = ( ServerParam::i().pitchLength() + ServerParam::i().pitchWidth() ) * 10.0;
        min_dist2 *= min_dist2;
        int mate_unum_nearest = -1;
        for( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
                 end_mate = wm.teammates().end();
             it_mate != end_mate;
             ++it_mate )
        {
            const CoachPlayerObject * p_mate = *it_mate;
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;
            int mate_unum = p_mate->unum();

            dlog.addText( Logger::ANALYZER,
                          __FILE__":(findOpponentToMark) M_c_o_u_t_m[%d] = %d",
                          mate_unum - 1,
                          M_candidate_opponent_unum_to_mark[mate_unum - 1] );

            if ( ! M_do_marking[mate_unum - 1] ) continue;
            if ( ! M_is_defender[mate_unum - 1] ) continue;
            if ( M_candidate_opponent_unum_to_mark[mate_unum - 1] != Unum_Unknown )
            {
                continue;
            }

            double dist2 = p_opp->pos().dist2( p_mate->pos() );
            if ( min_dist2 > dist2 )
            {
                min_dist2 = dist2;
                mate_unum_nearest = mate_unum;
            }
        }

        if ( mate_unum_nearest != -1 )
        {
            M_candidate_opponent_unum_to_mark[mate_unum_nearest - 1] = opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark: %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    return true;
}

/*------------------------------------------------------------------------*/
/*!

 */

bool
CoachMarkAnalyzer::findOpponentToMarkForTheOthers( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__":(findOpponentToMarkForTheOthers)" );

    const CoachWorldModel & wm = agent->world();

    // It is assumed that players with unum 2-5 are defenders.
    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_is_defender[i] )
        {
            M_candidate_opponent_unum_to_mark[i] = Unum_Unknown;
        }
    }

    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;
        if ( M_is_defender[i] ) continue;

        int max_opp_unum = Unum_Unknown;
        int max_cnt = 0;
        for ( int j = 0; j < 11; ++j )
        {
            CoachAnalyzerManager::RoleType role_type
                = CoachAnalyzerManager::i().opponentFormation( j + 1 );
            if ( role_type == CoachAnalyzerManager::OTHER )
                continue;

            if ( max_cnt < M_nearest_matrix[i][j] )
            {
                max_opp_unum = j + 1;
                max_cnt = M_nearest_matrix[i][j];
            }
        }

        if ( max_cnt != 0 )
        {
            M_candidate_opponent_unum_to_mark[i] = max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(1): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // duplication check
    // only the currently nearest player to the target is valid.
    // the other players that are assined to the target are canceled.
    for( CoachPlayerObject::Cont::const_iterator it = wm.opponents().begin(),
             end = wm.opponents().end();
         it != end;
         ++it )
    {
        const CoachPlayerObject * p = *it;
        if ( ! p ) continue;
        if ( p->goalie() ) continue;

        CoachAnalyzerManager::RoleType role_type
            = CoachAnalyzerManager::i().opponentFormation( p->unum() );
        if ( role_type == CoachAnalyzerManager::OTHER )
            continue;

        int unum_tgt = p->unum();
        int max_ratio_index = -1;
        double max_ratio = 0.0;
        for ( int i = 0; i < 11; ++i )
        {
            const CoachPlayerObject * p_mate = wm.teammate( i + 1 );
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;
            if ( M_candidate_opponent_unum_to_mark[i] != unum_tgt )
                continue;

            double ratio = (double)M_nearest_matrix[i][unum_tgt - 1]
                / M_nearest_sum[i];
            if( ratio > max_ratio )
            {
                max_ratio = ratio;
                max_ratio_index = i;
            }
        }

        if ( max_ratio_index != -1 )
        {
            for ( int i = 0; i < 11; ++i )
            {
                if ( M_candidate_opponent_unum_to_mark[i] == unum_tgt
                     && i != max_ratio_index )
                {
                    M_candidate_opponent_unum_to_mark[i] = Unum_Unknown;
                }
            }
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(2): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // mark target is determined according to the nearest matrix
    // for those whose target is not determined yet.
    // The procedure is almost the same as the first step.
    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;
        if ( M_is_defender[i] ) continue;
        if ( M_candidate_opponent_unum_to_mark[i] != Unum_Unknown )
            continue;

        int max_opp_unum = Unum_Unknown;
        int max_cnt = 0;
        for ( int j = 0; j < 11; ++j )
        {
            CoachAnalyzerManager::RoleType role_type
                = CoachAnalyzerManager::i().opponentFormation( j + 1 );
            if ( role_type == CoachAnalyzerManager::OTHER )
                continue;

            bool used = false;
            for ( int i2 = 0; i2 < 11; ++i2 )
            {
                if ( M_candidate_opponent_unum_to_mark[i2] == j + 1 )
                {
                    used = true;
                    break;
                }
            }
            if ( used == true )
            {
                continue;
            }

            if ( max_cnt < M_nearest_matrix[i][j] )
            {
                max_opp_unum = j + 1;
                max_cnt = M_nearest_matrix[i][j];
            }
        }

        if ( max_cnt != 0 )
        {
            M_candidate_opponent_unum_to_mark[i] = max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(3): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // for our players whose target is still unknown
    // the target is determined as the current nearest opponent player
    for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
             end_opp = wm.opponents().end();
         it_opp != end_opp;
         ++it_opp )
    {
        const CoachPlayerObject * p_opp = *it_opp;
        if ( ! p_opp ) continue;
        if ( p_opp->goalie() ) continue;
        int opp_unum = p_opp->unum();

        CoachAnalyzerManager::RoleType role_type
            = CoachAnalyzerManager::i().opponentFormation( p_opp->unum() );
        if ( role_type == CoachAnalyzerManager::OTHER )
            continue;

        // check if the player is already considered as a candidate mark target
        bool used = false;
        for ( int i = 0; i < 11; ++i )
        {
            if ( M_candidate_opponent_unum_to_mark[i] == opp_unum )
            {
                used = true;
                break;
            }
        }
        if ( used == true )
        {
            continue;
        }

        dlog.addText( Logger::ANALYZER,
                      __FILE__":(findOpponentToMark) No teammate is assigned for Opponent %d",
                      opp_unum );

        double min_dist2 = ( ServerParam::i().pitchLength() + ServerParam::i().pitchWidth() ) * 10.0;
        min_dist2 *= min_dist2;
        int mate_unum_nearest = -1;
        for( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
                 end_mate = wm.teammates().end();
             it_mate != end_mate;
             ++it_mate )
        {
            const CoachPlayerObject * p_mate = *it_mate;
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;
            int mate_unum = p_mate->unum();

            dlog.addText( Logger::ANALYZER,
                          __FILE__":(findOpponentToMark) M_c_o_u_t_m[%d] = %d",
                          mate_unum - 1,
                          M_candidate_opponent_unum_to_mark[mate_unum - 1] );

            if ( ! M_do_marking[mate_unum - 1] ) continue;
            if ( M_is_defender[mate_unum - 1] ) continue;
            if ( M_candidate_opponent_unum_to_mark[mate_unum - 1] != Unum_Unknown )
            {
                continue;
            }

            double dist2 = p_opp->pos().dist2( p_mate->pos() );
            if ( min_dist2 > dist2 )
            {
                min_dist2 = dist2;
                mate_unum_nearest = mate_unum;
            }
        }

        if ( mate_unum_nearest != -1 )
        {
            M_candidate_opponent_unum_to_mark[mate_unum_nearest - 1] = opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark: %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
CoachMarkAnalyzer::checkNearestPlayer( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__"(checkNearestPlayer)" );

    const CoachWorldModel & wm = agent->world();

    //
    // update nearest matrix
    //
    for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
             end_opp = wm.opponents().end();
         it_opp != end_opp;
         ++it_opp )
    {
        const CoachPlayerObject * p_opp = *it_opp;
        if ( ! p_opp ) continue;
        if ( p_opp->goalie() ) continue;

        double min_dist2 = ( ServerParam::i().pitchWidth() + ServerParam::i().pitchLength() ) * 10.0;
        min_dist2 *= min_dist2;
        int min_mate_unum = -1;
        for ( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
                 end_mate = wm.teammates().end();
             it_mate != end_mate;
             ++it_mate )
        {
            const CoachPlayerObject * p_mate = *it_mate;
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;

            double dist2 = p_mate->pos().dist2( p_opp->pos() );
            if ( dist2 < min_dist2 )
            {
                min_dist2 = dist2;
                min_mate_unum = p_mate->unum();
            }
        }

        if ( min_mate_unum != -1 )
        {
            // dlog.addText( Logger::ANALYZER,
            //               __FILE__":(checkNearestPlayer) opp->mate  opp=%d mate=%d",
            //               p_opp->unum(), min_mate_unum );
            M_nearest_matrix[min_mate_unum - 1][p_opp->unum() - 1] += 1;
            M_nearest_sum[min_mate_unum - 1] += 1;
        }
    }

    for ( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
             end_mate = wm.teammates().end();
         it_mate != end_mate;
         ++it_mate )
    {
        const CoachPlayerObject * p_mate = *it_mate;
        if ( ! p_mate ) continue;
        if( p_mate->goalie() ) continue;

        double min_dist2 = ( ServerParam::i().pitchWidth() + ServerParam::i().pitchLength() ) * 10.0;
        min_dist2 *= min_dist2;
        int min_opp_unum = -1;
        for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
                 end_opp = wm.opponents().end();
             it_opp != end_opp;
             ++it_opp )
        {
            const CoachPlayerObject * p_opp = *it_opp;
            if ( ! p_opp ) continue;
            if ( p_opp->goalie() ) continue;

            double dist2 = p_opp->pos().dist2( p_mate->pos() );
            if ( dist2 < min_dist2 )
            {
                min_dist2 = dist2;
                min_opp_unum = p_opp->unum();
            }
        }

        if ( min_opp_unum != -1 )
        {
            // dlog.addText( Logger::ANALYZER,
            //               __FILE__"(checkNearestPlayer) mate->opp mate=%d opp=%d",
            //               p_mate->unum(), min_opp_unum );
            M_nearest_matrix[p_mate->unum() - 1][min_opp_unum - 1] += 1;
            M_nearest_sum[p_mate->unum() - 1] += 1;
        }
    }

    for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
             end_opp = wm.opponents().end();
         it_opp != end_opp;
         ++it_opp )
    {
        const CoachPlayerObject * p_opp = *it_opp;
        if ( ! p_opp ) continue;
        if ( p_opp->goalie() ) continue;

        double min_dist2 = ( ServerParam::i().pitchWidth() + ServerParam::i().pitchLength() ) * 10.0;
        min_dist2 *= min_dist2;
        int min_mate_unum = -1;
        for ( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
                 end_mate = wm.teammates().end();
             it_mate != end_mate;
             ++it_mate )
        {
            const CoachPlayerObject * p_mate = *it_mate;
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;

            Vector2D home_pos = CoachStrategy::i().getPosition( p_mate->unum() );

            double dist2 = home_pos.dist2( p_opp->pos() );
            if ( dist2 < min_dist2 )
            {
                min_dist2 = dist2;
                min_mate_unum = p_mate->unum();
            }
        }

        if ( min_mate_unum != -1 )
        {
            // dlog.addText( Logger::ANALYZER,
            //               __FILE__":(checkNearestPlayer) opp->mate  opp=%d mate=%d",
            //               p_opp->unum(), min_mate_unum );
            M_nearest_matrix[min_mate_unum - 1][p_opp->unum() - 1] += 1;
            M_nearest_sum[min_mate_unum - 1] += 1;
        }
    }

    for ( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
             end_mate = wm.teammates().end();
         it_mate != end_mate;
         ++it_mate )
    {
        const CoachPlayerObject * p_mate = *it_mate;
        if ( ! p_mate ) continue;
        if( p_mate->goalie() ) continue;

        Vector2D home_pos = CoachStrategy::i().getPosition( p_mate->unum() );

        double min_dist2 = ( ServerParam::i().pitchWidth() + ServerParam::i().pitchLength() ) * 10.0;
        min_dist2 *= min_dist2;
        int min_opp_unum = -1;
        for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
                 end_opp = wm.opponents().end();
             it_opp != end_opp;
             ++it_opp )
        {
            const CoachPlayerObject * p_opp = *it_opp;
            if ( ! p_opp ) continue;
            if ( p_opp->goalie() ) continue;

            double dist2 = p_opp->pos().dist2( home_pos );
            if ( dist2 < min_dist2 )
            {
                min_dist2 = dist2;
                min_opp_unum = p_opp->unum();
            }
        }

        if ( min_opp_unum != -1 )
        {
            // dlog.addText( Logger::ANALYZER,
            //               __FILE__"(checkNearestPlayer) mate->opp mate=%d opp=%d",
            //               p_mate->unum(), min_opp_unum );
            M_nearest_matrix[p_mate->unum() - 1][min_opp_unum - 1] += 1;
            M_nearest_sum[p_mate->unum() - 1] += 1;
        }
    }


    dlog.addText( Logger::ANALYZER,
                  __FILE__":(checkNearestPlayer) Teammate-opponent nearest player matrix" );

    for ( int i = 0; i < 11; ++i )
    {
        std::string msg;
        msg.reserve( 128 );
        char buf[13];
        snprintf( buf, 13, "Teammate[%d] ", i + 1 );
        msg += buf;

        for ( int j = 0; j < 11; ++j )
        {
            char buf[6];
            snprintf( buf, 6, "%d ",
                      M_nearest_matrix[i][j] );
            msg += buf;
        }
        dlog.addText( Logger::ANALYZER, "%s: %d", msg.c_str(), M_nearest_sum[i] );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
SideID
CoachMarkAnalyzer::checkBallKickableSide( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__": (checkBallkickableSide)" );

    const CoachWorldModel & wm = agent->world();

    const CoachPlayerObject * nearest_player = wm.getPlayerNearestTo( wm.ball().pos() );

    if ( ! nearest_player )
    {
        dlog.addText( Logger::ANALYZER,
                      __FILE__":(checkBallKickableSide) Invalid nearest_player" );
        return NEUTRAL;
    }

    if ( wm.ball().pos().dist( nearest_player->pos() )
         < nearest_player->playerTypePtr()->kickableArea() )
    {
        if ( nearest_player->side() != wm.ourSide() )
        {
            return wm.theirSide();
        }
        else
        {
            return wm.ourSide();
        }
    }

    return NEUTRAL;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
CoachMarkAnalyzer::doSendFreeform( CoachAgent * agent )
{
    /*
      format:
      "(mark (1 -1) (2 -1) (3 -1) (4 -1) (5 -1) (6 0) (7 0) (8 0) (9 1) (10 1) (11 1))"
      ->
      (say (freeform "(mark ...)"))

      (teammate_unum opponent_unum_to_mark)
      opponent_unum_to_mark is -1 if the analyzer could not find an appropriate opponent to mark
    */

    dlog.addText( Logger::ANALYZER,
                  __FILE__": (doSendFreeForm)" );

    if ( ! agent->config().useFreeform() )
    {
        dlog.addText( Logger::ANALYZER,
                      __FILE__":(doSendFreeform) useFreeform is false" );
        return false;
    }

    if ( ! agent->world().canSendFreeform() )
    {
        dlog.addText( Logger::ANALYZER,
                      __FILE__":(doSendFreeform) canSendFreeform is false" );
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
        char buf[11];
        snprintf( buf, 11, "(%d %d %d)",
                  unum,
                  M_candidate_opponent_unum_to_mark[unum - 1],
                  M_second_opponent_unum_to_mark[unum - 1] );
        msg += buf;
    }

    msg += ")";
#if 0
    agent->doSayFreeform( msg );

    std::cout << agent->config().teamName()
              << " Coach: "
              << agent->world().time()
              << " send freeform " << msg
              << std::endl;

#endif
    agent->debugClient().addMessage( msg );
    dlog.addText( Logger::ANALYZER,
                  __FILE__": mark %s", msg.c_str() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
CoachMarkAnalyzer::isMarkingSituation( rcsc::CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__": (isMarkingSituation)" );

    const CoachWorldModel & wm = agent->world();

    SideID current_kickable = checkBallKickableSide( agent );

    dlog.addText( Logger::ANALYZER,
                  __FILE__":(doPlayOn) current_kickable is %d",
                  current_kickable );


    const CoachPlayerObject * fastest_player = wm.currentState().fastestInterceptPlayer();

    bool predicted_next_player_is_opp;

    if ( ! fastest_player )
    {
        predicted_next_player_is_opp = true; /* conservative guess */
    }
    else
    {
        dlog.addText( Logger::ANALYZER,
                      "%d: fastest_player unum is %d (%c)",
                      __LINE__,
                      fastest_player->unum(),
                      side_char( fastest_player->side() ) );

predicted_next_player_is_opp = fastest_player->side() == wm.theirSide() ? true : false;
    }

    if ( predicted_next_player_is_opp
         || current_kickable == agent->world().theirSide()
         || ( current_kickable == NEUTRAL
              && agent->world().lastKickerSide() == agent->world().theirSide() ) )
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
CoachMarkAnalyzer::doSetPlayTheirBall( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__":(doSetPlayTheirBall)" );

    checkNearestPlayerSetPlay( agent );

    findOpponentToMarkSetPlayForDefenders( agent );
    findOpponentToMarkSetPlayForTheOthers( agent );
    findSecondOpponentToMarkSetPlay();// agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
CoachMarkAnalyzer::checkNearestPlayerSetPlay( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__"(checkNearestPlayerSetPlay)" );

    const CoachWorldModel & wm = agent->world();

    int unum_kicker = Unum_Unknown;
    const CoachPlayerObject * opp_possible_kicker = wm.currentState().fastestInterceptOpponent();
    if ( opp_possible_kicker )
    {
        unum_kicker = opp_possible_kicker->unum();
    }

    //
    // update nearest matrix setplay
    //
    for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
             end_opp = wm.opponents().end();
         it_opp != end_opp;
         ++it_opp )
    {
        const CoachPlayerObject * p_opp = *it_opp;
        if ( ! p_opp ) continue;
        if ( p_opp->goalie() ) continue;
        if ( p_opp->unum() == unum_kicker ) continue;

        double min_dist2 = ( ServerParam::i().pitchWidth() + ServerParam::i().pitchLength() ) * 10.0;
        min_dist2 *= min_dist2;
        int min_mate_unum = -1;
        for ( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
                 end_mate = wm.teammates().end();
             it_mate != end_mate;
             ++it_mate )
        {
            const CoachPlayerObject * p_mate = *it_mate;
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;

            double dist2 = p_mate->pos().dist2( p_opp->pos() );
            if ( dist2 < min_dist2 )
            {
                min_dist2 = dist2;
                min_mate_unum = p_mate->unum();
            }
        }

        if ( min_mate_unum != -1 )
        {
            // dlog.addText( Logger::ANALYZER,
            //               __FILE__":(checkNearestPlayer) opp->mate  opp=%d mate=%d",
            //               p_opp->unum(), min_mate_unum );
            M_nearest_matrix_setplay[min_mate_unum - 1][p_opp->unum() - 1] += 1;
            M_nearest_sum_setplay[min_mate_unum - 1] += 1;
        }
    }

    for ( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
             end_mate = wm.teammates().end();
         it_mate != end_mate;
         ++it_mate )
    {
        const CoachPlayerObject * p_mate = *it_mate;
        if ( ! p_mate ) continue;
        if( p_mate->goalie() ) continue;

        double min_dist2 = ( ServerParam::i().pitchWidth() + ServerParam::i().pitchLength() ) * 10.0;
        min_dist2 *= min_dist2;
        int min_opp_unum = -1;
        for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
                 end_opp = wm.opponents().end();
             it_opp != end_opp;
             ++it_opp )
        {
            const CoachPlayerObject * p_opp = *it_opp;
            if ( ! p_opp ) continue;
            if ( p_opp->goalie() ) continue;
            if ( p_opp->unum() == unum_kicker ) continue;

            double dist2 = p_opp->pos().dist2( p_mate->pos() );
            if ( dist2 < min_dist2 )
            {
                min_dist2 = dist2;
                min_opp_unum = p_opp->unum();
            }
        }

        if ( min_opp_unum != -1 )
        {
            // dlog.addText( Logger::ANALYZER,
            //               __FILE__"(checkNearestPlayer) mate->opp mate=%d opp=%d",
            //               p_mate->unum(), min_opp_unum );
            M_nearest_matrix_setplay[p_mate->unum() - 1][min_opp_unum - 1] += 1;
            M_nearest_sum_setplay[p_mate->unum() - 1] += 1;
        }
    }

    for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
             end_opp = wm.opponents().end();
         it_opp != end_opp;
         ++it_opp )
    {
        const CoachPlayerObject * p_opp = *it_opp;
        if ( ! p_opp ) continue;
        if ( p_opp->goalie() ) continue;
        if ( p_opp->unum() == unum_kicker ) continue;

        double min_dist2 = ( ServerParam::i().pitchWidth() + ServerParam::i().pitchLength() ) * 10.0;
        min_dist2 *= min_dist2;
        int min_mate_unum = -1;
        for ( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
                 end_mate = wm.teammates().end();
             it_mate != end_mate;
             ++it_mate )
        {
            const CoachPlayerObject * p_mate = *it_mate;
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;

            Vector2D home_pos = CoachStrategy::i().getPosition( p_mate->unum() );

            double dist2 = home_pos.dist2( p_opp->pos() );
            if ( dist2 < min_dist2 )
            {
                min_dist2 = dist2;
                min_mate_unum = p_mate->unum();
            }
        }

        if ( min_mate_unum != -1 )
        {
            // dlog.addText( Logger::ANALYZER,
            //               __FILE__":(checkNearestPlayer) opp->mate  opp=%d mate=%d",
            //               p_opp->unum(), min_mate_unum );
            M_nearest_matrix_setplay[min_mate_unum - 1][p_opp->unum() - 1] += 2; // MAGIC NUMBER
            M_nearest_sum_setplay[min_mate_unum - 1] += 2; // MAGIC NUMBER
        }
    }

    for ( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
             end_mate = wm.teammates().end();
         it_mate != end_mate;
         ++it_mate )
    {
        const CoachPlayerObject * p_mate = *it_mate;
        if ( ! p_mate ) continue;
        if( p_mate->goalie() ) continue;

        Vector2D home_pos = CoachStrategy::i().getPosition( p_mate->unum() );

        double min_dist2 = ( ServerParam::i().pitchWidth() + ServerParam::i().pitchLength() ) * 10.0;
        min_dist2 *= min_dist2;
        int min_opp_unum = -1;
        for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
                 end_opp = wm.opponents().end();
             it_opp != end_opp;
             ++it_opp )
        {
            const CoachPlayerObject * p_opp = *it_opp;
            if ( ! p_opp ) continue;
            if ( p_opp->goalie() ) continue;
            if ( p_opp->unum() == unum_kicker ) continue;

            double dist2 = p_opp->pos().dist2( home_pos );
            if ( dist2 < min_dist2 )
            {
                min_dist2 = dist2;
                min_opp_unum = p_opp->unum();
            }
        }

        if ( min_opp_unum != -1 )
        {
            // dlog.addText( Logger::ANALYZER,
            //               __FILE__"(checkNearestPlayer) mate->opp mate=%d opp=%d",
            //               p_mate->unum(), min_opp_unum );
            M_nearest_matrix_setplay[p_mate->unum() - 1][min_opp_unum - 1] += 2; // MAGIC NUMBER
            M_nearest_sum_setplay[p_mate->unum() - 1] += 2; // MAGIC NUMBER
        }
    }


    dlog.addText( Logger::ANALYZER,
                  __FILE__":(checkNearestPlayer) Teammate-opponent nearest player matrix" );

    for ( int i = 0; i < 11; ++i )
    {
        std::string msg;
        msg.reserve( 128 );
        char buf[13];
        snprintf( buf, 13, "Teammate_setplay[%d] ", i + 1 );
        msg += buf;

        for ( int j = 0; j < 11; ++j )
        {
            char buf[6];
            snprintf( buf, 6, "%d ",
                      M_nearest_matrix_setplay[i][j] );
            msg += buf;
        }
        dlog.addText( Logger::ANALYZER, "%s: %d", msg.c_str(), M_nearest_sum_setplay[i] );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
CoachMarkAnalyzer::findOpponentToMarkSetPlay( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__":(findOpponentToMarkSetPlay)" );

    const CoachWorldModel & wm = agent->world();

    int unum_possible_kicker = Unum_Unknown;
    const CoachPlayerObject * opp_possible_kicker = wm.currentState().fastestInterceptOpponent();
    if ( opp_possible_kicker )
    {
        unum_possible_kicker = opp_possible_kicker->unum();
    }

    for ( int i = 0; i < 11; ++i )
    {
        M_candidate_opponent_unum_to_mark[i] = Unum_Unknown;
    }

    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;

        int max_opp_unum = Unum_Unknown;
        int max_cnt = 0;
        for ( int j = 0; j < 11; ++j )
        {
            if ( j + 1 == unum_possible_kicker ) continue;

            if ( max_cnt < M_nearest_matrix_setplay[i][j] )
            {
                max_opp_unum = j + 1;
                max_cnt = M_nearest_matrix_setplay[i][j];
            }
        }

        if ( max_cnt != 0 )
        {
            M_candidate_opponent_unum_to_mark[i] = max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(1): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // duplication check
    // only the currently nearest player to the target is valid.
    // the other players that are assined to the target are canceled.
    for( CoachPlayerObject::Cont::const_iterator it = wm.opponents().begin(),
             end = wm.opponents().end();
         it != end;
         ++it )
    {
        const CoachPlayerObject * p = *it;
        if ( ! p ) continue;
        if ( p->goalie() ) continue;
        if ( p->unum() == unum_possible_kicker ) continue;

        int unum_tgt = p->unum();
        int max_ratio_index = -1;
        double max_ratio = 0.0;
        for ( int i = 0; i < 11; ++i )
        {
            const CoachPlayerObject * p_mate = wm.teammate( i + 1 );
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;
            if ( M_candidate_opponent_unum_to_mark[i] != unum_tgt )
                continue;

            double ratio = (double)M_nearest_matrix_setplay[i][unum_tgt - 1]
                / M_nearest_sum_setplay[i];
            if( ratio > max_ratio )
            {
                max_ratio = ratio;
                max_ratio_index = i;
            }
        }

        if ( max_ratio_index != -1 )
        {
            for ( int i = 0; i < 11; ++i )
            {
                if ( M_candidate_opponent_unum_to_mark[i] == unum_tgt
                     && i != max_ratio_index )
                {
                    M_candidate_opponent_unum_to_mark[i] = Unum_Unknown;
                }
            }
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(2): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // mark target is determined according to the nearest matrix
    // for those whose target is not determined yet.
    // The procedure is almost the same as the first step.
    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;
        if ( M_candidate_opponent_unum_to_mark[i] != Unum_Unknown )
            continue;

        int max_opp_unum = Unum_Unknown;
        int max_cnt = 0;
        for ( int j = 0; j < 11; ++j )
        {
            if ( j + 1 == unum_possible_kicker ) continue;

            bool used = false;
            for ( int i2 = 0; i2 < 11; ++i2 )
            {
                if ( M_candidate_opponent_unum_to_mark[i2] == j + 1 )
                {
                    used = true;
                    break;
                }
            }
            if ( used == true )
            {
                continue;
            }

            if ( max_cnt < M_nearest_matrix_setplay[i][j] )
            {
                max_opp_unum = j + 1;
                max_cnt = M_nearest_matrix_setplay[i][j];
            }
        }

        if ( max_cnt != 0 )
        {
            M_candidate_opponent_unum_to_mark[i] = max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(3): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // for our players whose target is still unknown
    // the target is determined as the current nearest opponent player
    for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
             end_opp = wm.opponents().end();
         it_opp != end_opp;
         ++it_opp )
    {
        const CoachPlayerObject * p_opp = *it_opp;
        if ( ! p_opp ) continue;
        if ( p_opp->goalie() ) continue;
        int opp_unum = p_opp->unum();

        if ( opp_unum == unum_possible_kicker ) continue;

        // check if the player is already considered as a candidate mark target
        bool used = false;
        for ( int i = 0; i < 11; ++i )
        {
            if ( M_candidate_opponent_unum_to_mark[i] == opp_unum )
            {
                used = true;
                break;
            }
        }
        if ( used == true )
        {
            continue;
        }

        dlog.addText( Logger::ANALYZER,
                      __FILE__":(findOpponentToMark) No teammate is assigned for Opponent %d",
                      opp_unum );

        double min_dist2 = ( ServerParam::i().pitchLength() + ServerParam::i().pitchWidth() ) * 10.0;
        min_dist2 *= min_dist2;
        int mate_unum_nearest = -1;
        for( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
                 end_mate = wm.teammates().end();
             it_mate != end_mate;
             ++it_mate )
        {
            const CoachPlayerObject * p_mate = *it_mate;
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;
            int mate_unum = p_mate->unum();

            dlog.addText( Logger::ANALYZER,
                          __FILE__":(findOpponentToMark) M_c_o_u_t_m[%d] = %d",
                          mate_unum - 1,
                          M_candidate_opponent_unum_to_mark[mate_unum - 1] );

            if ( ! M_do_marking[mate_unum - 1] ) continue;
            if ( M_candidate_opponent_unum_to_mark[mate_unum - 1] != Unum_Unknown )
            {
                continue;
            }

            double dist2 = p_opp->pos().dist2( p_mate->pos() );
            if ( min_dist2 > dist2 )
            {
                min_dist2 = dist2;
                mate_unum_nearest = mate_unum;
            }
        }

        if ( mate_unum_nearest != -1 )
        {
            M_candidate_opponent_unum_to_mark[mate_unum_nearest - 1] = opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark: %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
CoachMarkAnalyzer::findOpponentToMarkSetPlayForDefenders( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__":(findOpponentToMarkSetPlayForDefenders)" );

    const CoachWorldModel & wm = agent->world();

    int unum_possible_kicker = Unum_Unknown;
    const CoachPlayerObject * opp_possible_kicker = wm.currentState().fastestInterceptOpponent();
    if ( opp_possible_kicker )
    {
        unum_possible_kicker = opp_possible_kicker->unum();
    }

    for ( int i = 0; i < 11; ++i )
    {
        M_candidate_opponent_unum_to_mark[i] = Unum_Unknown;
    }

    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;
        if ( ! M_is_defender[i] ) continue;

        int max_opp_unum = Unum_Unknown;
        int max_cnt = 0;
        for ( int j = 0; j < 11; ++j )
        {
            if ( j + 1 == unum_possible_kicker ) continue;

            if ( max_cnt < M_nearest_matrix_setplay[i][j] )
            {
                max_opp_unum = j + 1;
                max_cnt = M_nearest_matrix_setplay[i][j];
            }
        }

        if ( max_cnt != 0 )
        {
            M_candidate_opponent_unum_to_mark[i] = max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(1): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // duplication check
    // only the currently nearest player to the target is valid.
    // the other players that are assined to the target are canceled.
    for( CoachPlayerObject::Cont::const_iterator it = wm.opponents().begin(),
             end = wm.opponents().end();
         it != end;
         ++it )
    {
        const CoachPlayerObject * p = *it;
        if ( ! p ) continue;
        if ( p->goalie() ) continue;
        if ( p->unum() == unum_possible_kicker ) continue;

        int unum_tgt = p->unum();
        int max_ratio_index = -1;
        double max_ratio = 0.0;
        for ( int i = 0; i < 11; ++i )
        {
            const CoachPlayerObject * p_mate = wm.teammate( i + 1 );
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;
            if ( M_candidate_opponent_unum_to_mark[i] != unum_tgt )
                continue;

            double ratio = (double)M_nearest_matrix_setplay[i][unum_tgt - 1]
                / M_nearest_sum_setplay[i];
            if( ratio > max_ratio )
            {
                max_ratio = ratio;
                max_ratio_index = i;
            }
        }

        if ( max_ratio_index != -1 )
        {
            for ( int i = 0; i < 11; ++i )
            {
                if ( M_candidate_opponent_unum_to_mark[i] == unum_tgt
                     && i != max_ratio_index )
                {
                    M_candidate_opponent_unum_to_mark[i] = Unum_Unknown;
                }
            }
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(2): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // mark target is determined according to the nearest matrix
    // for those whose target is not determined yet.
    // The procedure is almost the same as the first step.
    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;
        if ( ! M_is_defender[i] ) continue;
        if ( M_candidate_opponent_unum_to_mark[i] != Unum_Unknown )
            continue;

        int max_opp_unum = Unum_Unknown;
        int max_cnt = 0;
        for ( int j = 0; j < 11; ++j )
        {
            if ( j + 1 == unum_possible_kicker ) continue;

            bool used = false;
            for ( int i2 = 0; i2 < 11; ++i2 )
            {
                if ( M_candidate_opponent_unum_to_mark[i2] == j + 1 )
                {
                    used = true;
                    break;
                }
            }
            if ( used == true )
            {
                continue;
            }

            if ( max_cnt < M_nearest_matrix_setplay[i][j] )
            {
                max_opp_unum = j + 1;
                max_cnt = M_nearest_matrix_setplay[i][j];
            }
        }

        if ( max_cnt != 0 )
        {
            M_candidate_opponent_unum_to_mark[i] = max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(3): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // for our players whose target is still unknown
    // the target is determined as the current nearest opponent player
    for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
             end_opp = wm.opponents().end();
         it_opp != end_opp;
         ++it_opp )
    {
        const CoachPlayerObject * p_opp = *it_opp;
        if ( ! p_opp ) continue;
        if ( p_opp->goalie() ) continue;
        int opp_unum = p_opp->unum();

        if ( opp_unum == unum_possible_kicker ) continue;

        // check if the player is already considered as a candidate mark target
        bool used = false;
        for ( int i = 0; i < 11; ++i )
        {
            if ( M_candidate_opponent_unum_to_mark[i] == opp_unum )
            {
                used = true;
                break;
            }
        }
        if ( used == true )
        {
            continue;
        }

        dlog.addText( Logger::ANALYZER,
                      __FILE__":(findOpponentToMark) No teammate is assigned for Opponent %d",
                      opp_unum );

        double min_dist2 = ( ServerParam::i().pitchLength() + ServerParam::i().pitchWidth() ) * 10.0;
        min_dist2 *= min_dist2;
        int mate_unum_nearest = -1;
        for( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
                 end_mate = wm.teammates().end();
             it_mate != end_mate;
             ++it_mate )
        {
            const CoachPlayerObject * p_mate = *it_mate;
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;
            int mate_unum = p_mate->unum();

            dlog.addText( Logger::ANALYZER,
                          __FILE__":(findOpponentToMark) M_c_o_u_t_m[%d] = %d",
                          mate_unum - 1,
                          M_candidate_opponent_unum_to_mark[mate_unum - 1] );

            if ( ! M_do_marking[mate_unum - 1] ) continue;
            if ( ! M_is_defender[mate_unum - 1] ) continue;

            if ( M_candidate_opponent_unum_to_mark[mate_unum - 1] != Unum_Unknown )
            {
                continue;
            }

            double dist2 = p_opp->pos().dist2( p_mate->pos() );
            if ( min_dist2 > dist2 )
            {
                min_dist2 = dist2;
                mate_unum_nearest = mate_unum;
            }
        }

        if ( mate_unum_nearest != -1 )
        {
            M_candidate_opponent_unum_to_mark[mate_unum_nearest - 1] = opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark: %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
CoachMarkAnalyzer::findOpponentToMarkSetPlayForTheOthers( CoachAgent * agent )
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__":(findOpponentToMarkSetPlayForTheOthers)" );

    const CoachWorldModel & wm = agent->world();

    int unum_possible_kicker = Unum_Unknown;
    const CoachPlayerObject * opp_possible_kicker = wm.currentState().fastestInterceptOpponent();
    if ( opp_possible_kicker )
    {
        unum_possible_kicker = opp_possible_kicker->unum();
    }

    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_is_defender[i] )
        {
            M_candidate_opponent_unum_to_mark[i] = Unum_Unknown;
        }
    }

    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;
        if ( M_is_defender[i] ) continue;

        int max_opp_unum = Unum_Unknown;
        int max_cnt = 0;
        for ( int j = 0; j < 11; ++j )
        {
            if ( j + 1 == unum_possible_kicker ) continue;

            if ( max_cnt < M_nearest_matrix_setplay[i][j] )
            {
                max_opp_unum = j + 1;
                max_cnt = M_nearest_matrix_setplay[i][j];
            }
        }

        if ( max_cnt != 0 )
        {
            M_candidate_opponent_unum_to_mark[i] = max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(1): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // duplication check
    // only the currently nearest player to the target is valid.
    // the other players that are assined to the target are canceled.
    for( CoachPlayerObject::Cont::const_iterator it = wm.opponents().begin(),
             end = wm.opponents().end();
         it != end;
         ++it )
    {
        const CoachPlayerObject * p = *it;
        if ( ! p ) continue;
        if ( p->goalie() ) continue;
        if ( p->unum() == unum_possible_kicker ) continue;

        int unum_tgt = p->unum();
        int max_ratio_index = -1;
        double max_ratio = 0.0;
        for ( int i = 0; i < 11; ++i )
        {
            const CoachPlayerObject * p_mate = wm.teammate( i + 1 );
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;
            if ( M_candidate_opponent_unum_to_mark[i] != unum_tgt )
                continue;

            double ratio = (double)M_nearest_matrix_setplay[i][unum_tgt - 1]
                / M_nearest_sum_setplay[i];
            if( ratio > max_ratio )
            {
                max_ratio = ratio;
                max_ratio_index = i;
            }
        }

        if ( max_ratio_index != -1 )
        {
            for ( int i = 0; i < 11; ++i )
            {
                if ( M_candidate_opponent_unum_to_mark[i] == unum_tgt
                     && i != max_ratio_index )
                {
                    M_candidate_opponent_unum_to_mark[i] = Unum_Unknown;
                }
            }
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(2): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // mark target is determined according to the nearest matrix
    // for those whose target is not determined yet.
    // The procedure is almost the same as the first step.
    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;
        if ( M_is_defender[i] ) continue;
        if ( M_candidate_opponent_unum_to_mark[i] != Unum_Unknown )
            continue;

        int max_opp_unum = Unum_Unknown;
        int max_cnt = 0;
        for ( int j = 0; j < 11; ++j )
        {
            if ( j + 1 == unum_possible_kicker ) continue;

            bool used = false;
            for ( int i2 = 0; i2 < 11; ++i2 )
            {
                if ( M_candidate_opponent_unum_to_mark[i2] == j + 1 )
                {
                    used = true;
                    break;
                }
            }
            if ( used == true )
            {
                continue;
            }

            if ( max_cnt < M_nearest_matrix_setplay[i][j] )
            {
                max_opp_unum = j + 1;
                max_cnt = M_nearest_matrix_setplay[i][j];
            }
        }

        if ( max_cnt != 0 )
        {
            M_candidate_opponent_unum_to_mark[i] = max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark(3): %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    // for our players whose target is still unknown
    // the target is determined as the current nearest opponent player
    for( CoachPlayerObject::Cont::const_iterator it_opp = wm.opponents().begin(),
             end_opp = wm.opponents().end();
         it_opp != end_opp;
         ++it_opp )
    {
        const CoachPlayerObject * p_opp = *it_opp;
        if ( ! p_opp ) continue;
        if ( p_opp->goalie() ) continue;
        int opp_unum = p_opp->unum();

        if ( opp_unum == unum_possible_kicker ) continue;

        // check if the player is already considered as a candidate mark target
        bool used = false;
        for ( int i = 0; i < 11; ++i )
        {
            if ( M_candidate_opponent_unum_to_mark[i] == opp_unum )
            {
                used = true;
                break;
            }
        }
        if ( used == true )
        {
            continue;
        }

        dlog.addText( Logger::ANALYZER,
                      __FILE__":(findOpponentToMark) No teammate is assigned for Opponent %d",
                      opp_unum );

        double min_dist2 = ( ServerParam::i().pitchLength() + ServerParam::i().pitchWidth() ) * 10.0;
        min_dist2 *= min_dist2;
        int mate_unum_nearest = -1;
        for( CoachPlayerObject::Cont::const_iterator it_mate = wm.teammates().begin(),
                 end_mate = wm.teammates().end();
             it_mate != end_mate;
             ++it_mate )
        {
            const CoachPlayerObject * p_mate = *it_mate;
            if ( ! p_mate ) continue;
            if ( p_mate->goalie() ) continue;
            int mate_unum = p_mate->unum();

            dlog.addText( Logger::ANALYZER,
                          __FILE__":(findOpponentToMark) M_c_o_u_t_m[%d] = %d",
                          mate_unum - 1,
                          M_candidate_opponent_unum_to_mark[mate_unum - 1] );

            if ( ! M_do_marking[mate_unum - 1] ) continue;
            if ( M_is_defender[mate_unum - 1] ) continue;

            if ( M_candidate_opponent_unum_to_mark[mate_unum - 1] != Unum_Unknown )
            {
                continue;
            }

            double dist2 = p_opp->pos().dist2( p_mate->pos() );
            if ( min_dist2 > dist2 )
            {
                min_dist2 = dist2;
                mate_unum_nearest = mate_unum;
            }
        }

        if ( mate_unum_nearest != -1 )
        {
            M_candidate_opponent_unum_to_mark[mate_unum_nearest - 1] = opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current mark: %d %d %d %d %d %d %d %d %d %d %d",
                  M_candidate_opponent_unum_to_mark[0],
                  M_candidate_opponent_unum_to_mark[1],
                  M_candidate_opponent_unum_to_mark[2],
                  M_candidate_opponent_unum_to_mark[3],
                  M_candidate_opponent_unum_to_mark[4],
                  M_candidate_opponent_unum_to_mark[5],
                  M_candidate_opponent_unum_to_mark[6],
                  M_candidate_opponent_unum_to_mark[7],
                  M_candidate_opponent_unum_to_mark[8],
                  M_candidate_opponent_unum_to_mark[9],
                  M_candidate_opponent_unum_to_mark[10]
                  );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
CoachMarkAnalyzer::findSecondOpponentToMarkSetPlay()
{
    dlog.addText( Logger::ANALYZER,
                  __FILE__":(findSecondOpponentToMarkSetPlay)" );

    // const CoachWorldModel & wm = agent->world();

    for ( int i = 0; i < 11; ++i )
    {
        M_second_opponent_unum_to_mark[i] = Unum_Unknown;
    }

    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_do_marking[i] ) continue;

        int second_max_opp_unum = Unum_Unknown;
        int second_max_cnt = -1;
        for ( int j = 0; j < 11; ++j )
        {
            if ( j + 1 == M_candidate_opponent_unum_to_mark[i] )
                continue;

            if ( second_max_cnt < M_nearest_matrix_setplay[i][j] )
            {
                second_max_opp_unum = j + 1;
                second_max_cnt = M_nearest_matrix_setplay[i][j];
            }
        }

        if ( second_max_cnt != -1 )
        {
            M_second_opponent_unum_to_mark[i] = second_max_opp_unum;
        }
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__": Current second mark: %d %d %d %d %d %d %d %d %d %d %d",
                  M_second_opponent_unum_to_mark[0],
                  M_second_opponent_unum_to_mark[1],
                  M_second_opponent_unum_to_mark[2],
                  M_second_opponent_unum_to_mark[3],
                  M_second_opponent_unum_to_mark[4],
                  M_second_opponent_unum_to_mark[5],
                  M_second_opponent_unum_to_mark[6],
                  M_second_opponent_unum_to_mark[7],
                  M_second_opponent_unum_to_mark[8],
                  M_second_opponent_unum_to_mark[9],
                  M_second_opponent_unum_to_mark[10]
                  );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
CoachMarkAnalyzer::saveOpponentData()
{
    dlog.addText( Logger::TEAM,
                  "(CoachMarkAnalyzer::saveOpponentData)" );

    const CoachAnalyzerManager & cam = CoachAnalyzerManager::i();

    if ( cam.opponentNickname().empty() )
    {
        return false;
    }

    std::string file_path = cam.opponentDataDir();
    if ( ! file_path.empty()
         && *file_path.rbegin() != '/' )
    {
        file_path += '/';
    }
    file_path += cam.opponentNickname();
    file_path += ".mark";

    std::ofstream ofs( file_path.c_str() );

    if ( ! ofs.is_open() )
    {
        std::cerr << "Could not save the opponent formation file [" << file_path
                  << "]" << std::endl;
        return false;
    }


    for ( int i = 0; i < 11; ++i )
    {
        for ( int j = 0; j < 11; ++j )
        {
            ofs << M_nearest_matrix[i][j] / 2;

            if ( j == 10 )
            {
                ofs << '\n';
            }
            else
            {
                ofs << ' ';
            }
        }
    }

    for ( int i = 0; i < 11; ++i )
    {
        for ( int j = 0; j < 11; ++j )
        {
            ofs << M_nearest_matrix_setplay[i][j] / 2;

            if ( j == 10 )
            {
                ofs << '\n';
            }
            else
            {
                ofs << ' ';
            }
        }
    }

    ofs.flush();

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
CoachMarkAnalyzer::loadOpponentData()
{
    dlog.addText( Logger::TEAM,
                  __FILE__": CoachMarkAnalyzer::loadOpponentData" );

    const CoachAnalyzerManager & cam = CoachAnalyzerManager::i();

    if ( cam.opponentNickname().empty() )
    {
        return false;
    }

    std::string file_path = cam.opponentDataDir();
    if ( ! file_path.empty()
         && *file_path.rbegin() != '/' )
    {
        file_path += '/';
    }
    file_path += cam.opponentNickname();
    file_path += ".mark";

    std::ifstream ifs( file_path.c_str() );

    if ( ! ifs.is_open() )
    {
        std::cerr << "Could not load the opponent mark file [" << file_path
                  << "]" << std::endl;
        return false;
    }

    int loaded_value = 0;

    for ( int i = 0; i < 11; ++i )
    {
        for ( int j = 0; j < 11; ++j )
        {
            ifs >> loaded_value;

            M_nearest_matrix[i][j] += loaded_value;

            M_nearest_sum[i] += loaded_value;
        }
    }

    for ( int i = 0; i < 11; ++i )
    {
        for ( int j = 0; j < 11; ++j )
        {
            ifs >> loaded_value;

            M_nearest_matrix_setplay[i][j] += loaded_value;

            M_nearest_sum_setplay[i] += loaded_value;
        }
    }

    return true;
}
