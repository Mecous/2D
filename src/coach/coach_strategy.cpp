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

#include "options.h"

#include <rcsc/coach/coach_world_model.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

#include <fstream>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
CoachStrategy::CoachStrategy()
    : M_current_situation( Normal_Situation )
{
    for ( size_t i = 0; i < 11; ++i )
    {
        M_role_number[i] = i + 1;
        M_position_types[i] = Position_Center;
        M_positions[i].assign( 0.0, 0.0 );
        M_marker[i] = false;
        M_setplay_marker[i] = false;
    }

    M_role_names[11] = "NoRole";

    // M_hetero_order.reserve( 11 );
    // // default assignment for 4231
    // M_hetero_order.push_back( 11 ); // center forward
    // M_hetero_order.push_back( 2 );  // sweeper
    // M_hetero_order.push_back( 3 );  // center back
    // M_hetero_order.push_back( 10 ); // side half
    // M_hetero_order.push_back( 9 );  // side half
    // M_hetero_order.push_back( 4 );  // side back
    // M_hetero_order.push_back( 5 );  // side back
    // M_hetero_order.push_back( 7 );  // defensive half
    // M_hetero_order.push_back( 8 );  // defensive half
    // M_hetero_order.push_back( 6 );  // center half
}

/*-------------------------------------------------------------------*/
/*!

 */
CoachStrategy &
CoachStrategy::instance()
{
    static CoachStrategy s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
CoachStrategy::init()
{
    if ( ! M_formation_factory.init() )
    {
        return false;
    }

    //
    // check role availability
    //

    if ( M_formation_factory.allFormationMap().empty() )
    {
        std::cerr << __FILE__ << ":(init) no formation" << std::endl;
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CoachStrategy::update( const CoachWorldModel & wm )
{
    static GameTime s_update_time( -1, 0 );

    if ( s_update_time == wm.time() )
    {
        return;
    }
    s_update_time = wm.time();

#if 1
    static bool s_print_formation = false;
    if ( ! s_print_formation
         && ! wm.theirTeamName().empty() )
    {
        std::cout << wm.ourTeamName() << " coach:"
                  << " opponent=[" << wm.theirTeamName() << "]" << std::endl;

        std::map< std::string, std::string >::const_iterator
            it = M_formation_factory.assignmentMap().find( wm.theirTeamName() );
        if ( it != M_formation_factory.assignmentMap().end() )
        {
            std::cout << wm.ourTeamName() << " coach:"
                      << " formation type=[" << it->second << "]" << std::endl;
        }
        it = M_formation_factory.overwriteAssignmentMap().find( wm.theirTeamName() );
        if ( it != M_formation_factory.overwriteAssignmentMap().end() )
        {
            std::cout << wm.ourTeamName() << " coach:"
                      << " overwrite formation type=["  << it->second << "]" << std::endl;
        }

        s_print_formation = true;
    }
#endif

    updateSituation( wm );
    updateFormation( wm );

    if ( wm.time().cycle() == 0 )
    {
        updateHeteroOrder( wm );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CoachStrategy::exchangeRole( const int unum0,
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
bool
CoachStrategy::isMarkerType( const int unum ) const
{
    if ( unum < 1 || 11 < unum )
    {
        return false;
    }

    return M_marker[unum - 1];
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
CoachStrategy::isSetPlayMarkerType( const int unum ) const
{
    if ( unum < 1 || 11 < unum )
    {
        return false;
    }

    return M_setplay_marker[unum - 1];
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CoachStrategy::updateSituation( const CoachWorldModel & wm )
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
        else if ( wm.gameMode().isPenaltyKickMode() )
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
#if 0
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
#endif
    dlog.addText( Logger::TEAM,
                  __FILE__": Situation Normal" );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CoachStrategy::updateFormation( const CoachWorldModel & wm )
{
    Formation::ConstPtr f = getFormation( wm );
    if ( ! f )
    {
        std::cerr << wm.ourTeamName() << " coach: " << wm.time()
                  << " ***ERROR*** could not get the current formation" << std::endl;
        return;
    }

    const ServerParam & SP = ServerParam::i();

    Vector2D ball_pos = wm.ball().pos();;
    int ball_step = wm.currentState().ballReachStep();

    if ( ball_step < 1000 )
    {
        ball_pos = wm.ball().inertiaPoint( ball_step );
        ball_pos.x = bound( -SP.pitchHalfLength(), ball_pos.x, +SP.pitchHalfLength() );
        ball_pos.y = bound( -SP.pitchHalfWidth(), ball_pos.y, +SP.pitchHalfWidth() );
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": HOME POSITION: ball pos=(%.1f %.1f) step=%d",
                  ball_pos.x, ball_pos.y,
                  ball_step );

    std::vector< Vector2D > positions;
    f->getPositions( ball_pos, positions );

    if ( positions.size() != 11 )
    {
        std::cerr << wm.ourTeamName() << " coach: " << wm.time()
                      << " ***ERROR*** could not get the positions." << std::endl;
        return;
    }

    std::copy( positions.begin(), positions.end(), M_positions );

    if ( SP.useOffside() )
    {
        double max_x = wm.ourOffsideLineX();
        if ( SP.kickoffOffside()
             && ( wm.gameMode().type() == GameMode::BeforeKickOff
                  || wm.gameMode().type() == GameMode::AfterGoal_ ) )
        {
            max_x = 0.0;
        }
        else
        {
            if ( ball_pos.x > max_x )
            {
                max_x = ball_pos.x;
            }

            max_x -= 1.0;
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

    for ( int i = 0; i < 11; ++i )
    {
        const int unum = i + 1;

        M_role_names[i] = f->getRoleName( unum );

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

        M_marker[i] = f->isMarker( unum );
        M_setplay_marker[i] = f->isSetPlayMarker( unum );

        dlog.addText( Logger::TEAM,
                      "__ %d home pos (%.2f %.2f) type=%d marker=%d setpla_marker=%d",
                      unum,
                      M_positions[i].x, M_positions[i].y,
                      type,
                      M_marker[i] ? 1 : 0,
                      M_setplay_marker[i] ? 1 : 0 );
        dlog.addCircle( Logger::TEAM,
                        M_positions[i], 0.5,
                        "#000" );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CoachStrategy::updateHeteroOrder( const CoachWorldModel & wm )
{
    static bool s_hetero_order_updated = false;
    if ( s_hetero_order_updated )
    {
        return;
    }

    if ( wm.theirTeamName().empty() )
    {
        return;
    }

    std::map< std::string, std::string >::const_iterator
        t = M_formation_factory.assignmentMap().find( wm.theirTeamName() );
    if ( t == M_formation_factory.assignmentMap().end() )
    {
        s_hetero_order_updated = true;
        return;
    }

    std::map< std::string, std::vector< int > >::const_iterator
        order = M_formation_factory.heteroOrderMap().find( t->second );
    if ( order == M_formation_factory.heteroOrderMap().end() )
    {
        s_hetero_order_updated = true;
        std::cerr << wm.ourTeamName()
                  << ": hetero order type for [" << t->second << "] not found."
                  << std::endl;
        return;
    }

    M_hetero_order = order->second;
}

/*-------------------------------------------------------------------*/
/*!

 */
const std::string &
CoachStrategy::getRoleName( const int unum ) const
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
CoachStrategy::getPositionType( const int unum ) const
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
CoachStrategy::getPosition( const int unum ) const
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
CoachStrategy::getFormation( const CoachWorldModel & wm ) const
{
    return M_formation_factory.getFormation( wm.ourSide(),
                                             wm.theirTeamName(),
                                             wm.gameMode(),
                                             wm.ball().pos(),
                                             M_current_situation,
                                             false );
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
CoachStrategy::getAgent2dSetplayDefenseFormation() const
{
    return M_formation_factory.getAgent2dSetplayDefenseFormation();
}
/*-------------------------------------------------------------------*/
/*!

 */
BallArea
CoachStrategy::get_ball_area( const CoachWorldModel & wm )
{
    const CoachPlayerObject * fastest_player = wm.currentState().fastestInterceptPlayer();
    if ( fastest_player )
    {
        Vector2D ball_pos = wm.ball().inertiaPoint( fastest_player->ballReachStep() );
        return get_ball_area( ball_pos );
    }

    return get_ball_area( wm.ball().pos() );
}

/*-------------------------------------------------------------------*/
/*!

 */
BallArea
CoachStrategy::get_ball_area( const Vector2D & ball_pos )
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
                          __FILE__": get_ball_area: Stopper" );
            dlog.addRect( Logger::TEAM,
                          //-36.5, -17.0, 36.5 - 30.0, 34.0,
                          -35.5, -17.0, 35.5 - 30.0, 34.0,
                          "#00ff00" );
            // 2009-06-17 akiyama: Stopper -> DefMidField
            //return BA_Stopper;
            return BA_DefMidField;
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
