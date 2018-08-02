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

#include "bhv_side_half_cross_move.h"

#include "field_analyzer.h"
#include "strategy.h"
#include "shoot_simulator.h"

#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_turn_to_angle.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>
#include <rcsc/action/neck_turn_to_point.h>
#include <rcsc/action/view_normal.h>
#include <rcsc/action/arm_point_to_point.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/soccer_intention.h>
#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/server_param.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>

// #define DEBUG_PRINT
// #define DEBUG_PRINT_TARGET_POINT

#define USE_GO_TO_FREE_SPACE

using namespace rcsc;

namespace {

/*-------------------------------------------------------------------*/

class IntentionGoToSpace
    : public SoccerIntention {
private:
    const Vector2D M_target_point;
    GameTime M_last_execute_time;
    int M_counter;
    int M_total_counter;
public:

    IntentionGoToSpace( const Vector2D & target_point,
                        const GameTime & start_time )
        : M_target_point( target_point ),
          M_last_execute_time( start_time ),
          M_counter( 0 ),
          M_total_counter( 0 )
      { }

    bool finished( const PlayerAgent * agent );

    bool execute( PlayerAgent * agent );

};

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionGoToSpace::finished( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( M_counter >= 4 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished). count over" );
        return true;
    }

    if ( M_total_counter >= 20 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished). total count over" );
        return true;
    }

    if ( wm.audioMemory().passTime() == wm.time()
         && ! wm.audioMemory().pass().empty()
         && ( wm.audioMemory().pass().front().receiver_ == wm.self().unum() )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished). heard pass message." );
        return true;
    }

    if ( M_last_execute_time.cycle() + 1 != wm.time().cycle() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished). illegal last execution time" );
        return true;
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ! wm.kickableTeammate()
         && self_min <= mate_min + 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) my intercept situation" );
        return true;
    }

    if ( wm.kickableOpponent()
         || opp_min < mate_min )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) opponent gets the ball" );
        return true;
    }

    const Vector2D ball_pos = wm.ball().inertiaPoint( mate_min );
    const double offside_bonus = std::max( 0.0, ball_pos.x - wm.ball().pos().x );
    const double max_x = std::max( ball_pos.x,
                                   std::min( ServerParam::i().pitchHalfLength() - 2.0,
                                             wm.offsideLineX() + offside_bonus ) );

    if ( wm.self().pos().x > max_x )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished). over offside or trap line" );
        return true;
    }

    if ( wm.self().pos().dist( M_target_point ) < 1.0 )
    {
        const PlayerObject * opponent = wm.getOpponentNearestToSelf( 5 );
        if ( opponent
             && opponent->distFromSelf() < 2.0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (finished). exist marker opponent" );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionGoToSpace::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    M_last_execute_time = wm.time();
    ++M_total_counter;

    double dist_thr = 0.5;

    agent->debugClient().addMessage( "I_Space" );

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();

    if ( self_min <= mate_min + 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": intention execute. intercept" );
        Vector2D face_point( ServerParam::i().pitchHalfLength(), 0.0 );
        Body_Intercept( true, face_point ).execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        return true;
    }

    Vector2D actual_target = M_target_point;
#if 1
    // 20009-07-05
    if ( actual_target.x > wm.offsideLineX() - 0.2 )
    {
        Line2D dash_line( wm.self().inertiaFinalPoint(), actual_target );
        actual_target.x = wm.offsideLineX() - 0.2;
        actual_target.y = dash_line.getY( actual_target.x );
    }
#endif

    agent->debugClient().setTarget( M_target_point );
    agent->debugClient().addCircle( actual_target, dist_thr );

    dlog.addText( Logger::TEAM,
                  __FILE__": intention execute. target=(%.2f, %.2f) actual=(%.2f %.2f)",
                  M_target_point.x, M_target_point.y,
                  actual_target.x, actual_target.y );

    if ( ! Body_GoToPoint( actual_target, dist_thr,
                           ServerParam::i().maxDashPower(),
                           -1.0, // dash speed
                           100, // cycle
                           true, // stamina save
                           20.0 // angle threshold
                           ).execute( agent ) )
    {
        AngleDeg body_angle( 0.0 );
        Body_TurnToAngle( body_angle ).execute( agent );

        ++M_counter;
    }

    if ( wm.ball().posCount() <= 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": intention execute. check goalie or scan field" );
        agent->setNeckAction( new Neck_TurnToGoalieOrScan( 2 ) );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": intention execute. check ball or scan field" );
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    }

    return true;
}

}

/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfCrossMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_SideHalfCrossMove" );

    const WorldModel & wm = agent->world();

    Vector2D target_point = getTargetPoint( agent );

#ifdef USE_GO_TO_FREE_SPACE
    if ( doGoToSpace( agent, target_point ) )
    {
        return true;
    }
#endif

    //----------------------------------------------
    // adjust target point
    target_point = getAvoidOffsidePosition( wm, target_point );

    //----------------------------------------------
    // set dash power

    double dash_power = getDashPower( agent, target_point );

    //----------------------------------------------
    // positioning to make the cross course!!

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    double turn_thr = 30.0;

    //     if ( wm.kickableTeammate()
    //          || wm.interceptTable()->teammateReachCycle() <= 2 )
    //     {
    //         turn_thr = 40.0;
    //     }

    agent->debugClient().addMessage( "GoToCross%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    dlog.addText( Logger::TEAM,
                  __FILE__":(execute) target=(%.2f, %.2f)",
                  target_point.x, target_point.y );

    if ( wm.self().pos().x > target_point.x + dist_thr
         && std::fabs( wm.self().pos().x - target_point.x ) < 3.0
         && wm.self().body().abs() < 10.0 )
    {
        agent->debugClient().addMessage( "Back" );
        double back_dash_power = wm.self().getSafetyDashPower( -dash_power );
        dlog.addText( Logger::TEAM,
                      __FILE__": doShootAreaMove. Back Move" );
        agent->doDash( back_dash_power );
    }
    else
    {
        if ( ! Body_GoToPoint( target_point, dist_thr, dash_power,
                               -1.0, // dash speed
                               5, // cycle
                               true, // save recovery
                               turn_thr // dir thr
                               ).execute( agent ) )
        {
            Body_TurnToAngle( 0.0 ).execute( agent );
        }
    }

    if ( wm.self().pos().x > 30.0 )
    {
        agent->setNeckAction( new Neck_TurnToGoalieOrScan( 2 ) );
    }
    else
    {
        agent->setNeckAction( new Neck_TurnToLowConfTeammate() );
        //agent->setNeckAction( new Neck_ScanField() );
    }


    if ( agent->world().self().armMovable() == 0
         && agent->world().self().armExpires() > 0 )
    {
        agent->debugClient().addMessage( "ArmOff" );
        agent->setArmAction( new Arm_Off() );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfCrossMove::doGoToSpace( PlayerAgent * agent,
                                    const Vector2D & base_target_point )
{
    const WorldModel & wm = agent->world();

    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    // check ball owner
    if ( mate_min <= 5
         || mate_min < opp_min + 3 )
    {

    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(doGoToSpace) false: no our ball" );
        return false;
    }

    // check home position range
    if ( wm.self().pos().dist( base_target_point ) > 10.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(doGoToSpace) false: far home position" );
        return false;
    }

    // stamina check
    const double available_stamina = wm.self().stamina()
        - ServerParam::i().recoverDecThrValue();
    if ( available_stamina < wm.self().playerType().getOneStepStaminaComsumption() * 20 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(doGoToSpace) false: no enough stamina" );
        return false;
    }

    const Vector2D ball_trap_pos = wm.ball().inertiaPoint( mate_min );

    //if ( ball_trap_pos.x < 36.0 )
    if ( ball_trap_pos.x < 27.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(doGoToSpace) false: no chance situation" );
        return false;
    }

    if ( std::fabs( ball_trap_pos.y - base_target_point.y ) > 15.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__" (doGoToSpace) false: big y diff %.1f ball=(%.2f %.2f) target=(%.2f %.2f)",
                      std::fabs( ball_trap_pos.y - base_target_point.y ),
                      ball_trap_pos.x, ball_trap_pos.y,
                      base_target_point.x, base_target_point.y );
        return false;
    }

    const PlayerObject * opponent = wm.getOpponentNearestToSelf( 5 );

    if ( ! opponent )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(doGoToSpace) false: no opponent" );
        return false;
    }

    const Vector2D self_pos = wm.self().inertiaFinalPoint();
    const Vector2D opponent_pos = opponent->inertiaFinalPoint();

    if ( self_pos.dist2( opponent_pos ) > std::pow( 2.0, 2 ) //std::pow( 3.0, 2 )
         || opponent_pos.dist( ball_trap_pos ) > self_pos.dist( ball_trap_pos ) + 2.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(doGoToSpace) false: no opponent marker" );
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(doGoToSpace) exist opponent marker %d (%.2f %.2f)",
                  opponent->unum(),
                  opponent->pos().x, opponent->pos().y );

    Vector2D target_point = base_target_point;

    if ( wm.offsideLineX() < base_target_point.x + 2.0 )
    {
        target_point.y = base_target_point.y
            + 3.5 * ( base_target_point.y < ball_trap_pos.y
                      ? -1.0
                      : 1.0 );
        dlog.addText( Logger::TEAM,
                      __FILE__":(doGoToSpace) false: set target(1). target=(%.1f %.1f)",
                      target_point.x, target_point.y );
        return false;
    }
    else
    {
        const double min_y = bound( - ServerParam::i().goalHalfWidth() - 6.0,
                                    ( base_target_point.y < ball_trap_pos.y
                                      ? base_target_point.y - 1.0
                                      : base_target_point.y - 7.0 ),
                                    + ServerParam::i().goalHalfWidth() + 6.0 );
        const double max_y = bound( - ServerParam::i().goalHalfWidth() - 6.0,
                                    ( base_target_point.y < ball_trap_pos.y
                                      ? base_target_point.y + 7.0
                                      : base_target_point.y + 1.0 ),
                                    + ServerParam::i().goalHalfWidth() + 6.0 );

        Vector2D best_pos = base_target_point;
        double max_dist = 0.0;

        agent->debugClient().addRectangle( Rect2D( Vector2D( base_target_point.x, min_y ),
                                                   Vector2D( wm.offsideLineX(), max_y ) ) );

        for ( double x = 49.0;
              x > base_target_point.x - 3.0;
              x -= 0.5 )
        {
            const int div = 8;
            for ( int d = 0; d <= div; ++d )
            {
                Vector2D pos( x,
                              min_y * d / div + max_y * ( div - d ) / div );
                double ball_dist2 = pos.dist2( ball_trap_pos );
                //if ( ball_dist2 < std::pow( 15.0, 2 ) )
                if ( ball_dist2 < std::pow( 13.0, 2 ) )
                {
#ifdef DEBUG_PRINT
                    dlog.addText( Logger::TEAM,
                                  __FILE__":____ pos=(%.1f %.1f) far ball trap pos",
                                  pos.x, pos.y );
#endif
                    continue;
                }

                if ( ball_dist2 < std::pow( 5.0, 2 ) )
                {
#ifdef DEBUG_PRINT
                    dlog.addText( Logger::TEAM,
                                  __FILE__":____ pos=(%.1f %.1f) near ball trap pos",
                                  pos.x, pos.y );
#endif
                    continue;
                }

                double dist = 1000.0;
#ifdef DEBUG_PRINT
                const PlayerObject * opp = wm.getOpponentNearestTo( pos, 20, &dist );
                if ( opp )
                {
                    dlog.addText( Logger::TEAM,
                                  __FILE__":____ pos=(%.1f %.1f) opp=%d (%.1f %.1f) dist=%f",
                                  pos.x, pos.y,
                                  opp->unum(), opp->pos().x, opp->pos().y,
                                  dist );
                }
#else
                wm.getOpponentNearestTo( pos, 20, &dist );
#endif

                if ( dist < 3.0 )
                {
                    continue;
                }

                //
                // check shoot course
                //
                if ( ShootSimulator::can_shoot_from( false, pos, wm.theirPlayers(), 10 ) )
                {
                    if ( dist > max_dist )
                    {
                        max_dist = dist;
                        best_pos = pos;
                    }
                }
            }
        }

        if ( max_dist > 3.0 )
        {
            target_point = best_pos;
            dlog.addText( Logger::TEAM,
                          __FILE__":(doGoToSpace) set target(2). target=(%.1f %.1f)",
                          target_point.x, target_point.y );
        }
        else
        {
            target_point.x = base_target_point.x;
            target_point.y = base_target_point.y
                + 3.5 * ( base_target_point.y < ball_trap_pos.y
                          ? -1.0
                          : 1.0 );
            dlog.addText( Logger::TEAM,
                          __FILE__":(doGoToSpace) false: set target(3). target=(%.1f %.1f)",
                          target_point.x, target_point.y );
            return false;
        }
    }

    agent->debugClient().addMessage( "GoToSpace" );
    dlog.addText( Logger::TEAM,
                  __FILE__":(doGoToSpace) true: target=(%.2f %.2f)",
                  target_point.x, target_point.y );

    if ( ! Body_GoToPoint( target_point,
                           0.5, // dist_thr
                           ServerParam::i().maxDashPower(),
                           -1.0, // dash speed
                           100, // cycle
                           true, // stamina save
                           20.0 // angle threshold
                           ).execute( agent ) )
    {
        // AngleDeg body_angle( 0.0 );
        // Body_TurnToAngle( body_angle ).execute( agent );
        Body_TurnToBall().execute( agent );
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    agent->setArmAction( new Arm_PointToPoint( target_point ) );

    agent->setIntention( new IntentionGoToSpace( target_point, wm.time() ) );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfCrossMove::doCheckCrossPoint( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.self().pos().x < 35.0 )
    {
        return false;
    }

    const AbstractPlayerObject * opp_goalie = wm.getTheirGoalie();
    if ( opp_goalie
         && opp_goalie->posCount() >= 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doCheckCrossTarget) goalie should be checked" );
        return false;
    }

    Vector2D opposite_pole( ServerParam::i().pitchHalfLength() - 5.5,
                            ServerParam::i().goalHalfWidth() );
    if ( wm.self().pos().y > 0.0 ) opposite_pole.y *= -1.0;

    AngleDeg opposite_pole_angle = ( opposite_pole - wm.self().pos() ).th();


    if ( wm.dirCount( opposite_pole_angle ) <= 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doCheckCrossTarget) enough accuracy to angle %.1f",
                      opposite_pole_angle.degree() );
        return false;
    }

    AngleDeg angle_diff = agent->effector().queuedNextAngleFromBody( opposite_pole );
    if ( angle_diff.abs() > 100.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doCheckCrossPoint) over view range. angle_diff=%.1f",
                      angle_diff.degree() );
        return false;
    }


    agent->setNeckAction( new Neck_TurnToPoint( opposite_pole ) );
    agent->debugClient().addMessage( "NeckToOpposite" );
    dlog.addText( Logger::TEAM,
                  __FILE__": (doCheckCrossPoint) Neck to oppsite pole" );
    return true;
}

namespace {
/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_target_point_2012( const WorldModel & wm )
{
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    Vector2D target_point = home_pos;

    dlog.addText( Logger::TEAM,
                  __FILE__":(get_target_point_2012) home_pos=(%.1f %.1f)",
                  home_pos.x, home_pos.y );

    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    Vector2D ball_trap_pos = ( mate_min < 100
                               ? wm.ball().inertiaPoint( mate_min )
                               : wm.ball().pos() );

    // 2009-07-05
    if ( target_point.x > wm.offsideLineX() - 2.0 )
    {
        target_point.x = wm.offsideLineX() - 0.5;
        dlog.addText( Logger::TEAM,
                      __FILE__":(get_target_point_2012) avoid offside" );
    }

    // 2009-05-10, 2011-06-24
    //
    // TODO: replaced by GeneratorCrossMove
    //
    if ( ( wm.kickableTeammate()
           || mate_min <= opp_min + 1 )
         //&& ball_trap_pos.x < 36.0
         && ball_trap_pos.x > target_point.x - 15.0
         && std::fabs( ball_trap_pos.y - target_point.y ) > 15.0 )
    {
        const double y_length = 14.0;

        const PlayerObject * fastest_teammate = wm.interceptTable()->fastestTeammate();
        const Rect2D rect
            = Rect2D::from_center( ( ball_trap_pos.x + target_point.x ) * 0.5,
                                   ( ball_trap_pos.y + target_point.y ) * 0.5,
                                   std::max( y_length, std::fabs( ball_trap_pos.x - target_point.x ) ),
                                   std::fabs( ball_trap_pos.y - target_point.y ) );
        bool exist_other_attacker = false;
        for ( PlayerObject::Cont::const_iterator it = wm.teammatesFromSelf().begin(),
                  end = wm.teammatesFromSelf().end();
              it != end;
              ++it )
        {
            if ( (*it)->posCount() > 10 ) continue;
            if ( (*it)->isGhost() ) continue;
            if ( *it == fastest_teammate ) continue;

            if ( rect.contains( (*it)->pos() ) )
            {
                exist_other_attacker = true;
                dlog.addText( Logger::TEAM,
                              __FILE__":(get_target_point_2012) exist other attacker. no adjust for support" );
                break;
            }
        }

        dlog.addRect( Logger::TEAM, rect, "#F00" );

        if ( ! exist_other_attacker )
        {
            Vector2D new_target = target_point;
            new_target.y = ball_trap_pos.y + y_length * sign( target_point.y - ball_trap_pos.y );
            if ( new_target.absY() > ServerParam::i().goalHalfWidth() )
            {
                new_target.y = ServerParam::i().goalHalfWidth() * sign( new_target.y );
            }

            dlog.addText( Logger::TEAM,
                          __FILE__": (get_target_point_2012) no other attacker."
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
                              __FILE__":(get_target_point_2012) no other attacker. update. "
                              "dash_cycle=%d << safe_cycle=%d",
                              dash_cycle, safe_cycle );
                target_point = new_target;
            }
        }
    }

    // consider near opponent
    if ( target_point.x > 36.0 )
    {
        double opp_dist = 200.0;
        const PlayerObject * opp = wm.getOpponentNearestTo( target_point,
                                                            10,
                                                            &opp_dist );
        if ( opp && opp_dist < 2.0 )
        {
            Vector2D tmp_target = target_point;
            for ( int i = 0; i < 3; ++i )
            {
                tmp_target.x -= 1.0;

                double d = 0.0;
                opp = wm.getOpponentNearestTo( tmp_target, 10, &d );
                if ( ! opp )
                {
                    opp_dist = 0.0;
                    target_point = tmp_target;
                    break;
                }

                if ( opp
                     && opp_dist < d )
                {
                    opp_dist = d;
                    target_point = tmp_target;
                }
            }
            dlog.addText( Logger::TEAM,
                          __FILE__":(get_target_point_2012) avoid opponent (%.2f, %.2f)->(%.2f, %.2f)",
                          home_pos.x, home_pos.y,
                          target_point.x, target_point.y );
        }
    }

    //
    // avoid ball owner
    //
    if ( target_point.dist( ball_trap_pos ) < 6.0 )
    {
        Circle2D target_circle( ball_trap_pos, 6.0 );
        Line2D target_line( target_point, AngleDeg( 90.0 ) );
        Vector2D sol_pos1, sol_pos2;
        int n_sol = target_circle.intersection( target_line, &sol_pos1, &sol_pos2 );

        if ( n_sol == 1 ) target_point = sol_pos1;
        if ( n_sol == 2 )
        {
            target_point = ( wm.self().pos().dist2( sol_pos1 ) < wm.self().pos().dist2( sol_pos2 )
                             ? sol_pos1
                             : sol_pos2 );

        }

        dlog.addText( Logger::TEAM,
                      __FILE__":(get_target_point_2012) avoid ball owner. (%.2f %.2f)->(%.2f %.2f)",
                      home_pos.x, home_pos.y,
                      target_point.x, target_point.y );
    }

    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_target_point_2013( const WorldModel & wm )
{
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    Vector2D target_point = home_pos;

    dlog.addText( Logger::TEAM,
                  __FILE__":(get_target_point_2013) home_pos=(%.2f %.2f)",
                  home_pos.x, home_pos.y );

    //
    // judge situation
    //
    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( wm.kickableTeammate()
         || teammate_step <= 2  )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(get_target_point_2013) maybe our ball" );
    }
    else if ( FieldAnalyzer::is_ball_moving_to_their_goal( wm ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(get_target_point_2013) ball is moving to their goal" );
    }
    else if ( opponent_step <= teammate_step - 5
              || opponent_step == 0
              || wm.kickableOpponent() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(get_target_point_2013) home_pos: their ball" );
        return home_pos;
    }

    //
    // check other teammate, ball position
    //

    const PlayerObject * first_teammate = wm.interceptTable()->fastestTeammate();
    if ( ! first_teammate )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(get_target_point_2013) home_pos: no teammate" );
        return home_pos;
    }

    const Vector2D teammate_ball_pos = wm.ball().inertiaPoint( teammate_step );

    dlog.addText( Logger::TEAM,
                  __FILE__":(get_target_point_2013) teammate_ball_pos=(%.2f %.2f)",
                  teammate_ball_pos.x, teammate_ball_pos.y );

    if ( wm.self().pos().x < 36.0
         && wm.self().pos().x < wm.offsideLineX() )
    {
        target_point.x = 47.0;
        target_point.y = ( Strategy::i().getPositionType( wm.self().unum() ) == Position_Left
                           ? -7.0
                           : Strategy::i().getPositionType( wm.self().unum() ) == Position_Right
                           ? + 7.0
                           : 0.0 );
        dlog.addText( Logger::TEAM,
                      __FILE__":(get_target_point_2013) far opponent goal. set static position (%.2f %.2f)",
                      target_point.x, target_point.y );
        return target_point;
    }
#if 0
    if ( home_pos.y * teammate_ball_pos.y > 0.0 ) // same side
    {
        target_point = home_pos;
        dlog.addText( Logger::TEAM,
                      __FILE__":(get_target_point_2013) same side. set home position (%.2f %.2f)",
                      target_point.x, target_point.y );
        return target_point;
    }
#endif
    if ( teammate_ball_pos.absY() > 20.0
         && std::fabs( home_pos.y - teammate_ball_pos.y ) > 20.0 )
    {
        target_point = home_pos;
        dlog.addText( Logger::TEAM,
                      __FILE__":(get_target_point_2013) big y diff. set home position (%.2f %.2f)",
                      target_point.x, target_point.y );
        return target_point;
    }

    if ( home_pos.x < 36.0
         && home_pos.x > teammate_ball_pos.x + 5.0 )
    {
        target_point = home_pos;
        dlog.addText( Logger::TEAM,
                      __FILE__":(get_target_point_2013) set home position(2) (%.2f %.2f)",
                      target_point.x, target_point.y );
        return target_point;
    }

    //
    // check other attacker
    //

    const double goal_w = ServerParam::i().goalHalfWidth();
    const double pitch_x = ServerParam::i().pitchHalfLength();
    const Rect2D rect = Rect2D::from_corners( std::max( pitch_x - 15.5, teammate_ball_pos.x - 10.0 ),
                                              -goal_w,
                                              pitch_x,
                                              +goal_w );
#ifdef DEBUG_PRINT_TARGET_POINT
    dlog.addMessage( Logger::TEAM, rect.topLeft(), "SH:Attack" );
    dlog.addRect( Logger::TEAM, rect, "#00F" );
#endif

    bool exist_other_attacker = false;
    int teammate_count = 0;
    for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(),
              end = wm.teammates().end();
          t != end;
          ++t )
    {
        if ( (*t)->posCount() > 10 ) continue;
        if ( (*t)->isGhost() ) continue;
        if ( *t == first_teammate ) continue;

        if (  rect.contains( (*t)->pos() ) )
        {
            ++teammate_count;
        }
    }
    if ( teammate_count >= 2 )
    {
        exist_other_attacker = true;
        dlog.addText( Logger::TEAM,
                      __FILE__":(get_target_point_2013) exist other attackers. no adjust for support" );
    }

    //
    // get the best point
    //
    if ( ! exist_other_attacker )
    {
        const double base_x = bound( home_pos.x, teammate_ball_pos.x, 49.0 );
        const double y_step = goal_w * 2.0 / 4.0;
        double best_score = -1000000.0;
        Vector2D best_pos = home_pos;
        for ( double y = -goal_w; y < goal_w + 1.0; y += y_step )
        {
            Vector2D pos( base_x, y );
            double opponent_dist = wm.getDistOpponentNearestTo( pos, 10 );
            double ball_dist = pos.dist( teammate_ball_pos );
            double ball_dist_penalty = 0.0;
            if ( ball_dist > 14.0 )
            {
                ball_dist_penalty = -3.0 * ( ball_dist - 14.0 );
            }
            else if ( ball_dist < 3.0 )
            {
                ball_dist_penalty = -5.0 * ( 3.0 - ball_dist );
            }

            double shoot_bonus = 0.0;
            if ( ShootSimulator::can_shoot_from( true, pos, wm.theirPlayers(), 10 ) )
            {
                shoot_bonus += 10.0;
            }

            double score = opponent_dist + ball_dist_penalty + shoot_bonus;
            if ( score > best_score )
            {
                best_score = score;
                best_pos = pos;
            }

#ifdef DEBUG_PRINT_TARGET_POINT
            dlog.addText( Logger::TEAM,
                          "__ pos=(%.2f %.2f) score=%f (odist_val=%.3f bdist_penalty=%.3f shoot_val=%.1f)",
                          pos.x, pos.y, score, opponent_dist, ball_dist_penalty, shoot_bonus );
            dlog.addRect( Logger::TEAM,
                          pos.x - 0.05, pos.y - 0.05, 0.1, 0.1, "#0F0" );
            char buf[8]; snprintf( buf, 8, "%.3f", score );
            dlog.addMessage( Logger::TEAM, pos, buf );
#endif
        }

#ifdef DEBUG_PRINT_TARGET_POINT
        dlog.addRect( Logger::TEAM,
                      best_pos.x - 0.08, best_pos.y - 0.08, 0.16, 0.16, "#FF0" );
#endif
        dlog.addText( Logger::TEAM,
                      __FILE__":(get_target_point_2013) no other attacker. updated target=(%.2f %.2f)->(%.2f %.2f)",
                      target_point.x, target_point.y,
                      best_pos.x, best_pos.y );
        target_point = best_pos;
    }

    //
    // avoid opponent
    //
    if ( target_point.x > 36.0 )
    {
        double opponent_dist = 200.0;
        const PlayerObject * opponent = wm.getOpponentNearestTo( target_point,
                                                                 10,
                                                                 &opponent_dist );
        if ( opponent
             && opponent_dist < 3.0 )
        {
            Vector2D new_target = target_point;
            for ( int i = 1; i <= 3; ++i )
            {
                Vector2D pos = target_point;
                pos.x -= 1.0 * i;

                double d = 0.0;
                opponent = wm.getOpponentNearestTo( pos, 10, &d );
                if ( ! opponent )
                {
                    opponent_dist = 0.0;
                    new_target = pos;
                    break;
                }

                if ( opponent
                     && opponent_dist < d )
                {
                    opponent_dist = d;
                    new_target = pos;
                }
            }
            dlog.addText( Logger::TEAM,
                          __FILE__":(get_target_point_2013) avoid opponent (%.2f, %.2f)->(%.2f, %.2f)",
                          target_point.x, target_point.y,
                          new_target.x, new_target.y );
            target_point = new_target;
        }
    }

    //
    // avoid ball owner
    //
    if ( target_point.dist2( teammate_ball_pos ) < std::pow( 6.0, 2 ) )
    {
        Circle2D target_circle( teammate_ball_pos, 6.0 );
        Line2D target_line( target_point, AngleDeg( 90.0 ) );
        Vector2D sol1, sol2;
        int n_sol = target_circle.intersection( target_line, &sol1, &sol2 );

        Vector2D new_target = target_point;
        if ( n_sol == 1 )
        {
            new_target = sol1;
        }
        else if ( n_sol == 2 )
        {
            new_target = ( wm.self().pos().dist2( sol1 ) < wm.self().pos().dist2( sol2 )
                           ? sol1
                           : sol2 );
        }

        dlog.addText( Logger::TEAM,
                      __FILE__":(get_target_point_2012) avoid ball owner. (%.2f %.2f)->(%.2f %.2f)",
                      target_point.x, target_point.y,
                      new_target.x, new_target.y );
        target_point = new_target;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(get_target_point_2013) result=(%.2f %.2f)",
                  target_point.x, target_point.y );
    return target_point;
}
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SideHalfCrossMove::getTargetPoint( PlayerAgent * agent )
{
    return get_target_point_2013( agent->world() );
    //return get_target_point_2012( agent->world() );
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_SideHalfCrossMove::getAvoidOffsidePosition( const WorldModel & wm,
                                                const Vector2D & old_target )
{
    if ( wm.self().pos().x < wm.offsideLineX() - 0.5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (getAvoidOffsidePosition) no offside position" );
        return old_target;
    }

    const int teammate_step = wm.interceptTable()->teammateReachCycle();
    const int opponent_step = wm.interceptTable()->opponentReachCycle();

    if ( opponent_step <= teammate_step - 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (getAvoidOffsidePosition) opponent ball" );
        return old_target;
    }

    Vector2D ball_pos = wm.ball().inertiaPoint( teammate_step );
    Segment2D move_segment( wm.self().pos(), old_target );
    double line_dist = move_segment.dist( ball_pos );

    if ( line_dist > ServerParam::i().offsideActiveAreaSize() + 1.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (getAvoidOffsidePosition) safety line."
                      " target=(%.2f %.2f) balll(%.2f %.2f) line_dist=%.2f",
                      old_target.x, old_target.y,
                      ball_pos.x, ball_pos.y,
                      line_dist );
        return old_target;
    }

    Vector2D new_target = old_target;

    // TODO: find the optimal path

    if ( wm.self().pos().y < ball_pos.y )
    {
        new_target.y = ball_pos.y - ( ServerParam::i().offsideActiveAreaSize() + 1.0 );
    }
    else
    {
        new_target.y = ball_pos.y + ( ServerParam::i().offsideActiveAreaSize() + 1.0 );
    }


    dlog.addText( Logger::TEAM,
                  __FILE__": (getAvoidOffsidePosition) old=(%.2f %.2f) new=(%.2f %.2f)",
                  old_target.x, old_target.y,
                  new_target.x, new_target.y );

    return new_target;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_SideHalfCrossMove::getDashPower( PlayerAgent * agent,
                                     const Vector2D & target_point )
{
    static bool s_recover_mode = false;

    const WorldModel & wm = agent->world();

    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        return std::min( ServerParam::i().maxPower(),
                         wm.self().stamina() + wm.self().playerType().extraStamina() );
    }

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const Vector2D ball_pos = wm.ball().inertiaPoint( mate_min );

    if ( wm.self().pos().x > 35.0
         && wm.self().stamina() < ServerParam::i().recoverDecThrValue() + 500.0 )
    {
        s_recover_mode = true;
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) recover on" );
    }

    if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.5 )
    {
        s_recover_mode = false;
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) recover off" );
    }

    if ( ball_pos.dist2( ServerParam::i().theirTeamGoalPos() )
         < std::pow( 20.0, 2 ) )
    {
        s_recover_mode = false;
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) chance recover off" );
    }

    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        s_recover_mode = false;
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) no stamina capacity. recover off" );
    }

    double dash_power = ServerParam::i().maxDashPower();
    if ( s_recover_mode )
    {
        const double my_inc
            = wm.self().playerType().staminaIncMax()
            * wm.self().recovery();
        dash_power = std::max( 1.0, my_inc - 25.0 );
        //dash_power = wm.self().playerType().staminaIncMax() * 0.6;
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) recover mode" );
    }
    else if ( wm.ball().pos().x > wm.self().pos().x )
    {
        if ( wm.kickableTeammate()
             && wm.ball().distFromSelf() < 10.0
             && std::fabs( wm.self().pos().x - wm.ball().pos().x ) < 5.0
             && wm.self().pos().x > 30.0
             && wm.ball().pos().x > 35.0 )
        {
            dash_power *= 0.5;
            dlog.addText( Logger::TEAM,
                          __FILE__": (getDashPower) near teammate kicker" );
        }
    }
    else if ( wm.self().pos().dist( target_point ) < 3.0 )
    {
        const double my_inc
            = wm.self().playerType().staminaIncMax()
            * wm.self().recovery();
        dash_power = std::min( ServerParam::i().maxDashPower(),
                               my_inc + 10.0 );
        //dash_power = ServerParam::i().maxDashPower() * 0.8;
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) close target" );
    }
    else if ( mate_min <= 1
              && wm.ball().pos().x > 33.0
              && wm.ball().pos().absY() < 7.0
              && wm.ball().pos().x < wm.self().pos().x
              && wm.self().pos().x < wm.offsideLineX()
              && wm.self().pos().absY() < 9.0
              && std::fabs( wm.ball().pos().y - wm.self().pos().y ) < 3.5
              && std::fabs( target_point.y - wm.self().pos().y ) > 5.0 )
    {
        dash_power = wm.self().playerType().getDashPowerToKeepSpeed( 0.3, wm.self().effort() );
        dash_power = std::min( ServerParam::i().maxDashPower() * 0.75,
                               dash_power );
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) slow for cross. power=%.1f",
                      dash_power );
    }

    return dash_power;
}
