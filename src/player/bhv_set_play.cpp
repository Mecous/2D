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

#include "bhv_set_play.h"

#include "strategy.h"
#include "mark_analyzer.h"

#include "bhv_find_player.h"
#include "bhv_goalie_free_kick.h"
#include "bhv_set_play_free_kick.h"
#include "bhv_set_play_goal_kick.h"
#include "bhv_set_play_goalie_catch_move.h"
#include "bhv_set_play_kick_off.h"
#include "bhv_set_play_kick_in.h"
#include "bhv_set_play_indirect_free_kick.h"
#include "bhv_set_play_our_corner_kick.h"
#include "bhv_their_goal_kick_move.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_before_kick_off.h>
#include <rcsc/action/bhv_scan_field.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/circle_2d.h>
#include <rcsc/geom/line_2d.h>
#include <rcsc/geom/segment_2d.h>

#include <vector>
#include <algorithm>
#include <limits>
#include <cstdio>

// #define DEBUG_PRINT

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlay::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_SetPlay" );

    const WorldModel & wm = agent->world();

    if ( wm.self().goalie() )
    {
        if ( wm.gameMode().type() != rcsc::GameMode::BackPass_
             && wm.gameMode().type() != rcsc::GameMode::IndFreeKick_ )
        {
            Bhv_GoalieFreeKick().execute( agent );
        }
        else
        {
            Bhv_SetPlayIndirectFreeKick().execute( agent );
        }

        return true;
    }

    switch ( wm.gameMode().type() ) {
    case GameMode::KickOff_:
        if ( wm.gameMode().side() == wm.ourSide() )
        {
            return Bhv_SetPlayKickOff().execute( agent );
        }
        else
        {
            doBasicTheirSetPlayMove( agent );
            //doMarkTheirSetPlayMove( agent );
            return true;
        }
        break;
    case GameMode::KickIn_:
        if ( wm.gameMode().side() == wm.ourSide() )
        {
            if ( wm.ball().pos().x > 40.0 )
            {
                Bhv_SetPlayOurCornerKick().execute( agent );
            }
            else
            {
                Bhv_SetPlayKickIn().execute( agent );
            }
            return true;
        }
        else
        {
            if ( wm.ball().pos().x < -25.0 )
            {
                doBasicTheirSetPlayMove( agent );
            }
            else
            {
                doMarkTheirSetPlayMove( agent );
            }
            return true;
        }
        break;
    case GameMode::CornerKick_:
        if ( wm.gameMode().side() == wm.ourSide() )
        {
            Bhv_SetPlayOurCornerKick().execute( agent );
            return true;
        }
        else
        {
            doBasicTheirSetPlayMove( agent );
            //doMarkTheirSetPlayMove( agent );
            return true;
        }
        break;
    case GameMode::GoalKick_:
        if ( wm.gameMode().side() == wm.ourSide() )
        {
            return Bhv_SetPlayGoalKick().execute( agent );
        }
        else
        {
            return Bhv_TheirGoalKickMove().execute( agent );
        }
        break;
    case GameMode::BackPass_:
    case GameMode::IndFreeKick_:
        return Bhv_SetPlayIndirectFreeKick().execute( agent );
        break;
    case GameMode::FoulCharge_:
    case GameMode::FoulPush_:
        if ( wm.ball().pos().x < ServerParam::i().ourPenaltyAreaLineX() + 1.0
             && wm.ball().pos().absY() < ServerParam::i().penaltyAreaHalfWidth() + 1.0 )
        {
            return Bhv_SetPlayIndirectFreeKick().execute( agent );
        }
        else if ( wm.ball().pos().x > ServerParam::i().theirPenaltyAreaLineX() - 1.0
                  && wm.ball().pos().absY() < ServerParam::i().penaltyAreaHalfWidth() + 1.0 )
        {
            return Bhv_SetPlayIndirectFreeKick().execute( agent );
        }
        break;
    case GameMode::GoalieCatch_:
        if ( wm.gameMode().isOurSetPlay( wm.ourSide() ) )
        {
            return Bhv_SetPlayGoalieCatchMove().execute( agent );
        }
#if 0
    case GameMode::FreeKick_:
    case GameMode::Offside_:
    case GameMode::FreeKickFault_:
    case GameMode::CatchFault_:
#endif
    default:
        break;
    }

    if ( wm.gameMode().isOurSetPlay( wm.ourSide() ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": our set play" );
        return Bhv_SetPlayFreeKick().execute( agent );
    }
    else
    {
        //doBasicTheirSetPlayMove( agent );
        doMarkTheirSetPlayMove( agent );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_SetPlay::get_set_play_dash_power( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( ! wm.gameMode().isOurSetPlay( wm.ourSide() ) )
    {
        Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
        if ( target_point.x > wm.self().pos().x )
        {
            if ( wm.ball().pos().x < -30.0
                 && target_point.x < wm.ball().pos().x )
            {
                return wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );
            }

            double rate = 0.0;
            if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.8 )
            {
                rate = 1.5 * wm.self().stamina() / ServerParam::i().staminaMax();
            }
            else
            {
                rate = 0.9
                    * ( wm.self().stamina() - ServerParam::i().recoverDecThrValue() )
                    / ServerParam::i().staminaMax();
                rate = std::max( 0.0, rate );
            }

            return ( wm.self().playerType().staminaIncMax()
                     * wm.self().recovery()
                     * rate );
        }
    }

    return wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );
}

namespace {

double
get_avoid_radius( const WorldModel & wm )
{
    return ServerParam::i().centerCircleR()
        + wm.self().playerType().playerSize()
        + 0.5;
}


bool
can_go_to( const WorldModel & wm,
           const Circle2D & ball_circle,
           const Vector2D & target_point )
{
    const Segment2D move_line( wm.self().pos(), target_point );

    if ( move_line.dist( wm.ball().pos() ) < ball_circle.radius() + 0.01 )
    {
        Vector2D projection = move_line.projection( wm.ball().pos() );
        if ( projection.isValid()
             && move_line.contains( projection ) )
        {
// #ifdef DEBUG_PRINT
//             dlog.addText( Logger::TEAM,
//                           "__ (can_go_to) NG. cross" );
// #endif
            return false;
        }
    }

    return true;
}

Vector2D
get_avoid_circle_point_old( const WorldModel & wm,
                            const Vector2D & target_point )
{
    const int ANGLE_DIVS = 18;

    const ServerParam & SP = ServerParam::i();
    const double avoid_radius = get_avoid_radius( wm );
    const Circle2D ball_circle( wm.ball().pos(), avoid_radius );

#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM,
                  __FILE__": (get_avoid_circle_point) first_target=(%.2f %.2f)",
                  target_point.x, target_point.y );
    dlog.addCircle( Logger::TEAM,
                    wm.ball().pos(), avoid_radius,
                    "#FFF" );
    dlog.addCircle( Logger::TEAM,
                    wm.ball().pos(), SP.centerCircleR() + wm.self().playerType().playerSize() - 0.5,
                    "#999" );
#endif

    if ( ( ( target_point - wm.ball().pos() ).th() - ( wm.ball().angleFromSelf() + 180.0 ) ).abs() < 20.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_avoid_circle_point) ok. first point. small angle difference" );
        return target_point;
    }

    if ( can_go_to( wm, ball_circle, target_point ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_avoid_circle_point) ok, first point. direct" );
        return target_point;
    }

    AngleDeg ball_target_angle = ( target_point - wm.ball().pos() ).th();
// #ifdef DEBUG_PRINT
//     dlog.addText( Logger::TEAM,
//                   __FILE__": (get_avoid_circle_point) ball_target_angle=%.1f",
//                   ball_target_angle.degree() );
// #endif

    Vector2D adjusted_target = target_point;

    double min_dist = 1000000.0;
#ifdef DEBUG_PRINT
    double best_angle = -360.0;
#endif
    for ( int a = 0; a < ANGLE_DIVS; ++a  )
    {
        AngleDeg angle = ball_target_angle + (360.0/ANGLE_DIVS)*a;
        Vector2D tmp_target = wm.ball().pos() + Vector2D::from_polar( avoid_radius + 1.0, angle );
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      "%d: angle=%.1f (%.2f %.2f)",
                      a, angle.degree(), tmp_target.x, tmp_target.y );
#endif
        if ( tmp_target.absX() > SP.pitchHalfLength() + 1.0
             || tmp_target.absY() > SP.pitchHalfWidth() + 1.0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          "%d: XX out of field", a );
#endif
            continue;
        }

        if ( can_go_to( wm, ball_circle, tmp_target ) )
        {
            double d = tmp_target.dist( target_point );
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          "%d: ok. dist=%.2f", a, d );
#endif
            if ( d < min_dist )
            {
                min_dist = d;
                adjusted_target = tmp_target;
#ifdef DEBUG_PRINT
                best_angle = angle.degree();
                dlog.addText( Logger::TEAM,
                              "%d: updated", a );
#endif
            }
        }
#ifdef DEBUG_PRINT
        else
        {
            dlog.addText( Logger::TEAM,
                          "%d: XX intersection", a );
        }
#endif
    }
#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM,
                  __FILE__": (get_avoid_circle_point) best_angle=%.1f pos=(%.2f %.2f)",
                  best_angle, adjusted_target.x, adjusted_target.y );
#endif

    return adjusted_target;
}

Vector2D
get_avoid_circle_point_new( const WorldModel & wm,
                            const Vector2D & target_point )
{
    const ServerParam & SP = ServerParam::i();
    const double avoid_radius = get_avoid_radius( wm );
    const Circle2D ball_circle( wm.ball().pos(), avoid_radius );
    const Segment2D move_segment( wm.self().pos(), target_point );

    dlog.addLine( Logger::TEAM,
                  wm.self().pos(), target_point, "#F00" );
    dlog.addCircle( Logger::TEAM,
                    wm.ball().pos(), avoid_radius, "#0F0" );

    Vector2D first_intersection, second_intersection;
    int n_sol = ball_circle.intersection( move_segment,
                                          &first_intersection,
                                          &second_intersection );
    if ( n_sol == 0 )
    {
        // no avoidance
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_avoid_circle_point_new) no intersection" );
        return target_point;
    }

    if ( n_sol == 1 )
    {
        if ( wm.self().pos().dist( wm.ball().pos() ) > avoid_radius )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (get_avoid_circle_point_new) 1 intersection. target in the circle" );
            return target_point;

        }

        dlog.addText( Logger::TEAM,
                      __FILE__": (get_avoid_circle_point_new) 1 intersection. self in the circle" );

        const AngleDeg angle_diff
            = ( wm.ball().pos() - wm.self().pos() ).th()
            - ( target_point -  wm.self().pos() ).th();
        if ( angle_diff.abs() > 90.0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (get_avoid_circle_point_new) 1 intersection. direct go" );

            return target_point;
        }

        second_intersection = first_intersection;
        first_intersection = wm.self().pos();
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_avoid_circle_point_new) 1 intersection. avoid" );
    }

    if ( wm.self().pos().dist2( first_intersection )
         > wm.self().pos().dist2( second_intersection ) )
    {
        std::swap( first_intersection, second_intersection );
    }

    AngleDeg first_angle = ( first_intersection - wm.ball().pos() ).th();

    // clock-wise route
    std::vector< Vector2D > cw_points;
    for ( double a = 30.0; a < 360.0; a += 30.0 )
    {
        Vector2D sub = wm.ball().pos()
            + Vector2D::from_polar( avoid_radius+0.1, first_angle + a );
        if ( sub.absX() > SP.pitchHalfLength() + SP.pitchMargin() - 1.0
             || sub.absY() > SP.pitchHalfWidth() + SP.pitchMargin() - 1.0 )
        {
            cw_points.clear();
            break;
        }

        cw_points.push_back( sub );

        Segment2D sub_segment( sub, target_point );
        if ( sub_segment.dist( wm.ball().pos() ) > avoid_radius )
        {
            break;
        }
    }

    // counter-clock-wise route
    std::vector< Vector2D > ccw_points;
    for ( double a = -30.0; a > -360.0; a -= 30.0 )
    {
        Vector2D sub = wm.ball().pos()
            + Vector2D::from_polar( avoid_radius+0.1, first_angle + a );
        if ( sub.absX() > SP.pitchHalfLength() + SP.pitchMargin() - 1.0
             || sub.absY() > SP.pitchHalfWidth() + SP.pitchMargin() - 1.0 )
        {
            ccw_points.clear();
            break;
        }

        ccw_points.push_back( sub );

        Segment2D sub_segment( sub, target_point );
        if ( sub_segment.dist( wm.ball().pos() ) > avoid_radius )
        {
            break;
        }
    }

    dlog.addText( Logger::TEAM,
                  __FILE__"(get_avoid_circle_point_new) cw_size=%zd ccw_size=%zd",
                  cw_points.size(), ccw_points.size() );

    if ( cw_points.empty()
         && ccw_points.empty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__"(get_avoid_circle_point_new) no solution" );
        return first_intersection;
    }

    if ( cw_points.empty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__"(get_avoid_circle_point_new) only ccw" );
        return ccw_points.front();
    }

    if ( ccw_points.empty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__"(get_avoid_circle_point_new) only cw" );
        return cw_points.front();
    }

    if ( cw_points.size() < ccw_points.size() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__"(get_avoid_circle_point_new) cw < ccw" );
        return cw_points.front();
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__"(get_avoid_circle_point_new) cw > ccw" );
        return ccw_points.front();
    }
}


} // end noname namespace


Vector2D
Bhv_SetPlay::get_avoid_circle_point( const WorldModel & wm,
                                     const Vector2D & target_point )
{
    return get_avoid_circle_point_new( wm, target_point );
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlay::is_kicker( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.gameMode().type() == GameMode::GoalieCatch_
         && wm.gameMode().side() == wm.ourSide() )
    {
        if ( wm.self().goalie() )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(is_kicker) true. goalie free kick" );
            return true;
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(is_kicker) false. goalie free kick" );
            return false;
        }
    }

    int kicker_unum = Unum_Unknown;
    double min_dist = 1000000.0;
    int second_kicker_unum = Unum_Unknown;
    double second_min_dist = 1000000.0;
    for ( int unum = 1; unum <= 11; ++unum )
    {
        if ( unum == wm.ourGoalieUnum() ) continue;

        Vector2D home_pos = Strategy::i().getPosition( unum );
        if ( ! home_pos.isValid() ) continue;

        double d2 = home_pos.dist2( wm.ball().pos() );
        if ( d2 < second_min_dist )
        {
            second_kicker_unum = unum;
            second_min_dist = d2;

            if ( second_min_dist < min_dist )
            {
                std::swap( second_kicker_unum, kicker_unum );
                std::swap( second_min_dist, min_dist );
            }
        }
    }

    min_dist = std::sqrt( min_dist );
    second_min_dist = std::sqrt( second_min_dist );

    dlog.addText( Logger::TEAM,
                  __FILE__":(is_kicker) kicker_unum=%d second_kicker_unum=%d",
                  kicker_unum, second_kicker_unum );

    const AbstractPlayerObject * kicker = static_cast< AbstractPlayerObject* >( 0 );
    const AbstractPlayerObject * second_kicker = static_cast< AbstractPlayerObject* >( 0 );

    if ( kicker_unum != Unum_Unknown )
    {
        kicker = wm.ourPlayer( kicker_unum );
    }

    if ( second_kicker_unum != Unum_Unknown )
    {
        second_kicker = wm.ourPlayer( second_kicker_unum );
    }

    if ( ! kicker )
    {
        if ( ! wm.teammatesFromBall().empty()
             && wm.teammatesFromBall().front()->distFromBall() < wm.ball().distFromSelf() * 0.9 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(is_kicker) first kicker",
                          kicker_unum, second_kicker_unum );
            return false;
        }

        dlog.addText( Logger::TEAM,
                      __FILE__":(is_kicker) self(1)" );
        return true;
    }

    if ( kicker
         && second_kicker
         && ( kicker->unum() == wm.self().unum()
              || second_kicker->unum() == wm.self().unum() ) )
    {
        static GameTime s_last_time( -1, 0 );
        static bool s_last_kicker_is_self = false;

        dlog.addText( Logger::TEAM,
                      __FILE__":(is_kicker) first=%d(dist=%.3f) second=%d(dist=%.3f)",
                      kicker_unum, min_dist, second_kicker_unum, second_min_dist );

        if ( s_last_kicker_is_self
             && ( s_last_time.cycle() == wm.time().cycle() - 1
                  || s_last_time.stopped() == wm.time().stopped() - 1 ) )
        {
            if ( second_kicker->unum() == wm.self().unum() )
            {
                if ( min_dist < second_min_dist * 0.8
                     || kicker->distFromBall() < second_kicker->distFromBall() * 0.8 )
                {
                    dlog.addText( Logger::TEAM,
                                  __FILE__":(is_kicker) erase last kicker. kicker=%d ",
                                  kicker->unum() );
                    s_last_kicker_is_self = false;
                    return s_last_kicker_is_self;
                }
            }

            if ( wm.kickableTeammate() )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__":(is_kicker) exist other kicker. erase last kicker. kicker=%d ",
                              kicker->unum() );
                s_last_kicker_is_self = false;
                return s_last_kicker_is_self;
            }

            dlog.addText( Logger::TEAM,
                          __FILE__":(is_kicker) keep last kicker" );
            s_last_time = wm.time();
            return true;
        }

        s_last_time = wm.time();

        if ( min_dist < second_min_dist * 0.95 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(is_kicker) kicker->unum=%d  (1)",
                          kicker->unum() );
            s_last_kicker_is_self = ( kicker->unum() == wm.self().unum() );
            return s_last_kicker_is_self;
        }
        else if ( kicker->distFromBall() < second_kicker->distFromBall() * 0.95 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(is_kicker) kicker->unum=%d  (2)",
                          kicker->unum() );
            s_last_kicker_is_self = ( kicker->unum() == wm.self().unum() );
            return s_last_kicker_is_self;
        }
        else if ( second_kicker->distFromBall() < kicker->distFromBall() * 0.95 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(is_kicker) kicker->unum=%d  (3)",
                          kicker->unum() );
            s_last_kicker_is_self = ( second_kicker->unum() == wm.self().unum() );
        }
        else  if ( ! wm.teammatesFromBall().empty()
                   && wm.teammatesFromBall().front()->distFromBall() < wm.self().distFromBall() * 0.95 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(is_kicker) other kicker",
                          kicker->unum() );
            s_last_kicker_is_self = false;
            return s_last_kicker_is_self;
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(is_kicker) self(2)" );
            s_last_kicker_is_self = true;
            return s_last_kicker_is_self;
        }
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(is_kicker) kicker->unum=%d",
                  kicker->unum() );
    return ( kicker->unum() == wm.self().unum() );
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlay::is_fast_restart_situation( const PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    int our_score = ( wm.ourSide() == LEFT
                      ? wm.gameMode().scoreLeft()
                      : wm.gameMode().scoreRight() );
    int opp_score = ( wm.ourSide() == LEFT
                      ? wm.gameMode().scoreRight()
                      : wm.gameMode().scoreLeft() );

    if ( our_score > opp_score )
    {
        return false;
    }

    int normal_time = ( SP.actualHalfTime() > 0
                        ? SP.actualHalfTime() * SP.nrNormalHalfs()
                        : -1 );

    if ( normal_time < 0 )
    {
        return false;
    }

    if ( wm.time().cycle() < normal_time - 200 )
    {
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlay::is_delaying_tactics_situation( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int real_set_play_count = wm.time().cycle() - wm.lastSetPlayStartTime().cycle();
    const int wait_buf = ( wm.gameMode().type() == GameMode::GoalKick_
                           ? 20
                           : 2 );

    if ( real_set_play_count >= ServerParam::i().dropBallTime() - wait_buf )
    {
        return false;
    }

    int our_score = ( wm.ourSide() == LEFT
                      ? wm.gameMode().scoreLeft()
                      : wm.gameMode().scoreRight() );
    int opp_score = ( wm.ourSide() == LEFT
                      ? wm.gameMode().scoreRight()
                      : wm.gameMode().scoreLeft() );

    if ( wm.audioMemory().recoveryTime().cycle() >= wm.time().cycle() - 10 )
    {
        if ( our_score > opp_score )
        {
            return true;
        }
    }

    long cycle_thr = std::max( 0,
                               ServerParam::i().actualHalfTime()
                               * ServerParam::i().nrNormalHalfs()
                               - 500 );

    if ( wm.time().cycle() < cycle_thr )
    {
        return false;
    }

    if ( our_score > opp_score
         && our_score - opp_score <= 1 )
    {
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */

int
Bhv_SetPlay::estimateOppMin( const WorldModel & wm )
{

    int opp_min = 100;
    if( wm.kickableOpponent() )
    {
        for( PlayerObject::Cont::const_iterator it = wm.opponentsFromBall().begin(),
             end = wm.opponentsFromBall().end();
             it != end;
             ++it )
        {
            if( !(*it) ) continue;
            if( (*it)->goalie() ) continue;
            if( (*it) == wm.kickableOpponent() ) continue;

            const PlayerObject *opp =  *it;

            Vector2D to_ball = wm.ball().pos() - opp->pos();
            if( opp->vel().r() > 0.005
                && to_ball.x * opp->vel().x > 0
                && to_ball.y * opp->vel().y > 0 )
            {
                double dist = wm.ball().pos().dist( opp->pos() );
                if( dist > 30.0 ) continue;
                if( dist > 5.0 )
                    dist -= 5.0;
                int tmp_cycle = opp->playerTypePtr()->cyclesToReachDistance( dist );
                if( opp_min < tmp_cycle )
                    opp_min = tmp_cycle;
            }
        }
        if( opp_min > 20 )
            opp_min = 5;
    }
    else
    {
        for( int i = 1; i <= 11; ++i )
        {
            int opp_unum = i;//unum_opp_mark[j];
            const AbstractPlayerObject * opp = wm.theirPlayer( opp_unum );

            if ( ! opp ) continue;
            if( opp->goalie() ) continue;

            double dist = wm.ball().pos().dist( opp->pos() + opp->vel() );
            int tmp_cycle = opp->playerTypePtr()->cyclesToReachDistance( dist );
            Vector2D to_ball = wm.ball().pos() - opp->pos();

            if( opp_min > tmp_cycle )
            {
                if(  opp->vel().r() > 0.005//0.01~0.001あたりが最適？
                     && ( to_ball.x * opp->vel().x > 0
                          || to_ball.x * opp->vel().y > 0 ) )
                    opp_min = tmp_cycle;
            }
        }
    }

    if( opp_min >= 100 )
    {
        opp_min = wm.interceptTable()->opponentReachCycle();
    }

    return opp_min;
}

/*-------------------------------------------------------------------*/
/*!

 */

Vector2D
Bhv_SetPlay::predictPlayerPos( const WorldModel & wm,
                               int opp_min,
                               int unum )
{
    const AbstractPlayerObject * opp =  wm.theirPlayer( unum );
    if( !opp ) return Vector2D::INVALIDATED;
    if( opp->goalie() ) return Vector2D::INVALIDATED;

    Vector2D opp_pos = opp->pos();
    if( opp_min <= 1
        || opp->vel().r() < 0.2 )
    {
        opp_pos += opp->vel();
    }
    else
    {
        Vector2D addvel = opp->vel().setLengthVector( opp->playerTypePtr()->realSpeedMax() ) * opp_min;
        if( ( opp_pos + addvel ).dist( wm.ball().pos() ) > 15.0 )
        {
            addvel = opp->vel() * opp_min;
        }
        opp_pos += addvel;
    }

    if( opp_pos.absX() > 52.4
        || opp_pos.absY() > 34.0 )
    {
        Line2D line( opp_pos, opp->pos() );
        if( ( opp_pos.absX() - 52.4 ) > ( opp_pos.absY() - 34.0 ) )
        {
            double new_y = line.getY( sign( opp_pos.x ) * 52.4 );
            opp_pos.x = sign( opp_pos.x ) * 52.4;
            opp_pos.y = new_y;
        }
        else
        {
            double new_x = line.getX( sign( opp_pos.y ) * 34.0 );
            opp_pos.x = new_x;
            opp_pos.y = sign( opp_pos.y ) * 34.0;
        }
    }
    if( opp->pos().dist( wm.ball().pos() ) < 3.0
        && ( opp->pos().absX() > 52.4
             || opp->pos().absY() > 34.0 ) )
    {
        opp_pos = opp->pos();
    }

    if( wm.kickableOpponent()
        && wm.ball().pos().dist( opp->pos() ) < 30.0
        && opp->vel().r() > 0.1
        && opp->vel().r() < 0.2 )
    {
        opp_pos += opp->vel().setLengthVector( 3.0 );
    }
    if( !opp_pos.isValid() ) opp_pos = opp->pos();

    return opp_pos;
}

/*-------------------------------------------------------------------*/
/*!

 */

int
Bhv_SetPlay::find_kicker( const WorldModel & wm,
                          int opp_min )
{
    int unum_possible_kicker = Unum_Unknown;
    double tmp = 0;
    double min_dist = 100;
    for( PlayerObject::Cont::const_iterator it = wm.opponentsFromBall().begin(),
             end = wm.opponentsFromBall().end();
         it != end;
         ++it )
    {
        if( !(*it) ) continue;
        if( (*it)->goalie() ) continue;

        Vector2D opp_pos = predictPlayerPos( wm,
                                             opp_min,
                                             (*it)->unum() );
        if( !opp_pos.isValid() ) return Unum_Unknown;
        tmp = opp_pos.dist( wm.ball().pos() );
        if( tmp > 10.0 )
            break;

        if( tmp < min_dist )
        {
            min_dist = tmp;
            unum_possible_kicker = (*it)->unum();
        }
    }

    return unum_possible_kicker;
}

/*-------------------------------------------------------------------*/
/*!

 */

int
Bhv_SetPlay::find_second_kicker( const WorldModel & wm,
                                 int opp_min,
                                 int unum_possible_kicker )
{
    if( unum_possible_kicker < 1 || unum_possible_kicker > 11 ) return Unum_Unknown;

    int unum_possible_2nd_kicker =  Unum_Unknown;
    double min_dist = 100;

    Vector2D kicker_pos = predictPlayerPos( wm,
                                            opp_min,
                                            unum_possible_kicker );
    if( !kicker_pos.isValid() ) return Unum_Unknown;

    for( PlayerObject::Cont::const_iterator it = wm.opponentsFromBall().begin(),
             end = wm.opponentsFromBall().end();
         it != end;
         ++it )
    {
        if( !(*it) )
            continue;
        if( (*it)->goalie() )
            continue;
        if( (*it)->unum() == unum_possible_kicker )
            continue;

        Vector2D opp_pos = predictPlayerPos( wm,
                                             opp_min,
                                             (*it)->unum() );
        if( !opp_pos.isValid() ) continue;

        double tmp = kicker_pos.dist( opp_pos );

        if( min_dist > tmp
            && wm.ball().pos().dist( opp_pos ) <= 15.0 )
        {
            min_dist = tmp;
            unum_possible_2nd_kicker = (*it)->unum();
        }

    }

    return unum_possible_2nd_kicker;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
Bhv_SetPlay::createRecieverCandidate( const WorldModel & wm,
                                      int opp_min,
                                      int unum_possible_kicker,
                                      int target_unum,
                                      std::vector<int>  & candidate_reciever )
{
    const AbstractPlayerObject * possible_2nd_kicker = wm.theirPlayer( target_unum );

    if( !possible_2nd_kicker )
        return false;

    for( PlayerObject::Cont::const_iterator it = wm.opponentsFromBall().begin(),
             end = wm.opponentsFromBall().end();
         it != end;
         ++it )
    {
        if( !(*it) )
            continue;
        if( (*it)->goalie() )
            continue;
        if( (*it)->unum() == unum_possible_kicker )
            continue;
        if( (*it)->unum() == target_unum )
            continue;
        Vector2D opp_pos = predictPlayerPos( wm,
                                             opp_min,
                                             (*it)->unum() );
        if( !opp_pos.isValid() ) continue;
        if( possible_2nd_kicker->pos().dist( opp_pos ) <= 25.0 )
            candidate_reciever.push_back( (*it)->unum() );

        if( possible_2nd_kicker->pos().dist( opp_pos ) >= 40.0 )
            break;
    }

    if( candidate_reciever.size() < 1 )
        return false;
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

Vector2D
Bhv_SetPlay::decide_reciever( const rcsc::WorldModel & wm,
                              int opp_min,
                              std::vector<int> & candidate_reciever,
                              int target_unum )
{
    double score = 0.0;
    Vector2D candidate_pos = Vector2D::INVALIDATED;
    const double circle_r = ServerParam::i().centerCircleR() + wm.self().playerType().playerSize() + 0.001;
    const AbstractPlayerObject * target = wm.theirPlayer( target_unum );

    for ( std::vector<int>::iterator it = candidate_reciever.begin();
          it != candidate_reciever.end();
          ++it )
    {
        /*
        const AbstractPlayerObject *opp  = wm.theirPlayer( *it );
        if( !opp ) continue;
        */
        Vector2D opp_pos = predictPlayerPos( wm,
                                             opp_min,
                                             *it );
        if( !opp_pos.isValid() ) continue;
        Segment2D pass_line( opp_pos, target->pos() );
        bool isNeed = true;
        double thr = 2 * ( opp_pos.dist( target->pos() ) ) / 10;
        if( thr < 2 )
            thr = 2;

        for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(), end = wm.teammates().end();
              t != end;
              ++t )
        {
            if ( (*t)->goalie() ) continue;
            if ( (*t)->unum() == wm.self().unum() ) continue;
            if ( !(*t) ) continue;
            if( pass_line.dist( (*t)->pos() ) < thr )//|| opp_pos().dist( (*t)->pos() ) < 2.5 )
            {
                isNeed = false;
                break;
            }
        }

        if( !isNeed ) continue;

        Circle2D ball_circle( wm.ball().pos(), circle_r );
        //Circle2D move_horizon( home_pos, 15.0 );
        Segment2D mark_line( target->pos(), opp_pos );
        Vector2D sol1, sol2;
        Vector2D cut_point = Vector2D::INVALIDATED;
        int n_sol = ball_circle.intersection( mark_line, &sol1, &sol2 );
        if ( n_sol == 0 ) continue;
        else if ( n_sol == 1 )
        {
            cut_point = sol1;
        }
        else
        {
            cut_point = ( target->pos().dist2( sol1 )
                          < target->pos().dist2( sol2 )
                          ? sol1
                          : sol2 );
        }

        if( !cut_point.isValid() ) continue;

        Vector2D goal_from_target =  ServerParam::i().ourTeamGoalPos() - opp_pos;
        double dist = opp_pos.dist( target->pos() );

        double tmp = ServerParam::i().ourTeamGoalPos().dist2( opp_pos ) * dist;
        //double tmp = ( target->pos().x - opp->pos().x ) * dist;
        if( tmp != 0.0 )
            tmp = goal_from_target.th().abs() / tmp;

        if( tmp > score )
        {
            score = tmp;
            candidate_pos = cut_point;
        }
    }

    return candidate_pos;

}


/*-------------------------------------------------------------------*/
/*!

 */

bool
Bhv_SetPlay::createPasserCandidate( const WorldModel & wm,
                                    int target_unum,
                                    std::vector<int>  & candidate_passer )
{
    const AbstractPlayerObject * reciever = wm.theirPlayer( target_unum );

    if( !reciever )
        return false;

    for( PlayerObject::Cont::const_iterator it = wm.opponentsFromBall().begin(),
             end = wm.opponentsFromBall().end();
         it != end;
         ++it )
    {
        if( !(*it) )
            continue;
        if( (*it)->goalie() )
            continue;
        if( (*it)->unum() == reciever->unum() )
            continue;
        if( sign( wm.ball().pos().y ) * reciever->pos().y > sign( wm.ball().pos().y ) * (*it)->pos().y )
            continue;

        if( reciever->pos().dist( (*it)->pos() ) <= 25.0 )
            candidate_passer.push_back( (*it)->unum() );

        if( wm.ball().pos().dist( (*it)->pos() ) >= 40.0 )
            break;
    }

    if( candidate_passer.size() < 1 )
        return false;

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

Vector2D
Bhv_SetPlay::decide_passer( const rcsc::WorldModel & wm,
                            std::vector<int> & candidate_passer,
                            int target_unum )
{
    double score = 100.0;

    Vector2D candidate_pos = Vector2D::INVALIDATED;
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    const AbstractPlayerObject * target = wm.theirPlayer( target_unum );

    for ( std::vector<int>::iterator it = candidate_passer.begin();
          it != candidate_passer.end();
          ++it )
    {
        const AbstractPlayerObject *opp  = wm.theirPlayer( *it );
        if( !opp ) continue;
        Segment2D pass_line( opp->pos(), target->pos() );
        bool isNeed = true;
        double thr = 2 * ( opp->pos().dist( target->pos() ) ) / 10;
        if( thr < 2 )
            thr = 2;

        for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(), end = wm.teammates().end();
              t != end;
              ++t )
        {
            if ( (*t)->goalie() ) continue;
            if ( (*t)->unum() == wm.self().unum() ) continue;
            if ( !(*t) ) continue;
            if( pass_line.dist( (*t)->pos() ) < thr
                || opp->pos().dist( (*t)->pos() ) < 2.5 )
            {
                isNeed = false;
                break;
            }
        }

        if( !isNeed ) continue;

        Circle2D move_horizon( home_pos, 15.0 );
        Segment2D mark_line( target->pos(), opp->pos() );
        Vector2D sol1, sol2;
        Vector2D cut_point = Vector2D::INVALIDATED;
        int n_sol = move_horizon.intersection( mark_line, &sol1, &sol2 );
        if ( n_sol == 0 ) continue;
        else if ( n_sol == 1 )
        {
            cut_point = sol1;
        }
        else
        {
            cut_point = ( target->pos().dist2( sol1 )
                          < target->pos().dist2( sol2 )
                          ? sol1
                          : sol2 );
        }
        if( !cut_point.isValid() ) continue;

        double tmp = opp->pos().dist( wm.ball().pos() );

        if( tmp < score )
        {
            score = tmp;
            candidate_pos = cut_point;
        }
    }

    return candidate_pos;

}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
Bhv_SetPlay::get_mark_point( const PlayerAgent * agent,
                             int opp_min )
{
    const WorldModel & wm = agent->world();
    const double circle_r = ServerParam::i().centerCircleR() + wm.self().playerType().playerSize() + 0.001;
    Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );


    int target_unum = MarkAnalyzer::i().getFirstTargetByCoach( wm.self().unum() );
    if( target_unum > 11 || target_unum < 1 )
        return Vector2D::INVALIDATED;

    int unum_possible_kicker = find_kicker( wm,
                                            opp_min );
    int unum_possible_2nd_kicker = Unum_Unknown;
    if( unum_possible_kicker != Unum_Unknown )
        unum_possible_2nd_kicker = find_second_kicker( wm,
                                                       opp_min,
                                                       unum_possible_kicker );

    const AbstractPlayerObject * mark_target = wm.theirPlayer( target_unum );
    if ( ! mark_target ) return Vector2D::INVALIDATED;

    Vector2D mark_point = predictPlayerPos( wm,
                                            opp_min,
                                            target_unum );
    if( !mark_point.isValid() ) return Vector2D::INVALIDATED;

    double add_length = mark_point.dist( wm.ball().pos() ) * 0.1;
    if( add_length < 0.6 )
        add_length = 0.6;
    else if( add_length > 5.0 )
        add_length = 5.0;

    Vector2D add_vec = ( ServerParam::i().ourTeamGoalPos() - mark_point ).setLengthVector( add_length );

    if( mark_point.dist( wm.ball().pos() ) < 20.0
        && mark_point.dist( wm.ball().pos() ) > circle_r
        && ( wm.ball().pos().x - mark_point.x ) < circle_r )
    {
        double len = wm.ball().pos().dist( mark_point ) - circle_r;
        if( len > 1.2 ) len = 1.2;
        if( len > 0.6 )
        {
            add_vec = ( wm.ball().pos() - mark_point ).setLengthVector( len );
        }
    }

    if( target_unum == unum_possible_kicker )
        mark_point = wm.ball().pos();

    if( target_unum == unum_possible_kicker
        || target_unum == unum_possible_2nd_kicker )
    {
        int mate_unum = Unum_Unknown;
        Vector2D penalty_area( ServerParam::i().ourPenaltyAreaLineX(), sign( wm.ball().pos().y ) * ( -1 ) * ServerParam::i().penaltyAreaHalfWidth() );

        if( unum_possible_2nd_kicker >= 1 && unum_possible_2nd_kicker <= 11 )
        {
            if( target_unum == unum_possible_kicker )
                mate_unum = MarkAnalyzer::i().getMarkerToFirstTargetByCoach( unum_possible_2nd_kicker );
            else
                mate_unum = MarkAnalyzer::i().getMarkerToFirstTargetByCoach( unum_possible_kicker );
        }

        if( wm.gameMode().type() == GameMode::CornerKick_ )
        {
            if( mate_unum >= 1 && mate_unum <= 11 )
            {
                const Vector2D mate_home_pos = Strategy::i().getPosition( mate_unum );
                //const AbstractPlayerObject * mate = wm.ourPlayer( mate_unum );
                if( mate_home_pos.dist( wm.ball().pos() ) > home_pos.dist( wm.ball().pos() ) )//if( mate->distFromBall() > wm.self().distFromBall() )
                    penalty_area.y *= -1;
            }
            add_vec = ( penalty_area - mark_point ).setLengthVector( circle_r - wm.ball().pos().dist( mark_point ) );
        }
        else if( wm.gameMode().type() == GameMode::KickIn_ )
        {
            if( mate_unum >= 1 && mate_unum <= 11 )
            {
                //const AbstractPlayerObject * mate = wm.ourPlayer( mate_unum );
                const Vector2D mate_home_pos = Strategy::i().getPosition( mate_unum );
                Vector2D penalty_Side( -ServerParam::i().pitchHalfLength(),-ServerParam::i().penaltyAreaHalfWidth() * sign( wm.ball().pos().y ) );

                if( mate_home_pos.dist( wm.ball().pos() ) < home_pos.dist( wm.ball().pos() ) )
                    add_vec = ( penalty_Side - wm.ball().pos() ).setLengthVector( circle_r - wm.ball().pos().dist( mark_point ) );

            }
            else
                add_vec = ( ServerParam::i().ourTeamGoalPos() - mark_point ).setLengthVector( add_length );
        }
    }

    mark_point += add_vec;

    dlog.addText( Logger::TEAM,
                  __FILE__":(get_mark_point) mark opponent[%d] mark_pos=(%.2f %.2f)",
                  mark_target->unum(),
                  mark_point.x, mark_point.y );


    if( unum_possible_2nd_kicker == target_unum
        && unum_possible_2nd_kicker != Unum_Unknown )
    {
        std::vector<int> candidate_reciever;
        if( Bhv_SetPlay::createRecieverCandidate( wm,
                                                  opp_min,
                                                  unum_possible_kicker,
                                                  unum_possible_2nd_kicker,
                                                  candidate_reciever ) )
        {
            Vector2D cut_point = decide_reciever( wm,
                                                  opp_min,
                                                  candidate_reciever,
                                                  unum_possible_2nd_kicker );
            if( cut_point.isValid() )
            {
                if( wm.ball().pos().dist2( cut_point ) > std::pow( circle_r, 2 )
                    && cut_point.dist( home_pos ) < 15.0 )
                    return cut_point;
            }
        }
    }


    if ( wm.ball().pos().dist2( mark_point ) < std::pow( circle_r, 2 ) )
    {
        Vector2D opp_pos = predictPlayerPos( wm,
                                             opp_min,
                                             target_unum );
        Circle2D ball_circle( wm.ball().pos(), circle_r );
        Line2D mark_line( mark_point, opp_pos );
        Vector2D sol1, sol2;
        int n_sol = ball_circle.intersection( mark_line, &sol1, &sol2 );
        if ( n_sol == 0 )
        {
            mark_point = Vector2D::INVALIDATED;
        }
        else if ( n_sol == 1 )
        {
            mark_point = sol1;
        }
        else
        {
            mark_point = ( sol1.absY() < sol2.absY()
                           ? sol1
                           : sol2 );
        }

        if( mark_point.absY() >= ServerParam::i().pitchHalfWidth()
            || mark_point.absX() >= ServerParam::i().pitchHalfLength() )
        {
            Circle2D ball_circle( wm.ball().pos(), circle_r );
            Line2D mark_line( mark_point, ServerParam::i().ourTeamGoalPos() );
            Vector2D sol1, sol2;
            int n_sol = ball_circle.intersection( mark_line, &sol1, &sol2 );
            if ( n_sol == 0 )
            {
                mark_point = Vector2D::INVALIDATED;
            }
            else if ( n_sol == 1 )
            {
                mark_point = sol1;
            }
            else
            {
                mark_point = ( ServerParam::i().ourTeamGoalPos().dist2( sol1 )
                               < ServerParam::i().ourTeamGoalPos().dist2( sol2 )
                               ? sol1
                               : sol2 );
            }
        }
        dlog.addText( Logger::TEAM,
                      __FILE__":(get_mark_point) mark target is kicker. adjusted point=(%.2f %.2f)",
                      mark_point.x, mark_point.y );
    }
    return mark_point;
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
Bhv_SetPlay::get_substitute_point( const PlayerAgent * agent,
                                   int opp_min )
{
    const WorldModel & wm = agent->world();
    //const double circle_r = ServerParam::i().centerCircleR() + wm.self().playerType().playerSize() + 0.001;
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    int target_unum = MarkAnalyzer::i().getFirstTargetByCoach( wm.self().unum() );
    if( target_unum > 11
        || target_unum < 1 )
      return Vector2D::INVALIDATED;

    const AbstractPlayerObject * mark_target = wm.theirPlayer( target_unum );
    if ( ! mark_target ) return Vector2D::INVALIDATED;

    Vector2D mark_point = predictPlayerPos( wm,
                                            opp_min,
                                            target_unum );

    if( !mark_point.isValid() ) return Vector2D::INVALIDATED;
    /*
    if( sign( wm.ball().pos().y ) * mark_target->pos().y <= sign( wm.ball().pos().y ) * wm.self().pos().y )
    {
        std::vector<int> candidate_passer;
        if( createPasserCandidate( wm,
                                   target_unum,
                                   candidate_passer ) )
        {
            mark_point = decide_passer( wm,
                                        candidate_passer,
                                        target_unum );
            if( !mark_point.isValid() )
            {
                mark_point = Vector2D::INVALIDATED;
                isCut = false;
            }
            else
            {
                isCut = true;
            }
        }
    }
    */
    if(  wm.ball().pos().dist( mark_point ) < wm.ball().pos().dist( home_pos ) )
    {
        Circle2D move_horizon( home_pos, 15.0 );
        Segment2D mark_line( mark_point, ServerParam::i().ourTeamGoalPos() );
        Vector2D sol1, sol2;
        int n_sol = move_horizon.intersection( mark_line, &sol1, &sol2 );
        if ( n_sol == 0 )
        {
            mark_point = Vector2D::INVALIDATED;
        }
        else if ( n_sol == 1 )
        {
            mark_point = sol1;
        }
        else
        {
            mark_point = ( mark_target->pos().dist2( sol1 )
                           < mark_target->pos().dist2( sol2 )
                           ? sol1
                           : sol2 );
        }
    }


    dlog.addText( Logger::TEAM,
                  __FILE__":(get_substitute_point) mark opponent[%d]. adjusted point=(%.2f %.2f)",
                  mark_target->unum(),
                  mark_point.x, mark_point.y );

    return mark_point;

}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_SetPlay::doBasicTheirSetPlayMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const double circle_r = ServerParam::i().centerCircleR() + wm.self().playerType().playerSize() + 0.001;

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    dlog.addText( Logger::TEAM,
                  __FILE__": their set play. HomePosition=(%.2f, %.2f)",
                  home_pos.x, home_pos.y );

    Vector2D target_point = home_pos;

#if 0
    //
    // decide mark target
    //
    if ( wm.gameMode().type() != GameMode::KickOff_ )
    {
        Vector2D mark_point = Bhv_SetPlay::get_mark_point( agent );

        if ( mark_point.isValid() )
        {
            if ( mark_point.dist2( home_pos ) < std::pow( 15.0, 2 ) )
            {
                target_point = mark_point;
            }

            const AbstractPlayerObject * mark_target = MarkAnalyzer::i().getTargetOf( wm.self().unum() );
            if ( mark_target
                 && ( mark_target->ghostCount() >= 1
                      || mark_target->unumCount() >= 5 ) )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": mark find target" );
                Bhv_FindPlayer( mark_target, -1 ).execute( agent );
                return;
            }
        }
    }
#endif

    //
    // avoid kickoff offside
    //
    if ( wm.gameMode().type() == GameMode::KickOff_
         && ServerParam::i().kickoffOffside() )
    {
        target_point.x = std::min( -0.5, target_point.x );

        dlog.addText( Logger::TEAM,
                      __FILE__": avoid kickoff offside. (%.2f %.2f)",
                      target_point.x, target_point.y );
    }

    //
    // adjust target to avoid ball circle
    //
    if ( wm.ball().pos().dist2( target_point ) < std::pow( circle_r, 2 ) )
    {
        if ( target_point.dist2( wm.ball().pos() ) < 0.001 )
        {
            target_point = wm.ball().pos() + ( ServerParam::i().ourTeamGoalPos() - wm.ball().pos() ).setLengthVector( circle_r );
        }
        else
        {
            target_point = wm.ball().pos() + ( target_point - wm.ball().pos() ).setLengthVector( circle_r );
        }

        dlog.addText( Logger::TEAM,
                      __FILE__": avoid circle(2). adjust len. new_pos=(%.2f %.2f)",
                      target_point.x, target_point.y );
    }

    //
    // find sub target to avoid ball circle
    //
    dlog.addText( Logger::TEAM,
                  __FILE__":(doBasicTheirSetPlayMove) find sub target to avoid ball circle" );

    Vector2D adjusted_point = target_point;
    if ( wm.ball().pos().dist( adjusted_point ) < get_avoid_radius( wm ) )
    {
        adjusted_point = wm.ball().pos()
            + ( adjusted_point - wm.ball().pos() ).setLengthVector( get_avoid_radius( wm ) + 0.1 );
    }


    if ( ! wm.gameMode().isServerCycleStoppedMode()
         || wm.getSetPlayCount() > 25 )
    {
        adjusted_point = get_avoid_circle_point( wm, adjusted_point );
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": target=(%.2f %.2f) adjusted=(%.2f %.2f)",
                  target_point.x, target_point.y,
                  adjusted_point.x, adjusted_point.y );

    //
    // go to the target point
    //
    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.7 ) dist_thr = 0.7;
    if ( wm.gameMode().type() == GameMode::CornerKick_ )
    {
        dist_thr = 0.7;
    }

    if ( adjusted_point != target_point
         && wm.ball().pos().dist( target_point ) > 10.0
         && wm.self().inertiaFinalPoint().dist( adjusted_point ) < dist_thr )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": reverted to the first target point" );
        adjusted_point = target_point;
    }

    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );

    //agent->debugClient().addMessage( "GoTo" );
    agent->debugClient().setTarget( adjusted_point );
    agent->debugClient().addCircle( adjusted_point, dist_thr );

    if ( ! Body_GoToPoint( adjusted_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        AngleDeg body_angle = wm.ball().angleFromSelf();
        if ( body_angle.degree() < 0.0 ) body_angle -= 90.0;
        else body_angle += 90.0;

        Body_TurnToAngle( body_angle ).execute( agent );
    }

    agent->setNeckAction( new Neck_TurnToBall() );
}

/*-------------------------------------------------------------------*/
/*!

 */

void
Bhv_SetPlay::doMarkTheirSetPlayMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if (( wm.gameMode().type() == GameMode::KickIn_
          || wm.gameMode().type() == GameMode::CornerKick_)
         && wm.gameMode().side() == wm.theirSide()
         && wm.ball().pos().x < -36.0 )
    {
        doBasicTheirSetPlayMove( agent );
        return;
    }

    const double circle_r = ServerParam::i().centerCircleR() + wm.self().playerType().playerSize() + 0.001;
    const bool isSetPlayMarker = Strategy::i().isSetPlayMarkerType( wm.self().unum() );
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    //const Vector2D defense_pos = Strategy::i().getDefensePosition( wm.self().unum() );

    dlog.addText( Logger::TEAM,
                  __FILE__": their set play. HomePosition=(%.2f, %.2f)",
                  home_pos.x, home_pos.y );

    Vector2D target_point = home_pos;
    int opp_min = estimateOppMin( wm );
    /*
    double their_offense_line_x = 100.0;
    for( int i = 1; i <= 11; ++i )
    {
        Vector2D opp_pos = predictPlayerPos( wm,
                                             opp_min,
                                             i );
        if( opp_pos.x < their_offense_line_x )
            their_offense_line_x = opp_pos.x;
    }
    */
    //
    // use assignment by coach
    //
    if ( ( wm.gameMode().type() == GameMode::KickIn_
           || wm.gameMode().type() == GameMode::CornerKick_)
         && wm.gameMode().side() == wm.theirSide() )
    {
        Vector2D mark_point = Bhv_SetPlay::get_mark_point( agent,
                                                           opp_min );
        const int target_unum = MarkAnalyzer::i().getFirstTargetByCoach( wm.self().unum() );
        const AbstractPlayerObject * mark_target = wm.theirPlayer( target_unum );

        if ( mark_point.isValid()
             && isSetPlayMarker )
        {
            if ( mark_point.dist2( home_pos ) < std::pow( 15.0, 2 ) )
            {
                target_point = mark_point;
            }
            else if( mark_point.dist2( home_pos ) < std::pow( 20.0, 2 ) )
            {
                mark_point = get_substitute_point( agent,
                                                   opp_min );
                if ( mark_point.isValid() )
                {
                    target_point = mark_point;
                }
            }

            if ( mark_target
                 && ( mark_target->ghostCount() >= 1
                      || mark_target->unumCount() >= 5 ) )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__": mark find target" );
                Bhv_FindPlayer( mark_target, -1 ).execute( agent );
                return;
            }
        }
    }


    //
    // avoid kickoff offside
    //
    if ( wm.gameMode().type() == GameMode::KickOff_
         && ServerParam::i().kickoffOffside() )
    {
        target_point.x = std::min( -0.5, target_point.x );

        dlog.addText( Logger::TEAM,
                      __FILE__": avoid kickoff offside. (%.2f %.2f)",
                      target_point.x, target_point.y );
    }

    //
    // adjust target to avoid ball circle
    //
    if ( wm.ball().pos().dist2( target_point ) < std::pow( circle_r, 2 ) )
    {
        if ( target_point.dist2( wm.ball().pos() ) < 0.001 )
        {
            target_point = wm.ball().pos() + ( ServerParam::i().ourTeamGoalPos() - wm.ball().pos() ).setLengthVector( circle_r );
        }
        else
        {
            target_point = wm.ball().pos() + ( target_point - wm.ball().pos() ).setLengthVector( circle_r );
        }

        dlog.addText( Logger::TEAM,
                      __FILE__": avoid circle(2). adjust len. new_pos=(%.2f %.2f)",
                      target_point.x, target_point.y );
    }

    //
    // find sub target to avoid ball circle
    //
    dlog.addText( Logger::TEAM,
                  __FILE__":(doBasicTheirSetPlayMove) find sub target to avoid ball circle" );

    Vector2D adjusted_point = target_point;
    if ( wm.ball().pos().dist( adjusted_point ) < get_avoid_radius( wm ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(doBasicTheirSetPlayMove) target in the ball circle. adjust" );
        adjusted_point = wm.ball().pos()
            + ( adjusted_point - wm.ball().pos() ).setLengthVector( get_avoid_radius( wm ) + 0.1 );
    }


    if ( ! wm.gameMode().isServerCycleStoppedMode()
         || wm.getSetPlayCount() > 25 )
    {
        adjusted_point = get_avoid_circle_point( wm, adjusted_point );
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": target=(%.2f %.2f) adjusted=(%.2f %.2f)",
                  target_point.x, target_point.y,
                  adjusted_point.x, adjusted_point.y );

    //
    // go to the target point
    //
    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 0.7 ) dist_thr = 0.7;
    dist_thr += 1 - ( wm.self().stamina() / ServerParam::i().staminaMax() );
    if ( wm.gameMode().type() == GameMode::CornerKick_ )
    {
        dist_thr = 0.7;
    }

    if ( adjusted_point != target_point
         && wm.ball().pos().dist( target_point ) > 10.0
         && wm.self().inertiaFinalPoint().dist( adjusted_point ) < dist_thr )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": reverted to the first target point" );
        adjusted_point = target_point;
    }

    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );

    //agent->debugClient().addMessage( "GoTo" );
    agent->debugClient().setTarget( adjusted_point );
    agent->debugClient().addCircle( adjusted_point, dist_thr );

    if ( ! Body_GoToPoint( adjusted_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        AngleDeg body_angle = wm.ball().angleFromSelf();
        if ( body_angle.degree() < 0.0 ) body_angle -= 90.0;
        else body_angle += 90.0;

        Body_TurnToAngle( body_angle ).execute( agent );
    }

    agent->setNeckAction( new Neck_TurnToBall() );
}
