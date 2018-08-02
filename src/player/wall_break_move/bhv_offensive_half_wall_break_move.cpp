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

#include "bhv_offensive_half_wall_break_move.h"

#include "../strategy.h"

#include <rcsc/common/audio_memory.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/soccer_intention.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>

#include <rcsc/action/body_intercept.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
class IntentionOffensiveHalfWallBreakMove
    : public SoccerIntention {
private:
    const Vector2D M_target_point;
    GameTime M_last_time;
    int M_ball_holder_unum;
    int M_count;
    int M_offside_count;
    int M_their_ball_count;
    int M_mid_field_count;
public:

    IntentionOffensiveHalfWallBreakMove( const Vector2D & target_point,
                                         const GameTime & start_time,
                                         const int ball_holder_unum )
        : M_target_point( target_point ),
          M_last_time( start_time ),
          M_ball_holder_unum( ball_holder_unum ),
          M_count( 0 ),
          M_offside_count( 0 ),
          M_their_ball_count( 0 ),
          M_mid_field_count( 0 )
      { }

    bool finished( const PlayerAgent * agent );
    bool execute( PlayerAgent * agent );
};


/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionOffensiveHalfWallBreakMove::finished( const PlayerAgent * agent )
{

    //if ( ++M_count > 5 )
    // we need change

    if ( ++M_count >= 13 ) // 2014-07-18
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(IntentionOffensiveHalfWallBreakMove::finished) no remained step" );
        return true;
    }

    const WorldModel & wm = agent->world();

    if ( wm.time().cycle() - 1 != M_last_time.cycle() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(IntentionOffensiveHalfWallBreakMove::finished) time mismatch" );
        return true;
    }


    if ( wm.audioMemory().passTime() == wm.time()
         && ! wm.audioMemory().pass().empty()
         && ( wm.audioMemory().pass().front().receiver_ == wm.self().unum() )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(IntentionOffensiveHalfWallBreakMove::finished) hear pass message" );
        return true;
    }

    // if ( wm.self().pos().x > wm.offsideLineX() + 1.0 )
    // {
    //     dlog.addText( Logger::TEAM,
    //                   __FILE__":(IntentionOffensiveHalfWallBreakMove::finished) too over offside line" );
    //     return true;
    // }


    if ( wm.ball().pos().x > ServerParam::i().theirPenaltyAreaLineX() )
    {
        ++M_mid_field_count;
        if ( M_mid_field_count >= 3 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(IntentionOffensiveHalfWallBreakMove::finished) situation is not attack" );
            return true;
        }
    }

    if ( wm.self().pos().x > wm.offsideLineX() )
    {
        ++M_offside_count;
        //if ( M_offside_count >= 5 )
        if ( M_offside_count >= 3 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(IntentionOffensiveHalfWallBreakMove::finished) over offside line count" );
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
                      __FILE__":(IntentionOffensiveHalfWallBreakMove::finished) their ball count = %d",
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
                      __FILE__":(IntentionOffensiveHalfWallBreakMove::finished) my ball" );
        return true;
    }

    // if ( M_ball_holder_unum != Unum_Unknown
    //      && wm.interceptTable()->fastestTeammate()
    //      && wm.interceptTable()->fastestTeammate()->unum() != M_ball_holder_unum )
    // {
    //     dlog.addText( Logger::TEAM,
    //                   __FILE__":(IntentionOffensiveHalfWallBreakMove::finished) ball holder changed." );
    //     return true;
    // }
    /*
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    if ( home_pos.x > M_target_point.x + 5.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(IntentionOffensiveHalfWallBreakMove::finished) home pos x >> target x." );
        return true;
    }
    */

    const Vector2D my_pos = wm.self().inertiaFinalPoint();
    if ( my_pos.dist2( M_target_point ) < 1.5 )
    {
        /*
        if ( Bhv_CenterForwardMove::is_marked( wm ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(IntentionOffensiveHalfWallBreakMove::finished) exist marker." );
            return true;
        }
        */
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionOffensiveHalfWallBreakMove::execute( PlayerAgent * agent )
{
    const double dash_power = ServerParam::i().maxDashPower();
    const WorldModel & wm = agent->world();

    M_last_time = agent->world().time();

    dlog.addText( Logger::TEAM,
                  __FILE__": (intention::execute) target=(%.1f %.1f) power=%.1f",
                  M_target_point.x, M_target_point.y,
                  dash_power );

    agent->debugClient().addMessage( "OHMove:Intention%d", M_count );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;



    if ( ! Body_GoToPoint( M_target_point, dist_thr, dash_power ).execute( agent ) )
    {
        Body_TurnToAngle( 0.0 ).execute( agent );
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

    return true;
}

/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfWallBreakMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_OffensiveHalfWallBreakMove" );

    if ( doCornerMove( agent ) )
    {
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfWallBreakMove::doCornerMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    Vector2D target_point = Vector2D::INVALIDATED;

    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();
    Vector2D receive_pos = wm.ball().inertiaPoint( mate_min );

    const int ball_holder_unum = ( wm.interceptTable()->fastestTeammate()
                                   ? wm.interceptTable()->fastestTeammate()->unum()
                                   : Unum_Unknown );
    if ( mate_min < opp_min + 2
         && receive_pos.x > 36.0
         && wm.ball().pos().x < wm.offsideLineX() )
    {
        if ( receive_pos.absY() > 20.0 )
        {
            target_point.x = 38.0;
            if ( wm.ball().pos().x > 36.0 )
            {
                target_point.x = wm.ball().pos().x - 6.0;
            }
            target_point.y = 11.0  * sign( wm.ball().pos().y );
        }
        else
        {
            target_point.x = 42.0;
            target_point.y = 5.0 * sign( wm.ball().pos().y );
        }

        if ( wm.ball().pos().x < 36.0
             && wm.self().pos().x < 36.0 )
        {
            agent->setIntention( new IntentionOffensiveHalfWallBreakMove( target_point, wm.time(), ball_holder_unum ) );
        }

    }
    /*
    else if ( mate_min < opp_min + 2
              && receive_pos.x < 36.0
              && wm.ball().pos().x > 36.0 )
    {
        target_point = wm.self().pos();
        target_point.x = 35.0;
        agent->setIntention( new IntentionOffensiveHalfWallBreakMove( target_point, wm.time(), ball_holder_unum ) );
    }
    */
    else
    {
        return false;
    }

    double tmp_dash_power = ServerParam::i().maxDashPower();

    if ( ( wm.ball().pos().dist( wm.self().pos() ) > 30.0
           && wm.self().stamina() < ServerParam::i().staminaMax() * 0.4 )
         || ( wm.self().pos().x < 0
              && wm.self().stamina() < ServerParam::i().staminaMax() * 0.6 )
         )
    {
        tmp_dash_power = Strategy::get_normal_dash_power( wm );
    }

    const double dash_power =  tmp_dash_power;
    //const double dash_power =  ServerParam::i().maxDashPower();//Strategy::get_normal_dash_power( wm );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    dlog.addText( Logger::ROLE,
                  __FILE__": (doWallBreakMove) pos=(%.2f %.2f) dash_power=%.1f dist_thr=%.3f",
                  home_pos.x, home_pos.y,
                  dash_power,
                  dist_thr );
    agent->debugClient().addMessage( "OH:WallBreakMove:%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point, dist_thr, dash_power ).execute( agent ) )
    {
        Body_TurnToAngle( 0.0 ).execute( agent );
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

    return true;

}

/*-------------------------------------------------------------------*/
/*!
  TODO:
  Not only the receive point but also receivable area (line) must be considered.

  candidate evaluation criteria:
  - opponent_reach_step
  - opponent_distance_to_target_point
  - y_diff [target_point, goal]
  - ball_move_angle
  - opponent_step_to(project_point) - ball_step_to(project_point)
 */

double
Bhv_OffensiveHalfWallBreakMove::evaluateTargetPoint( const rcsc::WorldModel & wm,
                                                     const rcsc::Vector2D & home_pos,
                                                     const rcsc::Vector2D & ball_pos,
                                                     const rcsc::Vector2D & target_point )
{
    //(void)wm;

    double value = 0.0;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM, "(evaluate)>>>>> (%.1f %.1f)",
                  target_point.x, target_point.y );
#endif

    {
        const double home_y_diff = std::fabs( home_pos.y - target_point.y );
        double tmp_value =  home_y_diff;
        value += tmp_value;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM, "home_y_diff=%.2f value=%f [%f]",
                      home_y_diff, tmp_value, value );
#endif
    }

    {
        double x_buf = -5.0;
        if ( wm.ball().pos().x < 36.0 ) x_buf = 0.0;

        const double ball_x_diff = std::fabs( ball_pos.x + x_buf - target_point.x );
        double tmp_value = ball_x_diff;
        value += tmp_value;
        /*
        if ( wm.ball().pos().absY() - 22.0 < 0
             && wm.self().pos().y * wm.ball().pos().y > 0 )
        {
            value -= tmp_value;
        }
        else
        {
            value += tmp_value;
        }
        */
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM, "ball_y_diff=%.2f value=%f [%f]",
                      ball_y_diff, tmp_value, value );
#endif
    }
    // {
    //     const double goal_y_diff = target_point.absY();
    //     //value = -0.1 * goal_y_diff;
    // }
#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM, "<<<<< result value=%f", value );
#endif
    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfWallBreakMove::isMoveSituation( PlayerAgent * agent  )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_OffensiveHalfWallBreakMove(isMoveSituation)" );

    const WorldModel & wm = agent->world();

    int self_step = wm.interceptTable()->selfReachStep();
    int teammate_step = wm.interceptTable()->teammateReachStep();

    int reach_step = std::min( self_step, teammate_step );

    Vector2D ball_pos = wm.ball().inertiaPoint( reach_step );

    if ( ball_pos.absY() < 20.0 )
    {
        //std::cout << wm.time().cycle() << " (isMoveSituation) y " << ball_pos.y << std::endl;
        dlog.addText( Logger::ROLE,
                      __FILE__": (isMoveSituation) situation is not side attack" );
        return false;
    }
    else if ( ball_pos.x < 30.0 )
    {
        double target_point_x = 40.0;

        double l_y = ServerParam::i().pitchHalfWidth();
        double r_y = ServerParam::i().penaltyAreaHalfWidth();

        if ( ball_pos.y < 0 )
        {
            l_y = -ServerParam::i().penaltyAreaHalfWidth();
            r_y = -ServerParam::i().pitchHalfWidth();
        }

        for ( double i = l_y; i > r_y; i -= 2.0 )
        {
            bool isIntercept = true;

            Vector2D tmp_vec( target_point_x, i );

            double dist = 1000.0;

            const PlayerObject * near_mate = wm.getTeammateNearestTo( tmp_vec,
                                                                      10,
                                                                      &dist );
            dlog.addText( Logger::ROLE,
                          __FILE__": (isMoveSituation) dist =%.3f",
                          dist );

            if ( !near_mate ) continue;

            int mate_step =  near_mate->playerTypePtr()->cyclesToReachDistance( dist ) + 2;

            reach_step += mate_step;

            double ball_move_dist = ball_pos.dist( tmp_vec );
            double first_ball_speed = ServerParam::i().firstBallSpeed( ball_move_dist, mate_step );

            if ( checkOpponent( wm, ball_pos, tmp_vec,
                                first_ball_speed, reach_step ) )
            {
                isIntercept = false;
            }

            if ( isIntercept )
            {
                std::cout << wm.time().cycle() << " (isMoveSituation) situation is side attack " << wm.self().unum() << std::endl;

                dlog.addText( Logger::TEAM,
                              __FILE__": (isMoveSituation) situation is side attack" );

                return true;
            }
        }
    }

    return false;

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_OffensiveHalfWallBreakMove::checkOpponent( const WorldModel & wm,
                                               const Vector2D & first_ball_pos,
                                               const Vector2D & receive_pos,
                                               const double first_ball_speed,
                                               const int ball_move_step )
{
    const ServerParam & SP = ServerParam::i();

    const AngleDeg ball_move_angle = ( receive_pos - first_ball_pos ).th();
    const Vector2D first_ball_vel = Vector2D::from_polar( first_ball_speed, ball_move_angle );

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
            //dash_dist -= ptype->realSpeedMax() * std::min( 5.0, (*o)->posCount() * 0.8 );

            int opponent_turn = 1;
            int opponent_dash = ptype->cyclesToReachDistance( dash_dist );

#ifdef DEBUG_PRINT_PASS_REQUEST_OPPONENT
            dlog.addText( Logger::TEAM,
                          "%d: opponent %d (%.1f %.1f) dash=%d move_dist=%.1f",
                          count,
                          (*o)->unum(), (*o)->pos().x, (*o)->pos().y,
                          opponent_dash, dash_dist );
#endif


            if ( 1 + opponent_turn + opponent_dash < step )
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
