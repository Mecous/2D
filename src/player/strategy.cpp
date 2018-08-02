// -*-c++-*-

/*!
  \file strategy.cpp
  \brief team strategh Source File
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "strategy.h"

#include "options.h"
#include "field_analyzer.h"

#include "soccer_role.h"

#include "role_sample.h"

#include "role_center_back.h"
#include "role_center_forward.h"
#include "role_center_half.h"
#include "role_defensive_half.h"
#include "role_forward.h"
#include "role_goalie.h"
#include "role_offensive_half.h"
#include "role_savior.h"
#include "role_side_back.h"
#include "role_side_forward.h"
#include "role_side_half.h"
#include "role_sweeper.h"

#include <rcsc/player/intercept_table.h>
#include <rcsc/player/world_model.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include <fstream>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
Strategy::Strategy()
    : M_current_situation( Normal_Situation ),
      M_current_opponent_type( Type_Unknown )
{
    //
    // roles
    //

    M_role_factory[RoleSample::name()] = &RoleSample::create;

    M_role_factory[RoleGoalie::name()] = &RoleGoalie::create;
    M_role_factory[RoleSavior::name()] = &RoleSavior::create;
    M_role_factory[RoleSweeper::name()] = &RoleSweeper::create;
    M_role_factory[RoleCenterBack::name()] = &RoleCenterBack::create;
    M_role_factory[RoleSideBack::name()] = &RoleSideBack::create;
    M_role_factory[RoleDefensiveHalf::name()] = &RoleDefensiveHalf::create;
    M_role_factory[RoleOffensiveHalf::name()] = &RoleOffensiveHalf::create;
    M_role_factory[RoleCenterHalf::name()] = &RoleCenterHalf::create;
    M_role_factory[RoleSideHalf::name()] = &RoleSideHalf::create;
    M_role_factory[RoleSideForward::name()] = &RoleSideForward::create;
    M_role_factory[RoleCenterForward::name()] = &RoleCenterForward::create;
    M_role_factory[RoleForward::name()] = &RoleForward::create;

    for ( size_t i = 0; i < 11; ++i )
    {
        M_role_number[i] = i + 1;
        M_role_types[i] = Formation::MidFielder;
        M_position_types[i] = Position_Center;
        M_positions[i].assign( 0.0, 0.0 );
        M_marker[i] = false;
        M_set_play_marker[i] = false;
    }

    M_role_names[11] = "NoRole";

    M_cornerkick_type = "default";

    M_defense_type = "default";

    M_opponent_type_map[ "MarliK" ] = Type_MarliK;
    M_opponent_type_map[ "MarIiK" ] = Type_MarliK;
    M_opponent_type_map[ "FCPerspolis" ] = Type_MarliK;
    M_opponent_type_map[ "Damash" ] = Type_MarliK;

    M_opponent_type_map[ "WrightEagle" ] = Type_WrightEagle;

    M_opponent_type_map[ "Oxsy" ] = Type_Oxsy;

    M_opponent_type_map[ "Gliders2013" ] = Type_Gliders;
    M_opponent_type_map[ "Gliders2014" ] = Type_Gliders;
    M_opponent_type_map[ "Gliders2015" ] = Type_Gliders;
    M_opponent_type_map[ "Gliders2016" ] = Type_Gliders;

    M_opponent_type_map[ "CYRUS" ] = Type_CYRUS;

    M_opponent_type_map[ "HELIOS_base" ] = Type_agent2d;
}

/*-------------------------------------------------------------------*/
/*!

 */
Strategy &
Strategy::instance()
{
    static Strategy s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Strategy::init()
{
    if ( ! M_formation_factory.init() )
    {
        return false;
    }


    //
    // check role availability
    //

    for ( FormationFactory::Map::const_iterator it = M_formation_factory.allFormationMap().begin(),
              end = M_formation_factory.allFormationMap().end();
          it != end;
          ++it )
    {
        for ( int unum = 1; unum < 11; ++unum )
        {
            const std::string role_name = it->second->getRoleName( unum );

            if ( M_role_factory.find( role_name ) == M_role_factory.end() )
            {
                std::cerr << __FILE__ << ':' << __LINE__ << ':'
                      << " ***ERROR*** Unsupported role name ["
                      << role_name << "]" << std::endl;
                return false;
            }

            M_role_names[unum - 1] = role_name;
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Strategy::update( const WorldModel & wm )
{
    static GameTime s_update_time( -1, 0 );

    Vector2D center(0,0);
    dlog.addCircle( Logger::TEAM,
                    center,
                    10.0 );

    if ( s_update_time == wm.time() )
    {
        return;
    }
    s_update_time = wm.time();

    updateOpponentType( wm );
    updateSituation( wm );
    updateFormation( wm );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Strategy::exchangeRole( const int unum0,
                        const int unum1 )
{
    if ( unum0 < 1 || 11 < unum0
         || unum1 < 1 || 11 < unum1 )
    {
        std::cerr << __FILE__ << ':' << __LINE__ << ':'
                  << "(exchangeRole) Illegal uniform number. "
                  << unum0 << ' ' << unum1
                  << std::endl;
        dlog.addText( Logger::TEAM,
                      __FILE__":(exchangeRole) Illegal unum. %d %d",
                      unum0, unum1 );
        return;
    }

    if ( unum0 == unum1 )
    {
        std::cerr << __FILE__ << ':' << __LINE__ << ':'
                  << "(exchangeRole) same uniform number. "
                  << unum0 << ' ' << unum1
                  << std::endl;
        dlog.addText( Logger::TEAM,
                      __FILE__":(exchangeRole) same unum. %d %d",
                      unum0, unum1 );
        return;
    }

    int role0 = M_role_number[unum0 - 1];
    int role1 = M_role_number[unum1 - 1];

    dlog.addText( Logger::TEAM,
                  __FILE__":(exchangeRole) unum=%d(role=%d) <-> unum=%d(role=%d)",
                  unum0, role0,
                  unum1, role1 );

    M_role_number[unum0 - 1] = role1;
    M_role_number[unum1 - 1] = role0;
}

/*-------------------------------------------------------------------*/
/*!

*/
Formation::RoleType
Strategy::roleType( const int unum ) const
{
    int number = roleNumber( unum );
    if ( number < 1 || 11 < number )
    {
        return Formation::MidFielder;
    }

    return M_role_types[number - 1];
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Strategy::isMarkerType( const int unum ) const
{
    int number = roleNumber( unum );
    if ( number < 1 || 11 < number )
    {
        return false;
    }

    return M_marker[number - 1];
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Strategy::isSetPlayMarkerType( const int unum ) const
{
    int number = roleNumber( unum );
    if ( number < 1 || 11 < number )
    {
        return false;
    }

    return M_set_play_marker[number - 1];
}

/*-------------------------------------------------------------------*/
/*!

 */
SoccerRole::Ptr
Strategy::createRole( const int unum,
                      const WorldModel & wm ) const
{
    const int number = roleNumber( unum );

    SoccerRole::Ptr role;

    if ( number < 1 || 11 < number )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " ***ERROR*** Invalid player number " << number
                  << std::endl;
        return role;
    }

    Formation::ConstPtr f = getFormation( wm );
    if ( ! f )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " ***ERROR*** faled to create role. Null formation" << std::endl;
        return role;
    }

    const std::string role_name = f->getRoleName( number );

    RoleFactory::const_iterator factory = M_role_factory.find( role_name );
    if ( factory != M_role_factory.end() )
    {
        role = factory->second();
    }

    if ( ! role )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " ***ERROR*** unsupported role name ["
                  << role_name << "]"
                  << std::endl;
    }
    return role;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Strategy::updateSituation( const WorldModel & wm )
{
    M_current_situation = Normal_Situation;

    if ( wm.gameMode().type() != GameMode::PlayOn )
    {
        if ( wm.gameMode().isPenaltyKickMode() )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": Situation PenaltyKick" );
            M_current_situation = PenaltyKick_Situation;
        }
        else if ( wm.gameMode().isOurSetPlay( wm.ourSide() ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": Situation OurSetPlay" );
            M_current_situation = OurSetPlay_Situation;
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": Situation OppSetPlay" );
            M_current_situation = OppSetPlay_Situation;
        }
        return;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();
    int our_min = std::min( self_min, mate_min );

    if ( opp_min <= our_min - 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Situation Defense" );
        M_current_situation = Defense_Situation;
        return;
    }

    if ( our_min <= opp_min - 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Situation Offense" );
        M_current_situation = Offense_Situation;
        return;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": Situation Normal" );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Strategy::updateFormation( const WorldModel & wm )
{
    Formation::ConstPtr f = getFormation( wm );
    FieldAnalyzer::instance().setOurFormation( f );

    if ( ! f )
    {
        std::cerr << wm.teamName() << ':' << wm.self().unum() << ": "
                  << wm.time()
                  << " ***ERROR*** could not get the current formation" << std::endl;
        return;
    }

    Formation::ConstPtr gf = getGoalieFormation( wm );
    if ( ! gf )
    {
        std::cerr << wm.teamName() << ':' << wm.self().unum() << ": "
                  << wm.time()
                  << " ***ERROR*** could not get the current formation" << std::endl;
        return;
    }

    //
    //
    //
    int ball_step = 0;
    if ( wm.gameMode().type() == GameMode::PlayOn
         || wm.gameMode().type() == GameMode::GoalKick_ )
    {
        ball_step = std::min( 1000, wm.interceptTable()->teammateReachCycle() );
        ball_step = std::min( ball_step, wm.interceptTable()->opponentReachCycle() );
        ball_step = std::min( ball_step, wm.interceptTable()->selfReachCycle() );
    }

    //const Vector2D ball_pos = wm.ball().inertiaPoint( ball_step );
    const Vector2D ball_pos = FieldAnalyzer::get_field_bound_predict_ball_pos( wm, ball_step, 0.0 );
    dlog.addText( Logger::TEAM,
                  __FILE__": HOME POSITION: ball pos=(%.1f %.1f) step=%d",
                  ball_pos.x, ball_pos.y,
                  ball_step );

    //
    //
    //
    {
        std::vector< Vector2D > positions;
        f->getPositions( ball_pos, positions );

        if ( positions.size() != 11 )
        {
            std::cerr << wm.teamName() << ':' << wm.self().unum() << ": "
                      << wm.time()
                      << " ***ERROR*** could not get positions." << std::endl;
            return;
        }

        std::copy( positions.begin(), positions.end(), M_positions );
    }

    //
    //
    //
    {
        int goalie_unum = goalieUnum();
        if ( goalie_unum < 1 || 11 < goalie_unum )
        {
            goalie_unum = 1;
        }

        //const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );
        const Vector2D opponent_ball_pos = FieldAnalyzer::get_field_bound_opponent_ball_pos( wm );

        Vector2D goalie_position = gf->getPosition( goalie_unum, opponent_ball_pos );

        if ( ! goalie_position.isValid() )
        {
            std::cerr << wm.teamName() << ':' << wm.self().unum() << ": "
                      << wm.time()
                      << " ***ERROR*** could not get positions." << std::endl;
        }
        else
        {
            M_positions[goalie_unum-1] = goalie_position;
        }
    }

    //
    //
    //

    if ( ServerParam::i().useOffside() )
    {
        double max_x = ServerParam::i().pitchHalfLength();
        if ( ServerParam::i().kickoffOffside()
             && ( wm.gameMode().type() == GameMode::BeforeKickOff
                  || wm.gameMode().type() == GameMode::AfterGoal_ ) )
        {
            max_x = -1.0e-3;
        }
        else if ( wm.gameMode().type() == GameMode::PlayOn )
        {
            max_x = wm.offsideLineX();
            if ( ball_pos.x > max_x )
            {
                max_x = ball_pos.x;
            }

            // if ( opponentType() == Type_Gliders )
            // {
            //     max_x -= 2.0;
            // }
            // else
            {
                max_x -= 1.0;
            }
        }

        for ( int i = 0; i < 11; ++i )
        {
            if ( M_positions[i].x > max_x )
            {
                dlog.addText( Logger::TEAM,
                              "____ %d offside. home_pos_x %.2f -> %.2f",
                              i + 1,
                              M_positions[i].x, max_x );
                M_positions[i].x = max_x;
            }
        }
    }

    //
    //
    //
    for ( int i = 0; i < 11; ++i )
    {
        const int unum = i + 1;

        // role name
        M_role_names[i] = f->getRoleName( unum );

        // position type
        PositionType type = Position_Center;
        if ( f->isSideType( unum ) )
        {
            type = Position_Left;
        }
        else if ( f->isSymmetryType( unum ) )
        {
            type = Position_Right;
        }

        M_position_types[i] = type;

        // marker flag
        M_marker[i] = false;
        M_set_play_marker[i] = false;
        if ( wm.ourCard( unum ) != RED )
        {
            M_role_types[i] = f->roleType( unum );
            M_marker[i] = f->isMarker( unum );
            M_set_play_marker[i] = f->isSetPlayMarker( unum );
        }
#if 0
        dlog.addText( Logger::TEAM,
                      "__ %d home pos (%.2f %.2f) type=%d marker=%s setplay_marker=%s",
                      unum,
                      M_positions[i].x, M_positions[i].y,
                      type,
                      M_marker[i] ? "on" : "off",
                      M_set_play_marker[i] ? "on" : "off" );
#endif
        dlog.addRect( Logger::TEAM,
                      M_positions[i].x - 0.4, M_positions[i].y - 0.4, 0.8, 0.8,
                      "#000" );
        char num[8]; snprintf( num, 8, "%d", unum );
        dlog.addMessage( Logger::TEAM, M_positions[i], num, "#000" );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Strategy::updateOpponentType( const WorldModel & wm )
{
    if ( M_current_opponent_type != Type_Unknown )
    {
        return;
    }

    std::map< std::string, OpponentType >::const_iterator it = M_opponent_type_map.find( wm.theirTeamName() );
    if ( it != M_opponent_type_map.end() )
    {
        M_current_opponent_type = it->second;
        dlog.addText( Logger::TEAM,
                      __FILE__"(updateOpponentType) name=[%s] type=%d",
                      wm.theirTeamName().c_str(), M_current_opponent_type );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__"(updateOpponentType) name=[%s] unknow type",
                      wm.theirTeamName().c_str() );
        M_current_opponent_type = Type_Unknown;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Strategy::updateOpponentType( const std::string & type_name )
{
    if ( M_current_opponent_type != Type_Unknown
         && M_current_opponent_type != Type_CYRUS )
    {
        return;
    }

    std::map< std::string, OpponentType >::const_iterator it = M_opponent_type_map.find( type_name );
    if ( it != M_opponent_type_map.end() )
    {
        M_current_opponent_type = it->second;
        dlog.addText( Logger::TEAM,
                      __FILE__"(updateOpponentType) name=[%s] type=%d",
                      type_name.c_str(), M_current_opponent_type );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__"(updateOpponentType) name=[%s] unknow type",
                      type_name.c_str() );
        M_current_opponent_type = Type_Unknown;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Strategy::setDefenseType( const std::string & defense_type )
{
    if ( defense_type == "wall" )
    {
        updateOpponentType( "CYRUS" );
    }
    else
    {
        updateOpponentType( "Default" );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
const std::string &
Strategy::getRoleName( const int unum ) const
{
    const int number = roleNumber( unum );

    if ( number < 1 || 11 < number )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": Illegal number : " << number
                  << std::endl;
        return M_role_names[11];
    }

    return M_role_names[number - 1];
}

/*-------------------------------------------------------------------*/
/*!

 */
PositionType
Strategy::getPositionType( const int unum ) const
{
    const int number = roleNumber( unum );

    if ( number < 1 || 11 < number )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": Illegal number : " << number
                  << std::endl;
        return Position_Center;
    }

    return M_position_types[number - 1];
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Strategy::getPosition( const int unum ) const
{
    const int number = roleNumber( unum );

    if ( number < 1 || 11 < number )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": Illegal number : " << number
                  << std::endl;
        return Vector2D::INVALIDATED;
    }

    return M_positions[number - 1];
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
Strategy::getFormation( const WorldModel & wm ) const
{
    std::string team_name = wm.theirTeamName();

    if ( M_current_opponent_type == Type_CYRUS )
    {
        team_name = "CYRUS";
    }

    return M_formation_factory.getFormation( wm.ourSide(),
                                             team_name,//wm.theirTeamName(),
                                             wm.gameMode(),
                                             wm.ball().pos(),
                                             M_current_situation,
                                             false );
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
Strategy::getCornerKickPreFormation( const std::string & teamname) const
{
    return M_formation_factory.getCornerKickPreFormation( teamname );
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
Strategy::getCornerKickPostFormation( const std::string & teamname,
                                      const std::string & cornerkick_type ) const
{
    return M_formation_factory.getCornerKickPostFormation( teamname,
                                                           cornerkick_type );
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
Strategy::getGoalieFormation( const WorldModel & wm ) const
{
    return M_formation_factory.getFormation( wm.ourSide(),
                                             wm.theirTeamName(),
                                             wm.gameMode(),
                                             wm.ball().pos(),
                                             M_current_situation,
                                             true );
}

/*-------------------------------------------------------------------*/
/*!

 */
BallArea
Strategy::get_ball_area( const WorldModel & wm )
{
    int ball_step = 1000;
    ball_step = std::min( ball_step, wm.interceptTable()->teammateReachCycle() );
    ball_step = std::min( ball_step, wm.interceptTable()->opponentReachCycle() );
    ball_step = std::min( ball_step, wm.interceptTable()->selfReachCycle() );

    //return get_ball_area( wm.ball().inertiaPoint( ball_step ) );
    return get_ball_area( FieldAnalyzer::get_field_bound_predict_ball_pos( wm, ball_step, 0.1 ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
BallArea
Strategy::get_ball_area( const Vector2D & ball_pos )
{
    dlog.addLine( Logger::TEAM,
                  52.5, -17.0, -52.5, -17.0,
                  "#999999" );
    dlog.addLine( Logger::TEAM,
                  52.5, 17.0, -52.5, 17.0,
                  "#999999" );
    dlog.addLine( Logger::TEAM,
                  36.0, -34.0, 36.0, 34.0,
                  "#999999" );
    dlog.addLine( Logger::TEAM,
                  -1.0, -34.0, -1.0, 34.0,
                  "#999999" );
    dlog.addLine( Logger::TEAM,
                  -30.0, -17.0, -30.0, 17.0,
                  "#999999" );
    dlog.addLine( Logger::TEAM,
                  //-36.5, -34.0, -36.5, 34.0,
                  -35.5, -34.0, -35.5, 34.0,
                  "#999999" );

    if ( ball_pos.x > 36.0 )
    {
        if ( ball_pos.absY() > 17.0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": get_ball_area: Cross" );
            dlog.addRect( Logger::TEAM,
                          36.0, -34.0, 52.5 - 36.0, 34.0 - 17.0,
                          "#00ff00" );
            dlog.addRect( Logger::TEAM,
                          36.0, 17.0, 52.5 - 36.0, 34.0 - 17.0,
                          "#00ff00" );
            return BA_Cross;
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": get_ball_area: ShootChance" );
            dlog.addRect( Logger::TEAM,
                          36.0, -17.0, 52.5 - 36.0, 34.0,
                          "#00ff00" );
            return BA_ShootChance;
        }
    }
    else if ( ball_pos.x > -1.0 )
    {
        if ( ball_pos.absY() > 17.0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": get_ball_area: DribbleAttack" );
            dlog.addRect( Logger::TEAM,
                          -1.0, -34.0, 36.0 + 1.0, 34.0 - 17.0,
                          "#00ff00" );
            dlog.addRect( Logger::TEAM,
                          -1.0, 17.0, 36.0 + 1.0, 34.0 - 17.0,
                          "#00ff00" );
            return BA_DribbleAttack;
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": get_ball_area: OffMidField" );
            dlog.addRect( Logger::TEAM,
                          -1.0, -17.0, 36.0 + 1.0, 34.0,
                          "#00ff00" );
            return BA_OffMidField;
        }
    }
    else if ( ball_pos.x > -30.0 )
    {
        if ( ball_pos.absY() > 17.0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": get_ball_area: DribbleBlock" );
            dlog.addRect( Logger::TEAM,
                          -30.0, -34.0, -1.0 + 30.0, 34.0 - 17.0,
                          "#00ff00" );
            dlog.addRect( Logger::TEAM,
                          -30.0, 17.0, -1.0 + 30.0, 34.0 - 17.0,
                          "#00ff00" );
            return BA_DribbleBlock;
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": get_ball_area: DefMidField" );
            dlog.addRect( Logger::TEAM,
                          -30.0, -17.0, -1.0 + 30.0, 34.0,
                          "#00ff00" );
            return BA_DefMidField;
        }
    }
    // 2009-06-17 akiyama: -36.5 -> -35.5
    //else if ( ball_pos.x > -36.5 )
    else if ( ball_pos.x > -35.5 )
    {
        if ( ball_pos.absY() > 17.0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": get_ball_area: CrossBlock" );
            dlog.addRect( Logger::TEAM,
                          //-36.5, -34.0, 36.5 - 30.0, 34.0 - 17.0,
                          -35.5, -34.0, 35.5 - 30.0, 34.0 - 17.0,
                          "#00ff00" );
            dlog.addRect( Logger::TEAM,
                          -35.5, 17.0, 35.5 - 30.0, 34.0 - 17.0,
                          "#00ff00" );
            return BA_CrossBlock;
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": get_ball_area: Danger (used to be Stopper)" );
            dlog.addRect( Logger::TEAM,
                          //-36.5, -17.0, 36.5 - 30.0, 34.0,
                          -35.5, -17.0, 35.5 - 30.0, 34.0,
                          "#00ff00" );
            // 2009-06-17 akiyama: Stopper -> DefMidField
            // 2011-07-01 akiyama: DefMidField -> Danger
            //return BA_Stopper;
            return BA_Danger;
        }
    }
    else
    {
        if ( ball_pos.absY() > 17.0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": get_ball_area: CrossBlock" );
            dlog.addRect( Logger::TEAM,
                          -52.5, -34.0, 52.5 - 36.5, 34.0 - 17.0,
                          "#00ff00" );
            dlog.addRect( Logger::TEAM,
                          -52.5, 17.0, 52.5 - 36.5, 34.0 - 17.0,
                          "#00ff00" );
            return BA_CrossBlock;
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": get_ball_area: Danger" );
            dlog.addRect( Logger::TEAM,
                          -52.5, -17.0, 52.5 - 36.5, 34.0,
                          "#00ff00" );
            return BA_Danger;
        }
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": get_ball_area: unknown area" );
    return BA_None;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Strategy::get_normal_dash_power( const WorldModel & wm )
{
    static bool s_recover_mode = false;

    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        return std::min( ServerParam::i().maxDashPower(),
                         wm.self().stamina() + wm.self().playerType().extraStamina() );
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    // check recover
    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        s_recover_mode = false;
    }
    else if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.5 )
    {
        s_recover_mode = true;
    }
    else if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.7 )
    {
        s_recover_mode = false;
    }

    /*--------------------------------------------------------*/
    double dash_power = ServerParam::i().maxDashPower();
    const double my_inc
        = wm.self().playerType().staminaIncMax()
        * wm.self().recovery();

    if ( wm.ourDefenseLineX() > wm.self().pos().x
         && wm.ball().pos().x < wm.ourDefenseLineX() + 20.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_normal_dash_power) correct DF line. keep max power" );
        // keep max power
        dash_power = ServerParam::i().maxDashPower();
    }
    else if ( s_recover_mode )
    {
        dash_power = my_inc - 25.0; // preffered recover value
        if ( dash_power < 0.0 ) dash_power = 0.0;

        dlog.addText( Logger::TEAM,
                      __FILE__": (get_normal_dash_power) recovering" );
    }
    else if ( mate_min <= 2
              && mate_min < opp_min - 1
              && wm.ball().distFromSelf() < 5.0 )
    {
        dash_power = ServerParam::i().maxDashPower();
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_normal_dash_power) fast move",
                      dash_power );
    }
    // exist kickable teammate
    else if ( wm.kickableTeammate()
              && wm.ball().distFromSelf() < 20.0 )
    {
        dash_power = std::min( my_inc * 1.1,
                               ServerParam::i().maxDashPower() );
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_normal_dash_power) exist kickable teammate. dash_power=%.1f",
                      dash_power );
    }
    // in offside area
    else if ( wm.self().pos().x > wm.offsideLineX() )
    {
        dash_power = ServerParam::i().maxDashPower();
        dlog.addText( Logger::TEAM,
                      __FILE__": in offside area. dash_power=%.1f",
                      dash_power );
    }
    else if ( wm.ball().pos().x > 25.0
              && wm.ball().pos().x > wm.self().pos().x + 10.0
              && self_min < opp_min - 6
              && mate_min < opp_min - 6 )
    {
        dash_power = bound( ServerParam::i().maxDashPower() * 0.1,
                            my_inc * 0.5,
                            ServerParam::i().maxDashPower() );
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_normal_dash_power) opponent ball dash_power=%.1f",
                      dash_power );
    }
    // normal
    else
    {
        dash_power = std::min( my_inc * 1.7,
                               ServerParam::i().maxDashPower() );
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_normal_dash_power) normal mode dash_power=%.1f",
                      dash_power );
    }

    return dash_power;
}


/*-------------------------------------------------------------------*/
/*!

 */
double
Strategy::get_remaining_time_rate( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();

    double current_rate = 1.0;

    if ( SP.actualHalfTime() >= 0
         && SP.nrNormalHalfs() >= 0 )
    {
        if ( wm.time().cycle() < SP.actualHalfTime() * SP.nrNormalHalfs() )
        {
            current_rate = static_cast< double >( wm.time().cycle() % SP.actualHalfTime() ) / SP.actualHalfTime();
        }
        else if ( SP.actualExtraHalfTime() > 0
                  && SP.nrExtraHalfs() > 0 )
        {
            int t = wm.time().cycle() - ( SP.actualHalfTime() * SP.nrNormalHalfs() );
            current_rate = t / ( SP.actualExtraHalfTime() * SP.nrExtraHalfs() );
        }

        current_rate = bound( 0.0, current_rate, 1.0 );
    }

    return 1.0 - current_rate;
}
