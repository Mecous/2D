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

#include "move_simulator.h"

#include <rcsc/player/world_model.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/logger.h>
#include <rcsc/geom/matrix_2d.h>
#include <rcsc/timer.h>
#include <rcsc/math_util.h>

#include <algorithm>
#include <cmath>

// #define DEBUG_PRINT_TURN_DASH
// #define DEBUG_PRINT_OMNI_DASH

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
void
MoveSimulator::Result::setValue( const int n_turn,
                                 const int n_dash,
                                 const double stamina,
                                 const double turn_moment,
                                 const double dash_power,
                                 const double dash_dir,
                                 const AngleDeg & body_angle )
{
    turn_step_ = n_turn;
    dash_step_ = n_dash;
    stamina_ = stamina;
    turn_moment_ = turn_moment;
    dash_power_ = dash_power;
    dash_dir_ = dash_dir;
    body_angle_ = body_angle;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
MoveSimulator::simulate_turn_step( const rcsc::WorldModel & wm,
                                   const rcsc::Vector2D & target_point,
                                   const double tolerance,
                                   const int move_step,
                                   const bool back_dash,
                                   AngleDeg * result_dash_angle )
{
    const Vector2D inertia_self_pos = wm.self().inertiaPoint( move_step );
    const Vector2D inertia_rel = target_point - inertia_self_pos;
    const double inertia_dist = inertia_rel.r();

    int n_turn = 0;

    if ( tolerance < inertia_dist )
    {
        const ServerParam & SP = ServerParam::i();
        const PlayerType & ptype = wm.self().playerType();

        AngleDeg dash_angle = wm.self().body();
        if ( back_dash ) dash_angle += 180.0;

        const AngleDeg target_angle = inertia_rel.th();
        const double turn_margin = std::max( 15.0, // Magic Number
                                             AngleDeg::asin_deg( tolerance / inertia_dist ) );

        double angle_diff = ( target_angle - dash_angle ).abs();
        double speed = wm.self().vel().r();
        while ( angle_diff > turn_margin )
        {
            angle_diff -= ptype.effectiveTurn( SP.maxMoment(), speed );
            speed *= ptype.playerDecay();
            ++n_turn;
        }

        if ( result_dash_angle )
        {
            if ( angle_diff <= 0.0 )
            {
                *result_dash_angle = target_angle;
            }
            else
            {
                AngleDeg rel_angle = dash_angle - target_angle;
                if ( rel_angle.degree() > 0.0 )
                {
                    *result_dash_angle = target_angle + angle_diff;
                }
                else
                {
                    *result_dash_angle = target_angle - angle_diff;
                }
            }
        }
    }

    return n_turn;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
MoveSimulator::simulate_self_turn_dash( const WorldModel & wm,
                                        const Vector2D & target_point,
                                        const double tolerance,
                                        const bool back_dash,
                                        const int max_step,
                                        Result * result )
{
    const int min_step = static_cast< int >( std::ceil( wm.self().pos().dist( target_point )
                                                        / wm.self().playerType().realSpeedMax() ) );

#ifdef DEBUG_PRINT_TURN_DASH
    dlog.addText( Logger::ACTION,
                  __FILE__":(simulate_self_turn_dash) pos=(%.2f %.2f) thr=%.3f, min_step=%d",
                  target_point.x, target_point.y, tolerance,
                  min_step );
#endif

    for ( int step = std::max( 1, min_step - 1 );
          step <= max_step;
          ++step )
    {
#ifdef DEBUG_PRINT_TURN_DASH
        dlog.addText( Logger::ACTION,
                      __FILE__":(simulate_self_turn_dash) step=%d: start [%s]",
                      step, ( back_dash ? "back_dash" : "forward_dash" ) );
#endif

        if ( self_can_reach_after_turn_dash( wm, target_point, tolerance, back_dash, step, result ) )
        {
#ifdef DEBUG_PRINT_TURN_DASH
            dlog.addText( Logger::ACTION,
                          __FILE__":(simulate_self_turn_dash) %d: found",
                          step );
#endif
            return step;
        }
    }

    return -1;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
MoveSimulator::self_can_reach_after_turn_dash( const WorldModel & wm,
                                               const Vector2D & target_point,
                                               const double tolerance,
                                               const bool back_dash,
                                               const int max_step,
                                               Result * result )
{
    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    if ( ptype.cyclesToReachDistance( wm.self().inertiaPoint( max_step ).dist( target_point ) - tolerance )
         > max_step )
    {
        return false;
    }

    AngleDeg dash_angle = ( back_dash
                            ? wm.self().body() + 180.0
                            : wm.self().body() );
    const int n_turn = simulate_turn_step( wm, target_point, tolerance, max_step, back_dash,
                                           &dash_angle );

    if ( n_turn >= max_step )
    {
        return false;
    }

    const AngleDeg body_angle = ( n_turn == 0
                                  ? wm.self().body()
                                  : back_dash
                                  ? dash_angle + 180.0
                                  : dash_angle );

    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -body_angle );

    {
        Vector2D self_inertia = wm.self().inertiaPoint( max_step );
        Vector2D target_rel_to_inertia = rotate_matrix.transform( target_point - self_inertia );

        if ( target_rel_to_inertia.r2() < std::pow( tolerance, 2 ) )
        {
            StaminaModel s = wm.self().staminaModel();
            s.simulateWaits( ptype, max_step );
            if ( result )
            {
                result->setValue( 0, 0, // no turn, no dash
                                  s.stamina(),
                                  0.0, 0.0, 0.0,
                                  wm.self().body() );
            }
#ifdef DEBUG_PRINT_TURN_DASH
            dlog.addText( Logger::ACTION,
                          "(turn_dash) %d: already there", max_step );
#endif
            return true;
        }

        if ( ( back_dash && target_rel_to_inertia.x > 0.0 )
             || ( ! back_dash && target_rel_to_inertia.x < 0.0 ) )
        {
            return false;
        }
    }

    Vector2D self_pos( 0.0, 0.0 );
    Vector2D self_vel = rotate_matrix.transform( wm.self().vel() );
    StaminaModel stamina_model = wm.self().staminaModel();

    for ( int i = 0; i < n_turn; ++i )
    {
        self_pos += self_vel;
        self_vel *= ptype.playerDecay();
        stamina_model.simulateWait( ptype );
    }

    const Vector2D target_rel = rotate_matrix.transform( target_point - wm.self().pos() );

    // if ( self_pos.dist2( target_rel ) < std::pow( tolerance, 2 ) )
    // {
    //     return true;
    // }

    const int max_dash_step = max_step - n_turn;

#ifdef DEBUG_PRINT_TURN_DASH
    dlog.addText( Logger::ACTION,
                  "(turn_dash) %d: body_angle=%.1f dash_angle=%.1f",
                  max_step, body_angle.degree(), dash_angle.degree() );
    dlog.addText( Logger::ACTION,
                  "(turn_dash) %d: max_dash_step=%d", max_step, max_dash_step );
#endif

    double first_dash_power = 0.0;
    for ( int i = 0; i < max_dash_step; ++i )
    {
        double required_vel_x = ( target_rel.x - self_pos.x )
            * ( 1.0 - ptype.playerDecay() )
            / ( 1.0 - std::pow( ptype.playerDecay(), max_dash_step - i ) );
        double required_accel_x = required_vel_x - self_vel.x;
        double dash_power = required_accel_x / ( ptype.dashPowerRate() * stamina_model.effort() );
        dash_power = bound( SP.minDashPower(), dash_power, SP.maxDashPower() );
        dash_power = stamina_model.getSafetyDashPower( ptype, dash_power, 1.0 );

        double accel_x = dash_power * ptype.dashPowerRate() * stamina_model.effort();

        self_vel.x += accel_x;
        self_pos += self_vel;
        self_vel *= ptype.playerDecay();
        stamina_model.simulateDash( ptype, dash_power );

#ifdef DEBUG_PRINT_TURN_DASH
        dlog.addText( Logger::ACTION,
                      "(turn_dash) %d: [%s] dash=%d req_vel_x=%.3f req_accel_x=%.3f power=%.3f accel_x=%.3f",
                      max_step, ( back_dash ? "back" : "forward" ),
                      i+1, required_vel_x, required_accel_x, dash_power, accel_x );
        dlog.addText( Logger::ACTION,
                      "(turn_dash) %d: dash=%d self_pos=(%.2f %.2f) self_vel=(%.2f %.2f) stamina=%.1f capacity=%.1f",
                      max_step, i+1, self_pos.x, self_pos.y, self_vel.x, self_vel.y,
                      stamina_model.stamina(), stamina_model.capacity() );
#endif

        if ( i == 0 )
        {
            first_dash_power = dash_power;
        }

        if ( std::fabs( required_accel_x ) < 1.0e-5
             || self_pos.absX() > target_rel.absX() - 1.0e-5
             || self_pos.r2() > target_rel.r2()
             || self_pos.dist2( target_rel ) < std::pow( tolerance, 2 ) )
        {
#ifdef DEBUG_PRINT_TURN_DASH
            dlog.addText( Logger::ACTION,
                          "OK (turn_dash) %d: turn=%d dash=%d stamina=%.1f",
                          max_step, n_turn, i + 1, stamina_model.stamina() );
            dlog.addText( Logger::ACTION,
                          "---> req_acc_x=%.3f",
                          required_accel_x );
            dlog.addText( Logger::ACTION,
                          "---> req_acc_x=%.3f self_pos.x=%.2f target_rel.x=%.2f",
                          self_pos.x, target_rel.x );
            dlog.addText( Logger::ACTION,
                          "---> self_move=(%.2f %2f) dist=%.2f target_rel=(%.2f %2f) r=%.2f",
                          self_pos.x, self_pos.y, self_pos.r(),
                          target_rel.x, target_rel.y, target_rel.r() );
#endif
            if ( result )
            {
                result->setValue( n_turn, i + 1,
                                  stamina_model.stamina(),
                                  ( body_angle - wm.self().body() ).degree(),
                                  first_dash_power, 0.0,
                                  body_angle );
            }
            return true;
        }
    }

    return false;
}

namespace {

struct State {
    double dash_power_;
    double dash_dir_;
    Vector2D result_pos_;

    State( const double dash_power,
           const double dash_dir,
           const Vector2D & result_pos )
        : dash_power_( dash_power ),
          dash_dir_( dash_dir ),
          result_pos_( result_pos )
      { }
};

}

/*-------------------------------------------------------------------*/
/*!

 */
int
MoveSimulator::simulate_self_omni_dash( const WorldModel & wm,
                                        const Vector2D & target_point,
                                        const double tolerance,
                                        const int max_step,
                                        Result * result )
{
#ifdef DEBUG_PRINT_OMNI_DASH
    dlog.addText( Logger::ACTION,
                  __FILE__":(simulate_self_omni_dash) pos=(%.2f %.2f) thr=%.3f, max_step=%d",
                  target_point.x, target_point.y, tolerance, max_step );
    std::vector< State > states;
    states.reserve( max_step );
#endif

    const ServerParam & SP = ServerParam::i();
    const double dash_angle_step = std::max( 15.0, SP.dashAngleStep() );
    const size_t dash_angle_divs = static_cast< size_t >( std::floor( 360.0 / dash_angle_step ) );

    const PlayerType & ptype = wm.self().playerType();
    const double max_side_speed = ( SP.maxDashPower()
                                    * ptype.dashPowerRate()
                                    * ptype.effortMax()
                                    * SP.dashDirRate( 90.0 ) ) / ( 1.0 - ptype.playerDecay() );

    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -wm.self().body() );

    //
    // prepare parameter cache
    //
    std::vector< double > dash_powers;
    std::vector< double > dash_base_rates;
    std::vector< AngleDeg > accel_angles;
    std::vector< Matrix2D > accel_rot_matrix;
    std::vector< Matrix2D > accel_inv_matrix;
    dash_powers.reserve( dash_angle_divs );
    dash_base_rates.reserve( dash_angle_divs );
    accel_angles.reserve( dash_angle_divs );
    accel_rot_matrix.reserve( dash_angle_divs );
    accel_inv_matrix.reserve( dash_angle_divs );

    for ( size_t d = 0; d < dash_angle_divs; ++d )
    {
        double dir = SP.discretizeDashAngle( SP.minDashAngle() + dash_angle_step * d );
        AngleDeg accel_angle = wm.self().body() + dir;
        double forward_dash_rate = SP.dashDirRate( dir );
        double back_dash_rate = SP.dashDirRate( AngleDeg::normalize_angle( dir + 180.0 ) );
        if ( std::fabs( forward_dash_rate * SP.maxDashPower() )
             > std::fabs( back_dash_rate * SP.minDashPower() ) - 0.001 )
        {
            dash_powers.push_back( SP.maxDashPower() );
            dash_base_rates.push_back( ptype.dashPowerRate() * forward_dash_rate );
        }
        else
        {
            dash_powers.push_back( SP.minDashPower() );
            dash_base_rates.push_back( ptype.dashPowerRate() * back_dash_rate );
        }
        accel_angles.push_back( accel_angle );
        accel_rot_matrix.push_back( Matrix2D::make_rotation( -accel_angle ) );
        accel_inv_matrix.push_back( Matrix2D::make_rotation( accel_angle ) );
    }

    const Vector2D target_rel = rotate_matrix.transform( target_point - wm.self().pos() );
    const int min_step = static_cast< int >( std::ceil( wm.self().pos().dist( target_point )
                                                        / ptype.realSpeedMax() ) );

    //
    // simulation loop
    //
    for ( int reach_step = std::max( 1, min_step - 1 ); reach_step <= max_step; ++reach_step )
    {
#ifdef DEBUG_PRINT_OMNI_DASH
        states.clear();
#endif
        if ( target_rel.absY() - tolerance > max_side_speed * reach_step )
        {
            continue;
        }

        double first_dash_power = 0.0;
        double first_dash_dir = 0.0;

        Vector2D self_pos = wm.self().pos();
        Vector2D self_vel = wm.self().vel();
        StaminaModel stamina_model = wm.self().staminaModel();

        for ( int step = 1; step <= reach_step; ++step )
        {
            const Vector2D required_vel = ( target_point - self_pos )
                * ( ( 1.0 - ptype.playerDecay() )
                    / ( 1.0 - std::pow( ptype.playerDecay(), reach_step - step + 1 ) ) );
            const Vector2D required_accel = required_vel - self_vel;

            double min_dist2 = 10000000.0;
            Vector2D best_pos = self_pos;
            Vector2D best_vel = self_vel;
            double best_dash_power = 0.0;
            double best_dash_dir = 0.0;

            for ( size_t d = 0; d < dash_angle_divs; ++d )
            {
                const Vector2D rel_accel = accel_rot_matrix[d].transform( required_accel );
                if ( rel_accel.x < 0.0 )
                {
#ifdef DEBUG_PRINT_OMNI_DASH
                    dlog.addText( Logger::ACTION,
                                  "(omni_dash) step=%d/%d dir=%.1f skip...",
                                  step, reach_step, SP.minDashAngle() + dash_angle_step * d );
#endif
                    continue;
                }

                const double dash_rate = dash_base_rates[d] * stamina_model.effort();
                double dash_power = rel_accel.x / dash_rate;
                dash_power = std::min( std::fabs( dash_powers[d] ), dash_power );
                if ( dash_powers[d] < 0.0 ) dash_power = -dash_power;
                dash_power = stamina_model.getSafetyDashPower( ptype, dash_power, 1.0 );

#ifdef DEBUG_PRINT_OMNI_DASH
                dlog.addText( Logger::ACTION,
                              "(omni_dash) step=%d/%d dir=%.1f rel_accel_x=%.3f (dash %.2f %.1f)",
                              step, reach_step,
                              SP.minDashAngle() + dash_angle_step * d,
                              rel_accel.x,
                              dash_power,
                              dash_power < 0.0
                              ? SP.minDashAngle() + dash_angle_step * d
                              : AngleDeg::normalize_angle( SP.minDashAngle() + dash_angle_step * d + 180.0 ) );
#endif

                double accel_mag = std::fabs( dash_power ) * dash_rate;
                Vector2D dash_accel = accel_inv_matrix[d].transform( Vector2D( accel_mag, 0.0 ) );
                Vector2D tmp_vel = self_vel + dash_accel;
                Vector2D tmp_pos = self_pos + tmp_vel;
                double d2 = tmp_pos.dist2( target_point );
                if ( d2 < min_dist2 )
                {
                    min_dist2 = d2;
                    best_pos = tmp_pos;
                    best_vel = tmp_vel;
                    best_dash_power = dash_power;
                    best_dash_dir = SP.minDashAngle() + dash_angle_step * d;
                    if ( dash_power < 0.0 )
                    {
                        best_dash_dir = AngleDeg::normalize_angle( best_dash_dir + 180.0 );
                    }
#ifdef DEBUG_PRINT_OMNI_DASH
                    dlog.addText( Logger::ACTION,
                                  "(omni_dash) step=%d/%d updated. dist=%.2f",
                                  step, reach_step, std::sqrt( d2 ) );
#endif
                }
            }
#ifdef DEBUG_PRINT_OMNI_DASH
            states.push_back( State( best_dash_power, best_dash_dir, best_pos ) );
#endif

            self_pos = best_pos;
            self_vel = best_vel;
            self_vel *= ptype.playerDecay();
            stamina_model.simulateDash( ptype, best_dash_power );

            if ( step == 1 )
            {
                first_dash_power = best_dash_power;
                first_dash_dir = best_dash_dir;
            }

            if ( self_pos.dist2( target_point ) < std::pow( tolerance, 2 )
                 || ( wm.self().pos().dist2( self_pos ) > wm.self().pos().dist2( target_point )
                      && Line2D( wm.self().pos(), self_pos ).dist2( target_point ) < std::pow( tolerance, 2 ) ) )
            {
                if ( result )
                {
                    result->setValue( 0, step,
                                      stamina_model.stamina(),
                                      0.0, first_dash_power, first_dash_dir,
                                      wm.self().body() );
                }
#ifdef DEBUG_PRINT_OMNI_DASH
                for ( size_t i = 0; i < states.size(); ++i )
                {
                    dlog.addText( Logger::ACTION,
                                  "(omni_dash) %zd power=%.1f dir=%.1f pos=(%.2f %.2f)",
                                  i, states[i].dash_power_, states[i].dash_dir_,
                                  states[i].result_pos_.x, states[i].result_pos_.y );
                }
#endif
                return step;
            }
        }

    }

    return -1;
}
