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

#include "shoot_simulator.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/logger.h>
#include <rcsc/timer.h>
#include <rcsc/math_util.h>

#include <algorithm>
#include <cmath>

// #define DEBUG_PROFILE
// #define DEBUG_PRINT
// #define DEBUG_CAN_SHOOT_FROM


using namespace rcsc;

#if 0
/*-------------------------------------------------------------------*/
/*!

 */
int
ShootSimulator::simulateOurShootToNearSide( const Vector2D & ball_pos )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
int
ShootSimulator::simulateOurShootToFarSide( const Vector2D & ball_pos )
{

    return 0;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
ShootSimulator::simulateTheirShootToNearSide( const Vector2D & ball_pos )
{

    return 0;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
ShootSimulator::simulateTheirShootToFarSide( const Vector2D & ball_pos )
{

    return 0;
}
#endif

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
ShootSimulator::get_our_team_near_goal_post_pos( const Vector2D & point )
{
    const ServerParam & SP = ServerParam::i();

    return Vector2D( -SP.pitchHalfLength(),
                     +sign( point.y ) * SP.goalHalfWidth() );
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
ShootSimulator::get_our_team_far_goal_post_pos( const Vector2D & point )
{
    return Vector2D( -ServerParam::i().pitchHalfLength(),
                     -sign( point.y ) * ServerParam::i().goalHalfWidth() );
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
ShootSimulator::get_their_team_near_goal_post_pos( const Vector2D & point )
{
    return Vector2D( +ServerParam::i().pitchHalfLength(),
                     +sign( point.y ) * ServerParam::i().goalHalfWidth() );
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
ShootSimulator::get_their_team_far_goal_post_pos( const Vector2D & point )
{
    return Vector2D( +ServerParam::i().pitchHalfLength(),
                     -sign( point.y ) * ServerParam::i().goalHalfWidth() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShootSimulator::get_dist_from_our_goal_post( const Vector2D & point,
                                             double * near_post_dist,
                                             double * far_post_dist )
{
    const ServerParam & SP = ServerParam::i();

    *near_post_dist = point.dist( Vector2D( - SP.pitchHalfLength(),
                                            - SP.goalHalfWidth() ) );
    *far_post_dist = point.dist( Vector2D( - SP.pitchHalfLength(),
                                           + SP.goalHalfWidth() ) );
    if ( *near_post_dist > *far_post_dist )
    {
        std::swap( *near_post_dist, *far_post_dist );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
double
ShootSimulator::get_dist_from_our_near_goal_post( const Vector2D & point )
{
    const ServerParam & SP = ServerParam::i();

    return std::sqrt( std::min( point.dist2( Vector2D( - SP.pitchHalfLength(),
                                                       - SP.goalHalfWidth() ) ),
                                point.dist2( Vector2D( - SP.pitchHalfLength(),
                                                       + SP.goalHalfWidth() ) ) ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
double
ShootSimulator::get_dist_from_our_far_goal_post( const Vector2D & point )
{
    const ServerParam & SP = ServerParam::i();

    return std::sqrt( std::min( point.dist2( Vector2D( - SP.pitchHalfLength(),
                                                       - SP.goalHalfWidth() ) ),
                                point.dist2( Vector2D( - SP.pitchHalfLength(),
                                                       + SP.goalHalfWidth() ) ) ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShootSimulator::get_dist_from_their_goal_post( const Vector2D & point,
                                               double * near_post_dist,
                                               double * far_post_dist )
{
    const ServerParam & SP = ServerParam::i();

    *near_post_dist = point.dist( Vector2D( + SP.pitchHalfLength(),
                                            - SP.goalHalfWidth() ) );
    *far_post_dist = point.dist( Vector2D( + SP.pitchHalfLength(),
                                           + SP.goalHalfWidth() ) );
    if ( *near_post_dist > *far_post_dist )
    {
        std::swap( *near_post_dist, *far_post_dist );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
double
ShootSimulator::get_dist_from_their_near_goal_post( const Vector2D & point )
{
    const ServerParam & SP = ServerParam::i();

    return std::sqrt( std::min( point.dist2( Vector2D( + SP.pitchHalfLength(),
                                                       - SP.goalHalfWidth() ) ),
                                point.dist2( Vector2D( + SP.pitchHalfLength(),
                                                       + SP.goalHalfWidth() ) ) ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
double
ShootSimulator::get_dist_from_their_far_goal_post( const Vector2D & point )
{
    const ServerParam & SP = ServerParam::i();

    return std::sqrt( std::min( point.dist2( Vector2D( + SP.pitchHalfLength(),
                                                       - SP.goalHalfWidth() ) ),
                                point.dist2( Vector2D( + SP.pitchHalfLength(),
                                                       + SP.goalHalfWidth() ) ) ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
ShootSimulator::is_ball_moving_to_their_goal( const Vector2D & ball_pos,
                                              const Vector2D & ball_vel,
                                              const double post_buffer )
{
    const Vector2D goal_post_plus( ServerParam::i().pitchHalfLength(),
                                   +ServerParam::i().pitchHalfWidth() + post_buffer );
    const Vector2D goal_post_minus( ServerParam::i().pitchHalfLength(),
                                    -ServerParam::i().pitchHalfWidth() - post_buffer );
    const AngleDeg ball_angle = ball_vel.th();

    return ( goal_post_plus - ball_pos ).th().isRightOf( ball_angle )
        && ( goal_post_minus - ball_pos ).th().isLeftOf( ball_angle );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
ShootSimulator::is_ball_moving_to_our_goal( const Vector2D & ball_pos,
                                            const Vector2D & ball_vel,
                                            const double post_buffer )
{
    const double goal_half_width = ServerParam::i().goalHalfWidth();
    const double goal_line_x = ServerParam::i().ourTeamGoalLineX();
    const Vector2D goal_plus_post( goal_line_x,
                                   +goal_half_width + post_buffer );
    const Vector2D goal_minus_post( goal_line_x,
                                    -goal_half_width - post_buffer );
    const AngleDeg ball_angle = ball_vel.th();

    return ( ( ( goal_plus_post - ball_pos ).th() - ball_angle ).degree() < 0
             && ( ( goal_minus_post - ball_pos ).th() - ball_angle ).degree() > 0 );
}


/*-------------------------------------------------------------------*/
/*!

 */
namespace {

struct Player {
    const AbstractPlayerObject * player_;
    AngleDeg angle_from_pos_;
    double dist_from_pos_;
    double hide_angle_;

    Player( const AbstractPlayerObject * player,
            const Vector2D & pos )
        : player_( player ),
          angle_from_pos_(),
          dist_from_pos_( 0.0 ),
          hide_angle_( 0.0 )
      {
          Vector2D inertia_pos = player->inertiaFinalPoint();
          double control_dist = ( player->goalie()
                                  ? ServerParam::i().catchAreaLength() * 0.75
                                  : player->playerTypePtr()->kickableArea() * 0.75 );
          double hide_angle_radian = std::asin( std::min( control_dist / inertia_pos.dist( pos ),
                                                          1.0 ) );

          angle_from_pos_ = ( inertia_pos - pos ).th();
          dist_from_pos_ =  pos.dist( inertia_pos );
          hide_angle_ = hide_angle_radian * AngleDeg::RAD2DEG;
      }

    struct Compare {
        bool operator()( const Player & lhs,
                         const Player & rhs ) const
          {
              return lhs.angle_from_pos_.degree() < rhs.angle_from_pos_.degree();
          }
    };
};

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
ShootSimulator::can_shoot_from( const bool is_self,
                                const Vector2D & pos,
                                const AbstractPlayerObject::Cont & opponents,
                                const int valid_opponent_threshold )
{
    static const double SHOOT_DIST_THR2 = std::pow( 17.0, 2 );
    //static const double SHOOT_ANGLE_THRESHOLD = 20.0;
    static const double SHOOT_ANGLE_THRESHOLD = ( is_self
                                                  ? 20.0
                                                  : 15.0 );
    static const double OPPONENT_DIST_THR2 = std::pow( 30.0, 2 );

    static const Vector2D goal_pos = ServerParam::i().theirTeamGoalPos();
    static const Vector2D goal_minus( ServerParam::i().pitchHalfLength(),
                                      -ServerParam::i().goalHalfWidth() + 0.5 );
    static const Vector2D goal_plus( ServerParam::i().pitchHalfLength(),
                                     +ServerParam::i().goalHalfWidth() - 0.5 );
    static const double shoot_course_radius2 = std::pow( ServerParam::i().goalHalfWidth() + 1.0, 2 );

    static const Sector2D shootable_sector( Vector2D( 58.0, 0.0 ),
                                            0.0, 20.0,
                                            137.5, -137.5 );

    if ( goal_pos.dist2( pos ) > SHOOT_DIST_THR2 )
    {
        return false;
    }

#ifdef DEBUG_CAN_SHOOT_FROM
    dlog.addText( Logger::SHOOT,
                  "===== "__FILE__": (can_shoot_from) pos=(%.1f %.1f) ===== ",
                  pos.x, pos.y );
#endif


    const AngleDeg goal_minus_angle = ( goal_minus - pos ).th();
    const AngleDeg goal_plus_angle = ( goal_plus - pos ).th();

    const double angle_width = ( goal_plus_angle - goal_minus_angle ).abs();

    if ( angle_width < 15.0 )
    {
        return false;
    }

    const Triangle2D shoot_course_triangle( pos, goal_minus, goal_plus );

    //
    // create opponent list
    //

    std::vector< Player > opponent_candidates;
    opponent_candidates.reserve( opponents.size() );

    int opponent_count = 100;
    if ( shootable_sector.contains( pos ) )
    {
        opponent_count = 0;
    }

    for ( AbstractPlayerObject::Cont::const_iterator o = opponents.begin(),
              end = opponents.end();
          o != end;
          ++o )
    {
        if ( (*o)->posCount() > valid_opponent_threshold )
        {
            continue;
        }

        if ( (*o)->pos().dist2( pos ) > OPPONENT_DIST_THR2 )
        {
            continue;
        }

        if ( opponent_count <= 1 )
        {
            if ( (*o)->pos().dist2( goal_pos ) < shoot_course_radius2
                 || shoot_course_triangle.contains( (*o)->pos() ) )
            {
                ++opponent_count;
            }
        }

        opponent_candidates.push_back( Player( *o, pos ) );
#ifdef DEBUG_CAN_SHOOT_FROM
        dlog.addText( Logger::SHOOT,
                      "(can_shoot_from) (opponent:%d) pos=(%.1f %.1f) angleFromPos=%.1f hideAngle=%.1f",
                      opponent_candidates.back().player_->unum(),
                      opponent_candidates.back().player_->pos().x,
                      opponent_candidates.back().player_->pos().y,
                      opponent_candidates.back().angle_from_pos_.degree(),
                      opponent_candidates.back().hide_angle_ );
#endif
    }

    if ( opponent_count <= 1 )
    {
        return true;
    }

    //
    // TODO: improve the search algorithm (e.g. consider only angle width between opponents)
    //
    // std::sort( opponent_candidates.begin(), opponent_candidates.end(),
    //            Opponent::Compare() );

    const double angle_step = angle_width / 10.0;
    const Vector2D add_vec = ( goal_plus - goal_minus ) / 10.0;

    double max_angle_diff = -1.0;

    for ( int i = 0; i < 11; ++i )
    {
        const Vector2D shoot_target = goal_minus + ( add_vec * i );
        const AngleDeg shoot_angle = goal_minus_angle + ( angle_step * i );

        const double shoot_length_thr = pos.dist( shoot_target ) * 1.5;

        double min_angle_diff = 180.0;
        for ( std::vector< Player >::const_iterator o = opponent_candidates.begin(),
                  end = opponent_candidates.end();
              o != end;
              ++o )
        {
            if ( o->dist_from_pos_ > shoot_length_thr )
            {
                continue;
            }

            double angle_diff = ( o->angle_from_pos_ - shoot_angle ).abs();

#ifdef DEBUG_CAN_SHOOT_FROM
            dlog.addText( Logger::SHOOT,
                          "(can_shoot_from) __ opp=%d rawAngleDiff=%.1f -> %.1f",
                          o->player_->unum(),
                          angle_diff, angle_diff - o->hide_angle_*0.5 );
#endif
            if ( is_self )
            {
                angle_diff -= o->hide_angle_;
            }
            else
            {
                angle_diff -= o->hide_angle_*0.5;
            }

            if ( angle_diff < min_angle_diff )
            {
                min_angle_diff = angle_diff;

                if ( min_angle_diff < SHOOT_ANGLE_THRESHOLD )
                {
                    break;
                }
            }
        }

        if ( min_angle_diff > max_angle_diff )
        {
            max_angle_diff = min_angle_diff;
        }

#ifdef DEBUG_CAN_SHOOT_FROM
        dlog.addText( Logger::SHOOT,
                      "(can_shoot_from) shootAngle=%.1f minAngleDiff=%.1f",
                      shoot_angle.degree(),
                      min_angle_diff );
#endif
    }

#ifdef DEBUG_CAN_SHOOT_FROM
    dlog.addText( Logger::SHOOT,
                  "(can_shoot_from) maxAngleDiff=%.1f",
                  max_angle_diff );
#endif

    return ( max_angle_diff > 0.0
             && max_angle_diff >= SHOOT_ANGLE_THRESHOLD );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
ShootSimulator::opponent_can_shoot_from( const Vector2D & pos,
                                         const AbstractPlayerObject::Cont & teammates,
                                         const int valid_teammate_threshold,
                                         const double shoot_dist_threshold,
                                         const double shoot_angle_threshold,
                                         const double teammate_dist_threshold,
                                         double * max_angle_diff_result,
                                         const bool calculate_detail )
{
    const double DEFAULT_SHOOT_DIST_THR = 40.0;
    const double DEFAULT_SHOOT_ANGLE_THR = 12.0;
    const double DEFAULT_TEAMMATE_DIST_THR2 = std::pow( 40.0, 2 );

    const double SHOOT_DIST_THR = ( shoot_dist_threshold > 0.0
                                    ? shoot_dist_threshold
                                    : DEFAULT_SHOOT_DIST_THR );
    const double SHOOT_ANGLE_THR = ( shoot_angle_threshold > 0.0
                                     ? shoot_angle_threshold
                                     : DEFAULT_SHOOT_ANGLE_THR );
    const double TEAMMATE_DIST_THR2 = ( teammate_dist_threshold > 0.0
                                        ? std::pow( teammate_dist_threshold, 2 )
                                        : DEFAULT_TEAMMATE_DIST_THR2 );

#ifdef DEBUG_CAN_SHOOT_FROM
    dlog.addText( Logger::SHOOT,
                  "===== "__FILE__": (opponent_can_shoot_from) from pos=(%.1f %.1f), n_teammates = %u ===== ",
                  pos.x, pos.y, static_cast< unsigned int >( teammates.size() ) );

    dlog.addText( Logger::SHOOT,
                  "(opponent_can_shoot_from) valid_teammate_threshold = %d",
                  valid_teammate_threshold );
    dlog.addText( Logger::SHOOT,
                  "(opponent_can_shoot_from) shoot_angle_threshold = %.2f",
                  SHOOT_ANGLE_THR );
    dlog.addText( Logger::SHOOT,
                  "(opponent_can_shoot_from) shoot_dist_threshold = %.2f",
                  SHOOT_DIST_THR );
    dlog.addText( Logger::SHOOT,
                  "(opponent_can_shoot_from) teammate_dist_threshold^2 = %.2f",
                  TEAMMATE_DIST_THR2 );
#endif

    if ( get_dist_from_our_near_goal_post( pos ) > SHOOT_DIST_THR )
    {
        if ( max_angle_diff_result )
        {
            *max_angle_diff_result = 0.0;
        }

        return false;
    }

    //
    // create teammate list
    //
    std::vector< Player > teammate_candidates;
    teammate_candidates.reserve( teammates.size() );

    for ( AbstractPlayerObject::Cont::const_iterator t = teammates.begin(),
              end = teammates.end();
          t != end;
          ++t )
    {
        if ( (*t)->posCount() > valid_teammate_threshold )
        {
#ifdef DEBUG_CAN_SHOOT_FROM
            dlog.addText( Logger::SHOOT,
                          "(opponent_can_shoot_from) skip teammate %d, too big pos count, pos count = %d",
                          (*t)->unum(), (*t)->posCount() );
#endif
            continue;
        }

        if ( (*t)->pos().dist2( pos ) > TEAMMATE_DIST_THR2 )
        {
#ifdef DEBUG_CAN_SHOOT_FROM
            dlog.addText( Logger::SHOOT,
                          "(opponent_can_shoot_from) skip teammate %d, too far from point, dist^2 = %f, pos = (%.2f, %.2f), teammate pos = (%.2f, %.2f)",
                          (*t)->unum(), (*t)->pos().dist2( pos ),
                          pos.x, pos.y, (*t) ->pos().x, (*t) ->pos().y );
#endif
            continue;
        }

        teammate_candidates.push_back( Player( *t, pos ) );
#ifdef DEBUG_CAN_SHOOT_FROM
        dlog.addText( Logger::SHOOT,
                      "(opponent_can_shoot_from) (teammate:%d) pos=(%.1f %.1f) angleFromPos=%.1f hideAngle=%.1f",
                      teammate_candidates.back().player_->unum(),
                      teammate_candidates.back().player_->pos().x,
                      teammate_candidates.back().player_->pos().y,
                      teammate_candidates.back().angle_from_pos_.degree(),
                      teammate_candidates.back().hide_angle_ );
#endif
    }

    //
    // TODO: improve the search algorithm (e.g. consider only angle width between opponents)
    //
    // std::sort( opponent_candidates.begin(), opponent_candidates.end(),
    //            Opponent::Compare() );

    const Vector2D goal_minus( -ServerParam::i().pitchHalfLength(),
                               -ServerParam::i().goalHalfWidth() + 0.5 );
    const Vector2D goal_plus( -ServerParam::i().pitchHalfLength(),
                              +ServerParam::i().goalHalfWidth() - 0.5 );

    const AngleDeg goal_minus_angle = ( goal_minus - pos ).th();
    const AngleDeg goal_plus_angle = ( goal_plus - pos ).th();

    const double angle_width = ( goal_plus_angle - goal_minus_angle ).abs();
#ifdef DEBUG_CAN_SHOOT_FROM
    dlog.addText( Logger::SHOOT,
                  "(opponent_can_shoot_from) angle_width = %.2f,"
                  " goal_plus_angle = %.2f, goal_minus_angle = %2f",
                  angle_width, goal_plus_angle.degree(), goal_minus_angle.degree() );
#endif
    const double angle_step = std::max( 2.0, angle_width / 10.0 );

    double max_angle_diff = 0.0;

    const std::vector< Player >::const_iterator begin = teammate_candidates.begin();
    const std::vector< Player >::const_iterator end = teammate_candidates.end();

    for ( double a = 0.0; a < angle_width + 0.001; a += angle_step )
    {
        const AngleDeg shoot_angle = goal_minus_angle - a;
#ifdef DEBUG_CAN_SHOOT_FROM
        dlog.addText( Logger::SHOOT,
                      "(opponent_can_shoot_from) shoot_angle = %.2f",
                      shoot_angle.degree() );
#endif

        double min_angle_diff = 180.0;
        for ( std::vector< Player >::const_iterator t = begin;
              t != end;
              ++t )
        {
            double angle_diff = ( t->angle_from_pos_ - shoot_angle ).abs();

#ifdef DEBUG_CAN_SHOOT_FROM
            dlog.addText( Logger::SHOOT,
                          "(opponent_can_shoot_from)__ teammate=%d rawAngleDiff=%.2f -> %.2f",
                          (*t).player_->unum(),
                          angle_diff, angle_diff - t->hide_angle_ );
#endif

            //angle_diff -= t->hide_angle_;
            angle_diff -= t->hide_angle_*0.5;

            if ( angle_diff < min_angle_diff )
            {
                min_angle_diff = angle_diff;

                if ( min_angle_diff < SHOOT_ANGLE_THR )
                {
                    if ( ! calculate_detail )
                    {
#ifdef DEBUG_CAN_SHOOT_FROM
                        dlog.addText( Logger::SHOOT,
                                      "(opponent_can_shoot_from)__ min_angle_diff < SHOOT_ANGLE_THR: skip other teammates" );
#endif

                        break;
                    }
                }
            }
        }

        if ( min_angle_diff > max_angle_diff )
        {
            max_angle_diff = min_angle_diff;
        }

#ifdef DEBUG_CAN_SHOOT_FROM
        dlog.addText( Logger::SHOOT,
                      "(opponent_can_shoot_from) shootAngle=%.2f minAngleDiff=%.2f",
                      shoot_angle.degree(),
                      min_angle_diff );
#endif
    }

    const bool result = ( max_angle_diff >= SHOOT_ANGLE_THR );
    if ( max_angle_diff_result )
    {
        *max_angle_diff_result = max_angle_diff;
    }

#ifdef DEBUG_CAN_SHOOT_FROM
    dlog.addText( Logger::SHOOT,
                  "(opponent_can_shoot_from) maxAngleDiff=%.2f, result = %s",
                  max_angle_diff, ( result? "true" : "false" ) );
#endif

    return result;
}
