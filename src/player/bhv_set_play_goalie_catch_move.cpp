// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Yosuke Narimoto

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

#include "bhv_set_play_goalie_catch_move.h"

#include "strategy.h"
#include "bhv_set_play.h"
#include "bhv_set_play_avoid_mark_move.h"

#include "field_analyzer.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/audio_memory.h>

#include <rcsc/geom/rect_2d.h>

#include <rcsc/math_util.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlayGoalieCatchMove::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_SetPlayGoalieCatchMove" );

    const WorldModel & wm = agent->world();

    if( wm.self().goalie() )
    {
      return false;
    }

    //doMoveOld( agent );
    doMove( agent );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_SetPlayGoalieCatchMove::doMove( PlayerAgent * agent )
{
    if ( getMoveStartCount() < agent->world().getSetPlayCount() )
    {
        doSecondMove( agent );
    }
    else
    {
        doFirstMove( agent );
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_SetPlayGoalieCatchMove::doFirstMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point;

    StaminaModel stamina_model = wm.self().staminaModel();
    stamina_model.simulateWaits( wm.self().playerType(),
                                 std::max( 0, getMoveStartCount() - wm.getSetPlayCount() ) );

    if ( stamina_model.stamina() > ServerParam::i().staminaMax() * 0.5 )
    {
        target_point = getFirstMoveTarget( agent );
    }
    else
    {
        target_point = getSecondMoveTarget( agent );
    }

    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
    double dist_thr = 1.0;

    agent->debugClient().addMessage( "GoalieCatch1st" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );
    dlog.addText( Logger::TEAM,
                  __FILE__":(doFirstMove) target=(%.2f %.2f)",
                  target_point.x, target_point.y );

    if ( ! Body_GoToPoint( target_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Body_TurnToBall().execute( agent );
    }

    agent->setNeckAction( new Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_SetPlayGoalieCatchMove::doSecondMove( PlayerAgent * agent )
{
    Vector2D target_point = getSecondMoveTarget( agent );

    double dash_power = ServerParam::i().maxDashPower();
    double dist_thr = 1.0;

    agent->debugClient().addMessage( "GoalieCatch2nd" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Body_TurnToBall().execute( agent );
    }

    agent->setNeckAction( new Neck_ScanField() );

    // if ( agent->world().self().armMovable() == 0 )
    // {
    //     agent->setArmAction( new Arm_PointToPoint( target_point ) );
    // }
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
Bhv_SetPlayGoalieCatchMove::getFirstMoveTarget( const PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    Vector2D target_point = getSecondMoveTarget( agent );
    double rate = bound( SP.ourPenaltyAreaLineX(), target_point.x, 0.0 ) / SP.ourPenaltyAreaLineX();
    double shift = std::fabs( 10.0 * rate );

    dlog.addText( Logger::TEAM,
                  __FILE__":(getFirstMoveTarget) base=(%.2f %.2f) rate=%.3f shift=%.3f",
                  target_point.x, target_point.y, rate, shift );

    double adjust_x = target_point.x;
    double adjust_y = ( wm.ball().pos().y < 0.0
                        ? target_point.y + shift
                        : target_point.y - shift );

    if ( std::fabs( adjust_y ) > SP.pitchHalfWidth() - 1.0 )
    {
        // double y_diff = std::fabs( std::fabs( adjust_y ) - ( SP.pitchHalfWidth() - 1.0 ) );
        // adjust_x = target_point.x + std::sqrt( std::pow( shift, 2 ) - std::pow( y_diff, 2 ) );
        adjust_y = sign( adjust_y ) * ( SP.pitchHalfWidth() - 1.0 );
    }

    target_point.assign( adjust_x, adjust_y );

    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
Bhv_SetPlayGoalieCatchMove::getSecondMoveTarget( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
    // target_point.y += ( wm.ball().pos().y < 0.0
    //                     ? -5.0
    //                     : +5.0 );

    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

*/
int
Bhv_SetPlayGoalieCatchMove::getMoveStartCount()
{
    return bound( 10,  ServerParam::i().dropBallTime() - 20, 80 );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_SetPlayGoalieCatchMove::doMoveOld( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );

    if ( wm.getSetPlayCount() >= SP.dropBallTime() - 70
         && wm.self().stamina() > SP.staminaMax() * 0.6 )
    {
        int teammate_count = 1;
        int opponent_count = 0;

        for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromSelf().begin(),
                  end = wm.teammatesFromSelf().end();
              t != end;
              ++t )
        {
            if ( target_point.dist2( (*t)->pos() ) < std::pow( 5.0, 2 ) )
            {
                ++teammate_count;
            }
        }

        for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
                  end = wm.opponentsFromSelf().end();
              o != end;
              ++o )
        {
            if ( target_point.dist2( (*o)->pos() ) < std::pow( 5.0, 2 ) )
            {
                ++opponent_count;
            }
        }

        if ( opponent_count <= teammate_count )
        {
            if ( Bhv_SetPlayAvoidMarkMove().execute( agent ) )
            {
                return;
            }
        }
    }

    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    agent->debugClient().addMessage( "GoalieCatchMove" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Body_TurnToBall().execute( agent );
    }

    if ( ! wm.self().staminaModel().capacityIsEmpty() )
    {
        const double capacity_thr = SP.staminaCapacity() * Strategy::get_remaining_time_rate( wm );
        if ( wm.self().stamina() < SP.staminaMax() * 0.7
             || wm.self().staminaCapacity() < capacity_thr
             || wm.self().pos().dist( target_point ) > wm.ball().pos().dist( target_point ) * 0.2 + 6.0 )
        {
            agent->debugClient().addMessage( "SayWait" );
            agent->addSayMessage( new WaitRequestMessage() );
        }
    }

    agent->setNeckAction( new Neck_ScanField() );
}
