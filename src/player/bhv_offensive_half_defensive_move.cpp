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

#include "bhv_offensive_half_defensive_move.h"

#include "strategy.h"
#include "defense_system.h"
#include "mark_analyzer.h"

#include "bhv_basic_move.h"
#include "bhv_attacker_offensive_move.h"
#include "bhv_block_ball_owner.h"
#include "bhv_get_ball.h"
#include "bhv_mid_fielder_mark_move.h"
#include "neck_check_ball_owner.h"
#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>

#include <rcsc/action/body_intercept.h>

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfDefensiveMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_OffensiveHalfDefensiveMove" );

    //
    // intercept
    //
    if ( doIntercept( agent ) )
    {
        return true;
    }

    //
    // get ball
    //
    if ( doGetBall( agent ) )
    {
        return true;
    }

    //
    // intercept 2nd try
    //
    // if ( doIntercept2nd( agent ) )
    // {
    //     return true;
    // }

    //
    //
    //
    if ( doMarkMove( agent ) )
    {
        return true;
    }

    //
    // move to space
    //
    // if ( doMoveToSpace( agent ) )
    // {
    //     return true;
    // }

    //
    // normal move
    //
    doNormalMove( agent );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfDefensiveMove::doIntercept( PlayerAgent * agent )
{
    const rcsc::WorldModel & wm = agent->world();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ! wm.kickableTeammate()
         && self_min <= mate_min
         && self_min <= opp_min + 1 )
    {
        agent->debugClient().addMessage( "OH:Def:Intercept" );
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        return true;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(doIntercept) false" );

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfDefensiveMove::doIntercept2nd( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    // intercept
    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min < 16
         && self_min <= opp_min + 1 )
    {
        double consumed_stamina = wm.self().playerType().getOneStepStaminaComsumption() * self_min;
        if ( wm.self().stamina() - consumed_stamina < ServerParam::i().recoverDecThrValue() )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doIntercept2nd) no stamina" );
        }
        else
        {
            const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
            const Vector2D self_trap_pos = wm.ball().inertiaPoint( self_min );

            if( opp_min < 3
                && ( home_pos.dist( self_trap_pos ) < 10.0
                     || ( home_pos.absY() < self_trap_pos.absY()
                          && home_pos.y * self_trap_pos.y > 0.0 ) // same side
                     || self_trap_pos.x < home_pos.x
                     )
                )
            {
                dlog.addText( Logger::ROLE,
                              __FILE__":(doIntercept2nd) intercept(1)" );
                agent->debugClient().addMessage( "OH:Def:Intercept2(1)" );
                Body_Intercept().execute( agent );
                agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
                return true;
            }
        }
    }

    if ( self_min < 15
         && self_min < mate_min + 2
         && ! wm.kickableTeammate() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doIntercept2nd) intercept(2)" );
        agent->debugClient().addMessage( "OH:Def:Intercept2(2)" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doIntercept2nd) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfDefensiveMove::doGetBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    //
    // judge the current situation
    //
    if ( ! DefenseSystem::is_midfielder_block_situation( wm ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGetBall) no block situation" );
        return false;
    }

    //
    // intercept
    //
    const int self_step = wm.interceptTable()->selfReachStep();

    if ( ! wm.kickableOpponent()
         && self_step <= 5 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGetBall) force intercept" );
        agent->debugClient().addMessage( "OH:GetBall:Intercept" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_CheckBallOwner() );
        return true;
    }

    //
    // block
    //

    Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    Rect2D bounding_rect( Vector2D( -50.0, home_pos.y - 25.0 ),
                          Vector2D( home_pos.x + 20.0, home_pos.y + 25.0 ) );
    if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
    {
        agent->debugClient().addMessage( "OH:GetBall" );
        return true;
    }

    //
    // force move
    //
    const int opponent_step = wm.interceptTable()->opponentReachStep();
    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opponent_step );

    Vector2D target_point
        = opponent_ball_pos * 0.7
        + ServerParam::i().ourTeamGoalPos() * 0.3;
    double dist_tolerance = std::max( 0.5, wm.ball().distFromSelf() * 0.1 );
    double dash_power = ServerParam::i().maxPower();
    double dir_tolerance = 25.0;

    dlog.addText( Logger::ROLE,
                  __FILE__":(doGetBall) force move" );

    agent->debugClient().addMessage( "OH:GetBall:ForceMove" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_tolerance );

    if ( ! Body_GoToPoint( target_point, dist_tolerance, dash_power,
                           -1.0, 100, true, // speed, cycke, recovery
                           dir_tolerance ).execute( agent ) )
    {
        Body_TurnToBall().execute( agent );
    }

    agent->setNeckAction( new Neck_CheckBallOwner() );

    return true;
}

namespace {

double
get_congestion( const WorldModel & wm,
                const Vector2D & pos )
{
    double value = 0.0;
    for ( PlayerObject::Cont::const_iterator p = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          p != end;
          ++p )
    {
        if ( (*p)->distFromSelf() > 20.0 ) break;

        value += 1.0 / (*p)->pos().dist2( pos );
    }

    return value;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfDefensiveMove::doMoveToSpace( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.6 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doMoveToSpace) no stamina" );
        return false;
    }

    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( opponent_step < teammate_step )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doMoveToSpace) not our ball" );
        return false;
    }

    Vector2D addvec[2];
    addvec[0] = Vector2D::polar2vector( 1.0, -90.0 );
    addvec[1] = Vector2D::polar2vector( 1.0, +90.0 );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    Vector2D target_point = home_pos;
    double best_congestion = get_congestion( wm, target_point );
    for ( int step = 0; step < 3; ++step )
    {
        Vector2D best_pos = target_point;
        for ( int i = 0; i < 2; ++i )
        {
            Vector2D tmp_pos = target_point + addvec[i];
            double congestion = get_congestion( wm, tmp_pos );
            if ( congestion < best_congestion )
            {
                best_congestion = congestion;
                best_pos = tmp_pos;
                dlog.addText( Logger::ROLE,
                              __FILE__": (doMoveToSpace) loop %d: updated to angle %.1f pos=(%.1f %.1f)",
                              step, 45.0*i, tmp_pos.x, tmp_pos.y );
            }
        }

        if ( best_pos == target_point )
        {
            break;
        }
        target_point = best_pos;
    }

    if ( home_pos == target_point )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doMoveToSpace) target point not updated" );
        return false;
    }


    const double dash_power = Strategy::get_normal_dash_power( wm );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    dlog.addText( Logger::ROLE,
                  __FILE__": OH:DefSpaceMove" );
    agent->debugClient().addMessage( "OH:DefSpaceMove:%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point, dist_thr, dash_power ).execute( agent ) )
    {
        Body_TurnToBall().execute( agent );
    }

    int count_thr = 0;
    if ( wm.self().viewWidth() == ViewWidth::NORMAL )
    {
        count_thr = 1;
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( count_thr ) );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfDefensiveMove::doMarkMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( opponent_step <= teammate_step )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doMark)" );
        if ( Bhv_MidFielderMarkMove().execute( agent ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doMarkMove) done" );
            return true;
        }
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(doMarkMove) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_OffensiveHalfDefensiveMove::doNormalMove( PlayerAgent * agent )
{
    //Bhv_BasicMove().execute( agent );

    const WorldModel & wm = agent->world();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    const Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );

    double dash_power = Strategy::get_normal_dash_power( wm );

    if ( opp_min <= mate_min
         && opp_min <= self_min
         && target_point.x < wm.self().pos().x - 3.0 )
    {
        dash_power = ServerParam::i().maxDashPower();
    }

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    dlog.addText( Logger::TEAM,
                  __FILE__":(doNormalMove) target=(%.1f %.1f) dist_thr=%.2f",
                  target_point.x, target_point.y,
                  dist_thr );

    agent->debugClient().addMessage( "OH:DefNormale%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point, dist_thr, dash_power
                           ).execute( agent ) )
    {
        Body_TurnToBall().execute( agent );
    }

    if ( wm.kickableOpponent()
         && wm.ball().distFromSelf() < 18.0 )
    {
        agent->setNeckAction( new Neck_TurnToBall() );
    }
    else
    {
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    }
}
