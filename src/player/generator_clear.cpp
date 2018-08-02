// -*-c++-*-

/*!
  \file generator_clear.cpp
  \brief clear generator class Source File
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 3 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "generator_clear.h"

#include "act_clear.h"

#include "field_analyzer.h"

#include <rcsc/action/kick_table.h>
#include <rcsc/player/world_model.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/timer.h>

// #define DEBUG_PROFILE
// #define DEBUG_PRINT

using namespace rcsc;

namespace {

enum {
    ANGLE_DIVS = 40,
};

}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorClear::GeneratorClear()
{
    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorClear &
GeneratorClear::instance()
{
    static GeneratorClear s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorClear::clear()
{
    M_best_action.reset();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorClear::generate( const WorldModel & wm )
{
    static GameTime s_update_time( 0, 0 );

    if ( s_update_time == wm.time() )
    {
        // dlog.addText( Logger::CLEAR,
        //               __FILE__": already updated" );
        return;
    }
    s_update_time = wm.time();

    clear();

    if ( ! wm.self().isKickable() )
    {
        return;
    }

    if ( wm.time().stopped() > 0 )
    {
        // dlog.addText( Logger::CLEAR,
        //               __FILE__": time stopped" );
        return;
    }

    if ( wm.ball().pos().x > -20.0
         || wm.ball().pos().absY() > 20.0 )
    {
        return;
    }

    if ( wm.gameMode().type() != GameMode::PlayOn
         && wm.gameMode().type() != GameMode::GoalKick_
         && wm.gameMode().type() != GameMode::GoalieCatch_
         && ! wm.gameMode().isPenaltyKickMode() )
    {
        return;
    }


#ifdef DEBUG_PROFILE
    MSecTimer timer;
#endif

    generateImpl( wm );

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::CLEAR,
                  __FILE__": PROFILE. elapsed=%.3f [ms]",
                  timer.elapsedReal() );
#endif

}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorClear::generateImpl( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();

    const double min_angle = -180.0;
    const double max_angle = +180.0;
    const double angle_step = std::fabs( max_angle - min_angle ) / ANGLE_DIVS;

    const Rect2D pitch = Rect2D::from_center( 0.0, 0.0, SP.pitchLength(), SP.pitchWidth() );
    const Vector2D goal_left( -SP.pitchHalfLength(), -SP.goalHalfWidth() - 10.0 );
    const Vector2D goal_right( -SP.pitchHalfLength(), +SP.goalHalfWidth() + 10.0 );

    const AngleDeg our_goal_left_angle = ( goal_left - wm.ball().pos() ).th();
    const AngleDeg our_goal_right_angle = ( goal_right - wm.ball().pos() ).th();

#ifdef DEBUG_PRINT
    dlog.addText( Logger::CLEAR,
                  __FILE__": min_angle=%.1f max_angle=%.1f angle_step=%.1f",
                  min_angle, max_angle, angle_step );
    dlog.addText( Logger::CLEAR,
                  __FILE__": goal_left_angle=%.1f goal_right_angle=%.1f",
                  our_goal_left_angle.degree(), our_goal_right_angle.degree() );
#endif

    Vector2D best_target_point = Vector2D::INVALIDATED;
    AngleDeg best_ball_move_angle = 180.0;
    double best_ball_speed = 0.0;
    int best_ball_move_step = 1000;
    int best_kick_step = 1000;
    int max_opponent_step = -1;

    for ( int a = 0; a < ANGLE_DIVS; ++a )
    {
        const AngleDeg ball_move_angle =  min_angle + angle_step * a;

#ifdef DEBUG_PRINT
            dlog.addText( Logger::CLEAR,
                          "%d: angle=%.1f",
                          a, ball_move_angle.degree() );
#endif

        if ( ball_move_angle.isLeftOf( our_goal_left_angle )
             && ball_move_angle.isRightOf( our_goal_right_angle ) )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CLEAR,
                          "%d: xxx our goal", a );
#endif
            continue;
        }

        int kick_step = 1;
        Vector2D ball_vel = KickTable::calc_max_velocity( ball_move_angle,
                                                          wm.self().kickRate(),
                                                          wm.ball().vel() );
        double ball_speed = ball_vel.r();
        if ( ball_speed < 2.0 ) // magic number
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CLEAR,
                          "%d: xxx insufficient ball speed %.3f", a, ball_speed );
#endif
            kick_step = 3;
            if ( ball_speed < 1.0e-3 )
            {
                ball_vel = Vector2D::from_polar( SP.ballSpeedMax(), ball_move_angle );
            }
            else
            {
                ball_vel *= SP.ballSpeedMax() / ball_speed;
            }
            ball_speed = SP.ballSpeedMax();
        }

        Vector2D final_point = inertia_final_point( wm.ball().pos(), ball_vel, SP.ballDecay() );
        int ball_move_step = 100;

        if ( ! pitch.contains( final_point ) )
        {
            const Segment2D ball_move_segment( wm.ball().pos(), final_point );
            Vector2D pitch_intersection;
            int n = pitch.intersection( ball_move_segment, &pitch_intersection, NULL );
            if ( n > 0 )
            {
                final_point = pitch_intersection;
                ball_move_step = SP.ballMoveStep( ball_speed, wm.ball().pos().dist( final_point ) );
            }
        }
        else
        {
            if ( final_point.x < wm.ball().pos().x )
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::CLEAR,
                              "%d: xxx backward clear final_pos=(%.2f %.2f)", a, final_point.x, final_point.y );
#endif
                continue;
            }
        }

        int opponent_step = predictOpponentReachStep( wm, ball_vel, ball_move_angle, ball_move_step );
        opponent_step -= kick_step - 1;

#ifdef DEBUG_PRINT
        dlog.addText( Logger::CLEAR,
                      "%d: ball_final_point=(%.2f %.2f) move_step=%d opponent_step=%d",
                      a, final_point.x, final_point.y, ball_move_step, opponent_step );
#endif

        if ( opponent_step > max_opponent_step
             || ( opponent_step > ball_move_step + 3
                  && ball_move_angle.abs() < best_ball_move_angle.abs() )
             || ( opponent_step == max_opponent_step
                  && ball_move_angle.abs() < best_ball_move_angle.abs() ) )
        {
            best_target_point = final_point;
            best_ball_move_angle = ball_move_angle;
            best_ball_speed = ball_speed;
            best_ball_move_step = ball_move_step;
            best_kick_step = kick_step;
            max_opponent_step = opponent_step;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CLEAR,
                          "%d: updated", a );
#endif
        }
    }

    if ( best_target_point.isValid() )
    {
        Vector2D ball_first_vel = Vector2D::from_polar( best_ball_speed, best_ball_move_angle );
        M_best_action = CooperativeAction::Ptr( new ActClear( wm.self().unum(),
                                                              best_target_point,
                                                              ball_first_vel,
                                                              best_ball_move_step + best_kick_step,
                                                              best_kick_step,
                                                              "Clear" ) );
        M_best_action->setIndex( 1 );
        M_best_action->setSafetyLevel( CooperativeAction::Safe );

        dlog.addText( Logger::CLEAR,
                      __FILE__": result target=(%.2f %.2f) angle=%.1f ball_speed=%.3f move_step=%d kick_step=%d opponent_step=%d",
                      best_target_point.x, best_target_point.y,
                      best_ball_move_angle.degree(), best_ball_speed,
                      best_ball_move_step, best_kick_step, max_opponent_step );
        dlog.addLine( Logger::CLEAR,
                      wm.ball().pos(), best_target_point, "#0F0" );
    }

}

/*-------------------------------------------------------------------*/
/*!

 */
int
GeneratorClear::predictOpponentReachStep( const WorldModel & wm,
                                          const Vector2D & first_ball_vel,
                                          const AngleDeg & ball_move_angle,
                                          const int ball_move_step )
{
    int min_step = ball_move_step;

    bool updated = false;
    for ( AbstractPlayerObject::Cont::const_iterator p = wm.theirPlayers().begin(),
              end = wm.theirPlayers().end();
          p != end;
          ++p )
    {
        int step = predictOpponentReachStep( wm, *p, first_ball_vel, ball_move_angle, min_step );
        if ( step < min_step )
        {
            updated = true;
            min_step = step;
        }
    }

    return ( updated ? min_step : 1000 );
}

/*-------------------------------------------------------------------*/
/*!

 */
int
GeneratorClear::predictOpponentReachStep( const WorldModel & wm,
                                          const AbstractPlayerObject * opponent,
                                          const Vector2D & first_ball_vel,
                                          const AngleDeg & ball_move_angle,
                                          const int max_step )
{
    const ServerParam & SP = ServerParam::i();

    const PlayerType * ptype = opponent->playerTypePtr();

    int min_step = FieldAnalyzer::estimate_min_reach_cycle( opponent->pos(),
                                                            ptype->kickableArea() + 0.2,
                                                            ptype->realSpeedMax(),
                                                            wm.ball().pos(),
                                                            ball_move_angle );
    if ( min_step < 0 )
    {
        min_step = 10; // set penalty step
    }

    min_step -= opponent->posCount();
    if ( min_step < 1 )
    {
        min_step = 1;
    }

    const double opponent_speed = opponent->vel().r();


    for ( int step = min_step; step < max_step; ++step )
    {
        const Vector2D ball_pos = inertia_n_step_point( wm.ball().pos(), first_ball_vel, step, SP.ballDecay() );

        Vector2D opponent_pos = opponent->inertiaPoint( step );
        double target_dist = opponent_pos.dist( ball_pos );

        if ( target_dist - ptype->kickableArea() < 0.001 )
        {
            return step;
        }

        double dash_dist = target_dist;
        if ( step > 1 )
        {
            dash_dist -= ptype->kickableArea();
            dash_dist -= 0.5; // special bonus
        }

        if ( dash_dist > ptype->realSpeedMax() * step )
        {
            continue;
        }

        //
        // dash
        //

        int n_dash = ptype->cyclesToReachDistance( dash_dist );

        if ( n_dash > step )
        {
            continue;
        }

#if 1
        //
        // turn
        //
        int n_turn = ( opponent->bodyCount() > 0
                       ? 0
                       : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                   opponent->body(),
                                                                   opponent_speed,
                                                                   target_dist,
                                                                   ( ball_pos - opponent_pos ).th(),
                                                                   ptype->kickableArea(),
                                                                   true ) );
        int opponent_step = ( n_turn == 0
                              ? n_dash
                              : n_turn + n_dash );
#else
        int opponent_step = n_dash;
#endif
        if ( opponent->isTackling() )
        {
            opponent_step += 5; // Magic Number
        }

        opponent_step -= std::min( 10, opponent->posCount() );

        if ( opponent_step <= step )
        {
            return opponent_step;
        }
    }

    return 1000;
}
