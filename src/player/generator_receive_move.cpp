// -*-c++-*-

/*!
  \file generator_receive_move.cpp
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

#include "generator_receive_move.h"

#include "field_analyzer.h"
#include "strategy.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/math_util.h>
#include <rcsc/timer.h>

#include <algorithm>
#include <cmath>

// #define DEBUG_PROFILE

// #define DEBUG_UPDATE_PASSER
// #define DEBUG_BALL_MOVE_STEP

// #define DEBUG_DIRECT_PASS
// #define DEBUG_AROUND_HOME
// #define DEBUG_LEADING_PASS
// #define DEBUG_THROUGH_PASS

// #define DEBUG_SAFETY_LEVEL

// #define DEBUG_PRINT_RESULTS


using namespace rcsc;

namespace {

const int DIRECT_PASS_MIN_RECEIVE_STEP = 3;
const double DIRECT_PASS_MIN_DIST = ServerParam::i().defaultKickableArea() * 2.2;
const double DIRECT_PASS_MAX_DIST =
    inertia_final_distance( ServerParam::i().ballSpeedMax(),
                            ServerParam::i().ballDecay() )
    * 0.8;
const double DIRECT_PASS_MAX_RECEIVE_BALL_SPEED =
    ServerParam::i().ballSpeedMax()
    * std::pow( ServerParam::i().ballDecay(), DIRECT_PASS_MIN_RECEIVE_STEP );

const int LEADING_PASS_MIN_RECEIVE_STEP = 4;
const double LEADING_PASS_MIN_DIST = 3.0;
const double LEADING_PASS_MAX_DIST =
    inertia_final_distance( ServerParam::i().ballSpeedMax(),
                            ServerParam::i().ballDecay() )
    * 0.8;
const double LEADING_PASS_MAX_RECEIVE_BALL_SPEED =
    ServerParam::i().ballSpeedMax()
    * std::pow( ServerParam::i().ballDecay(), LEADING_PASS_MIN_RECEIVE_STEP );


/*-------------------------------------------------------------------*/
/*!

 */
bool
check_receive_point( const WorldModel & wm,
                     const Vector2D & first_ball_pos,
                     const Vector2D & receive_point,
                     const int index )
{
    const ServerParam & SP = ServerParam::i();

    if ( wm.self().pos().x > wm.offsideLineX()
         && receive_point.x > wm.offsideLineX() )
    {
        dlog.addText( Logger::POSITIONING,
                      "%d: over offside line", index );
        return false;
    }

    if ( receive_point.x > SP.pitchHalfLength() - 1.5
         || receive_point.x < - SP.pitchHalfLength() + 5.0
         || receive_point.absY() > SP.pitchHalfWidth() - 1.5 )
    {
        dlog.addText( Logger::POSITIONING,
                      "%d: out of bounds", index );
        return false;
    }

    if ( wm.gameMode().type() == GameMode::GoalKick_
         && receive_point.x < SP.ourPenaltyAreaLineX() + 1.0
         && receive_point.absY() < SP.penaltyAreaHalfWidth() + 1.0 )
    {
        dlog.addText( Logger::POSITIONING,
                      "%d: in penalty area for goal kick mode",
                      index );
        return false;
    }

    if ( receive_point.x < first_ball_pos.x + 1.0
         && receive_point.dist2( SP.ourTeamGoalPos() ) < std::pow( 18.0, 2 ) )
    {
        dlog.addText( Logger::POSITIONING,
                      "%d: dangerous area", index );
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
exist_other_receiver( const WorldModel & wm,
                      const PlayerObject * passer,
                      const Vector2D & base_pos,
                      const Vector2D & receive_point )
{
    const double base_dist2 = base_pos.dist2( receive_point );

    for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(), end = wm.teammates().end();
          t != end;
          ++t )
    {
        if ( (*t)->goalie() ) continue;
        if ( (*t)->isGhost() ) continue;
        if ( (*t)->posCount() > 10 ) continue;
        if ( (*t)->unum() == passer->unum() ) continue;

        if ( (*t)->pos().dist2( receive_point ) < base_dist2 )
        {
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
exist_other_receiver_through_pass( const WorldModel & wm,
                                   const PlayerObject * passer,
                                   const Vector2D & base_pos,
                                   const Vector2D & receive_point )
{
    const double base_dist2 = std::pow( std::max( 0.1, base_pos.dist( receive_point ) - 2.0 ), 2 );

    for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(), end = wm.teammates().end();
          t != end;
          ++t )
    {
        if ( (*t)->goalie() ) continue;
        if ( (*t)->isGhost() ) continue;
        if ( (*t)->posCount() > 10 ) continue;
        if ( (*t)->unum() == passer->unum() ) continue;

        if ( (*t)->pos().dist2( receive_point ) < base_dist2 )
        {
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
get_self_move_step( const WorldModel & wm,
                    const Vector2D & self_pos,
                    const double self_speed,
                    const Vector2D & receive_point )
{
    const PlayerType & ptype = wm.self().playerType();
    const double self_move_dist = self_pos.dist( receive_point ) - ptype.kickableArea();
    const int self_turn_step = FieldAnalyzer::predict_player_turn_cycle( &ptype,
                                                                         wm.self().body(),
                                                                         self_speed,
                                                                         self_move_dist,
                                                                         ( receive_point - self_pos ).th(),
                                                                         0.5,
                                                                         false );
    const int self_dash_step = ptype.cyclesToReachDistance( self_move_dist );

    return self_turn_step + self_dash_step;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
calc_ball_move_step( const int index,
                     const double ball_move_dist,
                     const double max_ball_speed,
                     const int min_receive_step,
                     const double min_receive_ball_speed,
                     const double max_receive_ball_speed,
                     double * result_first_ball_speed,
                     double * result_receive_ball_speed )
{
    const ServerParam & SP = ServerParam::i();
#ifndef DEBUG_BALL_MOVE_STEP
    (void)index;
#endif

    int ball_move_step = SP.ballMoveStep( max_ball_speed, ball_move_dist );
    if ( ball_move_step <= 0 )
    {
#ifdef DEBUG_BALL_MOVE_STEP
        dlog.addText( Logger::POSITIONING,
                      "%d: xx never reach: ball_move=%.3f max_speed=%.3f",
                      index, ball_move_dist, max_ball_speed );
#endif
        return 0;
    }

    ball_move_step = std::max( ball_move_step, min_receive_step );

    double first_ball_speed = SP.firstBallSpeed( ball_move_dist, ball_move_step );
    double receive_ball_speed = first_ball_speed * std::pow( SP.ballDecay(), ball_move_step );

#ifdef DEBUG_PRINT_BALL_MOVE_STEP
    dlog.addText( Logger::POSITIONING,
                  "%d: ball_step=%d first_speed=%.3f receive_speed=%.3f",
                  index, ball_move_step, first_ball_speed, receive_ball_speed );
#endif

    if ( receive_ball_speed < min_receive_ball_speed )
    {
#ifdef DEBUG_BALL_MOVE_STEP
        dlog.addText( Logger::POSITIONING,
                      "%d: xx out of range: receive_ball_speed=%.3f < min=%.3f",
                      index, receive_ball_speed, min_receive_ball_speed );
#endif
        return 0;
    }

    while ( receive_ball_speed > max_receive_ball_speed )
    {
        if ( ball_move_step > 30 ) break; // magic number

        ball_move_step += 1;
        first_ball_speed = SP.firstBallSpeed( ball_move_dist, ball_move_step );
        receive_ball_speed = first_ball_speed * std::pow( SP.ballDecay(), ball_move_step );
    }

#ifdef DEBUG_BALL_MOVE_STEP
    dlog.addText( Logger::POSITIONING,
                  "%d: adjusted ball_move_step=%d first_speed=%.3f receive_speed=%.3f",
                  index, ball_move_step, first_ball_speed, receive_ball_speed );
#endif

    if ( receive_ball_speed < min_receive_ball_speed
         || receive_ball_speed > max_receive_ball_speed )
    {
#ifdef DEBUG_BALL_MOVE_STEP
        dlog.addText( Logger::POSITIONING,
                      "%d: xx out of range: receive_ball_speed",
                      index );
#endif
        return 0;
    }

    *result_first_ball_speed = first_ball_speed;
    *result_receive_ball_speed = receive_ball_speed;

    return ball_move_step;
}


/*-------------------------------------------------------------------*/
/*!

 */
void
debug_print_result( const char * type,
                    const int index,
                    const Vector2D & receive_point,
                    const int safety_level )
{
    (void)type;
    // dlog.addText( Logger::POSITIONING,
    //               "%d:(%s) (%.2f %.2f) safety_level = %d ",
    //               index, type, receive_point.x, receive_point.y, safety_level );

    char buf[8]; snprintf( buf, 8, "%d", index );
    dlog.addMessage( Logger::POSITIONING,
                     receive_point, buf, "0F0" );

    if ( safety_level == CooperativeAction::Safe )
    {
        dlog.addRect( Logger::POSITIONING,
                      receive_point.x - 0.05, receive_point.y - 0.05, 0.1, 0.1, "#0F0" );
    }
    else if ( safety_level == CooperativeAction::MaybeDangerous )
    {
        dlog.addRect( Logger::POSITIONING,
                      receive_point.x - 0.05, receive_point.y - 0.05, 0.1, 0.1, "#FF0" );
    }
    else if ( safety_level == CooperativeAction::Dangerous )
    {
        dlog.addRect( Logger::POSITIONING,
                      receive_point.x - 0.05, receive_point.y - 0.05, 0.1, 0.1, "#F00" );
    }
    else
    {
        dlog.addRect( Logger::POSITIONING,
                      receive_point.x - 0.05, receive_point.y - 0.05, 0.1, 0.1, "#00F" );
    }
}

}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorReceiveMove::GeneratorReceiveMove()
    : M_update_time( -1, 0 ),
      M_total_count( 0 ),
      M_passer( static_cast< const PlayerObject * >( 0 ) ),
      M_last_passer_unum( Unum_Unknown ),
      M_first_ball_pos( Vector2D::INVALIDATED )
{
    M_receive_points_direct_pass.reserve( 9*15 );
    M_receive_points_around_home.reserve( 9*15 );
    M_receive_points_leading_pass.reserve( 128 );
    M_receive_points_through_pass.reserve( 1024 );
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorReceiveMove &
GeneratorReceiveMove::instance()
{
    static GeneratorReceiveMove s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorReceiveMove::generate( const WorldModel & wm )
{
    if ( M_update_time == wm.time() )
    {
        return;
    }
    M_update_time = wm.time();

    clear();

    if ( wm.time().stopped() > 0
         || wm.gameMode().isPenaltyKickMode() )
    {
        return;
    }

    if ( wm.self().isFrozen() )
    {
        dlog.addText( Logger::POSITIONING,
                      __FILE__":(generate) cannot move." );
        return;
    }

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    updatePasser( wm );

    if ( ! M_passer )
    {
        dlog.addText( Logger::POSITIONING,
                      __FILE__":(generate) passer not found." );
        return;
    }

    if ( ! M_first_ball_pos.isValid() )
    {
        dlog.addText( Logger::POSITIONING,
                      __FILE__":(generate) illegal ball pos." );
        return;
    }

    generateImpl( wm );

    M_last_passer_unum = M_passer->unum();

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::POSITIONING,
                  __FILE__"(generate) PROFILE passer=%d d=%zd l=%zd t=%zd h=%zd elapsed %f [ms]",
                  M_passer->unum(),
                  M_receive_points_direct_pass.size(),
                  M_receive_points_leading_pass.size(),
                  M_receive_points_through_pass.size(),
                  M_receive_points_around_home.size(),
                  timer.elapsedReal() );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorReceiveMove::clear()
{
    M_total_count = 0;
    M_passer = static_cast< const PlayerObject * >( 0 );
    M_first_ball_pos.invalidate();

    M_receive_points_direct_pass.clear();
    M_receive_points_around_home.clear();
    M_receive_points_leading_pass.clear();
    M_receive_points_through_pass.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorReceiveMove::updatePasser( const WorldModel & wm )
{
    if ( wm.self().isKickable() )
    {
#ifdef DEBUG_UPDATE_PASSER
        dlog.addText( Logger::POSITIONING,
                      __FILE__":(updatePasser) self kickable." );
#endif
        return;
    }

    const int self_step = wm.interceptTable()->selfReachStep();
    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    const int our_step = std::min( self_step, teammate_step );

    if ( opponent_step < our_step - 2 )
    {
        dlog.addText( Logger::POSITIONING,
                      __FILE__":(updatePasser) opponent ball." );
        return;
    }

    if ( self_step <= teammate_step )
    {
        dlog.addText( Logger::POSITIONING,
                      __FILE__":(updatePasser) my ball." );
        return;
    }

    M_passer = wm.interceptTable()->fastestTeammate();

    if ( ! M_passer )
    {
        dlog.addText( Logger::POSITIONING,
                      __FILE__":(updatePasser) no passer." );
        return;
    }

    M_first_ball_pos = FieldAnalyzer::get_field_bound_predict_ball_pos( wm,
                                                                        teammate_step,
                                                                        0.5 );

#ifdef DEBUG_UPDATE_PASSER
    dlog.addText( Logger::POSITIONING,
                  __FILE__":(updatePasser) passer=%d(%.2f %.2f) reach_step=%d ball_pos=(%.2f %.2f)",
                  M_passer->unum(),
                  M_passer->pos().x, M_passer->pos().y,
                  M_passer->ballReachStep(),
                  M_first_ball_pos.x, M_first_ball_pos.y );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorReceiveMove::generateImpl( const WorldModel & wm )
{
    dlog.addCircle( Logger::POSITIONING,
                    M_first_ball_pos, 0.35, "#FFF" );
    dlog.addMessage( Logger::POSITIONING,
                     M_first_ball_pos.x, M_first_ball_pos.y - 0.35, "FirstBallPos" );

    // createDirectPass( wm );

    if ( ! wm.self().staminaModel().capacityIsEmpty() )
    {
        createAroundHomePosition( wm );
        //createLeadingPass( wm );
        createThroughPass( wm );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorReceiveMove::createDirectPass( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();

    const Vector2D receive_point = wm.self().pos();

    ++M_total_count;
#ifdef DEBUG_LEADING_PASS
    dlog.addText( Logger::POSITIONING,
                  "%d:(Direct) first_ball_pos (%.2f %.2f) receive_point (%.2f %.2f)",
                  M_total_count,
                  M_first_ball_pos.x, M_first_ball_pos.y, receive_point.x, receive_point.y );
#endif
    if ( ! check_receive_point( wm, M_first_ball_pos, receive_point, M_total_count ) )
    {
#ifdef DEBUG_LEADING_PASS
        dlog.addText( Logger::POSITIONING,
                      "%d:(Direct) xx: illegal receive point", M_total_count );
#endif
        return;
    }

    const double ball_move_dist = M_first_ball_pos.dist( receive_point );
#ifdef DEBUG_LEADING_PASS
    dlog.addText( Logger::POSITIONING,
                  "%d:(Direct) ball move dist %.3f",
                  M_total_count, ball_move_dist );
#endif
    if ( ball_move_dist < DIRECT_PASS_MIN_DIST
         || DIRECT_PASS_MAX_DIST < ball_move_dist )
    {
#ifdef DEBUG_LEADING_PASS
        dlog.addText( Logger::POSITIONING,
                      "%d:(Direct) xx: out of range: ball_move_dist",
                      M_total_count );
#endif
        return;
    }


    const PlayerType & ptype = wm.self().playerType();

    //
    // decide first ball speed
    //

    const double max_ball_speed = ( wm.gameMode().type() == GameMode::PlayOn
                                    ? SP.ballSpeedMax()
                                    : SP.kickPowerRate() * SP.maxPower() * 0.98 );
    const double max_receive_ball_speed
        = std::min( DIRECT_PASS_MAX_RECEIVE_BALL_SPEED,
                    ptype.kickableArea() + ( SP.maxDashPower()
                                             * ptype.dashPowerRate()
                                             * ptype.effortMax() ) * 1.8 );
    const double min_receive_ball_speed = ptype.realSpeedMax();

    double first_ball_speed = 0.0;
    double receive_ball_speed = 0.0;
    int ball_move_step = calc_ball_move_step( M_total_count,
                                              ball_move_dist,
                                              max_ball_speed,
                                              DIRECT_PASS_MIN_RECEIVE_STEP,
                                              min_receive_ball_speed,
                                              max_receive_ball_speed,
                                              &first_ball_speed,
                                              &receive_ball_speed );
    if ( ball_move_step <= 0 )
    {
#ifdef DEBUG_LEADING_PASS
        dlog.addText( Logger::POSITIONING,
                      "%d:(Direct) xx illegal ball move step", M_total_count );
#endif
        return;
    }

#ifdef DEBUG_LEADING_PASS
    dlog.addText( Logger::POSITIONING,
                  "%d:(Direct) check safety level",
                  M_total_count );
#endif
    CooperativeAction::SafetyLevel level = getSafetyLevel( wm, receive_point,
                                                           first_ball_speed, ball_move_step );
#ifdef DEBUG_LEADING_PASS
    dlog.addText( Logger::POSITIONING,
                  "%d:(Direct) safety_level = %d ",
                  M_total_count, level );
#endif
    if ( level != CooperativeAction::Failure )
    {
        // no move
        M_receive_points_direct_pass.push_back( ReceivePoint( receive_point, 0, level ) );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorReceiveMove::createAroundHomePosition( const WorldModel & wm )
{
    const int X_DIVS = 4;
    const double X_STEP = 2.0;
    const int Y_DIVS = 7;
    const double Y_STEP = 2.0;
    const double MAX_DIST2_FROM_HOME = std::pow( 15.0, 2 );

    const ServerParam & SP = ServerParam::i();
    const double max_ball_speed = ( wm.gameMode().type() == GameMode::PlayOn
                                    ? SP.ballSpeedMax()
                                    : SP.kickPowerRate() * SP.maxPower() * 0.98 );

    const PlayerType & ptype = wm.self().playerType();
    const double max_receive_ball_speed
        = std::min( DIRECT_PASS_MAX_RECEIVE_BALL_SPEED,
                    ptype.kickableArea() + ( SP.maxDashPower()
                                             * ptype.dashPowerRate()
                                             * ptype.effortMax() ) * 1.8 );
    const double min_receive_ball_speed = ptype.realSpeedMax();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D self_inertia_pos = wm.self().inertiaFinalPoint();
    const double self_speed = wm.self().vel().r();

    for ( int ix = -X_DIVS; ix <= X_DIVS; ++ix )
    {
        for ( int iy = -Y_DIVS; iy <= Y_DIVS; ++iy )
        {
            ++M_total_count;

            const Vector2D receive_point( home_pos.x + X_STEP*ix,
                                          home_pos.y + Y_STEP*iy );
            const double ball_move_dist = M_first_ball_pos.dist( receive_point );
#ifdef DEBUG_AROUND_HOME
            dlog.addText( Logger::POSITIONING,
                          "%d:(Home) ==========", M_total_count );
            dlog.addText( Logger::POSITIONING,
                          "%d:(Home) from=(%.2f %.2f) to=(%.2f %.2f) ball_move_dist=%.3f",
                          M_total_count,
                          M_first_ball_pos.x, M_first_ball_pos.y,
                          receive_point.x, receive_point.y,
                          ball_move_dist );
#endif

            if ( ball_move_dist < DIRECT_PASS_MIN_DIST
                 || ball_move_dist > DIRECT_PASS_MAX_DIST )
            {
#ifdef DEBUG_AROUND_HOME
                dlog.addText( Logger::POSITIONING,
                              "%d:(Home) xx: out of range: ball_move_dist", M_total_count );
#endif
                continue;
            }

            if ( receive_point.dist2( home_pos ) > MAX_DIST2_FROM_HOME )
            {
#ifdef DEBUG_AROUND_HOME
                dlog.addText( Logger::POSITIONING,
                              "%d:(Home) xx: out of range: far from home pos", M_total_count );
#endif
                continue;
            }

            if ( ! check_receive_point( wm, M_first_ball_pos, receive_point, M_total_count ) )
            {
#ifdef DEBUG_AROUND_HOME
                dlog.addText( Logger::POSITIONING,
                              "%d:(Home) xx: out of range: receive_point", M_total_count );
#endif
                continue;
            }

            if ( exist_other_receiver( wm, M_passer, home_pos, receive_point ) )
            {
#ifdef DEBUG_AROUND_HOME
                dlog.addText( Logger::POSITIONING,
                              "%d:(Home) xx: exist other receiver", M_total_count );
#endif
                continue;
            }

            const int self_move_step = get_self_move_step( wm, self_inertia_pos, self_speed, receive_point );

            double first_ball_speed = 0.0;
            double receive_ball_speed = 0.0;
            const int ball_move_step = calc_ball_move_step( M_total_count,
                                                            ball_move_dist,
                                                            max_ball_speed,
                                                            DIRECT_PASS_MIN_RECEIVE_STEP,
                                                            min_receive_ball_speed,
                                                            max_receive_ball_speed,
                                                            &first_ball_speed,
                                                            &receive_ball_speed );
            if ( ball_move_step <= 0 )
            {
#ifdef DEBUG_AROUND_HOME
                dlog.addText( Logger::POSITIONING,
                              "%d:(Home) xx illegal ball move step", M_total_count );
#endif
                continue;
            }

#ifdef DEBUG_AROUND_HOME
            dlog.addText( Logger::POSITIONING,
                          "%d:(Home) check safety level",
                          M_total_count );
#endif
            CooperativeAction::SafetyLevel level = getSafetyLevel( wm, receive_point,
                                                                   first_ball_speed, ball_move_step );
#ifdef DEBUG_PRINT_RESULTS
            debug_print_result( "Home", M_total_count, receive_point, level );
#endif
            if ( level != CooperativeAction::Failure )
            {
                M_receive_points_around_home.push_back( ReceivePoint( receive_point, self_move_step, level ) );
            }
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorReceiveMove::createLeadingPass( const WorldModel & wm )
{
    const int ANGLE_DIVS = 16;
    const double ANGLE_STEP = 360.0 / ANGLE_DIVS;
    const double DIST_DIVS = 5;
    const double DIST_STEP = 2.0;

    const ServerParam & SP = ServerParam::i();
    const double max_ball_speed = ( wm.gameMode().type() == GameMode::PlayOn
                                    ? SP.ballSpeedMax()
                                    : SP.kickPowerRate() * SP.maxPower() * 0.98 );

    const PlayerType & ptype = wm.self().playerType();
    const double self_speed = wm.self().vel().r();
    const Vector2D self_inertia_pos = wm.self().inertiaFinalPoint();

    const double max_receive_ball_speed
        = std::min( LEADING_PASS_MAX_RECEIVE_BALL_SPEED,
                    ptype.kickableArea() + ( SP.maxDashPower()
                                             * ptype.dashPowerRate()
                                             * ptype.effortMax() ) * 1.8 );
    const double min_receive_ball_speed = ptype.realSpeedMax();

    for ( int a = 0; a < ANGLE_DIVS; ++a )
    {
        const AngleDeg angle = wm.self().body() + ANGLE_STEP*a;
        const Vector2D unit_vec = Vector2D::from_polar( 1.0, angle );

        for ( int d = 1; d <= DIST_DIVS; ++d )
        {
            ++M_total_count;

            const double dash_dist = DIST_STEP * d;
            const Vector2D receive_point = self_inertia_pos + ( unit_vec * dash_dist );

            const double ball_move_dist = M_first_ball_pos.dist( receive_point );
#ifdef DEBUG_LEADING_PASS
            dlog.addText( Logger::POSITIONING,
                          "%d:(Lead) ==========", M_total_count );
            dlog.addText( Logger::POSITIONING,
                          "%d:(Lead) from=(%.2f %.2f) to=(%.2f %.2f) angle=%.0f dash_dist=%.3f ball_move=%.3f",
                          M_total_count,
                          M_first_ball_pos.x, M_first_ball_pos.y,
                          receive_point.x, receive_point.y,
                          angle.degree(), dash_dist, ball_move_dist );
#endif

            if ( ball_move_dist < LEADING_PASS_MIN_DIST
                 || ball_move_dist > LEADING_PASS_MAX_DIST )
            {
#ifdef DEBUG_LEADING_PASS
                dlog.addText( Logger::POSITIONING,
                              "%d:(Lead) xx: out of range: ball_move_dist", M_total_count );
#endif
                continue;
            }

            if ( ! check_receive_point( wm, M_first_ball_pos, receive_point, M_total_count ) )
            {
#ifdef DEBUG_LEADING_PASS
                dlog.addText( Logger::POSITIONING,
                              "%d:(Lead) xx: out of range: receive_point", M_total_count );
#endif
                continue;
            }

            if ( exist_other_receiver( wm, M_passer, self_inertia_pos, receive_point ) )
            {
#ifdef DEBUG_LEADING_PASS
                dlog.addText( Logger::POSITIONING,
                              "%d:(Lead) xx: exist other receiver", M_total_count );
#endif
                continue;
            }

            const int self_move_step = get_self_move_step( wm, self_inertia_pos, self_speed, receive_point );

            double first_ball_speed = 0.0;
            double receive_ball_speed = 0.0;
            const int ball_move_step = calc_ball_move_step( M_total_count,
                                                            ball_move_dist,
                                                            max_ball_speed,
                                                            LEADING_PASS_MIN_RECEIVE_STEP,
                                                            min_receive_ball_speed,
                                                            max_receive_ball_speed,
                                                            &first_ball_speed,
                                                            &receive_ball_speed );
            if ( ball_move_step <= 0 )
            {
#ifdef DEBUG_LEADING_PASS
                dlog.addText( Logger::POSITIONING,
                              "%d:(Lead) xx illegal ball move step", M_total_count );
#endif
                continue;
            }

#ifdef DEBUG_LEADING_PASS
            dlog.addText( Logger::POSITIONING,
                          "%d:(Lead) check safety level",
                          M_total_count );
#endif
            CooperativeAction::SafetyLevel level = getSafetyLevel( wm, receive_point,
                                                                   first_ball_speed, ball_move_step );
#ifdef DEBUG_PRINT_RESULTS
            debug_print_result( "Lead", M_total_count, receive_point, level );
#endif
            if ( level != CooperativeAction::Failure )
            {
                M_receive_points_leading_pass.push_back( ReceivePoint( receive_point, self_move_step, level ) );
            }
        }
    }

}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorReceiveMove::createThroughPass( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();

    const int min_receive_step = 6;
    const double max_pass_dist
        = 0.9
        * inertia_final_distance( SP.ballSpeedMax(), SP.ballDecay() );
    const double min_receive_ball_speed = 0.001;
    const double max_receive_ball_speed
        = SP.ballSpeedMax()
        * std::pow( SP.ballDecay(), min_receive_step );
    const double max_first_ball_speed = ( wm.gameMode().type() == GameMode::PlayOn
                                          ? SP.ballSpeedMax()
                                          : SP.kickPowerRate() * SP.maxPower() * 0.98 );

    const double max_x = SP.pitchHalfLength() - 3.0;
    const double max_y = SP.pitchHalfWidth() - 3.0;

    const PlayerType & ptype = wm.self().playerType();
    const Vector2D self_inertia_pos = wm.self().inertiaFinalPoint();
    const double self_speed = wm.self().vel().r();

    const int teammate_step = wm.interceptTable()->teammateReachStep();

    //
    // angle loop
    //
    for ( double a = -60.0; a < 61.0; a += 5.0 )
    {
        const Vector2D unit_vec = Vector2D::from_polar( 1.0, a );

        //
        // distance loop
        //
        double dist_step = 2.5;
        for ( double d = 10.0; d < max_pass_dist; d += dist_step, dist_step += 0.25 )
        {
            ++M_total_count;

            const Vector2D receive_point = M_first_ball_pos + ( unit_vec * d );
            if ( receive_point.x > max_x
                 || receive_point.absY() > max_y )
            {
                continue;
            }

            if ( receive_point.x < wm.offsideLineX() + 3.0 )
            {
                continue;
            }

            if ( exist_other_receiver_through_pass( wm, M_passer, wm.self().pos(), receive_point ) )
            {
                continue;
            }

            const double ball_move_dist = d;

            const double self_move_dist = self_inertia_pos.dist( receive_point ) - ptype.kickableArea();
            const int self_turn_step = FieldAnalyzer::predict_player_turn_cycle( &ptype,
                                                                                 wm.self().body(),
                                                                                 self_speed,
                                                                                 self_move_dist,
                                                                                 ( receive_point - self_inertia_pos ).th(),
                                                                                 0.5,
                                                                                 false );
            const int self_dash_step = ptype.cyclesToReachDistance( self_move_dist );
            const int self_move_step = self_turn_step + self_dash_step;

            //const int min_step = min_receive_step;
            const int min_step = std::max( min_receive_step, self_move_step - teammate_step - 3 );

            double first_ball_speed = 0.0;
            double receive_ball_speed = 0.0;
            const int ball_move_step = calc_ball_move_step( M_total_count,
                                                            ball_move_dist,
                                                            max_first_ball_speed,
                                                            min_step,
                                                            min_receive_ball_speed,
                                                            max_receive_ball_speed,
                                                            &first_ball_speed,
                                                            &receive_ball_speed );
            if ( ball_move_step <= 0 )
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::POSITIONING,
                              "%d:(Through) xx illegal ball move step", M_total_count );
#endif
                continue;
            }

#ifdef DEBUG_THROUGH_PASS
            dlog.addText( Logger::POSITIONING,
                          "%d:(Through) self_move=%.3f self_step=%d min_step=%d ball_step=%d first_speed=%.3f",
                          M_total_count, self_move_dist, self_move_step, first_ball_speed );
            dlog.addText( Logger::POSITIONING,
                          "%d:(Through) check safety level", M_total_count );
#endif
            CooperativeAction::SafetyLevel level = getSafetyLevel( wm, receive_point,
                                                                   first_ball_speed, ball_move_step );
#ifdef DEBUG_PRINT_RESULTS
            debug_print_result( "Through", M_total_count, receive_point, level );
#endif
            if ( level != CooperativeAction::Failure )
            {
                M_receive_points_through_pass.push_back( ReceivePoint( receive_point,
                                                                       self_move_step,
                                                                       level ) );
            }
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::SafetyLevel
GeneratorReceiveMove::getSafetyLevel( const WorldModel & wm,
                                      const Vector2D & receive_point,
                                      const double first_ball_speed,
                                      const int ball_move_step )
{
    const ServerParam & SP = ServerParam::i();
    const Vector2D first_ball_vel
        = ( receive_point - M_first_ball_pos ).setLengthVector( first_ball_speed );

    Vector2D ball_pos = M_first_ball_pos;
    Vector2D ball_vel = first_ball_vel;


    CooperativeAction::SafetyLevel worst_level = CooperativeAction::Safe;
    for ( int step = 1; step <= ball_move_step; ++step )
    {
        ball_pos += ball_vel;
        ball_vel *= SP.ballDecay();

        for ( PlayerObject::Cont::const_iterator o = wm.opponents().begin(),
                  end = wm.opponents().end();
              o != end;
              ++o )
        {
            const PlayerType * ptype = (*o)->playerTypePtr();
            const Vector2D pos = (*o)->inertiaPoint( step );
            const double opponent_move_dist = pos.dist( ball_pos ) - ServerParam::i().tackleDist();

            // penalty of observation & turn
            const int move_step = 2 + ptype->cyclesToReachDistance( opponent_move_dist );

            CooperativeAction::SafetyLevel level = CooperativeAction::Safe;
            if ( move_step <= step - 1 )
            {
                level = CooperativeAction::Failure;
            }
            else if ( move_step <= step )
            {
                level = CooperativeAction::Dangerous;
            }
            else if ( move_step <= step + 1  )
            {
                level = CooperativeAction::MaybeDangerous;
            }
            else if ( move_step <= step + 2 )
            {
                level = CooperativeAction::Safe;
            }

            if ( level < worst_level )
            {
                worst_level = level;
#ifdef DEBUG_SAFETY_LEVEL
                dlog.addText( Logger::POSITIONING,
                              "__ update safety_level: ball_step=%d opponent[%d] step=%d safe=%d",
                              step, (*o)->unum(), move_step, level );
#endif
                if ( worst_level == CooperativeAction::Failure )
                {
                    return worst_level;
                }
            }
        }
    }

    return worst_level;
}
