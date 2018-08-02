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

#include "helios_player.h"

#include "options.h"

#include "statistics.h"
#include "strategy.h"
#include "mark_analyzer.h"
#include "field_analyzer.h"

#include "action_chain_holder.h"
#include "field_evaluator.h"
#include "field_evaluator_default.h"
#include "field_evaluator2013.h"
#include "field_evaluator2016.h"
#include "field_evaluator_svmrank.h"

#include "generator_center_forward_free_move.h"
#include "generator_clear.h"

#include "soccer_role.h"

#include "default_communication.h"
#include "keepaway_communication.h"
#include "default_freeform_message_parser.h"

#include "bhv_penalty_kick.h"
#include "bhv_set_play.h"
#include "bhv_set_play_kick_in.h"
#include "bhv_set_play_indirect_free_kick.h"

#include "bhv_custom_before_kick_off.h"
#include "bhv_shoot.h"
#include "bhv_tactical_tackle.h"

#include "bhv_hold_ball.h"

#include "view_tactical.h"

// #include "goalie_optimal_position.h"
// #include "intercept_probability.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_emergency.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>
#include <rcsc/action/view_synch.h>

#include <rcsc/formation/formation.h>
#include <rcsc/action/kick_table.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/player/audio_sensor.h>

#include <rcsc/common/abstract_client.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_param.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/say_message_parser.h>

#include <rcsc/param/param_map.h>
#include <rcsc/param/cmd_line_parser.h>

#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>

// #define DEBUG_PRINT

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
HeliosPlayer::HeliosPlayer()
    : PlayerAgent(),
      M_communication()
{
    boost::shared_ptr< AudioMemory > audio_memory( new AudioMemory );

    M_worldmodel.setAudioMemory( audio_memory );

    //
    // set communication message parser
    //
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

    //
    // set freeform message parser
    //
    addFreeformMessageParser( new GoalieUnumMessageParser( M_worldmodel ) );
    addFreeformMessageParser( new OpponentPlayerTypeMessageParser( M_worldmodel ) );
    addFreeformMessageParser( new OpponentFormationMessageParser() );
    addFreeformMessageParser( new CornerKickTypeMessageParser( M_worldmodel) );
    addFreeformMessageParser( new OpponentWallDetectorMessageParser( M_worldmodel ) );

    //
    // set communication planner
    //
    M_communication = Communication::Ptr( new DefaultCommunication() );
}

/*-------------------------------------------------------------------*/
/*!

 */
HeliosPlayer::~HeliosPlayer()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
HeliosPlayer::initImpl( CmdLineParser & cmd_parser )
{
    bool result = PlayerAgent::initImpl( cmd_parser );

    // read additional options
    result &= Options::instance().init( cmd_parser );

    // ParamMap param_map( "Additional options" );
    // std::string file_path = "file";
    // param_map.add()
    //     ( "file-path", "", &file_path, "specified file path" );
    // cmd_parser.parse( param_map );
    // if ( cmd_parser.count( "help" ) > 0 )
    // {
    //     param_map.printHelp( std::cout );
    //     return false;
    // }

    if ( cmd_parser.failed() )
    {
        std::cerr << "player: ***WARNING*** detected unsuppprted options: ";
        cmd_parser.print( std::cerr );
        std::cerr << std::endl;
    }

    if ( ! result )
    {
        return false;
    }

    Options::instance().setLogDir( this->config().logDir() );

#if 1
    if ( config().teamName().length() < 6
         || config().teamName()[0] != 'H'
         || config().teamName()[1] != 'E'
         || config().teamName()[2] != 'L'
         || config().teamName()[3] != 'I'
         || config().teamName()[4] != 'O'
         || config().teamName()[5] != 'S' )
    {
        std::cerr << config().teamName()
                  << ": ***ERROR*** Illegal team name ["
                  << config().teamName() << "]"
                  << std::endl;
        return false;
    }
#endif

    const Options & opt = Options::i();

    if ( ! Strategy::instance().init() )
    {
        std::cerr << config().teamName()
                  << ": ***ERROR*** Failed to read team strategy." << std::endl;
        return false;
    }

    if ( ! opt.kickConfFile().empty()
         && ! KickTable::instance().read( opt.kickConfFile() ) )
    {
        std::cerr << config().teamName()
                  << ": ***ERROR*** Could not read the kick table: ["
                  << opt.kickConfFile() << "]" << std::endl;
    }

    if ( opt.ballTableFile().empty()
         || ! FieldAnalyzer::instance().init( opt.ballTableFile() ) )
    {
        std::cerr << config().teamName()
                  << ": ***ERROR*** Could not initialize the ball model table."
                  << std::endl;
        return false;
    }

    // if ( ! FieldEvaluatorSIRMsModel::read_parameters( opt.sirmEvaluatorParamDir() ) )
    // {
    //     std::cerr << "***ERROR*** Failed to read SIRM evaluator parameters. directory=["
    //               << opt.sirmEvaluatorParamDir() << "]" << std::endl;
    //     //SIRMsModelFieldEvaluator::save_parameters( opt.sirmEvaluatorParamDir() );
    //     return false;
    // }

    if ( ! GeneratorCenterForwardFreeMove::instance().init() )
    {
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosPlayer::handleExit()
{
#if 0
    std::cout << config().teamName() << ' ' << world().self().unum() << ':'
              << " action search.\n"
              << " count=" << Statistics::instance().totalActionSearchCount()
              << " total_size=" << Statistics::instance().totalActionSearchSize()
              << " max_size=" << Statistics::instance().maxActionSearchSize()
              << " ave_size=" << Statistics::instance().averageActionSearchSize()
              << "\n max_time=" << Statistics::instance().maxActionSearchMSec()
              << " ave_time=" << Statistics::instance().averageActionSearchMSec()
              << std::endl;
#endif
    PlayerAgent::handleExit();
}

/*-------------------------------------------------------------------*/
/*!
  main decision
  virtual method in super class
*/
void
HeliosPlayer::actionImpl()
{
    //
    // update strategy and analyzer
    //
    Strategy::instance().update( world() );
    MarkAnalyzer::instance().update( world() );
    FieldAnalyzer::instance().update( world() );

    //
    // handle special situations
    //
    if ( doPreprocess() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": preprocess done" );
        return;
    }

    //
    // create current role
    //
    SoccerRole::Ptr role = Strategy::i().createRole( world().self().unum(), world() );

    if ( ! role )
    {
        std::cerr << config().teamName() << ' ' << world().self().unum()
                  << ": Error. Role is not registerd.\nExit ..."
                  << std::endl;
        M_client->setServerAlive( false );
        return;
    }

    {
        FieldEvaluator::Ptr eval = role->createFieldEvaluator();
        if ( eval )
        {
            ActionChainHolder::instance().setFieldEvaluator( eval );
        }
    }

    //
    // search the best action sequence
    //
    ActionChainHolder::instance().update( world() );


    GeneratorClear::instance().generate( world() );

    //
    // override execute if role accept
    //
    if ( world().gameMode().type() == GameMode::PlayOn
         || role->acceptExecution( world() ) )
    {
        //writeInterceptDecisionLog( role->shortName() );
        role->execute( this );
        return;
    }

    //
    // penalty kick mode
    //
    if ( world().gameMode().isPenaltyKickMode() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": penalty kick" );
        Bhv_PenaltyKick().execute( this );
        return;
    }

    //
    // other set play behavior
    //
    Bhv_SetPlay().execute( this );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosPlayer::handleActionStart()
{
    Statistics::instance().processPreActionCallbacks();
}

namespace {

void
check_duplicated_player( const WorldModel & wm,
                         const PlayerObject::Cont & players )
{
    for ( PlayerObject::Cont::const_iterator p1 = players.begin();
          p1 != players.end();
          ++p1 )
    {
        if ( (*p1)->unum() == Unum_Unknown )
        {
            continue;
        }

        PlayerObject::Cont::const_iterator p2 = p1;
        ++p2;
        for ( ; p2 != players.end(); ++p2 )
        {
            if ( (*p1)->unum() == (*p2)->unum() )
            {
                std::cerr << wm.teamName() << ' ' << wm.self().unum()
                          << ' ' << wm.time()
                          << ": *************** detected duplicated player."
                          << " side = " << side_str( (*p1)->side() )
                          << " unum = " << (*p1)->unum() << std::endl;
            }
        }
    }
}

void
print_ball_history( const BallObject & ball )
{
    if ( ball.posHistory().empty() )
    {
        return;
    }

    std::list< Vector2D >::const_iterator p1 = ball.posHistory().begin();
    dlog.addLine( Logger::WORLD, ball.pos(), *p1, "#FFF" );

    std::list< Vector2D >::const_iterator p2 = p1;
    ++p2;
    std::list< Vector2D >::const_iterator end = ball.posHistory().end();
    while ( p2 != end )
    {
        dlog.addLine( Logger::WORLD, *p1, *p2, "#FFF" );
        p1 = p2;
        ++p2;
    }
}

void
print_players_history( const PlayerObject::Cont & players )
{
    for ( PlayerObject::Cont::const_iterator p = players.begin(), end = players.end();
          p != end;
          ++p )
    {
        if ( (*p)->posHistory().empty() ) continue;

        int count = 0;
        std::list< Vector2D >::const_iterator p1 = (*p)->posHistory().begin();
        dlog.addLine( Logger::WORLD, (*p)->pos(), *p1, "#00F" );

        std::list< Vector2D >::const_iterator p2 = p1; ++p2;
        std::list< Vector2D >::const_iterator pos_end = (*p)->posHistory().end();
        while ( ++count <= 10 && p2 != pos_end )
        {
            dlog.addLine( Logger::WORLD, *p1, *p2, "#00F" );
            p1 = p2;
            ++p2;
        }
    }
}

}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosPlayer::handleActionEnd()
{
#if 0
    debugClient().addSelfComment( "self%d", world().self().unum() );

    for ( PlayerCont::const_iterator p = world().teammates().begin(),
              end = world().teammates().end();
          p != end;
          ++p )
    {
        debugClient().addComment( &(*p), "teammate%d", p->unum() );
    }
    for ( PlayerCont::const_iterator p = world().opponents().begin(),
              end = world().opponents().end();
          p != end;
          ++p )
    {
        debugClient().addComment( &(*p), "opponent%d", p->unum() );
    }
    int count = 0;
    for ( PlayerCont::const_iterator p = world().unknownPlayers().begin(),
              end = world().unknownPlayers().end();
          p != end;
          ++p, ++count )
    {
        debugClient().addComment( &(*p), "unknown%d", count );
    }
#endif

    if ( world().self().posValid() )
    {
#if 0
        const ServerParam & SP = ServerParam::i();
        //
        // inside of pitch
        //

        // top,lower
        debugClient().addLine( Vector2D( world().ourOffenseLineX(),
                                         -SP.pitchHalfWidth() ),
                               Vector2D( world().ourOffenseLineX(),
                                         -SP.pitchHalfWidth() + 3.0 ) );
        // top,lower
        debugClient().addLine( Vector2D( world().ourDefenseLineX(),
                                         -SP.pitchHalfWidth() ),
                               Vector2D( world().ourDefenseLineX(),
                                         -SP.pitchHalfWidth() + 3.0 ) );

        // bottom,upper
        debugClient().addLine( Vector2D( world().theirOffenseLineX(),
                                         +SP.pitchHalfWidth() - 3.0 ),
                               Vector2D( world().theirOffenseLineX(),
                                         +SP.pitchHalfWidth() ) );
        //
        debugClient().addLine( Vector2D( world().offsideLineX(),
                                         world().self().pos().y - 15.0 ),
                               Vector2D( world().offsideLineX(),
                                         world().self().pos().y + 15.0 ) );

        // outside of pitch

        // top,upper
        debugClient().addLine( Vector2D( world().ourOffensePlayerLineX(),
                                         -SP.pitchHalfWidth() - 3.0 ),
                               Vector2D( world().ourOffensePlayerLineX(),
                                         -SP.pitchHalfWidth() ) );
        // top,upper
        debugClient().addLine( Vector2D( world().ourDefensePlayerLineX(),
                                         -SP.pitchHalfWidth() - 3.0 ),
                               Vector2D( world().ourDefensePlayerLineX(),
                                         -SP.pitchHalfWidth() ) );
        // bottom,lower
        debugClient().addLine( Vector2D( world().theirOffensePlayerLineX(),
                                         +SP.pitchHalfWidth() ),
                               Vector2D( world().theirOffensePlayerLineX(),
                                         +SP.pitchHalfWidth() + 3.0 ) );
        // bottom,lower
        debugClient().addLine( Vector2D( world().theirDefensePlayerLineX(),
                                         +SP.pitchHalfWidth() ),
                               Vector2D( world().theirDefensePlayerLineX(),
                                         +SP.pitchHalfWidth() + 3.0 ) );
#endif
        // top,lower
        debugClient().addLine( Vector2D( world().ourDefenseLineX(),
                                         world().self().pos().y - 2.0 ),
                               Vector2D( world().ourDefenseLineX(),
                                         world().self().pos().y + 2.0 ) );

        //
        debugClient().addLine( Vector2D( world().offsideLineX(),
                                         world().self().pos().y - 15.0 ),
                               Vector2D( world().offsideLineX(),
                                         world().self().pos().y + 15.0 ) );
    }

    //
    // ball position & velocity
    //
    dlog.addText( Logger::WORLD,
                  "WM: BALL pos%d=(%.2f, %.2f), vel=(%.2f, %.2f)(r=%.3f, ang=%.2f)",
                  world().ball().posCount(),
                  world().ball().pos().x, world().ball().pos().y,
                  world().ball().vel().x, world().ball().vel().y,
                  world().ball().vel().r(),
                  world().ball().vel().th().degree() );
    dlog.addText( Logger::WORLD,
                  "WM: BALL seen_pos%d=(%.2f, %.2f), seen_vel=(%.2f, %.2f)(r=%.3f, ang=%.2f)",
                  world().ball().seenPosCount(),
                  world().ball().seenPos().x, world().ball().seenPos().y,
                  world().ball().seenVel().x, world().ball().seenVel().y,
                  world().ball().seenVel().r(),
                  world().ball().seenVel().th().degree() );

    if ( world().self().lastMove().isValid() )
    {
        dlog.addText( Logger::WORLD,
                      "WM: SELF move=(%lf, %lf, r=%lf, th=%lf)",
                      world().self().lastMove().x, world().self().lastMove().y,
                      world().self().lastMove().r(),
                      world().self().lastMove().th().degree() );

        if ( world().prevBall().rpos().isValid() )
        {
            Vector2D diff = world().ball().rpos() - world().prevBall().rpos();
            dlog.addText( Logger::WORLD,
                          "WM: BALL rpos=(%lf %lf) prev_rpos=(%lf %lf) diff=(%lf %lf)",
                          world().ball().rpos().x, world().ball().rpos().y,
                          world().prevBall().rpos().x, world().prevBall().rpos().y,
                          diff.x, diff.y );

            Vector2D ball_move = diff + world().self().lastMove();
            Vector2D diff_vel = ball_move * ServerParam::i().ballDecay();
            dlog.addText( Logger::WORLD,
                          "---> ball_move=(%lf %lf) vel=(%lf, %lf, r=%lf, th=%lf)",
                          ball_move.x, ball_move.y,
                          diff_vel.x, diff_vel.y,
                          diff_vel.r(),
                          diff_vel.th().degree() );
        }
    }

#if 0
    check_duplicated_player( world(), world().teammates() );
    check_duplicated_player( world(), world().opponents() );
#endif

#if 0
    if ( dlog.isEnabled( Logger::WORLD ) )
    {
        print_ball_history( world().ball() );
        print_players_history( world().teammates() );
        print_players_history( world().opponents() );
    }
#endif

    //
    // write field evaluator debug information
    //
    // M_field_evaluator->writeDebugLog( world() );

    //
    // post action callback
    //

    Statistics::instance().processPostActionCallbacks();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosPlayer::handleInitMessage()
{
    if ( world().self().goalie()
         && Strategy::i().goalieUnum() != world().self().unum() )
    {
        std::cerr << world().teamName() << ' '
                  << world().self().unum() << ": "
                  << "Illegal goalie uniform number." << std::endl;
    }

    M_worldmodel.setOurGoalieUnum( Strategy::i().goalieUnum() );

    {
        // Initializing the order of penalty kickers
        std::vector< int > unum_order_pk_kickers;
        unum_order_pk_kickers.push_back( 5 );
        unum_order_pk_kickers.push_back( 2 );
        unum_order_pk_kickers.push_back( 3 );
        unum_order_pk_kickers.push_back( 1 );
        unum_order_pk_kickers.push_back( 11 );
        unum_order_pk_kickers.push_back( 7 );
        unum_order_pk_kickers.push_back( 4 );
        unum_order_pk_kickers.push_back( 10 );
        unum_order_pk_kickers.push_back( 9 );
        unum_order_pk_kickers.push_back( 8 );
        unum_order_pk_kickers.push_back( 6 );

        M_worldmodel.setPenaltyKickTakerOrder( unum_order_pk_kickers );
    }

    FieldEvaluator::Ptr field_evaluator = createFieldEvaluator();
    if ( ! field_evaluator )
    {
        std::cerr << world().teamName() << ' '
                  << world().self().unum() << ": "
                  << ": ***ERROR*** NULL field evaluator." << std::endl;
        M_client->setServerAlive( false );
    }

    ActionGeneratorHolder::ConstPtr action_generator = createActionGenerator();
    if ( ! action_generator )
    {
        std::cerr << world().teamName() << ' '
                  << world().self().unum() << ": "
                  << ": ***ERROR*** NULL action generator." << std::endl;
        M_client->setServerAlive( false );
    }

    ActionChainHolder::instance().init( field_evaluator, action_generator );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosPlayer::handleServerParam()
{
    if ( ServerParam::i().keepawayMode() )
    {
        std::cerr << "set Keepaway mode communication." << std::endl;
        M_communication = Communication::Ptr( new KeepawayCommunication() );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosPlayer::handlePlayerParam()
{
    if ( KickTable::instance().createTables() )
    {
        std::cerr << world().teamName() << ' '
                  << world().self().unum() << ": "
                  << " KickTable created."
                  << std::endl;
    }
    else
    {
        std::cerr << world().teamName() << ' '
                  << world().self().unum() << ": "
                  << " KickTable failed..."
                  << std::endl;
        M_client->setServerAlive( false );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
HeliosPlayer::handlePlayerType()
{
    // TODO: create kick table for each player type.
}

/*-------------------------------------------------------------------*/
/*!

*/
void
HeliosPlayer::handleOnlineCoachAudio()
{
    if ( audioSensor().clangTime() != world().time() )
    {
        return;
    }

    for ( int unum = 1; unum <= 11; ++unum )
    {
        M_clang_holder.setTheirPlayerType( unum,
                                           world().theirPlayerTypeId( unum ) );


    }

    if ( M_clang_holder.buildDataFrom( world().time(),
                                       audioSensor().clangParser().message() ) )
    {
        {
#ifdef DEBUG_PRINT
            std::ostringstream ostr;
            bool changed = false;
#endif
            for ( int unum = 1; unum <= 11; ++unum )
            {
                int old = world().theirPlayerTypeId( unum );
                int type = M_clang_holder.theirPlayerType( unum );
                if ( old != type )
                {
#ifdef DEBUG_PRINT
                    changed = true;
                    ostr << '(' << unum << ' ' << type << ')';
#endif
                    M_worldmodel.setTheirPlayerType( unum, type );
                }
            }
#ifdef DEBUG_PRINT
            if ( changed )
            {
                std::cerr << world().teamName() << ' ' << world().self().unum()
                          << ' ' << world().time()
                          << " update player_type: "
                          << ostr.str() << std::endl;
            }
#endif
        }

        {
            std::ostringstream ostr;
            bool changed = false;

            for ( int unum = 1; unum <= 11; ++unum )
            {
                int first = M_clang_holder.firstMarkTarget( unum );
                int second = M_clang_holder.secondMarkTarget( unum );

                if ( MarkAnalyzer::instance().setAssignmentByCoach( unum, first, second ) )
                {
                    changed = true;
                    ostr << '(' << unum << ' ' << first << ' ' << second << ')';
                }
            }
            if ( changed
                 && world().self().unum() == 1 )
            {
                std::cerr << world().teamName() << ' ' << world().self().unum()
                          << ' ' << world().time()
                          << " update mark: "
                          << ostr.str() << std::endl;
            }
        }
    }
}

/*-------------------------------------------------------------------*/
/*!
  communication decision.
  virtual method in super class
*/
void
HeliosPlayer::communicationImpl()
{
    if ( M_communication )
    {
        M_communication->execute( this );
    }
}

/*-------------------------------------------------------------------*/
/*!
*/
bool
HeliosPlayer::doPreprocess()
{
    // check tackle expires
    // check self position accuracy
    // ball search
    // check queued intention
    // check simultaneous kick

    const WorldModel & wm = this->world();

    dlog.addText( Logger::TEAM,
                  __FILE__": (doPreProcess)" );

    //
    // freezed by tackle effect
    //
    if ( wm.self().isFrozen() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": tackle wait. expires= %d",
                      wm.self().tackleExpires() );
        this->debugClient().addMessage( "TackleWait" );
        // face neck to ball
        this->setViewAction( new View_Tactical() );
        this->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
        return true;
    }

    //
    // BeforeKickOff or AfterGoal. jump to the initial position
    //
    if ( wm.gameMode().type() == GameMode::BeforeKickOff
         || wm.gameMode().type() == GameMode::AfterGoal_ )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": before_kick_off" );
        Vector2D move_point =  Strategy::i().getPosition( wm.self().unum() );
        Bhv_CustomBeforeKickOff( move_point ).execute( this );
        this->setViewAction( new View_Tactical() );
        return true;
    }

    //
    // self localization error
    //
    if ( ! wm.self().posValid() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": invalid my pos" );
        this->debugClient().addMessage( "SearchMyPosition" );
        Bhv_Emergency().execute( this ); // includes change view
        return true;
    }

    //
    // ball localization error
    //
    const int count_thr = ( wm.self().goalie()
                            ? 10
                            : 5 );
    if ( wm.ball().posCount() > count_thr
         || ( wm.gameMode().type() != GameMode::PlayOn
              && wm.ball().seenPosCount() > count_thr + 10 ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": search ball" );
        this->debugClient().addMessage( "SearchBall" );
        this->setViewAction( new View_Tactical() );
        Bhv_NeckBodyToBall().execute( this );
        return true;
    }

    //
    // set default change view
    //

    this->setViewAction( new View_Tactical() );

    //
    // check shoot chance
    //
    if ( doShoot() )
    {
        return true;
    }

    // if ( doTurnForShoot() )
    // {
    //     return true;
    // }

    //
    // check queued action
    //
    if ( doIntention() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": do queued intention" );
        return true;
    }

    //
    // try tackle
    //
    if ( ! wm.self().goalie()
         && wm.gameMode().type() != GameMode::PenaltySetup_
         && Bhv_TacticalTackle().execute( this ) )
    {
        return true;
    }

    //
    // check simultaneous kick
    //
    if ( doForceKick() )
    {
        return true;
    }

    //
    // check pass message
    //
    if ( doHeardPassReceive() )
    {
        return true;
    }

    //
    // check intercept
    //
#if 0
    if( Bhv_ProbabilisticIntercept().execute( this ) )
    {
        return true;
    }
#endif

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
HeliosPlayer::doShoot()
{
    const WorldModel & wm = this->world();

    if ( wm.self().isKickable()
         && wm.time().stopped() == 0
         && wm.gameMode().type() != GameMode::IndFreeKick_
         && ( wm.gameMode().type() == GameMode::PlayOn
              || ( 28.0 < wm.ball().pos().x
                   && wm.ball().pos().absY() < ServerParam::i().goalAreaHalfWidth() ) )
         && Bhv_Shoot().execute( this ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": shooted" );

        // reset intention
        this->setIntention( static_cast< SoccerIntention * >( 0 ) );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
HeliosPlayer::doTurnForShoot()
{
    const WorldModel & wm = this->world();

    if ( wm.self().isKickable()
         && wm.gameMode().type() == GameMode::PlayOn )
    {
        const AngleDeg goal_angle = ( ServerParam::i().theirTeamGoalPos() - wm.self().pos() ).th();
        if ( ( goal_angle - wm.self().body() ).abs() < 90.0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(doTurnForShoot) already facing to goal" );
            return false;
        }

        const Sector2D sector( Vector2D( 58.0, 0.0 ),
                               0.0, 20.0,
                               137.5, -137.5 );
        dlog.addSector( Logger::TEAM, sector, "#F00" );
        if ( ! sector.contains( wm.self().pos() ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(doTurnForShoot) not in sector" );
            return false;
        }

        const PlayerObject * opponent = wm.getOpponentNearestToSelf( 3 );
        if ( opponent
             && opponent->distFromSelf() < 3.0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(doTurnForShoot) exist opponent" );
            return false;
        }

        const Vector2D self_next = wm.self().pos() + wm.self().vel();
        const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
        const double noised_kickable_area = wm.self().playerType().kickableArea()
            - wm.ball().vel().r() * ServerParam::i().ballRand()
            - wm.self().vel().r() * ServerParam::i().playerRand()
            - 0.15;
        // turn
        if ( self_next.dist( ball_next ) < noised_kickable_area )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(doTurnForShoot) next kickable. only turn" );
            this->debugClient().addMessage( "TurnForShoot:Turn" );
            Body_TurnToPoint( ServerParam::i().theirTeamGoalPos() ).execute( this );
            this->setNeckAction( new Neck_TurnToGoalieOrScan( 0 ) );
            return true;
        }

        // collision kick
        Vector2D collision_kick_accel = self_next - ball_next;
        double collision_kick_accel_len = collision_kick_accel.r();
        if ( ServerParam::i().maxPower() * wm.self().kickRate()
             > collision_kick_accel_len - wm.self().playerType().playerSize() - ServerParam::i().ballSize() )
        {
            double kick_power = std::min( collision_kick_accel_len / wm.self().kickRate(),
                                          ServerParam::i().maxPower() );
            dlog.addText( Logger::TEAM,
                          __FILE__":(doTurnForShoot) collision kick" );
            this->debugClient().addMessage( "TurnForShoot:Collide" );
            this->doKick( kick_power, collision_kick_accel.th() - wm.self().body() );
            this->setNeckAction( new Neck_TurnToGoalieOrScan( 0 ) );
            return true;
        }

        const Vector2D ball_vel = Bhv_HoldBall::get_keep_ball_vel( wm );
        if ( ball_vel.isValid() )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(doTurnForShoot) illegal ball vel" );
            return false;
        }

        const Vector2D kick_accel = ball_vel - wm.ball().vel();
        const double kick_power = kick_accel.r() / wm.self().kickRate();

        if ( kick_power > ServerParam::i().maxPower() )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(doTurnForShootl) over max power" );
            return false;
        }

        dlog.addText( Logger::TEAM,
                      __FILE__":(doTurnForShoot) keep kick" );
        this->debugClient().addMessage( "TurnForShoot:Keep" );
        this->debugClient().setTarget( wm.ball().pos()
                                       + ball_vel
                                       + ball_vel * ServerParam::i().ballDecay() );
        this->doKick( kick_power, kick_accel.th() - wm.self().body() );
        this->setNeckAction( new Neck_TurnToGoalieOrScan( 0 ) );
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
HeliosPlayer::doForceKick()
{
    const WorldModel & wm = this->world();

    if ( wm.gameMode().type() == GameMode::PlayOn
         && ! wm.self().goalie()
         && wm.self().isKickable()
         && wm.kickableOpponent()
         && ( wm.kickableOpponent()->distFromBall()
              < wm.kickableOpponent()->playerTypePtr()->kickableArea() - 0.1 ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": simultaneous kick" );
        this->debugClient().addMessage( "SimultaneousKick" );
        Vector2D goal_pos( ServerParam::i().pitchHalfLength(), 0.0 );

        if ( wm.self().pos().x > 36.0
             && wm.self().pos().absY() > 10.0 )
        {
            goal_pos.x = 45.0;
            dlog.addText( Logger::TEAM,
                          __FILE__": simultaneous kick cross type" );
        }
        Body_KickOneStep( goal_pos,
                          ServerParam::i().ballSpeedMax()
                          ).execute( this );
        this->setNeckAction( new Neck_ScanField() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
HeliosPlayer::doHeardPassReceive()
{
    const WorldModel & wm = this->world();

    if ( wm.audioMemory().passTime() != wm.time()
         || wm.audioMemory().pass().empty()
         || wm.audioMemory().pass().front().receiver_ != wm.self().unum() )
    {

        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    Vector2D heard_pos = wm.audioMemory().pass().front().receive_pos_;

    dlog.addText( Logger::TEAM,
                  __FILE__":  (doHeardPassReceive) heard_pos(%.2f %.2f)",
                  heard_pos.x, heard_pos.y );

    bool kicking = false;
    if ( wm.audioMemory().ballTime() == wm.time()
         && wm.ball().heardVel().isValid()
         && wm.ball().heardVel().r2() < std::pow( 0.1, 2 ) )
    {
        kicking = true;
        dlog.addText( Logger::TEAM,
                      __FILE__": (doHeardPassReceive) still kicking" );
    }

    if ( ! kicking
         && wm.ball().posCount() <= 1
         && wm.ball().velCount() <= 1
         && self_min < 30 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doHeardPassReceive) intercept cycle=%d. intercept",
                      self_min );
        this->debugClient().addMessage( "Comm:Receive:Intercept" );
        Body_Intercept().execute( this );
        this->setNeckAction( new Neck_TurnToBall() );
        return true;
    }


    dlog.addText( Logger::TEAM,
                  __FILE__": (doHeardPassReceive) intercept cycle=%d. go to receive point",
                  self_min );

    double dash_power = ServerParam::i().maxDashPower();
    if ( kicking
         && heard_pos.x > wm.offsideLineX() )
    {
        AngleDeg target_angle = ( heard_pos - wm.self().inertiaFinalPoint() ).th();

        if ( ( target_angle - wm.self().body() ).abs() < 15.0 )
        {
            dash_power = 0.0;

            const double offside_buf = ( Strategy::i().opponentType() == Strategy::Type_Gliders
                                         ? 1.5
                                         : 1.0 );

            const Vector2D accel_vec = Vector2D::from_polar( 1.0, wm.self().body() );
            for ( double p = ServerParam::i().maxDashPower(); p > 0.0; p -= 10.0 )
            {
                Vector2D self_vel = wm.self().vel() + accel_vec * ( p * wm.self().dashRate() );
                Vector2D self_next = wm.self().pos() + self_vel;

                if ( self_next.x < wm.offsideLineX() - offside_buf )
                {
                    dash_power = p;
                    break;
                }
            }
        }
    }

    this->debugClient().setTarget( heard_pos );
    this->debugClient().addMessage( "Comm:Receive:GoTo%.0f", dash_power );
    Body_GoToPoint( heard_pos,
                    0.5,
                    dash_power ).execute( this );
    this->setNeckAction( new Neck_TurnToBall() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
HeliosPlayer::writeInterceptDecisionLog( const char * short_role_name )
{
    Vector2D trap_pos = world().ball().inertiaPoint( world().interceptTable()->selfReachCycle() );

    dlog.addText( Logger::INTERCEPT,
                  "%s:"
                  "%d," // self step
                  "%d,%d," // teammate step, count
                  "%d,%d," // opponent step count
                  "%lf,%lf," // self pos
                  "%lf,%lf," // ball pos
                  "%lf,%lf," // trap pos
                  ,
                  short_role_name,
                  world().interceptTable()->selfReachCycle(),
                  world().interceptTable()->teammateReachCycle(),
                  ( world().interceptTable()->fastestTeammate() ? world().interceptTable()->fastestTeammate()->posCount() : 1000 ),
                  world().interceptTable()->opponentReachCycle(),
                  ( world().interceptTable()->fastestOpponent() ? world().interceptTable()->fastestOpponent()->posCount() : 1000 ),
                  world().self().pos().x,
                  world().self().pos().y,
                  world().ball().pos().x,
                  world().ball().pos().y,
                  trap_pos.x,
                  trap_pos.y );
}

/*-------------------------------------------------------------------*/
/*!

*/
FieldEvaluator::Ptr
HeliosPlayer::createFieldEvaluator() const
{
    FieldEvaluator::Ptr ptr;

    if ( Options::i().evaluatorName() == "Default" )
    {
        ptr = FieldEvaluator::Ptr( new FieldEvaluator2016() );
    }
    else if ( Options::i().evaluatorName() == "SVMRank" )
    {
        ptr = FieldEvaluator::Ptr( new FieldEvaluatorSVMRank() );
    }
    else
    {
        std::cerr << __FILE__ << ": Unknown evaluator name ["
                  << Options::i().evaluatorName() << ']' << std::endl;
    }

    if ( ptr
         && ! ptr->isValid() )
    {
        std::cerr << "ERROR: invalid FieldEvaluator" << std::endl;
        return FieldEvaluator::Ptr();
    }

    return ptr;
}

/*-------------------------------------------------------------------*/
/*!

*/
#include "actgen_clear.h"
#include "actgen_cross.h"
#include "actgen_direct_pass.h"
#include "actgen_hold.h"
#include "actgen_keep_dribble.h"
#include "actgen_omni_dribble.h"
#include "actgen_pass.h"
#include "actgen_self_pass.h"
#include "actgen_short_dribble.h"
#include "actgen_simple_cross.h"
#include "actgen_simple_dribble.h"
#include "actgen_voronoi_pass.h"
#include "actgen_shoot.h"
#include "actgen_filter.h"

ActionGeneratorHolder::ConstPtr
HeliosPlayer::createActionGenerator() const
{
    ActionGeneratorHolder::Ptr g( new ActionGeneratorHolder() );

    if ( world().self().goalie() )
    {
        g->addGenerator( new ActGen_MinActionChainLengthFilter( new ActGen_Shoot(), 2 ) );
        g->addGenerator( new ActGen_MaxActionChainLengthFilter( new ActGen_Pass(), 1 ) );
        g->addGenerator( new ActGen_MinActionChainLengthFilter( new ActGen_DirectPass(), 2 ) );

        return g;
    }

    //
    // shoot generator (for depth 2 or more) have to be registered first.
    //

    // shoot
    g->addGenerator( new ActGen_MinActionChainLengthFilter( new ActGen_Shoot(), 2 ) );


    //
    // for depth 1
    //

    // cross
    g->addGenerator( new ActGen_MaxActionChainLengthFilter( new ActGen_Cross(), 1 ) );
    // precise pass
    g->addGenerator( new ActGen_MaxActionChainLengthFilter( new ActGen_Pass(), 1 ) );
    // short dribble
    g->addGenerator( new ActGen_MaxActionChainLengthFilter( new ActGen_ShortDribble(), 1 ) );
    // self pass (long dribble)
    g->addGenerator( new ActGen_MaxActionChainLengthFilter( new ActGen_SelfPass(), 1 ) );
    // omni dribble
    g->addGenerator( new ActGen_MaxActionChainLengthFilter( new ActGen_OmniDribble(), 1 ) );
    // keep dribble
    g->addGenerator( new ActGen_MaxActionChainLengthFilter( new ActGen_KeepDribble(), 1 ) );


#if 0
    // clear ball
    g->addGenerator( new ActGen_MaxActionChainLengthFilter( new ActGen_Clear(), 1 ) );
#endif
#if 0
    // hold ball
    g->addGenerator( new ActGen_MaxActionChainLengthFilter( new ActGen_Hold(), 1 ) );
#endif


    //
    // for depth 2
    //

    // direct pass
    g->addGenerator( new ActGen_MinActionChainLengthFilter( new ActGen_DirectPass(), 2 ) );
    // voronoi pass
    g->addGenerator( new ActGen_MinActionChainLengthFilter( new ActGen_VoronoiPass(), 2 ) );
    // simple cross
    g->addGenerator( new ActGen_MinActionChainLengthFilter( new ActGen_SimpleCross(), 2 ) );
    // simple dribble
    g->addGenerator( new ActGen_MinActionChainLengthFilter( new ActGen_SimpleDribble(), 2 ) );

    return g;
}
