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

#ifndef COACH_STRATEGY_H
#define COACH_STRATEGY_H

#include "types.h"
#include "formation_factory.h"

#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_mode.h>
#include <rcsc/types.h>

namespace rcsc {
class CoachWorldModel;
}

class CoachStrategy {
private:

    FormationFactory M_formation_factory;

    SituationType M_current_situation;

    int M_role_number[11];

    // current formation data
    std::string M_role_names[12];
    PositionType M_position_types[11];
    rcsc::Vector2D M_positions[11];
    bool M_marker[11];
    bool M_setplay_marker[11];

    std::vector< int > M_hetero_order;

    // private for singleton
    CoachStrategy();

    // not used
    CoachStrategy( const CoachStrategy & );
    const CoachStrategy & operator=( const CoachStrategy & );
public:

    static
    CoachStrategy & instance();

    static
    const CoachStrategy & i()
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

    void update( const rcsc::CoachWorldModel & wm );

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

    bool isMarkerType( const int unum ) const;
    bool isSetPlayMarkerType( const int unum ) const;

    const std::string & getRoleName( const int unum ) const;
    PositionType getPositionType( const int unum ) const;
    rcsc::Vector2D getPosition( const int unum ) const;
    rcsc::Formation::ConstPtr getAgent2dSetplayDefenseFormation() const;


    const std::vector< int > & heteroOrder() const { return M_hetero_order; }

private:

    void updateSituation( const rcsc::CoachWorldModel & wm );
    void updateFormation( const rcsc::CoachWorldModel & wm );
    void updateHeteroOrder( const rcsc::CoachWorldModel & wm );

    rcsc::Formation::ConstPtr getFormation( const rcsc::CoachWorldModel & wm ) const;

public:
    static
    BallArea get_ball_area( const rcsc::CoachWorldModel & wm );
    static
    BallArea get_ball_area( const rcsc::Vector2D & ball_pos );
};

#endif
