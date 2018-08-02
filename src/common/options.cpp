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

#include "options.h"

#include <rcsc/param/param_map.h>
#include <rcsc/param/cmd_line_parser.h>

#include <iostream>

using namespace rcsc;

const std::string Options::BEFORE_KICK_OFF_BASENAME = "before-kick-off";
const std::string Options::NORMAL_FORMATION_BASENAME = "normal-formation";
const std::string Options::DEFENSE_FORMATION_BASENAME = "defense-formation";
const std::string Options::OFFENSE_FORMATION_BASENAME = "offense-formation";
const std::string Options::CORNER_KICK_OUR_PRE_FORMATION_BASENAME = "cornerkick-our-pre-formation";
const std::string Options::CORNER_KICK_OUR_POST_FORMATION_BASENAME = "cornerkick-our-post-formation";
const std::string Options::GOAL_KICK_OPP_FORMATION_BASENAME = "goal-kick-opp";
const std::string Options::GOAL_KICK_OUR_FORMATION_BASENAME = "goal-kick-our";
const std::string Options::GOALIE_CATCH_OPP_FORMATION_BASENAME = "goalie-catch-opp";
const std::string Options::GOALIE_CATCH_OUR_FORMATION_BASENAME = "goalie-catch-our";
const std::string Options::KICKIN_OUR_FORMATION_BASENAME = "kickin-our-formation";
const std::string Options::SETPLAY_OPP_FORMATION_BASENAME = "setplay-opp-formation";
const std::string Options::SETPLAY_OUR_FORMATION_BASENAME = "setplay-our-formation";
const std::string Options::INDIRECT_FREEKICK_OPP_FORMATION_BASENAME = "indirect-freekick-opp-formation";
const std::string Options::INDIRECT_FREEKICK_OUR_FORMATION_BASENAME = "indirect-freekick-our-formation";
const std::string Options::AGENT2D_SETPLAY_DEFENSE_FORMATION_BASENAME = "agent2d-setplay-defense-formation";

const std::string Options::GOALIE_POSITION_CONF = "goalie-position.conf";

/*-------------------------------------------------------------------*/
/*!

*/
Options &
Options::instance()
{
    static Options s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

*/
Options::Options()
    : M_log_dir( "/tmp/" ),
      M_formation_conf_dir( "./data/formations/" ),
      M_kick_conf_file(), //( "./data/kick.conf" ),
      M_formation_conf_file( "./data/formation.conf"),
      M_overwrite_formation_conf_file( "./data/overwrite_formation.conf"),
      M_hetero_conf_file( "./data/hetero.conf" ),
      M_ball_table_file( "./data/ball_table.dat" ),
      M_chain_search_method( "BestFirstSearch" ),
      M_evaluator_name( "Default" ),
      M_max_chain_length( 4 ),
      M_max_evaluate_size( 1000 ),
      M_sirm_evaluator_param_dir( "./data/sirm_evaluator" ),
      M_svmrank_evaluator_model( "./data/svmrank_evaluator/model" ),
      M_center_forward_free_move_model( "./data/center_forward_free_move/model" ),
      M_intercept_conf_dir( "./data/intercept_probability/" ),
      M_goalie_position_dir( "./data/goalie_position/" ),
      M_opponent_data_dir( "./data/opponent_data/" ),
      M_test_setplay_dir( "./data/test_setplay/" ),
      M_statistic_logging( false )
{

}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Options::init( CmdLineParser & cmd_parser )
{
    ParamMap param_map( "HELIOS global options" );

    param_map.add()
        // ( "log-dir", "", &M_log_dir, "the directory where log files are stored." )
        //
        ( "formation-conf-dir", "", &M_formation_conf_dir, "the directory where formation files exist." )
        //
        ( "kick-conf", "", &M_kick_conf_file, "kick configuration file path." )
        ( "formation-conf", "", &M_formation_conf_file, "formation assignment file." )
        ( "overwrite-formation-conf", "", &M_overwrite_formation_conf_file, "overwrite formation assignment file." )
        ( "hetero-conf", "", &M_hetero_conf_file, "heterogeneous assignment file" )
        ( "ball-table", "", &M_ball_table_file, "ball movement table file path." )
        //
        ( "chain-search-method", "", &M_chain_search_method, "the name of action chain search algorithm." )
        ( "evaluator-name", "", &M_evaluator_name, "field evaluator name." )
        ( "max-chain-length", "", &M_max_chain_length, "maximum action chain length." )
        ( "max-evaluate-size", "", &M_max_evaluate_size, "maximum evaluation size for action search." )
        //
        ( "sirm-evaluator-param-dir", "", &M_sirm_evaluator_param_dir, "parameter directory for the SIRM field evaluator." )
        ( "svmrank-evaluator-model", "", &M_svmrank_evaluator_model, "SVMRank field evaluator model file." )
        ( "center-forward-free-move-model", "", &M_center_forward_free_move_model, "CenterForwardFreeMove model file." )
        //
        ( "intercept-conf-dir", "", &M_intercept_conf_dir, "the directory where intercept conf files exist." )
        //
        ( "goalie-position-dir", "", &M_goalie_position_dir, "the directory where goalie position data files exist." )
        //
        ( "opponent-data-dir", "", &M_opponent_data_dir, "the directory where analyzed opponent data files exist." )
        //
        ( "statistic-logging", "", BoolSwitch( &M_statistic_logging ), "record statistic log" )
        ;


    cmd_parser.parse( param_map );

    if ( cmd_parser.count( "help" ) > 0 )
    {
        param_map.printHelp( std::cout );
        return false;
    }

    return true;
}
