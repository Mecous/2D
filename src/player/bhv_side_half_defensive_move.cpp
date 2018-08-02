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

#include "bhv_side_half_defensive_move.h"

#include "strategy.h"
#include "defense_system.h"
#include "mark_analyzer.h"

#include "role_side_half.h"

#include "bhv_basic_move.h"
#include "bhv_attacker_offensive_move.h"
#include "bhv_mid_fielder_mark_move.h"
#include "bhv_block_ball_owner.h"
#include "bhv_get_ball.h"
#include "neck_check_ball_owner.h"
#include "neck_offensive_intercept_neck.h"
#include "neck_default_intercept_neck.h"

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
Bhv_SideHalfDefensiveMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_SideHalfDefensiveMove" );

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

    if ( doGetBallMarkTarget( agent ) )
    {
        return true;
    }

    //
    // mark
    //
    if ( doMark( agent ) )
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
Bhv_SideHalfDefensiveMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ! wm.kickableTeammate()
         && self_min <= mate_min
         && self_min <= opp_min + 1 )
    {
        agent->debugClient().addMessage( "SH:Def:Intercept" );
        dlog.addText( Logger::ROLE,
                      __FILE__": intercept" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        return true;
    }

    return false;
}

namespace {
/*-------------------------------------------------------------------*/
/*!

 */
bool
do_get_ball_2012( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.5 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) no stamina" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    const Vector2D opp_trap_pos = wm.ball().inertiaPoint( opp_min );


    if ( opp_trap_pos.x < -35.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) too back" );
        return false;
    }

    double dist_to_trap_pos = opp_trap_pos.dist( home_pos );

    if ( ! wm.kickableTeammate()
         && opp_min < mate_min
         && opp_min < self_min
         && ( dist_to_trap_pos < 10.0
#if 1
              || ( position_type == Position_Left
                   && opp_trap_pos.y < home_pos.y
                   && opp_trap_pos.y > home_pos.y - 7.0
                   && opp_trap_pos.x < home_pos.x + 5.0 )
              || ( position_type == Position_Right
                   && opp_trap_pos.y > home_pos.y
                   && opp_trap_pos.y < home_pos.y + 7.0
                   && opp_trap_pos.x < home_pos.x + 5.0 )
#endif
              )
         )
    {

        if ( ! wm.kickableTeammate()
             && self_min <= 2 )
        {
            Vector2D my_final = wm.self().inertiaFinalPoint();
            if ( ( my_final - opp_trap_pos ).th().abs() > 140.0  )
            {
                agent->debugClient().addMessage( "SH:GetBall:Intercept" );
                dlog.addText( Logger::ROLE,
                              __FILE__": intercept" );
                Body_Intercept().execute( agent );
                agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
                return true;
            }
        }

        // 2012-05-21: check other blocker
        bool exist_other_blocker = false;
        {
            const Vector2D opponent_pos = DefenseSystem::get_block_opponent_trap_point( wm );
            const Vector2D center_pos = DefenseSystem::get_block_center_point( wm );
            const AngleDeg block_angle = ( center_pos - opponent_pos ).th();
            // const Segment2D block_segment( opponent_pos,
            //                                opponent_pos
            //                                + ( center_pos - opponent_pos ).setLengthVector( 5.0 ) );
            const Sector2D block_area( opponent_pos, 1.0, 15.0, block_angle - 20.0, block_angle + 20.0 );

            for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromBall().begin(), end = wm.teammatesFromBall().end();
                  t != end;
                  ++t )
            {
                if ( (*t)->goalie() ) continue;
                if ( (*t)->isTackling() ) continue;

                if ( block_area.contains( (*t)->pos() )
                     // || ( block_segment.contains( (*t)->pos() )
                     //      && block_segment.dist( (*t)->pos() ) < 1.5 )
                     )
                {
                    exist_other_blocker = true;
                    dlog.addText( Logger::ROLE,
                                  __FILE__": (doGetBall) exist other blocker %d (%.1f %.1f)",
                                  (*t)->unum(), (*t)->pos().x, (*t)->pos().y );
                    break;
                }
            }
        }

        Rect2D bounding_rect( Vector2D( home_pos.x - 30.0, home_pos.y - 15.0 ),
                              Vector2D( home_pos.x + 6.0, home_pos.y + 15.0 ) );
        dlog.addText( Logger::ROLE,
                      __FILE__": (doGetBall) rect=(%.1f %.1f)(%.1f %.1f)",
                      bounding_rect.left(), bounding_rect.top(),
                      bounding_rect.right(), bounding_rect.bottom() );

        if ( ! exist_other_blocker )
        {
            if ( opp_trap_pos.x > -36.0 )
            {
                if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
                {
                    agent->debugClient().addMessage( "SH:GetBall" );
                    return true;
                }
            }
        }

#if 1
        // 2009-07-03
        if ( self_min < 16
             && opp_min <= mate_min )
        {
            Vector2D self_trap_pos = wm.ball().inertiaPoint( self_min );
            bool enough_stamina = true;
            double estimated_consume
                = wm.self().playerType().getOneStepStaminaComsumption()
                * self_min;
            if ( wm.self().stamina() - estimated_consume < ServerParam::i().recoverDecThrValue() )
            {
                enough_stamina = false;
            }

            if ( enough_stamina
                 && opp_min < 3
                 && ( home_pos.dist( self_trap_pos ) < 10.0
                      || ( home_pos.absY() < self_trap_pos.absY()
                           && home_pos.y * self_trap_pos.y > 0.0 ) // same side
                      || self_trap_pos.x < home_pos.x
                      )
                 )
            {
                agent->debugClient().addMessage( "SH:Def:Get:Intercept(1)" );
                Body_Intercept().execute( agent );
                agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
                return true;
            }
        }


        if ( self_min < 15
             && self_min < mate_min + 2
             && ! wm.kickableTeammate() )
        {
            agent->debugClient().addMessage( "SH:Def:Get:Intercept(2)" );
            Body_Intercept().execute( agent );
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
            return true;
        }
#endif

        if ( Bhv_BlockBallOwner( new Rect2D( bounding_rect )
                                 ).execute( agent ) )
        {
            agent->debugClient().addMessage( "SH:Def:Block" );
            return true;
        }
    }


    return false;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfDefensiveMove::doGetBall( PlayerAgent * agent )
{
    //return do_get_ball_2012( agent );
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_SideHalfDefensiveMove::doGetBall" );
    return RoleSideHalf::do_get_ball_2013( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfDefensiveMove::doGetBallMarkTarget( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.kickableTeammate() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGetBallMarkTarget) exist kickable teammate" );
        return false;
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( mate_min == 0
         || mate_min < opp_min - 1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGetBallMarkTarget) our ball" );
        return false;
    }

    const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );
    if ( ! mark_target )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGetBallMarkTarget) no mark target" );
        return false;
    }

    const AbstractPlayerObject * fastest_opponent = wm.interceptTable()->fastestOpponent();

    if ( mark_target != fastest_opponent )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGetBallMarkTarget) mark_target != fastest opponent" );
        return false;
    }

    if ( self_min <= 3 )
    {
        agent->debugClient().addMessage( "SH:GetBall2:Intercept" );
        dlog.addText( Logger::ROLE,
                      __FILE__":(doGetBallMarkTarget) intercept" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
        return true;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    Rect2D bounding_rect( Vector2D( home_pos.x - 30.0, home_pos.y - 15.0 ),
                          Vector2D( home_pos.x + 6.0, home_pos.y + 15.0 ) );
    dlog.addText( Logger::ROLE,
                  __FILE__": (doGetBall) try GetBall" );

    if ( Bhv_GetBall( bounding_rect ).execute( agent ) )
    {
        agent->debugClient().addMessage( "SH:GetBall2" );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__": (doGetBall) failed" );

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfDefensiveMove::doMark( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int t_step = wm.interceptTable()->teammateReachCycle();
    const int o_step = wm.interceptTable()->opponentReachCycle();

    if ( ( ! wm.kickableOpponent()
           && wm.lastKickerSide() == wm.ourSide()
           && ( t_step <= 1
                || t_step < o_step ) )
         || t_step <= o_step - 2 )
    {
        dlog.addText( Logger::MARK,
                      __FILE__": (doMark) no mark situation." );
        return false;
    }


    return Bhv_MidFielderMarkMove().execute( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SideHalfDefensiveMove::doNormalMove( PlayerAgent * agent )
{

    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    // intercept
    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min < 16
         && opp_min >= self_min - 1 )
    {
        Vector2D self_trap_pos
            = inertia_n_step_point( wm.ball().pos(),
                                    wm.ball().vel(),
                                    self_min,
                                    ServerParam::i().ballDecay() );
        bool enough_stamina = true;
        double estimated_consume = wm.self().playerType().getOneStepStaminaComsumption() * self_min;
        if ( wm.self().stamina() - estimated_consume < ServerParam::i().recoverDecThrValue() )
        {
            enough_stamina = false;
        }

        if ( enough_stamina
             && opp_min < 3
             && ( home_pos.dist( self_trap_pos ) < 10.0
                  || ( home_pos.absY() < self_trap_pos.absY()
                       && home_pos.y * self_trap_pos.y > 0.0 ) // same side
                  || self_trap_pos.x < home_pos.x
                  )
             )
        {
            agent->debugClient().addMessage( "Intercept(1)" );
            Body_Intercept().execute( agent );
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
            return;
        }
    }

    if ( self_min < 15
         && self_min < mate_min + 2
         && ! wm.kickableTeammate() )
    {
        agent->debugClient().addMessage( "Intercept(2)" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        return;
    }

    Bhv_BasicMove().execute( agent );
}
