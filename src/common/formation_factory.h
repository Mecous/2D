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

#ifndef FORMATION_FACTORY_H
#define FORMATION_FACTORY_H

#include "types.h"

#include <rcsc/formation/formation.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_mode.h>
#include <rcsc/types.h>

#include <map>
#include <vector>
#include <string>

class FormationFactory {
public:

    //! key: filename or teamname
    typedef std::map< std::string , rcsc::Formation::ConstPtr > Map;

private:

    //
    // default formations
    //

    rcsc::Formation::ConstPtr M_before_kick_off_formation;

    rcsc::Formation::ConstPtr M_normal_formation;
    rcsc::Formation::ConstPtr M_defense_formation;
    rcsc::Formation::ConstPtr M_offense_formation;

    rcsc::Formation::ConstPtr M_goal_kick_opp_formation;
    rcsc::Formation::ConstPtr M_goal_kick_our_formation;
    rcsc::Formation::ConstPtr M_goalie_catch_opp_formation;
    rcsc::Formation::ConstPtr M_goalie_catch_our_formation;
    rcsc::Formation::ConstPtr M_kickin_our_formation;
    rcsc::Formation::ConstPtr M_setplay_opp_formation;
    rcsc::Formation::ConstPtr M_setplay_our_formation;
    rcsc::Formation::ConstPtr M_indirect_freekick_opp_formation;
    rcsc::Formation::ConstPtr M_indirect_freekick_our_formation;

    rcsc::Formation::ConstPtr M_corner_kick_our_pre_formation;
    rcsc::Formation::ConstPtr M_corner_kick_our_post_formation;
    rcsc::Formation::ConstPtr M_agent2d_setplay_defense_formation;



    rcsc::Formation::ConstPtr M_goalie_position;

    Map M_all_formation_map; //! key: filename

    //

    Map M_before_kick_off_formation_map; //! key: opponent team name

    Map M_normal_formation_map; //! key: opponent team name
    Map M_defense_formation_map; //! key: opponent team name
    Map M_offense_formation_map; //! key: opponent team name

    Map M_goal_kick_opp_formation_map; //! key: opponent team name
    Map M_goal_kick_our_formation_map; //! key: opponent team name
    Map M_goalie_catch_opp_formation_map; //! key: opponent team name
    Map M_goalie_catch_our_formation_map; //! key: opponent team name
    Map M_kickin_our_formation_map; //! key: opponent team name
    Map M_setplay_opp_formation_map; //! key: opponent team name
    Map M_setplay_our_formation_map; //! key: opponent team name
    Map M_indirect_freekick_opp_formation_map; //! key: opponent team name
    Map M_indirect_freekick_our_formation_map; //! key: opponent team name

    Map M_corner_kick_pre_formation_map; //! key: opponent team name
    Map M_corner_kick_post_formation_map; //! key: opponent team name

    //
    int M_goalie_unum;

    //! key: opponent team name, value: formation type name(4231, 433, ...)
    std::map< std::string, std::string > M_assignment_map;
    std::map< std::string, std::string > M_overwrite_assignment_map;

    //! key: formation type name, value: order of uniform number
    std::map< std::string, std::vector< int > > M_hetero_order_map;

public:

    FormationFactory();
    ~FormationFactory();

    bool init();

    int goalieUnum() const
      {
          return M_goalie_unum;
      }

    const Map & allFormationMap() const
      {
          return M_all_formation_map;
      }

    rcsc::Formation::ConstPtr getFormation( const rcsc::SideID our_side,
                                            const std::string & their_team_name,
                                            const rcsc::GameMode & game_mode,
                                            const rcsc::Vector2D & ball_pos,
                                            const SituationType current_situation,
                                            const bool goalie ) const;

    rcsc::Formation::ConstPtr getFormation( const std::string & filename ) const;

    //
    // default formations
    //

    // before_kick_off
    rcsc::Formation::ConstPtr getBeforeKickOffFormation() const { return M_before_kick_off_formation; }

    // play_on formations
    rcsc::Formation::ConstPtr getNormalFormation() const { return M_normal_formation; }
    rcsc::Formation::ConstPtr getDefenseFormation() const { return M_defense_formation; }
    rcsc::Formation::ConstPtr getOffenseFormation() const { return M_offense_formation; }

    // goal_kick formations
    rcsc::Formation::ConstPtr getTheirGoalKickFormation() const { return M_goal_kick_opp_formation; }
    rcsc::Formation::ConstPtr getOurGoalKickFormation() const { return M_goal_kick_our_formation; }

    // goalie catch
    rcsc::Formation::ConstPtr getTheirGoalieCatchFormation() const { return M_goalie_catch_opp_formation; }
    rcsc::Formation::ConstPtr getOurGoalieCatchFormation() const { return M_goalie_catch_our_formation; }

    // our kickin
    rcsc::Formation::ConstPtr getOurKickInFormation() const { return M_kickin_our_formation; }

    // our corner kick
    rcsc::Formation::ConstPtr getCornerKickOurPreFormation() const { return M_corner_kick_our_pre_formation; }
    rcsc::Formation::ConstPtr getCornerKickOurPostFormation() const { return M_corner_kick_our_post_formation; }

    // other setplay situation
    rcsc::Formation::ConstPtr getTheirSetplayFormation() const { return M_setplay_opp_formation; }
    rcsc::Formation::ConstPtr getOurSetplayFormation() const { return M_setplay_our_formation; }

    // indirect freekick
    rcsc::Formation::ConstPtr getTheirIndirectFreekickFormation() const { return M_indirect_freekick_opp_formation; }
    rcsc::Formation::ConstPtr getOurIndirectFreekickFormation() const { return M_indirect_freekick_our_formation; }

    // agent2d setplay defense
    rcsc::Formation::ConstPtr getAgent2dSetplayDefenseFormation() const { return M_agent2d_setplay_defense_formation; }

    //
    //
    //
    rcsc::Formation::ConstPtr getBeforeKickOffFormation( const std::string & their_team_name ) const;

    rcsc::Formation::ConstPtr getNormalFormation( const std::string & their_team_name ) const;
    rcsc::Formation::ConstPtr getDefenseFormation( const std::string & their_team_name ) const;
    rcsc::Formation::ConstPtr getOffenseFormation( const std::string & their_team_name ) const;

    rcsc::Formation::ConstPtr getOurGoalKickFormation( const std::string & their_team_name ) const;
    rcsc::Formation::ConstPtr getTheirGoalKickFormation( const std::string & their_team_name ) const;

    rcsc::Formation::ConstPtr getTheirGoalieCatchFormation( const std::string & their_team_name ) const;
    rcsc::Formation::ConstPtr getOurGoalieCatchFormation( const std::string & their_team_name ) const;

    rcsc::Formation::ConstPtr getOurKickInFormation( const std::string & their_team_name ) const;

    rcsc::Formation::ConstPtr getCornerKickOurPreFormation( const std::string & their_team_name ) const;
    rcsc::Formation::ConstPtr getCornerKickOurPostFormation( const std::string & their_team_name ) const;

    rcsc::Formation::ConstPtr getTheirSetplayFormation( const std::string & their_team_name ) const;
    rcsc::Formation::ConstPtr getOurSetplayFormation( const std::string & their_team_name ) const;

    rcsc::Formation::ConstPtr getTheirIndirectFreekickFormation( const std::string & their_team_name ) const;
    rcsc::Formation::ConstPtr getOurIndirectFreekickFormation( const std::string & their_team_name ) const;

    // our corner kick pre/post
    rcsc::Formation::ConstPtr getCornerKickPreFormation( const std::string & their_team_name ) const;
    rcsc::Formation::ConstPtr getCornerKickPostFormation( const std::string & their_team_name,
                                                          const std::string & cornerkick_type ) const;

    // our goalie position
    rcsc::Formation::ConstPtr getGoaliePosition() const { return M_goalie_position; }

    //
    //
    //
    //! key: team name, value: formation type name
    const std::map< std::string, std::string > & assignmentMap() const { return M_assignment_map; }
    const std::map< std::string, std::string > & overwriteAssignmentMap() const { return M_overwrite_assignment_map; }

    //! key: formation type name, value: (unum,heteroId)
    const std::map< std::string, std::vector< int > > & heteroOrderMap() const { return M_hetero_order_map; }

private:
    rcsc::Formation::ConstPtr createFormation( const std::string & filepath );

    bool readAllFormations( const std::string & formation_dir );
    bool setFormationFromAllMap( const std::string & filename,
                                 rcsc::Formation::ConstPtr * ptr );

    bool readFormationConf( const std::string & conf_file );
    bool assignFormation( const std::string & their_team_name,
                          const std::string & basename,
                          const std::string & type_name,
                          Map & formation_map );

    bool readOverwriteFormationConf( const std::string & conf_file );
public:
    //! overwrite type of all formations (if exists)
    bool overwriteAllFormations( const std::string & team_name,
                                 const std::string & type_name );
private:
    //! overwrite the specified formation
    bool overwriteFormation( const std::string & team_name,
                             const std::string & basename,
                             const std::string & type_name,
                             Map & formation_map );

    bool readHeteroOrder( const std::string & conf_file );
};

#endif
