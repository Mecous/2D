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

#include "bhv_defensive_half_avoid_mark_move.h"

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
check_stamina( const WorldModel & wm )
{
    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(check_stamina) false: empty stamina capacity" );
        return false;
    }

    const double dash_dist = 30.0;
    const int dash_step = wm.self().playerType().cyclesToReachDistance( dash_dist );
    const double required_stamina
        = wm.self().playerType().getOneStepStaminaComsumption() * dash_step;
    const double available_stamina = wm.self().stamina() - ServerParam::i().recoverDecThrValue() - 1.0;

    dlog.addText( Logger::ROLE,
                  __FILE__":(check_stamina) check stamina for %.0fm dash. required=%.1f available=%.1f",
                  dash_dist, required_stamina, available_stamina );

    if ( available_stamina < required_stamina )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(check_stamina) false: no sufficient stamina" );
        return false;
    }

    const double capacity_rate = wm.self().staminaModel().capacity() / ServerParam::i().staminaCapacity();

    const int normal_time = ServerParam::i().actualHalfTime() * ServerParam::i().nrNormalHalfs();
    if ( ServerParam::i().actualHalfTime() > 0
         && wm.time().cycle() < normal_time )
    {
        double time_rate
            = static_cast< double >( wm.time().cycle() % ServerParam::i().actualHalfTime() )
            / ServerParam::i().actualHalfTime();
        dlog.addText( Logger::ROLE,
                      __FILE__":(check_stamina) check stamina capacity rate (normal time)" );
        dlog.addText( Logger::ROLE,
                      __FILE__":(check_stamina) current=%.3f time_rate=%.3f",
                      capacity_rate, 1.0 - time_rate );

        if ( capacity_rate < 1.0 - time_rate - 0.1 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(check_stamina) false: save capacity (normal time)" );
            return false;
        }
    }
    else if ( ServerParam::i().actualExtraHalfTime() > 0
              && wm.time().cycle() >= normal_time )
    {
        // overtime
        int elapsed_time = wm.time().cycle() - normal_time;
        int extra_time = ServerParam::i().actualExtraHalfTime() * ServerParam::i().nrExtraHalfs();
        double time_rate = static_cast< double >( elapsed_time ) / extra_time;
        dlog.addText( Logger::ROLE,
                      __FILE__":(check_stamina) check stamina capacity rate (over time)" );
        dlog.addText( Logger::ROLE,
                      __FILE__":(check_stamina) current=%.3f time_rate=%.3f",
                      capacity_rate, 1.0 - time_rate );

        if ( capacity_rate < 1.0 - time_rate )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(check_stamina) false: save capacity (over time)" );
            return false;
        }
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(check_stamina) true" );
    return true;
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

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromBall().begin(),
              end = wm.opponentsFromBall().end();
          o != end;
          ++o )
    {
        if ( (*o)->isGhost() ) continue;
        if ( (*o)->isTackling() ) continue;
        if ( (*o)->posCount() > 10 ) continue;
        if ( (*o)->distFromBall() > 5.0 ) break;

        if ( (*o)->distFromBall() < 1.5 )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(check_ball_status) false: opponent is close to the ball" );
            return false;
        }
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(check_ball_status) OK" );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_target_point( const WorldModel & wm )
{
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    const PositionType position_type = Strategy::i().getPositionType( wm.self().unum() );
    const double min_y = ( position_type == Position_Left
                           ? std::max( home_pos.y - 10.0, -ServerParam::i().pitchHalfWidth() + 2.0 )
                           : position_type == Position_Right
                           ? home_pos.y - 3.0
                           : std::max( home_pos.y - 10.0, -ServerParam::i().pitchHalfWidth() + 2.0 ) );
    const double max_y = ( position_type == Position_Left
                           ? home_pos.y + 3.0
                           : position_type == Position_Right
                           ? std::min( home_pos.y + 10.0, +ServerParam::i().pitchHalfWidth() - 2.0 )
                           : std::min( home_pos.y + 10.0, +ServerParam::i().pitchHalfWidth() - 2.0 ) );
    const double y_step = ( max_y - min_y ) / 10.0;

    Vector2D best_point = home_pos;
    double best_dist2 = 0.0;
    for ( double y = min_y; y < max_y + 0.001; y += y_step )
    {
        Vector2D pos( home_pos.x, y );
        double min_dist2 = 100000.0;
        for ( PlayerObject::Cont::const_iterator o = wm.opponents().begin(), end = wm.opponents().end();
              o != end;
              ++o )
        {
            double d2 = (*o)->pos().dist2( pos );
            if ( d2 < min_dist2 )
            {
                min_dist2 = d2;
            }
        }

        dlog.addText( Logger::ROLE,
                      __FILE__":(doAvoidMark) check pos(%.2f %.2f) dist=%.2f",
                      pos.x, pos.y, std::sqrt( min_dist2 ) );

        if ( best_dist2 < min_dist2 )
        {
            best_point = pos;
            best_dist2 = min_dist2;
        }
    }

    return best_point;
}

/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*!

 */
class IntentionAvoidMark
    : public SoccerIntention {
private:
    GameTime M_last_time;
    int M_count;
    int M_stay_count;
    Vector2D M_target_point;

public:

    IntentionAvoidMark( const GameTime & start_time,
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
IntentionAvoidMark::finished( const PlayerAgent * agent )
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
                      __FILE__":(intention::finished) true: illegal ball status" );
        return true;
    }

    dlog.addText( Logger::ROLE,
                  __FILE__":(intention::finished) false" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionAvoidMark::execute( PlayerAgent * agent )
{
    M_last_time = agent->world().time();

    if ( ! Body_GoToPoint( M_target_point, 0.5, ServerParam::i().maxDashPower(),
                           -1.0, 100 ).execute( agent ) )
    {
        Body_TurnToBall().execute( agent );
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );

    agent->debugClient().addMessage( "DH:IntentionAvoidMark" );
    agent->debugClient().setTarget( M_target_point );

    return true;
}

}

/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_DefensiveHalfAvoidMarkMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ROLE,
                  __FILE__": Bhv_DefensiveHalfAvoidMarkMove" );

    const WorldModel & wm = agent->world();

    //
    // check distance to the home position
    //

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    //if ( wm.self().pos().dist2( home_pos ) > std::max( 0.5, wm.ball().distFromSelf() * 0.1 ) )
    if ( wm.self().pos().dist2( home_pos ) > 5.0 )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(execute) far home position" );
        return false;
    }

    //
    // check stamina
    //
    if ( ! check_stamina( wm ) )
    {
        return false;
    }

    //
    // check ball status
    //
    if ( ! check_ball_status( wm ) )
    {
        return false;
    }

    //
    // check marking status
    //

    if ( ! FieldAnalyzer::i().existOpponentManMarker()
         && ! FieldAnalyzer::i().existOpponentPassLineMarker() )
    {
        return false;
    }

    const Vector2D target_point = get_target_point( wm );

    dlog.addText( Logger::ROLE,
                  __FILE__":(doAvoidMark) target=(%.2f %.2f)",
                  target_point.x, target_point.y );

    if ( ! Body_GoToPoint( target_point, 0.5, ServerParam::i().maxDashPower(),
                           -1.0, 100 ).execute( agent ) )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__":(doAvoidMark) already there",
                      target_point.x, target_point.y );
        return false;
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    agent->setArmAction( new Arm_PointToPoint( target_point ) );

    agent->setIntention( new IntentionAvoidMark( wm.time(), target_point ) );

    agent->debugClient().addMessage( "DH:AvoidMark" );
    agent->debugClient().setTarget( target_point );

    return true;
}
