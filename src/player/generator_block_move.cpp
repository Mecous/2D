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

#include "generator_block_move.h"

#include "strategy.h"
#include "mark_analyzer.h"
#include "defense_system.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/color/thermo_color_provider.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/geom/matrix_2d.h>
#include <rcsc/timer.h>

#define DEBUG_PROFILE
#define DEBUG_PRINT
#define DEBUG_PRINT_SELF
#define DEBUG_PRINT_EVALUATE
#define DEBUG_PAINT_VALUE

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorBlockMove::GeneratorBlockMove()
    : M_update_time( -1, 0 ),
      M_total_count( 0 ),
      M_best_point( -1, Vector2D::INVALIDATED ),
      M_previous_time( -1, 0 ),
      M_previous_best_point( -1, Vector2D::INVALIDATED )
{
    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorBlockMove &
GeneratorBlockMove::instance()
{
    static GeneratorBlockMove s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorBlockMove::clear()
{
    M_total_count = 0;
    M_best_point = TargetPoint();
    M_target_points.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorBlockMove::generate( const WorldModel & wm )
{
    if ( M_update_time == wm.time() )
    {
        return;
    }
    M_update_time = wm.time();

    if ( wm.gameMode().type() != GameMode::PlayOn )
    {
        return;
    }

    if ( wm.self().isKickable()
         || wm.kickableTeammate() )
    {
        // our ball?
        return;
    }

    // check ball owner

    const int self_step = wm.interceptTable()->selfReachStep();
    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( self_step <= opponent_step
         || teammate_step <= opponent_step - 1 )
    {
        // our ball
        return;
    }

    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    if ( ! opponent )
    {
        // opponent not found
        return;
    }

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    generateImpl( wm );
    evaluate( wm );

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::TEAM,
                  __FILE__": (generate) PROFILE size=%d elapsed %.3f [ms]",
                  (int)M_target_points.size(), timer.elapsedReal() );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorBlockMove::generateImpl( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();

    const double pitch_x = SP.pitchHalfLength();
    const double pitch_y = SP.pitchHalfWidth();

    const Vector2D opponent_pos = DefenseSystem::get_block_opponent_trap_point( wm );
    const Vector2D center_pos = DefenseSystem::get_block_center_point( wm );
    const Segment2D block_segment( opponent_pos, center_pos );
    const Vector2D self_inertia = wm.self().inertiaFinalPoint();
    const bool on_segment = ( block_segment.contains( self_inertia )
                              && block_segment.dist( self_inertia ) < 0.5 );
    const double max_length = std::min( 30.0, opponent_pos.dist( center_pos ) + 1.0 );

#ifdef DEBUG_PRINT
    dlog.addText( Logger::BLOCK,
                  __FILE__":(generateImpl) opponent=(%.2f %.2f) center=(%.1f %.1f)"
                  " on_segment=%d",
                  opponent_pos.x, opponent_pos.y, center_pos.x, center_pos.y,
                  (int)on_segment );
#endif

    const Vector2D unit_vec = ( center_pos - opponent_pos ).setLengthVector( 1.0 );

    double length_step = 0.3;
    for ( double length = 0.3;
          length < max_length;
          length += length_step, length_step += 0.15 )
    {
        ++M_total_count;

        const Vector2D target_point = opponent_pos + ( unit_vec * length );
#ifdef DEBUG_PRINT
        dlog.addText( Logger::BLOCK,
                      "%d: len=%.2f step=%.2f point=(%.1f %.1f)",
                      M_total_count, length, length_step, target_point.x, target_point.y );
#endif

        if ( target_point.absX() > pitch_x
             || target_point.absY() > pitch_y )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::BLOCK,
                          "%d: xx out of pitch", M_total_count );
#endif
            break;
        }

        int turn_step = 0;
        int self_step = predictSelfReachStep( wm, target_point, &turn_step );
#ifdef DEBUG_PRINT_SELF
        dlog.addText( Logger::BLOCK,
                      "%d: n_turn=%d n_dash=%d stamina=%lf",
                      M_total_count, turn_step, self_step - turn_step );
#endif
    }

}

/*-------------------------------------------------------------------*/
/*!

 */
int
GeneratorBlockMove::predictSelfReachStep( const WorldModel & wm,
                                          const Vector2D & target_point,
                                          int * result_turn_step )
{
    // const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();
    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -wm.self().body() );
    const Vector2D self_first_vel = rotate_matrix.transform( wm.self().vel() );
    // const double self_first_speed = wm.self().vel().r();
#ifdef DEBUG_PRINT_SELF
    dlog.addText( Logger::BLOCK,
                  "%d: (predictSelfReachStep)", M_total_count );
#endif

    for ( int step = 1; step <= 20; ++step )
    {
        const Vector2D inertia_pos = wm.self().inertiaPoint( step );
        const Vector2D target_rel = rotate_matrix.transform( target_point - inertia_pos );
        double move_dist = target_rel.r();
        if ( move_dist > ptype.realSpeedMax() * step )
        {
            continue;
        }

        int n_dash = 0;

        if ( target_rel.x > -0.5
             && target_rel.absY() < 2.0 )
        {
            n_dash = predictOmniDashStep( wm, target_rel, self_first_vel, step );
            if ( n_dash <= step )
            {
                *result_turn_step = 0;
                return n_dash;
            }
        }

        // AngleDeg target_angle = ( target_point - inertia_pos ).th();



    }


    return -1;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
GeneratorBlockMove::predictOmniDashStep( const WorldModel & wm,
                                         const Vector2D & target_rel,
                                         const Vector2D & self_first_vel,
                                         const int max_step )
{
    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    const double dash_angle_step = std::max( 15.0, SP.dashAngleStep() );
    const double min_dash_angle = ( -180.0 < SP.minDashAngle() && SP.maxDashAngle() < 180.0
                                    ? SP.minDashAngle()
                                    : dash_angle_step * static_cast< int >( -180.0 / dash_angle_step ) );
    const double max_dash_angle = ( -180.0 < SP.minDashAngle() && SP.maxDashAngle() < 180.0
                                    ? SP.maxDashAngle() + dash_angle_step * 0.5
                                    : dash_angle_step * static_cast< int >( 180.0 / dash_angle_step ) - 1.0 );
    const double speed_factor = ( 1.0 - ptype.playerDecay() ) / ( 1.0 - std::pow( ptype.playerDecay(), 10 ) ); // magic number


    Vector2D self_pos( 0.0, 0.0 );
    Vector2D self_vel = self_first_vel;
    StaminaModel stamina_model = wm.self().staminaModel();

    int omni_step = 0;
    for ( int step = max_step; step > 0; --step )
    {
        if ( std::fabs( target_rel.y - self_pos.y ) < 0.001 )
        {
            break;
        }

        Vector2D required_vel = ( target_rel - self_pos ) * speed_factor;
        Vector2D required_accel = required_vel - self_vel;

        const double safe_dash_power = stamina_model.getSafetyDashPower( ptype, SP.maxDashPower() );

        double min_dist2 = 1000000.0;
        Vector2D best_self_pos = Vector2D::INVALIDATED;
        Vector2D best_self_vel;
        double best_dash_power = 0.0;
        for ( double dir = min_dash_angle;
              dir < max_dash_angle;
              dir += dash_angle_step )
        {
            const AngleDeg dash_angle = SP.discretizeDashAngle( dir );
            const double dash_rate = ptype.dashPowerRate() * stamina_model.effort() * SP.dashDirRate( dir );
            const Vector2D required_accel_rel = required_accel.rotate( -dash_angle );

            const double dash_power = bound( 0.0, required_accel_rel.x / dash_rate, safe_dash_power );
            Vector2D dash_accel = Vector2D::polar2vector( dash_rate * dash_power, dash_angle );

            Vector2D tmp_self_vel = self_vel + dash_accel;
            Vector2D tmp_self_pos = self_pos + tmp_self_vel;
            double d2 = tmp_self_pos.dist2( target_rel );
            if ( d2 < min_dist2 )
            {
                min_dist2 = d2;
                best_self_pos = tmp_self_pos;
                best_self_vel = tmp_self_vel;
                best_dash_power = dash_power;
            }
        }

        self_pos = best_self_pos;
        self_vel = best_self_vel;
        self_vel *= ptype.playerDecay();
        stamina_model.simulateDash( ptype, best_dash_power );

        ++omni_step;
    }

    if ( target_rel.x - self_pos.x < -0.1 )
    {
        // never reach
        return -1;
    }

    //
    // normal dash
    //
    int forward_step = 0;
    for ( int step = omni_step; step < max_step; ++step )
    {
        if ( std::fabs( self_pos.x - target_rel.x ) < 0.001 )
        {
            break;
        }

        Vector2D required_vel = target_rel - self_pos;
        Vector2D required_accel = required_vel - self_vel;

        double safe_dash_power = stamina_model.getSafetyDashPower( ptype, SP.maxDashPower() );
        double dash_rate = ptype.dashPowerRate() * stamina_model.effort();
        double dash_power = bound( 0.0, required_accel.x / dash_rate, safe_dash_power );

        self_vel.x += dash_power * ptype.dashPowerRate() * stamina_model.effort();
        self_pos += self_vel;
        self_vel *= ptype.playerDecay();
        stamina_model.simulateDash( ptype, dash_power );

        ++forward_step;
    }

    return omni_step + forward_step;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorBlockMove::evaluate( const WorldModel & wm )
{
    (void)wm;
}
