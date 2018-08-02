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

#include "helios_coach.h"

#include "options.h"

#include "coach_strategy.h"
#include "coach_analyzer_manager.h"
#include "default_freeform_message.h"

#include "goalie_unum_analyzer.h"
#include "opponent_formation_analyzer.h"
#include "opponent_coordination_analyzer.h"
#include "opponent_setplay_mark.h"
#include "coach_mark_analyzer.h"
#include "opponent_defense_mark_or_zone_decider.h"
#include "coach_path_planner.h"
//#include "player_type_sender.h"
//#include "opponent_wall_detector.h"

#include <rcsc/coach/coach_command.h>
#include <rcsc/coach/coach_config.h>
#include <rcsc/coach/coach_debug_client.h>
#include <rcsc/common/abstract_client.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/player_param.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/say_message_parser.h>
#include <rcsc/param/param_map.h>
#include <rcsc/param/cmd_line_parser.h>

#include <rcsc/coach/coach_world_model.h>

#include <cstdio>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <functional>

#include "team_logo.xpm"

using namespace rcsc;

/////////////////////////////////////////////////////////

struct RealSpeedMaxSorter
    : public std::binary_function< const PlayerType *,
                                   const PlayerType *,
                                   bool > {

    result_type operator()( first_argument_type lhs,
                            second_argument_type rhs ) const
      {
          if ( std::fabs( lhs->realSpeedMax() - rhs->realSpeedMax() ) < 0.005 )
          {
              return lhs->cyclesToReachMaxSpeed() < rhs->cyclesToReachMaxSpeed();
          }

          return lhs->realSpeedMax() > rhs->realSpeedMax();
      }
};

struct PlayerTypeSorter
    : public std::binary_function< const PlayerType *,
                                   const PlayerType *,
                                   bool > {

    result_type operator()( first_argument_type lhs,
                            second_argument_type rhs ) const
      {
          if ( lhs->cyclesToReachDistance( 5.0 ) < rhs->cyclesToReachDistance( 5.0 ) )
          {
              return true;
          }

          if ( lhs->cyclesToReachDistance( 5.0 ) == rhs->cyclesToReachDistance( 5.0 )
               && lhs->cyclesToReachDistance( 10.0 ) < rhs->cyclesToReachDistance( 10.0 ) )
          {
              return true;
          }

          if ( lhs->cyclesToReachDistance( 5.0 ) == rhs->cyclesToReachDistance( 5.0 )
               && lhs->cyclesToReachDistance( 10.0 ) == rhs->cyclesToReachDistance( 10.0 )
               && lhs->cyclesToReachDistance( 20.0 ) < rhs->cyclesToReachDistance( 20.0 ) )
          {
              return true;
          }

          if ( lhs->cyclesToReachDistance( 5.0 ) == rhs->cyclesToReachDistance( 5.0 )
               && lhs->cyclesToReachDistance( 10.0 ) == rhs->cyclesToReachDistance( 10.0 )
               && lhs->cyclesToReachDistance( 20.0 ) == rhs->cyclesToReachDistance( 20.0 )
               && lhs->cyclesToReachDistance( 30.0 ) < rhs->cyclesToReachDistance( 30.0 ) )
          {
              return true;
          }

          if ( lhs->cyclesToReachDistance( 5.0 ) == rhs->cyclesToReachDistance( 5.0 )
               && lhs->cyclesToReachDistance( 10.0 ) == rhs->cyclesToReachDistance( 10.0 )
               && lhs->cyclesToReachDistance( 20.0 ) == rhs->cyclesToReachDistance( 20.0 )
               && lhs->cyclesToReachDistance( 30.0 ) == rhs->cyclesToReachDistance( 30.0 ) )
          {
              if ( lhs->effectiveTurn( ServerParam::i().maxMoment(), lhs->realSpeedMax() * lhs->playerDecay() )
                   > rhs->effectiveTurn( ServerParam::i().maxMoment(), rhs->realSpeedMax() * rhs->playerDecay() ) )
              {
                  return true;
              }
          }

          return false;
      }

};

/*-------------------------------------------------------------------*/
/*!

 */
HeliosCoach::HeliosCoach()
    : CoachAgent()
{
    //
    // register audio memory & say message parsers
    //

    boost::shared_ptr< AudioMemory > audio_memory( new AudioMemory );

    M_worldmodel.setAudioMemory( audio_memory );

    addSayMessageParser( new BallMessageParser( audio_memory ) );
    addSayMessageParser( new PassMessageParser( audio_memory ) );
    addSayMessageParser( new InterceptMessageParser( audio_memory ) );
    addSayMessageParser( new GoalieMessageParser( audio_memory ) );
    addSayMessageParser( new GoalieAndPlayerMessageParser( audio_memory ) );
    addSayMessageParser( new OffsideLineMessageParser( audio_memory ) );
    addSayMessageParser( new DefenseLineMessageParser( audio_memory ) );
    addSayMessageParser( new WaitRequestMessageParser( audio_memory ) );
    addSayMessageParser( new PassRequestMessageParser( audio_memory ) );
    addSayMessageParser( new DribbleMessageParser( audio_memory ) );
    addSayMessageParser( new BallGoalieMessageParser( audio_memory ) );
    addSayMessageParser( new OnePlayerMessageParser( audio_memory ) );
    addSayMessageParser( new TwoPlayerMessageParser( audio_memory ) );
    addSayMessageParser( new ThreePlayerMessageParser( audio_memory ) );
    addSayMessageParser( new SelfMessageParser( audio_memory ) );
    addSayMessageParser( new TeammateMessageParser( audio_memory ) );
    addSayMessageParser( new OpponentMessageParser( audio_memory ) );
    addSayMessageParser( new BallPlayerMessageParser( audio_memory ) );
    addSayMessageParser( new StaminaMessageParser( audio_memory ) );
    addSayMessageParser( new RecoveryMessageParser( audio_memory ) );
    addSayMessageParser( new StaminaCapacityMessageParser( audio_memory ) );

    // addSayMessageParser( new FreeMessageParser< 9 >( audio_memory ) );
    // addSayMessageParser( new FreeMessageParser< 8 >( audio_memory ) );
    // addSayMessageParser( new FreeMessageParser< 7 >( audio_memory ) );
    // addSayMessageParser( new FreeMessageParser< 6 >( audio_memory ) );
    // addSayMessageParser( new FreeMessageParser< 5 >( audio_memory ) );
    // addSayMessageParser( new FreeMessageParser< 4 >( audio_memory ) );
    // addSayMessageParser( new FreeMessageParser< 3 >( audio_memory ) );
    // addSayMessageParser( new FreeMessageParser< 2 >( audio_memory ) );
    // addSayMessageParser( new FreeMessageParser< 1 >( audio_memory ) );


    // M_analyzers.push_back( AbstractCoachAnalyzer::Ptr( new PlayerTypeSender() ) );
    M_analyzers.push_back( AbstractCoachAnalyzer::Ptr( new GoalieUnumAnalyzer() ) );
    M_analyzers.push_back( AbstractCoachAnalyzer::Ptr( new OpponentFormationAnalyzer() ) );
    M_analyzers.push_back( AbstractCoachAnalyzer::Ptr( new OpponentCoordinationAnalyzer() ) );
    //M_analyzers.push_back( AbstractCoachAnalyzer::Ptr( new CoachMarkAnalyzer() ) );
    M_analyzers.push_back( AbstractCoachAnalyzer::Ptr( new CoachPathPlanner() ) );
    M_analyzers.push_back( AbstractCoachAnalyzer::Ptr( new OpponentSetplayMark() ) );
    //M_analyzers.push_back( AbstractCoachAnalyzer::Ptr( new OpponentWallDetector() ) );

}

/*-------------------------------------------------------------------*/
/*!

 */
HeliosCoach::~HeliosCoach()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
HeliosCoach::initImpl( CmdLineParser & cmd_parser )
{
    bool result = CoachAgent::initImpl( cmd_parser );


    // read additional options
    result &= Options::instance().init( cmd_parser );

    // ParamMap param_map( "Additional options" );
    // std::string file_path = "file";
    // param_map.add()
    //     ( "file-path", "", &file_path, "specified file path" );
    // cmd_parser.parse( param_map );
    // if ( cmd_parser.count( "help" ) > 0 )
    // {
    //    my_params.printHelp( std::cout );
    //    return false;
    // }
    // cmd_parser.parse( my_params );

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

    if ( ! CoachStrategy::instance().init() )
    {
        std::cerr << config().teamName() << " coach: "
                  << "***ERROR*** Failed to read team strategy." << std::endl;
        return false;
    }

    CoachAnalyzerManager::instance().init( Options::i().opponentDataDir() );

    if ( config().useTeamGraphic() )
    {
        if ( config().teamGraphicFile().empty() )
        {
            M_team_graphic.createXpmTiles( team_logo_xpm );
        }
        else
        {
            M_team_graphic.readXpmFile( config().teamGraphicFile().c_str() );
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::actionImpl()
{
    debugClient().addMessage( "Kicker:%c %d",
                              side_char( world().lastKickerSide() ),
                              world().lastKickerUnum() );

    CoachStrategy::instance().update( world() );

    if ( world().time().cycle() == 0
         && config().useTeamGraphic()
         && M_team_graphic.tiles().size() != teamGraphicOKSet().size() )
    {
        sendTeamGraphic();
    }

    if ( CoachAnalyzerManager::i().opponentNickname().empty() )
    {
        bool analyzed =
            CoachAnalyzerManager::i().analyzeOpponentNickname( world() );

        if ( analyzed )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": opponent nickname is %s",
                          CoachAnalyzerManager::i().opponentNickname().c_str() );
            std::cout << "Coach: opponent nickname is " << CoachAnalyzerManager::i().opponentNickname() << std::endl;

            for ( std::vector< AbstractCoachAnalyzer::Ptr >::iterator it = M_analyzers.begin(),
                      end = M_analyzers.end();
                  it != end;
                  ++it )
            {
                (*it)->loadOpponentData();
            }
        }

    }

    doSubstitute();

    if ( CoachAnalyzerManager::i().opponentCornerKickFormation().empty()
         && world().gameMode().type() == GameMode::CornerKick_
         && world().gameMode().isOurSetPlay( world().ourSide() ) )
    {
        CoachAnalyzerManager::i().analyzeOpponentCornerKickFormation( world() );
    }


    //
    // loop for all analyers
    //
    for ( std::vector< AbstractCoachAnalyzer::Ptr >::iterator it = M_analyzers.begin(),
              end = M_analyzers.end();
          it != end;
          ++it )
    {
        (*it)->analyze( this );
    }

    doCLangAdvice();
    doFreeformAdvice();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::handleActionStart()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::handleActionEnd()
{
#if 0
    for ( int i = 1; i <= 11; ++i )
    {
        const CoachPlayerObject * p = world().teammate( i );
        if ( p )
        {
            debugClient().addComment( p, "teammate%d", i );
        }

        p = world().opponent( i );
        if ( p )
        {
            debugClient().addComment( p, "opponent%d", i );
        }
    }
#endif
#if 0
    const double offside_x = world().ourOffsideLineX();
    debugClient().addLine( Vector2D( offside_x, -34.0 ),
                           Vector2D( offside_x, +34.0 ),
                           "#AA0" );

    const double defense_x = world().theirOffsideLineX();
    debugClient().addLine( Vector2D( defense_x, -34.0 ),
                           Vector2D( defense_x, +34.0 ),
                           "#A00" );
#endif
#if 1
    for ( int unum = 1; unum <= 11; ++unum )
    {
        Vector2D pos = CoachStrategy::i().getPosition( unum );
        if ( pos.isValid() )
        {
            debugClient().addRectangle( Rect2D::from_center( pos, 1.0, 1.0 ) );
        }
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::handleInitMessage()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::handleServerParam()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::handlePlayerParam()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::handlePlayerType()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::doSubstitute()
{
    static bool S_first_substituted = false;

    if ( world().time().cycle() > 0 )
    {
        if ( world().gameMode().type() != GameMode::PlayOn
             && ! world().gameMode().isPenaltyKickMode() )
        {
            doSubstituteTiredPlayers();
        }
    }
    else // time == 0
    {
        if ( ! S_first_substituted
             && ! world().theirTeamName().empty()
             && world().time().stopped() > 10 )
        {
            doFirstSubstitute();
            S_first_substituted = true;
            return;
        }

        if ( ! S_first_substituted
             && world().time().stopped() > ServerParam::i().kickOffWait() - 20 )
        {
            doFirstSubstitute();
            S_first_substituted = true;
            return;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::doFirstSubstitute()
{
    PlayerTypePtrCont candidates;

    std::fprintf( stderr,
                  "id speed step inc karea turn 10m 20m 30m"
                  "\n" );

    for ( int id = 0; id < PlayerParam::i().playerTypes(); ++id )
    {
        const PlayerType * param = PlayerTypeSet::i().get( id );

        if ( ! param )
        {
            std::cerr << config().teamName() << " coach: "
                      << " could not get the player type " << id << std::endl;
            continue;
        }

        if ( id == Hetero_Default
             && PlayerParam::i().allowMultDefaultType() )
        {
            for ( int i = 0; i <= MAX_PLAYER; ++i )
            {
                candidates.push_back( param );
            }
        }

        for ( int i = 0; i < PlayerParam::i().ptMax(); ++i )
        {
            candidates.push_back( param );
        }

        std::fprintf( stderr,
                      "%2d"
                      " %.3f" // max speed
                      "  %2d"  // steps to reach max speed
                      "  %.1f" // stamina incmax
                      " %.3f" // kicakble area
                      " %5.1f" // effective turn
                      " %3d %3d %3d %3d"
                      "\n",
                      id,
                      param->realSpeedMax(),
                      param->cyclesToReachMaxSpeed(),
                      param->staminaIncMax(),
                      param->kickableArea(),
                      param->effectiveTurn( ServerParam::i().maxMoment(),
                                            param->realSpeedMax() * param->playerDecay() ),
                      param->cyclesToReachDistance( 5.0 ),
                      param->cyclesToReachDistance( 10.0 ),
                      param->cyclesToReachDistance( 20.0 ),
                      param->cyclesToReachDistance( 30.0 ) );
    }

#if 0
    {
        std::sort( candidates.begin(), candidates.end(), RealSpeedMaxSorter() );
        std::cerr << " ===== \n";
        std::cerr << "sorted by actual speed max\n";
        for ( PlayerTypePtrCont::const_iterator it = candidates.begin();
              it != candidates.end();
              ++it )
        {
            std::cerr << (*it)->id() << ": " << (*it)->realSpeedMax() << '\n';
        }
        std::cerr << " ===== " << std::endl;

        std::sort( candidates.begin(), candidates.end(), PlayerTypeSorter() );
        std::cerr << " ===== \n";
        std::cerr << "sorted by reach step\n";
        for ( PlayerTypePtrCont::const_iterator it = candidates.begin();
              it != candidates.end();
              ++it )
        {
            std::cerr << (*it)->id()
                      << ": " << (*it)->cyclesToReachDistance( 5.0 )
                      << ' ' << (*it)->cyclesToReachDistance( 10.0 )
                      << ' ' << (*it)->cyclesToReachDistance( 20.0 )
                      << ' ' << (*it)->cyclesToReachDistance( 30.0 )
                      << ' ' << (*it)->realSpeedMax()
                //<< ' ' << (*it)->kickableArea()
                      << ' ' << (*it)->effectiveTurn( ServerParam::i().maxMoment(),
                                                      (*it)->realSpeedMax() * (*it)->playerDecay() )
                      << '\n';
        }
        std::cerr << " ===== " << std::endl;
    }
#endif

    //
    // default assignment
    //
    std::vector< int > ordered_unum;
    ordered_unum.reserve( 11 );

    if ( ! CoachStrategy::i().heteroOrder().empty() )
    {
        ordered_unum = CoachStrategy::i().heteroOrder();
    }
    else
    {
        ordered_unum.push_back( 11 ); // center forward
        ordered_unum.push_back( 2 );  // sweeper
        ordered_unum.push_back( 3 );  // center back
        ordered_unum.push_back( 10 ); // side half
        ordered_unum.push_back( 9 );  // side half
        ordered_unum.push_back( 4 );  // side back
        ordered_unum.push_back( 5 );  // side back
        ordered_unum.push_back( 7 );  // defensive half
        ordered_unum.push_back( 8 );  // defensive half
        ordered_unum.push_back( 6 );  // center half
    }


    std::vector< std::pair< int, int > > type_pairs;
    type_pairs.reserve( 11 );

    //
    // goalie:
    // goalie is always assigned to the default type so far.
    //

    if ( config().version() >= 14.0 )
    {
        type_pairs.push_back( std::pair< int, int >( 1, Hetero_Default ) );
        //substituteTo( 1, Hetero_Default ); // goalie
    }
    {
        PlayerTypePtrCont::iterator it = candidates.begin();
        for ( ; it != candidates.end(); ++it )
        {
            if ( (*it)->id() == Hetero_Default )
            {
                break;
            }
        }

        if ( it != candidates.end() )
        {
            candidates.erase( it );
        }
    }

    //
    // change field players
    //

    //
    // if top 2 fastest types have almost same speed, set better turn ability player to the sweeper
    //
    std::sort( candidates.begin(), candidates.end(), PlayerTypeSorter() );
    //std::sort( candidates.begin(), candidates.end(), std::not2( PlayerTypeSorter() ) );

    if ( candidates.size() >= 2 )
    {
        if ( candidates[0]->cyclesToReachDistance( 5.0 ) == candidates[1]->cyclesToReachDistance( 5.0 )
             && candidates[0]->cyclesToReachDistance( 10.0 ) == candidates[1]->cyclesToReachDistance( 10.0 )
             && candidates[0]->cyclesToReachDistance( 20.0 ) == candidates[1]->cyclesToReachDistance( 20.0 )
             && candidates[0]->cyclesToReachDistance( 30.0 ) == candidates[1]->cyclesToReachDistance( 30.0 ) )
        {
            std::swap( ordered_unum[0], ordered_unum[1] );
        }
    }


    for ( std::vector< int >::iterator unum = ordered_unum.begin();
          unum != ordered_unum.end();
          ++unum )
    {
        const CoachPlayerObject * p = world().teammate( *unum );
        if ( ! p )
        {
            std::cerr << config().teamName() << " coach: "
                      << " teammate " << *unum << " does not exist."
                      << " skip first substitution." << std::endl;
            dlog.addText( Logger::TEAM,
                          __FILE__": teammate %d does not exist. skip first substitution.",
                          *unum );
            continue;
        }

        int type = getFastestType( candidates );
        if ( type != Hetero_Unknown )
        {
            type_pairs.push_back( std::pair< int, int >( *unum, type ) );
            //substituteTo( *unum, type );
        }
    }

    this->doChangePlayerTypes( type_pairs );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::doSubstituteTiredPlayers()
{
    int substitute_count = world().ourSubstituteCount();

    if ( substitute_count >= PlayerParam::i().subsMax() )
    {
        // over the maximum substitution
        return;
    }

    const ServerParam & SP = ServerParam::i();

    //
    // check game time
    //
    const int half_time = SP.actualHalfTime();
    const int normal_time = half_time * SP.nrNormalHalfs();

    if ( world().time().cycle() < normal_time - 500
         //|| world().time().cycle() <= half_time + 1
         //|| world().gameMode().type() == GameMode::KickOff_
         )
    {
        return;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": consider to substitute tired teammates." );

    //
    // create candidate teamamte
    //
    std::vector< int > tired_teammate_unum;

    for ( CoachPlayerObject::Cont::const_iterator t = world().teammates().begin(), end = world().teammates().end();
          t != end;
          ++t )
    {
        if ( (*t)->recovery() < ServerParam::i().recoverInit() - 0.002 )
        {
            tired_teammate_unum.push_back( (*t)->unum() );
        }
    }

    if ( tired_teammate_unum.empty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": no tired teammates." );
        return;
    }

    //
    // create candidate player type
    //
    PlayerTypePtrCont candidates;

    for ( std::vector< int >::const_iterator
              id = world().availablePlayerTypeId().begin(),
              end = world().availablePlayerTypeId().end();
          id != end;
          ++id )
    {
        const PlayerType * param = PlayerTypeSet::i().get( *id );
        if ( ! param )
        {
            std::cerr << config().teamName() << " coach: " << world().time()
                      << " : Could not get player type. id=" << *id << std::endl;
            continue;
        }

        candidates.push_back( param );
    }

    //
    // try substitution
    //


    for ( std::vector< int >::iterator unum = tired_teammate_unum.begin();
          unum != tired_teammate_unum.end();
          ++unum )
    {
        int type = getFastestType( candidates );
        if ( type != Hetero_Unknown )
        {
            substituteTo( *unum, type );
            if ( ++substitute_count >= PlayerParam::i().subsMax() )
            {
                // over the maximum substitution
                break;
            }
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::substituteTo( const int unum,
                           const int type )
{
    if ( world().time().cycle() > 0
         && world().ourSubstituteCount() >= PlayerParam::i().subsMax() )
    {
        std::cerr << "***Warning*** "
                  << config().teamName() << " coach: over the substitution max."
                  << " cannot change the player " << unum
                  << " to type " << type
                  << std::endl;
        return;
    }

    std::vector< int >::const_iterator
        it = std::find( world().availablePlayerTypeId().begin(),
                        world().availablePlayerTypeId().end(),
                        type );
    if ( it == world().availablePlayerTypeId().end() )
    {
        std::cerr << "***ERROR*** "
                  << config().teamName() << " coach: "
                  << " cannot change the player " << unum
                  << " to type " << type
                  << std::endl;
        return;
    }

    doChangePlayerType( unum, type );

    std::cout << config().teamName() << " coach: "
              << "change player " << unum
              << " to type " << type
              << std::endl;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
HeliosCoach::getFastestType( PlayerTypePtrCont & candidates )
{
    if ( candidates.empty() )
    {
        return Hetero_Unknown;
    }

    std::sort( candidates.begin(), candidates.end(), PlayerTypeSorter() );
    //std::sort( candidates.begin(), candidates.end(), std::not2( PlayerTypeSorter() ) );

    //     std::cerr << "getFastestType candidate = ";
    //     for ( PlayerTypePtrCont::iterator it = candidates.begin();
    //           it != candidates.end();
    //           ++it )
    //     {
    //         std::cerr << (*it)->id() << ' ';
    //     }
    //     std::cerr << std::endl;

    PlayerTypePtrCont::iterator best_type = candidates.end();
    double max_speed = 0.0;
    int min_cycle = 100;
    for ( PlayerTypePtrCont::iterator it = candidates.begin();
          it != candidates.end();
          ++it )
    {
        if ( (*it)->realSpeedMax() < max_speed - 0.001 )
        {
            break;
        }

        if ( (*it)->cyclesToReachMaxSpeed() < min_cycle )
        {
            best_type = it;
            max_speed = (*best_type)->realSpeedMax();
            min_cycle = (*best_type)->cyclesToReachMaxSpeed();
            continue;
        }

        if ( (*it)->cyclesToReachMaxSpeed() == min_cycle )
        {
            if ( (*it)->getOneStepStaminaComsumption()
                 < (*best_type)->getOneStepStaminaComsumption() )
            {
                best_type = it;
                max_speed = (*best_type)->realSpeedMax();
            }
        }
    }

    if ( best_type != candidates.end() )
    {
        int id = (*best_type)->id();
        candidates.erase( best_type );
        return id;
    }

    return Hetero_Unknown;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::doCLangAdvice()
{
    for ( int unum = 1; unum <= 11; ++unum )
    {
        //
        // update player type
        //
        const int type = world().theirPlayerTypeId( unum );
        M_clang_holder.setTheirPlayerType( unum, type );

        //
        // update mark assignment
        //
        if ( ( world().gameMode().type() == GameMode::KickIn_
               || world().gameMode().type() == GameMode::CornerKick_ )
             && world().gameMode().isTheirSetPlay( world().ourSide() ) )
        {
            const std::pair< int, int > mark = CoachAnalyzerManager::i().markTargets( unum );
            M_clang_holder.setMarkAssignment( unum, mark.first, mark.second );

        }
    }

    if ( world().canSendCLang( CLANG_INFO )
         && ( M_clang_holder.theirPlayerTypeChanged()
              || M_clang_holder.markAssignmentChanged() ) )
    {
        CLangMessage * msg = M_clang_holder.buildCLang( world().ourTeamName(),
                                                        world().time() );
        doSendCLang( msg );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::doFreeformAdvice()
{
    if ( ! this->config().useFreeform() )
    {
        return;
    }

    if ( ! this->world().canSendFreeform() )
    {
        return;
    }

    if ( CoachAnalyzerManager::i().goalieUnumChanged()
         && ( world().time().cycle() > 0
              || world().time().stopped() > 15 ) )
    {
        boost::shared_ptr< FreeformMessage > ptr( new GoalieUnumMessage( CoachAnalyzerManager::i().ourGoalieUnum(),
                                                                         CoachAnalyzerManager::i().theirGoalieUnum() ) );
        this->addFreeformMessage( ptr );

        std::cout << this->config().teamName() << " coach: "
                  << this->world().time()
                  << " send freeform (goalie"
                  << " (our " << CoachAnalyzerManager::i().ourGoalieUnum() << ')'
                  << " (opp " << CoachAnalyzerManager::i().theirGoalieUnum() << ')'
                  << ')' << std::endl;

        CoachAnalyzerManager::instance().clearGoalieUnumChanged();
    }

    if ( CoachAnalyzerManager::i().cornerKickAnalyzed() )
    {
        boost::shared_ptr< FreeformMessage > ptr( new CornerKickTypeMessage( CoachAnalyzerManager::i().opponentCornerKickFormation() ) );
        this->addFreeformMessage( ptr );

        std::cout << this->config().teamName() << " coach: "
                  << this->world().time()
                  << " send freeform (cornerkick_type:"
                  << CoachAnalyzerManager::i().opponentCornerKickFormation()
                  << ')' << std::endl;

        CoachAnalyzerManager::instance().clearCornerKickAnalyzed();
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosCoach::sendTeamGraphic()
{
    int count = 0;
    for ( TeamGraphic::Map::const_reverse_iterator tile = M_team_graphic.tiles().rbegin();
          tile != M_team_graphic.tiles().rend();
          ++tile )
    {
        if ( teamGraphicOKSet().find( tile->first ) == teamGraphicOKSet().end() )
        {
            if ( ! doTeamGraphic( tile->first.first,
                                  tile->first.second,
                                  M_team_graphic ) )
            {
                break;
            }
            ++count;
        }
    }

    if ( count > 0 )
    {
        std::cout << config().teamName()
                  << " coach: "
                  << world().time()
                  << " send team_graphic " << count << " tiles"
                  << std::endl;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */

void
HeliosCoach::handleExit()
{
    finalize();
}

/*-------------------------------------------------------------------*/
/*!

 */

void
HeliosCoach::finalize()
{
    for ( std::vector< AbstractCoachAnalyzer::Ptr >::iterator it = M_analyzers.begin(),
              end = M_analyzers.end();
          it != end;
          ++it )
    {
        (*it)->saveOpponentData();
    }

    CoachAgent::finalize();
}
