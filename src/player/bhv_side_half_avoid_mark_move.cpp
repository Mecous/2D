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

#include "bhv_side_half_avoid_mark_move.h"

#include "field_analyzer.h"
#include "strategy.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/arm_point_to_point.h>

#include <rcsc/action/body_intercept.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/soccer_intention.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

using namespace rcsc;

namespace {

/*-------------------------------------------------------------------*/
/*!

 */
bool
check_marked( const WorldModel & wm )
{
    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(), end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        if ( (*o)->goalie() ) continue;
        if ( (*o)->ghostCount() > 1 ) continue;
        if ( (*o)->posCount() >= 3 ) continue;

        if ( (*o)->distFromSelf() > 3.0 ) break;  // distance threshold

        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
check_stamina( const WorldModel & wm )
{
    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        return false;
    }

    if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.8 )
    {
        return false;
    }

    const double dash_dist = 10.0;
    const int dash_step = wm.self().playerType().cyclesToReachDistance( dash_dist );
    const double required_stamina = wm.self().playerType().getOneStepStaminaComsumption() * dash_step;
    const double available_stamina = std::max( 0.0, wm.self().stamina() - ServerParam::i().recoverDecThrValue() - 1.0 );

    if ( available_stamina < required_stamina )
    {
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
get_player_distance( const WorldModel & wm,
                     const Vector2D & pos )
{
    double min_dist2 = 10000000.0;

    for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(), end = wm.teammates().end();
          t != end;
          ++t )
    {
        if ( (*t)->goalie() ) continue;

        double d2 = (*t)->pos().dist2( pos );
        if ( d2 < min_dist2 )
        {
            min_dist2 = d2;
        }
    }

    for ( PlayerObject::Cont::const_iterator o = wm.opponents().begin(), end = wm.opponents().end();
          o != end;
          ++o )
    {
        if ( (*o)->goalie() ) continue;

        double d2 = (*o)->pos().dist2( pos );
        if ( d2 < min_dist2 )
        {
            min_dist2 = d2;
        }
    }

    if ( min_dist2 > 100000.0 )
    {
        return -1.0;
    }

    return std::sqrt( min_dist2 );
}

/*-------------------------------------------------------------------*/
/*!

 */
double
evaluate( const WorldModel & wm,
          const Vector2D & pos )
{
    double player_dist = get_player_distance( wm, pos );
    double player_dist_value = 0.0;

    if ( player_dist > 10.0 )
    {
        player_dist_value = 10.0;
    }
    else
    {
        player_dist_value = player_dist;
    }

    int teammate_step = wm.interceptTable()->teammateReachStep();
    Vector2D ball_pos = wm.ball().inertiaPoint( teammate_step );
    double ball_dist = ball_pos.dist( pos );
    double ball_dist_value = 0.0;
    if ( ball_dist < 5.0 )
    {
        ball_dist_value -= ( 5.0 - ball_dist ) * 10.0;
    }
    else
    {
        ball_dist_value -= ( ball_dist - 5.0 ) * 0.3;
    }

    double x_value = ( pos.x - wm.self().pos().x ) * 0.5;

    double value = player_dist_value + ball_dist_value + x_value;

    dlog.addText( Logger::ROLE,
                  "__ (%.1f %.1f) value=%.3f (pdist_val=%.2f bdist_value=%.2f)",
                  pos.x, pos.y, value, player_dist_value, ball_dist_value );

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_target_point( const WorldModel & wm )
{
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const Vector2D ball_pos = wm.ball().inertiaPoint( teammate_step );
    const AngleDeg ball_angle = ( wm.ball().pos() - home_pos ).th();

    Vector2D best_point = Vector2D::INVALIDATED;
    double best_value = 0.0;

    for ( double d = 5.0; d < 11.0; d+= 5.0 )
    {
        for ( double a = 0.0; a < 360.0; a += 30.0 )
        {
            AngleDeg angle = a;
            if ( ( ball_angle - angle ).abs() > 130.0 )
            {
                continue;
            }

            Vector2D pos = home_pos + Vector2D::from_polar( d, angle );

            if ( pos.absX() > ServerParam::i().pitchHalfLength() - 3.0
                 || pos.absY() > ServerParam::i().pitchHalfWidth() - 3.0 )
            {
                continue;
            }

            if ( wm.self().pos().dist( pos ) > ball_pos.dist( pos ) * 0.9 )
            {
                continue;
            }

            double value = evaluate( wm, pos );

            if ( value > best_value )
            {
                best_point = pos;
                best_value = value;
            }
        }
    }

    if ( best_point.isValid() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": target=(%.2f %.2f) value=%.3f",
                      best_point.x, best_point.y, best_value );
    }

    return best_point;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
check_ball_status( const WorldModel & wm )
{
    const int self_step = wm.interceptTable()->selfReachStep();
    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const int opponent_step = wm.interceptTable()->opponentReachStep();

    if ( self_step <= teammate_step
         && self_step <= opponent_step + 1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(check_ball_status) false: intercept situation" );
        return false;
    }

    if ( opponent_step <= teammate_step + 1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(check_ball_status) false: opponent may have the ball" );
        return false;
    }

    const Vector2D teammate_ball_pos = wm.ball().inertiaPoint( teammate_step );

    if ( teammate_ball_pos.dist2( wm.self().pos() ) > std::pow( 30.0, 2 ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(check_ball_staus) false: too far ball" );
        return false;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(check_ball_status) OK" );
    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
class IntentionSideHalfAvoidMark
    : public SoccerIntention {
private:
    GameTime M_last_time;
    int M_count;
    int M_stay_count;
    Vector2D M_target_point;

public:

    IntentionSideHalfAvoidMark( const GameTime & start_time,
                                const Vector2D & target_point )
        : M_last_time( start_time ),
          M_count( 0 ),
          M_stay_count( 0 ),
          M_target_point( target_point )
      { }

    bool finished( const PlayerAgent * agent );

    bool execute( PlayerAgent * agent );

};


/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionSideHalfAvoidMark::finished( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    ++M_count;
    if ( M_count > 20 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(intention::finished) true: over count %d", M_count );
        return true;
    }

    if ( M_last_time.cycle() != wm.time().cycle() - 1 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(intention::finished) true: illegal time" );
        return true;
    }

    if ( wm.self().pos().dist2( M_target_point ) < std::pow( 0.5, 2 ) )
    {
        ++M_stay_count;
        if ( M_stay_count >= 2 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(intention::finished) true: over stay" );
            return true;
        }
    }

    if ( wm.audioMemory().passTime() == wm.time()
         && ! wm.audioMemory().pass().empty()
         && wm.audioMemory().pass().front().receiver_ == wm.self().unum() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(intention::finished) true: receive pass message" );
        return true;
    }

    if ( ! check_ball_status( wm ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(intention:finished) true: ball status NG" );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(intention::finished) false: continue" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionSideHalfAvoidMark::execute( PlayerAgent * agent )
{
    M_last_time = agent->world().time();

    if ( ! Body_GoToPoint( M_target_point, 0.5, ServerParam::i().maxDashPower(),
                           -1.0, 100 ).execute( agent ) )
    {
        Body_TurnToBall().execute( agent );
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );

    agent->debugClient().addMessage( "SH:IntentionAvoidMark" );
    agent->debugClient().setTarget( M_target_point );

    return true;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SideHalfAvoidMarkMove::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_SideHalfAvoidMarkMove" );

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    if ( wm.self().pos().dist2( home_pos ) > std::pow( 10.0, 2 ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(execute) far home position" );
        return false;
    }

    if ( ! check_marked( wm ) )
    {
        return false;
    }

    if ( ! check_stamina( wm ) )
    {
        return false;
    }


    const Vector2D target_point = get_target_point( wm );

    if ( ! target_point.isValid() )
    {
        return false;
    }

    if ( ! Body_GoToPoint( target_point, 0.5, ServerParam::i().maxDashPower(),
                           -1.0, 100 ).execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doAvoidMark) already there",
                      target_point.x, target_point.y );
        Body_TurnToBall().execute( agent );
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    agent->setArmAction( new Arm_PointToPoint( target_point ) );

    agent->setIntention( new IntentionSideHalfAvoidMark( wm.time(), target_point ) );

    agent->debugClient().addMessage( "SH:AvoidMark" );
    agent->debugClient().setTarget( target_point );

    return true;
}
