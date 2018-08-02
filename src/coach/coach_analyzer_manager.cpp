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
#include <config.h>
#endif

#include "coach_strategy.h"
#include "coach_analyzer_manager.h"

#include <rcsc/coach/coach_agent.h>
#include <rcsc/common/logger.h>

#include <iostream>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
CoachAnalyzerManager &
CoachAnalyzerManager::instance()
{
    static CoachAnalyzerManager s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
CoachAnalyzerManager::CoachAnalyzerManager()
    : M_goalie_unum_changed( false ),
      M_cornerkick_analyzed( false ),
      M_our_goalie_unum( Unum_Unknown ),
      M_their_goalie_unum( Unum_Unknown ),
      M_marked_count( 0 ),
      M_agent2d_count( 0 ),
      M_opponent_formation_time( -1, 0 ),
      M_opponent_nickname(),
      M_opponent_cornerkick_formation()
{
    for ( int i = 0; i < 11; ++i )
    {
        M_opponent_formation[i] = UNKNOWN;


        for ( int j = 0; j < 2; ++j )
        {
            M_mark_assignment[i][j] = Unum_Unknown;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
CoachAnalyzerManager::init( const std::string & opponent_data_dir )
{
    M_opponent_data_dir = opponent_data_dir;

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CoachAnalyzerManager::setGoalieUnum( const int our_unum,
                                     const int their_unum )
{
    if ( 1 <= our_unum && our_unum <= 11 )
    {
        if ( M_our_goalie_unum != our_unum )
        {
            M_goalie_unum_changed = true;
            M_our_goalie_unum = our_unum;
        }
    }

    if ( 1 <= their_unum && their_unum <= 11 )
    {
        if ( M_their_goalie_unum != our_unum )
        {
            M_goalie_unum_changed = true;
            M_their_goalie_unum = their_unum;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CoachAnalyzerManager::setMarkAssignment( const int our_unum,
                                         const int first_target_unum,
                                         const int second_target_unum )
{
    if ( our_unum < 1 || 11 < our_unum
         || ( first_target_unum != Unum_Unknown
              && ( first_target_unum < 1 || 11 < first_target_unum ) )
         || ( second_target_unum != Unum_Unknown
              && ( second_target_unum < 1 || 11 < second_target_unum ) )
         )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(setMarkAssignment) illegal uniform number. "
                  << our_unum << ' ' << first_target_unum << ' ' << second_target_unum
                  << std::endl;
        return;
    }

    M_mark_assignment[our_unum - 1][0] = first_target_unum;
    M_mark_assignment[our_unum - 1][1] = second_target_unum;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
CoachAnalyzerManager::analyzeOpponentNickname( const CoachWorldModel & wm )
{
    // opponent team name analysis
    if ( wm.theirTeamName().compare( 0, 4, "WE20" ) == 0
              || wm.theirTeamName().compare( 0, 11, "WrightEagle" ) == 0 )
    {
        M_opponent_nickname = "wrighteagle";
    }
    else if ( wm.theirTeamName().compare( 0, 6, "Apollo" ) == 0 )
    {
        M_opponent_nickname = "apollo";
    }
    else if ( wm.theirTeamName().compare( 0, 7, "Bahia2D" ) == 0 )
    {
        M_opponent_nickname = "bahia";
    }
    else if ( wm.theirTeamName().compare( 0, 7, "FC_Pars" ) == 0 )
    {
        M_opponent_nickname = "fcpars";
    }
    else if ( wm.theirTeamName().compare( 0, 12, "Fifty_Storms" ) == 0 )
    {
        M_opponent_nickname = "fiftystorms";
    }
    else if ( wm.theirTeamName().compare( 0, 10, "HfutEngine" ) == 0
              || wm.theirTeamName().compare( 0, 10, "HFUT_FINAL" ) == 0 )
    {
        M_opponent_nickname = "hfutengine";
    }
    else if ( wm.theirTeamName().compare( 0, 4, "RaiC" ) == 0
              || wm.theirTeamName().compare( 0, 6, "raic11" ) == 0 )
    {
        M_opponent_nickname = "raic";
    }
    else if ( wm.theirTeamName().compare( 0, 6, "Unique" ) == 0 )
    {
        M_opponent_nickname = "unique";
    }
    else if ( wm.theirTeamName().compare( 0, 3, "AUA" ) == 0
              || wm.theirTeamName().compare( 0, 7, "AUA2011" ) == 0 )
    {
        M_opponent_nickname = "aua";
    }
    else if ( wm.theirTeamName().compare( 0, 7, "ESKILAS" ) == 0 )
    {
        M_opponent_nickname = "eskilas";
    }
    else if ( wm.theirTeamName().compare( 0, 10, "FCPortugal" ) == 0 )
    {
        M_opponent_nickname = "fcportugal";
    }
    else if ( wm.theirTeamName().compare( 0, 8, "HELIOS20" ) == 0
              || wm.theirTeamName() == "HELIOS" )
    {
        M_opponent_nickname = "helios";
    }
    else if ( wm.theirTeamName().compare( 0, 4, "Iran" ) == 0 )
    {
        M_opponent_nickname = "iran";
    }
    else if ( wm.theirTeamName().compare( 0, 7, "Nemesis" ) == 0 )
    {
        M_opponent_nickname = "nemesis";
    }
    else if ( wm.theirTeamName().compare( 0, 4, "Oxsy" ) == 0 )
    {
        M_opponent_nickname = "oxsy";
    }
    else if ( wm.theirTeamName().compare( 0, 6, "Ri-one" ) == 0
              || wm.theirTeamName().compare( 0, 10, "Ri-one2011" ) == 0 )
    {
        M_opponent_nickname = "rione";
    }
    else if ( wm.theirTeamName().compare( 0, 6, "Unique" ) == 0
              || wm.theirTeamName().compare( 0, 6, "unique" ) == 0 )
    {
        M_opponent_nickname = "unique";
    }
    else if ( wm.theirTeamName().compare( 0, 9, "DAInamite" ) == 0 )
    {
        M_opponent_nickname = "dainamite";
    }
    else if ( wm.theirTeamName().compare( 0, 11, "AbouAliSina" ) == 0 )
    {
        M_opponent_nickname = "aboualisina";
    }
    else if ( wm.theirTeamName().compare( 0, 8, "NADCO_2D" ) == 0 )
    {
        M_opponent_nickname = "nadco";
    }
    else if ( wm.theirTeamName().compare( 0, 6, "Photon" ) == 0 )
    {
        M_opponent_nickname = "photon";
    }
    else if ( wm.theirTeamName().compare( 0, 6, "Marlik" ) == 0 )
    {
        M_opponent_nickname = "marlik";
    }
    else if ( wm.theirTeamName().compare( 0, 10, "WARTHOGSIM" ) == 0 )
    {
        M_opponent_nickname = "warthogsim";
    }
    else if ( wm.theirTeamName().compare( 0, 8, "ROSEMARY" ) == 0 )
    {
        M_opponent_nickname = "warthogsim";
    }
    else if ( wm.theirTeamName().compare( 0, 11, "ArtSapience" ) == 0 )
    {
        M_opponent_nickname = "artsapience";
    }
    else if ( wm.theirTeamName().compare( 0, 12, "Edinferno_2D" ) == 0 )
    {
        M_opponent_nickname = "artsapience";
    }
    else
    {
        M_opponent_nickname.clear();
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CoachAnalyzerManager::analyzeOpponentCornerKickFormation( const CoachWorldModel & wm )
{
    if( wm.getSetPlayCount() < 30 ) return;

    bool isMark = false;
    for( CoachPlayerObject::Cont::const_iterator it = wm.teammates().begin(),
             end = wm.teammates().end();
         it != end;
         ++it )
    {
        const CoachPlayerObject * mate = *it;
        if ( ! mate ) continue;
        if ( mate->goalie() ) continue;

        if( mate->pos().dist( wm.ball().pos() ) < 20.0
            && mate->pos().dist( wm.ball().pos() ) > 13.0 )
        {
            isMark = false;
            for( CoachPlayerObject::Cont::const_iterator it2 = wm.opponents().begin(),
                     end = wm.opponents().end();
                 it2 != end;
                 ++it2 )
            {
                const CoachPlayerObject * opp = *it2;
                if ( ! opp ) continue;
                if ( opp->goalie() ) continue;

                if( mate->pos().dist( opp->pos() ) < 3.0 )
                {
                    isMark = true;
                }

            }

            if( ! isMark )
            {
                M_marked_count = 0;
                break;
            }
        }

    }

    if( isMark )
    {
        ++ M_marked_count;
        if( M_marked_count > 5 )
        {
            M_opponent_cornerkick_formation = "marked";
            M_cornerkick_analyzed = true;
        }
    }

    bool isAgent2d = false;

    Formation::ConstPtr f = CoachStrategy::i().getAgent2dSetplayDefenseFormation();

    for( CoachPlayerObject::Cont::const_iterator it = wm.opponents().begin(),
             end = wm.opponents().end();
         it != end;
         ++it )
    {
        const CoachPlayerObject * opp = *it;
        if ( ! opp ) continue;
        if ( opp->goalie() ) continue;

        if( opp->pos().x > 40.0 )
        {

            if( opp->pos().dist( f->getPosition( opp->unum(), wm.ball().pos().reversedVector() ).reversedVector() ) < 2.0 )
            {
                isAgent2d = true;
            }

            if( ! isAgent2d )
            {
                M_agent2d_count = 0;
                break;
            }
        }
    }


    if( isAgent2d )
    {
        ++ M_agent2d_count;
        if( M_agent2d_count > 5 )
        {
            M_opponent_cornerkick_formation = "agent2d";
            M_cornerkick_analyzed = true;
        }
    }


}

/*-------------------------------------------------------------------*/
/*!

 */
