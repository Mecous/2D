// -*-c++-*-

/*!
  \file strategy.h
  \brief team strategy manager Header File
*/

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

#ifndef STRATEGY_H
#define STRATEGY_H

#include "types.h"
#include "formation_factory.h"

#include "soccer_role.h"

#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_mode.h>
#include <rcsc/types.h>

#include <map>
#include <string>

namespace rcsc {
class WorldModel;
}

class Strategy {
public:

    enum OpponentType {
        Type_WrightEagle,
        Type_MarliK,
        Type_Oxsy,
        Type_Gliders,
        Type_CYRUS,
        Type_agent2d,
        Type_Unknown,
    };

private:

    typedef std::map< std::string, SoccerRole::Creator > RoleFactory;

    RoleFactory M_role_factory;

    FormationFactory M_formation_factory;

    // situation type
    SituationType M_current_situation;

    // role assignment
    int M_role_number[11];

    // current formation data
    std::string M_role_names[12];
    rcsc::Formation::RoleType M_role_types[11];
    PositionType M_position_types[11];
    rcsc::Vector2D M_positions[11];
    bool M_marker[11];
    bool M_set_play_marker[11];


    //! key : team name, value: opponent type
    std::map< std::string, OpponentType > M_opponent_type_map;

    OpponentType M_current_opponent_type;

    // corner kick formation type
    std::string M_cornerkick_type;


    // defense type
    std::string M_defense_type;


    // private for singleton
    Strategy();

    // not used
    Strategy( const Strategy & );
    const Strategy & operator=( const Strategy & );
public:

    static
    Strategy & instance();

    static
    const Strategy & i()
      {
          return instance();
      }

    //
    // initialization
    //

    bool init();


    //
    // update
    //

    void update( const rcsc::WorldModel & wm );

    void exchangeRole( const int unum0,
                       const int unum1 );

    //
    // accessor
    //

    int goalieUnum() const
      {
          return M_formation_factory.goalieUnum();
      }

    int roleNumber( const int unum ) const
      {
          if ( unum < 1 || 11 < unum ) return unum;
          return M_role_number[unum - 1];
      }

    //
    // role param
    //

    rcsc::Formation::RoleType roleType( const int inum ) const;
    bool isMarkerType( const int unum ) const;
    bool isSetPlayMarkerType( const int unum ) const;

    //
    //
    //

    SoccerRole::Ptr createRole( const int unum,
                                const rcsc::WorldModel & wm ) const;

    const std::string & getRoleName( const int unum ) const;
    PositionType getPositionType( const int unum ) const;
    rcsc::Vector2D getPosition( const int unum ) const;

    rcsc::Formation::ConstPtr getCornerKickPreFormation( const std::string & teamname ) const;
    rcsc::Formation::ConstPtr getCornerKickPostFormation( const std::string & teamname,
                                                          const std::string & cornerkick_type ) const;

    OpponentType opponentType() const
      {
          return M_current_opponent_type;
      }

    const std::string & getCornerKickType() const
      {
          return M_cornerkick_type;
      }
    void setCornerKickType( const std::string & cornerkick_type )
      {
          M_cornerkick_type = cornerkick_type;
      }

    void setDefenseType( const std::string & defense_type );

private:

    void updateSituation( const rcsc::WorldModel & wm );
    void updateFormation( const rcsc::WorldModel & wm );
    void updateOpponentType( const rcsc::WorldModel & wm );
    void updateOpponentType( const std::string & type_name );

    rcsc::Formation::ConstPtr getFormation( const rcsc::WorldModel & wm ) const;

    rcsc::Formation::ConstPtr getWallBreakFormation( const rcsc::WorldModel & wm ) const;

    rcsc::Formation::ConstPtr getGoalieFormation( const rcsc::WorldModel & wm ) const;

public:
    static
    BallArea get_ball_area( const rcsc::WorldModel & wm );
    static
    BallArea get_ball_area( const rcsc::Vector2D & ball_pos );

    static
    double get_normal_dash_power( const rcsc::WorldModel & wm );

    static
    double get_remaining_time_rate( const rcsc::WorldModel & wm );
};

#endif
