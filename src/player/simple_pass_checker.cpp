// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA, Hidehisa AKIYAMA

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

#include "simple_pass_checker.h"

#include "predict_state.h"

#include <rcsc/player/world_model.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

using namespace rcsc;

static const double PASS_ANGLE_THRESHOLD = 17.5; // 14.5
static const double BACK_PASS_ANGLE_THRESHOLD = 17.5;
static const double CHANCE_PASS_ANGLE_THRESHOLD = 14.5;
static const double GOALIE_PASS_ANGLE_THRESHOLD = 17.5;
static const double PASS_RECEIVER_PREDICT_STEP = 2.5;

static const double PASS_REFERENCE_SPEED = 3.0;

static const double PASS_SPEED_THRESHOLD = 1.5;

static const double NEAR_PASS_DIST_THR = 4.0;
static const double FAR_PASS_DIST_THR = 35.0;

static const long VALID_TEAMMATE_ACCURACY = 8;
static const long VALID_OPPONENT_ACCURACY = 20;
// static const double OPPONENT_DIST_THR2 = std::pow( 5.0, 2 );

// #ifdef DEBUG_PRINT
// #undef DEBUG_PRINT
// #endif
// #define DEBUG_PRINT


namespace {

/*-------------------------------------------------------------------*/
/*!

 */
double
get_angle_threshold( const AbstractPlayerObject * from,
                     const AbstractPlayerObject * to,
                     const double first_ball_speed )

{
    double thr = PASS_ANGLE_THRESHOLD;

    if ( to->pos().x - 2.0 <= from->pos().x )
    {
        thr = BACK_PASS_ANGLE_THRESHOLD;
    }

    if ( from->pos().x >= +25.0
         && to->pos().x >= +25.0 )
    {
        thr = CHANCE_PASS_ANGLE_THRESHOLD;
    }

    if ( from->goalie() )
    {
        thr = GOALIE_PASS_ANGLE_THRESHOLD;
    }


    // adjust angle threshold by ball speed
    thr *= ( PASS_REFERENCE_SPEED / first_ball_speed );

    return thr;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
double
SimplePassChecker::check( const PredictState & state,
                          const AbstractPlayerObject * from,
                          const AbstractPlayerObject * to,
                          const Vector2D & receive_point,
                          const double & first_ball_speed,
                          const int ball_step )
{
    // NULL passer or NULL receiver%
    if ( ! from || ! to )
    {
        return 0.0;
    }

    //
    // inhibit self pass
    //
    if ( from->unum() == to->unum() )
    {
        return 0.0;
    }

    if ( first_ball_speed < PASS_SPEED_THRESHOLD )
    {
        return 0.0;
    }

    if ( from->isGhost()
         || to->isGhost()
         || from->posCount() > VALID_TEAMMATE_ACCURACY
         || to->posCount() > VALID_TEAMMATE_ACCURACY )
    {
        return 0.0;
    }

    const Vector2D from_pos = ( from->isSelf()
                                ? state.ball().pos()
                                : from->pos() );
    const double pass_dist = from_pos.dist( receive_point );


#ifdef DEBUG_PRINT
    bool debug = ( from->unum() == 9
                   && to->unum() == 11
                   && ( from_pos.dist2( Vector2D( 50.0, -18.9 ) ) < 0.5 )
                   //&& ( receive_point.dist2( Vector2D( 37.7, 12.9 ) ) < 0.5 )
                   );
#endif

    if ( pass_dist <= NEAR_PASS_DIST_THR )
    {
#ifdef DEBUG_PRINT
        if ( debug )
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) too near. dist=%.2f",
                      from->unum(), to->unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y,
                      pass_dist );
#endif
        return 0.0;
    }

    if ( pass_dist >= FAR_PASS_DIST_THR )
    {
#ifdef DEBUG_PRINT
        if ( debug )
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) too far. dist=%.2f",
                      from->unum(), to->unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y,
                      pass_dist );
#endif
        return 0.0;
    }

    if ( to->pos().x >= state.offsideLineX() )
    {
#ifdef DEBUG_PRINT
        if ( debug )
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) offsideX=%.2f",
                      from->unum(), to->unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y,
                      state.offsideLineX() );
#endif
        return 0.0;
    }

    if ( receive_point.x <= ServerParam::i().ourPenaltyAreaLineX() + 3.0
         && receive_point.absY() <= ServerParam::i().penaltyAreaHalfWidth() + 3.0 )
    {
#ifdef DEBUG_PRINT
        if ( debug )
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) in penalty area",
                      from->unum(), to->unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y );
#endif
        return 0.0;
    }

    if ( receive_point.absX() >= ServerParam::i().pitchHalfLength()
         || receive_point.absY() >= ServerParam::i().pitchHalfWidth() )
    {
#ifdef DEBUG_PRINT
        if ( debug )
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) out of field",
                      from->unum(), to->unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y );
#endif
        return 0.0;
    }

    if ( to->goalie()
         && receive_point.x < ServerParam::i().ourPenaltyAreaLineX() + 1.0
         && receive_point.absY() < ServerParam::i().penaltyAreaHalfWidth() + 1.0 )
    {
#ifdef DEBUG_PRINT
        if ( debug )
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) our goalie can catch",
                      from->unum(), to->unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y );
#endif
        return 0.0;
    }

    if ( to->goalie()
         && receive_point.x > state.ourDefenseLineX() - 3.0 )
    {
#ifdef DEBUG_PRINT
        if ( debug )
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) our goalie over defense line",
                      from->unum(), to->unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y );
#endif
        return 0.0;
    }

    const double angle_threshold = get_angle_threshold( from, to, first_ball_speed );

    const AngleDeg pass_angle = ( receive_point - from_pos ).th();

#ifdef DEBUG_PRINT
    double debug_min_angle = +360.0;

    if ( debug )
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) pass_angle=%.1f",
                      from->unum(), to->unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y,
                      pass_angle.degree() );
#endif

    for ( PredictPlayerObject::Cont::const_iterator o = state.theirPlayers().begin(),
              end = state.theirPlayers().end();
          o != end;
          ++o )
    {
        if ( (*o)->posCount() > VALID_OPPONENT_ACCURACY )
        {
            continue;
        }

//         if ( (*o)->pos().dist2( receive_point ) < OPPONENT_DIST_THR2 )
//         {
// #ifdef DEBUG_PRINT
//             if ( debug )
//                 dlog.addText( Logger::PASS,
//                               "xx %d to %d: (%.2f %.2f)->(%.2f %.2f) near opponent %d",
//                               from->unum(), to->unum(),
//                               from_pos.x, from_pos.y,
//                               receive_point.x, receive_point.y,
//                               (*o)->unum() );
// #endif
//             return 0.0;
//         }

        Vector2D opp_pos = (*o)->inertiaFinalPoint();

        const double opp_move_dist = opp_pos.dist( receive_point );

        // if ( opp_move_dist > receiver_move_dist )
        // {
        //     continue;
        // }

        const int opp_step = (*o)->playerTypePtr()->cyclesToReachDistance( opp_move_dist ) + 2;
        if ( opp_step < ball_step )
        {
#ifdef DEBUG_PRINT
            if ( debug )
                dlog.addText( Logger::PASS,
                              "xx %d to %d: (%.2f %.2f)->(%.2f %.2f) bstep=%d > opp_step(%d)=%d",
                              from->unum(), to->unum(),
                              from_pos.x, from_pos.y,
                              receive_point.x, receive_point.y,
                              ball_step, (*o)->unum(), opp_step );
#endif
            return 0.0;
        }

        if ( opp_pos.dist( from_pos ) > pass_dist + 1.0 )
        {
            continue;
        }

        double angle_diff = ( ( opp_pos - from_pos ).th() - pass_angle ).abs();

#ifdef DEBUG_PRINT
        if ( debug )
            dlog.addText( Logger::PASS,
                          "-- %d to %d: (%.2f %.2f)->(%.2f %.2f) opp=%d angle_diff=%.1f",
                          from->unum(), to->unum(),
                          from_pos.x, from_pos.y,
                          receive_point.x, receive_point.y,
                          (*o)->unum(),
                          angle_diff );
#endif

        if ( from->isSelf() )
        {
            const double control_area = (*o)->playerTypePtr()->kickableArea();
            const double hide_radian = std::asin( std::min( control_area / from_pos.dist( opp_pos ),
                                                            1.0 ) );
            angle_diff = std::max( angle_diff - AngleDeg::rad2deg( hide_radian ), 0.0 );
        }

#ifdef DEBUG_PRINT
        if ( debug_min_angle > angle_diff )
        {
            debug_min_angle = angle_diff;
        }
#endif

        if ( angle_diff < angle_threshold )
        {
#ifdef DEBUG_PRINT
            if ( debug )
                dlog.addText( Logger::PASS,
                              "xx %d to %d: (%.2f %.2f)->(%.2f %.2f) too narrow. angleWidth=%.3f",
                              from->unum(), to->unum(),
                              from_pos.x, from_pos.y,
                              receive_point.x, receive_point.y,
                              angle_diff );
#endif
            return 0.0;
        }
    }

#ifdef DEBUG_PRINT
    dlog.addText( Logger::PASS,
                  "ok %d to %d: (%.2f %.2f)->(%.2f %.2f) angleWidth=%.3f",
                  from->unum(), to->unum(),
                  from_pos.x, from_pos.y,
                  receive_point.x, receive_point.y,
                  debug_min_angle );
#endif
    return 1.0;
}
