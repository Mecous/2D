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

#ifndef HELIOS_OPTIONS_H
#define HELIOS_OPTIONS_H

#include <string>

namespace rcsc {
class CmdLineParser;
}

/*!
  \class Options
  \brief singleton command line option holder
*/
class Options {
public:

    static const std::string BEFORE_KICK_OFF_BASENAME;
    static const std::string NORMAL_FORMATION_BASENAME;
    static const std::string DEFENSE_FORMATION_BASENAME;
    static const std::string OFFENSE_FORMATION_BASENAME;
    static const std::string GOAL_KICK_OPP_FORMATION_BASENAME;
    static const std::string GOAL_KICK_OUR_FORMATION_BASENAME;
    static const std::string GOALIE_CATCH_OPP_FORMATION_BASENAME;
    static const std::string GOALIE_CATCH_OUR_FORMATION_BASENAME;
    static const std::string KICKIN_OUR_FORMATION_BASENAME;
    static const std::string SETPLAY_OPP_FORMATION_BASENAME;
    static const std::string SETPLAY_OUR_FORMATION_BASENAME;
    static const std::string INDIRECT_FREEKICK_OPP_FORMATION_BASENAME;
    static const std::string INDIRECT_FREEKICK_OUR_FORMATION_BASENAME;
    static const std::string CORNER_KICK_OUR_PRE_FORMATION_BASENAME;
    static const std::string CORNER_KICK_OUR_POST_FORMATION_BASENAME;
    static const std::string AGENT2D_SETPLAY_DEFENSE_FORMATION_BASENAME;

    static const std::string GOALIE_POSITION_CONF;

private:

    std::string M_log_dir;

    std::string M_formation_conf_dir;
    std::string M_kick_conf_file;
    std::string M_formation_conf_file;
    std::string M_overwrite_formation_conf_file;
    std::string M_hetero_conf_file;
    std::string M_ball_table_file;

    std::string M_chain_search_method;
    std::string M_evaluator_name;
    size_t M_max_chain_length;
    size_t M_max_evaluate_size;

    std::string M_sirm_evaluator_param_dir;
    std::string M_svmrank_evaluator_model;
    std::string M_center_forward_free_move_model;

    std::string M_intercept_conf_dir;

    std::string M_goalie_position_dir;

    std::string M_opponent_data_dir;

    std::string M_test_setplay_dir;

    bool M_statistic_logging;

    //
    //
    //

    Options(); // private for singleton

    // not used
    Options( const Options & );
    const Options & operator=( const Options & );

public:

    static
    Options & instance();

    /*!
      \brief singleton instance interface
      \return const reference to local static instance
    */
    static
    const Options & i()
      {
          return instance();
      }

    bool init( rcsc::CmdLineParser & cmd_parser );

    void setLogDir( const std::string & dir ) { M_log_dir = dir; }
    const std::string & logDir() const { return M_log_dir; }

    const std::string & formationConfDir() const { return M_formation_conf_dir; }
    const std::string & kickConfFile() const { return M_kick_conf_file; }
    const std::string & formationConfFile() const { return M_formation_conf_file; }
    const std::string & overwriteFormationConfFile() const { return M_overwrite_formation_conf_file; }
    const std::string & heteroConfFile() const { return M_hetero_conf_file; }
    const std::string & ballTableFile() const { return M_ball_table_file; }

    const std::string & chainSearchMethod() const { return M_chain_search_method; }
    const std::string & evaluatorName() const { return M_evaluator_name; }
    size_t maxChainLength() const { return M_max_chain_length; }
    size_t maxEvaluateSize() const { return M_max_evaluate_size; }

    const std::string & sirmEvaluatorParamDir() const { return M_sirm_evaluator_param_dir; }
    const std::string & svmrankEvaluatorModel() const { return M_svmrank_evaluator_model; }
    const std::string & centerForwardFreeMoveModel() const { return M_center_forward_free_move_model; }

    const std::string & interceptConfDir() const { return M_intercept_conf_dir; }

    const std::string & goaliePositionDir() const { return M_goalie_position_dir; }

    const std::string & opponentDataDir() const { return M_opponent_data_dir; }

    const std::string & TestSetplayDir() const { return M_test_setplay_dir; }

    bool statisticLogging() const { return M_statistic_logging; }

};

#endif
