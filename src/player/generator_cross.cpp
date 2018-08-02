// -*-c++-*-

/*!
  \file generator_cross.cpp
  \brief cross pass generator Source File
*/

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

#include "generator_cross.h"

#include "field_analyzer.h"

#include <rcsc/action/kick_table.h>

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/soccer_math.h>
#include <rcsc/timer.h>

// #define USE_POINTTO

// #define DEBUG_PROFILE
// #define DEBUG_PRINT
// #define DEBUG_PRINT_SUCCESS_COURSE
// #define DEBUG_PRINT_FAILED_COURSE
// #define DEBUG_PRINT_EVAL

// #define DEBUG_PRINT_RECEIVER

using namespace rcsc;

namespace {

inline
void
debug_paint_failed( const int count,
                    const Vector2D & receive_point,
                    const int step_count )
{
    dlog.addRect( Logger::CROSS,
                  receive_point.x - 0.1, receive_point.y - 0.1,
                  0.2, 0.2,
                  "#F00" );
    char num[8];
    snprintf( num, 8, "%d", count );
    dlog.addMessage( Logger::CROSS,
                     receive_point.x, receive_point.y + 0.15*step_count, num );
}

inline
void
debug_paint_result( const int count,
                    const Vector2D & receive_point,
                    const int safe,
                    const int step_count )
{
    dlog.addRect( Logger::CROSS,
                  receive_point.x - 0.05, receive_point.y - 0.05,
                  0.1, 0.1,
                  "#0F0" );
    char num[8];
    snprintf( num, 8, "%d:%d", count, safe );
    dlog.addMessage( Logger::CROSS,
                     receive_point.x, receive_point.y + 0.15*step_count, num, "#0F0" );
}

struct CrossCompare {

    bool operator()( const GeneratorCross::Course & lhs,
                     const GeneratorCross::Course & rhs ) const
      {
          return lhs.value_ > rhs.value_;
      }
};

}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorCross::GeneratorCross()
{
    M_dash_line_courses.reserve( 64 );
    M_courses.reserve( 64 );

    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorCross &
GeneratorCross::instance()
{
    static GeneratorCross s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCross::clear()
{
    M_total_count = 0;
    M_passer = static_cast< const AbstractPlayerObject * >( 0 );
    M_first_point.invalidate();
    M_receiver_candidates.clear();
    M_opponents.clear();
    M_dash_line_courses.clear();
    M_courses.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCross::generate( const WorldModel & wm )
{
    static GameTime s_update_time( -1, 0 );
    if ( s_update_time == wm.time() )
    {
        return;
    }
    s_update_time = wm.time();

    clear();

    if ( wm.time().stopped() > 0
         || wm.gameMode().isPenaltyKickMode() )
    {
        return;
    }

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    updatePasser( wm );

    if ( ! M_passer
         || ! M_first_point.isValid() )
    {
        dlog.addText( Logger::CROSS,
                      __FILE__" (generate) passer not found." );
        return;
    }

    if ( ServerParam::i().theirTeamGoalPos().dist( M_first_point ) > 35.0 )
    {
        dlog.addText( Logger::CROSS,
                      __FILE__" (generate) first point(%.1f %.1f) is too far from the goal.",
                      M_first_point.x, M_first_point.y );
        return;
    }

    updateReceivers( wm );

    if ( M_receiver_candidates.empty() )
    {
        dlog.addText( Logger::CROSS,
                      __FILE__" (generate) no receiver." );
        return;
    }

    updateOpponents( wm );

    createCourses( wm );
    evaluateCourses( wm );

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::CROSS,
                  __FILE__" (generate) PROFILE course_size=%d/%d elapsed %f [ms]",
                  (int)M_courses.size(),
                  M_total_count,
                  timer.elapsedReal() );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCross::updatePasser( const WorldModel & wm )
{
    if ( wm.self().isKickable()
         && ! wm.self().isFrozen() )
    {
        M_passer = &wm.self();
        M_first_point = wm.ball().pos();
#ifdef DEBUG_UPDATE_PASSER
        dlog.addText( Logger::CROSS,
                      __FILE__" (updatePasser) self kickable." );
#endif
        return;
    }

    int self_step = wm.interceptTable()->selfReachCycle();
    int teammate_step = wm.interceptTable()->teammateReachCycle();
    int opponent_step = wm.interceptTable()->opponentReachCycle();

    int our_step = std::min( self_step, teammate_step );
    if ( opponent_step < std::min( our_step - 4, (int)rint( our_step * 0.9 ) ) )
    {
#ifdef DEBUG_UPDATE_PASSER
        dlog.addText( Logger::CROSS,
                      __FILE__" (updatePasser) opponent ball." );
#endif
        return;
    }

    if ( self_step <= teammate_step )
    {
        if ( self_step <= 2 )
        {
            M_passer = &wm.self();
            M_first_point = wm.ball().inertiaPoint( self_step );
        }
    }
    else
    {
        if ( teammate_step <= 2 )
        {
            M_passer = wm.interceptTable()->fastestTeammate();
            M_first_point = wm.ball().inertiaPoint( teammate_step );
        }
    }

    if ( ! M_passer )
    {
#ifdef DEBUG_UPDATE_PASSER
        dlog.addText( Logger::CROSS,
                      __FILE__" (updatePasser) no passer." );
#endif
        return;
    }

    if ( M_passer->unum() != wm.self().unum() )
    {
        if ( M_first_point.dist2( wm.self().pos() ) > std::pow( 20.0, 2 ) )
        {
            M_passer = static_cast< const AbstractPlayerObject * >( 0 );
#ifdef DEBUG_UPDATE_PASSER
            dlog.addText( Logger::CROSS,
                          __FILE__" (updatePasser) passer is too far." );
#endif
            return;
        }
    }

#ifdef DEBUG_UPDATE_PASSER
    dlog.addText( Logger::CROSS,
                  __FILE__" (updatePasser) passer=%d(%.1f %.1f) reachStep=%d startPos=(%.1f %.1f)",
                  M_passer->unum(),
                  M_passer->pos().x, M_passer->pos().y,
                  teammate_step,
                  M_first_point.x, M_first_point.y );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCross::updateReceivers( const WorldModel & wm )
{
    static const double shootable_dist2 = std::pow( 16.0, 2 ); // Magic Number
    static const double min_cross_dist2
        = std::pow( ServerParam::i().defaultKickableArea() * 2.2, 2 );
    static const double max_cross_dist2
        = std::pow( inertia_n_step_distance( ServerParam::i().ballSpeedMax(),
                                             10,
                                             ServerParam::i().ballDecay() ),
                    2 );

    const Vector2D goal = ServerParam::i().theirTeamGoalPos();

    const bool is_self_passer = ( M_passer->unum() == wm.self().unum() );

#ifdef DEBUG_PRINT_RECEIVER
    dlog.addText( Logger::CROSS,
                  __FILE__"(updateReceivers) min_dist=%.3f max_dist=%.3f",
                  std::sqrt( min_cross_dist2 ),
                  std::sqrt( max_cross_dist2 ) );
#endif

    for ( AbstractPlayerObject::Cont::const_iterator p = wm.ourPlayers().begin(),
              end = wm.ourPlayers().end();
          p != end;
          ++p )
    {
        if ( *p == M_passer ) continue;
        if ( (*p)->isTackling() ) continue;

        if ( is_self_passer )
        {
            if ( (*p)->isGhost() ) continue;
            if ( (*p)->posCount() >= 4 ) continue;
            if ( (*p)->pos().x > wm.offsideLineX() ) continue;
        }
        else
        {
            // ignore other players
            if ( (*p)->unum() != wm.self().unum() )
            {
                continue;
            }
        }

        if ( (*p)->pos().dist2( goal ) < shootable_dist2
             || ( (*p)->pos().x > 35.0
                  && (*p)->pos().absY() < 12.0 ) )
        {
            // ok
        }
        else
        {
            continue;
        }

        double d2 = (*p)->pos().dist2( M_passer->pos() );
        if ( d2 < min_cross_dist2 ) continue;
        if ( max_cross_dist2 < d2 ) continue;

        M_receiver_candidates.push_back( *p );

#ifdef DEBUG_PRINT_RECEIVER
        dlog.addText( Logger::CROSS,
                      "Cross receiver %d pos(%.1f %.1f)",
                      (*p)->unum(),
                      (*p)->pos().x, (*p)->pos().y );
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCross::updateOpponents( const WorldModel & wm )
{
    const double opponent_dist_thr2 = std::pow( 20.0, 2 );

    const Vector2D goal = ServerParam::i().theirTeamGoalPos();
    const AngleDeg goal_angle_from_ball = ( goal - M_first_point ).th();

    for ( AbstractPlayerObject::Cont::const_iterator p = wm.theirPlayers().begin(),
              end = wm.theirPlayers().end();
          p != end;
          ++p )
    {
        AngleDeg opponent_angle_from_ball = ( (*p)->pos() - M_first_point ).th();
        if ( ( opponent_angle_from_ball - goal_angle_from_ball ).abs() > 90.0 )
        {
            continue;
        }

        if ( (*p)->goalie() )
        {
            if ( (*p)->pos().dist2( M_first_point ) > std::pow( 30.0, 2 ) )
            {
                continue;
            }
        }
        else
        {
            if ( (*p)->pos().dist2( M_first_point ) > opponent_dist_thr2 )
            {
                continue;
            }
        }

        M_opponents.push_back( *p );

#ifdef DEBUG_PRINT
        dlog.addText( Logger::CROSS,
                      "Cross opponent %d pos(%.1f %.1f)",
                      (*p)->unum(),
                      (*p)->pos().x, (*p)->pos().y );
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCross::createCourses( const WorldModel & wm )
{
    for ( AbstractPlayerObject::Cont::const_iterator p = M_receiver_candidates.begin();
          p != M_receiver_candidates.end();
          ++p )
    {
        createCrossOnDashLine( wm, *p );
        createCross( wm, *p );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCross::evaluateCourses( const WorldModel & wm )
{
    const double factor = 2.0 * std::pow( 1.5, 2 );

    for ( Cont::iterator it = M_courses.begin(), end = M_courses.end();
          it != end;
          ++it )
    {
#ifdef DEBUG_PRINT_EVAL
        dlog.addText( Logger::CROSS, "-----" );
#endif
        const double pass_dist = it->action_->targetBallPos().dist( wm.ball().pos() );
        const AngleDeg pass_angle = it->action_->firstBallVel().th();

        double score = 1000.0;
        double min_dir_diff = 180.0;

        if ( it->action_->safetyLevel() == CooperativeAction::Safe )
        {
            const AbstractPlayerObject * player = wm.ourPlayer( it->action_->targetPlayerUnum() );
            if ( player )
            {
                score -= it->action_->targetBallPos().dist( player->pos() ) * 0.01;
#ifdef DEBUG_PRINT_EVAL
                dlog.addText( Logger::CROSS,
                              "Cross %d: (eval) SAFE receiver move dist %.3f (%.3f)",
                              it->action_->index(),
                              -it->action_->targetBallPos().dist( player->pos() ) * 0.01,
                              score );
#endif
            }
            else
            {
                score -= 1.0;
#ifdef DEBUG_PRINT_EVAL
                dlog.addText( Logger::CROSS,
                              "Cross %d: (eval) SAFE null receiver %.3f (%.3f)",
                              it->action_->index(),
                              -1.0, score );
#endif
            }
        }
        else
        {
            for ( AbstractPlayerObject::Cont::const_iterator p = wm.theirPlayers().begin(),
                      end = wm.theirPlayers().end();
                  p != end;
                  ++p )
            {
                if ( (*p)->distFromBall() > pass_dist ) continue;
                double dir_diff = ( pass_angle - (*p)->angleFromBall() ).abs();
                if ( dir_diff < min_dir_diff )
                {
                    min_dir_diff = dir_diff;
                }
            }
        }
        score += min_dir_diff;
#ifdef DEBUG_PRINT_EVAL
        dlog.addText( Logger::CROSS,
                      "Cross %d: (eval) dir_diff %.3f (%.3f)",
                      it->action_->index(),
                      min_dir_diff, score );
#endif

        score *= std::pow( 0.9, std::max( 0, it->action_->kickCount() - 1 ) );
#ifdef DEBUG_PRINT_EVAL
        dlog.addText( Logger::CROSS,
                      "Cross %d: (eval) kick count penalty rate %.3f (%.3f)",
                      it->action_->index(),
                      std::pow( 0.9, std::max( 0, it->action_->kickCount() - 1 ) ),
                      score );
#endif

        double opp_rate = 1.0;
        for ( AbstractPlayerObject::Cont::const_iterator p = wm.theirPlayers().begin(),
                  end = wm.theirPlayers().end();
              p != end;
              ++p )
        {
            double d2 = it->action_->targetBallPos().dist2( (*p)->pos() );
            opp_rate *= 1.0 - 0.5 * std::exp( - d2 / factor );
        }
        score *= opp_rate;
#ifdef DEBUG_PRINT_EVAL
        dlog.addText( Logger::CROSS,
                      "Cross %d: (eval) opponent dist rate %.3f (%.3f)",
                      it->action_->index(),
                      opp_rate, score );
#endif

        switch ( it->action_->safetyLevel() ) {
        case CooperativeAction::Dangerous:
            score *= 0.001;
#ifdef DEBUG_PRINT_EVAL
            dlog.addText( Logger::CROSS,
                          "Cross %d: (eval) dangerous rate 0.001 (%.3f)",
                          it->action_->index(), score );
#endif
            break;
        case CooperativeAction::MaybeDangerous:
#ifdef DEBUG_PRINT_EVAL
            dlog.addText( Logger::CROSS,
                          "Cross %d: (eval) maybe dangerous rate 0.5 (%.3f)",
                          it->action_->index(), score );
#endif
            score *= 0.5;
            break;
        case CooperativeAction::Safe:
        default:
            break;
        }

        it->value_ = score;
#ifdef DEBUG_PRINT_EVAL
        dlog.addText( Logger::CROSS,
                      "Cross %d: eval %f target[%d](%.1f %.1f) dir_diff=%.1f safe=%d kick=%d opp_rate=%f",
                      it->action_->index(),
                      score,
                      it->action_->targetPlayerUnum(),
                      it->action_->targetBallPos().x, it->action_->targetBallPos().y,
                      min_dir_diff,
                      it->action_->safetyLevel(),
                      it->action_->kickCount(),
                      opp_rate );
#endif
    }

    std::sort( M_courses.begin(), M_courses.end(), CrossCompare() );

    //
    // TODO: leave 1 candidate for 1 target player
    //
    if ( M_courses.size() > 1 )
    {
        M_courses.erase( M_courses.begin() + 1, M_courses.end() );
    }

#ifdef DEBUG_PRINT_EVAL
    dlog.addText( Logger::CROSS, "-----" );

    if ( ! M_courses.empty() )
    {
        dlog.addLine( Logger::CROSS,
                      wm.ball().pos(), M_courses.front().action_->targetBallPos() );
        Vector2D bpos = wm.ball().pos();
        Vector2D bvel = M_courses.front().action_->firstBallVel();
        const int move_step = M_courses.front().action_->durationTime() - M_courses.front().action_->kickCount();
        for ( int i = 0; i < move_step; ++i )
        {
            bpos += bvel;
            bvel *= ServerParam::i().ballDecay();
            dlog.addRect( Logger::CROSS,
                          bpos.x - 0.05, bpos.y - 0.05, 0.1, 0.1, "#0F0" );
        }
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCross::createCrossOnDashLine( const WorldModel & wm,
                                       const AbstractPlayerObject * receiver )
{
#ifdef USE_POINTTO
    if ( receiver->pointtoCount() > 2
         && ( receiver->bodyCount() > 0
              || receiver->body().abs() > 50.0 ) )
    {
        return;
    }
#else
    if ( receiver->bodyCount() > 1
         || receiver->body().abs() > 50.0 )
    {
        return;
    }
#endif

    const ServerParam & SP = ServerParam::i();
    const PlayerType * ptype = receiver->playerTypePtr();

    Vector2D receiver_pos = receiver->pos();
    Vector2D receiver_vel = receiver->vel();

    AngleDeg dash_angle = receiver->body();
#ifdef USE_POINTTO
    if ( receiver->pointtoCount() == 0
         && receiver->bodyCount() == 0 )
    {
        if ( ( receiver->body() - receiver->pointtoAngle() ).abs() < 10.0 )
        {
            dlog.addText( Logger::CROSS, __FILE__":(DashLine) body dir (1)" );
            dash_angle = receiver->body();
        }
        else
        {
            dlog.addText( Logger::CROSS, __FILE__":(DashLine) pointto dir (1)" );
            dash_angle = receiver->pointtoAngle();
        }
    }
    else if ( receiver->bodyCount() == 0 )
    {
        dlog.addText( Logger::CROSS, __FILE__":(DashLine) body dir (2)" );
        dash_angle == receiver->body();
    }
    else
    {
        dlog.addText( Logger::CROSS, __FILE__":(DashLine) pointto dir (2)" );
        dash_angle = receiver->pointtoAngle();
    }
#endif

    const Vector2D body_vec = Vector2D::from_polar( 1.0, dash_angle );
    const Vector2D dash_accel = body_vec * ( SP.maxDashPower()
                                             * ptype->dashPowerRate()
                                             * ptype->effortMax() );
    const double trap_dist = ptype->playerSize() + SP.ballSize() + 0.1;
    //const Vector2D trap_pos = body_vec * ( ptype->playerSize() + SP.ballSize() + 0.1 );
    //const Vector2D trap_pos = body_vec * 0.1;

    receiver_pos += receiver_vel;
    receiver_vel *= ptype->playerDecay();

    for ( int n_dash = 2; n_dash <= 10; ++n_dash )
    {
        ++M_total_count;

        receiver_vel += dash_accel;
        receiver_pos += receiver_vel;
        receiver_vel *= ptype->playerDecay();

        //const Vector2D target_point = receiver_pos + trap_pos;
        const Vector2D trap_pos = body_vec * ( trap_dist - ( 0.1 * ( n_dash - 1 ) ) );
        const Vector2D target_point = receiver_pos + trap_pos;

        if ( target_point.x > SP.pitchHalfLength() - 1.0 )
        {
            break;
        }

        if ( target_point.x > 49.0
             && target_point.absY() > SP.goalHalfWidth() + 2.0 )
        {
            break;
        }

        const double ball_move_dist = wm.ball().pos().dist( target_point );

        if ( ball_move_dist < 2.0 )
        {
            break;
        }
        if ( target_point.absY() > SP.goalHalfWidth()
             && wm.ball().pos().absY() > SP.goalHalfWidth()
             && target_point.y * wm.ball().pos().y > 0.0 // same side
             && target_point.absY() > wm.ball().pos().absY() )
        {
            break;
        }

        const AngleDeg ball_move_angle = ( target_point - wm.ball().pos() ).th();

        const Vector2D max_one_step_vel = ( M_passer->isSelf()
                                            ? KickTable::calc_max_velocity( ball_move_angle,
                                                                            wm.self().kickRate(),
                                                                            wm.ball().vel() )
                                            : Vector2D::from_polar( SP.ballSpeedMax(), ball_move_angle ) );
        const double max_one_step_speed = max_one_step_vel.r();

        int kick_count = 1;
        int ball_move_step = n_dash;
        double first_ball_speed = SP.firstBallSpeed( ball_move_dist, ball_move_step );

        while ( first_ball_speed > SP.ballSpeedMax() )
        {
            ++ball_move_step;
            first_ball_speed = SP.firstBallSpeed( ball_move_dist, ball_move_step );
        }

#ifdef DEBUG_PRINT
        dlog.addText( Logger::CROSS,
                      "%d: n_dash=%d (%.2f %.2f) first_ball_speed=%.3f one_step_speed=%.3f move_step=%d",
                      M_total_count, n_dash, target_point.x, target_point.y,
                      first_ball_speed, max_one_step_speed, ball_move_step );
#endif

        if ( first_ball_speed > max_one_step_speed
             && first_ball_speed * 0.96 < max_one_step_speed )
        {
            first_ball_speed = max_one_step_speed;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CROSS,
                          "%d: first_ball_speed*0.96 < one_step_speed. changed to one step kick",
                          M_total_count );
#endif
        }

        CooperativeAction::SafetyLevel safety_level = CooperativeAction::Failure;

        //
        // player cannot kick the ball by 1 step
        //
        if ( first_ball_speed > max_one_step_speed + 1.0e-5 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CROSS,
                          "%d: no 1step kick. first_ball_speed=%.3f max_one_step_speed=%.3f",
                          M_total_count, first_ball_speed, max_one_step_speed );
#endif
            //
            // try 1 step kick
            //
            int one_kick_ball_move_step = SP.ballMoveStep( max_one_step_speed, ball_move_dist );
            if ( one_kick_ball_move_step > 0 )
            {
                double one_kick_first_ball_speed = SP.firstBallSpeed( ball_move_dist, one_kick_ball_move_step );
#ifdef DEBUG_PRINT
                dlog.addText( Logger::CROSS,
                              "%d: try 1step kick. first_ball_speed=%.3f move_step=%d",
                              M_total_count, one_kick_first_ball_speed, one_kick_ball_move_step );
#endif

                CooperativeAction::SafetyLevel one_step_safety_level = getSafetyLevel( one_kick_first_ball_speed,
                                                                                       ball_move_angle,
                                                                                       1, // kick count
                                                                                       one_kick_ball_move_step );

                if ( one_step_safety_level > CooperativeAction::Dangerous )
                {
                    ball_move_step = one_kick_ball_move_step;
                    first_ball_speed = one_kick_first_ball_speed;
                    safety_level = one_step_safety_level;
#ifdef DEBUG_PRINT
                    dlog.addText( Logger::CROSS,
                                  "%d: adjust to 1step kick. first_ball_speed=%.3f move_step=%d",
                                  M_total_count, first_ball_speed, ball_move_step );
#endif
                }
                else
                {
                    //
                    // adjust first ball speed
                    //
                    double two_kick_speed = std::min( SP.ballSpeedMax(),
                                                      SP.firstBallSpeed( ball_move_dist, ball_move_step - 1 ) );
                    kick_count = 2;
                    --ball_move_step;
                    first_ball_speed = two_kick_speed;
#ifdef DEBUG_PRINT
                    dlog.addText( Logger::CROSS,
                                  "%d: update to first_ball_speed=%.3f move_step=%d",
                                  M_total_count, first_ball_speed, ball_move_step );
#endif
                }
            }
        }

        if ( safety_level == CooperativeAction::Failure )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CROSS,
                          "%d: check safety level. bspeed=%.3f n_kick=%d move_step=%d",
                          M_total_count, first_ball_speed, kick_count, ball_move_step );
#endif
            safety_level = getSafetyLevel( first_ball_speed, ball_move_angle, kick_count, ball_move_step );
        }

        if ( safety_level != CooperativeAction::Failure )
        {
            Vector2D first_ball_vel = ( target_point - M_first_point ).setLengthVector( first_ball_speed );

            CooperativeAction::Ptr ptr( new ActPass( M_passer->unum(),
                                                     receiver->unum(),
                                                     target_point,
                                                     first_ball_vel,
                                                     kick_count - 1 + ball_move_step,
                                                     kick_count,
                                                     "crossOnDash" ) );
            ptr->setIndex( M_total_count );
            ptr->setMode( ActPass::CROSS );
            ptr->setSafetyLevel( safety_level );
            M_dash_line_courses.push_back( Course( ptr ) );
            M_courses.push_back( Course( ptr ) );
#ifdef DEBUG_PRINT_SUCCESS_COURSE
            dlog.addText( Logger::CROSS,
                          "%d: ok CrossOnDash step=%d kick=%d pos=(%.1f %.1f) speed=%.3f safe=%d",
                          M_total_count,
                          ball_move_step, kick_count,
                          target_point.x, target_point.y,
                          first_ball_speed,
                          safety_level );
            debug_paint_result( M_total_count, target_point, safety_level, 0 );
#endif
        }
#ifdef DEBUG_PRINT_FAILED_COURSE
        else
        {
            dlog.addText( Logger::CROSS,
                          "%d: xxx CrossOnDash step=%d kick=%d pos=(%.1f %.1f) speed=%.3f",
                          M_total_count,
                          ball_move_step, kick_count,
                          target_point.x, target_point.y,
                          first_ball_speed );
            debug_paint_failed( M_total_count, target_point, 0 );
        }
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCross::createCross( const WorldModel & wm,
                             const AbstractPlayerObject * receiver )
{
    static const int MIN_RECEIVE_STEP = 2;
    static const int MAX_RECEIVE_STEP = 12; // Magic Number

    static const double MIN_RECEIVE_BALL_SPEED = ServerParam::i().defaultPlayerSpeedMax();

    static const double ANGLE_STEP = 2.5;
    static const double DIST_STEP = 0.9;

    const ServerParam & SP = ServerParam::i();

    const double min_first_ball_speed = SP.ballSpeedMax() * 0.67; // Magic Number
    const double max_first_ball_speed = ( wm.gameMode().type() == GameMode::PlayOn
                                          ? SP.ballSpeedMax()
                                          : wm.self().isKickable()
                                          ? wm.self().kickRate() * SP.maxPower()
                                          : SP.kickPowerRate() * SP.maxPower() );

    const PlayerType * ptype = receiver->playerTypePtr();
    const Vector2D receiver_pos = receiver->inertiaFinalPoint();
    const double receiver_dist = M_first_point.dist( receiver_pos );
    const AngleDeg receiver_angle_from_ball = ( receiver_pos - M_first_point ).th();

    //
    // angle loop
    //
    for ( int a = -2; a < 3; ++a )
    {
        const AngleDeg cross_angle = receiver_angle_from_ball + ( ANGLE_STEP * a );
        const Vector2D unit_vec = Vector2D::from_polar( 1.0, cross_angle );

        const Vector2D max_one_step_vel = ( M_passer->isSelf()
                                            ? KickTable::calc_max_velocity( cross_angle,
                                                                            wm.self().kickRate(),
                                                                            wm.ball().vel() )
                                            : Vector2D::from_polar( SP.ballSpeedMax(), cross_angle ) );
        const double max_one_step_speed = max_one_step_vel.r();

#ifdef DEBUG_PRINT
            dlog.addText( Logger::CROSS,
                          ">>>> receiver=%d angle=%.1f max_one_step_speed=%f",
                          receiver->unum(), cross_angle.degree(), max_one_step_speed );
#endif

        //
        // distance loop
        //
        for ( int d = 0; d < 5; ++d )
        {
            const double sub_dist = DIST_STEP * d;
            const double ball_move_dist = receiver_dist - sub_dist;
            const Vector2D receive_point = M_first_point + ( unit_vec * ball_move_dist );

#ifdef DEBUG_PRINT
            dlog.addText( Logger::CROSS,
                          "==== receiver=%d receivePos=(%.2f %.2f) loop=%d angle=%.1f dist=%.1f",
                          receiver->unum(),
                          receive_point.x, receive_point.y,
                          a, cross_angle.degree(), ball_move_dist );
#endif

            if ( receive_point.x > SP.pitchHalfLength() - 0.5
                 || receive_point.absY() > SP.pitchHalfWidth() - 3.0 )
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::CROSS,
                              "xxx unum=%d (%.2f %.2f) outOfBounds",
                              receiver->unum(), receive_point.x, receive_point.y );
                debug_paint_failed( M_total_count, receive_point, 0 );
#endif
                continue;
            }

            const int receiver_step = ptype->cyclesToReachDistance( sub_dist ) + 1;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CROSS,
                          "==== receiver=%d step=%d",
                          receiver->unum(), receiver_step );
#endif
            //
            // step loop
            //

#if defined (DEBUG_PRINT_SUCCESS_COURSE) || defined (DEBUG_PRINT_FAILED_COURSE)
            int step_count = 0;
#endif
            for ( int step = std::max( MIN_RECEIVE_STEP, receiver_step );
                  step <= MAX_RECEIVE_STEP;
                  ++step )
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::CROSS,
                              "---------- ball_step %d ----------", step );
#endif

                ++M_total_count;

                double first_ball_speed = SP.firstBallSpeed( ball_move_dist, step );
                if ( first_ball_speed < min_first_ball_speed )
                {
#ifdef DEBUG_PRINT_FAILED_COURSE
                    dlog.addText( Logger::CROSS,
                                  "%d: xxx unum=%d (%.1f %.1f) step=%d firstSpeed=%.3f < min=%.3f",
                                  M_total_count,
                                  receiver->unum(),
                                  receive_point.x, receive_point.y,
                                  step,
                                  first_ball_speed, min_first_ball_speed );
                    //debug_paint_failed( M_total_count, receive_point, step_count );
#endif
                    break;
                }

                if ( max_first_ball_speed < first_ball_speed )
                {
#ifdef DEBUG_PRINT_FAILED_COURSE
                    dlog.addText( Logger::CROSS,
                                  "%d: xxx unum=%d (%.1f %.1f) step=%d firstSpeed=%.3f > max=%.3f",
                                  M_total_count,
                                  receiver->unum(),
                                  receive_point.x, receive_point.y,
                                  step,
                                  first_ball_speed, max_first_ball_speed );
                    //debug_paint_failed( M_total_count, receive_point, step_count++ );
#endif
                    continue;
                }

                int kick_count = 1;
                if ( first_ball_speed > max_one_step_speed )
                {
                    if ( first_ball_speed < max_one_step_speed * 1.05 )
                    {
                        first_ball_speed = max_one_step_speed;
                    }
                    else
                    {
                        kick_count = 2;
                    }
                }

                double receive_ball_speed = first_ball_speed * std::pow( SP.ballDecay(), step );
                if ( receive_ball_speed < MIN_RECEIVE_BALL_SPEED )
                {
#ifdef DEBUG_PRINT_FAILED_COURSE
                    dlog.addText( Logger::CROSS,
                                  "%d: xxx unum=%d (%.1f %.1f) step=%d recvSpeed=%.3f < min=%.3f",
                                  M_total_count,
                                  receiver->unum(),
                                  receive_point.x, receive_point.y,
                                  step,
                                  receive_ball_speed, min_first_ball_speed );
                    //debug_paint_failed( M_total_count, receive_point, step_count );
#endif
                    break;
                }

                // int kick_count = FieldAnalyzer::predict_kick_count( wm, M_passer,
                //                                                     first_ball_speed, cross_angle );

                CooperativeAction::SafetyLevel safety_level = getSafetyLevel( first_ball_speed, cross_angle,
                                                                              kick_count, step );
                if ( safety_level != CooperativeAction::Failure )
                {
                    Vector2D first_ball_vel = ( receive_point - M_first_point ).setLengthVector( first_ball_speed );

                    CooperativeAction::Ptr ptr( new ActPass( M_passer->unum(),
                                                             receiver->unum(),
                                                             receive_point,
                                                             first_ball_vel,
                                                             kick_count - 1 + step,
                                                             kick_count,
                                                             "cross" ) );
                    ptr->setIndex( M_total_count );
                    ptr->setMode( ActPass::CROSS );
                    ptr->setSafetyLevel( safety_level );
                    M_courses.push_back( Course( ptr ) );
#ifdef DEBUG_PRINT_SUCCESS_COURSE
                    dlog.addText( Logger::CROSS,
                                  "%d: ok Cross step=%d kick=%d pos=(%.1f %.1f) speed=%.3f->%.3f safe=%d",
                                  M_total_count, step, kick_count,
                                  receive_point.x, receive_point.y,
                                  first_ball_speed, receive_ball_speed,
                                  safety_level );
                    debug_paint_result( M_total_count, receive_point, safety_level, step_count++ );
#endif
                }
                else
                {
#ifdef DEBUG_PRINT_FAILED_COURSE
                    dlog.addText( Logger::CROSS,
                                  "%d: xxx Cross step=%d kick=%d pos=(%.1f %.1f) speed=%.3f->%.3f",
                                  M_total_count,
                                  step, kick_count,
                                  receive_point.x, receive_point.y,
                                  first_ball_speed, receive_ball_speed );
                    debug_paint_failed( M_total_count, receive_point, step_count++ );
#endif
                    break;
                }
            }
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::SafetyLevel
GeneratorCross::getSafetyLevel( const double first_ball_speed,
                                const AngleDeg & ball_move_angle,
                                const int n_kick,
                                const int ball_step )
{
    const Vector2D first_ball_vel = Vector2D::polar2vector( first_ball_speed, ball_move_angle );

#ifdef DEBUG_PRINT
    dlog.addText( Logger::CROSS,
                  "%d: (getSafetyLevel) bspeed=%.3f angle=%.1f nKick=%d bstep=%d",
                  M_total_count, first_ball_speed, ball_move_angle.degree(), n_kick, ball_step );
#endif

    CooperativeAction::SafetyLevel result = CooperativeAction::Safe;

    for ( AbstractPlayerObject::Cont::const_iterator o = M_opponents.begin(), end = M_opponents.end();
          o != end;
          ++o )
    {
        CooperativeAction::SafetyLevel level = getOpponentSafetyLevel( *o, first_ball_vel, ball_move_angle, n_kick, ball_step );
        if ( result > level )
        {
            result = level;
            if ( result == CooperativeAction::Failure )
            {
                break;
            }
        }
    }

#ifdef DEBUG_PRINT
    dlog.addText( Logger::CROSS,
                  "%d: safe=%d",
                  M_total_count, result );
#endif
    return result;
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::SafetyLevel
GeneratorCross::getOpponentSafetyLevel( const AbstractPlayerObject * opponent,
                                        const Vector2D & first_ball_vel,
                                        const AngleDeg & ball_move_angle,
                                        const int n_kick,
                                        const int ball_step )
{
#ifdef DEBUG_PRINT
    dlog.addText( Logger::CROSS,
                  "%d: check opponent[%d]",
                  M_total_count, opponent->unum() );
#endif

    //if ( ( opponent->angleFromBall() - ball_move_angle ).abs() > 90.0 )
    if ( ( opponent->angleFromBall() - ball_move_angle ).abs() > 50.0 )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::CROSS,
                      "%d: opponent[%d](%.1f %.1f) backside. never reach.",
                      M_total_count,
                      opponent->unum(),
                      opponent->pos().x, opponent->pos().y );
#endif
        return CooperativeAction::Safe;
    }

    const ServerParam & SP = ServerParam::i();
    const PlayerType * ptype = opponent->playerTypePtr();

    int min_step = FieldAnalyzer::estimate_min_reach_cycle( opponent->pos(),
                                                            SP.catchableArea(),
                                                            ptype->realSpeedMax(),
                                                            M_first_point,
                                                            ball_move_angle );
    if ( min_step < 0 )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::CROSS,
                      "%d: opponent[%d](%.1f %.1f) negative min reach step",
                      M_total_count,
                      opponent->unum(),
                      opponent->pos().x, opponent->pos().y );
#endif
        return CooperativeAction::Safe;
    }

    if ( min_step == 0 )
    {
        min_step = 1;
    }

    //const int max_step = ball_step + ( n_kick - 1 );
    const double opponent_speed = opponent->vel().r();

    CooperativeAction::SafetyLevel result = CooperativeAction::Safe;

    Vector2D ball_pos = inertia_n_step_point( M_first_point,
                                              first_ball_vel,
                                              min_step - 1,
                                              SP.ballDecay() );
    Vector2D ball_vel = first_ball_vel * std::pow( SP.ballDecay(), min_step - 1 );
    for ( int step = min_step; step <= ball_step; ++step )
    {
        ball_pos += ball_vel;
        ball_vel *= SP.ballDecay();

        const double control_area = ( opponent->goalie()
                                      && ball_pos.x > SP.theirPenaltyAreaLineX()
                                      && ball_pos.absY() < SP.penaltyAreaHalfWidth()
                                      ? SP.catchableArea()
                                      : ptype->kickableArea() );
        const Vector2D opponent_pos = opponent->inertiaPoint( step );
        const double ball_dist = opponent_pos.dist( ball_pos );
        double dash_dist = ball_dist - control_area - 0.15; // buffer for KickTable

        if ( ! opponent->isTackling()
             && dash_dist < 0.001 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CROSS,
                          "%d: opponent[%d](%.2f %.2f) step=%d controllable",
                          M_total_count,
                          opponent->unum(), opponent->pos().x, opponent->pos().y, step );
#endif
            return CooperativeAction::Failure;
        }

        if ( dash_dist > ptype->realSpeedMax() * ( step + opponent->posCount() ) + 1.0 )
        {
            continue;
        }

        //
        // dash
        //

        const int opponent_dash = ptype->cyclesToReachDistance( dash_dist );
        const int opponent_turn = ( opponent->bodyCount() > 0
                                    ? 1
                                    : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                                opponent->body(),
                                                                                opponent_speed,
                                                                                ball_dist,
                                                                                ( ball_pos - opponent_pos ).th(),
                                                                                control_area + 0.1,
                                                                                true ) );
        int opponent_step = ( opponent_turn == 0
                              ? opponent_dash
                              : 1 + opponent_turn + opponent_dash );

        if ( dash_dist < 3.0 )
        {
            int omni_step = ptype->cyclesToReachDistance( dash_dist / 0.7 );
            omni_step += 1;
            if ( omni_step < opponent_step )
            {
                opponent_step = omni_step;
            }
        }

        if ( opponent->isTackling() )
        {
            opponent_step += 5; // magic number
        }
        else if ( opponent->posCount() > 0 )
        {
            opponent_step -= 1;
        }

        // if ( opponent->goalie() )
        // {
        //     opponent_step += 1;
        // }

        CooperativeAction::SafetyLevel level = CooperativeAction::Failure;

        const int total_step = ( opponent->goalie()
                                 ? step
                                 : step + n_kick - 1 );

        if ( opponent_step <= std::max( 0, total_step - 3 ) ) level = CooperativeAction::Failure;
        else if ( opponent_step <= std::max( 0, total_step - 2 ) ) level = CooperativeAction::Failure;
        else if ( opponent_step <= std::max( 0, total_step - 1 ) ) level = CooperativeAction::Failure;
        else if ( opponent_step <= total_step )
        {
            if ( step >= ball_step + n_kick - 1 )
            {
                level = CooperativeAction::Failure;
            }
            //else if ( opponent->bodyCount() == 0 )
            else if ( opponent_turn == 0 )
            {
                level = CooperativeAction::Failure;
            }
            else
            {
                level = CooperativeAction::Dangerous;
            }
        }
        else if ( opponent_step <= total_step + 1 )
        {
            if ( step >= ball_step + n_kick - 1 )
            {
                level = CooperativeAction::Dangerous;
            }
            //else if ( opponent_turn == 0 )
            else if ( opponent->posCount() > 1 )
            {
                level = CooperativeAction::Dangerous;
            }
            else
            {
                level = CooperativeAction::MaybeDangerous;
            }
        }
        else if ( opponent_step <= total_step + 2 ) level = CooperativeAction::Safe;
        else if ( opponent_step <= total_step + 3 ) level = CooperativeAction::Safe;
        else level = CooperativeAction::Safe;

#ifdef DEBUG_PRINT
        dlog.addText( Logger::CROSS,
                      "%d: opponent[%d](%.2f %.2f) bstep=%d(%.2f %.2f) n_kick=%d bdist=%.3f odist=%.3f ostep=%d(t%d d%d) safe=%d",
                      M_total_count,
                      opponent->unum(), opponent->pos().x, opponent->pos().y,
                      step, ball_pos.x, ball_pos.y,
                      n_kick, ball_dist, dash_dist, opponent_step, opponent_turn, opponent_dash, level );
#endif
        if ( result > level )
        {
            result = level;
            if ( result == CooperativeAction::Failure )
            {
                break;
            }
        }
    }

    return result;
}
