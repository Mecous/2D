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

#include "bhv_goalie_free_kick.h"

#include "strategy.h"

#include "bhv_chain_action.h"
#include "bhv_clear_ball.h"
#include "bhv_set_play.h"
#include "intention_wait_after_set_play_kick.h"

#include "action_chain_holder.h"
#include "cooperative_action.h"

#include <rcsc/action/body_clear_ball.h>
#include <rcsc/action/body_pass.h>
#include <rcsc/action/body_go_to_point.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/rect_2d.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_GoalieFreeKick::execute( PlayerAgent * agent )
{
    static bool s_first_move = false;
    static bool s_second_move = false;

    const WorldModel & wm = agent->world();

    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_GoalieFreeKick" );
    if ( wm.gameMode().type() != GameMode::GoalieCatch_
         || wm.gameMode().side() != wm.ourSide()
         || ! wm.self().isKickable() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Bhv_GoalieFreeKick. Not a goalie catch mode" );

        doFreeKickMove( agent );
        return true;
    }


    const long time_diff
        = wm.time().cycle()
        - agent->effector().getCatchTime().cycle();

    // reset flags & wait
    if ( time_diff <= 2 )
    {
        s_first_move = false;
        s_second_move = false;

        doWait( agent );
        return true;
    }

    // first move
    if ( ! s_first_move )
    {
        //Vector2D move_target( ServerParam::i().ourPenaltyAreaLine() - 0.8, 0.0 );
        Vector2D move_target( ServerParam::i().ourPenaltyAreaLineX() - 1.5,
                              wm.ball().pos().y > 0.0 ? -13.0 : 13.0 );
        //Vector2D move_target( -45.0, 0.0 );
        s_first_move = true;
        s_second_move = false;
        agent->doMove( move_target.x, move_target.y );
        agent->setNeckAction( new Neck_ScanField );
        return true;
    }

    // after first move
    // check stamina recovery or wait teammate
    Rect2D our_pen( Vector2D( -52.5, -40.0 ),
                    Vector2D( -36.0, 40.0 ) );

    if ( wm.getSetPlayCount() < ServerParam::i().dropBallTime() - 10
         // && ( time_diff < 50
         //      || wm.getSetPlayCount() < 3
         //      || wm.self().stamina() < ServerParam::i().staminaMax() * 0.9
         //      || wm.audioMemory().waitRequestTime().cycle() > wm.time().cycle() - 10
         //      )
        )
    {
        doWait( agent );
        return true;
    }

    // second move
    if ( ! s_second_move )
    {
        Vector2D kick_point = getKickPoint( agent );
        agent->doMove( kick_point.x, kick_point.y );
        agent->setNeckAction( new Neck_ScanField );
        s_second_move = true;
        return true;
    }

    // after second move
    // wait see info
    if ( wm.getSetPlayCount() < ServerParam::i().dropBallTime() - 10
         || wm.seeTime() != wm.time() )
    {
        doWait( agent );
        return true;
    }

    if ( Bhv_ChainAction().execute( agent ) )
    {
        agent->setIntention( new IntentionWaitAfterSetPlayKick() );
        return true;
    }

    s_first_move = false;
    s_second_move = false;

    // register kick intention
    doKick( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_GoalieFreeKick::getKickPoint( const PlayerAgent * agent )
{
    static const double base_x = -43.0;
    static const double base_y = 12.0;

    const WorldModel & wm = agent->world();

    std::vector< std::pair< Vector2D, double > > candidates;
    candidates.reserve( 4 );
    candidates.push_back( std::make_pair( Vector2D( base_x, base_y ), 0.0 ) );
    candidates.push_back( std::make_pair( Vector2D( base_x, -base_y ), 0.0 ) );
    candidates.push_back( std::make_pair( Vector2D( base_x, 0.0 ), 0.0 ) );
    /*
    for ( PlayerPtrCont::const_iterator o = wm.opponentsFromSelf().begin(),
              o_end = wm.opponentsFromSelf().end();
          o != o_end;
          ++o )
    {
        for ( std::vector< std::pair< Vector2D, double > >::iterator it = candidates.begin();
              it != candidates.end();
              ++it )
        {
            it->second += 1.0 / (*o)->pos().dist2( it->first );
        }
    }
    */
    std::vector< std::pair< Vector2D, double > > candidates_mate;
    for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromSelf().begin(),
              end = wm.teammatesFromSelf().end();
          t != end;
          ++t )
    {
        std::pair< Vector2D, double > mate = std::make_pair( (*t)->pos(), 0.0 );
        if ( wm.getOpponentNearestTo( (*t)->pos(),
                                      5,
                                      &mate.second ) )
        {
            candidates_mate.push_back( mate );
        }
    }

    for ( std::vector< std::pair< Vector2D, double > >::iterator it = candidates.begin(),
              it_end = candidates.end();
          it !=it_end;
          ++it )
    {
        double min_value = 10000.0;
        for ( std::vector< std::pair< Vector2D, double > >::iterator m = candidates_mate.begin(),
                  m_end = candidates_mate.end();
              m != m_end;
              ++m )
        {
            double value = it->first.dist( m->first ) / m->second;
            if ( value < min_value )
            {
                min_value = value;
            }
        }
        it->second = min_value;
    }

    Vector2D best_pos = candidates.front().first;
    double min_cong = 10000.0;
    for ( std::vector< std::pair< Vector2D, double > >::iterator it = candidates.begin(),
              end = candidates.end();
          it != end;
          ++it )
    {
        if ( it->second < min_cong )
        {
            best_pos = it->first;
            min_cong = it->second;
        }
    }

    return best_pos;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_GoalieFreeKick::doKick( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const double clear_y_threshold = SP.penaltyAreaHalfWidth() - 5.0;

    const WorldModel & wm = agent->world();

    const CooperativeAction & first_action = ActionChainHolder::i().graph().bestFirstAction();

    //
    // if side of field, clear the ball
    //
    if ( first_action.type() == CooperativeAction::Pass )
    {
        const Vector2D target_point = first_action.targetBallPos();

        dlog.addText( Logger::ROLE,
                      __FILE__":(doKick) self pos = [%.1f, %.1f], "
                      "pass target = [%.1f, %.1f]",
                      wm.self().pos().x, wm.self().pos().y,
                      target_point.x, target_point.y );

        if ( wm.self().pos().absY() > clear_y_threshold
             && ! ( target_point.y * wm.self().pos().y > 0.0
                    && target_point.absY() > wm.self().pos().absY() + 5.0
                    && target_point.x > wm.self().pos().x + 5.0 ) )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__":(doKick) self pos y %.2f is grater than %.2f,"
                          "pass target pos y = %.2f, clear ball",
                          wm.self().pos().absY(), clear_y_threshold,
                          target_point.y );
            agent->debugClient().addMessage( "SideForceClear" );

            Bhv_ClearBall().execute( agent );
            //agent->setNeckAction( new Neck_ScanField() );
            return;
        }

        if ( Bhv_ChainAction().execute( agent ) )
        {
            return;
        }
    }

    //
    // default clear ball
    //
    agent->debugClient().addMessage( "Clear" );
    dlog.addText( Logger::ROLE,
                  __FILE__":(doKick) no action, default clear ball" );

    Bhv_ClearBall().execute( agent );
    //agent->setNeckAction( new Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_GoalieFreeKick::doWait( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D face_target( 0.0, 0.0 );

    if ( wm.self().pos().x > ServerParam::i().ourPenaltyAreaLineX()
         - ServerParam::i().ballSize()
         - wm.self().playerType().playerSize()
         - 0.5 )
    {
        face_target.assign( ServerParam::i().ourPenaltyAreaLineX() - 1.0,
                            0.0 );
    }

    Body_TurnToPoint( face_target ).execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_GoalieFreeKick::doFreeKickMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    double tolerance = wm.ball().distFromSelf() * 0.1;
    if ( tolerance < 0.7 ) tolerance = 0.7;
    if ( wm.gameMode().type() == GameMode::CornerKick_ )
    {
        tolerance = 0.7;
    }

    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );

    agent->debugClient().addMessage( "FreeKickMove" );
    agent->debugClient().setTarget( home_pos );
    agent->debugClient().addCircle( home_pos, tolerance );

    if ( ! Body_GoToPoint( home_pos,
                           tolerance,
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
