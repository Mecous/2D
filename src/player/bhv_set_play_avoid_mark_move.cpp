// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa Akiyama

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

#include "bhv_set_play_avoid_mark_move.h"

#include "strategy.h"
#include "bhv_set_play.h"

#include "field_analyzer.h"

#include "intention_setplay_move.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/arm_point_to_point.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/say_message_builder.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/audio_memory.h>

#include <rcsc/math_util.h>

// #define DEBUG_PRINT

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlayAvoidMarkMove::execute( PlayerAgent * agent )
{

    dlog.addText( Logger::TEAM,
                  __FILE__":(execute)" );

    if ( isWaitPeriod( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": wait period" );
        return false;
    }

    if ( existOtherReceiver( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": exist other receiver" );
        return false;
    }

    if ( ! existOpponent( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": no opponent" );
        return false;
    }

    //
    // move
    //

    const WorldModel & wm = agent->world();

    //const Vector2D target_point = getTargetPointVoronoi( agent );
    const Vector2D target_point = getTargetPointOnCircle( agent );

    agent->debugClient().addMessage( "SetPlayAvoidMarkMove" );
    agent->debugClient().setTarget( target_point );

    const double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
    const double dist_thr = std::max( 1.0, wm.ball().distFromSelf() * 0.085 );

    dlog.addText( Logger::TEAM,
                  __FILE__": target=(%.1f %.1f) dash_power=%.1f dist_thr=%.2f",
                  target_point.x, target_point.y, dash_power, dist_thr );


    if ( ! Body_GoToPoint( target_point, dist_thr, dash_power ).execute( agent ) )
    {
        double kicker_ball_dist = ( ! wm.teammatesFromBall().empty()
                                    ? wm.teammatesFromBall().front()->distFromBall()
                                    : 1000.0 );
        if ( kicker_ball_dist > 1.0 )
        {
            agent->doTurn( 120.0 );
        }
        else
        {
            Body_TurnToBall().execute( agent );
        }
    }

    agent->setNeckAction( new Neck_ScanField() );
    agent->setArmAction( new Arm_PointToPoint( target_point ) );
    agent->setIntention( new IntentionSetplayMove( target_point, 20, wm.time() ) );

    //doSay( agent, target_point );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_SetPlayAvoidMarkMove::doSay( PlayerAgent * agent,
                                 const Vector2D & target_point )
{
    const WorldModel & wm = agent->world();

    if ( ! wm.self().staminaModel().capacityIsEmpty() )
    {
        if ( wm.self().stamina() < ServerParam::i().staminaMax() * 0.7
             || wm.self().staminaCapacity() < ServerParam::i().staminaCapacity() * Strategy::get_remaining_time_rate( wm )
             || wm.self().inertiaFinalPoint().dist( target_point ) > wm.ball().pos().dist( target_point ) * 0.2 + 3.0 )
        {
            agent->debugClient().addMessage( "SayWait" );
            agent->addSayMessage( new WaitRequestMessage() );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlayAvoidMarkMove::isWaitPeriod( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.audioMemory().setplayTime() == wm.time()
         && ! wm.audioMemory().setplay().empty() )
    {
        const int wait_step = wm.audioMemory().setplay().front().wait_step_;

        if ( wait_step > 0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(isWaitPeriod) wait_ste=%d", wait_step );
            return false;
        }
    }

    const int time_buf = ( wm.gameMode().type() == GameMode::GoalKick_
                           ? 35
                           : 15 );

    if ( wm.getSetPlayCount() > ServerParam::i().dropBallTime() - time_buf )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(isWaitPeriod) drop ball is coming. setplay count=%d",
                      wm.getSetPlayCount() );
        return false;
    }

    // if ( checkRecoverSituation( agent ) )
    // {
    //     dlog.addText( Logger::TEAM,
    //                   __FILE__": recover" );
    //     return true;
    // }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlayAvoidMarkMove::checkRecoverSituation( const PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    if ( wm.self().staminaModel().capacityIsEmpty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(checkRecoverSituation) no stamina capacity" );
        return false;
    }

    if ( wm.self().stamina() < SP.staminaMax() * 0.7 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(checkRecoverSituation) stamina check. try recovering" );
        return true;
    }

    const double time_rate = Strategy::get_remaining_time_rate( wm );
    const double capacity_thr = SP.staminaCapacity() * time_rate;
    if ( wm.self().staminaCapacity() < capacity_thr )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(checkRecoverSituation) stamina capacity check. capacity=%.1f < thr=%.1f (time_rate=%.3f)",
                      wm.self().staminaCapacity(), capacity_thr, time_rate );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlayAvoidMarkMove::existOtherReceiver( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );

    Vector2D target_point = home_pos;

    if ( home_pos.dist2( wm.ball().pos() ) < std::pow( 10.0, 2 )  )
    {
        target_point = wm.ball().pos() + ( home_pos - wm.ball().pos() ).setLengthVector( 7.0 );
    }


    AngleDeg pass_angle( ( target_point - wm.ball().pos() ).th() );
    Sector2D pass_sector( wm.ball().pos(),
                          0.0,
                          target_point.dist( wm.ball().pos() ),
                          pass_angle - 30.0,
                          pass_angle + 30.0 );

    for ( PlayerObject::Cont::const_iterator t = wm.teammatesFromSelf().begin(),
              end = wm.teammatesFromSelf().end();
          t != end;
          ++t )
    {
        if ( pass_sector.contains( (*t)->pos() ) )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(existOtherReceiver) exist other teammate in the pass sector" );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlayAvoidMarkMove::existOpponent( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const double ball_move_dist = wm.ball().pos().dist( wm.self().pos() );
    const int ball_move_step = ServerParam::i().ballMoveStep( ServerParam::i().kickPowerRate() * ServerParam::i().maxPower(),
                                                               ball_move_dist );

    for ( PlayerObject::Cont::const_iterator p = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          p != end;
          ++p )
    {
        if ( (*p)->distFromSelf() > 5.0 ) break;

        const PlayerType * ptype = (*p)->playerTypePtr();
        double control_area = ptype->kickableArea();

        double dash_dist = (*p)->pos().dist( wm.self().pos() );
        dash_dist -= control_area;
        int n_step = ptype->cyclesToReachDistance( dash_dist );
        n_step += 1;

        if ( n_step <= ball_move_step )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(existOpponent) exist ball_step=%d opponent_dash_step=%d",
                          ball_move_step, n_step );
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
Bhv_SetPlayAvoidMarkMove::getTargetPointVoronoi( const PlayerAgent * agent )
{
    static GameTime s_update_time( -1, 0 );
    static Vector2D s_best_pos = Vector2D::INVALIDATED;
    static std::vector< Vector2D > s_candidates;


    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    if ( home_pos.dist2( wm.ball().pos() ) < std::pow( 7.0, 2 )  )
    {
        home_pos = wm.ball().pos() + ( home_pos - wm.ball().pos() ).setLengthVector( 7.0 );
    }

    //
    // update position
    //
    if ( s_update_time != wm.time() )
    {
        const Vector2D goal_pos = SP.theirTeamGoalPos();
        const double max_x = SP.pitchHalfLength() - 2.0;
        const double max_y = SP.pitchHalfWidth() - 2.0;
        const double pen_x = SP.ourPenaltyAreaLineX();
        const double pen_y = SP.penaltyAreaHalfWidth();
        const double home_dist_thr2 = std::pow( 10.0, 2 );

        Vector2D best_pos = home_pos;
        double best_score = -1000000.0;

        FieldAnalyzer::i().positioningVoronoiDiagram().getPointsOnSegments( 2.0, // min length
                                                                            10, // max division
                                                                            &s_candidates );

        for ( std::vector< Vector2D >::const_iterator p = s_candidates.begin(), end = s_candidates.end();
              p != end;
              ++p )
        {
            // filter
            if ( p->dist2( home_pos ) > home_dist_thr2 ) continue;
            if ( p->absX() > max_x || p->absY() > max_y ) continue;
            if ( p->x < pen_x && p->absY() < pen_y ) continue;

            // evaluation
            double score = 0.0;
            switch ( wm.gameMode().type() ) {
            case GameMode::IndFreeKick_:
                score = 1.0 / ( p->dist( goal_pos ) * std::max( p->dist( home_pos ), 0.1 ) );
                break;
            case GameMode::GoalieCatch_:
                score = wm.getDistOpponentNearestTo( (*p), 5 );
                break;
            default:
                score = wm.getDistOpponentNearestTo( *p, 5 ) / std::max( p->absX(), 0.1 );
                break;
            }

            // update
            if ( score > best_score )
            {
                best_pos = *p;
                best_score = score;
            }
        }

        s_update_time = wm.time();
        s_best_pos = best_pos;
    }

    Vector2D target_point = ( s_best_pos.isValid()
                              ? s_best_pos
                              : home_pos );

    target_point.x = bound( -SP.pitchHalfLength(),
                            target_point.x,
                            +SP.pitchHalfLength() );
    target_point.y = bound( -SP.pitchHalfWidth(),
                            target_point.y,
                            +SP.pitchHalfWidth() );

    return target_point;
}

/*-------------------------------------------------------------------*/
/*!

*/
Vector2D
Bhv_SetPlayAvoidMarkMove::getTargetPointOnCircle( const PlayerAgent * agent )
{
    static GameTime s_update_time( -1, 0 );
    static Vector2D s_best_pos = Vector2D::INVALIDATED;

    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    if ( s_update_time == wm.time() )
    {
        return s_best_pos;
    }
    s_update_time = wm.time();

    Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    if ( home_pos.dist2( wm.ball().pos() ) < std::pow( 7.0, 2 )  )
    {
        home_pos = wm.ball().pos() + ( home_pos - wm.ball().pos() ).setLengthVector( 7.0 );
    }

    const double ball_dist = wm.ball().pos().dist( home_pos );
    const AngleDeg home_angle = ( home_pos - wm.ball().pos() ).th();

    const Vector2D goal_pos = SP.theirTeamGoalPos();
    const double max_x = SP.pitchHalfLength() - 2.0;
    const double max_y = SP.pitchHalfWidth() - 2.0;
    const double pen_x = SP.ourPenaltyAreaLineX();
    const double pen_y = SP.penaltyAreaHalfWidth();
    const double home_dist_thr2 = std::pow( 10.0, 2 );

    Vector2D best_pos = home_pos;
    double best_score = -1000000.0;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM,
                  __FILE__"(getTargetPointOnCircle) start" );
#endif

    const int angle_divs = 60;
    for ( int a = 0; a < angle_divs; ++a )
    {
        const AngleDeg angle = home_angle + ( 360.0/angle_divs * a );
        const Vector2D pos = wm.ball().pos() + Vector2D::from_polar( ball_dist, angle );

        // filter
        if ( pos.dist2( home_pos ) > home_dist_thr2 ) continue;
        if ( pos.absX() > max_x || pos.absY() > max_y ) continue;
        if ( pos.x < pen_x && pos.absY() < pen_y ) continue;

        // evaluation
        double score = 0.0;
        switch ( wm.gameMode().type() ) {
        case GameMode::IndFreeKick_:
            score = 1.0 / ( pos.dist( goal_pos ) * std::max( pos.dist( home_pos ), 0.1 ) );
            break;
        case GameMode::GoalieCatch_:
            score = wm.getDistOpponentNearestTo( pos, 5 );
            break;
        default:
            score = wm.getDistOpponentNearestTo( pos, 5 ) / std::max( pos.absX(), 0.1 );
            break;
        }

#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      "%d: pos=(%.2f %.2f) angle=%.1f score=%f",
                      a, pos.x, pos.y, angle.degree(), score );
#endif

        // update
        if ( score > best_score )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM, "%d: updated", a );
#endif
            best_pos = pos;
            best_score = score;
        }
    }

    s_best_pos = best_pos;
    return s_best_pos;
}
