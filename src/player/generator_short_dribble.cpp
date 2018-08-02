// -*-c++-*-

/*!
  \file generator_short_dribble.cpp
  \brief short step dribble course generator Source File
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

#include "generator_short_dribble.h"

#include "act_dribble.h"
#include "field_analyzer.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/geom/circle_2d.h>
#include <rcsc/geom/segment_2d.h>
#include <rcsc/timer.h>

#include <algorithm>
#include <limits>

#include <cmath>

// #define USE_TWO_STEP_KICK

// #define DEBUG_PROFILE
// #define DEBUG_PRINT
// #define DEBUG_PRINT_SIMULATE_DASHES
// #define DEBUG_PRINT_OPPONENT

// #define DEBUG_PRINT_SUCCESS_COURSE
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

}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorShortDribble::GeneratorShortDribble()
    : M_update_time( -1, 0 ),
      M_total_count( 0 ),
      M_queued_action_time( -1, 0 )
{
    M_courses.reserve( 128 );

    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorShortDribble &
GeneratorShortDribble::instance()
{
    static GeneratorShortDribble s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorShortDribble::clear()
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
GeneratorShortDribble::generate( const WorldModel & wm )
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
    // check queued action
    //

    if ( M_queued_action_time != wm.time() )
    {
        M_queued_action.reset();
    }
    else
    {
        M_courses.push_back( M_queued_action );
    }

    //
    // updater ball holder
    //
    if ( ! wm.self().isKickable()
         || wm.self().isFrozen() )
    {
        return;
    }

    M_first_ball_pos = wm.ball().pos();
    M_first_ball_vel = wm.ball().vel();

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    createCourses( wm );

    std::sort( M_courses.begin(), M_courses.end(),
               CooperativeAction::DistanceSorter( ServerParam::i().theirTeamGoalPos() ) );

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
GeneratorShortDribble::setQueuedAction( const rcsc::WorldModel & wm,
                                        CooperativeAction::Ptr action )
{
    const CooperativeAction::SafetyLevel safety_level = getSafetyLevel( wm,
                                                                        0, 0, action->dashCount(),
                                                                        wm.ball().vel(),
                                                                        wm.ball().inertiaPoint( action->dashCount() ) );
    if ( safety_level == CooperativeAction::Failure )
    {
        return;
    }
    action->setSafetyLevel( safety_level );

    M_queued_action_time = wm.time();
    M_queued_action = action;


}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorShortDribble::createCourses( const WorldModel & wm )
{
    static const int angle_div = 16;
    static const double angle_step = 360.0 / angle_div;

    const ServerParam & SP = ServerParam::i();

    const PlayerType & ptype = wm.self().playerType();

    const double my_first_speed = wm.self().vel().r();

    //
    // angle loop
    //

    for ( int a = 0; a < angle_div; ++a )
    {
        AngleDeg dash_angle = wm.self().body() + ( angle_step * a );

        //
        // angle filter
        //

        if ( wm.self().pos().x < 3.0
             && dash_angle.abs() > 100.0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (createTargetPoints) cancel(1) dash_angle=%.1f",
                          dash_angle.degree() );
#endif
            continue;
        }

        if ( wm.self().pos().x < -36.0
             && wm.self().pos().absY() < 20.0
             && dash_angle.abs() > 45.0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (createTargetPoints) cancel(2) dash_angle=%.1f",
                          dash_angle.degree() );
#endif
            continue;
        }

        int n_turn = 0;
        double dir_diff = 0.0;

        if ( a > 0 )
        {
            double my_speed = my_first_speed * ptype.playerDecay(); // first action is kick
            dir_diff = std::fabs( AngleDeg::normalize_angle( angle_step * a ) );

            while ( dir_diff > 10.0 )
            {
                dir_diff -= ptype.effectiveTurn( SP.maxMoment(), my_speed );
                if ( dir_diff < 0.0 ) dir_diff = 0.0;
                my_speed *= ptype.playerDecay();
                ++n_turn;
            }

            if ( n_turn >= 3 )
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::DRIBBLE,
                              __FILE__": (createTargetPoints) canceled dash_angle=%.1f n_turn=%d",
                              dash_angle.degree(), n_turn );
#endif
                continue;
            }
        }

        if ( angle_step * a < 180.0 )
        {
            dash_angle -= dir_diff;
        }
        else
        {
            dash_angle += dir_diff;
        }

        simulateKickTurnsDashes( wm, dash_angle, n_turn );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorShortDribble::simulateKickTurnsDashes( const WorldModel & wm,
                                                const AngleDeg & dash_angle,
                                                const int n_turn )
{
    //static const int max_dash = 5;
    static const int max_dash = 4; // 2009-06-29
    static const int min_dash = 2;
    //static const int min_dash = 1;

    std::vector< Vector2D > self_cache;
#ifdef USE_TWO_STEP_KICK
    std::vector< Vector2D > self_cache_2kick;
#endif

    //
    // create self position cache
    //
    createSelfCache( wm, dash_angle, 1, n_turn, max_dash, self_cache );
#ifdef USE_TWO_STEP_KICK
    createSelfCache( wm, dash_angle, 2, n_turn, max_dash, self_cache_2kick );
#endif

    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    const Vector2D trap_rel
        //= Vector2D::polar2vector( ptype.playerSize() + ptype.kickableMargin() * 0.2 + SP.ballSize(),
        = Vector2D::polar2vector( ptype.playerSize() + SP.ballSize(), dash_angle );

    const double max_x = ( SP.keepawayMode()
                           ? SP.keepawayLength() * 0.5 - 1.5
                           : SP.pitchHalfLength() - 1.0 );
    const double max_y = ( SP.keepawayMode()
                           ? SP.keepawayWidth() * 0.5 - 1.5
                           : SP.pitchHalfWidth() - 1.0 );

#ifdef DEBUG_PRINT
    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (simulateKickTurnsDashes) dash_angle=%.1f n_turn=%d",
                  dash_angle.degree(), n_turn );
#endif

    for ( int n_dash = max_dash; n_dash >= min_dash; --n_dash )
    {
        int n_kick = 1;
        Vector2D receive_pos = self_cache[n_turn + n_dash] + trap_rel;

        ++M_total_count;

#ifdef DEBUG_PRINT
        dlog.addText( Logger::DRIBBLE,
                      "%d: n_turn=%d n_dash=%d ball_trap=(%.3f %.3f)",
                      M_total_count,
                      n_turn, n_dash,
                      receive_pos.x, receive_pos.y );
#endif
        if ( receive_pos.absX() > max_x
             || receive_pos.absY() > max_y )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx out of pitch", M_total_count );
            debug_paint_failed( M_total_count, receive_pos );
#endif
            continue;
        }

        const double term
            = ( 1.0 - std::pow( SP.ballDecay(), 1 + n_turn + n_dash ) )
            / ( 1.0 - SP.ballDecay() );
        const Vector2D first_vel = ( receive_pos - M_first_ball_pos ) / term;
        const Vector2D kick_accel = first_vel - M_first_ball_vel;
        const double kick_power = kick_accel.r() / wm.self().kickRate();

        // never kickable
        if ( kick_power > SP.maxPower()
             || kick_accel.r2() > std::pow( SP.ballAccelMax(), 2 )
             || first_vel.r2() > std::pow( SP.ballSpeedMax(), 2 ) )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx cannot kick. first_vel=(%.1f %.1f, r=%.2f) accel=(%.1f %.1f)r=%.2f power=%.1f",
                          M_total_count,
                          first_vel.x, first_vel.y, first_vel.r(),
                          kick_accel.x, kick_accel.y, kick_accel.r(),
                          kick_power );
            debug_paint_failed( M_total_count, receive_pos );
#endif
            continue;
        }

        // collision check
        if ( ( M_first_ball_pos + first_vel ).dist2( self_cache[0] )
             < std::pow( ptype.playerSize() + SP.ballSize() + 0.1, 2 ) )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx collision. first_vel=(%.1f %.1f, r=%.2f) accel=(%.1f %.1f)r=%.2f power=%.1f",
                          M_total_count,
                          first_vel.x, first_vel.y, first_vel.r(),
                          kick_accel.x, kick_accel.y, kick_accel.r(),
                          kick_power );
            debug_paint_failed( M_total_count, receive_pos );
#endif
#ifdef USE_TWO_STEP_KICK
            n_kick = 2;
            receive_pos = self_cache_2kick[1 + n_turn + n_dash] + trap_rel;
#else
            continue;
#endif
        }

        const CooperativeAction::SafetyLevel safety_level = getSafetyLevel( wm, n_kick, n_turn, n_dash, first_vel, receive_pos );

        if ( safety_level != CooperativeAction::Failure )
        {
            CooperativeAction::Ptr ptr = ActDribble::create_normal( wm.self().unum(),
                                                                    receive_pos,
                                                                    dash_angle.degree(),
                                                                    first_vel,
                                                                    n_kick,
                                                                    n_turn,
                                                                    n_dash,
                                                                    "shortDribble" );
            ptr->setIndex( M_total_count );
            ptr->setSafetyLevel( safety_level );
            M_courses.push_back( ptr );

#ifdef DEBUG_PRINT_SUCCESS_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: ok pos=(%.2f %.2f) first_vel=(%.1f %.1f, r=%.2f) n_turn=%d n_dash=%d safe=%d",
                          M_total_count,
                          receive_pos.x, receive_pos.y,
                          first_vel.x, first_vel.y, first_vel.r(),
                          n_turn, n_dash, safety_level );
            dlog.addCircle( Logger::DRIBBLE,
                            receive_pos.x, receive_pos.y, 0.1,
                            "#00ff00" );
            char num[8]; snprintf( num, 8, "%d:%d", M_total_count, safety_level );
            dlog.addMessage( Logger::DRIBBLE,
                             receive_pos, num );
#endif
            if ( safety_level == CooperativeAction::Safe )
            {
                break;
            }
        }
#ifdef DEBUG_PRINT_FAILED_COURSE
        else
        {
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx pos=(%.2f %.2f) first_vel=(%.1f %.1f, r=%.2f) n_turn=%d n_dash=%d safe=%d",
                          M_total_count,
                          receive_pos.x, receive_pos.y,
                          first_vel.x, first_vel.y, first_vel.r(),
                          n_turn, n_dash, safety_level );
            debug_paint_failed( M_total_count, receive_pos );
        }
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorShortDribble::createSelfCache( const WorldModel & wm,
                                        const AngleDeg & dash_angle,
                                        const int n_kick,
                                        const int n_turn,
                                        const int n_dash,
                                        std::vector< Vector2D > & self_cache )
{
    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    self_cache.clear();

    StaminaModel stamina_model = wm.self().staminaModel();

    Vector2D my_pos = wm.self().pos();
    Vector2D my_vel = wm.self().vel();

    for ( int i = 0; i < n_kick; ++i )
    {
        my_pos += my_vel;
        my_vel *= ptype.playerDecay();

        self_cache.push_back( my_pos );
    }

    for ( int i = 0; i < n_turn; ++i )
    {
        my_pos += my_vel;
        my_vel *= ptype.playerDecay();
        self_cache.push_back( my_pos );
    }

    stamina_model.simulateWaits( ptype, n_kick + n_turn );

    const Vector2D unit_vec = Vector2D::polar2vector( 1.0, dash_angle );

    for ( int i = 0; i < n_dash; ++i )
    {
        double available_stamina = std::max( 0.0,
                                             stamina_model.stamina() - SP.recoverDecThrValue() - 300.0 );
        double dash_power = std::min( available_stamina, SP.maxDashPower() );
        Vector2D dash_accel = unit_vec * ( dash_power
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
CooperativeAction::SafetyLevel
GeneratorShortDribble::getSafetyLevel( const WorldModel & wm,
                                       const int n_kick,
                                       const int n_turn,
                                       const int n_dash,
                                       const Vector2D & ball_first_vel,
                                       const Vector2D & /*receive_pos*/ )
{
    CooperativeAction::SafetyLevel result = CooperativeAction::Safe;

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        CooperativeAction::SafetyLevel level = getOpponentSafetyLevel( wm, *o, n_kick, n_turn, n_dash, ball_first_vel );

        if ( result > level )
        {
            result = level;
            if ( result == CooperativeAction::Failure )
            {
                break;
            }
        }
    }

#ifdef DEBUG_PRINT
    dlog.addText( Logger::DRIBBLE,
                  "%d: safety_level=%d",
                  M_total_count, result );
#endif
    return result;
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::SafetyLevel
GeneratorShortDribble::getOpponentSafetyLevel( const WorldModel & wm,
                                               const PlayerObject * opponent,
                                               const int n_kick,
                                               const int n_turn,
                                               const int n_dash,
                                               const Vector2D & ball_first_vel )

{
    const ServerParam & SP = ServerParam::i();
    const PlayerType * ptype = opponent->playerTypePtr();
    const double opponent_speed = opponent->vel().r();

    const int max_step = n_kick + n_turn + n_dash;

    CooperativeAction::SafetyLevel result = CooperativeAction::Safe;

    Vector2D ball_pos = wm.ball().pos();
    Vector2D ball_vel = ball_first_vel;
    for ( int step = 1; step <= max_step; ++step )
    {
        ball_pos += ball_vel;
        ball_vel *= SP.ballDecay();

        const double control_area = ( opponent->goalie()
                                      && ball_pos.x > SP.theirPenaltyAreaLineX()
                                      && ball_pos.absY() < SP.penaltyAreaHalfWidth()
                                      ? SP.catchableArea()
                                      : ptype->kickableArea() );

        Vector2D opp_pos = opponent->inertiaPoint( step );
        double ball_dist = opp_pos.dist( ball_pos );
        double dash_dist = ball_dist - control_area;

        if ( ! opponent->isTackling()
             && dash_dist < 0.001 )
        {
#ifdef DEBUG_PRINT_OPPONENT
            dlog.addText( Logger::DRIBBLE,
                          "%d: opponent[%d](%.2f %.2f) step=%d prob=1 controllable",
                          M_total_count,
                          opponent->unum(), opponent->pos().x, opponent->pos().y, step );
#endif
            return CooperativeAction::Failure;
        }

        if ( dash_dist > ptype->realSpeedMax() * ( step + opponent->posCount() ) + 1.0 )
        {
            continue;
        }

        int opp_dash = ptype->cyclesToReachDistance( dash_dist );
        int opp_turn = ( opponent->bodyCount() > 1
                         ? 0
                         : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                     opponent->body(),
                                                                     opponent_speed,
                                                                     ball_dist,
                                                                     ( ball_pos - opp_pos ).th(),
                                                                     control_area,
                                                                     true ) ); // use back dash
        int opp_step = ( opp_turn == 0
                         ? opp_dash
                         : opp_turn + opp_dash + 1 );
        if ( opponent->isTackling() )
        {
            opp_step += std::max( 0, ServerParam::i().tackleCycles() - opponent->tackleCount() - 2 );
        }

        if ( wm.self().pos().x < 0.0 )
        {
            opp_step -= std::min( 8, std::min( opponent->heardPosCount(), opponent->seenPosCount() ) );
        }
        else
        {
            opp_step -= std::min( 3, std::min( opponent->heardPosCount(), opponent->seenPosCount() ) );
        }

        CooperativeAction::SafetyLevel level = CooperativeAction::Failure;

        if ( opp_step <= std::max( 0, step - 3 ) ) level = CooperativeAction::Failure;
        else if ( opp_step <= std::max( 0, step - 2 ) ) level = CooperativeAction::Failure;
        else if ( opp_step <= std::max( 0, step - 1 ) ) level = CooperativeAction::Failure;
        else if ( opp_step <= step ) level = CooperativeAction::Failure;
        else if ( opp_step <= step + 1 ) level = CooperativeAction::Dangerous;
        else if ( opp_step <= step + 2 ) level = CooperativeAction::MaybeDangerous;
        else if ( opp_step <= step + 3 ) level = CooperativeAction::MaybeDangerous;
        else level = CooperativeAction::Safe;

#ifdef DEBUG_PRINT_OPPONENT
        dlog.addText( Logger::DRIBBLE,
                      "%d: opponent[%d](%.2f %.2f) bstep=%d ostep=%d(t%d:d%d) safe=%d",
                      M_total_count,
                      opponent->unum(), opponent->pos().x, opponent->pos().y,
                      step, opp_step, opp_turn, opp_dash, level );
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
