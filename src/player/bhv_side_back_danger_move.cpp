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

#include "bhv_side_back_danger_move.h"

#include "strategy.h"
#include "field_analyzer.h"
#include "mark_analyzer.h"

//#include "bhv_get_ball.h"
#include "bhv_side_back_block_ball_owner.h"
#include "bhv_side_back_mark_move.h"
#include "bhv_tackle_intercept.h"
#include "neck_check_ball_owner.h"
#include "neck_default_intercept_neck.h"
#include "neck_scan_opponent.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/bhv_go_to_point_look_ball.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDangerMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_SideBackDangerMove" );

    //
    // intercept
    //
    if ( doIntercept( agent ) )
    {
        return true;
    }


    if ( doTackleIntercept( agent ) )
    {
        return true;
    }

    //
    // block shoot
    //
    // if ( doBlockShoot( agent ) )
    // {
    //     return true;
    // }

    //
    // get ball
    //
    if ( doGetBall( agent ) )
    {
        return true;
    }

    //
    // mark
    //
    if ( doMarkMove( agent ) )
    {
        return true;
    }

    //
    // block goal
    //
    if ( doBlockGoal( agent ) )
    {
        return true;
    }

    doNormalMove( agent );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDangerMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.kickableTeammate() )
    {
        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    const InterceptInfo info = Body_Intercept::get_best_intercept( wm, true );
    if ( ! info.isValid() )
    {
        return false;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__"(doIntercept) 1st intercept turn=%d dash=%d",
                  info.turnStep(), info.dashStep() );

    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opp_min );
    const Vector2D self_ball_pos = wm.ball().inertiaPoint( self_min );

#if 1
    // 2013-06-02
    if ( opp_min == 1
         && self_min >= 2
         && opponent_ball_pos.dist( self_ball_pos ) > wm.self().playerType().kickableArea() - 0.15 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__" (doIntercept) false (1)" );
        return false;
    }
#endif

    {
        const PlayerObject * teammate = wm.interceptTable()->fastestTeammate();
        if ( teammate
             && teammate->goalie() )
        {
            mate_min = wm.interceptTable()->secondTeammateReachCycle();
        }
    }

    bool intercept = false;


    if ( self_min <= opp_min
         && self_min <= mate_min )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__" (doIntercept) true (1)" );
        intercept = true;
    }

    const bool safe_trap = ( self_ball_pos.dist2( ServerParam::i().ourTeamGoalPos() )
                             > wm.self().pos().dist2( ServerParam::i().ourTeamGoalPos() ) );

    if ( ! intercept
         && safe_trap
         && ! wm.kickableOpponent()
         && opp_min > 0
         && self_min <= opp_min + 3
         && self_min <= mate_min + 5
         && wm.ball().vel().r() * std::pow( ServerParam::i().ballDecay(), opp_min ) < 0.5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__" (doIntercept) true (2)" );
        intercept = true;
    }

    if ( ! intercept
         && safe_trap
         && self_min <= opp_min + 3
         && self_min <= mate_min + 5 )
    {
        const PlayerObject * fastest_opponent = wm.interceptTable()->fastestOpponent();
        if ( fastest_opponent )
        {
            Vector2D opp_inertia = fastest_opponent->inertiaFinalPoint();

            Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );
            Vector2D goal_l( - ServerParam::i().pitchHalfLength(),
                             - ServerParam::i().goalHalfWidth() );
            Vector2D goal_r( - ServerParam::i().pitchHalfLength(),
                             + ServerParam::i().goalHalfWidth() );
            AngleDeg goal_l_angle = ( goal_l - opp_trap_pos ).th();
            AngleDeg goal_r_angle = ( goal_r - opp_trap_pos ).th();

            Vector2D my_inertia = wm.self().inertiaPoint( self_min );
            AngleDeg my_angle = ( my_inertia - opp_trap_pos ).th();

            if ( my_angle.isLeftOf( goal_l_angle )
                 && my_angle.isRightOf( goal_r_angle ) )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__" (doIntercept) true (3) " );
                intercept = true;
            }

            if ( ! intercept
                 && my_inertia.dist2( opp_trap_pos ) < opp_inertia.dist2( opp_trap_pos ) )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__" (doIntercept) true (4) " );
                intercept = true;
            }
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__" (doIntercept) true (5) " );
            intercept = true;
        }
    }

#if 1
    if ( ! intercept
         && self_min >= 15
         && self_min <= opp_min + 3
         && self_min <= mate_min )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__" (doIntercept) true (6) " );
        intercept = true;
    }
#endif

#if 1
    // 2012-06-14
    if ( ! intercept
         && self_min <= opp_min + 2
         && self_min <= mate_min - 5
         && info.turnStep() == 0 // 2013-06-09
         && self_ball_pos.x < wm.self().pos().x + 1.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__" (doIntercept) true (7) " );
        intercept = true;
    }
#endif


//     if ( ! intercept
//          && 2 <= opp_min && opp_min <= 6
//          && self_min <= opp_min + 3
//          && self_min <= mate_min + 5 )
//     {
//         intercept = true;
//     }

//     if ( ! intercept
//          && 2 <= opp_min
//          && self_min <= std::min( opp_min * 1.5, opp_min + 6.0 )
//          && self_min <= std::min( mate_min * 1.5, mate_min + 6.0 ) )
//     {
//         intercept = true;
//     }

    if ( intercept )
    {
        agent->debugClient().addMessage( "SB:Danger:Intercept" );
        Body_Intercept().execute( agent );

        if ( opp_min >= self_min + 3 )
        {
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        }
        else
        {
            agent->setNeckAction( new Neck_DefaultInterceptNeck
                                  ( new Neck_TurnToBallOrScan( 0 ) ) );
        }

        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDangerMove::doTackleIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( FieldAnalyzer::is_ball_moving_to_our_goal( wm )
         || wm.ball().pos().dist2( ServerParam::i().ourTeamGoalPos() ) < std::pow( 15.0, 2 ) )
    {
        if ( Bhv_TackleIntercept().execute( agent ) )
        {
            agent->debugClient().addMessage( "SB:TackleIntercept" );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDangerMove::doBlockShoot( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const double dist_thr = 0.6;

    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    if ( ! opponent )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doBlockShoot) no opponent" );
        return false;
    }

    //int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    if ( opp_min <= 1
         && opponent->bodyCount() <= 1 )
    {
        opp_trap_pos += Vector2D::polar2vector( 0.7, opponent->body() );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doBlockShoot) adjust oppoinent trap pos to (%.2f %.2f)",
                      opp_trap_pos.x, opp_trap_pos.y );
    }

    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    if ( ( wm.kickableTeammate()
           || mate_min <= 1 )
         && mate_min <= opp_min + 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doBlockShoot) no shoot block situation (1)" );
        return false;
    }

    if ( opp_trap_pos.absY() > 15.0
         // || ( position_type == Position_Left
         //      && opp_trap_pos.y > -4.0 )
         // || ( position_type == Position_Right
         //      && opp_trap_pos.y < 4.0 )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doBlockShoot) no shoot block situation (2) opp_trap=(%.1f %.1f)",
                      opp_trap_pos.x, opp_trap_pos.y );
        return false;
    }

    if ( opp_trap_pos.x < - ServerParam::i().pitchHalfLength() + ServerParam::i().catchableArea() * 2.0
         && opp_trap_pos.absY() > ServerParam::i().goalHalfWidth() + 3.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doBlockShoot) goalie should block the ball.  opp_trap=(%.1f %.1f)",
                      opp_trap_pos.x, opp_trap_pos.y );
        return false;
    }

    Vector2D goal_point( - ServerParam::i().pitchHalfLength(),
                         - ServerParam::i().goalHalfWidth() + 1.0 );
    if ( position_type == Position_Right )
    {
        goal_point.y *= -1.0;
    }

    agent->debugClient().addLine( opp_trap_pos, goal_point );

    //
    // deciede the best point
    //

    Vector2D best_point = goal_point;
    int best_cycle = 1000;

    const Vector2D unit_vec = ( goal_point - opp_trap_pos ).setLengthVector( 1.0 );

    double min_dist = 3.0;
    double shoot_len = opp_trap_pos.dist( goal_point );

    dlog.addText( Logger::ROLE,
                  __FILE__": (doBlockShoot) shoot_len=%.2f",
                  shoot_len );
    {
        Line2D shoot_line( opp_trap_pos, goal_point );
        Vector2D self_end_pos = wm.self().inertiaFinalPoint();
        bool self_on_shoot_line = ( shoot_line.dist( self_end_pos ) < wm.self().playerType().kickableArea() * 0.7
                                    && self_end_pos.x < opp_trap_pos.x );
        if ( self_on_shoot_line )
        {
            best_point = shoot_line.projection( self_end_pos );
            shoot_len = std::max( 1.0, opp_trap_pos.dist( best_point ) - 1.0 );
            dlog.addText( Logger::ROLE,
                          __FILE__": (doBlockShoot) self on shoot line. shoot_len=%.2f",
                          shoot_len );
            min_dist = 0.5;
//             dlog.addText( Logger::BLOCK,
//                           __FILE__": (doBlockShoot) self on shoot line. cancel",
//                           shoot_len );
//             return false;
        }
    }

    const double dist_step = std::max( 1.0, shoot_len / 12.0 );
    for ( double d = min_dist; d <= shoot_len; d += dist_step )
    {
        Vector2D target_pos = opp_trap_pos + unit_vec * d;
        int self_step = FieldAnalyzer::predict_self_reach_cycle( wm, target_pos, dist_thr, 0, true, NULL );
        if ( self_step < best_cycle )
        {
            best_point = target_pos;
            best_cycle = self_step;
        }
    }

    //
    // perform the action
    //

    agent->debugClient().setTarget( best_point );
    agent->debugClient().addCircle( best_point, dist_thr );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doBlockShoot) block_point=(%.1f %.1f) cycle=%d",
                  best_point.x, best_point.y,
                  best_cycle );
    double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );

    if ( Body_GoToPoint( best_point,
                         dist_thr,
                         dash_power,
                         -1.0, // dash speed
                         5, // cycle
                         true, // save recovery
                         15.0 // dir thr
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SB:Danger:BlockShootGo" );
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockShoot) turn." );
        agent->debugClient().addMessage( "SB:Danger:BlockShootTurn" );
        Body_TurnToPoint( opp_trap_pos ).execute( agent );
    }


    //agent->setNeckAction( new Neck_CheckBallOwner() );
    doTurnNeck( agent );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDangerMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * fastest_opp = wm.interceptTable()->fastestOpponent();
    if ( ! fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no opponent" );
        return false;
    }

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );
    if ( mark_target
         && mark_target != fastest_opp )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) mark target is not the fastest player." );
        return false;
    }

    int teammate_step = wm.interceptTable()->teammateReachStep();
    int opponent_step = wm.interceptTable()->opponentReachStep();
    Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    if ( wm.kickableTeammate()
         || teammate_step <= opponent_step - 1
         || opponent_ball_pos.x > -26.0
         || ( opponent_ball_pos.dist2( home_pos ) > std::pow( 10.0, 2 )
              && opponent_ball_pos.absY() > 15.0 )
        )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no get ball situation" );
        return false;
    }

    double min_y = home_pos.y - 8.0;
    double max_y = home_pos.y + 8.0;

    Rect2D bounding_rect( Vector2D( -60.0, min_y ),
                          Vector2D( home_pos.x + 10.0, max_y ) );

    Vector2D center_pos = Vector2D::INVALIDATED;

    // 2013-06-01
    if ( opponent_ball_pos.absY() < ServerParam::i().goalHalfWidth() )
    {
        const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

        if ( position_type == Position_Left )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doGetBall) front of goal. block left side" );
            center_pos.assign( -ServerParam::i().pitchHalfLength(),
                               -ServerParam::i().goalHalfWidth() + 1.5 );
        }

        if ( position_type == Position_Right )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (doGetBall) front of goal. block right side" );
            center_pos.assign( -ServerParam::i().pitchHalfLength(),
                               +ServerParam::i().goalHalfWidth() - 1.5 );
        }
    }
    else
    {
        const AbstractPlayerObject * goalie = wm.getOurGoalie();
        if ( goalie )
        {
            const Vector2D goalie_home = Strategy::i().getPosition( wm.ourGoalieUnum() );
            const double goalie_move_dist = goalie->pos().dist( goalie_home ) - 1.0;
            const int goalie_move_step = goalie->playerTypePtr()->cyclesToReachDistance( goalie_move_dist );

            // if ( goalie->pos().dist( goalie_home )
            //      > 1.0 + goalie->posCount() * goalie->playerTypePtr()->realSpeedMax() * 0.7 )
            if ( goalie_move_step - goalie->posCount() > opponent_step + 2 )
            {
                const Vector2D goal_l( -ServerParam::i().pitchHalfLength(), -ServerParam::i().goalHalfWidth() );
                const Vector2D goal_r( -ServerParam::i().pitchHalfLength(), +ServerParam::i().goalHalfWidth() );
                AngleDeg goal_l_angle = ( goal_l - opponent_ball_pos ).th();
                AngleDeg goal_r_angle = ( goal_r - opponent_ball_pos ).th();
                AngleDeg mid_angle = AngleDeg::bisect( goal_r_angle, goal_l_angle );
                AngleDeg goalie_angle = ( goalie->pos() - opponent_ball_pos ).th();

                const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

                dlog.addText( Logger::ROLE,
                              __FILE__": (doGetBall) check goalie angle. mid=%.1f goalie=%.1f",
                              mid_angle.degree(), goalie_angle.degree() );

                if ( position_type == Position_Left
                     && opponent_ball_pos.y < 0.0
                     && goalie_angle.isLeftOf( mid_angle ) )
                {
                    dlog.addText( Logger::ROLE,
                                  __FILE__": (doGetBall) block left side" );
                    center_pos.assign( -ServerParam::i().pitchHalfLength(),
                                       -ServerParam::i().goalHalfWidth() + 1.5 );
                }

                if ( position_type == Position_Right
                     && opponent_ball_pos.y > 0.0
                     && goalie_angle.isRightOf( mid_angle ) )
                {
                    dlog.addText( Logger::ROLE,
                                  __FILE__": (doGetBall) block right side" );
                    center_pos.assign( -ServerParam::i().pitchHalfLength(),
                                       +ServerParam::i().goalHalfWidth() - 1.5 );
                }
            }
            else
            {
                dlog.addText( Logger::ROLE,
                              __FILE__": (doGetBall) goalie exists at home position" );
            }
        }
    }

    if ( Bhv_SideBackBlockBallOwner( bounding_rect, center_pos ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SB:Danger:GetBall" );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) performed" );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGetBall) could not find the position" );

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDangerMove::doMarkMove( PlayerAgent * agent )
{
    return Bhv_SideBackMarkMove().execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideBackDangerMove::doBlockGoal( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();
    Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );

    if ( opp_min > 10
         && opp_trap_pos.x > -40.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockGoal) no dangerous opponent" );
        return false;
    }

    if ( mate_min <= opp_min - 3
         || ( wm.kickableTeammate()
              && opp_min >= 2 )
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockGoal) our ball" );
        return false;
    }

    if ( wm.kickableOpponent()
         || opp_min == 0 )
    {
        const PlayerObject * fastest_opponent = wm.interceptTable()->fastestOpponent();
        if ( fastest_opponent )
        {
            opp_trap_pos = fastest_opponent->inertiaFinalPoint();
        }
    }

    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

#if 0
    if ( opp_trap_pos.absY() > ServerParam::i().goalHalfWidth()
         && ( ( position_type == Position_Left && wm.ball().pos().y > 0.0 )
              || ( position_type == Position_Right && wm.ball().pos().y < 0.0 ) )
         )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockGoal) no block goal situation" );
        return false;
    }
#endif

    //--------------------------------------------------
    // block opposite side goal
#if 0
    Vector2D goal_post( - ServerParam::i().pitchHalfLength(),
                        ServerParam::i().goalHalfWidth() - 0.8 );
    if ( position_type == Position_Left )
    {
        goal_post.y *= -1.0;
    }

    Line2D block_line( wm.ball().pos(), goal_post );
    Vector2D block_point( -48.0, 0.0 );
    block_point.y = block_line.getY( block_point.x );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doBlockGoal) block goal post. original_y = %.1f",
                  block_point.y );
    block_point.y = bound( home_pos.y - 1.0,
                           block_point.y,
                           home_pos.y + 1.0 );
#else
    Vector2D block_point( -48.0, 0.0 );
    if ( opp_trap_pos.x > -46.0 )
    {
        Vector2D goal_post( - ServerParam::i().pitchHalfLength(),
                            ServerParam::i().goalHalfWidth() );
        if ( position_type == Position_Left )
        {
            goal_post.y *= -1.0;
        }

        Line2D block_line( opp_trap_pos, goal_post );
        block_point.y = block_line.getY( block_point.x );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockGoal) block point(1). original=(%.3f %.3f)",
                      block_point.x, block_point.y );

        block_point.y += ( goal_post.y < 0.0
                           ? + wm.self().playerType().kickableArea()
                           : - wm.self().playerType().kickableArea() );
    }
    else
    {
        Vector2D goal_post( - ServerParam::i().pitchHalfLength(),
                            ServerParam::i().goalHalfWidth() );
        if ( position_type == Position_Left )
        {
            goal_post.y *= -1.0;
        }

        double len = std::min( 4.5, opp_trap_pos.dist( goal_post ) * 0.5 );
        block_point = goal_post + ( opp_trap_pos - goal_post ).setLengthVector( len );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockGoal) block point(2). original=(%.3f %.3f)",
                      block_point.x, block_point.y );

        AngleDeg rel_angle = ( opp_trap_pos - goal_post ).th();
        block_point -= goal_post;
        block_point.rotate( -rel_angle );
        block_point.y += ( position_type == Position_Left
                           ? + wm.self().playerType().kickableArea()
                           : - wm.self().playerType().kickableArea() );
        block_point.rotate( rel_angle );
        block_point += goal_post;
    }
#endif


    dlog.addText( Logger::ROLE,
                  __FILE__": (doBlockGoal) block point (%.3f %.3f)",
                  block_point.x, block_point.y );
    agent->debugClient().setTarget( block_point );
    agent->debugClient().addCircle( block_point, 1.0 );

    double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );
    double dist_thr = std::max( 0.4, wm.self().pos().dist( opp_trap_pos ) * 0.1 );



    Vector2D face_point( wm.self().pos().x, 0.0 );
    if ( wm.self().pos().y * wm.ball().pos().y < 0.0 )
    {
        face_point.assign( -52.5, 0.0 );
    }

    if ( Body_GoToPoint( block_point,
                         dist_thr,
                         dash_power,
                         -1.0, // dash speed
                         1, // cycle
                         true, // save recovery
                         15.0 // dir thr
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SB:Danger:BlockGoal:Go%.0f",
                                         dash_power );
    }
    else
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doBlockGoal). already block goal(2)" );
        agent->debugClient().addMessage( "SB:Danger:BlockGoal:TurnTo(2)" );
        Body_TurnToPoint( face_point ).execute( agent );
    }

    //agent->setNeckAction( new Neck_CheckBallOwner() );
    doTurnNeck( agent );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideBackDangerMove::doNormalMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    Vector2D next_self_pos = wm.self().pos() + wm.self().vel();
    Vector2D trap_pos = wm.ball().inertiaPoint( std::min( wm.interceptTable()->teammateReachCycle(),
                                                          wm.interceptTable()->opponentReachCycle() ) );

    double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );
    if ( next_self_pos.x < trap_pos.x )
    {
        // behind of trap point
        dash_power *= std::min( 1.0, 7.0 / wm.ball().distFromSelf() );
        dash_power = std::max( 30.0, dash_power );
    }

    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().setTarget( home_pos );
    agent->debugClient().addCircle( home_pos, dist_thr );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doNormalMove) go to home (%.1f %.1f)  dist_thr=%.3f  dash_power=%.1f",
                  home_pos.x, home_pos.y,
                  dist_thr,
                  dash_power );

    if ( Body_GoToPoint( home_pos, dist_thr, dash_power,
                         -1.0, // dash speed
                         1,
                         true,
                         12.0
                         ).execute( agent )  )
    {
        agent->debugClient().addMessage( "SB:Danger:GoHome%.0f", dash_power );
    }
    else
    {
        Body_TurnToBall().execute( agent );
        agent->debugClient().addMessage( "SB:Danger:TurnToBall" );
    }

    // agent->setNeckAction( new Neck_CheckBallOwner() );
    doTurnNeck( agent );
}

/*-------------------------------------------------------------------*/
/*!
  TODO: implement as new NeckAction
 */
void
Bhv_SideBackDangerMove::doTurnNeck( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const Vector2D ball_next = agent->effector().queuedNextBallPos();
    const Vector2D my_next = agent->effector().queuedNextSelfPos();
    const AngleDeg my_next_body = agent->effector().queuedNextSelfBody();
    const double next_view_width = agent->effector().queuedNextViewWidth().width();

    if ( wm.ball().posCount() >= 2 )
    {
        if ( wm.ball().posCount() <= 0
             && ! wm.kickableOpponent()
             && ! wm.kickableTeammate()
             && my_next.dist( ball_next ) < SP.visibleDistance() - 0.2 )
        {
            // in visible distance
        }
        else if  ( ( ( ball_next - my_next ).th() - my_next_body ).abs()
                   > SP.maxNeckAngle() + next_view_width * 0.5 + 2.0 )
        {
            // never face to ball
        }
        else
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTurnNeck) turn neck to ball" );
            agent->debugClient().addMessage( "CheckBall" );
            agent->setNeckAction( new Neck_TurnToBall() );
            return;
        }
    }

    const AbstractPlayerObject * first_opponent = wm.interceptTable()->fastestOpponent();
    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );
    if ( first_opponent
         && mark_target
         && mark_target == first_opponent )
    {
        if ( mark_target->posCount() > 0 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doTurnNeck) neck check ball owner and mark target" );
            agent->debugClient().addMessage( "Check1stOpp" );
            agent->setNeckAction( new Neck_TurnToPlayerOrScan( first_opponent, 0 ) );
            return;
        }
    }

    if ( mark_target
         && mark_target->posCount() > 1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doTurnNeck) neck check mark target" );
            agent->debugClient().addMessage( "CheckMarkTarget" );
        agent->setNeckAction( new Neck_TurnToPlayerOrScan( mark_target, 0 ) );
        return;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doTurnNeck) scan opponent or check ball owner" );

    const double max_x = SP.ourPenaltyAreaLineX() + 5.0;
    boost::shared_ptr< Region2D > region( new Rect2D( Vector2D( - SP.pitchHalfLength(),
                                                                - SP.pitchHalfWidth() ),
                                                      Vector2D( max_x,
                                                                + SP.pitchHalfWidth() ) ) );
    NeckAction::Ptr neck( new Neck_CheckBallOwner() );
    agent->setNeckAction( new Neck_ScanOpponent( region, neck ) );
}
