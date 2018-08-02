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

#include "bhv_center_forward_move.h"

#include "strategy.h"
#include "field_analyzer.h"

#include "bhv_cross_move.h"
#include "bhv_block_ball_owner.h"
#include "neck_offensive_intercept_neck.h"

#include "generator_center_forward_free_move.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/arm_point_to_point.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/soccer_intention.h>
#include <rcsc/player/world_model.h>
#include <rcsc/color/thermo_color_provider.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/time/timer.h>
#include <rcsc/types.h>

// #define USE_PASS_REQUEST_MOVE

// #define DEBUG_PROFILE
// #define DEBUG_PRINT
// #define DEBUG_PAINT_FREE_MOVE_POINT
// #define DEBUG_PRINT_EVAL


using namespace rcsc;

namespace {


/*-------------------------------------------------------------------*/
/*!

 */
bool
PassRequestMove_checkOpponent( const int count,
                               const WorldModel & wm,
                               const Vector2D & first_ball_pos,
                               const Vector2D & receive_pos,
                               const double first_ball_speed,
                               const int ball_move_step )
{
    const ServerParam & SP = ServerParam::i();

    const Vector2D first_ball_vel =( receive_pos - first_ball_pos ).setLengthVector( first_ball_speed );
#ifdef DEBUG_PRINT_PASS_REQUEST
    dlog.addText( Logger::TEAM,
                  "%d: ball_vel=(%.3f %.3f) speed=%.3f angle=%.1f ball_step=%d",
                  count,
                  first_ball_vel.x, first_ball_vel.y, first_ball_speed, ball_move_angle.degree(),
                  ball_move_step );
#else
    (void)count;
#endif

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromBall().begin(), end = wm.opponentsFromBall().end();
          o != end;
          ++o )
    {
        const PlayerType * ptype = (*o)->playerTypePtr();

        Vector2D ball_pos = first_ball_pos;
        Vector2D ball_vel = first_ball_vel;

        for ( int step = 1; step <= ball_move_step; ++step )
        {
            ball_pos += ball_vel;
            ball_vel *= SP.ballDecay();

            const  double control_area = ( ( *o)->goalie()
                                           && ball_pos.x > SP.theirPenaltyAreaLineX()
                                           && ball_pos.absY() < SP.penaltyAreaHalfWidth()
                                           ? ptype->reliableCatchableDist() + 0.1
                                           : ptype->kickableArea() + 0.1 );
            const double ball_dist = (*o)->pos().dist( ball_pos );
            double dash_dist = ball_dist;
            dash_dist -= control_area;
            dash_dist -= 0.15;
            //dash_dist -= ptype->realSpeedMax() * std::min( 5.0, (*o)->posCount() * 0.8 );

            int opponent_turn = 1;
            int opponent_dash = ptype->cyclesToReachDistance( dash_dist );
            int opponent_step = opponent_turn + opponent_dash;
            opponent_step -= std::min( 3, (*o)->posCount() );

#ifdef DEBUG_PRINT_PASS_REQUEST_OPPONENT
            dlog.addText( Logger::TEAM,
                          "%d: opponent %d (%.1f %.1f) dash=%d move_dist=%.1f",
                          count,
                          (*o)->unum(), (*o)->pos().x, (*o)->pos().y,
                          opponent_dash, dash_dist );
#endif


            if ( opponent_step < step )
            {
#ifdef DEBUG_PRINT_PASS_REQUEST_OPPONENT
                dlog.addText( Logger::TEAM,
                              "%d: exist reachable opponent %d (%.1f %.1f)",
                              count,
                              (*o)->unum(), (*o)->pos().x, (*o)->pos().y );
#endif
                return true;
            }
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
get_teammate_reach_step( const WorldModel & wm )
{
    const PlayerObject * t = wm.getTeammateNearestToBall( 5 );
    if ( t
         && t->distFromBall() < ( t->playerTypePtr()->kickableArea()
                                  + t->distFromSelf() * 0.05
                                  + wm.ball().distFromSelf() * 0.05 ) )
    {
        return 0;
    }

    return wm.interceptTable()->teammateReachStep();
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
PassRequestMove_getReceivePos( const WorldModel & wm,
                               int * result_self_move_step,
                               int * result_offside_reach_step,
                               int * result_ball_move_step )
{
    static GameTime s_cache_time( -1, 0 );
    static Vector2D s_cache_pos = Vector2D::INVALIDATED;
    static int s_cache_self_move_step = 1000;
    static int s_cache_offside_reach_step = 1000;
    static int s_cache_ball_move_step = 1000;

    if ( s_cache_time == wm.time() )
    {
        *result_self_move_step = s_cache_self_move_step;
        *result_offside_reach_step = s_cache_offside_reach_step;
        *result_ball_move_step = s_cache_ball_move_step;
        return s_cache_pos;
    }
    s_cache_time = wm.time();


    const ServerParam & SP = ServerParam::i();
    const Vector2D goal_pos = SP.theirTeamGoalPos();

    const int teammate_step = get_teammate_reach_step( wm );
    const Vector2D first_ball_pos = wm.ball().inertiaPoint( teammate_step );
    const Vector2D self_pos = wm.self().inertiaFinalPoint();

    int count = 0;

    Vector2D best_point = Vector2D::INVALIDATED;
    int best_self_move_step = 1000;
    int best_offside_reach_step = -1;
    int best_ball_move_step = 1000;
    double best_point_goal_dist2 = 10000000.0;

    //
    // angle loop
    //
    for ( double a = -45.0; a < 46.0; a += 6.0 )
    {
        const Vector2D unit_vec = Vector2D::from_polar( 1.0, a );

        //
        // distance loop
        //
        double dist_step = 3.0;
        for ( double d = 10.0; d < 40.0; d += dist_step, dist_step += 0.5 )
        {
            const Vector2D receive_pos = self_pos + ( unit_vec * d );
            const double goal_dist2 = receive_pos.dist2( goal_pos );

            ++count;
#ifdef DEBUG_PRINT_PASS_REQUEST
            dlog.addText( Logger::TEAM,
                          "%d: receive_pos=(%.1f %.1f) angle=%.0f dist=%.1f dist_step=%.1f goal_dist=%.1f",
                          count, receive_pos.x, receive_pos.y, a, d, dist_step, std::sqrt( goal_dist2 ) );
#endif

            // check range
            if ( receive_pos.x < wm.offsideLineX()
                 && receive_pos.x < SP.pitchLength() / 6.0 )
            {
#ifdef DEBUG_PRINT_PASS_REQUEST
                dlog.addText( Logger::TEAM,
                              "%d: xxx not a through pass / not attacking third area.", count );
#endif
                continue;
            }

            // check exsiting candidate
            if ( best_point_goal_dist2 < goal_dist2 )
            {
#ifdef DEBUG_PRINT_PASS_REQUEST
                dlog.addText( Logger::TEAM,
                              "%d: xxx already exist better candidate.", count );
#endif
                continue;
            }

            if ( receive_pos.x > SP.pitchHalfLength() - 2.0
                 || receive_pos.absY() > SP.pitchHalfWidth() - 2.0 )
            {
                // exit distance loop
#ifdef DEBUG_PRINT_PASS_REQUEST
                dlog.addText( Logger::TEAM,
                              "%d: xxx out of bounds", count );
#endif
                break;
            }

//             // check other receivers
//             if ( PassRequestMove_checkOtherReceiver( count, wm, receive_pos ) )
//             {
// #ifdef DEBUG_PRINT_PASS_REQUEST
//                 dlog.addText( Logger::TEAM,
//                               "%d: xxx exist other receiver candidate.", count );
// #endif
//                 continue;
//             }

            //
            // estimate required time steps
            //

            int offside_reach_step = -1;
            int self_move_step = 1000;
            if ( receive_pos.x > wm.offsideLineX() )
            {
                const Segment2D self_move_line( self_pos, receive_pos );
                const Line2D offside_line( Vector2D( wm.offsideLineX(), -100.0 ),
                                           Vector2D( wm.offsideLineX(), +100.0 ) );
                const Vector2D offside_line_pos = self_move_line.intersection( offside_line );
                if ( offside_line_pos.isValid() )
                {
                    StaminaModel stamina_model = wm.self().staminaModel();
                    offside_reach_step = FieldAnalyzer::predict_self_reach_cycle( wm,
                                                                                  offside_line_pos,
                                                                                  0.1,
                                                                                  0, // wait cycle
                                                                                  true,
                                                                                  &stamina_model );
                    offside_reach_step -= 1; // need to decrease 1 step
                }
            }
            {
                const int wait_cycle = ( ( offside_reach_step >= 0
                                           && offside_reach_step <= teammate_step + 1 )
                                         ? ( teammate_step + 2 ) - offside_reach_step
                                         : 0 );
                StaminaModel stamina_model = wm.self().staminaModel();

                self_move_step = FieldAnalyzer::predict_self_reach_cycle( wm,
                                                                          receive_pos,
                                                                          1.0, // dist_thr
                                                                          wait_cycle, // wait cycle
                                                                          true, // save_recovery
                                                                          &stamina_model );
            }

            const double ball_move_dist = first_ball_pos.dist( receive_pos );
            int ball_move_step = std::max( 1, self_move_step - ( teammate_step + 1 ) );
            double first_ball_speed = SP.firstBallSpeed( ball_move_dist, ball_move_step );

            if ( first_ball_speed > SP.ballSpeedMax() )
            {
#ifdef DEBUG_PRINT_PASS_REQUEST
                dlog.addText( Logger::TEAM,
                              "%d: over the max speed %.3f",
                              count, first_ball_speed );
#endif
                first_ball_speed = SP.ballSpeedMax();
                ball_move_step = SP.ballMoveStep( first_ball_speed, ball_move_dist );
            }

#ifdef DEBUG_PRINT_PASS_REQUEST
            dlog.addText( Logger::TEAM,
                          "%d: self_move_step=%d ball_move_step=%d teammate_step=%d first_ball_speed=%.2f",
                          count, self_move_step, ball_move_step, teammate_step, first_ball_speed );
#endif
            if ( ball_move_step < 1 )
            {
#ifdef DEBUG_PRINT_PASS_REQUEST
                dlog.addText( Logger::TEAM,
                              "%d: xxx illegal ball move step %d", count, ball_move_step );
#endif
                continue;
            }
#ifdef DEBUG_PRINT_PASS_REQUEST
            char msg[8];
            snprintf( msg, 8, "%d", count );
            dlog.addMessage( Logger::TEAM,
                             receive_pos, msg );
            dlog.addRect( Logger::TEAM,
                          receive_pos.x - 0.1, receive_pos.y - 0.1, 0.2, 0.2,
                          "#F00" );
#endif

            // check other receivers
            if ( PassRequestMove_checkOpponent( count, wm, first_ball_pos, receive_pos,
                                                first_ball_speed,  ball_move_step ) )
            {
#ifdef DEBUG_PRINT_PASS_REQUEST
                dlog.addText( Logger::TEAM,
                              "%d: xxx opponent will intercept the ball.", count );
#endif
                continue;
            }

#ifdef DEBUG_PRINT_PASS_REQUEST
            dlog.addText( Logger::TEAM,
                          "%d: OK receive=(%.1f %.1f)", count,
                          receive_pos.x, receive_pos.y );
            dlog.addMessage( Logger::TEAM,
                             receive_pos, msg );
            dlog.addRect( Logger::TEAM,
                          receive_pos.x - 0.1, receive_pos.y - 0.1, 0.2, 0.2,
                          "#00F", true );
#endif
            best_point = receive_pos;
            best_self_move_step = self_move_step;
            best_offside_reach_step = offside_reach_step;
            best_ball_move_step = ball_move_step;
            best_point_goal_dist2 = goal_dist2;
        }
    }

    if ( best_point.isValid() )
    {
        s_cache_pos = best_point;
        s_cache_self_move_step = best_self_move_step;
        s_cache_offside_reach_step = best_offside_reach_step;
        s_cache_ball_move_step = best_ball_move_step;

        *result_self_move_step = best_self_move_step;
        *result_offside_reach_step = best_offside_reach_step;
        *result_ball_move_step = best_ball_move_step;

#if 1
        // #ifdef DEBUG_PRINT_PASS_REQUEST
        dlog.addText( Logger::TEAM,
                      __FILE__":(getReceivePos) target=(%.1f %.1f) self_step=%d offside_step=%d ball_step=%d",
                      best_point.x, best_point.y,
                      best_self_move_step, best_offside_reach_step, best_ball_move_step );
        dlog.addRect( Logger::TEAM,
                      best_point.x - 0.1, best_point.y - 0.1, 0.2, 0.2,
                      "#0FF", true );
#endif
    }
    else
    {
        s_cache_pos = Vector2D::INVALIDATED;
        s_cache_self_move_step = 1000;
        s_cache_offside_reach_step = 1000;
        s_cache_ball_move_step = 1000;
    }

    return best_point;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
is_intercept_situation( const WorldModel & wm )
{
    if ( wm.kickableTeammate() )
    {
        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opponent_step = wm.interceptTable()->opponentReachStep();
    const PlayerObject * first_teammate = wm.interceptTable()->fastestTeammate();

    if ( first_teammate
         && opponent_step >= 5
         && self_min >= mate_min
         && first_teammate->distFromSelf() < 0.75 )
    {
        Vector2D ball_pos = wm.ball().inertiaPoint( mate_min );
        double self_dist = wm.self().pos().dist( ball_pos );
        double teammate_dist = first_teammate->pos().dist( ball_pos );
        if ( teammate_dist < self_dist * 0.8
             || teammate_dist < self_dist - 0.3 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": intercept cancel. self_dist=%.3f teammate_dist=%.3f",
                          self_dist, teammate_dist );
            return false;
        }
    }

    if ( self_min >= 3 )
    {
        Vector2D ball_pos = wm.ball().inertiaPoint( self_min );
        if ( ball_pos.x < 36.0
             && ( wm.kickableOpponent()
                  || opponent_step <= self_min - 5 ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(is_intercept_situation) false: opponent has ball" );
            return false;
        }
    }

    if ( ( 2 <= mate_min
           && self_min <= 4 )
         || ( self_min <= mate_min + 1
              && 4 <= mate_min ) )
    {
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
class IntentionCenterForwardMove
    : public SoccerIntention {
private:
    const Vector2D M_target_point;
    GameTime M_last_time;
    int M_ball_holder_unum;
    int M_count;
    int M_offside_count;
    int M_their_ball_count;
public:

    IntentionCenterForwardMove( const Vector2D & target_point,
                                const GameTime & start_time,
                                const int ball_holder_unum )
        : M_target_point( target_point ),
          M_last_time( start_time ),
          M_ball_holder_unum( ball_holder_unum ),
          M_count( 0 ),
          M_offside_count( 0 ),
          M_their_ball_count( 0 )
      { }

    bool finished( const PlayerAgent * agent );
    bool execute( PlayerAgent * agent );
};


/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionCenterForwardMove::finished( const PlayerAgent * agent )
{
    //if ( ++M_count > 13 )
    if ( ++M_count >= 5 ) // 2014-07-18
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(IntentionCenterForwardMove::finished) no remained step" );
        return true;
    }

    const WorldModel & wm = agent->world();

    if ( wm.time().cycle() - 1 != M_last_time.cycle() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(IntentionCenterForwardMove::finished) time mismatch" );
        return true;
    }

    if ( wm.audioMemory().passTime() == wm.time()
         && ! wm.audioMemory().pass().empty()
         && ( wm.audioMemory().pass().front().receiver_ == wm.self().unum() )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(IntentionCenterForwardMove::finished) hear pass message" );
        return true;
    }

    // if ( wm.self().pos().x > wm.offsideLineX() + 1.0 )
    // {
    //     dlog.addText( Logger::TEAM,
    //                   __FILE__":(IntentionCenterForwardMove::finished) too over offside line" );
    //     return true;
    // }

    if ( wm.self().pos().x > wm.offsideLineX() )
    {
        ++M_offside_count;
        //if ( M_offside_count >= 5 )
        if ( M_offside_count >= 3 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(IntentionCenterForwardMove::finished) over offside line count" );
            return true;
        }
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( opp_min < mate_min )
    {
        ++M_their_ball_count;
        dlog.addText( Logger::TEAM,
                      __FILE__":(IntentionCenterForwardMove::finished) their ball count = %d",
                      M_their_ball_count );
        if ( M_their_ball_count >= 3 )
        {
            return true;
        }
    }

    if ( ! wm.kickableTeammate()
         && self_min <= mate_min )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(IntentionCenterForwardMove::finished) my ball" );
        return true;
    }

    // if ( M_ball_holder_unum != Unum_Unknown
    //      && wm.interceptTable()->fastestTeammate()
    //      && wm.interceptTable()->fastestTeammate()->unum() != M_ball_holder_unum )
    // {
    //     dlog.addText( Logger::TEAM,
    //                   __FILE__":(IntentionCenterForwardMove::finished) ball holder changed." );
    //     return true;
    // }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    if ( home_pos.x > M_target_point.x + 5.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(IntentionCenterForwardMove::finished) home pos x >> target x." );
        return true;
    }

    const Vector2D my_pos = wm.self().inertiaFinalPoint();
    if ( my_pos.dist2( M_target_point ) < 1.5 )
    {
        if ( Bhv_CenterForwardMove::is_marked( wm ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(IntentionCenterForwardMove::finished) exist marker." );
            return true;
        }
    }

#ifdef USE_PASS_REQUEST_MOVE
    // 2012-06-23
    if ( wm.self().pos().x < wm.offsideLineX() - 0.5
         && wm.self().pos().x < 20.0 )
    {
        int self_move_step = 1000;
        int offside_reach_step = 1000;
        int ball_move_step = 1000;
        Vector2D receive_pos = PassRequestMove_getReceivePos(  wm,
                                                               &self_move_step,
                                                               &offside_reach_step,
                                                               &ball_move_step );
        if ( receive_pos.isValid() )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(IntentionCenterForwardMove::finished) found pass request point." );
            return true;
        }
    }
#endif

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionCenterForwardMove::execute( PlayerAgent * agent )
{
    const double dash_power = ServerParam::i().maxDashPower();

    M_last_time = agent->world().time();

    dlog.addText( Logger::TEAM,
                  __FILE__": (intention::execute) target=(%.1f %.1f) power=%.1f",
                  M_target_point.x, M_target_point.y,
                  dash_power );

    agent->debugClient().addMessage( "CFMove:Intention%d", M_count );

    Bhv_CenterForwardMove::go_to_point( agent, M_target_point, dash_power );
    Bhv_CenterForwardMove::set_turn_neck( agent );

    return true;
}

}

/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterForwardMove::execute( PlayerAgent * agent )
{
    if ( doIntercept( agent ) )
    {
        return false;
    }

    if ( doBlock( agent ) )
    {
        return true;
    }

    // if ( doCrossMove( agent ) )
    // {
    //     return true;
    // }

#ifdef USE_PASS_REQUEST_MOVE
    // 2012-06-23
    if ( doPassRequestMove( agent ) )
    {
        return true;
    }
#endif

    if ( Strategy::i().opponentType() == Strategy::Type_Gliders
         || Strategy::i().opponentType() == Strategy::Type_WrightEagle
         )
    {
        // nothing to do
    }
    else
    {
        if ( doFreeMove( agent ) )
        {
            return true;
        }
    }

    doNormalMove( agent );

    if ( agent->world().self().armMovable() == 0
         && agent->world().self().armExpires() > 0 )
    {
        agent->debugClient().addMessage( "CF:ArmOff" );
        agent->setArmAction( new Arm_Off() );
    }
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterForwardMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( is_intercept_situation( wm ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doIntercept)" );
        agent->debugClient().addMessage( "CFMove:Intercept" );

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
bool
Bhv_CenterForwardMove::doBlock( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    const Vector2D opponent_ball_pos = wm.ball().inertiaPoint( opp_min );
    const double teammate_dist = wm.getDistTeammateNearestTo( opponent_ball_pos, 5 );

    if ( //wm.self().stamina() > ServerParam::i().staminaMax() * 0.6 &&
        opp_min < 3
         && opp_min < mate_min - 2
         && ( opp_min <= self_min - 5
              || opp_min == 0 )
         && ( wm.ball().pos().dist( home_pos ) < 10.0
              || teammate_dist > wm.self().pos().dist( opponent_ball_pos ) )
         && wm.ball().distFromSelf() < 15.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doBlock) try. self=%d, mate=%d, opp=%d opp_ball_pos=(%.2f %.2f)",
                      self_min, mate_min, opp_min,
                      opponent_ball_pos.x, opponent_ball_pos.y );

        const AbstractPlayerObject * other_blocker
            = FieldAnalyzer::get_blocker( wm, opponent_ball_pos );
        if ( other_blocker )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (doBlock) other blocker. try intercept" );
            if ( Body_Intercept( true ).execute( agent ) )
            {
                agent->debugClient().addMessage( "CFMove:Block:Intercept" );
                agent->setNeckAction( new Neck_TurnToBall() );
                return true;
            }
        }

        Rect2D bounding_rect = Rect2D::from_center( home_pos, 40.0, 50.0 );
        if ( Bhv_BlockBallOwner( new Rect2D( bounding_rect ) ).execute( agent ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (doBlock) block ball owner" );
            //agent->debugClient().addMessage( "Attack" );
            return true;
        }
        dlog.addText( Logger::TEAM,
                      __FILE__": (doBlock) failed" );

        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (doBlock) no block situation" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterForwardMove::doCrossMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.offsideLineX() < ServerParam::i().theirPenaltyAreaLineX() + 3.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doCrossMove) offside line < penalty line" );
        return false;
    }

    int mate_min = wm.interceptTable()->teammateReachCycle();
    Vector2D ball_pos = wm.ball().inertiaPoint( mate_min );

    if ( ball_pos.x < 20.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doCrossMove) ball.x xxx" );
        return false;
    }

    return Bhv_CrossMove().execute( agent );
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterForwardMove::doPassRequestMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    dlog.addText( Logger::TEAM,
                  __FILE__": (doPassRequestMove) start" );

    if ( wm.self().pos().x > wm.offsideLineX() - 0.5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doPassRequestMove) avoid offside" );
        return false;
    }

    // 2012-06-21
    //if ( wm.self().pos().x > 20.0 )
    if ( wm.self().pos().x > 36.0 ) // 2014-07-15
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doPassRequestMove) too front" );
        return false;
    }

    const int teammate_step = wm.interceptTable()->teammateReachCycle();
    const int opponent_step = wm.interceptTable()->opponentReachCycle();

    if ( teammate_step > opponent_step )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doPassRequestMove) their ball?" );
        const PlayerObject * t =  wm.teammatesFromBall().front();
        if ( t->distFromBall()
             < t->playerTypePtr()->kickableArea()
             + t->distFromSelf() * 0.05
             + wm.ball().distFromSelf() * 0.05 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (doPassRequestMove) maybe teammate kickable" );
        }
        else
        {
            return false;
        }
    }

#ifdef DEBUG_PRINT_PASS_REQUEST
    dlog.addText( Logger::TEAM,
                  __FILE__": (doPassRequestMove) start" );
#endif

    int self_move_step = 1000;
    int offside_reach_step = -1;
    int ball_move_step = 1000;
    const Vector2D receive_pos = PassRequestMove_getReceivePos( wm,
                                                                &self_move_step,
                                                                &offside_reach_step,
                                                                &ball_move_step );

    if ( ! receive_pos.isValid() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doPassRequestMove) target point not found." );
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (doPassRequestMove) pos=(%.1f %.1f) self_move_step=%d offside_step=%d teammate_step=%d ball_move_step=%d",
                  receive_pos.x, receive_pos.y,
                  self_move_step, offside_reach_step, teammate_step, ball_move_step );
#ifdef DEBUG_PAINT_REQUESTED_PASS_COURSE
    const Vector2D first_ball_pos = wm.ball().inertiaPoint( teammate_step );
    dlog.addLine( Logger::TEAM,
                  first_ball_pos, receive_pos, "#0F0" );
#endif

    if ( offside_reach_step >= 0
         && offside_reach_step <= teammate_step )
    {
        const Vector2D my_inertia = wm.self().inertiaPoint( teammate_step + 1 );
        // if ( my_inertia.x > wm.offsideLineX() - 0.5 )
        // {
        //     Vector2D avoid_offside_pos = wm.self().pos();
        //     avoid_offside_pos.x -= 1.0;
        //     agent->debugClient().addMessage( "SH:PassRequest:WaitOffside:Back" );
        //     dlog.addText( Logger::TEAM,
        //                   __FILE__": (doPassRequestMove) go to back" );
        //     Bhv_SideHalfOffensiveMove::go_to_point( agent,
        //                                             avoid_offside_pos,
        //                                             ServerParam::i().maxDashPower() );
        // } else
        if ( my_inertia.x > wm.offsideLineX() - 0.8 )
        {
            Body_TurnToPoint( receive_pos ).execute( agent );
            agent->debugClient().addMessage( "CF:PassRequest:WaitOffside:Turn" );
            dlog.addText( Logger::TEAM,
                          __FILE__": (doPassRequestMove) turn to" );
        }
        else
        {
            double required_speed = ( wm.offsideLineX() - 0.8 - wm.self().pos().x )
                * ( 1.0 - wm.self().playerType().playerDecay() )
                / ( 1.0 - std::pow( wm.self().playerType().playerDecay(), teammate_step + 3 ) );
            double dash_power = std::min( ServerParam::i().maxDashPower(),
                                          required_speed / wm.self().dashRate() );
            agent->debugClient().addMessage( "CF:PassRequest:WaitOffside:Move%.0f", dash_power );
            dlog.addText( Logger::TEAM,
                          __FILE__": (doPassRequestMove) adjust. x_move=%.3f required_speed=%.3f dash_power=%.1f",
                          wm.offsideLineX() - 0.8 - wm.self().pos().x,
                          required_speed,
                          dash_power );
            go_to_point( agent, receive_pos, dash_power );
        }
        set_turn_neck( agent );
        //agent->setArmAction( new Arm_PointToPoint( receive_pos ) );
        return true;
    }

    // if ( self_move_step < ball_move_step )
    // {
    //     agent->debugClient().addMessage( "SHOffMove:PassRequestWaitKick" );
    //     dlog.addText( Logger::TEAM,
    //                   __FILE__": (doPassRequestMove) wait" );
    //     Body_TurnToPoint( receive_pos ).execute( agent );
    //     Bhv_SideHalfOffensiveMove::set_turn_neck( agent );
    //     return true;
    // }

    double dash_power = ServerParam::i().maxDashPower();
    if ( wm.self().pos().x > wm.offsideLineX() - 1.5 )
    {
        dash_power *= 0.8;
    }

    if ( receive_pos.x > wm.offsideLineX()
         && ( teammate_step <= 0
              || wm.kickableTeammate() ) )
    {
        AngleDeg target_angle = ( receive_pos - wm.self().inertiaFinalPoint() ).th();
        if ( ( target_angle - wm.self().body() ).abs() < 25.0 )
        {
            dash_power = 0.0;

            const Vector2D accel_vec = Vector2D::from_polar( 1.0, wm.self().body() );
            for ( double p = ServerParam::i().maxDashPower(); p > 0.0; p -= 10.0 )
            {
                Vector2D self_vel = wm.self().vel() + accel_vec * ( p * wm.self().dashRate() );
                Vector2D self_next = wm.self().pos() + self_vel;

                if ( self_next.x < wm.offsideLineX() - 0.5 )
                {
                    dash_power = p;
                    break;
                }
            }
        }
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (doPassRequestMove) go to point. dash_power=%.1f", dash_power );

    agent->debugClient().addMessage( "CF:PassRequest%.0f", dash_power );


    go_to_point( agent, receive_pos, dash_power );
    set_turn_neck( agent );
    agent->setArmAction( new Arm_PointToPoint( receive_pos ) );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterForwardMove::doFreeMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.6 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doFreeMove) insufficient stamina" );
        return false;
    }

    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( opp_min < mate_min - 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doFreeMove) no free move situation" );
        return false;
    }

    if ( mate_min > 3 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doFreeMove) wait teammate trap time" );
        return false;
    }

    if ( wm.self().pos().x > wm.offsideLineX() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doFreeMove) over offside line" );
        return false;
    }

    if ( ! Bhv_CenterForwardMove::is_marked( wm ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doFreeMove) no marker" );
        return false;
    }

    if ( ! Bhv_CenterForwardMove::is_triangle_member( wm ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doFreeMove) no triangle member" );
        return false;
    }

    const int ball_holder_unum = ( wm.interceptTable()->fastestTeammate()
                                   ? wm.interceptTable()->fastestTeammate()->unum()
                                   : Unum_Unknown );

    const Vector2D target_point = GeneratorCenterForwardFreeMove::instance().getBestPoint( wm );
    //const Vector2D target_point = getFreeMoveTargetGrid( wm );
    //const Vector2D target_point = getFreeMoveTarget( wm );

    if ( ! target_point.isValid() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doFreeMove) no target found" );
        return false;
    }

    agent->debugClient().addMessage( "CFMove:FreeMove" );

    Bhv_CenterForwardMove::go_to_point( agent, target_point, ServerParam::i().maxDashPower() );
    Bhv_CenterForwardMove::set_turn_neck( agent );

    agent->setIntention( new IntentionCenterForwardMove( target_point, wm.time(), ball_holder_unum ) );
    //agent->setArmAction( new Arm_PointToPoint( target_point ) );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_CenterForwardMove::getFreeMoveTargetVoronoi( const WorldModel & wm )
{
    static GameTime s_update_time( -1, 0 );
    static std::vector< Vector2D > s_candidates;
    static Vector2D s_best_pos( 0.0, 0.0 );

    if ( s_update_time == wm.time() )
    {
        return s_best_pos;
    }
    s_update_time = wm.time();
    s_candidates.clear();
    s_best_pos = Vector2D::INVALIDATED;

#ifdef DEBUG_PROFILE
    MSecTimer timer;
#endif

    FieldAnalyzer::i().positioningVoronoiDiagram().getPointsOnSegments( 2.0, // min length
                                                                        10, // max division
                                                                        &s_candidates );
    if ( s_candidates.empty() )
    {
        return s_best_pos;
    }

    const double home_dist_thr2 = std::pow( 15.0, 2 );
    const double ball_dist_thr2 = std::pow( 5.0, 2 );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D ball_pos = wm.ball().inertiaPoint( wm.interceptTable()->teammateReachCycle() );

    const double offside_line_bonus = std::max( 0.0, ( ball_pos.x - wm.ball().pos().x ) * 0.5 );
    const double max_x = std::max( ball_pos.x + 1.0, wm.offsideLineX() + offside_line_bonus );
    const double min_x = home_pos.x - 8.0;

    Vector2D best_pos = Vector2D::INVALIDATED;
    double best_score = -1000000.0;
    int count = 0;
    for ( std::vector< Vector2D >::const_iterator p = s_candidates.begin(),
              end = s_candidates.end();
          p != end;
          ++p )
    {
        const Vector2D & pos = *p;

        if ( pos.x > max_x ) continue;
        if ( pos.x < min_x ) continue;
        if ( pos.dist2( home_pos ) > home_dist_thr2 )
        {
            continue;
        }
        if ( pos.dist2( ball_pos ) < ball_dist_thr2 ) continue;

        ++count;
        double score = evaluate_free_move_point( count, wm, home_pos, ball_pos, *p );

#ifdef DEBUG_PAINT_FREE_MOVE_POINT
        dlog.addRect( Logger::TEAM,
                      pos.x - 0.1, pos.y - 0.1, 0.2, 0.2,
                      "#F00" );
        char msg[16];
        snprintf( msg, 16, "%d:%.3f", count, score );
        dlog.addMessage( Logger::TEAM,
                         *p, msg );
#endif
        if ( score > best_score )
        {
#ifdef DEBUG_PRINT_EVAL
            dlog.addText( Logger::TEAM,
                          ">> update" );
#endif
            best_pos = *p;
            best_score = score;
        }
    }

#ifdef DEBUG_PRINT_EVAL
    dlog.addRect( Logger::TEAM,
                  best_pos.x - 0.1, best_pos.y - 0.1, 0.2, 0.2,
                  "#0F0", true );
#endif

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::TEAM,
                  __FILE__": (getFreeMoveTarget) size=%d. elapsed %f [ms]",
                  s_candidates.size(), timer.elapsedReal() );
#endif
    s_best_pos = best_pos;
    return s_best_pos;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_CenterForwardMove::getFreeMoveTargetGrid( const WorldModel & wm )
{
    static GameTime s_update_time( -1, 0 );
    static Vector2D s_best_pos( 0.0, 0.0 );

    if ( s_update_time == wm.time() )
    {
        return s_best_pos;
    }
    s_update_time = wm.time();

#ifdef DEBUG_PROFILE
    MSecTimer timer;
#endif

    const double x_step = 1.0;
    const double y_step = 1.0;
    //const double max_x = ServerParam::i().pitchHalfLength() - 0.5;
    //const double max_y = ServerParam::i().pitchHalfWidth() - 0.5;
    const double max_y = ServerParam::i().penaltyAreaHalfWidth() - 1.0;
    //const double ball_dist_thr2 = std::pow( 10.0, 2 );
    const double ball_dist_thr2 = std::pow( 2.0, 2 );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D ball_pos = wm.ball().inertiaPoint( wm.interceptTable()->teammateReachCycle() );

    double max_x = std::max( ball_pos.x, wm.offsideLineX() );
// #if 1
//     if ( ball_pos.x > wm.offsideLineX() )
//     {
//         max_x = std::min( ball_pos.x + 5.0, ServerParam::i().pitchHalfLength() - 2.0 );
//     }
// #endif


    Vector2D best_pos = home_pos;
    double best_score = -1000000.0;
    double min_score = +1000000.0;
    double max_score = -1000000.0;

#ifdef DEBUG_PAINT_FREE_MOVE_POINT
    std::vector< std::pair< Vector2D, double > > points;
    points.reserve( 8 * 15 );
#endif

    int count = 0;
    for ( int ix = -2; ix <= 5; ++ix )
    {
        for ( int iy = -7; iy <= 7; ++iy )
        {
            Vector2D move_pos( home_pos.x + x_step*ix,
                               home_pos.y + y_step*iy );
            if ( move_pos.absX() > max_x
                 || move_pos.absY() > max_y
                 || move_pos.dist2( ball_pos ) < ball_dist_thr2 )
            {
                continue;
            }

            ++count;

            double score = evaluate_free_move_point( count, wm, home_pos, ball_pos, move_pos );

#ifdef DEBUG_PAINT_FREE_MOVE_POINT
            points.push_back( std::pair< Vector2D, double >( move_pos, score ) );
#endif
            if ( score < min_score ) min_score = score;
            if ( score > max_score ) max_score = score;

            if ( score > best_score )
            {
                best_pos = move_pos;
                best_score = score;
            }
        }
    }

#ifdef DEBUG_PAINT_FREE_MOVE_POINT
    ThermoColorProvider color;
    count = 1;
    for ( std::vector< std::pair< Vector2D, double > >::const_iterator it = points.begin();
          it != points.end();
          ++it, ++count )
    {
        char msg[16]; snprintf( msg, 16, "%d:%.3f", count, it->second );
        RGBColor c = color.convertToColor( ( it->second - min_score ) / ( max_score - min_score ) );
        dlog.addRect( Logger::TEAM,
                      it->first.x - 0.1, it->first.y - 0.1, 0.2, 0.2,
                      c.name().c_str(), true );
        dlog.addMessage( Logger::TEAM,
                         it->first.x + 0.1, it->first.y + 0.1,
                         msg );

    }

    dlog.addRect( Logger::TEAM,
                  best_pos.x - 0.2, best_pos.y - 0.2, 0.4, 0.4,
                  "#0F0", false );
#endif
#ifdef DEBUG_PROFILE
    dlog.addText( Logger::TEAM,
                  __FILE__": (getFreeMoveTarget) size=%d. elapsed %f [ms]",
                  count, timer.elapsedReal() );
#endif
    s_best_pos = best_pos;
    return best_pos;
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
        if ( (*p)->isGhost() ) continue;
        if ( (*p)->posCount() >= 4 ) continue;
        if ( (*p)->distFromSelf() > 10.0 ) break;

        double d2 = (*p)->pos().dist2( pos );
        if ( d2 > std::pow( 5.0, 2 ) ) continue;

        value += 1.0 / d2;
    }

    return value;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_CenterForwardMove::doNormalMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    Vector2D target_point = home_pos;

#if 0
    // 2016-06-24
    if ( Strategy::i().opponentType() == Strategy::Type_Gliders )
    {
        const int t_step = wm.interceptTable()->opponentReachStep();
        const int o_step = wm.interceptTable()->opponentReachStep();
        if ( t_step > 1
             && o_step > 1
             && wm.ball().vel().x < 0.2 )
        {
            // TODO: estimate gliders's offside line

            if ( wm.self().pos().x > wm.offsideLineX() - 2.0 )
            {
                target_point.x = wm.offsideLineX() - 2.0;
                dlog.addText( Logger::TEAM,
                              __FILE__":(doNormalMove) avoid offside for gliders (%.2f, %.2f)",
                              target_point.x, target_point.y );
            }
        }
    }
#endif

    // 2012-06-22
    if ( wm.self().pos().dist2( target_point ) < std::pow( 3.0, 2 ) )
    {
        Vector2D best_point = target_point;
        double best_congestion = get_congestion( wm, target_point );
        dlog.addText( Logger::TEAM,
                      __FILE__":(doNormalMove) initial target (%.2f, %.2f)",
                      target_point.x, target_point.y );
        const double offside_buf = ( Strategy::i().opponentType() == Strategy::Type_Gliders
                                     ? 0.8
                                     : 0.5 );

        for ( int a = 0; a < 8; ++a )
        {
            Vector2D pos = home_pos + Vector2D::from_polar( 1.0, (360.0/8)*a );
            if ( pos.x > wm.offsideLineX() - offside_buf ) continue;

            double congestion = get_congestion( wm, pos );
            if ( congestion < best_congestion )
            {
                best_point = pos;
                best_congestion = congestion;
                dlog.addText( Logger::TEAM,
                              __FILE__":(doNormalMove) change target to (%.2f, %.2f)",
                              pos.x, pos.y );
            }
        }
        target_point = best_point;
    }

    const double dash_power = Bhv_CenterForwardMove::get_dash_power( wm, target_point );

    dlog.addText( Logger::TEAM,
                  __FILE__": (doNormalMove) target=(%.1f %.1f) power=%.1f",
                  target_point.x, target_point.y,
                  dash_power );

    agent->debugClient().addMessage( "CFMove:Normal%.0f", dash_power );

    Bhv_CenterForwardMove::go_to_point( agent, target_point, dash_power );
    Bhv_CenterForwardMove::set_turn_neck( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_CenterForwardMove::go_to_point( PlayerAgent * agent,
                                    const Vector2D & target_point,
                                    const double dash_power )
{
    //
    // TODO: avoid ball holder.
    //

    const WorldModel & wm = agent->world();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    // const Vector2D mate_trap_pos = wm.ball().inertiaPoint( mate_min );

    // double dist_thr = std::fabs( wm.ball().pos().x - wm.self().pos().x ) * 0.2 + 0.25;
    // if ( dist_thr < 1.0 ) dist_thr = 1.0;
    // if ( target_point.x > wm.self().pos().x - 0.5
    //      && wm.self().pos().x < wm.offsideLineX()
    //      && std::fabs( target_point.x - wm.self().pos().x ) > 1.0 )
    // {
    //     dist_thr = std::min( 1.0, wm.ball().pos().dist( target_point ) * 0.1 + 0.5 );
    // }
    double dist_thr = std::min( 1.0, wm.ball().pos().dist( target_point ) * 0.1 + 0.5 );

    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );


    const Vector2D my_inertia = wm.self().inertiaPoint( mate_min );

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

        agent->debugClient().addMessage( "CFMove:Back%.0f", back_dash_power );
        dlog.addText( Logger::ROLE,
                      __FILE__":(go_to_point) Back Move. power=%.1f", back_dash_power );

        agent->doDash( back_dash_power );
        return;
    }

    double adjusted_dash_power = dash_power;

    if ( target_point.x > wm.offsideLineX() )
    {
        const Vector2D inertia_pos = wm.self().inertiaFinalPoint();
        AngleDeg target_angle = ( target_point - inertia_pos ).th();
        if ( ( target_angle - wm.self().body() ).abs() < 25.0 )
        {
            adjusted_dash_power = 0.0;

            const Vector2D accel_vec = Vector2D::from_polar( 1.0, wm.self().body() );
            for ( double p = dash_power; p > 0.0; p -= 10.0 )
            {
                Vector2D self_vel = wm.self().vel() + accel_vec * ( p * wm.self().dashRate() );
                Vector2D self_next = wm.self().pos() + self_vel;
                if ( self_next.x < wm.offsideLineX() - 0.5 )
                {
                    adjusted_dash_power = p;
                    dlog.addText( Logger::ROLE,
                                  __FILE__":(go_to_point) adjust dash power %.1f -> %.1f",
                              dash_power, adjusted_dash_power );
                    break;
                }
            }
        }
    }
    dlog.addText( Logger::ROLE,
                  __FILE__":(go_to_point) target=(%.2f %.2f) power=%.1f",
                  target_point.x, target_point.y, adjusted_dash_power );

    if ( Body_GoToPoint( target_point, dist_thr, adjusted_dash_power,
                         -1.0, // dash speed
                         100, // cycle
                         true, // save recovery
                         25.0 // angle threshold
                         ).execute( agent ) )
    {
        agent->debugClient().addMessage( "CFMove:GoTo%.0f", adjusted_dash_power );
        return;
    }

    AngleDeg body_angle( 0.0 );
    Body_TurnToAngle( body_angle ).execute( agent );

    agent->debugClient().addMessage( "CFMove:Wait" );
    dlog.addText( Logger::ROLE,
                  __FILE__":(go_to_point) turn to angle=%.1f",
                  body_angle.degree() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_CenterForwardMove::set_turn_neck( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const Vector2D mate_trap_pos = wm.ball().inertiaPoint( mate_min );

    int count_thr = 0;

    if ( ( wm.kickableTeammate()
           || mate_min <= 2 )
         && wm.self().pos().dist( mate_trap_pos ) < 20.0 )
    {
        count_thr = ( wm.time().cycle() % 3 == 0
                      ? 0
                      : -1 );
        dlog.addText( Logger::TEAM,
                      __FILE__": (set_turn_neck) ball or scan. count_thr=%d", count_thr );
        agent->setNeckAction( new Neck_TurnToBallOrScan( count_thr ) );
        return;
    }

    int opp_min = wm.interceptTable()->opponentReachCycle();
    ViewWidth view_width = agent->effector().queuedNextViewWidth();
    int see_cycle = agent->effector().queuedNextSeeCycles();

    count_thr = 0;
    if ( view_width.type() == ViewWidth::WIDE )
    {
        if ( opp_min > see_cycle )
        {
            count_thr = 2;
        }
    }
    else if ( view_width.type() == ViewWidth::NORMAL )
    {
        if ( opp_min > see_cycle )
        {
            count_thr = 1;
        }
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( count_thr ) );
    dlog.addText( Logger::TEAM,
                  __FILE__":(set_turn_neck) ball or scan. opp_min=%d see_cycle=%d count_thr=%d",
                  opp_min, see_cycle, count_thr );
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_CenterForwardMove::get_dash_power( const WorldModel & wm,
                                       const Vector2D & target_point )
{
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
                      __FILE__": (get_dash_power) chance. fast move" );
        return ServerParam::i().maxDashPower();
    }

    if  ( wm.self().pos().x > wm.offsideLineX()
          && ( wm.kickableTeammate()
               || mate_min <= opp_min + 2 )
          && target_point.x < receive_pos.x + 20.0
          && wm.self().pos().dist( receive_pos ) < 30.0
          && wm.self().pos().dist( target_point ) < 10.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_dash_power) offside max power" );

        return ServerParam::i().maxDashPower();
    }

    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_dash_power) empty capacity. max power" );

        return ServerParam::i().maxDashPower();
    }

    //------------------------------------------------------
    // decide dash power
    static bool s_recover_mode = false;

    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_dash_power) no stamina capacity. never recover mode" );
        s_recover_mode = false;
    }
    else if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.4 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_dash_power) change to recover mode." );
        s_recover_mode = true;
    }
    else if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.7 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_dash_power) came back from recover mode" );
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
                      __FILE__": (get_dash_power) recover mode" );
        //return std::max( 0.0, my_inc - 30.0 );
        return 1.0;
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
                      __FILE__": (get_dash_power) exist near opponent. full power" );
        return ServerParam::i().maxDashPower();
    }

    // 2016-06-24
    if ( Strategy::i().opponentType() == Strategy::Type_Gliders
         && wm.self().pos().x > wm.offsideLineX() - 1.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_dash_power) avoid offside for gliders" );
        return ServerParam::i().maxDashPower();
    }

    if  ( wm.ball().pos().x < wm.self().pos().x
          && wm.self().pos().x < wm.offsideLineX() - 0.5 )
    {
        // ball is back
        // not offside
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_dash_power) ball is back and not offside." );
#if 1
        // 2013-06-28
        if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.4
             && wm.offsideLineX() > 45.0
             && mate_min <= opp_min - 2
             && receive_pos.dist2( ServerParam::i().theirTeamGoalPos() ) < std::pow( 32.0, 2 ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (get_dash_power) still chance. full power" );
            return ServerParam::i().maxDashPower();
        }
#endif

        if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.8 )
        {
            return std::min( std::max( 5.0, my_inc - 40.0 ),
                             ServerParam::i().maxDashPower() );
            //return 0.1;
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
                              __FILE__": (get_dash_power) ball is front. recover" );
                // return std::min( std::max( 0.1, my_inc - 30.0 ),
                //                  ServerParam::i().maxDashPower() );
                return 1.0;
            }
            else if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.8 )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": (get_dash_power) ball is front. keep" );
                //return std::min( my_inc, ServerParam::i().maxDashPower() );
                return my_inc * 0.9;
            }
            else
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": (get_dash_power) ball is front. max" );
                return ServerParam::i().maxDashPower();
            }
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (get_dash_power) ball is front full powerr" );
            return ServerParam::i().maxDashPower();
        }
    }


    if ( target_point.x < wm.self().pos().x
         && wm.self().pos().x > wm.offsideLineX() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_dash_power) normal mode: offside" );
        return ServerParam::i().maxDashPower();
    }
    else if ( target_point.x > wm.self().pos().x + 2.0
              && wm.self().stamina() > ServerParam::i().staminaMax() * 0.6 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_dash_power) normal mode: stamina>60%" );
        return ServerParam::i().maxDashPower();
    }
    else if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.8 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_dash_power) normal mode: stamina<80%" );
        return std::min( my_inc * 0.9,
                         ServerParam::i().maxDashPower() );
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (get_dash_power) normal mode" );
    return std::min( my_inc * 1.5,
                     ServerParam::i().maxDashPower() );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterForwardMove::is_marked( const WorldModel & wm )
{
    //
    // TODO: compare with previous analysis.
    // if same player is detected within marker poisition continuously in several times,
    // that player would be the marker.
    //

    static GameTime s_update_time( 0, 0 );
    static bool s_result = false;
    if ( s_update_time == wm.time() )
    {
        return s_result;
    }
    s_update_time = wm.time();

    const double mark_dist_thr2 = std::pow( 2.0, 2 );
    const double mark_line_dist_thr2 = std::pow( 4.0, 2 );
    // const double mark_x_thr = 10.0;
    // const double mark_y_thr = 3.0;

    const Vector2D my_pos = wm.self().inertiaFinalPoint();
    const Vector2D ball_pos = wm.ball().inertiaPoint( wm.interceptTable()->teammateReachCycle() );

    const Sector2D front_space_sector( Vector2D( my_pos.x - 5.0, my_pos.y ),
                                       4.0, 20.0,
                                       -15.0, 15.0 );

    dlog.addSector( Logger::TEAM,
                    front_space_sector, "#F00" );

    for ( PlayerObject::Cont::const_iterator p = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          p != end;
          ++p )
    {
        if ( (*p)->distFromSelf() > 15.0 ) break;
        if ( (*p)->ghostCount() >= 5 ) continue;

        const Vector2D opos = (*p)->pos() + (*p)->vel();

        double d2 = opos.dist2( my_pos );
        if ( d2 < mark_dist_thr2 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (is_marked) found (1) marker %d (%.1f %.1f)",
                          (*p)->unum(), (*p)->pos().x, (*p)->pos().y );
            s_result = true;
            return true;
        }

        // opponent exists on my front space
        if ( ! (*p)->goalie()
             && my_pos.x > wm.offsideLineX() - 10.0
             && front_space_sector.contains( opos )
             // && std::fabs( opos.y - my_pos.y ) < mark_y_thr
             // && my_pos.x < opos.x
             // && opos.x < my_pos.x + mark_x_thr
             )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (is_marked) found (2) marker %d (%.1f %.1f)",
                          (*p)->unum(), (*p)->pos().x, (*p)->pos().y );
            s_result = true;
            return true;
        }

        // opponent exist on pass line
        if ( d2 < mark_line_dist_thr2
             && opos.x > my_pos.x - 1.0
             && ( opos.y - my_pos.y ) * ( ball_pos.y - my_pos.y ) > 0.0 )
        {
            AngleDeg ball_to_self = ( my_pos - ball_pos ).th();
            AngleDeg ball_to_opp = ( (*p)->pos() - ball_pos ).th();

            if ( ( ball_to_self - ball_to_opp ).abs() < 10.0 )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": (is_marked) found (3) marker %d (%.1f %.1f)",
                              (*p)->unum(), (*p)->pos().x, (*p)->pos().y );
                s_result = true;
                return true;
            }
        }
    }

    s_result = false;
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_CenterForwardMove::is_triangle_member( const WorldModel & wm )
{
    const AbstractPlayerObject * first_teammate = wm.interceptTable()->fastestTeammate() ;

    if ( ! first_teammate )
    {
        return false;
    }

    const AbstractPlayerObject * self_ptr = &(wm.self());

    const PlayerGraph & graph = FieldAnalyzer::instance().ourPlayersGraph();

    for ( std::vector< PlayerGraph::Connection >::const_iterator it = graph.connections().begin(),
              end = graph.connections().end();
          it != end;
          ++it )
    {
        if ( ( it->first_->player_ == first_teammate
               && it->second_->player_ == self_ptr )
             || ( it->first_->player_ == self_ptr
                  && it->second_->player_ == first_teammate ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (is_triangle_member) found edge" );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_CenterForwardMove::evaluate_free_move_point( const int count,
                                                 const WorldModel & wm,
                                                 const Vector2D & home_pos,
                                                 const Vector2D & ball_pos,
                                                 const Vector2D & move_pos )
{
    const double var_factor_opponent_dist = ( home_pos.x < 20.0
                                              ? 1.0 / ( 2.0 * std::pow( 5.0, 2 ) )
                                              : 1.0 / ( 2.0 * std::pow( 2.0, 2 ) ) );
    const double var_factor_teammate_dist = 1.0 / ( 2.0 * std::pow( 3.0, 2 ) );
    // const double var_factor_angle = 1.0 / ( 2.0 * std::pow( 10.0, 2 ) );

    const Sector2D front_space_sector( Vector2D( move_pos.x - 5.0, move_pos.y ),
                                       4.0, 20.0,
                                       -15.0, 15.0 );

#ifndef DEBUG_PRINT_EVAL
    (void)count;
#endif

#ifdef DEBUG_PRINT_EVAL
    dlog.addText( Logger::TEAM,
                  "%d: (evaluate) pos=(%.1f %.1f) ball_to_pos_angle=%.1f",
                  count,
                  move_pos.x, move_pos.y,
                  ( move_pos - ball_pos ).th().degree() );
#endif

    double opp_dist_score = 0.0;
    double front_space_score = 0.0;

    if ( home_pos.x > 30.0 )
    {
        double opponent_dist = wm.getDistOpponentNearestTo( move_pos, false );
        opp_dist_score = opponent_dist;
    }
    else
    {
        for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
                  end = wm.opponentsFromSelf().end();
              o != end;
              ++o )
        {
            double opp_d2 = std::max( 0.001, (*o)->pos().dist2( move_pos ) );
            opp_dist_score += -std::exp( -opp_d2 * var_factor_opponent_dist ) * 2.0;
            front_space_score += ( ! (*o)->goalie()
                                   && front_space_sector.contains( (*o)->pos() )
                                   ? -1.0
                                   : 0.0 );

#ifdef DEBUG_PRINT_EVAL
            dlog.addText( Logger::TEAM,
                          "%d: __opponent[%d] dist=%.2f(%f) space=%.1f",
                          count,
                          (*o)->unum(),
                          std::sqrt( opp_d2 ), -std::exp( -opp_d2 * var_factor_opponent_dist ),
                          ( ! (*o)->goalie()
                            && front_space_sector.contains( (*o)->pos() )
                            ? -1.0 : 0.0 ) );
#endif
        }
    }

    //const double my_move_dist2 = wm.self().pos().dist2( move_pos );

    double teammate_dist_score = 0.0;
    for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromSelf().begin(),
              end = wm.teammatesFromSelf().end();
          t != end;
          ++t )
    {
        double d2 = std::max( 0.001, (*t)->pos().dist2( move_pos ) );
        teammate_dist_score += -std::exp( -d2 * var_factor_teammate_dist );
    }

    double ball_ydiff_score = 0.0;
    {
        double ball_ydiff = std::fabs( ball_pos.y - move_pos.y );
        if ( ball_ydiff > 10.0 )
        {
            ball_ydiff_score = std::exp( - std::pow( ball_ydiff - 10.0, 2 )
                                         / ( 2.0 * std::pow( 10.0, 2 ) ) );
            ball_ydiff_score -= 1.0;
        }
    }

    //
    // - pass count
    // - distance from current position
    // - distance from home position
    // - distance from the ball
    // - ...
    //
    //const double base_score = 100.0 - ServerParam::i().theirTeamGoalPos().dist( move_pos );
    //const double base_score = move_pos.x;
#if 1
    const double base_score = move_pos.x * 0.1;
#else
    const double base_score = 0.1 * std::max( 0.0, 10.0 - home_pos.dist( move_pos ) );
#endif
    double score = base_score;

    score += opp_dist_score;
#if 0
    score += front_space_score;
    score += teammate_dist_score;
    score += ball_ydiff_score;
#endif
    // double home_pos_rate = std::exp( - home_pos.dist2( move_pos ) / ( 2.0 * std::pow( 20.0, 2 ) ) );
    // score -= home_pos_rate;


#ifdef DEBUG_PRINT_EVAL
    dlog.addText( Logger::TEAM,
                  "%d: === score %f(%f) | opp_dist=%f space=%f teammate_dist=%f"
                  " ball_ydiff=%f"
                  //"home_rate=%f"
                  ,
                  count,
                  score, base_score,
                  opp_dist_score,
                  front_space_score,
                  teammate_dist_score,
                  ball_ydiff_score
                  //home_pos_rate
                  );
#endif

    return score;
}
