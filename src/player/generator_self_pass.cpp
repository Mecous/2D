// -*-c++-*-

/*!
  \file generator_self_pass.cpp
  \brief self pass generator Source File
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

#include "generator_self_pass.h"

#include "act_dribble.h"
#include "field_analyzer.h"

#include <rcsc/action/kick_table.h>
#include <rcsc/player/world_model.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/timer.h>

#include <limits>
#include <cmath>

// #define DEBUG_PROFILE
// #define DEBUG_PRINT_CANDIDATES

// #define DEBUG_PRINT
// #define DEBUG_PRINT_OPPONENT
// #define DEBUG_PRINT_OPPONENT_LEVEL2
// #define DEBUG_PRINT_SELF_CACHE

// #define DEBUG_PRINT_SUCCESS_COURSE
// #define DEBUG_PRINT_FAILED_COURSE

using namespace rcsc;


namespace {

inline
void
debug_paint( const int count,
             const Vector2D & receive_point,
             const int safety_level,
             const char * color )
{
    dlog.addRect( Logger::DRIBBLE,
                  receive_point.x - 0.1, receive_point.y - 0.1,
                  0.2, 0.2,
                  color );
    char num[32];
    snprintf( num, 32, "%d:%d", count, safety_level );
    dlog.addMessage( Logger::DRIBBLE,
                     receive_point, num );
}

inline
const char *
safety_level_string( const int level )
{
    switch ( level ) {
    case CooperativeAction::Failure:
        return "Failure";
    case CooperativeAction::Dangerous:
        return "Dangerous";
    case CooperativeAction::MaybeDangerous:
        return "MaybeDangerous";
    case CooperativeAction::Safe:
        return "Safe";
    default:
        break;
    }
    return "???";
}
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorSelfPass::GeneratorSelfPass()
{
    M_courses.reserve( 128 );

    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorSelfPass &
GeneratorSelfPass::instance()
{
    static GeneratorSelfPass s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorSelfPass::clear()
{
    M_total_count = 0;
    M_courses.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorSelfPass::generate( const WorldModel & wm )
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

    if ( ! wm.self().isKickable()
         || wm.self().isFrozen() )
    {
        return;
    }

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    createCourses( wm );

    std::sort( M_courses.begin(), M_courses.end(),
               //CooperativeAction::DistanceSorter( ServerParam::i().theirTeamGoalPos() ) );
               CooperativeAction::DistanceSorter( Vector2D( 41.5, 0.0 ) ) );
    if ( M_courses.size() > 20 )
    {
        M_courses.erase( M_courses.begin() + 20, M_courses.end() );
    }

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (generate) PROFILE size=%d/%d elapsed %.3f [ms]",
                  (int)M_courses.size(),
                  M_total_count,
                  timer.elapsedReal() );
#endif
#ifdef DEBUG_PRINT_CANDIDATES
    debugPrintCandidates( wm );
#endif
}


/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorSelfPass::debugPrintCandidates( const WorldModel & wm )
{
    dlog.addText( Logger::DRIBBLE,
                  "(SelfPass) candidate size = %zd",
                  M_courses.size() );

    for ( std::vector< CooperativeAction::Ptr >::const_iterator it = M_courses.begin(),
              end = M_courses.end();
          it != end;
          ++it )
    {
        dlog.addText( Logger::DRIBBLE,
                      "(SelfPass) %d target=(%.2f %.2f) angle=%.1f step=%d (k:%d t:%d d%d) safe=%s",
                      (*it)->index(), (*it)->targetBallPos().x, (*it)->targetBallPos().y,
                      ( (*it)->targetBallPos() - wm.self().pos() ).th().degree(),
                      (*it)->durationTime(),
                      (*it)->kickCount(), (*it)->turnCount(), (*it)->dashCount(),
                      safety_level_string( (*it)->safetyLevel() ) );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorSelfPass::createCourses( const WorldModel & wm )
{
    //static const int ANGLE_DIVS = 40;
    static const int ANGLE_DIVS = 30;
    static const double ANGLE_STEP = 360.0 / ANGLE_DIVS;

    static std::vector< Vector2D > self_cache( 24 );

    const ServerParam & SP = ServerParam::i();

    const Vector2D ball_pos = wm.ball().pos();
    const AngleDeg body_angle = wm.self().body();

    const int min_dash = 5;
    const int max_dash = ( ball_pos.x < -20.0 ? 6
                           : ball_pos.x < 0.0 ? 7
                           //: ball_pos.x < 10.0 ? 13
                           : ball_pos.x < 10.0 ? 30
                           : ball_pos.x < 20.0 ? 15
                           //: 20 );
                           : 10 );
    //const int max_dash = 30;

    const PlayerType & ptype = wm.self().playerType();

    const double max_effective_turn
        = ptype.effectiveTurn( SP.maxMoment(),
                               wm.self().vel().r() * ptype.playerDecay() );

    const Vector2D our_goal = ServerParam::i().ourTeamGoalPos();
    const double goal_dist_thr2 = std::pow( 18.0, 2 ); // Magic Number

    for ( int a = 0; a < ANGLE_DIVS; ++a )
    {
        const double add_angle = ANGLE_STEP * a;

        int n_kick = 1;
        int n_turn = 0;

        if ( a != 0 )
        {
            const AngleDeg angle = add_angle;
            if ( angle.abs() > max_effective_turn )
            {
                // cannot turn by 1 step
#ifdef DEBUG_PRINT_FAILED_COURSE
                dlog.addText( Logger::DRIBBLE,
                              "?: xxx SelfPass rel_angle=%.1f > maxTurn=%.1f cannot turn by 1 step.",
                              add_angle, max_effective_turn );
#endif
                continue;
            }

            n_turn = 1;
        }

        const AngleDeg dash_angle = body_angle + add_angle;

        if ( ball_pos.x < SP.theirPenaltyAreaLineX() + 5.0
             && dash_angle.abs() > 85.0 ) // Magic Number
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "?: xxx SelfPass angle=%.1f over angle.",
                          dash_angle.degree() );
#endif
            continue;
        }

        createSelfCache( wm, dash_angle,
                         n_kick, n_turn, max_dash,
                         self_cache );

        int n_dash = self_cache.size() - n_kick - n_turn;

        if ( n_dash < min_dash )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "?: xxx SelfPass angle=%.1f turn=%d dash=%d too short dash step.",
                          dash_angle.degree(),
                          n_turn, n_dash );
#endif
            continue;
        }

        int count = 0;
        int dash_dec = 5;
        for ( ; n_dash >= min_dash; n_dash -= dash_dec )
        {
            ++M_total_count;

            if ( n_dash <= 20 )
            {
                dash_dec = 2;
            }
            else if ( n_dash <= 10 )
            {
                dash_dec = 1;
            }

            Vector2D receive_pos = self_cache[n_kick + n_turn + n_dash - 1];

            if ( receive_pos.dist2( our_goal ) < goal_dist_thr2 )
            {
#ifdef DEBUG_PRINT_FAILED_COURSE
                dlog.addText( Logger::DRIBBLE,
                              "%d: xxx SelfPass step=%d(t:%d,d:%d) pos=(%.1f %.1f) near our goal",
                              M_total_count,
                              n_kick + n_turn + n_dash, n_turn, n_dash,
                              receive_pos.x, receive_pos.y );
#endif
                continue;
            }

            if ( ! canKickOneStep( wm, n_turn, n_dash, receive_pos ) )
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::DRIBBLE,
                              "%d: ===== SelfPass step=%d(t:%d,d:%d) pos=(%.1f %.1f) cannot kick by one step.",
                              M_total_count,
                              n_kick + n_turn + n_dash, n_turn, n_dash,
                              receive_pos.x, receive_pos.y );
#endif
                n_kick = 2;
                receive_pos = simulateSelfPos( wm, dash_angle, n_kick, n_turn, n_dash );
            }

#ifdef DEBUG_PRINT
            //#if (defined DEBUG_PRINT_SUCCESS_COURSE) || (defined DEBUG_PRINT_SUCCESS_COURSE)
            dlog.addText( Logger::DRIBBLE,
                          "%d ===== SelfPass angle=%.1f turn=%d dash=%d pos=(%.1f %.1f) =====",
                          M_total_count,
                          dash_angle.degree(),
                          n_turn, n_dash,
                          receive_pos.x, receive_pos.y );
#endif
            const double first_speed = SP.firstBallSpeed( ball_pos.dist( receive_pos ),
                                                          n_kick + n_turn + n_dash );
            const Vector2D first_vel = ( receive_pos - ball_pos ).setLengthVector( first_speed );

            CooperativeAction::SafetyLevel safety_level = getSafetyLevel( wm,
                                                                          n_kick, n_turn, n_dash,
                                                                          first_vel, receive_pos );
            if ( safety_level != CooperativeAction::Failure )
            {
                CooperativeAction::Ptr ptr = ActDribble::create_normal( wm.self().unum(),
                                                                        receive_pos,
                                                                        dash_angle.degree(),
                                                                        first_vel,
                                                                        n_kick,
                                                                        n_turn,
                                                                        n_dash,
                                                                        "SelfPass" );
                ptr->setSafetyLevel( safety_level );
                ptr->setIndex( M_total_count );
                M_courses.push_back( ptr );

#ifdef DEBUG_PRINT_SUCCESS_COURSE
                dlog.addText( Logger::DRIBBLE,
                              "%d: ok SelfPass step=%d(t:%d,d:%d) pos=(%.1f %.1f) speed=%.3f safe=%d",
                              M_total_count,
                              1 + n_turn + n_dash, n_turn, n_dash,
                              receive_pos.x, receive_pos.y,
                              first_speed, safety_level );
                debug_paint( M_total_count, receive_pos, safety_level, "#0F0" );
#endif

                ++count;
                if ( count >= 10 )
                {
                    break;
                }
            }
#ifdef DEBUG_PRINT_FAILED_COURSE
            else
            {
                dlog.addText( Logger::DRIBBLE,
                              "%d: xx SelfPass step=%d(t:%d,d:%d) pos=(%.1f %.1f) speed=%.3f safe=%d",
                              M_total_count,
                              1 + n_turn + n_dash, n_turn, n_dash,
                              receive_pos.x, receive_pos.y,
                              first_speed, safety_level );
                debug_paint( M_total_count, receive_pos, safety_level, "#F00" );
            }
#endif
        }
    }

}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorSelfPass::createSelfCache( const WorldModel & wm,
                                    const AngleDeg & dash_angle,
                                    const int n_kick,
                                    const int n_turn,
                                    const int n_dash,
                                    std::vector< Vector2D > & self_cache )
{
    self_cache.clear();

    const PlayerType & ptype = wm.self().playerType();

    const double dash_power = ServerParam::i().maxDashPower();
    const double stamina_thr = ( wm.self().staminaModel().capacityIsEmpty()
                                 ? -ptype.extraStamina() // minus value to set available stamina
                                 : ServerParam::i().recoverDecThrValue() + 350.0 );

    StaminaModel stamina_model = wm.self().staminaModel();

    Vector2D my_pos = wm.self().pos();
    Vector2D my_vel = wm.self().vel();

    //
    // kicks
    //
    for ( int i = 0; i < n_kick; ++i )
    {
        my_pos += my_vel;
        my_vel *= ptype.playerDecay();
        stamina_model.simulateWait( ptype );
        self_cache.push_back( my_pos );
    }

    //
    // turns
    //
    for ( int i = 0; i < n_turn; ++i )
    {
        my_pos += my_vel;
        my_vel *= ptype.playerDecay();
        stamina_model.simulateWait( ptype );
        self_cache.push_back( my_pos );
    }

    //
    // simulate dashes
    //
    for ( int i = 0; i < n_dash; ++i )
    {
        if ( stamina_model.stamina() < stamina_thr )
        {
#ifdef DEBUG_PRINT_SELF_CACHE
            dlog.addText( Logger::DRIBBLE,
                          "?: SelfPass (createSelfCache) turn=%d dash=%d. stamina=%.1f < threshold",
                          n_turn, n_dash, stamina_model.stamina() );
#endif
            break;
        }

        double available_stamina =  std::max( 0.0, stamina_model.stamina() - stamina_thr );
        double actual_dash_power = std::min( dash_power, available_stamina );
        double accel_mag = actual_dash_power * ptype.dashPowerRate() * stamina_model.effort();
        Vector2D dash_accel = Vector2D::polar2vector( accel_mag, dash_angle );

        // TODO: check playerSpeedMax & update actual_dash_power if necessary
        // if ( ptype.normalizeAccel( my_vel, &dash_accel ) ) actual_dash_power = ...

        my_vel += dash_accel;
        my_pos += my_vel;

// #ifdef DEBUG_PRINT_SELF_CACHE
//         dlog.addText( Logger::DRIBBLE,
//                       "___ dash=%d accel=(%.2f %.2f)r=%.2f th=%.1f pos=(%.2f %.2f) vel=(%.2f %.2f)",
//                       i + 1,
//                       dash_accel.x, dash_accel.y, dash_accel.r(), dash_accel.th().degree(),
//                       my_pos.x, my_pos.y,
//                       my_vel.x, my_vel.y );
// #endif

        if ( my_pos.x > ServerParam::i().pitchHalfLength() - 2.5 )
        {
#ifdef DEBUG_PRINT_SELF_CACHE
            dlog.addText( Logger::DRIBBLE,
                          "?: SelfPass (createSelfCache) turn=%d dash=%d. my_x=%.2f. over goal line",
                          n_turn, n_dash, my_pos.x );
#endif
            break;
        }

        if ( my_pos.absY() > ServerParam::i().pitchHalfWidth() - 3.0
             && ( ( my_pos.y > 0.0 && dash_angle.degree() > 0.0 )
                  || ( my_pos.y < 0.0 && dash_angle.degree() < 0.0 ) )
             )
        {
#ifdef DEBUG_PRINT_SELF_CACHE
            dlog.addText( Logger::DRIBBLE,
                          "?: SelfPass (createSelfCache) turn=%d dash=%d."
                          " my_pos=(%.2f %.2f). dash_angle=%.1f",
                          n_turn, n_dash,
                          my_pos.x, my_pos.y,
                          dash_angle.degree() );
            dlog.addText( Logger::DRIBBLE,
                          "__ dash_accel=(%.2f %.2f)r=%.2f  vel=(%.2f %.2f)r=%.2f th=%.1f",
                          dash_accel.x, dash_accel.y, accel_mag,
                          my_vel.x, my_vel.y, my_vel.r(), my_vel.th().degree() );

#endif

            break;
        }

        my_vel *= ptype.playerDecay();
        stamina_model.simulateDash( ptype, actual_dash_power );

        self_cache.push_back( my_pos );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
GeneratorSelfPass::simulateSelfPos( const rcsc::WorldModel & wm,
                                    const rcsc::AngleDeg & dash_angle,
                                    const int n_kick,
                                    const int n_turn,
                                    const int n_dash )
{

    const PlayerType & ptype = wm.self().playerType();

    const double dash_power = ServerParam::i().maxDashPower();
    const double stamina_thr = ( wm.self().staminaModel().capacityIsEmpty()
                                 ? -ptype.extraStamina() // minus value to set available stamina
                                 : ServerParam::i().recoverDecThrValue() + 350.0 );

    StaminaModel stamina_model = wm.self().staminaModel();

    Vector2D my_pos = wm.self().pos();
    Vector2D my_vel = wm.self().vel();

    //
    // kicks
    //
    for ( int i = 0; i < n_kick; ++i )
    {
        my_pos += my_vel;
        my_vel *= ptype.playerDecay();
        stamina_model.simulateWait( ptype );
    }

    //
    // turns
    //
    for ( int i = 0; i < n_turn; ++i )
    {
        my_pos += my_vel;
        my_vel *= ptype.playerDecay();
        stamina_model.simulateWait( ptype );
    }

    //
    // simulate dashes
    //
    for ( int i = 0; i < n_dash; ++i )
    {
        if ( stamina_model.stamina() < stamina_thr )
        {
            break;
        }

        double available_stamina =  std::max( 0.0, stamina_model.stamina() - stamina_thr );
        double actual_dash_power = std::min( dash_power, available_stamina );
        double accel_mag = actual_dash_power * ptype.dashPowerRate() * stamina_model.effort();
        Vector2D dash_accel = Vector2D::polar2vector( accel_mag, dash_angle );

        // TODO: check playerSpeedMax & update actual_dash_power if necessary
        // if ( ptype.normalizeAccel( my_vel, &dash_accel ) ) actual_dash_power = ...

        my_vel += dash_accel;
        my_pos += my_vel;

        if ( my_pos.x > ServerParam::i().pitchHalfLength() - 2.5 )
        {
            break;
        }

        //const AngleDeg target_angle = ( my_pos - first_ball_pos ).th();

        if ( my_pos.absY() > ServerParam::i().pitchHalfWidth() - 3.0
             && ( ( my_pos.y > 0.0 && dash_angle.degree() > 0.0 )
                  || ( my_pos.y < 0.0 && dash_angle.degree() < 0.0 ) )
             )
        {
            break;
        }

        my_vel *= ptype.playerDecay();
        stamina_model.simulateDash( ptype, actual_dash_power );
    }

    return my_pos;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
GeneratorSelfPass::canKickOneStep( const WorldModel & wm,
                                   const int n_turn,
                                   const int n_dash,
                                   const Vector2D & receive_pos )
{
    const ServerParam & SP = ServerParam::i();

    const Vector2D ball_pos = wm.ball().pos();
    const Vector2D ball_vel = wm.ball().vel();
    const AngleDeg target_angle = ( receive_pos - ball_pos ).th();

    //
    // check kick possibility
    //
    double first_speed = calc_first_term_geom_series( ball_pos.dist( receive_pos ),
                                                      SP.ballDecay(),
                                                      1 + n_turn + n_dash );
    Vector2D max_vel = KickTable::calc_max_velocity( target_angle,
                                                     wm.self().kickRate(),
                                                     ball_vel );

#ifdef DEBUG_PRINT_FAILED_COURSE
        dlog.addText( Logger::DRIBBLE,
                      "%d: first_speed=%.3f  max_one_step=%.3f",
                      M_total_count, first_speed, max_vel.r() );
#endif

    if ( max_vel.r2() < std::pow( first_speed, 2 ) )
    {
#ifdef DEBUG_PRINT_FAILED_COURSE
        dlog.addText( Logger::DRIBBLE,
                      "%d: xxx SelfPass step=%d(t:%d,d=%d) cannot kick by 1 step."
                      " first_speed=%.2f > max_speed=%.2f",
                      M_total_count,
                      1 + n_turn + n_dash, n_turn, n_dash,
                      first_speed,
                      max_vel.r() );
        debug_paint( M_total_count, receive_pos, 0.0, "#F00" );
#endif
        return false;
    }

    //
    // check collision
    //
    const Vector2D my_next = wm.self().pos() + wm.self().vel();
    const Vector2D ball_next
        = ball_pos
        + ( receive_pos - ball_pos ).setLengthVector( first_speed );

    if ( my_next.dist2( ball_next ) < std::pow( wm.self().playerType().playerSize()
                                                + SP.ballSize()
                                                + 0.1,
                                                2 ) )
    {
#ifdef DEBUG_PRINT_FAILED_COURSE
        dlog.addText( Logger::DRIBBLE,
                      "%d: xxx SelfPass step=%d(t:%d,d=%d) maybe collision. next_dist=%.3f first_speed=%.2f",
                      M_total_count,
                      1 + n_turn + n_dash, n_turn, n_dash,
                      my_next.dist( ball_next ),
                      first_speed );
        debug_paint( M_total_count, receive_pos, 0.0, "#F00" );
#endif
        return false;
    }

    //
    // check opponent kickable area
    //

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        const PlayerType * ptype = (*o)->playerTypePtr();
        Vector2D o_next = (*o)->pos() + (*o)->vel();

        const double control_area = ( ( (*o)->goalie()
                                        && ball_next.x > SP.theirPenaltyAreaLineX()
                                        && ball_next.absY() < SP.penaltyAreaHalfWidth() )
                                      ? SP.catchableArea()
                                      : ptype->kickableArea() );

        if ( ball_next.dist2( o_next ) < std::pow( control_area + 0.1, 2 ) )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx SelfPass (canKick) opponent may be kickable(1) dist=%.3f < control=%.3f + 0.1",
                          M_total_count, ball_next.dist( o_next ), control_area );
            debug_paint( M_total_count, receive_pos, 0.0, "#F00" );
#endif
            return false;
        }

        if ( (*o)->bodyCount() <= 1 )
        {
            o_next += Vector2D::from_polar( SP.maxDashPower() * ptype->dashPowerRate() * ptype->effortMax(),
                                            (*o)->body() );
        }
        else
        {
            o_next += (*o)->vel().setLengthVector( SP.maxDashPower()
                                                   * ptype->dashPowerRate()
                                                   * ptype->effortMax() );
        }

        if ( ball_next.dist2( o_next ) < std::pow( control_area, 2 ) )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d xxx SelfPass (canKick) opponent may be kickable(2) dist=%.3f < control=%.3f",
                          M_total_count, ball_next.dist( o_next ), control_area );
            debug_paint( M_total_count, receive_pos, 0.0, "#F00" );
#endif
            return false;
        }

    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::SafetyLevel
GeneratorSelfPass::getSafetyLevel( const WorldModel & wm,
                                   const int n_kick,
                                   const int n_turn,
                                   const int n_dash,
                                   const Vector2D & ball_first_vel,
                                   const Vector2D & /*receive_pos*/ )
{
    const AngleDeg ball_move_angle = ball_first_vel.th();

    CooperativeAction::SafetyLevel result = CooperativeAction::Safe;

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        CooperativeAction::SafetyLevel level = getOpponentSafetyLevel( wm, *o, n_kick, n_turn, n_dash,
                                                                       ball_first_vel, ball_move_angle );
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
                  "%d: safe=%d",
                  M_total_count, result );
#endif
    return result;
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::SafetyLevel
GeneratorSelfPass::getOpponentSafetyLevel( const WorldModel & wm,
                                           const PlayerObject * opponent,
                                           const int n_kick,
                                           const int n_turn,
                                           const int n_dash,
                                           const Vector2D & ball_first_vel,
                                           const AngleDeg & ball_move_angle )
{
    const ServerParam & SP = ServerParam::i();
    const PlayerType * ptype = opponent->playerTypePtr();

    const double opponent_speed = ( opponent->seenVelCount() <= opponent->velCount()
                                    ? opponent->seenVel().r()
                                    : opponent->vel().r() );
    const int max_step = n_kick + n_turn + n_dash;
    //const Vector2D ball_to_opp_rel = ( opponent->pos() - wm.ball().pos() ).rotatedVector( -ball_first_vel.th() );
    const bool opponent_is_back = ( ( ball_move_angle - opponent->angleFromBall() ).abs() > 100.0 );

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

        const int opponent_dash = ptype->cyclesToReachDistance( dash_dist );
        const int opponent_turn = ( opponent->bodyCount() > 1
                                    ? 0
                                    : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                                opponent->body(),
                                                                                opponent_speed,
                                                                                ball_dist,
                                                                                ( ball_pos - opp_pos ).th(),
                                                                                control_area,
                                                                                true ) ); // use back dash
        int opponent_step = ( opponent_turn == 0
                              ? opponent_dash
                              : opponent_turn + opponent_dash + n_kick );
        if ( opponent->isTackling() )
        {
            opponent_step += 5; // magic number
        }

        // if ( ( opp_pos - wm.self().pos() ).innerProduct( ball_pos - wm.self().pos() ) < 0.0 )
        if ( opponent_is_back )
        {
            opponent_step -= bound( 0, static_cast< int >( std::floor( opponent->posCount() * 0.5 ) ), 3 );
        }
        else
        {
            opponent_step -= bound( 0, static_cast< int >( std::floor( opponent->posCount() * 0.75 ) ), 6 );
        }

        if ( wm.self().pos().x < wm.offsideLineX() - 5.0 )
        {
            opponent_step -= 1;
        }

        if ( opponent->goalie() )
        {
            opponent_step += 2;
        }

        CooperativeAction::SafetyLevel level = CooperativeAction::Failure;

        if ( opponent_step <= step - 3 ) level = CooperativeAction::Failure;
        else if ( opponent_step <= step - 2 ) level = CooperativeAction::Failure;
        else if ( opponent_step <= step - 1 ) level = CooperativeAction::Failure;
        else if ( opponent_step <= step ) level = CooperativeAction::Failure;
        else if ( opponent_step <= step + 1 ) level = CooperativeAction::Dangerous;
        else if ( opponent_step <= step + 2 ) level = CooperativeAction::MaybeDangerous;
        //else if ( opponent_step <= step + 3 ) level = CooperativeAction::MaybeDangerous;
        else level = CooperativeAction::Safe;

#ifdef DEBUG_PRINT_OPPONENT
        dlog.addText( Logger::DRIBBLE,
                      "%d: opponent[%d](%.2f %.2f) bstep=%d ostep=%d(t%d,d%d) safe=%d",
                      M_total_count,
                      opponent->unum(), opponent->pos().x, opponent->pos().y,
                      step, opponent_step, opponent_turn, opponent_dash, level );
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
