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

#include "role_side_half.h"

#include "strategy.h"
#include "mark_analyzer.h"

#include "bhv_tactical_intercept.h"
#include "bhv_chain_action.h"

#include "bhv_basic_move.h"
#include "bhv_block_ball_owner.h"
#include "bhv_go_to_cross_point.h"
#include "bhv_get_ball.h"

#include "bhv_attacker_offensive_move.h"
#include "bhv_offensive_half_defensive_move.h"
#include "bhv_side_half_defensive_move.h"
#include "bhv_side_half_offensive_move.h"
#include "bhv_side_half_attack_move.h"

#include "neck_check_ball_owner.h"

#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

using namespace rcsc;

const std::string RoleSideHalf::NAME( "SideHalf" );

/*-------------------------------------------------------------------*/
/*!

 */
namespace {
rcss::RegHolder role = SoccerRole::creators().autoReg( &RoleSideHalf::create,
                                                       RoleSideHalf::NAME );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleSideHalf::execute( PlayerAgent * agent )
{
    bool kickable = agent->world().self().isKickable();
    if ( agent->world().kickableTeammate()
         && agent->world().teammatesFromBall().front()->distFromBall()
         < agent->world().ball().distFromSelf() )
    {
        kickable = false;
    }

    if ( kickable )
    {
        doKick( agent );
    }
    else
    {
        doMove( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleSideHalf::doKick( PlayerAgent * agent )
{
    if ( Bhv_ChainAction().execute( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (execute) do chain action" );
        agent->debugClient().addMessage( "ChainAction" );
        return;
    }

    Body_HoldBall().execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleSideHalf::doMove( PlayerAgent * agent )
{
#ifdef USE_TACTICAL_INTERCEPT
    if ( Bhv_TacticalIntercept().execute( agent ) )
    {
        return;
    }
#endif

    switch ( Strategy::get_ball_area( agent->world() ) ) {
    case BA_CrossBlock:
    case BA_Danger:
        Bhv_SideHalfDefensiveMove().execute( agent );
        break;
    // case BA_DribbleBlock:
    //     Bhv_SideHalfDefensiveMove().execute( agent );
    //     break;
    case BA_DribbleBlock:
    case BA_DefMidField:
        {
            const int self_step = agent->world().interceptTable()->selfReachStep();
            const int teammate_step = agent->world().interceptTable()->teammateReachStep();
            const int opponent_step = agent->world().interceptTable()->opponentReachStep();
            const int min_step = std::min ( self_step, std::min( teammate_step, opponent_step ) );
            const Vector2D ball_pos = agent->world().ball().inertiaPoint( min_step );

            if ( ball_pos.x > -10.0
                 && teammate_step < opponent_step - 1 )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": area=DefMid OffensiveMove" );
                Bhv_SideHalfOffensiveMove().execute( agent );
            }
            else
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": area=DefMid DefensiveMove" );
                Bhv_SideHalfDefensiveMove().execute( agent );
            }
        }
        break;
    case BA_DribbleAttack:
    case BA_OffMidField:
        {
            const int self_step = agent->world().interceptTable()->selfReachStep();
            const int teammate_step = agent->world().interceptTable()->teammateReachStep();
            const int opponent_step = agent->world().interceptTable()->opponentReachStep();
            const int min_step = std::min ( self_step, std::min( teammate_step, opponent_step ) );
            const Vector2D ball_pos = agent->world().ball().inertiaPoint( min_step );

            if ( ( self_step <= opponent_step
                   || teammate_step < opponent_step + 1
                   || ( teammate_step < opponent_step + 3
                        && ! agent->world().teammatesFromBall().empty()
                        && agent->world().teammatesFromBall().front()->distFromBall() < 2.0 ) )
                 && ball_pos.dist2( ServerParam::i().theirTeamGoalPos() ) < std::pow( 25.0, 2 ) )
            {
                Bhv_SideHalfAttackMove().execute( agent );
            }
            else
            {
                Bhv_SideHalfOffensiveMove().execute( agent );
            }
        }
        break;
    case BA_Cross:
    case BA_ShootChance:
        Bhv_SideHalfAttackMove().execute( agent );
        break;
    default:
        Bhv_BasicMove().execute( agent );
        break;
    }
}

/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
namespace {
/*!

 */
bool
is_block_situation( const WorldModel & wm )
{
    if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.5 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(is_block_situation) no stamina" );
        return false;
    }

    const int self_step = wm.interceptTable()->selfReachStep();
    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( wm.kickableTeammate()
         || opponent_step > teammate_step
         || opponent_step > self_step )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(is_block_situation) our ball" );
        return false;
    }

    const AbstractPlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(is_block_situation) no opponent" );
        return false;
    }

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );

    if ( mark_target
         && mark_target->unum() == opponent->unum() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(is_block_situation) my mark target %d", mark_target->unum() );
        return true;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    if ( std::fabs( opponent_ball_pos.y - home_pos.y ) > 20.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(is_block_situation) too big y difference" );
        return false;
    }

    if ( opponent_ball_pos.x < -35.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(is_block_situation) too back" );
        return false;
    }

    const double self_dist = wm.self().pos().dist( opponent_ball_pos );

    //
    // check other blocker
    //
    for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(), end = wm.teammates().end();
          t != end;
          ++t )
    {
        if ( (*t)->goalie() ) continue;
        if ( (*t)->isTackling() ) continue;
        if ( (*t)->unum() == Unum_Unknown ) continue;

        const AbstractPlayerObject * target = MarkAnalyzer::i().getTargetOf( (*t)->unum() );
        if ( target )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (is_block_situation) %d has mark target %d",
                          (*t)->unum(), target->unum() );
            continue;
        }

        if ( (*t)->pos().dist( opponent_ball_pos ) < self_dist - 3.0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (is_block_situation) exist other blocker",
                          opponent->unum(), (*t)->unum() );
            return false;
        }
    }

    if ( mark_target
         && std::fabs( opponent_ball_pos.y - home_pos.y ) > 10.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (is_block_situation) block mode, but far target" );
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
exist_other_blocker( const WorldModel & wm )
{
    const AbstractPlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(exist_other_blocker) no opponent" );
        return false;
    }

    const AbstractPlayerObject * other_marker = MarkAnalyzer::i().getTargetOf( opponent->unum() );
    if ( other_marker
         && other_marker->unum() != wm.self().unum() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(exist_other_blocker) exist other marker %d", other_marker->unum() );
        return true;
    }

    const Segment2D goal_line( opponent->pos(), ServerParam::i().ourTeamGoalPos() );
    const Vector2D goal_l( -ServerParam::i().pitchHalfLength(), -ServerParam::i().goalHalfWidth() );
    const Vector2D goal_r( -ServerParam::i().pitchHalfLength(), +ServerParam::i().goalHalfWidth() );
    const AngleDeg goal_l_angle = ( goal_l - opponent->pos() ).th();
    const AngleDeg goal_r_angle = ( goal_r - opponent->pos() ).th();

    for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(), end = wm.teammates().end();
          t != end;
          ++t )
    {
        if ( (*t)->goalie() ) continue;
        if ( (*t)->posCount() >= 3 ) continue;
        if ( (*t)->pos().x > opponent->pos().x ) continue;

        const AngleDeg angle_from_opponent = ( (*t)->pos() - opponent->pos() ).th();
        if ( angle_from_opponent.isWithin( goal_r_angle, goal_l_angle )
             || goal_line.dist( (*t)->pos() ) < (*t)->playerTypePtr()->kickableArea() + 0.5 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (do_get_ball_2013) exist blocker %d", (*t)->unum() );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
do_support_other_blocker( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int self_step = wm.interceptTable()->selfReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( self_step < opponent_step + 3 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(do_support_other_blocker) intercept: support other blocker" );
        agent->debugClient().addMessage( "SH:GetBall:Intercept1" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }

    const AbstractPlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    if ( ! opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(do_support_other_blocker) no opponent" );
        return false;
    }

    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    Vector2D target_point = opponent_ball_pos;

    if ( opponent->bodyCount() <= 1 )
    {
        target_point += Vector2D::from_polar( 1.0, opponent->body() );
    }
    else if ( wm.kickableOpponent() )
    {
        target_point = opponent->pos() + opponent->vel();
    }
    else
    {
        target_point.x -= 1.0;
    }

    agent->debugClient().setTarget( target_point );

    const double dist_tolerance = 0.5; // 3.0;
    const double dir_tolerance = 20.0;

    if ( Body_GoToPoint( target_point, dist_tolerance, ServerParam::i().maxDashPower(),
                         -1.0, 100, true, dir_tolerance,
                         0.5 // omni_dash_dist_thr
                         ).execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(do_support_other_blocker) support move" );
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_tolerance );
        agent->debugClient().addMessage( "SH:GetBall:SupportMove" );
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(do_support_other_blocker) support move intercept" );
        agent->debugClient().addMessage( "SH:GetBall:SupportIntercept" );
        if ( ! Body_GoToPoint( opponent_ball_pos, 0.5, ServerParam::i().maxDashPower(), -1.0, 100, true, 15.0 ).execute( agent ) )
        {
            Body_Intercept().execute( agent );
        }
    }

    agent->setNeckAction( new Neck_CheckBallOwner() );

    return true;
}
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleSideHalf::do_get_ball_2013( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    //
    // check if block situation or not
    //

    if ( ! is_block_situation( wm ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (do_get_ball_2013) false: no block mode" );
        return false;
    }

    //
    // support other blocker
    //

    if ( exist_other_blocker( wm ) )
    {
        if ( do_support_other_blocker( agent ) )
        {
            return true;
        }
    }

    //
    // force intercept
    //
    if ( ! wm.kickableOpponent()
         && wm.interceptTable()->selfReachStep() <= 2 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (do_get_ball_2013) force intercept" );
        agent->debugClient().addMessage( "SH:GetBall:ForceIntercept1" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }

    //
    // get ball
    //
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    Rect2D bounding_rect( Vector2D( home_pos.x - 30.0, home_pos.y - 30.0 ),
                          Vector2D( home_pos.x + 6.0, home_pos.y + 30.0 ) );

    if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (do_get_ball_2013) block mark target" );
        agent->debugClient().addMessage( "SH:GetBall" );
        return true;
    }


    //
    // block
    //
    if ( Bhv_BlockBallOwner( new Rect2D( bounding_rect ) ).execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (do_get_ball_2013) block ball owner" );
        agent->debugClient().addMessage( "OH:Off:Block" );
        return true;
    }

    //
    // force move
    //
    const int opponent_step = wm.interceptTable()->opponentReachStep();
    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    Vector2D force_move_target( -45.0, 0.0 );
    if ( opponent_ball_pos.absY() > 25.0 ) force_move_target.y = 24.0;
    else if ( opponent_ball_pos.absY() > 20.0 ) force_move_target.y = 19.0;
    else if ( opponent_ball_pos.absY() > 15.0 ) force_move_target.y = 14.0;
    else force_move_target.y = 10.0;

    if ( Strategy::i().getPositionType( wm.self().unum() ) == Position_Left )
    {
        force_move_target.y *= -1.0;
    }

    const double dist_tolerance = 3.0;
    const double dir_tolerance = 20.0;

    if ( Body_GoToPoint( force_move_target, dist_tolerance, ServerParam::i().maxDashPower(),
                         -1.0, 100, true, dir_tolerance ).execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (do_get_ball_2013) force move" );
        agent->debugClient().setTarget( force_move_target );
        agent->debugClient().addCircle( force_move_target, dist_tolerance );
        agent->debugClient().addMessage( "SH:GetBall:ForceMove" );
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (do_get_ball_2013) force intercept" );
        agent->debugClient().addMessage( "SH:GetBall:ForceIntercept" );
        Body_Intercept().execute( agent );
    }

    agent->setNeckAction( new Neck_CheckBallOwner() );

    return true;
}
