// -*-c++-*-

/*!
  \file generator_omni_dribble.cpp
  \brief omni direction dribble course generator Source File
*/

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

#include "generator_omni_dribble.h"

#include "act_dribble.h"
#include "field_analyzer.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/geom/circle_2d.h>
#include <rcsc/geom/segment_2d.h>
#include <rcsc/geom/line_2d.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/timer.h>

#include <algorithm>
#include <limits>

#include <cmath>

// #define DEBUG_PROFILE
// #define DEBUG_PRINT
// #define DEBUG_PRINT_SUCCESS_COURSE

// #define DEBUG_PRINT_LEVEL_1
// #define DEBUG_PRINT_LEVEL_2
// #define DEBUG_PRINT_FAILED_COURSE


using namespace rcsc;

namespace {

inline
void
debug_paint_failed( const int count,
                    const Vector2D & receive_point )
{
    dlog.addCircle( Logger::DRIBBLE,
                    receive_point.x, receive_point.y, 0.1,
                    "#ff0000" );
    char num[8];
    snprintf( num, 8, "%d", count );
    dlog.addMessage( Logger::DRIBBLE,
                     receive_point, num );
}

bool
is_bad_body_angle( const WorldModel & wm )
{
    if ( wm.self().pos().x > ServerParam::i().pitchHalfLength() - 3.0
         && wm.self().pos().absY() > ServerParam::i().goalHalfWidth() )
    {
        if ( wm.self().body().abs() < 90.0
             && ( ( ServerParam::i().theirTeamGoalPos() - wm.self().pos() ).th()
                  - wm.self().body() ).abs() > 20.0 )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__":(is_bad_body_angle) true(1)" );
            return false;
        }
    }

    if ( wm.self().pos().x > ServerParam::i().pitchHalfLength() - 10.0 )
    {
        Line2D goal_line( Vector2D( ServerParam::i().pitchHalfLength(), -10.0 ),
                          Vector2D( ServerParam::i().pitchHalfLength(), 10.0 ) );
        Ray2D body_ray( wm.self().pos(), wm.self().body() );
        Vector2D intersection = body_ray.intersection( goal_line );
        if ( intersection.isValid()
             && intersection.absY() > 15.0 )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__":(is_bad_body_angle) true(2)" );
            return true;
        }
    }

    return false;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorOmniDribble::GeneratorOmniDribble()
    : M_total_count( 0 ),
      M_first_ball_pos( Vector2D::INVALIDATED ),
      M_first_ball_vel( 0.0, 0.0 )
{
    M_courses.reserve( 128 );
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorOmniDribble &
GeneratorOmniDribble::instance()
{
    static GeneratorOmniDribble s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorOmniDribble::clear()
{
    M_total_count = 0;
    M_first_ball_pos = Vector2D::INVALIDATED;
    M_first_ball_vel.assign( 0.0, 0.0 );
    M_courses.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorOmniDribble::generate( const WorldModel & wm )
{
    if ( M_update_time == wm.time() )
    {
        return;
    }
    M_update_time = wm.time();

    clear();

    if ( wm.gameMode().type() != GameMode::PlayOn
         && ! wm.gameMode().isPenaltyKickMode() )
    {
        return;
    }

    //
    // updater ball holder
    //
    if ( ! wm.self().isKickable()
         || wm.self().isFrozen() )
    {
        return;
    }

    //
    // check position
    //
    if ( wm.self().pos().x < 0.0 )
         // && wm.self().body().abs() > 90.0
    {
        return;
    }

    if ( wm.self().pos().absY() > 15.0 )
    {
        return;
    }


    if ( is_bad_body_angle( wm ) )
    {
        return;
    }


    M_first_ball_pos = wm.ball().pos();
    M_first_ball_vel = wm.ball().vel();

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    generateImpl( wm );

    std::sort( M_courses.begin(), M_courses.end(),
               CooperativeAction::DistanceSorter( ServerParam::i().theirTeamGoalPos() ) );

    // for ( std::vector< rcsc::CooperativeAction::Ptr >::const_iterator it = M_courses.begin(),
    //           end = M_courses.end();
    //       it != end;
    //       ++it )
    // {
    // }

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (generate) PROFILE size=%d/%d elapsed %.3f [ms]",
                  (int)M_courses.size(),
                  M_total_count,
                  timer.elapsedReal() );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorOmniDribble::generateImpl( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();

    const double dash_angle_step = std::max( 15.0, SP.dashAngleStep() );
    const double min_dash_angle = ( -180.0 < SP.minDashAngle() && SP.maxDashAngle() < 180.0
                                    ? SP.minDashAngle()
                                    : dash_angle_step * static_cast< int >( -180.0 / dash_angle_step ) );
    const double max_dash_angle = ( -180.0 < SP.minDashAngle() && SP.maxDashAngle() < 180.0
                                    ? SP.maxDashAngle() + dash_angle_step * 0.5
                                    : dash_angle_step * static_cast< int >( 180.0 / dash_angle_step ) - 1.0 );


#ifdef DEBUG_PRINT
    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (generateImpl) min_angle=%.1f max_angle=%.1f step=%.1f",
                  min_dash_angle, max_dash_angle, dash_angle_step );
#endif

    for ( double dir = min_dash_angle;
          dir < max_dash_angle;
          dir += dash_angle_step )
    {
        if ( std::fabs( dir ) < 0.5 ) continue;
        //if ( std::fabs( dir ) > 100.0 ) continue; // Magic Number

        double actual_dir = SP.discretizeDashAngle( dir );

        simulateKickDashes( wm, actual_dir );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorOmniDribble::simulateKickDashes( const WorldModel & wm,
                                          const double dash_dir )
{
    static const int min_dash = 3;
    static const int max_dash = 5;

    static std::vector< Vector2D > self_cache;

    //
    // create self position cache
    //
    createSelfCache( wm, dash_dir, max_dash, self_cache );

    const int angle_divs = 10;
    const double angle_step = 360.0 / angle_divs;
    const double max_x = ( ServerParam::i().keepawayMode()
                           ? ServerParam::i().keepawayLength() * 0.5 - 1.5
                           : ServerParam::i().pitchHalfLength() - 1.0 );
    const double max_y = ( ServerParam::i().keepawayMode()
                           ? ServerParam::i().keepawayWidth() * 0.5 - 1.5
                           : ServerParam::i().pitchHalfWidth() - 1.0 );
    const double trap_dist
        = wm.self().playerType().playerSize()
        + wm.self().playerType().kickableMargin() * 0.6
        + ServerParam::i().ballSize() * 0.5;

    const AngleDeg dash_angle = wm.self().body() + dash_dir;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::DRIBBLE,
                  "(simulateKickDashes) dash_dir=%.1f angle=%.1f",
                  dash_dir, dash_angle.degree() );
#endif

    CooperativeAction::SafetyLevel max_safety_level = CooperativeAction::Failure;
    CooperativeAction::Ptr best_action;

    for ( int a = 0; a < angle_divs; ++a )
    {
        const AngleDeg keep_angle = dash_angle + angle_step*a;
        const Vector2D last_ball_rel = Vector2D::polar2vector( trap_dist, keep_angle );

        for ( int n_dash = min_dash; n_dash <= max_dash; ++n_dash )
        {
            const Vector2D last_ball_pos = self_cache[n_dash] + last_ball_rel;

            ++M_total_count;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::DRIBBLE,
                          "%d: n_dash=%d dashDir=%.1f keepDir=%.1f my=(%.1f %.1f)",
                          M_total_count, n_dash, dash_dir, AngleDeg::normalize_angle( angle_step*a ),
                          self_cache[n_dash].x, self_cache[n_dash].y );
            dlog.addText( Logger::DRIBBLE,
                          "%d: keepAngle=%.1f last_ball_rel=(%.1f %.1f) last_ball_pos=(%.1f %1f)",
                          M_total_count,
                          keep_angle.degree(),
                          last_ball_rel.x, last_ball_rel.y,
                          last_ball_pos.x, last_ball_pos.y );
#endif
            if ( last_ball_pos.absX() > max_x
                 || last_ball_pos.absY() > max_y )
            {
                continue;
            }

            if ( ! checkExecutable( wm, n_dash, self_cache, last_ball_pos ) )
            {
                continue;
            }

            CooperativeAction::SafetyLevel safety_level = getSafetyLevel( wm, n_dash, last_ball_pos );

#ifdef DEBUG_PRINT
            dlog.addText( Logger::DRIBBLE,
                          "%d ==== safe %d",
                          M_total_count, safety_level );
#endif
            if ( safety_level == CooperativeAction::Failure )
            {
#ifdef DEBUG_PRINT_FAILED_COURSE
                dlog.addCircle( Logger::DRIBBLE,
                                last_ball_pos.x, last_ball_pos.y, 0.01,
                                "#0f0" );
                char str[16]; snprintf( str, 8, "%d:%d,%d", M_total_count, n_dash, safety_level );
                dlog.addMessage( Logger::DRIBBLE,
                                 last_ball_pos.x + 0.01, last_ball_pos.y,
                                 str, "#0f0" );
#endif
                continue;
            }

            if ( safety_level >= max_safety_level
                 || safety_level == CooperativeAction::Safe )
            {
                Vector2D ball_move = last_ball_pos - wm.ball().pos();
                double first_ball_speed = ServerParam::i().firstBallSpeed( ball_move.r(), 1 + n_dash );
                Vector2D first_ball_vel = ball_move.setLengthVector( first_ball_speed );
                double dash_power = wm.self().staminaModel().getSafetyDashPower( wm.self().playerType(),
                                                                                 ServerParam::i().maxDashPower() );

#ifdef DEBUG_PRINT
                dlog.addText( Logger::DRIBBLE,
                              "%d <<<<< updated best action old_max_prob=%.3f new_prob=%.3f",
                              M_total_count, max_prob, prob );
#endif
#ifdef DEBUG_PRINT_SUCCESS_COURSE
                dlog.addCircle( Logger::DRIBBLE,
                                last_ball_pos.x, last_ball_pos.y, 0.01,
                                "#f00" );
                char str[16]; snprintf( str, 8, "%d:%d,%.2f", M_total_count, n_dash, prob );
                dlog.addMessage( Logger::DRIBBLE,
                                 last_ball_pos.x + 0.01, last_ball_pos.y,
                                 str, "#f00" );
#endif
                max_safety_level = safety_level;

                best_action = ActDribble::create_omni( wm.self().unum(),
                                                       last_ball_pos,
                                                       self_cache[n_dash],
                                                       wm.self().body().degree(),
                                                       first_ball_vel,
                                                       dash_power,
                                                       dash_dir,
                                                       n_dash,
                                                       "omniDribble" );
                best_action->setIndex( M_total_count );
                best_action->setSafetyLevel( safety_level );
            }
        }
    }

    if ( best_action )
    {
        M_courses.push_back( best_action );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorOmniDribble::createSelfCache( const WorldModel & wm,
                                       const double dash_dir,
                                       const int n_dash,
                                       std::vector< Vector2D > & self_cache )
{
    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    self_cache.clear();

    StaminaModel stamina_model = wm.self().staminaModel();

    Vector2D my_pos = wm.self().pos();
    Vector2D my_vel = wm.self().vel();

    my_pos += my_vel;
    my_vel *= ptype.playerDecay();

    // first element is next cycle just after kick
    self_cache.push_back( my_pos );
    stamina_model.simulateWaits( ptype, 1 );

    const AngleDeg dash_angle = wm.self().body() + dash_dir;
    const double dash_dir_rate = SP.dashDirRate( dash_dir );
    const Vector2D unit_vec = Vector2D::polar2vector( 1.0, dash_angle );

    for ( int i = 0; i < n_dash; ++i )
    {
        double dash_power = stamina_model.getSafetyDashPower( ptype, SP.maxDashPower() );
        Vector2D dash_accel = unit_vec * ( dash_power
                                           * dash_dir_rate
                                           * ptype.dashPowerRate()
                                           * stamina_model.effort() );

        my_vel += dash_accel;
        my_pos += my_vel;
        my_vel *= ptype.playerDecay();

        stamina_model.simulateDash( ptype, dash_power );

        self_cache.push_back( my_pos );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
GeneratorOmniDribble::checkExecutable( const WorldModel & wm,
                                       const int n_dash,
                                       const std::vector< Vector2D > & self_cache,
                                       const Vector2D & last_ball_pos )
{
    const ServerParam & SP = ServerParam::i();

    const double bdecay = SP.ballDecay();
    const double kickable_thr = wm.self().playerType().kickableArea() + 0.05;
    const double collide_thr = wm.self().playerType().playerSize() + SP.ballSize() + 0.2;

    const double ball_move_dist = wm.ball().pos().dist( last_ball_pos );
    const double ball_first_speed = SP.firstBallSpeed( ball_move_dist, 1 + n_dash );

    if ( ball_first_speed > SP.ballSpeedMax() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::DRIBBLE,
                      "%d: xxx over ball_speed_max %.2f",
                      M_total_count, ball_first_speed );
#endif
        return false;
    }

    Vector2D ball_vel = last_ball_pos - wm.ball().pos();
    ball_vel.setLength( ball_first_speed );

#ifdef DEBUG_PRINT
    dlog.addText( Logger::DRIBBLE,
                  "%d: ball first_speed=%.3f move_angle=%.1f",
                  M_total_count, ball_first_speed, ball_vel.th().degree() );
#endif

    //
    // check kickable or not
    //
    {
        Vector2D kick_accel = ball_vel - wm.ball().vel();
        double kick_accel_mag = kick_accel.r();
        if ( kick_accel_mag > SP.ballAccelMax() )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx over kick accel = %.3f",
                          M_total_count, kick_accel_mag );
#endif
            return false;
        }

        if ( kick_accel_mag / wm.self().kickRate() > SP.maxPower() )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx over kick power = %.3f",
                          M_total_count, kick_accel_mag / wm.self().kickRate() );
#endif
            return false;
        }
    }

    //
    // dash simulation
    //

    Vector2D ball_pos = wm.ball().pos();
    ball_pos += ball_vel;

    for ( int i = 0; i <= n_dash; ++i )
    {
        const double d = self_cache[i].dist( ball_pos );
        if ( d < collide_thr - ( 0.1 * i ) )
        {
#ifdef DEBUG_PRINT_LEVEL_1
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx dash=%d collision. dist=%f",
                          M_total_count, i, d );
#endif
            return false;
        }

        if ( d > kickable_thr - ( 0.02 * i ) )
        {
#ifdef DEBUG_PRINT_LEVEL_1
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx dash=%d over kickable area. dist=%f my=(%.2f %.2f) ball=(%.2f %.2f)",
                          M_total_count, i, d,
                          self_cache[i].x, self_cache[i].y,
                          ball_pos.x, ball_pos.y );
#endif
            return false;
        }

        for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
                  end = wm.opponentsFromSelf().end();
              o != end;
              ++o )
        {
            if ( (*o)->distFromSelf() > 6.0 ) continue;

            if ( (*o)->pos().dist2( ball_pos ) < std::pow( (*o)->playerTypePtr()->kickableArea(), 2 ) )
            {
#ifdef DEBUG_PRINT_LEVEL_1
                dlog.addText( Logger::DRIBBLE,
                              "%d: xxx dash=%d exist opponent[%d] on the ball move line",
                              M_total_count, i, (*o)->unum() );
#endif
                return false;
            }
        }

        ball_vel *= bdecay;
        ball_pos += ball_vel;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::SafetyLevel
GeneratorOmniDribble::getSafetyLevel( const WorldModel & wm,
                                      const int n_dash,
                                      const Vector2D & last_ball_pos )
{
    CooperativeAction::SafetyLevel result = CooperativeAction::Safe;

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        if ( (*o)->distFromSelf() > 20.0 ) continue;

        int step = 0;
        CooperativeAction::SafetyLevel level = getOpponentSafetyLevel( n_dash,
                                                                       last_ball_pos,
                                                                       *o, &step );

#ifdef DEBUG_PRINT_LEVEL_1
        dlog.addText( Logger::DRIBBLE,
                      "%d: >>>> opp=%d (%.1f %.1f) step=%d safe=%d",
                      M_total_count,
                      (*o)->unum(), (*o)->pos().x, (*o)->pos().y, step, level );
#endif

        if ( result > level )
        {
            result = level;
            if ( result == CooperativeAction::Failure )
            {
                break;
            }
        }
    }

    return result;
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::SafetyLevel
GeneratorOmniDribble::getOpponentSafetyLevel( const int my_dash,
                                              const Vector2D & last_ball_pos,
                                              const PlayerObject * opponent,
                                              int * result_step )
{
    const ServerParam & SP = ServerParam::i();
    const PlayerType * ptype = opponent->playerTypePtr();
    const double control_area = ( opponent->goalie()
                                  && last_ball_pos.x > SP.theirPenaltyAreaLineX()
                                  && last_ball_pos.absY() > SP.penaltyAreaHalfWidth() )
        ? SP.catchableArea()
        : ptype->kickableArea();

    const Vector2D inertia_pos = opponent->inertiaPoint( 1 + my_dash );
    double ball_dist = inertia_pos.dist( last_ball_pos );
    double dash_dist = ball_dist - control_area;

    if ( ! opponent->isTackling()
         && dash_dist < 0.001 )
    {
#ifdef DEBUG_PRINT_LEVEL_2
        dlog.addText( Logger::DRIBBLE,
                      "%d: ______ already controllable",
                      M_total_count );
#endif
        *result_step = 0;
        return CooperativeAction::Failure;
    }

    int opp_dash = ptype->cyclesToReachDistance( dash_dist );
    int opp_turn = ( opponent->bodyCount() > 1
                     ? 0
                     : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                 opponent->body(),
                                                                 opponent->vel().r(),
                                                                 ball_dist,
                                                                 ( last_ball_pos - inertia_pos ).th(),
                                                                 control_area,
                                                                 true ) ); // use back dash
    int opp_step = ( opp_turn == 0
                   ? opp_turn + opp_dash
                   : opp_turn + opp_dash + 1 );
    if ( opponent->isTackling() )
    {
        opp_step += std::max( 0, ServerParam::i().tackleCycles() - opponent->tackleCount() - 2 );
    }
    else
    {
        opp_step -= std::min( 2, std::min( opponent->heardPosCount(), opponent->seenPosCount() ) );
    }

#ifdef DEBUG_PRINT_LEVEL_2
    dlog.addText( Logger::DRIBBLE,
                  "%d: ______ step=%d (t=%d, d=%d) bdist=%.2f dash_dist=%.2f",
                  M_total_count, opp_step, opp_turn, opp_dash,
                  ball_dist, dash_dist );
#endif
    *result_step = opp_step;

    if ( opp_step <= std::max( 0, 1 + my_dash - 3 ) ) return CooperativeAction::Failure;
    if ( opp_step <= std::max( 0, 1 + my_dash - 2 ) ) return CooperativeAction::Failure;
    if ( opp_step <= std::max( 0, 1 + my_dash - 1 ) ) return CooperativeAction::Failure;
    if ( opp_step <= 1 + my_dash ) return CooperativeAction::Failure;
    if ( opp_step <= 1 + my_dash + 1 ) return CooperativeAction::Dangerous;
    if ( opp_step <= 1 + my_dash + 2 ) return CooperativeAction::MaybeDangerous;
    if ( opp_step <= 1 + my_dash + 3 ) return CooperativeAction::MaybeDangerous;
    return CooperativeAction::Safe;
}
