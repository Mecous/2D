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

#include "bhv_cross_move.h"

#include "strategy.h"
#include "field_analyzer.h"
#include "shoot_simulator.h"
#include "simple_pass_checker.h"

#include "bhv_basic_move.h"
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

#include <rcsc/geom/sector_2d.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>

// #define DEBUG_PRINT
// #define DEBUG_PRINT_CROSS_CONE

// #define TEST_GENERATOR_CROSS_MOVE
// #include "generator_cross_move.h"

using namespace rcsc;

namespace {

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

    //----------------------------------------------
    // intercept check
    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();

    if ( 1 < mate_min
         && self_min <= mate_min )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (is_intercept_situation) ok (1)" );
        return true;
    }

    if ( 3 <= mate_min
         && self_min <= mate_min + 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (is_intercept_situation) ok (2)" );
        return true;
    }

    if ( 5 <= mate_min
         && self_min <= mate_min + 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (is_intercept_situation) ok (3)" );
        return true;
    }

    if ( 2 <= mate_min
         && self_min <= 6 )
    {
        const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
        if ( wm.ball().pos().dist2( home_pos ) < std::pow( 10.0, 2 ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (is_intercept_situation) ok (4)" );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
class IntentionCrossMove
    : public SoccerIntention {
private:
    const Vector2D M_target_point;
    const Vector2D M_first_ball_pos;
    GameTime M_last_execute_time;
    int M_stay_counter;
    int M_over_offside_count;
    int M_total_counter;
public:

    IntentionCrossMove( const Vector2D & target_point,
                        const Vector2D & first_ball_pos,
                        const GameTime & start_time )
        : M_target_point( target_point ),
          M_first_ball_pos( first_ball_pos ),
          M_last_execute_time( start_time ),
          M_stay_counter( 0 ),
          M_over_offside_count( 0 ),
          M_total_counter( 0 )
      { }

    bool finished( const PlayerAgent * agent );

    bool execute( PlayerAgent * agent );

};

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionCrossMove::finished( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( M_stay_counter >= 2 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished). count over" );
        return true;
    }

    if ( M_total_counter >= 10 )
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

    dlog.addText( Logger::TEAM,
                  __FILE__": (finished). first ball pos (%.2f %.2f)",
                  M_first_ball_pos.x, M_first_ball_pos.y );

    if ( wm.ball().pos().dist2( M_first_ball_pos ) > std::pow( 2.0, 2 ) ) // magic number
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished). ball moved" );
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

    if ( ! wm.kickableTeammate()
         && ( wm.kickableOpponent()
              || opp_min < mate_min - 2 ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished) opponent gets the ball" );
        return true;
    }

    const Vector2D ball_pos = wm.ball().inertiaPoint( mate_min );
    const double offside_line_bonus = std::max( 0.0, ball_pos.x - wm.ball().pos().x );
    const double max_x = std::max( ball_pos.x, wm.offsideLineX() + offside_line_bonus );

    if ( wm.self().pos().x > max_x )
    {
        if ( ++M_over_offside_count >= 2 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (finished). over offside or trap line" );
            return true;
        }
    }

    if ( wm.self().pos().dist2( M_target_point ) < std::pow( 1.0, 2 ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (finished). already there" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionCrossMove::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

#ifdef TEST_GENERATOR_CROSS_MOVE
    (void)GeneratorCrossMove::instance().bestPoint( wm );
#endif

    M_last_execute_time = wm.time();
    ++M_total_counter;

    double dist_thr = 0.5;

    agent->debugClient().addMessage( "CrossMove%d", M_total_counter );

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();

    if ( ! wm.kickableTeammate()
         && self_min <= mate_min + 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": intention execute. intercept" );
        Vector2D face_point( ServerParam::i().pitchHalfLength(), 0.0 );
        Body_Intercept( true, face_point ).execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        return true;
    }

    Vector2D actual_target = M_target_point;
// #if 1
//     // 2009-07-05
//     if ( actual_target.x > wm.offsideLineX() - 0.2 )
//     {
//         Line2D dash_line( wm.self().inertiaFinalPoint(), actual_target );
//         actual_target.x = wm.offsideLineX() - 0.2;
//         actual_target.y = dash_line.getY( actual_target.x );
//     }
// #endif

    agent->debugClient().setTarget( actual_target );
    agent->debugClient().addCircle( actual_target, dist_thr );

    dlog.addText( Logger::TEAM,
                  __FILE__": intention execute. target=(%.2f, %.2f) actual=(%.2f %.2f)",
                  M_target_point.x, M_target_point.y,
                  actual_target.x, actual_target.y );

    if ( ! Body_GoToPoint( actual_target, dist_thr,
                           ServerParam::i().maxDashPower(),
                           -1.0, // dash speed
                           1, // cycle
                           true, // stamina save
                           20.0 // angle threshold
                           ).execute( agent ) )
    {
        //Body_TurnToBall().execute( agent );
        Vector2D face_point = ServerParam::i().theirTeamGoalPos();
        face_point.y = ( face_point.y + wm.self().pos().y ) * 0.5;
        Body_TurnToPoint( face_point ).execute( agent );

        ++M_stay_counter;
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
/*!

*/
bool
Bhv_CrossMove::execute( PlayerAgent * agent )
{
    if ( doIntercept( agent ) )
    {
        return true;
    }

    // if ( doFastMove( agent ) )
    // {
    //     return true;
    // }

    if ( doGoToSpace( agent ) )
    {
        return true;
    }

    if ( doGetBall( agent ) )
    {
        return true;
    }

    doNormalMove( agent );

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
Bhv_CrossMove::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( ! is_intercept_situation( wm ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doIntercept) no intercept situation" );
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (doIntercept) try intercept" );
    agent->debugClient().addMessage( "Cross:Intercept" );

    Body_Intercept().execute( agent );

    if ( wm.self().pos().x > 30.0
         && wm.self().pos().absY() < 20.0 )
    {
        const int self_min = wm.interceptTable()->selfReachCycle();
        const int opp_min = wm.interceptTable()->opponentReachCycle();

        if ( self_min == 3 && opp_min >= 3 )
        {
            agent->setViewAction( new View_Normal() );
        }

        // if ( ! doCheckCrossPoint( agent ) )
        {
            agent->setNeckAction( new Neck_TurnToGoalieOrScan( 0 ) );
        }
    }
    else
    {
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_CrossMove::doFastMove( PlayerAgent * agent )
{
    //
    // TODO
    //

    (void)agent;
    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_CrossMove::doGoToSpace( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

#ifdef TEST_GENERATOR_CROSS_MOVE
    (void)GeneratorCrossMove::instance().bestPoint( wm );
#endif

    // check ball owner

    if ( mate_min > 5
         && mate_min >= opp_min + 3 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doGoToSpace) no our ball" );
        return false;
    }

    if ( mate_min >= 5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doGoToSpace) wait teammate interception" );
        return false;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    // check home position range
    if ( wm.self().pos().dist( home_pos ) > 15.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doGoToSpace) far home position" );
        return false;
    }

    // stamina check
    const double available_stamina = wm.self().stamina() - SP.recoverDecThrValue();
    if ( available_stamina < wm.self().playerType().getOneStepStaminaComsumption() * 20 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doGoToSpace) no enough stamina" );
        return false;
    }

    const Vector2D ball_pos = wm.ball().inertiaPoint( mate_min );

    if ( SP.theirTeamGoalPos().dist2( ball_pos ) > std::pow( 40.0, 2 ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doGoToSpace) no chance situation" );
        return false;
    }

    if ( std::fabs( ball_pos.y - home_pos.y ) > 30.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doGoToSpace) big y difference %.1f ball_trap=(%.1f %.1f) target=(%.1f %.1f)",
                      std::fabs( ball_pos.y - home_pos.y ),
                      ball_pos.x, ball_pos.y,
                      home_pos.x, home_pos.y );
        return false;
    }

    const Vector2D target_point = getTargetPoint( agent );
    if ( ! target_point.isValid() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doGoToSpace) target not found" );
        return false;
    }

    const double dist_thr = 0.5;

    agent->debugClient().addMessage( "GoToSpace" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );
    dlog.addText( Logger::TEAM,
                  __FILE__": (doGoToSpace) target=(%.1f %.1f)",
                  target_point.x, target_point.y );

    if ( ! Body_GoToPoint( target_point, dist_thr,
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

    agent->setIntention( new IntentionCrossMove( target_point,
                                                 ball_pos,
                                                 wm.time() ) );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_CrossMove::doGetBall( PlayerAgent * agent )
{
    //
    // TODO
    //

    (void)agent;
    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_CrossMove::doNormalMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
    double dash_power = getDashPower( agent, target_point );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    double turn_thr = 30.0;

    agent->debugClient().addMessage( "GoToCross%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    dlog.addText( Logger::TEAM,
                  __FILE__": doGoToCross. to (%.2f, %.2f)",
                  target_point.x, target_point.y );

    if ( wm.self().pos().x > target_point.x + dist_thr
         && std::fabs( wm.self().pos().x - target_point.x ) < 3.0
         && wm.self().body().abs() < 10.0 )
    {
        dash_power = wm.self().getSafetyDashPower( -dash_power );

        agent->debugClient().addMessage( "Back%.0f", dash_power );
        dlog.addText( Logger::TEAM,
                      __FILE__":(doNormalMove) back move. dash_power=%.1f", dash_power );
        agent->doDash( dash_power );
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
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_CrossMove::getDashPower( PlayerAgent * agent,
                                  const Vector2D & target_point )
{
    static bool s_recover_mode = false;

    const WorldModel & wm = agent->world();

    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        return std::min( ServerParam::i().maxPower(),
                         wm.self().stamina() + wm.self().playerType().extraStamina() );
    }

    //
    // update recover mode flag
    //
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

    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        s_recover_mode = false;
        dlog.addText( Logger::TEAM,
                      __FILE__": (getDashPower) no stamina capacity. recover off" );
    }

    const int mate_min = wm.interceptTable()->teammateReachCycle();

    double dash_power = ServerParam::i().maxDashPower();
    if ( s_recover_mode )
    {
        const double my_inc
            = wm.self().playerType().staminaIncMax()
            * wm.self().recovery();
        dash_power = std::max( 1.0, my_inc - 25.0 );
        //dash_power = wm.self().playerType().staminaIncMax() * 0.6;
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

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_CrossMove::getTargetPoint( PlayerAgent * agent )
{
    static GameTime s_update_time( -1, 0 );
    static std::vector< Vector2D > s_candidates;
    static Vector2D s_best_point( 0.0, 0.0 );

    const WorldModel & wm = agent->world();

    if ( s_update_time == wm.time() )
    {
        return s_best_point;
    }
    s_update_time = wm.time();
    s_candidates.clear();
    s_best_point = Vector2D::INVALIDATED;

    FieldAnalyzer::i().positioningVoronoiDiagram().getPointsOnSegments( 2.0, // min length
                                                                        10, // max division
                                                                        &s_candidates );
    if ( s_candidates.empty() )
    {
        return s_best_point;
    }

    const double home_dist_thr2 = std::pow( 12.0, 2 );
    const double ball_dist_min_thr2 = std::pow( 3.0, 2 );
    const double ball_dist_max_thr2 = std::pow( 20.0, 2 );
    const double goal_dist_thr2 = std::pow( 20.0, 2 );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D ball_pos = wm.ball().inertiaPoint( wm.interceptTable()->teammateReachCycle() );
    const Vector2D goal_pos = ServerParam::i().theirTeamGoalPos();

    const double offside_line_bonus = std::max( 0.0, ball_pos.x - wm.ball().pos().x );
    const double max_x = std::max( ball_pos.x + 1.0, wm.offsideLineX() + offside_line_bonus );
    const double min_x = ServerParam::i().theirPenaltyAreaLineX() - 2.0;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM,
                  "home=(%.1f %.1f) ball=(%.1f %.1f)",
                  home_pos.x, home_pos.y,
                  ball_pos.x, ball_pos.y );
#endif

    Vector2D best_point = Vector2D::INVALIDATED;
    double best_score = -1000000.0;
    int count = 0;
    for ( std::vector< Vector2D >::const_iterator p = s_candidates.begin(), end = s_candidates.end();
          p != end;
          ++p )
    {
        if ( p->x > max_x ) continue;
        if ( p->x < min_x ) continue;
        if ( p->x < home_pos.x - 8.0 ) continue;
        if ( p->dist2( home_pos ) > home_dist_thr2 ) continue;
        const double ball_d2 = p->dist2( ball_pos );
        if ( ball_d2 < ball_dist_min_thr2 ) continue;
        if ( ball_d2 > ball_dist_max_thr2 ) continue;
        if ( p->dist2( goal_pos ) > goal_dist_thr2 ) continue;

        ++count;

        double score = evaluateTargetPoint( count, wm, home_pos, ball_pos, *p );
#ifdef DEBUG_PRINT
        dlog.addRect( Logger::TEAM,
                      p->x - 0.1, p->y - 0.1, 0.2, 0.2,
                      "#F00" );
        char msg[16];
        snprintf( msg, 16, "%d:%.3f", count, score );
        dlog.addMessage( Logger::TEAM,
                         *p, msg );
#endif
        if ( score > best_score )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          ">> update" );
#endif
            best_score = score;
            best_point = *p;
        }
    }

    // if ( count < 2 )
    // {
    //     // generate grid point
    // }

#ifdef DEBUG_PRINT
    dlog.addRect( Logger::TEAM,
                  best_point.x - 0.1, best_point.y - 0.1, 0.2, 0.2,
                  "#0F0", true );
#endif
#ifdef DEBUG_PROFILE
    dlog.addText( Logger::TEAM,
                  __FILE__": (getFreeMoveTarget) size=%d. elapsed %f [ms]",
                  s_candidates.size(), timer.elapsedReal() );
#endif
    s_best_point = best_point;

    return s_best_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_CrossMove::evaluateTargetPoint( const int count,
                                    const WorldModel & wm,
                                    const Vector2D & home_pos,
                                    const Vector2D & ball_pos,
                                    const Vector2D & move_pos )
{
    const double var_factor_opponent_dist = 1.0 / ( 2.0 * std::pow( 3.0, 2 ) );
    const double var_factor_teammate_dist = 1.0 / ( 2.0 * std::pow( 2.0, 2 ) );

#ifndef DEBUG_PRINT
    (void)count;
    (void)home_pos;
#endif

    double shoot_rate = 1.0;
    if ( ! ShootSimulator::can_shoot_from( true, move_pos, wm.theirPlayers(), 10 ) )
    {
        shoot_rate = 0.9;
    }

    const int opp_count = Bhv_CrossMove::opponents_in_cross_cone( wm, ball_pos, move_pos );
    double cross_rate = std::pow( 0.9, opp_count );

    double opponent_rate = 1.0;
    if ( ! wm.opponentsFromSelf().empty() )
    {
        for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
                  end = wm.opponentsFromSelf().end();
              o != end;
              ++o )
        {
            double d2 = std::max( 0.001, (*o)->pos().dist2( move_pos ) );
            double r = 1.0 - ( std::exp( -d2 * var_factor_opponent_dist )
                               * std::pow( 0.98, (*o)->posCount() ) );
            if ( r < opponent_rate )
            {
                opponent_rate = r;
            }
        }
    }

    double teammate_rate = 1.0;
    if ( ! wm.teammatesFromSelf().empty() )
    {
        for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromSelf().begin(),
                  end = wm.teammatesFromSelf().end();
              t != end;
              ++t )
        {
            double d2 = (*t)->pos().dist2( move_pos );
            double r = 1.0 - std::exp( -d2 * var_factor_teammate_dist );
            if ( r < teammate_rate )
            {
                teammate_rate = r;
            }
        }
    }

    double home_dist_rate = std::exp( -home_pos.dist2( move_pos ) / ( 2.0 * std::pow( 4.0, 2 ) ) );
    double ball_dist_rate = std::exp( -ball_pos.dist2( move_pos ) / ( 2.0 * std::pow( 100.0, 2 ) ) );

    double score = 100.0 * shoot_rate * cross_rate * opponent_rate * teammate_rate * home_dist_rate * ball_dist_rate;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM,
                  "%d: ---> score %f | shoot=%f cross=%d(%f) opp=%f our=%f"
                  " home=%f ball=%f",
                  count,
                  score,
                  shoot_rate,
                  opp_count, cross_rate,
                  opponent_rate,
                  teammate_rate,
                  home_dist_rate,
                  ball_dist_rate );
#endif

    return score;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_CrossMove::opponents_in_cross_cone( const WorldModel & wm,
                                        const Vector2D & ball_pos,
                                        const Vector2D & move_pos )
{
    if ( wm.opponentsFromBall().empty() )
    {
        return 0;
    }

    const double ball_move_dist = ball_pos.dist( move_pos );

    if ( ball_move_dist > 20.0 )
    {
        return 1000;
    }

    const ServerParam & SP = ServerParam::i();

    const Vector2D ball_vel = ( move_pos - ball_pos ).setLengthVector( SP.ballSpeedMax() );
    const AngleDeg ball_move_angle = ball_vel.th();

#ifdef DEBUG_PRINT_CROSS_CONE
    dlog.addText( Logger::TEAM,
                  "(opponents_in_cross_cone) ball(%.1f %.1f) move_angle=%.1f",
                  ball_pos.x, ball_pos.y, ball_move_angle.degree() );
#endif

    int count = 0;
    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromBall().begin(),
              end = wm.opponentsFromBall().end();
          o != end;
          ++o )
    {
        if ( (*o)->ghostCount() >= 2 ) continue;
        if ( (*o)->posCount() >= 5 ) continue;

        const Vector2D opp_pos = (*o)->inertiaFinalPoint();
        const double opp_dist = opp_pos.dist( ball_pos );

        const double control_area = ( (*o)->goalie()
                                      ? SP.catchableArea()
                                      : (*o)->playerTypePtr()->kickableArea() );
        const double hide_radian = std::asin( std::min( control_area / opp_dist, 1.0 ) );
        double angle_diff = ( ( opp_pos - ball_pos ).th() - ball_move_angle ).abs();
        angle_diff = std::max( angle_diff - AngleDeg::rad2deg( hide_radian ), 0.0 );

        if ( angle_diff < 14.0 )
        {
            count += 1;
        }
#ifdef DEBUG_PRINT_CROSS_CONE
        dlog.addText( Logger::TEAM,
                      "__ opp %d (%.1f %.1f) angle_diff=%.1f",
                      (*o)->unum(), opp_pos.x, opp_pos.y, angle_diff );
#endif
    }

    return count;
}

#if 0
int
Bhv_CrossMove::can_cross( const WorldModel & wm,
                          const Vector2D & ball_pos,
                          const Vector2D & move_pos )
{
    if ( wm.opponentsFromBall().empty() )
    {
        return 0;
    }

    const double ball_move_dist = ball_pos.dist( move_pos );

    if ( ball_move_dist > 20.0 )
    {
        return 1000;
    }

    const ServerParam & SP = ServerParam::i();

    //const int ball_move_step = SP.ballMoveStep( SP.ballSpeedMax(), ball_move_dist );
    const Vector2D ball_vel = ( move_pos - ball_pos ).setLengthVector( SP.ballSpeedMax() );
    const AngleDeg ball_move_angle = ball_vel.th();

    double cross_course_cone = 360.0;
    for ( PlayerPtrCont::const_iterator o = wm.opponentsFromBall().begin(),
              o_end = wm.opponentsFromBall().end();
          o != o_end;
          ++o )
    {
        if ( (*o)->posCount() > 10 ) continue;
        if ( (*o)->pos().x < 30.0 ) continue;

        const double control_area = ( (*o)->goalie()
                                      ? SP.catchableArea() + 0.2
                                      : (*o)->playerTypePtr()->kickableArea() + 0.2 );
        const Vector2D opp_pos = (*o)->inertiaFinalPoint();
        const double opp_dist = opp_pos.dist( ball_pos );

        if ( opp_dist - control_area < ball_move_dist )
        {
            const double hide_radian = std::asin( std::min( control_area / opp_dist, 1.0 ) );

            double angle_diff = ( ( opp_pos - ball_pos ).th() - ball_move_angle ).abs();
            angle_diff = std::max( angle_diff - AngleDeg::rad2deg( hide_radian ), 0.0 );
#if 1
            dlog.addText( Logger::TEAM,
                          "__ opponent %d (%.1f %.1f) angle_diff=%.1f hide_angle=%.1f",
                          (*o)->unum(), (*o)->pos().x, (*o)->pos().y,
                          angle_diff, AngleDeg::rad2deg( hide_radian ) );
#endif
            if ( cross_course_cone > angle_diff )
            {
                cross_course_cone = angle_diff;
                if ( cross_course_cone < 5.0 )
                {
#if 1
                    dlog.addText( Logger::TEAM,
                                  "(can_cross) too narrow" );
#endif
                    return 0.0;
                }
            }
        }
    }

#if 1
    dlog.addText( Logger::TEAM,
                  "(can_cross) value=%f",
                  std::min( 1.0, cross_course_cone / 90.0 ));
#endif
    return std::min( 1.0, cross_course_cone / 90.0 );
}
#endif
