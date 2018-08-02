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

#include "opponent_formation_analyzer.h"
#include "coach_analyzer_manager.h"

#include "default_freeform_message.h"

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/coach/coach_agent.h>
#include <rcsc/coach/coach_world_model.h>

using namespace rcsc;

// #define DEBUG_PRINT_COUT
#define DEBUG_ANALYZE_FORMATION

namespace {

struct OpponentYCoordinateSorter {
    const CoachWorldModel & wm_;

    OpponentYCoordinateSorter( const CoachWorldModel & wm )
        : wm_( wm )
      { }

    bool operator()( const int lhs,
                     const int rhs ) const
      {
          const CoachPlayerObject * l = wm_.opponent( lhs );
          const CoachPlayerObject * r = wm_.opponent( rhs );
          return ( l
                   && r
                   && l->pos().y < r->pos().y );
      }
};

}

/*-------------------------------------------------------------------*/
/*!

 */
OpponentFormationAnalyzer::OpponentFormationAnalyzer()
    : M_cycle_last_sent( -1000 ), M_count_modified( 0 )
{
    for ( int i = 0; i < 11; ++i )
    {
        M_count_forward[i] = 0;
        M_count_midfielder[i] = 0;
        M_count_defender[i] = 0;

        M_opponent_formation[i] = CoachAnalyzerManager::UNKNOWN;
        M_formation_modified[i] = false;
    }

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentFormationAnalyzer::analyze( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "(OpponentFormationAnalyzer::analyze)" );

    const CoachWorldModel & wm = agent->world();

    if ( wm.gameMode().type() == GameMode::PlayOn )
    {
        doPlayOn( agent );
    }

    doSendFreeform( agent );

    doReportToAnalyzerManager( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentFormationAnalyzer::doReportToAnalyzerManager( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "(OpponentFormationAnalyzer::doReportToAnalyzerManager)" );

    const CoachWorldModel & wm = agent->world();

    CoachAnalyzerManager::i().setOpponentFormationTime( wm.time() );

    for ( int unum = 1; unum <= 11; ++unum )
    {
        CoachAnalyzerManager::i().setOpponentFormation( unum,
                                                        M_opponent_formation[unum - 1] );
    }

    return true;
}
/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentFormationAnalyzer::doPlayOn( CoachAgent * agent )
{
    const CoachWorldModel & wm = agent->world();

    std::vector< int > unum_fw;
    std::vector< int > unum_mf;
    unum_fw.reserve( 10 );
    unum_mf.reserve( 10 );

    findForwardOpponents( agent, unum_fw );
    findMidfielderOpponents( agent, unum_fw, unum_mf );

    bool checked[11];
    for ( int i = 0; i < 11; ++i )
    {
        checked[i] = false;
    }

    const long weight = wm.time().cycle() * 2; // = 1; MAGIC NUMBER

    // std::cout << "Current FW: ";
    for ( std::vector< int >::iterator it = unum_fw.begin(),
              end = unum_fw.end();
          it != end;
          ++it )
    {
        M_count_forward[*it-1] += weight;
        checked[*it-1] = true;
        //std::cout << *it << " ";
    }
    //std::cout << std::endl;

    //std::cout << "Current MF: ";
    for ( std::vector< int >::iterator it = unum_mf.begin(),
              end = unum_mf.end();
          it != end;
          ++it )
    {
        M_count_midfielder[*it-1] += weight;
        checked[*it-1] = true;
        //std::cout << *it << " ";
    }
    //std::cout << std::endl;

    //std::cout << "Current DF: ";
    for ( int i = 0; i < 11; ++i )
    {
        // assign the remainder as defenders
        if ( checked[i] == false )
        {
            M_count_defender[i] += weight;
            //std::cout << i+1 << " ";
        }
    }
    //std::cout << std::endl;

    //! Example of identifying role (fw, mf, or df)
    for ( int i = 0 ; i < 11 ; ++i )
    {
        if ( M_count_forward[i] >= M_count_midfielder[i]
             && M_count_forward[i] >= M_count_defender[i] )
        {
            if ( M_opponent_formation[i] != CoachAnalyzerManager::FORWARD )
            {
#ifdef DEBUG_PRINT_COUT
                std::cout << wm.ourTeamName() << " coach: " << wm.time()
                          << " determined opponent " << i + 1
                          << " formation = FW" << std::endl;
#endif
                agent->debugClient().addMessage( "Opp(%d):FW", i + 1 );

                if ( M_formation_modified[i] == false )
                {
                    ++M_count_modified;
                    M_formation_modified[i] = true;
                }
            }

            M_opponent_formation[i] = CoachAnalyzerManager::FORWARD; // FW
        }
        else if ( M_count_midfielder[i] >= M_count_forward[i]
                  && M_count_midfielder[i] >= M_count_defender[i] )
        {
            if ( M_opponent_formation[i] != CoachAnalyzerManager::MIDFIELDER )
            {
#ifdef DEBUG_PRINT_COUT
                std::cout << wm.ourTeamName() << " coach: " << wm.time()
                          << " determined opponent " << i+1
                          << " formation = MF" << std::endl;
#endif
                agent->debugClient().addMessage( "Opp(%d):MF", i + 1 );

                if ( M_formation_modified[i] == false )
                {
                    ++M_count_modified;
                    M_formation_modified[i] = true;
                }
            }

            M_opponent_formation[i] = CoachAnalyzerManager::MIDFIELDER; // MF
        }
        else
        {
            if ( M_opponent_formation[i] != CoachAnalyzerManager::OTHER )
            {
#ifdef DEBUG_PRINT_COUT
                std::cout << wm.ourTeamName() << " coach: " << wm.time()
                          << " determined opponent " << i+1
                          << " formation = DF" << std::endl;
#endif
                agent->debugClient().addMessage( "Opp(%d):DF", i + 1 );

                if ( M_formation_modified[i] == false )
                {
                    ++M_count_modified;
                    M_formation_modified[i] = true;
                }
            }

            M_opponent_formation[i] = CoachAnalyzerManager::OTHER; // DF
        }

        dlog.addText( Logger::TEAM,
                      __FILE__": cnt: unum( %d): fw( %d ) mf( %d ) df( %d ): label = %d",
                      i + 1,
                      M_count_forward[i],
                      M_count_midfielder[i],
                      M_count_defender[i],
                      M_opponent_formation[i] );

        // std::cout << "[" << i+1 << "]: ( " << M_count_fw[i] << " ), ( " << M_count_mf[i] << " ), ( " << M_count_df[i] << " ): Identified posision " << M_opp_formation[i] << std::endl;
    }

    // std::cout << std::endl << config().teamName() << " coach: " << wm.time() << " M_count_modified = " << M_count_modified << std::endl;

    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentFormationAnalyzer::doSendFreeform( CoachAgent * agent )
{
    /*
      format:
      "(of OOOOOOMMFFF)"
      ->
      (say (freeform "(of ...)"))

      D: DF
      M: MF
      F: FW
    */

    dlog.addText( Logger::TEAM,
                  "(OpponentFormationAnalyzer::doSendFreeform)" );

    const CoachWorldModel & wm = agent->world();

    // send the opponent formation information in a freeform format
    if ( wm.time().cycle() > 200
         && agent->config().useFreeform()
         && wm.canSendFreeform()
         && M_count_modified > 0
         && ( wm.time().cycle() - M_cycle_last_sent > 500 ))
    {
#if 0
        std::string msg;
        msg.reserve( 128 );

        msg = "(opponent_formation ";

        for ( int unum = 1; unum <= 11; ++unum )
        {
            char buf[8];
            snprintf( buf, 8, "(%d %d)",
                      unum, static_cast< int >( M_opponent_formation[unum - 1] ) );
            msg += buf;

            M_formation_modified[unum - 1] = false;
        }
        M_count_modified = 0;

        msg += ")";

        agent->doSayFreeform( msg );

        std::cout << agent->config().teamName() << " coach: " << wm.time()
                  << " send freeform " << msg
                  << std::endl;
#endif


        boost::shared_ptr< FreeformMessage > ptr( new OpponentFormationMessage( M_opponent_formation[0],
                                                                                M_opponent_formation[1],
                                                                                M_opponent_formation[2],
                                                                                M_opponent_formation[3],
                                                                                M_opponent_formation[4],
                                                                                M_opponent_formation[5],
                                                                                M_opponent_formation[6],
                                                                                M_opponent_formation[7],
                                                                                M_opponent_formation[8],
                                                                                M_opponent_formation[9],
                                                                                M_opponent_formation[10] ) );
        agent->addFreeformMessage( ptr );
#if 1
        std::cout << agent->config().teamName() << " coach: "  << agent->world().time()
                  << " send freeform ";

        std::cout << "(opponent_formation ";
        for ( int i = 0; i < 11; ++i )
        {
            std::cout << '(' << i + 1 << ' ' << M_opponent_formation[i] << ')';
        }
        std::cout << ')' << std::endl;

        std::string msg;
        msg.reserve( 128 );
        ptr->append( msg );
        agent->debugClient().addMessage( msg );
#endif


        M_cycle_last_sent = wm.time().cycle();
    }

    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
void
OpponentFormationAnalyzer::findForwardOpponents( CoachAgent * agent,
                                                 std::vector< int > & unum_list )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": OpponentFormationAnalyzer::findForwardOpponents" );

    const ServerParam & SP = ServerParam::i();
    const CoachWorldModel & wm = agent->world();

    double opp_front_x = SP.pitchHalfLength();
    for ( int unum = 1; unum <= 11; ++unum )
    {
        const CoachPlayerObject * p = wm.opponent( unum );
        if ( ! p ) continue;
        if ( p->goalie() ) continue;

        if ( p->pos().x < opp_front_x )
        {
            opp_front_x = p->pos().x;
        }
    }

    std::vector< int > forward_opp;

    for ( CoachPlayerObject::Cont::const_iterator it = wm.opponents().begin(),
              end = wm.opponents().end();
          it != end;
          ++it )
    {
        const CoachPlayerObject * p = *it;

        if ( ! p ) continue;
        if ( p->goalie() ) continue;

        if ( p->pos().x > opp_front_x - 1.0 )
        {
            bool opp_is_forward = true;
            Vector2D sector_point( opp_front_x - 0.1, p->pos().y );
            Sector2D sector( sector_point, 0.0,
                             ( p->pos() - sector_point ).r(),
                             AngleDeg( -90.0 ), AngleDeg( 90.0 ) );


            for ( CoachPlayerObject::Cont::const_iterator it2 = wm.opponents().begin(),
                      end2 = wm.opponents().end();
                  it2 != end2;
                  ++it2 )
            {
                const CoachPlayerObject * p2 = *it2;

                if ( p->unum() == p2->unum() ) continue;

                if ( sector.contains( p2->pos() ) )
                {
                    opp_is_forward = false;
                    break;
                }
            }

            if ( opp_is_forward )
            {
                forward_opp.push_back( p->unum() );
            }
        }
    }

    std::vector< int > tmp_unum_list = forward_opp;
    std::sort( tmp_unum_list.begin(), tmp_unum_list.end(), OpponentYCoordinateSorter( wm ) );

    std::vector< Vector2D > opp_pos;
    opp_pos.reserve( forward_opp.size() );

    for ( std::vector< int >::iterator unum = tmp_unum_list.begin(),
              end = tmp_unum_list.end();
          unum != end;
          ++unum )
    {
        const CoachPlayerObject * p = wm.opponent( *unum );
        if ( p
             && p->pos().absY() < SP.pitchHalfWidth() )
        {
            unum_list.push_back( *unum );
            opp_pos.push_back( p->pos() );
        }
    }

#ifdef DEBUG_ANALYZE_FORMATION
    for( size_t i = 1; i < opp_pos.size(); i++ )
    {
        agent->debugClient().addLine( opp_pos[i - 1], opp_pos[i], "#A00" );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
OpponentFormationAnalyzer::findMidfielderOpponents( CoachAgent * agent,
                                                    const std::vector< int > & unum_fw,
                                                    std::vector< int > & unum_list )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": OpponentFormationAnalyzer::findMidfielderOpponents" );

    std::vector< int > offensive_opps;
    offensive_opps.reserve( 10 );

    findOffensiveOpponents( agent, offensive_opps );


    std::vector< int > candidate_mf;

    for ( std::vector< int >::const_iterator it = offensive_opps.begin(),
              end = offensive_opps.end();
          it != end;
          ++it )
    {
        if ( std::find( unum_fw.begin(), unum_fw.end(), *it ) != unum_fw.end() )
        {
            continue;
        }

        candidate_mf.push_back( *it );
    }

    // sorted by y-coordinate

    const ServerParam & SP = ServerParam::i();
    const CoachWorldModel & wm = agent->world();

    std::vector< int > tmp_unum_list = candidate_mf;
    std::sort( tmp_unum_list.begin(), tmp_unum_list.end(), OpponentYCoordinateSorter( wm ) );

    std::vector< Vector2D > pos_mf;
    pos_mf.reserve( candidate_mf.size() );

    for ( std::vector< int >::iterator unum = tmp_unum_list.begin(),
              end = tmp_unum_list.end();
          unum != end;
          ++unum )
    {
        const CoachPlayerObject * p = wm.opponent( *unum );
        if ( p
             && p->pos().absY() < SP.pitchHalfWidth() )
        {
            unum_list.push_back( *unum );
            pos_mf.push_back( p->pos() );
        }
    }

#ifdef DEBUG_ANALYZE_FORMATION
    for ( size_t i = 1; i < pos_mf.size(); i++ )
    {
        agent->debugClient().addLine( pos_mf[i - 1], pos_mf[i], "#A00" );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
OpponentFormationAnalyzer::findOffensiveOpponents( CoachAgent * agent,
                                                   std::vector< int > & unum_list )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": OpponentFormationAnalyzer::findOffensiveOpponents" );

    const CoachWorldModel & wm = agent->world();

    Vector2D centroid( 0.0, 0.0 );

    int count = 0;
    for ( CoachPlayerObject::Cont::const_iterator it = wm.opponents().begin(),
              end = wm.opponents().end();
          it != end;
          ++it )
    {
        const CoachPlayerObject * p = *it;
        if ( ! p ) continue;
        if ( p->goalie() ) continue;

        centroid += p->pos();
        ++count;
    }

    if ( count == 0 )
    {
        return;
    }

    centroid /= static_cast< double >( count );

    for ( CoachPlayerObject::Cont::const_iterator it = wm.opponents().begin(),
              end = wm.opponents().end();
          it != end;
          ++it )
    {
        const CoachPlayerObject * p = *it;
        if ( ! p ) continue;
        if ( p->goalie() ) continue;

        if ( p->pos().x < centroid.x )
        {
            unum_list.push_back( p->unum() );
#ifdef DEBUG_ANALYZE_FORMATION
            agent->debugClient().addCircle( p->pos(), 3.0, "#A00" );
#endif
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
OpponentFormationAnalyzer::saveOpponentData()
{
    dlog.addText( Logger::TEAM,
                  "(OpponentFormationAnalyzer::saveOpponentData)" );

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
    file_path += ".formation";

    std::ofstream ofs( file_path.c_str() );

    if ( ! ofs.is_open() )
    {
        std::cerr << "Could not save the opponent formation file [" << file_path
                  << "]" << std::endl;
        return false;
    }

    for ( int i = 0; i < 11; ++i )
    {
        ofs << M_count_forward[i] / 2 << '\n';
    }
    for ( int i = 0; i < 11; ++i )
    {
        ofs << M_count_midfielder[i] / 2 << '\n';
    }
    for ( int i = 0; i < 11; ++i )
    {
        ofs << M_count_defender[i] / 2 << '\n';
    }
    ofs.flush();

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
OpponentFormationAnalyzer::loadOpponentData()
{
    dlog.addText( Logger::TEAM,
                  __FILE__": OpponentFormationAnalyzer::loadOpponentData" );

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
    file_path += ".formation";

    std::ifstream ifs( file_path.c_str() );

    if ( ! ifs.is_open() )
    {
        std::cerr << "Could not load the opponent formation file [" << file_path
                  << "]" << std::endl;
        return false;
    }

    int loaded_value = 0;
    for ( int i = 0; i < 11; ++i )
    {
        ifs >> loaded_value;
        M_count_forward[i] += loaded_value;
    }
    for ( int i = 0; i < 11; ++i )
    {
        ifs >> loaded_value;
        M_count_midfielder[i] += loaded_value;
    }
    for ( int i = 0; i < 11; ++i )
    {
        ifs >> loaded_value;
        M_count_defender[i] += loaded_value;
    }

    return true;
}
