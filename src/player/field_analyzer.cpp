// -*-c++-*-

/*!
  \file field_analyzer.cpp
  \brief miscellaneous field analysis Source File
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA, Hiroki SHIMORA

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
#include "config.h"
#endif

#include "field_analyzer.h"
#include "predict_state.h"
#include "pass_checker.h"
#include "strategy.h"

#include <rcsc/action/kick_table.h>
#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/player_predicate.h>
#include <rcsc/player/world_model.h>
#include <rcsc/player/player_object.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/logger.h>
#include <rcsc/color/thermo_color_provider.h>
#include <rcsc/geom/triangle_2d.h>
#include <rcsc/geom/matrix_2d.h>
#include <rcsc/timer.h>
#include <rcsc/math_util.h>

#include <algorithm>

// #define DEBUG_PROFILE

// #ifdef DEBUG_PRINT
// #undef DEBUG_PRINT
// #endif
// #define DEBUG_PRINT

// #define DEBUG_PREDICT_PLAYER_TURN_CYCLE

using namespace rcsc;

namespace {


struct PointDistSorter {

    const Vector2D point_;

    PointDistSorter( const Vector2D & p )
        : point_( p )
      { }

    bool operator()( const Vector2D & lhs,
                     const Vector2D & rhs ) const
      {
          return lhs.dist2( point_ ) < rhs.dist2( point_ );
      }
};

namespace {
/*-------------------------------------------------------------------*/
/*!

 */
bool
opponent_can_block_shoot( const PlayerObject * opponent,
                          const Vector2D & first_ball_pos,
                          const Vector2D & first_ball_vel )
{
    const ServerParam & SP = ServerParam::i();
    const PlayerType * ptype = opponent->playerTypePtr();
    const double goal_line = SP.pitchHalfLength();
    const double control_area = ( opponent->goalie()
                                  ? SP.catchableArea()
                                  : ptype->kickableArea() );

    Vector2D ball_pos = first_ball_pos;
    Vector2D ball_vel = first_ball_vel;

    ball_pos += ball_vel;
    ball_vel *= SP.ballDecay();

    int step = 0;
    while ( ball_pos.x < goal_line || ++step < 20 )
    {
        double dash_dist = opponent->pos().dist( ball_pos ) - control_area;

        if ( dash_dist < 0.001 )
        {
            return true;
        }

        if ( dash_dist > ptype->realSpeedMax() * step )
        {
            ball_pos += ball_vel;
            ball_vel *= SP.ballDecay();
            continue;
        }

        int n_step = ptype->cyclesToReachDistance( dash_dist );

        n_step += 2; // penalty step for observation and turn action
        if ( opponent->isTackling() )
        {
            n_step += 5;
        }

        if ( n_step < step )
        {
            return true;
        }

        ball_pos += ball_vel;
        ball_vel *= SP.ballDecay();
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
get_shoot_point_value( const WorldModel & wm,
                       const Vector2D & pos )
{
    // static const Sector2D shootable_sector( Vector2D( 58.0, 0.0 ),
    //                                  0.0, 20.0,
    //                                  137.5, -137.5 );
    // static const Vector2D goal_pos = ServerParam::i().theirTeamGoalPos();
    static const Vector2D goal_minus( ServerParam::i().pitchHalfLength(),
                                      -ServerParam::i().goalHalfWidth() + 0.5 );
    static const Vector2D goal_plus( ServerParam::i().pitchHalfLength(),
                                     +ServerParam::i().goalHalfWidth() - 0.5 );

    //
    // create shoot blocer candidates
    //

    const Triangle2D shoot_course_triangle( pos, goal_minus, goal_plus );

    std::vector< const PlayerObject * > opponents;
    opponents.reserve( wm.opponents().size() );

    double min_dist = 1000.0;
    for ( PlayerObject::Cont::const_iterator o = wm.opponents().begin(), o_end = wm.opponents().end();
          o != o_end;
          ++o )
    {
        if ( (*o)->posCount() > 10 ) continue;

        double dist = (*o)->pos().dist( pos );
        if ( dist < 1.5 )
        {
            return 0.0;
        }
        if ( dist > 30.0 ) continue;
        if ( shoot_course_triangle.contains( (*o)->pos() )  )
        {
            opponents.push_back( *o );
        }

        if ( dist < min_dist )
        {
            min_dist = dist;
        }
    }

    //
    //
    //

    double total_value = 0.0;

    const Vector2D vec_step = ( goal_plus - goal_minus ) / 10.0;

    for ( int i = 0; i < 11; ++i )
    {
        const Vector2D shoot_target = goal_minus + ( vec_step * i );
        const Vector2D shoot_ball_vel = ( shoot_target - pos ).setLengthVector( ServerParam::i().ballSpeedMax() );

        const double opponent_dist_thr2 = std::pow( pos.dist( shoot_target ) + 3.0, 2 );

        double value = 1.0;
        for ( std::vector< const PlayerObject * >::const_iterator o = opponents.begin(),
                  o_end = opponents.end();
              o != o_end;
              ++o )
        {
            if ( (*o)->pos().dist2( pos ) > opponent_dist_thr2 ) continue;

            if ( opponent_can_block_shoot( *o, pos, shoot_ball_vel ) )
            {
                value = 0.0;
                break;
            }
        }
        total_value += value;
    }

    double opponent_dist_rate = 1.0 - std::exp( -std::pow( min_dist, 2 )
                                                / ( 2.0 * std::pow( 3.0, 2 ) ) );

    return ( total_value == 0.0
             ? 100.0
             : total_value )
        * opponent_dist_rate;
}

/*-------------------------------------------------------------------*/
/*!

 */
inline
double
index_to_x_coordinate( const int ix )
{
    return ServerParam::i().pitchHalfLength() - 1.5 - ix;
}

/*-------------------------------------------------------------------*/
/*!

 */
inline
double
index_to_y_coordinate( const int iy )
{
    return static_cast< double >( iy ) - FieldAnalyzer::SHOOT_AREA_Y_DIVS * 0.5 + 0.5;
}
}
}

/*-------------------------------------------------------------------*/
/*!

 */
FieldAnalyzer::FieldAnalyzer()
    : M_our_shoot_blocker( static_cast< const AbstractPlayerObject * >( 0 ) ),
      M_exist_opponent_man_marker( false ),
      M_exist_opponent_pass_line_marker( false ),
      M_offside_line_speed( 0.0 )
{
    for ( int ix = 0; ix < SHOOT_AREA_X_DIVS; ++ix )
    {
        for ( int iy = 0; iy < SHOOT_AREA_Y_DIVS; ++iy )
        {
            M_shoot_point_values[ix][iy] = 0.0;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
FieldAnalyzer &
FieldAnalyzer::instance()
{
    static FieldAnalyzer s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FieldAnalyzer::init( const std::string & ball_table_file )
{
    if ( ! M_ball_move_model.init( ball_table_file ) )
    {
        return false;
    }

    //std::cerr << ball_table_file << ": initialize ball error table." << std::endl;

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldAnalyzer::estimate_virtual_dash_distance( const AbstractPlayerObject * player )
{
    const int pos_count = std::min( 10, // 15 // Magic Number
                                    std::min( player->seenPosCount(),
                                              player->posCount() ) );
    const double max_speed = player->playerTypePtr()->realSpeedMax() * 0.9; // Magic Number

    double d = 0.0;
    for ( int i = 1; i <= pos_count; ++i ) // start_value==1 to set the initial_value<1
    {
        //d += max_speed * std::exp( - (i*i) / 20.0 ); // Magic Number
        d += max_speed * std::exp( - (i*i) / 15.0 ); // Magic Number
        //d += max_speed * std::pow( 0.99, i );
    }

    return d;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
FieldAnalyzer::predict_player_turn_cycle( const PlayerType * ptype,
                                          const AngleDeg & player_body,
                                          const double & player_speed,
                                          const double & target_dist,
                                          const AngleDeg & target_angle,
                                          const double & dist_thr,
                                          const bool use_back_dash )
{
    const ServerParam & SP = ServerParam::i();

    int n_turn = 0;

    double angle_diff = ( target_angle - player_body ).abs();

    if ( use_back_dash
         && target_dist < 5.0 // Magic Number
         && angle_diff > 90.0
         && SP.minDashPower() < -SP.maxDashPower() + 1.0 )
    {
        angle_diff = std::fabs( angle_diff - 180.0 );    // assume backward dash
    }

    double turn_margin = 180.0;
    if ( dist_thr < target_dist )
    {
        turn_margin = std::max( 15.0, // Magic Number
                                AngleDeg::asin_deg( dist_thr / target_dist ) );
    }

    double speed = player_speed;
    while ( angle_diff > turn_margin )
    {
        angle_diff -= ptype->effectiveTurn( SP.maxMoment(), speed );
        speed *= ptype->playerDecay();
        ++n_turn;
    }

#ifdef DEBUG_PREDICT_PLAYER_TURN_CYCLE
    dlog.addText( Logger::ANALYZER,
                  "(predict_player_turn_cycle) angleDiff=%.3f turnMargin=%.3f speed=%.2f n_turn=%d",
                  angle_diff, turn_margin, player_speed, n_turn );
#endif

    return n_turn;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
FieldAnalyzer::predict_self_reach_cycle( const WorldModel & wm,
                                         const Vector2D & target_point,
                                         const double & dist_thr,
                                         const int wait_cycle,
                                         const bool save_recovery,
                                         StaminaModel * stamina )
{
    if ( wm.self().inertiaPoint( wait_cycle ).dist2( target_point ) < std::pow( dist_thr, 2 ) )
    {
        return 0;
    }

    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();
    const double recover_dec_thr = SP.recoverDecThrValue();

    const double first_speed = wm.self().vel().r() * std::pow( ptype.playerDecay(), wait_cycle );

    StaminaModel first_stamina_model = wm.self().staminaModel();
    if ( wait_cycle > 0 )
    {
        first_stamina_model.simulateWaits( ptype, wait_cycle );
    }

    for ( int cycle = std::max( 0, wait_cycle ); cycle < 30; ++cycle )
    {
        const Vector2D inertia_pos = wm.self().inertiaPoint( cycle );
        const double target_dist = inertia_pos.dist( target_point );

        if ( target_dist < dist_thr )
        {
            return cycle;
        }

        double dash_dist = target_dist - dist_thr * 0.5;

        if ( dash_dist > ptype.realSpeedMax() * ( cycle - wait_cycle ) )
        {
            continue;
        }

        AngleDeg target_angle = ( target_point - inertia_pos ).th();

        //
        // turn
        //

        int n_turn = predict_player_turn_cycle( &ptype,
                                                wm.self().body(),
                                                first_speed,
                                                target_dist,
                                                target_angle,
                                                dist_thr,
                                                false );
        if ( wait_cycle + n_turn >= cycle )
        {
            continue;
        }

        StaminaModel stamina_model = first_stamina_model;
        if ( n_turn > 0 )
        {
            stamina_model.simulateWaits( ptype, n_turn );
        }

        //
        // dash
        //

        int n_dash = ptype.cyclesToReachDistance( dash_dist );
        if ( wait_cycle + n_turn + n_dash > cycle )
        {
            continue;
        }

        double speed = first_speed * std::pow( ptype.playerDecay(), n_turn );
        double reach_dist = 0.0;

        n_dash = 0;
        while ( wait_cycle + n_turn + n_dash < cycle )
        {
            double dash_power = std::min( SP.maxDashPower(), stamina_model.stamina() );
            if ( save_recovery
                 && stamina_model.stamina() - dash_power < recover_dec_thr )
            {
                dash_power = std::max( 0.0, stamina_model.stamina() - recover_dec_thr );
                if ( dash_power < 1.0 )
                {
                    break;
                }
            }

            double accel = dash_power * ptype.dashPowerRate() * stamina_model.effort();
            speed += accel;
            if ( speed > ptype.playerSpeedMax() )
            {
                speed = ptype.playerSpeedMax();
            }

            reach_dist += speed;
            speed *= ptype.playerDecay();

            stamina_model.simulateDash( ptype, dash_power );

            ++n_dash;

            if ( reach_dist >= dash_dist )
            {
                break;
            }
        }

        if ( reach_dist >= dash_dist )
        {
            if ( stamina )
            {
                *stamina = stamina_model;
            }
            return wait_cycle + n_turn + n_dash;
        }
    }

    return 1000;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
FieldAnalyzer::predict_player_reach_cycle( const AbstractPlayerObject * player,
                                           const Vector2D & target_point,
                                           const double & dist_thr,
                                           const double & penalty_distance,
                                           const int body_count_thr,
                                           const int default_n_turn,
                                           const int wait_cycle,
                                           const bool use_back_dash )
{
    const PlayerType * ptype = player->playerTypePtr();

    const Vector2D & first_player_pos = ( player->seenPosCount() <= player->posCount()
                                          ? player->seenPos()
                                          : player->pos() );
    const Vector2D & first_player_vel = ( player->seenVelCount() <= player->velCount()
                                          ? player->seenVel()
                                          : player->vel() );
    const double first_player_speed = first_player_vel.r() * std::pow( ptype->playerDecay(), wait_cycle );

    int final_reach_cycle = -1;
    {
        Vector2D inertia_pos = ptype->inertiaFinalPoint( first_player_pos, first_player_vel );
        double target_dist = inertia_pos.dist( target_point );

        int n_turn = ( player->bodyCount() > body_count_thr
                       ? default_n_turn
                       : predict_player_turn_cycle( ptype,
                                                    player->body(),
                                                    first_player_speed,
                                                    target_dist,
                                                    ( target_point - inertia_pos ).th(),
                                                    dist_thr,
                                                    use_back_dash ) );
        int n_dash = ptype->cyclesToReachDistance( target_dist + penalty_distance );

        final_reach_cycle = wait_cycle + n_turn + n_dash;
    }

    const int max_cycle = 6; // Magic Number

    if ( final_reach_cycle > max_cycle )
    {
        return final_reach_cycle;
    }

    for ( int cycle = std::max( 0, wait_cycle ); cycle <= max_cycle; ++cycle )
    {
        Vector2D inertia_pos = ptype->inertiaPoint( first_player_pos, first_player_vel, cycle );
        double target_dist = inertia_pos.dist( target_point ) + penalty_distance;

        if ( target_dist < dist_thr )
        {
            return cycle;
        }

        double dash_dist = target_dist - dist_thr * 0.5;

        if ( dash_dist < 0.001 )
        {
            return cycle;
        }

        if ( dash_dist > ptype->realSpeedMax() * ( cycle - wait_cycle ) )
        {
            continue;
        }

        int n_dash = ptype->cyclesToReachDistance( dash_dist );

        if ( wait_cycle + n_dash > cycle )
        {
            continue;
        }

        int n_turn = ( player->bodyCount() > body_count_thr
                       ? default_n_turn
                       : predict_player_turn_cycle( ptype,
                                                    player->body(),
                                                    first_player_speed,
                                                    target_dist,
                                                    ( target_point - inertia_pos ).th(),
                                                    dist_thr,
                                                    use_back_dash ) );

        if ( wait_cycle + n_turn + n_dash <= cycle )
        {
            return cycle;
        }

    }

    return final_reach_cycle;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
FieldAnalyzer::predict_kick_count( const WorldModel & wm,
                                   const AbstractPlayerObject * kicker,
                                   const double & first_ball_speed,
                                   const AngleDeg & ball_move_angle )
{
    if ( wm.gameMode().type() != GameMode::PlayOn
         && ! wm.gameMode().isPenaltyKickMode() )
    {
        return 1;
    }

    if ( kicker->unum() == wm.self().unum()
         && wm.self().isKickable() )
    {
        Vector2D max_vel = KickTable::calc_max_velocity( ball_move_angle,
                                                         wm.self().kickRate(),
                                                         wm.ball().vel() );
        if ( max_vel.r2() >= std::pow( first_ball_speed, 2 ) )
        {
            return 1;
        }
    }

    if ( first_ball_speed > 2.5 )
    {
        if ( ( kicker->angleFromBall() - ball_move_angle ).abs() > 120.0 )
        {
            return 2;
        }
        else
        {
            return 3;
        }
    }

    return 2;
}


/*-------------------------------------------------------------------*/
/*!

 */
int
FieldAnalyzer::get_pass_count( const PredictState & state,
                               const PassChecker & pass_checker,
                               const double first_ball_speed,
                               const int max_count )
{
    const AbstractPlayerObject & from = state.ballHolder();

    int pass_count = 0;
    for ( PredictPlayerObject::Cont::const_iterator it = state.ourPlayers().begin(), end = state.ourPlayers().end();
          it != end;
          ++it )
    {
        if ( ! (*it)->isValid()
             || (*it)->unum() == from.unum() )
        {
            continue;
        }

        const double dist = from.pos().dist( (*it)->pos() );
        const int ball_step
            = static_cast< int >( std::ceil( calc_length_geom_series( ServerParam::i().ballSpeedMax(),
                                                                      dist,
                                                                      ServerParam::i().ballDecay() ) ) );

        double success_prob = pass_checker( state,
                                            &from,
                                            (*it).get(),
                                            (*it)->pos(),
                                            first_ball_speed,
                                            ball_step );

        // TODO: determine the optimized value.
        if ( success_prob >= 0.5 )
        {
            ++pass_count;

            if ( max_count >= 0
                 && pass_count >= max_count )
            {
                return max_count;
            }
        }
    }

    return pass_count;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldAnalyzer::get_congestion( const PredictState & state,
                               const Vector2D & point,
                               const int opponent_additional_chase_time )
{
    //static const double factor = 1.0 / ( 2.0 * std::pow( 7.0, 2 ) );
    //static const double line_factor = 1.0 / ( 2.0 * std::pow( 3.0, 2 ) );
    //static const double factor = 1.0 / ( 2.0 * std::pow( 3.0, 2 ) );
    static const double factor = 1.0 / ( 2.0 * std::pow( 2.5, 2 ) );
    // static const double line_factor = 1.0 / ( 2.0 * std::pow( 1.5, 2 ) );

    double congestion = 0.0;

    for ( PredictPlayerObject::Cont::const_iterator it = state.theirPlayers().begin(),
              end = state.theirPlayers().end();
          it != end;
          ++it )
    {
        if ( (*it)->goalie() ) continue;
        if ( (*it)->ghostCount() >= 3 ) continue;

        double d = (*it)->pos().dist( point );
        d -= (*it)->playerTypePtr()->realSpeedMax()
            * ( bound( 0, (*it)->posCount() - 2, 2 ) + opponent_additional_chase_time );

        congestion += std::exp( - std::pow( d, 2 ) * factor );
    }

    // const double dist_line = std::min( std::fabs( ServerParam::i().pitchHalfLength() - point.absX() ),
    //                                    std::fabs( ServerParam::i().pitchHalfWidth() - point.absY() ) );
    // congestion += std::exp( - std::pow( dist_line, 2 ) * line_factor );

    return congestion;
}

/*-------------------------------------------------------------------*/
/*!

 */
const AbstractPlayerObject *
FieldAnalyzer::get_blocker( const WorldModel & wm,
                            const Vector2D & opponent_pos )
{
    return get_blocker( wm,
                        opponent_pos,
                        Vector2D( -ServerParam::i().pitchHalfLength()*0.6
                                  + ServerParam::i().ourPenaltyAreaLineX()*0.4,
                                  0.0 ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
const AbstractPlayerObject *
FieldAnalyzer::get_blocker( const WorldModel & wm,
                            const Vector2D & opponent_pos,
                            const Vector2D & base_pos )
{
    static const double min_dist_thr2 = std::pow( 1.0, 2 );
    static const double max_dist_thr2 = std::pow( 4.0, 2 );
    static const double angle_thr = 15.0;

    const AngleDeg attack_angle = ( base_pos - opponent_pos ).th();

    for ( AbstractPlayerObject::Cont::const_iterator t = wm.ourPlayers().begin(),
              end = wm.ourPlayers().end();
          t != end;
          ++t )
    {
        if ( (*t)->goalie() ) continue;
        if ( (*t)->posCount() >= 5 ) continue;
        if ( (*t)->unumCount() >= 10 ) continue;
        if ( (*t)->ghostCount() >= 2 ) continue;

        double d2 = opponent_pos.dist2( (*t)->pos() );
        if ( d2 < min_dist_thr2
             || max_dist_thr2 < d2 )
        {
            continue;
        }

        AngleDeg teammate_angle = ( (*t)->pos() - opponent_pos ).th();

        if ( ( teammate_angle - attack_angle ).abs() < angle_thr )
        {
            return *t;
        }
    }

    return static_cast< const AbstractPlayerObject * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
FieldAnalyzer::get_field_bound_predict_ball_pos( const WorldModel & wm,
                                                 const int predict_step,
                                                 const double shrink_offset )
{
    const double half_len = ServerParam::i().pitchHalfLength() - shrink_offset;
    const double half_wid = ServerParam::i().pitchHalfWidth() - shrink_offset;
    const Rect2D pitch_rect = Rect2D::from_center( Vector2D( 0.0, 0.0 ),
                                                   half_len*2.0, half_wid*2.0 );

    const Vector2D current_pos = wm.ball().pos();
    const Vector2D predict_pos = wm.ball().inertiaPoint( predict_step );

    if ( pitch_rect.contains( current_pos )
         && pitch_rect.contains( predict_pos ) )
    {
        return predict_pos;
    }

    Vector2D sol1, sol2;
    int n = pitch_rect.intersection( Segment2D( current_pos, predict_pos ),
                                     &sol1, &sol2 );
    if ( n == 0 )
    {
        return predict_pos;
    }
    else if ( n == 1 )
    {
        return sol1;
    }

    return Vector2D( bound( -half_len, current_pos.x, +half_len ),
                     bound( -half_wid, current_pos.y, +half_wid ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
FieldAnalyzer::get_field_bound_opponent_ball_pos( const WorldModel & wm )
{
    Vector2D pos = get_field_bound_predict_ball_pos( wm,
                                                     wm.interceptTable()->opponentReachStep(),
                                                     0.5 );
    if ( wm.kickableOpponent()
         || wm.interceptTable()->opponentReachStep() == 0 )
    {
        const PlayerObject * fastest_opponent = wm.interceptTable()->fastestOpponent();
        if ( fastest_opponent
             && fastest_opponent->posCount() <= wm.ball().posCount() )
        {
            pos = fastest_opponent->inertiaFinalPoint();
        }
    }

    return pos;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FieldAnalyzer::is_ball_moving_to_our_goal( const WorldModel & wm )
{
    static GameTime s_last_time( -1, 0 );
    static bool s_last_result = false;

    if ( s_last_time == wm.time() )
    {
        return s_last_result;
    }
    s_last_time = wm.time();


    const ServerParam & SP = ServerParam::i();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const Vector2D self_reach_point = wm.ball().inertiaPoint( self_min );

    if ( self_reach_point.x < -SP.pitchHalfLength() )
    {
        const Ray2D ball_ray( wm.ball().pos(), wm.ball().vel().th() );
        const Line2D goal_line( Vector2D( -SP.pitchHalfLength(), 10.0 ),
                                Vector2D( -SP.pitchHalfLength(), -10.0 ) );

        const Vector2D intersect = ball_ray.intersection( goal_line );
        if ( intersect.isValid()
             && intersect.absY() < SP.goalHalfWidth() + 1.0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": ball will be in our goal. intersect=(%.2f %.2f)",
                          intersect.x, intersect.y );
            s_last_result = true;
            return true;
        }
    }

    s_last_result = false;
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FieldAnalyzer::is_ball_moving_to_their_goal( const WorldModel & wm )
{
    if ( wm.self().isKickable()
         || wm.kickableTeammate()
         || wm.kickableOpponent() )
    {
        return false;
    }

    const Segment2D goal( Vector2D( +ServerParam::i().pitchHalfLength(),
                                    -ServerParam::i().goalHalfWidth() - 1.5 ),
                          Vector2D( +ServerParam::i().pitchHalfLength(),
                                    +ServerParam::i().goalHalfWidth() + 1.5 ) );
    const Segment2D ball_move( wm.ball().pos(), wm.ball().inertiaFinalPoint() );

    return goal.intersectsExceptEndpoint( ball_move );
}

namespace {
/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_self_pos_after_dash( const WorldModel & wm,
                         const double dash_power,
                         const double dash_dir )
{
    const PlayerType & ptype = wm.self().playerType();
    double accel = dash_power
        * ptype.dashPowerRate()
        * ServerParam::i().dashDirRate( dash_dir )
        * wm.self().effort();

    Vector2D pos = wm.self().pos() + wm.self().vel();
    pos += Vector2D::from_polar( accel, wm.self().body() + dash_dir );
    return pos;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
get_tackle_probability( const Vector2D & player_to_ball )
{
    const ServerParam & SP = ServerParam::i();

    double tackle_dist = ( player_to_ball.x > 0.0
                           ? SP.tackleDist()
                           : SP.tackleBackDist() );
    if ( tackle_dist < EPS
         || SP.tackleWidth() < EPS )
    {
        return 0.0;
    }

    double fail_prob = ( std::pow( player_to_ball.absX() / tackle_dist,
                                   SP.tackleExponent() )
                         + std::pow( player_to_ball.absY() / SP.tackleWidth(),
                                     SP.tackleExponent() ) );
    return std::max( 0.0, 1.0 - fail_prob );
}
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldAnalyzer::get_tackle_probability_after_dash( const WorldModel & wm,
                                                  double * result_dash_power,
                                                  double * result_dash_dir )
{
    const ServerParam & SP = ServerParam::i();

    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -wm.self().body() );
    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();

    const double max_dash_power
        = wm.self().staminaModel().getSafetyDashPower( wm.self().playerType(),
                                                       SP.maxDashPower(),
                                                       0.001 );
    if ( wm.self().pos().dist( ball_next ) > ( std::sqrt( std::pow( SP.tackleDist(), 2 )
                                                          + std::pow( SP.tackleWidth(), 2 ) )
                                               + max_dash_power * wm.self().dashRate() ) )
    {
        return 0.0;
    }

    const double dash_angle_step = std::max( 15.0, SP.dashAngleStep() );
    const size_t dash_angle_divs = static_cast< size_t >( std::floor( 360.0 / dash_angle_step ) );

    double max_prob = 0.0;
    for ( size_t d = 0; d < dash_angle_divs; ++d )
    {
        double dir = SP.discretizeDashAngle( SP.minDashAngle() + dash_angle_step * d );
        Vector2D self_next = get_self_pos_after_dash( wm, max_dash_power, dir );
        double prob = get_tackle_probability( rotate_matrix.transform( ball_next - self_next ) );
        if ( prob > max_prob )
        {
            max_prob = prob;
            if ( result_dash_power ) *result_dash_power = max_dash_power;
            if ( result_dash_dir ) *result_dash_dir = dir;
        }
    }
    return max_prob;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldAnalyzer::get_tackle_probability_after_turn( const WorldModel & wm )
{
    const Vector2D self_next = wm.self().pos() + wm.self().vel();
    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    const Vector2D ball_rel = ball_next - self_next;
    const double ball_dist2 = ball_rel.r2();

    if ( ball_dist2 > ( std::pow( ServerParam::i().tackleDist(), 2 )
                        + std::pow( ServerParam::i().tackleWidth(), 2 ) ) )
    {
        return 0.0;
    }

    double max_turn = wm.self().playerType().effectiveTurn( ServerParam::i().maxMoment(),
                                                            wm.self().vel().r() );
    AngleDeg ball_angle = ( ball_next - self_next ).th() - wm.self().body();
    ball_angle = std::max( 0.0, ball_angle.abs() - max_turn );

    return get_tackle_probability( Vector2D::from_polar( std::sqrt( ball_dist2 ), ball_angle ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::update( const WorldModel & wm )
{
    static GameTime s_update_time( 0, 0 );

    if ( s_update_time == wm.time() )
    {
        return;
    }
    s_update_time = wm.time();

    if ( wm.gameMode().type() == GameMode::BeforeKickOff
         || wm.gameMode().type() == GameMode::AfterGoal_
         || wm.gameMode().isPenaltyKickMode() )
    {
        return;
    }

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    updateVoronoiDiagram( wm );
    updatePlayerGraph( wm );
    //updateOurShootBlocker( wm );
    //updateShootPointValues( wm );
    updateOpponentManMarker( wm );
    updateOpponentPassLineMarker( wm );
    //updateOffsideLines( wm );

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::ANALYZER,
                  "FieldAnalyzer::update() elapsed %f [ms]",
                  timer.elapsedReal() );
#endif

    // if ( dlog.isEnabled( Logger::ANALYZER ) )
    // {
    //     debugPrintMovableRange( wm );
    // }
    // if ( dlog.isEnabled( Logger::SHOOT ) )
    // {
    //     debugPrintShootPositions();
    // }
    // if ( dlog.isEnabled( Logger::PASS ) ) //|| dlog.isEnabled( Logger::ANALYZER )
    // {
    //     debugPrintTargetVoronoiDiagram();
    // }
    // if ( dlog.isEnabled( Logger::POSITIONING ) ) //|| dlog.isEnabled( Logger::ANALYZER )
    // {
    //     debugPrintPositioningVoronoiDiagram();
    // }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::updateVoronoiDiagram( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();
    const double pitch_l = SP.pitchLength();
    const double pitch_half_l = pitch_l * 0.5;
    const double pitch_w = SP.pitchWidth();
    const double pitch_half_w = SP.pitchHalfWidth();

    const Rect2D target_rect = Rect2D::from_center( 0.0, 0.0,
                                                    pitch_l - 5.0,
                                                    pitch_w - 5.0 );
    const Rect2D positioning_rect = Rect2D::from_center( 0.0, 0.0,
                                                         pitch_l - 1.0,
                                                         pitch_w - 1.0 );
    M_target_voronoi_diagram.clear();
    M_voronoi_target_points.clear();
    M_positioning_voronoi_diagram.clear();

    for ( AbstractPlayerObject::Cont::const_iterator p = wm.theirPlayers().begin(),
              end = wm.theirPlayers().end();
          p != end;
          ++p )
    {
        M_target_voronoi_diagram.addPoint( (*p)->pos() );
        M_positioning_voronoi_diagram.addPoint( (*p)->pos() );
    }


    for ( AbstractPlayerObject::Cont::const_iterator p = wm.ourPlayers().begin(),
              end = wm.ourPlayers().end();
          p != end;
          ++p )
    {
        if ( wm.self().unum() == (*p)->unum() ) continue;
        M_positioning_voronoi_diagram.addPoint( (*p)->pos() );
    }


    // our goal
    M_target_voronoi_diagram.addPoint( Vector2D( - SP.pitchHalfLength() + 5.5, 0.0 ) );

    // opponent side corners
    M_target_voronoi_diagram.addPoint( Vector2D( +pitch_half_l, -pitch_half_w ) );
    M_target_voronoi_diagram.addPoint( Vector2D( +pitch_half_l, +pitch_half_w ) );

    // M_target_voronoi_diagram.addPoint( Vector2D(   0.0, -pitch_half_w - 5.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( +15.0, -pitch_half_w - 5.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( +20.0, -pitch_half_w - 5.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( +30.0, -pitch_half_w - 5.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( +40.0, -pitch_half_w - 5.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( +50.0, -pitch_half_w - 5.0 ) );

    // M_target_voronoi_diagram.addPoint( Vector2D(   0.0, +pitch_half_w + 5.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( +10.0, +pitch_half_w + 5.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( +20.0, +pitch_half_w + 5.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( +20.0, +pitch_half_w + 5.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( +30.0, +pitch_half_w + 5.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( +50.0, +pitch_half_w + 5.0 ) );

    // M_target_voronoi_diagram.addPoint( Vector2D( pitch_half_l, +5.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( pitch_half_l, -5.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( pitch_half_l, +15.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( pitch_half_l, -15.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( pitch_half_l, +25.0 ) );
    // M_target_voronoi_diagram.addPoint( Vector2D( pitch_half_l, -25.0 ) );

    M_positioning_voronoi_diagram.addPoint( Vector2D( +pitch_half_l + 10.0, -pitch_half_w - 10.0 ) );
    M_positioning_voronoi_diagram.addPoint( Vector2D( +pitch_half_l + 10.0, +pitch_half_w + 10.0 ) );

    //
    // set side points
    //
    for ( double x = -10.0; x < pitch_half_l - 5.0; x += 10.0 )
    {
        M_positioning_voronoi_diagram.addPoint( Vector2D( x, - pitch_half_w - 10.0 ) );
        M_positioning_voronoi_diagram.addPoint( Vector2D( x, + pitch_half_w + 10.0 ) );
    }

    //
    // set bounding rect
    //
    M_target_voronoi_diagram.setBoundingRect( target_rect );
    M_positioning_voronoi_diagram.setBoundingRect( positioning_rect );

    //
    // computation
    //
    // M_all_players_voronoi_diagram.compute();
    // M_teammates_voronoi_diagram.compute();
    M_target_voronoi_diagram.compute();
    M_positioning_voronoi_diagram.compute();

    //
    // create points on segments
    //

    const double MIN_LEN = 3.0;
    const int MAX_DIV = 8;
    M_target_voronoi_diagram.getPointsOnSegments( MIN_LEN, MAX_DIV, &M_voronoi_target_points );
    std::sort( M_voronoi_target_points.begin(), M_voronoi_target_points.end(),
               PointDistSorter( ServerParam::i().theirTeamGoalPos() ) );

}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::updatePlayerGraph( const WorldModel & wm )
{
    //
    // update field player graph
    //
    M_our_players_graph.setPredicate( new AndPlayerPredicate
                                      ( new FieldPlayerPredicate(),
                                        new TeammateOrSelfPlayerPredicate( wm ) ) );
    M_our_players_graph.update( wm );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::updateOurShootBlocker( const WorldModel & wm )
{
    // clear old data
    M_our_shoot_blocker = static_cast< const AbstractPlayerObject * >( 0 );

    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( ! opponent )
    {
        return;
    }

    const ServerParam & SP = ServerParam::i();

    const Vector2D ball_pos = ( wm.kickableOpponent()
                                ? wm.kickableOpponent()->inertiaFinalPoint()
                                : wm.ball().inertiaPoint( opp_min ) );

    if ( SP.ourTeamGoalPos().dist2( ball_pos ) > std::pow( 25.0, 2 ) )
    {
        return;
    }

    const Vector2D goal_post_minus( -SP.pitchHalfLength(), -SP.goalHalfWidth() );
    const Vector2D goal_post_plus( -SP.pitchHalfLength(), +SP.goalHalfWidth() );

    // const Line2D goal_post_line_minus( ball_pos, goal_post_minus );
    // const Line2D goal_post_line_plus( ball_pos, goal_post_plus );
    const Line2D goal_c_line( ball_pos, SP.ourTeamGoalPos() );

    const AngleDeg goal_post_angle_minus = ( goal_post_minus - ball_pos ).th();
    const AngleDeg goal_post_angle_plus = ( goal_post_minus - ball_pos ).th();

    //
    // TODO: check goalie position
    //

    const AbstractPlayerObject * candidate = static_cast< const AbstractPlayerObject * >( 0 );

    for ( AbstractPlayerObject::Cont::const_iterator t = wm.ourPlayers().begin(),
              end = wm.ourPlayers().end();
          t != end;
          ++t )
    {
        if ( (*t)->goalie() ) continue;

        const Vector2D player_pos = (*t)->inertiaFinalPoint();
        const AngleDeg player_angle = ( player_pos - ball_pos ).th();
        if ( ( player_angle.isLeftOf( goal_post_angle_minus )
               && player_angle.isRightOf( goal_post_angle_plus ) )
             || goal_c_line.dist2( player_pos ) < std::pow( (*t)->playerTypePtr()->kickableArea(), 2 ) )
        {
            if ( ! candidate )
            {
                candidate = *t;
#ifdef DEBUG_PRINT
                dlog.addText( Logger::ANALYZER,
                              __FILE__": update shoot blocker(1) %d (%.1f %.1f)",
                              candidate->unum(),
                              candidate->pos().x, candidate->pos().y );
#endif
            }
            else
            {
                if ( candidate->pos().x > (*t)->pos().x )
                {
                    candidate = *t;
#ifdef DEBUG_PRINT
                    dlog.addText( Logger::ANALYZER,
                                  __FILE__": update shoot blocker(2) %d (%.1f %.1f)",
                                  candidate->unum(),
                                  candidate->pos().x, candidate->pos().y );
#endif
                }
            }
        }
    }

#ifdef DEBUG_PRINT
    if ( candidate )
    {
        dlog.addText( Logger::ANALYZER,
                      __FILE__": exist shoot blocker %d (%.1f %.1f)",
                      candidate->unum(),
                      candidate->pos().x, candidate->pos().y );
    }
#endif

    M_our_shoot_blocker = candidate;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::updateShootPointValues( const WorldModel & wm )
{
    for ( int ix = 0; ix < SHOOT_AREA_X_DIVS; ++ix )
    {
        const double x = index_to_x_coordinate( ix );

        for ( int iy = 0; iy < SHOOT_AREA_Y_DIVS; ++iy )
        {
            const double y = index_to_y_coordinate( iy );
            M_shoot_point_values[ix][iy] = get_shoot_point_value( wm, Vector2D( x, y ) );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::updateOpponentManMarker( const WorldModel & wm )
{
    static GameTime s_last_detect_time( -1, 0 );
    static int s_detect_count = 0;

    if ( wm.gameMode().type() == GameMode::BeforeKickOff
         || wm.gameMode().type() == GameMode::AfterGoal_
         || wm.gameMode().isPenaltyKickMode() )
    {
        M_exist_opponent_man_marker = false;
        return;
    }

    const PlayerObject * opponent = static_cast< const PlayerObject * >( 0 );

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        if ( (*o)->isGhost() ) continue;
        if ( (*o)->posCount() > 20 ) continue;
        if ( (*o)->distFromSelf() > 5.0 ) break;

        if ( (*o)->distFromSelf() < 2.0 + 1.0 * (*o)->seenPosCount() )
        {
            opponent = *o;
            break;
        }
    }

    if ( ! opponent )
    {
        dlog.addText( Logger::ANALYZER,
                      __FILE__":(updateOpponentManMarker) no man marker" );
        s_detect_count = 0;
        M_exist_opponent_man_marker = false;
        return;
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__":(updateOpponentManMarker) detect unum=%d dist=%.3f",
                  opponent->unum(), opponent->distFromSelf() );

    if ( s_last_detect_time.cycle() == wm.time().cycle() - 1
         || s_last_detect_time.stopped() == wm.time().stopped() - 1 )
    {
        ++s_detect_count;
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__":(updateOpponentManMarker) detect_count=%d",
                  s_detect_count );

    if ( s_detect_count >= 5 )
    {
        dlog.addText( Logger::ANALYZER,
                      __FILE__":(updateOpponentManMarker) exist man marker" );
        M_exist_opponent_man_marker = true;
    }

    s_last_detect_time = wm.time();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::updateOpponentPassLineMarker( const WorldModel & wm )
{
    static GameTime s_last_detect_time( -1, 0 );
    static int s_detect_count = 0;

    if ( wm.gameMode().type() == GameMode::BeforeKickOff
         || wm.gameMode().isPenaltyKickMode() )
    {
        s_detect_count = 0;
        M_exist_opponent_pass_line_marker = false;
        return;
    }

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const Vector2D teammate_ball_pos = wm.ball().inertiaPoint( wm.interceptTable()->teammateReachStep() );

    const double ball_move_dist = teammate_ball_pos.dist( home_pos );

    double first_ball_speed = ServerParam::i().ballSpeedMax();
    int ball_move_step = ServerParam::i().ballMoveStep( first_ball_speed, ball_move_dist );
    if ( ball_move_step <= 0 )
    {
        s_detect_count = 0;
        M_exist_opponent_pass_line_marker = false;
        return;
    }

    if ( ball_move_step < 3 )
    {
        ball_move_step = 3;
        first_ball_speed = ServerParam::i().firstBallSpeed( ball_move_dist, ball_move_step );
    }

    const Vector2D first_ball_vel = ( home_pos - teammate_ball_pos ).setLengthVector( first_ball_speed );

    dlog.addText( Logger::ANALYZER,
                  __FILE__":(updateOpponentPassLineMarker) check pass: home=(%.2f %.2f) ball_step=%d first_speed=%.3f",
                  home_pos.x, home_pos.y, ball_move_step, first_ball_speed );

    const PlayerObject * opponent = static_cast< const PlayerObject * >( 0 );
    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        if ( (*o)->isGhost() ) continue;
        if ( (*o)->posCount() > 20 ) continue;
        if ( (*o)->distFromSelf() > 5.0 ) break;

        const PlayerType * ptype = (*o)->playerTypePtr();
        Vector2D ball_pos = teammate_ball_pos;
        Vector2D ball_vel = first_ball_vel;

        for ( int step = 1; step <= ball_move_step; ++step )
        {
            ball_pos += ball_vel;
            ball_vel *= ServerParam::i().ballDecay();

            if ( ptype->cyclesToReachDistance( (*o)->pos().dist( ball_pos ) ) <= step + 2 )
            {
                dlog.addText( Logger::ANALYZER,
                              __FILE__":(updateOpponentPassLineMarker) detect pass line marker %d step=%d",
                              (*o)->unum() );
                opponent = *o;
                break;
            }
        }

        if ( opponent )
        {
            break;
        }
    }

    if ( ! opponent )
    {
        dlog.addText( Logger::ANALYZER,
                      __FILE__":(updateOpponentPassLineMarker) no pass line marker" );
        s_detect_count = 0;
        M_exist_opponent_pass_line_marker = false;
        return;
    }

    if ( s_last_detect_time.cycle() == wm.time().cycle() - 1
         || s_last_detect_time.stopped() == wm.time().stopped() - 1 )
    {
        ++s_detect_count;
    }
    else
    {
        s_detect_count = 1;
    }

    dlog.addText( Logger::ANALYZER,
                  __FILE__":(updateOpponentPassLineMarker) detect_count=%d",
                  s_detect_count );

    if ( s_detect_count >= 5 )
    {
        dlog.addText( Logger::ANALYZER,
                      __FILE__":(updateOpponentPassLineMarker) exist pass line marker %d",
                      opponent->unum() );
        M_exist_opponent_pass_line_marker = true;
    }

    s_last_detect_time = wm.time();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::updateOffsideLines( const WorldModel & wm )
{
    M_offside_line_speed = 0.0;

    if ( wm.gameMode().type() != GameMode::PlayOn )
    {
        M_playon_offside_lines.clear();
        return;
    }

    if ( M_playon_offside_lines.size() >= 20 )
    {
        M_playon_offside_lines.pop_back();
    }

    M_playon_offside_lines.push_front( wm.offsideLineX() );

    if ( M_playon_offside_lines.size() == 1 )
    {
        return;
    }

    int count = 0;
    double sum = 0.0;
    std::list< double >::const_iterator x = M_playon_offside_lines.begin();
    double next_x = *x;
    ++x;
    while ( count < 10
            && x != M_playon_offside_lines.end() )
    {
        dlog.addText( Logger::ANALYZER,
                      "(updateOffsideLines) count=%d diff=%.2f",
                      count, next_x - *x );
        sum += next_x - *x;
        next_x = *x;
        ++count;
        ++x;
    }

    if ( count > 0 )
    {
        M_offside_line_speed = sum / count;
    }

    dlog.addText( Logger::ANALYZER,
                  "(updateOffsideLines) sum=%.1f count=%d",
                  sum, count );
    dlog.addText( Logger::ANALYZER,
                  "(updateOffsideLines) offside_line_speed(avg.) %.1f",
                  M_offside_line_speed );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::debugPrintMovableRange( const WorldModel & wm )
{
    const int dir_step = 15;

    const PlayerType & ptype = wm.self().playerType();
    const double first_speed = wm.self().vel().r();

    std::vector< Vector2D > polygon;
    polygon.reserve( 360 / dir_step );

    // 1 dash (with omnidir dash )
    {
        const Vector2D inertia_pos = wm.self().pos() + wm.self().vel();
        for ( double dir = -180.0; dir < 180.0; dir += 45.0 )
        {
            double front_max_accel = ServerParam::i().maxDashPower()
                * ptype.dashPowerRate()
                * ptype.effortMax()
                * ServerParam::i().dashDirRate( dir );
            double back_max_accel = ServerParam::i().minDashPower()
                * ptype.dashPowerRate()
                * ptype.effortMax()
                * ServerParam::i().dashDirRate( AngleDeg::normalize_angle( dir + 180.0 ) );
            polygon.push_back( inertia_pos + Vector2D::from_polar( std::max( front_max_accel,
                                                                             std::fabs( back_max_accel ) ),
                                                                   wm.self().body() + dir ) );
        }

        for ( size_t i = 1; i < polygon.size(); ++i )
        {
            dlog.addLine( Logger::ANALYZER, polygon[i-1], polygon[i], "#FFF" );
        }
        dlog.addLine( Logger::ANALYZER, polygon.back(), polygon.front(), "#FFF" );

        polygon.clear();
    }

    // 2 or more dashes (no omnidir dash)
    for ( int i = 2; i < 20; ++i )
    {
        const Vector2D inertia_pos = wm.self().inertiaPoint( i );

        for ( int a = 0; a < 360; a += dir_step )
        {
            AngleDeg angle = wm.self().body() + a;
            int n_turn = 0;
            {
                double speed = first_speed;
                double dir_diff = AngleDeg( a ).abs();

                if ( dir_diff > 100.0 )
                {
                    dir_diff = 180.0 - dir_diff; // back dash
                }

                while ( dir_diff > 0.0 )
                {
                    dir_diff -= ptype.effectiveTurn( 180.0, speed );
                    speed *= ptype.playerDecay();
                    ++n_turn;
                }
            }

            int n_dash = i - n_turn;
            if ( n_dash <= 0 )
            {
                polygon.push_back( inertia_pos );
            }
            else
            {
                polygon.push_back( inertia_pos + Vector2D::from_polar( ptype.dashDistanceTable()[n_dash-1], angle ) );
            }
        }

        char col[8];
        snprintf( col, 8, "#%02x%02x%02x", 255-i*12, 255-i*12, 255-i*12 );
        for ( size_t j = 1; j < polygon.size(); ++j )
        {
            dlog.addLine( Logger::ANALYZER, polygon[j-1], polygon[j], col );
        }
        dlog.addLine( Logger::ANALYZER, polygon.back(), polygon.front(), col );

        char id[4];
        snprintf( id, 4, "%d", i );
        dlog.addMessage( Logger::ANALYZER, polygon[0], id );
        dlog.addMessage( Logger::ANALYZER, polygon[polygon.size()/4], id );
        dlog.addMessage( Logger::ANALYZER, polygon[polygon.size()*2/4], id );
        dlog.addMessage( Logger::ANALYZER, polygon[polygon.size()*3/4], id );

        polygon.clear();
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::debugPrintShootPositions()
{
    double min_value = 1000000.0;
    double max_value = 0.0;
    for ( int ix = 0; ix < SHOOT_AREA_X_DIVS; ++ix )
    {
        for ( int iy = 0; iy < SHOOT_AREA_Y_DIVS; ++iy )
        {
            min_value = std::min( M_shoot_point_values[ix][iy], min_value );
            max_value = std::max( M_shoot_point_values[ix][iy], max_value );
        }
    }

    double range = max_value - min_value;
    if ( std::fabs( range ) < 1.0e-10 ) range = 1.0;

    ThermoColorProvider thermo;
    for ( int ix = 0; ix < SHOOT_AREA_X_DIVS; ++ix )
    {
        const double x = index_to_x_coordinate( ix );
        for ( int iy = 0; iy < SHOOT_AREA_Y_DIVS; ++iy )
        {
            const double y = index_to_y_coordinate( iy );
            RGBColor col = thermo.convertToColor( ( M_shoot_point_values[ix][iy] - min_value ) / range );
            dlog.addRect( Logger::SHOOT,
                          x - 0.1, y - 0.1, 0.2, 0.2, col.name().c_str(), true );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::debugPrintTargetVoronoiDiagram()
{
    for ( VoronoiDiagram::Segment2DCont::const_iterator s = M_target_voronoi_diagram.resultSegments().begin(),
              end = M_target_voronoi_diagram.resultSegments().end();
          s != end;
          ++s )
    {
        dlog.addLine( Logger::PASS | Logger::DRIBBLE,
                      s->origin(), s->terminal(),
                      "#00F" );
    }

    for ( VoronoiDiagram::Ray2DCont::const_iterator r = M_target_voronoi_diagram.resultRays().begin(),
              end = M_target_voronoi_diagram.resultRays().end();
          r != end;
          ++r )
    {
        dlog.addLine( Logger::PASS | Logger::DRIBBLE,
                      r->origin(), r->origin() + Vector2D::polar2vector( 20.0, r->dir() ),
                      "#00F" );
    }

    for ( DelaunayTriangulation::VertexCont::const_iterator v = M_target_voronoi_diagram.triangulation().vertices().begin(),
              end = M_target_voronoi_diagram.triangulation().vertices().end();
          v != end;
          ++v )
    {
        dlog.addCircle( Logger::PASS | Logger::DRIBBLE,
                        v->pos(), 0.2, "#0FF", true );
    }

    for ( std::vector< Vector2D >::const_iterator p = M_voronoi_target_points.begin(),
              end = M_voronoi_target_points.end();
          p != end;
          ++p )
    {
        dlog.addRect( Logger::PASS | Logger::DRIBBLE,
                      p->x - 0.15,  p->y - 0.15, 0.3, 0.3,
                      "#F0F" );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldAnalyzer::debugPrintPositioningVoronoiDiagram()
{
    for ( VoronoiDiagram::Segment2DCont::const_iterator s = M_positioning_voronoi_diagram.resultSegments().begin(),
              end = M_positioning_voronoi_diagram.resultSegments().end();
          s != end;
          ++s )
    {
        dlog.addLine( Logger::POSITIONING,
                      s->origin(), s->terminal(),
                      "#ff0" );
    }

    for ( VoronoiDiagram::Ray2DCont::const_iterator r = M_positioning_voronoi_diagram.resultRays().begin(),
              end = M_positioning_voronoi_diagram.resultRays().end();
          r != end;
          ++r )
    {
        dlog.addLine( Logger::POSITIONING,
                      r->origin(), r->origin() + Vector2D::polar2vector( 20.0, r->dir() ),
                      "#ff0" );
    }

    //
    // our players graph
    //
    dlog.addText( Logger::POSITIONING,
                  __FILE__": our players graph, node=%d connection=%d",
                  M_our_players_graph.nodes().size(),
                  M_our_players_graph.connections().size() );
    for ( std::vector< PlayerGraph::Connection >::const_iterator e = M_our_players_graph.connections().begin(),
              end = M_our_players_graph.connections().end();
          e != end;
          ++e )
    {
        dlog.addLine( Logger::POSITIONING,
                      e->first_->pos(), e->second_->pos(),
                      "#ff00ff" );
    }
}
