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

#include "bhv_attacker_offensive_move.h"

#include "strategy.h"
#include "field_analyzer.h"

#include "bhv_block_ball_owner.h"
#include "neck_offensive_intercept_neck.h"
#include "neck_default_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/soccer_intention.h>
#include <rcsc/player/say_message_builder.h>

#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/

class IntentionAttackerBreakAway
    : public SoccerIntention {
private:
    const Vector2D M_target_point;
    int M_step;
    GameTime M_last_execute_time;

public:

    IntentionAttackerBreakAway( const Vector2D & target_point,
                                const int step,
                                const GameTime & start_time )
        : M_target_point( target_point )
        , M_step( step )
        , M_last_execute_time( start_time )
      { }

    bool finished( const PlayerAgent * agent );

    bool execute( PlayerAgent * agent );

};

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionAttackerBreakAway::finished( const PlayerAgent * agent )
{
    if ( M_step == 0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() empty queue" );
        return true;
    }

    const WorldModel & wm = agent->world();

    if ( wm.audioMemory().passTime() == agent->world().time()
         && ! wm.audioMemory().pass().empty()
         && ( wm.audioMemory().pass().front().receiver_ == wm.self().unum() )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished(). heard pass message." );
        return true;
    }

    if ( M_last_execute_time.cycle() + 1 != wm.time().cycle() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished(). last execute time is illegal" );
        return true;
    }

//     if ( wm.audioMemory().passTime() == wm.time() )
//     {
//         dlog.addText( Logger::TEAM,
//                       __FILE__": finished() heard passt" );
//         return false;
//     }

    if ( wm.ball().pos().x > M_target_point.x + 3.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() ball is over target point" );
        return false;
    }

    //const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( mate_min > 3 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() teammate intercept cycle is big" );
        return true;
    }

    if ( opp_min < mate_min )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() ball owner is opponent" );
        return true;
    }

    const Vector2D mate_trap_pos = wm.ball().inertiaPoint( mate_min );

    const double max_x = std::max( mate_trap_pos.x, wm.offsideLineX() );

    if ( wm.self().pos().x > max_x )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished(). over offside or trap line" );

        return true;
    }

    if ( std::fabs( M_target_point.y -  mate_trap_pos.y ) > 15.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() mate trap pos is too far."
                      " target=(%.1f %.1f) mate_trap=(%.1f %.1f)",
                      M_target_point.x, M_target_point.y,
                      mate_trap_pos.x, mate_trap_pos.y );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionAttackerBreakAway::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    --M_step;
    M_last_execute_time = wm.time();

    double dist_thr = 1.0;

    agent->debugClient().addMessage( "I_BreakAway" );

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();

    if ( self_min <= mate_min + 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": intention execute. intercept" );
        Vector2D face_point( 52.5, wm.self().pos().y * 0.9 );
        Body_Intercept( true, face_point ).execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        return true;
    }


    agent->debugClient().setTarget( M_target_point );
    agent->debugClient().addCircle( M_target_point, dist_thr );

    Vector2D actual_target = M_target_point;

    if ( M_target_point.x > wm.offsideLineX() )
    {
        Line2D target_line( wm.self().inertiaFinalPoint(), M_target_point );
        double target_y = target_line.getY( wm.offsideLineX() - 0.1 );
        if ( target_y != Line2D::ERROR_VALUE )
        {
            actual_target.assign( wm.offsideLineX() - 0.1,
                                  target_y );
        }
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": intention execute. go_to=(%.2f, %.2f) actual_target=(%.2f %.2f) left_step=%d",
                  M_target_point.x, M_target_point.y,
                  actual_target.x, actual_target.y,
                  M_step );

    if ( ! Body_GoToPoint( actual_target, dist_thr,
                           ServerParam::i().maxDashPower(),
                           -1.0, // dash speed
                           3, //100, // cycle
                           true, // stamina save
                           20.0 // angle threshold
                           ).execute( agent ) )
    {
        AngleDeg body_angle( 0.0 );
        Body_TurnToAngle( body_angle ).execute( agent );
    }

    if ( wm.ball().posCount() <= 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": intention execute. scan field" );
        agent->setNeckAction( new Neck_ScanField() );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": intention execute. ball or scan" );
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    }

    if ( M_step <= 1 )
    {
        agent->doPointtoOff();
    }

    agent->debugClient().addMessage( "Say_req" );
    agent->addSayMessage( new PassRequestMessage( M_target_point ) );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_AttackerOffensiveMove::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    dlog.addText( Logger::TEAM,
                  __FILE__": AttackerOffensiveMove. home_pos=(%.2f %.2f)",
                  home_pos.x, home_pos.y );

    //
    // forestall
    //
    if ( doForestall( agent ) )
    {
        return true;
    }

    //
    // intercept
    //
    if ( doIntercept( agent ) )
    {
        return false;
    }

    //------------------------------------------------------

    Vector2D target_point = getTargetPoint( agent );

    bool breakaway = false;
    bool intentional = false;

    // int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const Vector2D mate_trap_pos = wm.ball().inertiaPoint( mate_min );
    const double max_x = std::max( wm.offsideLineX(), mate_trap_pos.x );

    if ( wm.kickableTeammate()
         || mate_min <= opp_min + 1 )
    {
        if ( M_forward_player
             && mate_min <= 2
             // && opp_min >= 3 // 2010-6-18 commented out
             && mate_trap_pos.x > wm.offsideLineX() - 10.0
             && wm.self().pos().x > 5.0
             && wm.self().pos().x < 27.0
             && wm.self().pos().x > max_x - 7.0
             && wm.self().pos().x < max_x - 1.0
             && wm.self().pos().x < mate_trap_pos.x + 10.0
#if 1
             // 2009-06-17
             && std::fabs( mate_trap_pos.y - wm.self().pos().y ) < 15.0
#else
             && ( std::fabs( mate_trap_pos.y - wm.self().pos().y ) < 8.0
                  || ( mate_trap_pos - wm.self().pos() ).th().abs() < 110.0
                  //|| ( mate_trap_pos.x > wm.offsideLineX() - 8.0
                  //     && std::fabs( mate_trap_pos.y - wm.self().pos().y ) < 20.0 )
                  )
#endif
             && wm.self().pos().dist( mate_trap_pos ) < 20.0
             && std::fabs( home_pos.y - wm.self().pos().y ) < 15.0
             && checkStaminaForBreakaway( agent )
             )
        {
            double x_diff = max_x - wm.self().pos().x;
            int dash_step = wm.self().playerType().cyclesToReachDistance( x_diff );
            if ( mate_min < dash_step - 1 )
            {
                target_point.x = std::min( wm.self().pos().x + 20.0, 50.0 );
                target_point.y = wm.self().pos().y * 0.8 + mate_trap_pos.y * 0.2;
                if ( target_point.absY() > 8.0
                     && target_point.absY() > home_pos.absY() )
                {
                    target_point.y = home_pos.y;
                }
                intentional = true;
            }

            dlog.addText( Logger::TEAM,
                          __FILE__": try breakaway=(%.1f %.1f)",
                          target_point.x, target_point.y );
            breakaway = true;
        }
    }

    const double dash_power = ( breakaway
                                ? ServerParam::i().maxDashPower()
                                : getDashPower( agent, target_point ) );

    if ( dash_power < 1.0 )
    {
        agent->debugClient().addMessage( "Attack:Recover" );
        agent->debugClient().setTarget( target_point );
        dlog.addText( Logger::TEAM,
                      __FILE__": execute. turn only" );
        AngleDeg face_angle = wm.ball().angleFromSelf() + 90.0;
        if ( face_angle.abs() > 90.0 ) face_angle += 180.0;
        Body_TurnToAngle( face_angle ).execute( agent );
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
        return true;
    }


    doGoToPoint( agent, target_point, dash_power, intentional );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_AttackerOffensiveMove::doForestall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.6
         && opp_min < 3
         && opp_min < mate_min - 2
         && ( opp_min <= self_min - 5
              || opp_min == 0 )
         && wm.ball().pos().dist( home_pos ) < 10.0
         && wm.ball().distFromSelf() < 15.0 )
    {
        Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opp_min );

        dlog.addText( Logger::TEAM,
                      __FILE__": (doForestall) try. self=%d, mate=%d, opp=%d opp_ball_pos=(%.2f %.2f)",
                      self_min, mate_min, opp_min,
                      opponent_ball_pos.x, opponent_ball_pos.y );

        const AbstractPlayerObject * other_blocker
            = FieldAnalyzer::get_blocker( wm, opponent_ball_pos );
        if ( other_blocker )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (doForestall) other blocker. try intercept" );
            if ( Body_Intercept( true ).execute( agent ) )
            {
                agent->debugClient().addMessage( "Attack:Block:Intercept" );
                agent->setNeckAction( new Neck_TurnToBall() );
                return true;
            }
        }

        Rect2D bounding_rect = Rect2D::from_center( home_pos, 30.0, 30.0 );
        if ( Bhv_BlockBallOwner( new Rect2D( bounding_rect ) ).execute( agent ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (doForestall) block ball owner" );
            //agent->debugClient().addMessage( "Attack" );
            return true;
        }
        dlog.addText( Logger::TEAM,
                      __FILE__": (doForestall) failed" );

        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (doForestall) no block situation" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_AttackerOffensiveMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.kickableTeammate() )
    {
        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    //int opp_min = wm.interceptTable()->opponentReachCycle();

    bool intercept = false;

#if 1
    if ( ( mate_min >= 2 && self_min <= 4 )
         || ( self_min <= mate_min + 1
              && mate_min >= 4 ) )
    {
        intercept = true;
    }
#else
    if ( self_min <= mate_min + 1 )
    {
        intercept = true;
    }

    if ( mate_min <= 10
         && self_min > mate_min )
    {
        Vector2D my_final = wm.self().inertiaFinalPoint();
        Vector2D trap_pos = wm.ball().inertiaPoint( mate_min );
        if ( trap_pos.x < my_final.x )
        {
            intercept = false;

            dlog.addText( Logger::TEAM,
                          __FILE__": (doIntercept) cancel. my_final=(%.2f %.2f) trap_pos=(%.2f %.1f)",
                          my_final.x, my_final.y,
                          trap_pos.x, trap_pos.y );
        }
    }
#endif

    if ( intercept )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doIntercept) performed" );
        agent->debugClient().addMessage( "Attack:Intercept" );
        Vector2D face_point( 52.5, wm.self().pos().y );
        if ( wm.self().pos().absY() < 10.0 )
        {
            face_point.y *= 0.8;
        }
        else if ( wm.self().pos().absY() < 20.0 )
        {
            face_point.y *= 0.9;
        }

        Body_Intercept( true, face_point ).execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_AttackerOffensiveMove::doGoToPoint( PlayerAgent * agent,
                                        const Vector2D & target_point,
                                        const double & dash_power,
                                        const bool intentional )
{
    const WorldModel & wm = agent->world();

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const Vector2D mate_trap_pos = wm.ball().inertiaPoint( mate_min );

    agent->debugClient().addMessage( "Attack:Go%.0f", dash_power );

    double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.2 + 0.25;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;
    if ( target_point.x > wm.self().pos().x - 0.5
         && wm.self().pos().x < wm.offsideLineX()
         && std::fabs( target_point.x - wm.self().pos().x ) > 1.0 )
    {
        dist_thr = std::min( 1.0, wm.ball().pos().dist( target_point ) * 0.1 + 0.5 );
    }

    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );
    dlog.addText( Logger::TEAM,
                  __FILE__": (doGoToPoint) target=(%.2f, %.2f) dash_power=%.2f",
                  target_point.x, target_point.y, dash_power );

    Vector2D my_inertia = wm.self().inertiaPoint( mate_min );

    if ( mate_min <= 5
         && wm.self().stamina() > ServerParam::i().staminaMax() * 0.6
         && my_inertia.dist( target_point ) > dist_thr
         && ( my_inertia.x > target_point.x
              || my_inertia.x > wm.offsideLineX() )
         && std::fabs( my_inertia.x - target_point.x ) < 3.0
         && wm.self().body().abs() < 15.0 )
    {
        double back_accel
            = std::min( target_point.x, wm.offsideLineX() )
            - wm.self().pos().x
            - wm.self().vel().x;
        double back_dash_power = back_accel / wm.self().dashRate();
        back_dash_power = wm.self().getSafetyDashPower( back_dash_power );
        back_dash_power = ServerParam::i().normalizePower( back_dash_power );
        agent->debugClient().addMessage( "Attack:Back%.0f", back_dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__": Back Move. power=%.1f", back_dash_power );
        agent->doDash( back_dash_power );
    }
    else if ( Body_GoToPoint( target_point, dist_thr, dash_power,
                              -1.0, // dash speed
                              100, // cycle
                              true, // save recovery
                              25.0 // angle threshold
                              ).execute( agent ) )
    {
        if ( intentional )
        {
            agent->debugClient().addMessage( "BreakAway" );

            agent->debugClient().addMessage( "Say_HeyPass" );
            agent->addSayMessage( new PassRequestMessage( target_point ) );

            dlog.addText( Logger::TEAM,
                          __FILE__": intention breakaway" );
            agent->setIntention( new IntentionAttackerBreakAway( target_point,
                                                                 10,
                                                                 wm.time() ) );
            agent->setArmAction( new Arm_PointToPoint( target_point ) );
        }
    }
    else if ( wm.self().pos().x > wm.offsideLineX() - 0.1
              && Body_GoToPoint( Vector2D( wm.offsideLineX() - 0.5, wm.self().pos().y ),
                                 0.3, // small dist threshold
                                 dash_power,
                                 -1.0, // dash speed
                                 1, // cycle
                                 true, // stamina save
                                 25.0 // angle threshold
                                 ).execute( agent ) )
    {
        agent->debugClient().addMessage( "Attack:AvoidOffside" );
        dlog.addText( Logger::TEAM,
                          __FILE__": avoid offside" );
    }
    else
    {
        AngleDeg body_angle( 0.0 );
        Body_TurnToAngle( body_angle ).execute( agent );
        agent->debugClient().addMessage( "Attack:Turn" );
        dlog.addText( Logger::TEAM,
                      __FILE__": execute. turn to angle=%.1f",
                      body_angle.degree() );
    }

    if ( ( wm.kickableTeammate()
           || mate_min <= 2 )
         && wm.self().pos().dist( mate_trap_pos ) < 20.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (execute) turn neck to our ball holder." );
        agent->setNeckAction( new Neck_TurnToBallOrScan( -1 ) );
    }
    else
    {
        //int min_step = std::min( self_min, opp_min );
        int min_step = opp_min;
        int count_thr = 0;
        ViewWidth view_width = agent->effector().queuedNextViewWidth();
        int see_cycle = agent->effector().queuedNextSeeCycles();
        if ( view_width.type() == ViewWidth::WIDE )
        {
            if ( min_step > see_cycle )
            {
                count_thr = 2;
            }
        }
        else if ( view_width.type() == ViewWidth::NORMAL )
        {
            if ( min_step > see_cycle )
            {
                count_thr = 1;
            }
        }

        agent->setNeckAction( new Neck_TurnToBallOrScan( count_thr ) );
        dlog.addText( Logger::TEAM,
                      __FILE__": execute. neck ball or scan. min_step=%d  see_cycle=%d  count_thr=%d",
                      min_step, see_cycle, count_thr );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_AttackerOffensiveMove::getTargetPoint( rcsc::PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    Vector2D target_point = home_pos;

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();
    const Vector2D mate_trap_pos = wm.ball().inertiaPoint( mate_min );
    const double max_x = ( ( mate_min < opp_min - 2
                             && mate_trap_pos.x > wm.offsideLineX() - 2.0 )
                           ? std::max( wm.offsideLineX(), mate_trap_pos.x + 2.0 )
                           : std::max( wm.offsideLineX(), mate_trap_pos.x ) );
    dlog.addText( Logger::TEAM,
                  __FILE__": (getTargetPoint) max_x=%.2f (offside=%.2f, trapX=%.2f)",
                  max_x, wm.offsideLineX(), mate_trap_pos.x );

    if ( wm.self().pos().x > max_x
         && wm.self().pos().x < 42.0
         //&& std::fabs( wm.self().pos().y - M_home_pos.y ) < 10.0
         )
    {
        target_point.y = wm.self().pos().y;
    }

//     // 2008-07-17 akiyama
//     if ( M_forward_player
//          && target_point.x < max_x - 1.0
//          && Strategy::i().opponentDefenseStrategy() == ManMark_Strategy )
//     {
//         target_point.x = std::min( max_x - 1.0, home_pos.x + 15.0 );
//         dlog.addText( Logger::TEAM,
//                       __FILE__": (getTargetPoint) for ManMark" );
//     }

    if ( std::fabs( mate_trap_pos.y - home_pos.y ) < 15.0
         || mate_trap_pos.x > max_x - 5.0 )
    {
#if 1
        if ( target_point.x > max_x - 1.5 )
        {
            target_point.x = std::min( home_pos.x, max_x - 1.5 );
        }
#else
        // 2009-07-05
        if ( target_point.x > max_x - 0.5 )
        {
            target_point.x = std::min( home_pos.x, max_x - 0.5 );
        }
#endif
    }
    else
    {
#if 1
        if ( target_point.x > max_x - 3.0 )
        {
            target_point.x = std::min( home_pos.x, max_x - 3.0 );
        }
#else
        // 2009-07-05
        if ( target_point.x > max_x - 1.0 )
        {
            target_point.x = std::min( home_pos.x, max_x - 1.0 );
        }
#endif
    }

#if 0
    // 2009-05-10 akiyama
    if ( M_forward_player
         && target_point.x < max_x - 15.0 )
    {
        target_point.x  = max_x - 15.0;
        dlog.addText( Logger::TEAM,
                      __FILE__": (getTargetPoint) adjust to max_x" );
    }
#endif

    // 2008-04-28 akiyama
    // 2009-05-10 akiyama, moved
    if ( mate_min < 3
         && std::fabs( wm.self().pos().y - home_pos.y ) < 3.0 )
    {
        double new_y = wm.self().pos().y * 0.9 + home_pos.y * 0.1;
        dlog.addText( Logger::TEAM,
                      __FILE__": (getTargetPoint) adjust target point to prepare receive. y=%.1f -> %.1f",
                      target_point.y, new_y );
        target_point.y = new_y;
    }

    // 2008-04-23 akiyama
    if ( mate_min >= 3
         && wm.self().pos().dist2( target_point ) < 5.0*5.0 )
    {
        double opp_dist = 1000.0;
        const PlayerObject * opp = wm.getOpponentNearestTo( target_point,
                                                            10,
                                                            &opp_dist );
        if ( opp
             && opp_dist < 4.0
             && std::fabs( opp->pos().y - target_point.y ) < 2.0 )
        {
            double new_y = ( target_point.y > opp->pos().y
                             ? opp->pos().y + 2.0
                             : opp->pos().y - 2.0 );
            dlog.addText( Logger::TEAM,
                          __FILE__": (getTargetPoint) adjust target point to avvoid opponent. y=%.1f -> %.1f",
                          target_point.y, new_y );
            target_point.y = new_y;
        }
    }

#if 1
    // 2009-05-10
    if ( M_forward_player
         && mate_min < opp_min
         //&& mate_trap_pos.x > target_point.x - 15.0
         && mate_trap_pos.x > 36.0 // 2010-05-03
         && std::fabs( mate_trap_pos.y - target_point.y ) > 10.0 )
    {
        //const double y_length = 14.0;
        const double y_length = 13.5;
        //const double y_length = 12.5;

        const PlayerObject * fastest_teammate = wm.interceptTable()->fastestTeammate();
        const Rect2D rect
            = Rect2D::from_center( ( mate_trap_pos.x + target_point.x ) * 0.5,
                                   ( mate_trap_pos.y + target_point.y ) * 0.5,
                                   std::max( y_length, std::fabs( mate_trap_pos.x - target_point.x ) ),
                                   std::fabs( mate_trap_pos.y - target_point.y ) );
        bool exist_other_attacker = false;
        for ( PlayerObject::Cont::const_iterator it = wm.teammatesFromSelf().begin(),
                  end = wm.teammatesFromSelf().end();
              it != end;
              ++it )
        {
            if ( (*it)->posCount() > 10
                 || (*it)->isGhost()
                 || (*it) == fastest_teammate )
            {
                continue;
            }

            if ( rect.contains( (*it)->pos() ) )
            {
                exist_other_attacker = true;
                dlog.addText( Logger::TEAM,
                              __FILE__": (getTargetPoint) exist other attacker. no adjust for support" );
                break;
            }
        }

        agent->debugClient().addRectangle( rect );

        if ( ! exist_other_attacker )
        {
            Vector2D new_target = target_point;
            new_target.y = mate_trap_pos.y + y_length * sign( target_point.y - mate_trap_pos.y );

            dlog.addText( Logger::TEAM,
                          __FILE__": (getTargetPoint) no other attacker."
                          " old_target=(%.1f, %.1f)"
                          " new_target=(%.1f, %.1f)",
                          target_point.x, target_point.y,
                          new_target.x, new_target.y );
            double dash_dist = wm.self().pos().dist( new_target );
            int dash_cycle = wm.self().playerType().cyclesToReachDistance( dash_dist );
            int safe_cycle = wm.self().playerType().getMaxDashCyclesSavingRecovery( ServerParam::i().maxDashPower(),
                                                                                    wm.self().stamina(),
                                                                                    wm.self().recovery() );
            if ( dash_cycle < safe_cycle - 20 )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": (getTargetPoint) no other attacker. update. "
                              "dash_cycle=%d << safe_cycle=%d",
                              dash_cycle, safe_cycle );
                target_point = new_target;
            }
        }
    }
#endif

    //
    // avoid offside
    //
    if ( ( wm.kickableTeammate()
           || mate_min <= 5
           || mate_min <= opp_min + 1 )
         && wm.self().pos().x > max_x
         && std::fabs( mate_trap_pos.y - target_point.y ) < 4.0 )
    {
        double abs_y = wm.ball().pos().absY();
        bool outer = ( wm.self().pos().absY() > abs_y );
        if ( abs_y  > 25.0 ) target_point.y = ( outer ? 30.0 : 20.0 );
        else if ( abs_y > 20.0 ) target_point.y = ( outer ? 25.0 : 15.0 );
        else if ( abs_y > 15.0 ) target_point.y = ( outer ? 20.0 : 10.0 );
        else if ( abs_y > 10.0 ) target_point.y = ( outer ? 15.0 : 5.0 );
        else if ( abs_y > 5.0 ) target_point.y = ( outer ? 10.0 : 0.0 );
        else target_point.y = ( outer ? 5.0 : -5.0 );

        if ( wm.self().pos().y < 0.0 )
        {
            target_point.y *= -1.0;
        }

        //agent->debugClient().addMessage( "Attack:AvoidOffside" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (getTargetPoint) avoid offside possibility. new_target=(%.1f %.1f)",
                      target_point.x, target_point.y );
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (getTargetPoint) result=(%.2f %.2f)",
                  target_point.x, target_point.y );

    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_AttackerOffensiveMove::getDashPower( const PlayerAgent * agent,
                                         const Vector2D & target_point )
{
    const WorldModel & wm = agent->world();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();
    Vector2D receive_pos = wm.ball().inertiaPoint( mate_min );

    if ( target_point.x > wm.self().pos().x
         && wm.self().stamina() > ServerParam::i().staminaMax() * 0.7
         && mate_min <= 8
         && ( mate_min <= opp_min + 3
              || wm.kickableTeammate() )
         && std::fabs( receive_pos.y - target_point.y ) < 25.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) chance. fast move" );
        return ServerParam::i().maxDashPower();
    }

    if  ( wm.self().pos().x > wm.offsideLineX()
          && ( wm.kickableTeammate()
               || mate_min <= opp_min + 2 )
          && target_point.x < receive_pos.x + 30.0
          && wm.self().pos().dist( receive_pos ) < 30.0
          && wm.self().pos().dist( target_point ) < 20.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) offside max power" );

        return ServerParam::i().maxDashPower();
    }

    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) empty capacity. max power" );

        return ServerParam::i().maxDashPower();
    }

    //------------------------------------------------------
    // decide dash power
    static bool s_recover_mode = false;

    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) no stamina capacity. never recover mode" );
        s_recover_mode = false;
    }
    else if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.4 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) change to recover mode." );
        s_recover_mode = true;
    }
    else if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.7 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) came back from recover mode" );
        s_recover_mode = false;
    }

    const double my_inc
        = wm.self().playerType().staminaIncMax()
        * wm.self().recovery();

    // dash power
    if ( s_recover_mode )
    {
        // Magic Number.
        // recommended one cycle's stamina recover value
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) recover mode" );
        return std::max( 0.0, my_inc - 30.0 );
    }

    if ( ! wm.opponentsFromSelf().empty()
         && wm.opponentsFromSelf().front()->distFromSelf() < 2.0
         && wm.self().stamina() > ServerParam::i().staminaMax() * 0.7
         && mate_min <= 8
         && ( mate_min <= opp_min + 3
              || wm.kickableTeammate() )
         )
    {
        // opponent is very close
        // full power
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) exist near opponent. full power" );
        return ServerParam::i().maxDashPower();
    }

    if  ( wm.ball().pos().x < wm.self().pos().x
          && wm.self().pos().x < wm.offsideLineX() )
    {
        // ball is back
        // not offside
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) ball is back and not offside." );
        if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.8 )
        {
            return std::min( std::max( 5.0, my_inc - 30.0 ),
                             ServerParam::i().maxDashPower() );
        }
        else
        {
            return std::min( my_inc * 1.1,
                             ServerParam::i().maxDashPower() );
        }
    }

    if ( wm.ball().pos().x > wm.self().pos().x + 3.0 )
    {
        // ball is front
        if ( opp_min <= mate_min - 3 )
        {
            if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.6 )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": (getDashPower) ball is front. recover" );
                return std::min( std::max( 0.1, my_inc - 30.0 ),
                                 ServerParam::i().maxDashPower() );
            }
            else if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.8 )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": (getDashPower) ball is front. keep" );
                return std::min( my_inc, ServerParam::i().maxDashPower() );
            }
            else
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": (getDashPower) ball is front. max" );
                return ServerParam::i().maxDashPower();
            }
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (getDashPower) ball is front full powerr" );
            return ServerParam::i().maxDashPower();
        }
    }


    dlog.addText( Logger::TEAM,
                  __FILE__": (getDashPower) normal mode." );
    if ( target_point.x > wm.self().pos().x + 2.0
         && wm.self().stamina() > ServerParam::i().staminaMax() * 0.6 )
    {
        return ServerParam::i().maxDashPower();
    }
    else if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.8 )
    {
        return std::min( my_inc * 0.9,
                         ServerParam::i().maxDashPower() );
    }
    else
    {
        return std::min( my_inc * 1.5,
                         ServerParam::i().maxDashPower() );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_AttackerOffensiveMove::checkStaminaForBreakaway( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();
    const PlayerType & ptype = wm.self().playerType();

    Vector2D my_final = wm.self().inertiaFinalPoint();
    Vector2D target_point( std::min( my_final.x + 30.0, SP.pitchHalfLength() - 10.0 ),
                           my_final.y * 0.9 );

    const double dash_dist = my_final.dist( target_point );
    const double dash_power = SP.maxDashPower();
    const double recover_thr = SP.recoverDecThrValue();

    StaminaModel stamina_model = wm.self().staminaModel();

    int i = 0;
    double dist = 0.0;
    double speed = 0.0;
    while ( i < 100
            && dist < dash_dist
            //&& stamina > recover_thr )
            && stamina_model.stamina() > recover_thr )
    {
        //double power = std::min( stamina + ptype.extraStamina(), dash_power );
        //double accel = power * ptype.dashRate( effort );
        double power = std::min( stamina_model.stamina() + ptype.extraStamina(), dash_power );
        double accel = power * ptype.dashRate( stamina_model.effort() );

        speed += accel;
        dist += speed;
        speed *= ptype.playerDecay();

        stamina_model.simulateDash( ptype, power );
        if ( stamina_model.recovery() < 0.99 )
        {
            return false;
        }
        ++i;
    }

    return stamina_model.stamina() > recover_thr + 500.0;
}
