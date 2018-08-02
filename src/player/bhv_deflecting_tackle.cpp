// -*-c++-*-

/*!
  \file bhv_deflecting_tackle.cpp
  \brief tackle ball to out of our goal
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

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

#include "bhv_deflecting_tackle.h"

#include "field_analyzer.h"
#include "shoot_simulator.h"

#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/math_util.h>

#include <cmath>
#include <vector>

using namespace rcsc;

// #define DEBUG_PRINT

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DeflectingTackle::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ACTION,
                  __FILE__"(execute)  "
                  "probability_threshold = %f, force = %s",
                  M_tackle_prob_threshold,
                  ( M_force_tackle ? "true" : "false" ) );

    const WorldModel & wm = agent->world();

    const int teammate_reach_cycle = wm.interceptTable()->teammateReachCycle();
    const int self_reach_cycle = wm.interceptTable()->selfReachCycle();

    if ( ! M_force_tackle
         && ! ( ShootSimulator::is_ball_moving_to_our_goal( wm.ball().pos(), wm.ball().vel(), 2.0 )
                && wm.ball().inertiaPoint( std::min( teammate_reach_cycle,
                                                     self_reach_cycle ) ).x
                <= ServerParam::i().ourTeamGoalLineX() ) )
    {
        dlog.addText( Logger::ACTION,
                      __FILE__": no need to deflecting" );

        return false;
    }


    //
    // check tackle success probability
    //
    const double tackle_probability = wm.self().tackleProbability();

    dlog.addText( Logger::ACTION,
                  __FILE__": tackle probability = %f",
                  tackle_probability );

    if ( tackle_probability < M_tackle_prob_threshold )
    {
        dlog.addText( Logger::ACTION,
                      __FILE__": tackle probability too low" );
        return false;
    }


    const double goal_half_width = ServerParam::i().goalHalfWidth();
    const double goal_line_x = ServerParam::i().ourTeamGoalLineX();
    const Vector2D goal_plus_post( goal_line_x, +goal_half_width );
    const Vector2D goal_minus_post( goal_line_x, -goal_half_width );

    const Vector2D ball_pos = wm.ball().pos();


    //
    // create candidates
    //
    std::vector< AngleDeg > candidates;
    candidates.reserve( 20 );

    if ( ball_pos.y > 0.0 )
    {
        candidates.push_back( ( goal_plus_post + Vector2D( 0.0, +5.0 )
                                - ball_pos ).th() );
    }
    else
    {
        candidates.push_back( ( goal_minus_post + Vector2D( 0.0, -5.0 )
                                - ball_pos ).th() );
    }

    for( int d = -180; d < 180; d += 10 )
    {
        candidates.push_back( AngleDeg( d ) );
    }


    //
    // evalute candidates
    //
    static const double not_shoot_ball_eval = 10000;

    struct EvaluateFunction
    {
        static
        double evaluate( const Vector2D & vec,
                         const WorldModel & wm )
          {
              double eval = 0.0;

              if ( ! ShootSimulator::is_ball_moving_to_our_goal( wm.ball().pos(), vec, 2.0 ) )
              {
                  eval += not_shoot_ball_eval;
              }

              Vector2D final_ball_pos;
              final_ball_pos = inertia_final_point( wm.ball().pos(),
                                                    vec,
                                                    ServerParam::i().ballDecay() );

              if ( vec.x < 0.0
                   && wm.ball().pos().x > ServerParam::i().ourTeamGoalLineX() + 0.5 )
              {
                  //const double goal_half_width = ServerParam::i().goalHalfWidth();
                  const double pitch_half_width = ServerParam::i().pitchHalfWidth();
                  const double goal_line_x = ServerParam::i().ourTeamGoalLineX();
                  const Vector2D corner_plus_post( goal_line_x, +pitch_half_width );
                  const Vector2D corner_minus_post( goal_line_x, -pitch_half_width );

                  const Line2D goal_line( corner_plus_post, corner_minus_post );

                  const Segment2D ball_segment( wm.ball().pos(), final_ball_pos );
                  Vector2D cross_point = ball_segment.intersection( goal_line );
                  if ( cross_point.isValid() )
                  {
                      eval += 1000.0;

                      double c = std::min( std::fabs( cross_point.y ),
                                           ServerParam::i().pitchHalfWidth() );

                      //if ( c > goal_half_width
                      //     && ( cross_point.y * wm.self().pos().y >= 0.0
                      //          && c > wm.self().pos().absY() ) )
                      {
                          eval += c;
                      }
                  }
              }
              else
              {
                  if ( final_ball_pos.x > ServerParam::i().ourTeamGoalLineX() + 5.0 )
                  {
                      eval += 1000.0;
                  }

                  eval += sign( wm.ball().pos().y ) * vec.y;
              }
#ifdef DEBUG_PRINT
              dlog.addLine( Logger::ACTION,
                            wm.ball().pos(),
                            wm.ball().pos() + vec, //final_ball_pos,
                            "#0000ff" );
#endif
              return eval;
          }
    };

    double max_eval = -1;
    size_t max_index = 0;

    for( size_t i = 0; i < candidates.size(); ++i )
    {
        const Vector2D result_vec = getTackleResult( candidates[i], wm );

        double eval = EvaluateFunction::evaluate( result_vec, wm );

        dlog.addText( Logger::ACTION,
                      __FILE__": th = %.1f, result_ball_vel=(%.3f, %.3f), vel_angle=%.1f "
                      "eval = %f",
                      candidates[i].degree(),
                      result_vec.x, result_vec.y,
                      result_vec.th().degree(),
                      eval );

        if ( eval > max_eval )
        {
            max_index = i;
            max_eval = eval;
        }
    }


    const double tackle_dir = ( candidates[max_index] - wm.self().body() ).degree();

#ifdef DEBUG_PRINT
    //
    // debug print
    //
    {
        const Vector2D & best_vec = getTackleResult( candidates[max_index], wm );
        const Vector2D next_ball_pos = wm.ball().pos() + best_vec;

        const Vector2D final_ball_pos = inertia_final_point( wm.ball().pos(),
                                                             best_vec,
                                                             ServerParam::i().ballDecay() );

        dlog.addText( Logger::ACTION,
                      __FILE__": best candidate: th = %f, eval = %f, "
                      "next ball pos = [%f, %f], final ball pos = [%f, %f ]",
                      candidates[max_index].degree(),
                      max_eval,
                      next_ball_pos.x, next_ball_pos.y,
                      final_ball_pos.x, final_ball_pos.y );

        dlog.addText( Logger::ACTION,
                      __FILE__": tackle_dir = %f, self body direction = %f",
                      tackle_dir, wm.self().body().degree() );

        agent->debugClient().setTarget( final_ball_pos );
    }
#endif

    //
    // do tackle
    //
    if ( max_eval < not_shoot_ball_eval
         && ! M_force_tackle )
    {
        dlog.addText( Logger::ACTION,
                      __FILE__": no valid tackle course found" );
        return false;
    }


    agent->doTackle( tackle_dir );

    agent->setNeckAction( new Neck_TurnToBallOrScan( -1 ) );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_DeflectingTackle::getTackleResult( const AngleDeg & absolute_dir,
                                       const WorldModel & wm )
{
    const ServerParam & param = ServerParam::i();

    const double tackle_target_penalty = 1.0 - ( ( absolute_dir - wm.self().body() ).abs() / 180.0 );
    const double ball_relative_penalty = 1.0 - 0.5 * ( ( wm.ball().angleFromSelf() - wm.self().body() ).abs() / 180.0 );

    double effective_power = ( param.maxBackTacklePower()
                               + ( param.maxTacklePower() - param.maxBackTacklePower() )
                               * tackle_target_penalty )
        * ball_relative_penalty
        * param.tacklePowerRate();
    //Vector2D accel = Vector2D::polar2vector( effective_power, absolute_dir );
    Vector2D vel = wm.ball().vel()
        + Vector2D::polar2vector( effective_power, absolute_dir );

    double len = vel.r();
    if ( len > param.ballSpeedMax() )
    {
        vel *= ( param.ballSpeedMax() / len );
    }

    //     dlog.addText( Logger::ACTION,
    //                         "(getTackleResult) accel(%.3f %.3f) r=%.3f angle=%.1f -> vel=(%.3f %.3f)%.3f",
    //                         accel.x, accel.y,
    //                         effective_power,
    //                         absolute_dir.degree(),
    //                         vel.x, vel.y, vel.r() );

    return vel;
}
