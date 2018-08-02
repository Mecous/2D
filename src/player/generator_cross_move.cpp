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

#include "generator_cross_move.h"

#include "field_analyzer.h"
#include "shoot_simulator.h"
#include "strategy.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/color/thermo_color_provider.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/timer.h>

// #define DEBUG_PROFILE
// #define DEBUG_PRINT
// #define DEBUG_PRINT_EVALUATE
// #define DEBUG_PAINT_VALUE

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorCrossMove::GeneratorCrossMove()
    : M_update_time( -1, 0 ),
      M_best_point( -1, Vector2D::INVALIDATED ),
      M_previous_time( -1, 0 ),
      M_previous_best_point( -1, Vector2D::INVALIDATED )
{
    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorCrossMove &
GeneratorCrossMove::instance()
{
    static GeneratorCrossMove s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCrossMove::clear()
{
    M_best_point.id_ = -1;
    M_best_point.pos_ = Vector2D::INVALIDATED;
    M_best_point.self_move_step_ = 1000;
    M_best_point.value_ = -100000.0;
    M_target_points.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCrossMove::generate( const WorldModel & wm )
{
    if ( M_update_time == wm.time() )
    {
        return;
    }
    M_update_time = wm.time();

    clear();

    if ( wm.self().isKickable()
         || wm.gameMode().isPenaltyKickMode()
         || wm.gameMode().isTheirSetPlay( wm.ourSide() ) )
    {
        return;
    }

    // check ball owner

    const int s_min = wm.interceptTable()->selfReachCycle();
    const int t_min = wm.interceptTable()->teammateReachCycle();
    const int o_min = wm.interceptTable()->opponentReachCycle();

    if ( ! wm.kickableTeammate()
         && s_min <= t_min )
    {
        return;
    }

    if ( o_min < t_min - 2 )
    {
        return;
    }


#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    createTargetPoints( wm );
    evaluate( wm );

    M_previous_time = wm.time();
    M_previous_best_point = M_best_point;

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::POSITIONING,
                  __FILE__": (generate) PROFILE size=%d elapsed %.3f [ms]",
                  (int)M_target_points.size(), timer.elapsedReal() );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCrossMove::createTargetPoints( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();

    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const Vector2D first_ball_pos = wm.ball().inertiaPoint( teammate_step );

    const double min_x = SP.theirPenaltyAreaLineX() - 2.0;
    const double max_x = std::min( SP.pitchHalfLength() - 2.0,
                                   std::max( wm.offsideLineX(),
                                             first_ball_pos.x ) );

    const double max_y = SP.penaltyAreaHalfWidth();

    //const Segment2D goal_segment( Vector2D( 52.5, -3.0 ), Vector2D( 52.5, 3.0 ) );
    const Segment2D goal_segment( Vector2D( 52.5, -8.0 ), Vector2D( 52.5, 8.0 ) );

    // const Vector2D self_pos = wm.self().inertiaFinalPoint();
    // const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    // Vector2D best_point = Vector2D::INVALIDATED;
    // int best_self_move_step = 1000;
    // int best_ball_move_step = 1000;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::POSITIONING,
                  __FILE__"(createTargetPoints) min_x=%.1f max_x=%.1f max_y=%.1f",
                  min_x, max_x, max_y );
#endif

    int count = 0;

    for ( double x = min_x; x < max_x; x += 1.0 )
    {
        for ( double y = 0.0; y < max_y; y += 1.0 )
        {
            Vector2D point( x, +y );

            if ( goal_segment.dist( point ) > 16.0 )
            {
                continue;
            }

            if ( ! existOtherReceiver( wm, point ) )
            {
// #ifdef DEBUG_PRINT
//                 dlog.addText( Logger::POSITIONING,
//                               "add target (%.1f %.1f)", point.x, point.y );
// #endif
                M_target_points.push_back( TargetPoint( ++count, point ) );
            }

            point.y = -point.y;
            if ( ! existOtherReceiver( wm, point ) )
            {
// #ifdef DEBUG_PRINT
//                 dlog.addText( Logger::POSITIONING,
//                               "add target (%.1f %.1f)", point.x, point.y );
// #endif
                M_target_points.push_back( TargetPoint( ++count, point ) );
            }
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
GeneratorCrossMove::existOtherReceiver( const WorldModel & wm,
                                        const Vector2D & pos )
{
    const double my_dist = wm.self().pos().dist( pos );

    for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromBall().begin(),
              end = wm.teammatesFromBall().end();
          t != end;
          ++t )
    {
        if ( (*t)->goalie() ) continue;
        if ( (*t)->posCount() > 20 ) continue;

        double d
            = (*t)->pos().dist( pos )
            - ( ( (*t)->playerTypePtr()->realSpeedMax() * 0.5 ) * (*t)->posCount() );

        if ( d < my_dist )
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
GeneratorCrossMove::existPassCourse( const WorldModel & wm,
                                     const Vector2D & first_ball_pos,
                                     const TargetPoint & target_point )
{
    const ServerParam & SP = ServerParam::i();

    const int ball_move_step = SP.ballMoveStep( SP.ballSpeedMax(), first_ball_pos.dist( target_point.pos_ ) );
    const Vector2D first_ball_vel = ( target_point.pos_ - first_ball_pos ).setLengthVector( SP.ballSpeedMax() );

#ifdef DEBUG_PRINT
    dlog.addText( Logger::POSITIONING,
                  "%d: check pass course. pos=(%.1f %.1f)",
                  target_point.id_, target_point.pos_.x, target_point.pos_.y );
#endif


    Vector2D ball_pos = first_ball_pos;
    Vector2D ball_vel = first_ball_vel;

    for ( int step = 2; step <= ball_move_step; ++step )
    {
        ball_pos += ball_vel;
        ball_vel *= SP.ballDecay();

        for ( PlayerObject::Cont::const_iterator o = wm.opponents().begin(), end = wm.opponents().end();
              o != end;
              ++o )
        {
            const PlayerType * ptype = (*o)->playerTypePtr();

            const double control_area = ( (*o)->goalie()
                                          && ball_pos.x > SP.theirPenaltyAreaLineX()
                                          && ball_pos.absY() < SP.penaltyAreaHalfWidth()
                                          ? ptype->reliableCatchableDist() + 0.1
                                          : ptype->kickableArea() + 0.1 );
            double move_dist = (*o)->pos().dist( ball_pos );
            move_dist -= control_area;

            int opponent_turn = 1;
            int opponent_dash = ptype->cyclesToReachDistance( move_dist );

            if ( 1 + opponent_turn + opponent_dash <= step ) // plus 1 for the observation delay
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::POSITIONING,
                              "%d: __ opponent %d (%.1f %.1f)",
                              target_point.id_,
                              (*o)->unum(), (*o)->pos().x, (*o)->pos().y );
#endif
                return false;
            }
        }
    }

    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCrossMove::evaluate( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();

    const double opp_dist_factor = 1.0 / ( 2.0 * std::pow( 3.0, 2 ) );
    //const double home_dist_factor = 1.0 / ( 2.0 * std::pow( 20.0, 2 ) );
    const double home_dist_factor = 1.0 / ( 2.0 * std::pow( 5.0, 2 ) );
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const Vector2D first_ball_pos = wm.ball().inertiaPoint( teammate_step );

    AbstractPlayerObject::Cont opponents;
    for ( AbstractPlayerObject::Cont::const_iterator o = wm.theirPlayers().begin(), end = wm.theirPlayers().end();
          o != end;
          ++o )
    {
        if ( (*o)->pos().dist2( SP.theirTeamGoalPos() ) > std::pow( 20.0, 2 ) ) continue;

        opponents.push_back( *o );
    }

#ifdef DEBUG_PRINT_EVALUATE
    dlog.addText( Logger::POSITIONING,
                  "(GeneratorCrossMove::evaluate)" );
#endif

    const TargetPoint * best = static_cast< const TargetPoint * >( 0 );
    double min_value = +100000.0;
    double max_value = -100000.0;
    for ( std::vector< TargetPoint >::iterator it = M_target_points.begin(), end = M_target_points.end();
          it != end;
          ++it )
    {
        it->value_ = 100.0;

        if ( ! existPassCourse( wm, first_ball_pos, *it ) )
        {
            it->value_ *= 0.5;
        }

        if ( ! ShootSimulator::can_shoot_from( true, it->pos_, opponents, 5 ) )
        {
            it->value_ *= 0.8;
#ifdef DEBUG_PRINT_EVALUATE
            dlog.addText( Logger::POSITIONING,
                          "%d: cannot shoot rate 0.8 -> %f", it->id_, it->value_ );
#endif
        }

        if ( it->pos_.absY() > SP.goalHalfWidth() )
        {
            double over_y = ( it->pos_.absY() - SP.goalHalfWidth() );
            it->value_ -= over_y;
#ifdef DEBUG_PRINT_EVALUATE
            dlog.addText( Logger::POSITIONING,
                          "%d: over y %.3f -> %f", it->id_, over_y, it->value_ );
#endif
        }

        {
            double opponent_dist = wm.getDistOpponentNearestTo( it->pos_, 5 );
            double rate = 1.0 - 0.5*std::exp( -std::pow( opponent_dist, 2 ) * opp_dist_factor );

            it->value_ *= rate;
#ifdef DEBUG_PRINT_EVALUATE
            dlog.addText( Logger::POSITIONING,
                          "%d: opponent dist rate %.3f -> %f", it->id_, rate, it->value_ );
#endif
        }

        {
            double home_dist2 = it->pos_.dist2( home_pos );
            it->value_ *= std::exp( -home_dist2 * home_dist_factor );

#ifdef DEBUG_PRINT_EVALUATE
            dlog.addText( Logger::POSITIONING,
                          "%d: move_dist_rate=%f -> %f",
                          it->id_, std::exp( -home_dist2 * home_dist_factor ), it->value_ );
#endif
        }

#ifdef DEBUG_PRINT_EVALUATE
        dlog.addText( Logger::POSITIONING,
                      "%d: pos=(%.2f %.2f) move_step=%d value=%f",
                      it->id_, it->pos_.x, it->pos_.y, it->self_move_step_, it->value_ );
#endif
        if ( it->value_ > max_value )
        {
            max_value = it->value_;
            best = &(*it);
        }
        else if ( it->value_ < min_value )
        {
            min_value = it->value_;
        }
    }

    if ( best )
    {
        M_best_point = *best;
#ifdef DEBUG_PRINT_EVALUATE
        dlog.addText( Logger::POSITIONING,
                      ">>>> best point %d: pos=(%.2f %.2f) move_step=%d value=%f",
                      best->id_, best->pos_.x, best->pos_.y, best->self_move_step_, best->value_ );
#endif
    }

#ifdef DEBUG_PAINT_VALUE
    ThermoColorProvider color;
    for ( std::vector< TargetPoint >::iterator it = M_target_points.begin(), end = M_target_points.end();
          it != end;
          ++it )
    {
        char msg[16]; snprintf( msg, 16, "%d:%.3f", it->id_, it->value_ );
        RGBColor c = color.convertToColor( ( it->value_ - min_value ) / ( max_value - min_value ) );
        std::string name = c.name();

        dlog.addRect( Logger::POSITIONING,
                      it->pos_.x - 0.05, it->pos_.y - 0.05, 0.1, 0.1, name.c_str(), true );
        dlog.addMessage( Logger::POSITIONING,
                         it->pos_.x + 0.1, it->pos_.y, msg );
    }

    if ( best )
    {
        dlog.addRect( Logger::POSITIONING,
                      best->pos_.x - 0.2, best->pos_.y - 0.2, 0.4, 0.4, "#F00" );
    }
#endif

}
