// -*-c++-*-

/*!
  \file generator_pass.cpp
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

#include "generator_pass.h"

#include "act_pass.h"
#include "field_analyzer.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/math_util.h>
#include <rcsc/timer.h>

#include <algorithm>
#include <limits>
#include <sstream>
#include <cmath>

#define USE_FILTER

// #define USE_POINTTO
// #define USE_PRE_CHECK_RECEIVER

// #define DEBUG_PROFILE

// #define DEBUG_PRINT_COMMON
// #define DEBUG_PRINT_SUCCESS_PASS
// #define DEBUG_PRINT_FAILED_PASS

// #define DEBUG_UPDATE_PASSER
// #define DEBUG_UPDATE_RECEIVERS
// #define DEBUG_UPDATE_OPPONENT
// #define DEBUG_DIRECT_PASS
// #define DEBUG_LEADING_PASS
// #define DEBUG_THROUGH_PASS

// #define DEBUG_PREDICT_RECEIVER
// #define DEBUG_PREDICT_OPPONENT_REACH_STEP


using namespace rcsc;

namespace {

const double KICK_COUNT_FACTOR = 0.99;
const double OFFENSIVE_PROB_FACTOR = 0.8;

inline
void
debug_paint_pass( const int count,
                  const Vector2D & receive_point,
                  const int safety_level,
                  const char * color )
{
    dlog.addRect( Logger::PASS,
                  receive_point.x - 0.1, receive_point.y - 0.1,
                  0.2, 0.2,
                  color );
    if ( count >= 0 )
    {
        char num[16];
        snprintf( num, 16, "%d:%d", count, safety_level );
        dlog.addMessage( Logger::PASS,
                         receive_point, num );
    }
}

inline
char
to_pass_char( int pass_type )
{
    switch ( pass_type ) {
    case ActPass::DIRECT:
        return 'D';
    case ActPass::LEADING:
        return 'L';
    case ActPass::THROUGH:
        return 'T';
    default:
        break;
    }

    return '-';
}

struct LeadingPassSorter {
    const Vector2D player_pos_;
    const Vector2D goal_pos_;

    LeadingPassSorter( const Vector2D & pos )
        : player_pos_( pos ),
          goal_pos_( ServerParam::i().theirTeamGoalPos() )
      { }

    bool operator()( const CooperativeAction::Ptr & lhs,
                     const CooperativeAction::Ptr & rhs ) const
      {
          if ( lhs->kickCount() < rhs->kickCount() )
          {
              return true;
          }
          if ( lhs->kickCount() > rhs->kickCount() )
          {
              return false;
          }

          double ld2 = lhs->targetBallPos().dist2( player_pos_ );
          double rd2 = rhs->targetBallPos().dist2( player_pos_ );
          if ( std::fabs( ld2 - rd2 ) < 0.001 )
          {
              return lhs->targetBallPos().dist2( goal_pos_ ) < rhs->targetBallPos().dist2( goal_pos_ );
          }

          return ld2 < rd2;
      }
};

struct ThroughPassSorter {
    bool operator()( const CooperativeAction::Ptr & lhs,
                     const CooperativeAction::Ptr & rhs ) const
      {
          if ( lhs )
          {
              dlog.addText( Logger::PASS,
                            "__ lhs index=%d", lhs->index() );
          }
          if ( rhs )
          {
              dlog.addText( Logger::PASS,
                            "__ rhs index=%d", rhs->index() );
          }
          if ( ! lhs && ! rhs )
          {
              dlog.addText( Logger::PASS,
                            "__ detect NULL both" );
              return false;
          }
          if ( ! lhs )
          {
              dlog.addText( Logger::PASS,
                            "__ detect NULL lhs" );
              return false;
          }
          if ( ! rhs )
          {
              dlog.addText( Logger::PASS,
                            "__ detect NULL rhs" );
              return false;
          }


          return lhs->safetyLevel() >= rhs->safetyLevel();
      }
};

struct SafetyLevelNotEqual {
    CooperativeAction::SafetyLevel level_;

    SafetyLevelNotEqual( const CooperativeAction::SafetyLevel level )
        : level_( level )
      { }
    bool operator()( const CooperativeAction::Ptr & val ) const
      {
          return level_ != val->safetyLevel();
      }
};

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
GeneratorPass::can_check_receiver_without_opponent( const WorldModel & wm )
{
    static GameTime s_last_time( -1, 0 );
    static bool s_last_result = false;

    if ( s_last_time == wm.time() )
    {
        return s_last_result;
    }
    s_last_time = wm.time();

    const ServerParam & SP = ServerParam::i();

    bool result = true;

    for ( PlayerObject::Cont::const_iterator p = wm.opponentsFromBall().begin(),
              end = wm.opponentsFromBall().end();
          p != end;
          ++p )
    {
        if ( (*p)->distFromBall() > 5.0 ) break;

        const PlayerType * ptype = (*p)->playerTypePtr();

        const double control_area = ( (*p)->goalie()
                                      && wm.ball().pos().x > SP.theirPenaltyAreaLineX()
                                      && wm.ball().pos().absY() < SP.penaltyAreaHalfWidth()
                                      ? SP.catchableArea()
                                      : SP.tackleDist() ); //: ptype->kickableArea() );

        Vector2D pos = (*p)->pos() + (*p)->vel();
        double ball_dist = wm.ball().pos().dist( pos );
        double dash_dist = ball_dist - control_area;

        if ( dash_dist < 0.001 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (can_check_receiver) opponent found(1)" );
            result = false;
            break;
        }

        int n_dash = ptype->cyclesToReachDistance( dash_dist );
        int n_turn = ( (*p)->bodyCount() > 1
                       ? 0
                       : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                   (*p)->body(),
                                                                   (*p)->vel().r(),
                                                                   ball_dist,
                                                                   ( wm.ball().pos() - pos ).th(),
                                                                   control_area,
                                                                   true ) ); // use back dash

        //if ( n_turn + n_dash <= 2 )
        if ( n_turn + n_dash <= 1 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (can_check_receiver) opponent found(2)" );
            result = false;
            break;
        }
    }

    s_last_result = result;
    return s_last_result;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
GeneratorPass::can_see_only_turn_neck( const WorldModel & wm,
                                       const AbstractPlayerObject & receiver )
{
    const ServerParam & SP = ServerParam::i();

    double angle_diff = ( receiver.angleFromSelf() - wm.self().body() ).abs();
    double view_max = SP.maxNeckAngle() + ViewWidth::width( ViewWidth::NARROW ) * 0.5 - 10.0;

    if ( angle_diff > view_max )
    {
        return false;
    }

    for ( PlayerObject::Cont::const_iterator p = wm.opponentsFromBall().begin(),
              end = wm.opponentsFromBall().end();
          p != end;
          ++p )
    {
        if ( (*p)->distFromBall() > 2.0 ) break;
        if ( (*p)->isTackling() ) continue;

        const double control_area = ( (*p)->goalie()
                                      && wm.ball().pos().x > SP.theirPenaltyAreaLineX()
                                      && wm.ball().pos().absY() < SP.penaltyAreaHalfWidth()
                                      ? (*p)->playerTypePtr()->maxCatchableDist()
                                      : (*p)->playerTypePtr()->kickableArea() );
        if ( (*p)->distFromBall() < control_area )
        {
            return false;
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorPass::Receiver::Receiver( const AbstractPlayerObject * p,
                                   const Vector2D & first_ball_pos )
    : player_( p ),
      pos_( p->seenPosCount() <= p->posCount() ? p->seenPos() : p->pos() ),
      vel_( p->seenVelCount() <= p->velCount() ? p->seenVel() : p->vel() ),
      inertia_pos_( p->playerTypePtr()->inertiaFinalPoint( pos_, vel_ ) ),
      speed_( vel_.r() ),
      penalty_distance_( FieldAnalyzer::estimate_virtual_dash_distance( p ) ),
      penalty_step_( p->playerTypePtr()->cyclesToReachDistance( penalty_distance_ ) ),
      angle_from_ball_( ( p->pos() - first_ball_pos ).th() )
{
#ifdef USE_POINTTO
    if ( p->pointtoCount() <= 2 )
    {
        double dash_dist = ( p->playerTypePtr()->realSpeedMax() * 0.8 ) * p->seenPosCount();
        inertia_pos_ += Vector2D::from_polar( dash_dist, p->pointtoAngle() );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorPass::Opponent::Opponent( const AbstractPlayerObject * p )
    : player_( p ),
      pos_( p->seenPosCount() <= p->posCount() ? p->seenPos() : p->pos() ),
      vel_( p->seenVelCount() <= p->velCount() ? p->seenVel() : p->vel() ),
      speed_( vel_.r() ),
      bonus_distance_( FieldAnalyzer::estimate_virtual_dash_distance( p ) )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorPass::GeneratorPass()
    : M_update_time( -1, 0 ),
      M_total_count( 0 ),
      M_pass_type( ActPass::UNKNOWN ),
      M_passer( static_cast< AbstractPlayerObject * >( 0 ) ),
      M_start_time( -1, 0 )
{
    M_receiver_candidates.reserve( 11 );
    M_opponents.reserve( 16 );
    for ( int i = 0; i < 11; ++i )
    {
        M_direct_pass[i].reserve( 2 );
        M_leading_pass[i].reserve( 100 );
        M_through_pass[i].reserve( 256 );
    }
    M_courses.reserve( 1024 );

    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorPass &
GeneratorPass::instance()
{
    static GeneratorPass s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorPass::clear()
{
    M_total_count = 0;
    M_pass_type = ActPass::UNKNOWN;
    M_passer = static_cast< AbstractPlayerObject * >( 0 );
    M_start_time.assign( -1, 0 );
    M_first_point.invalidate();
    M_receiver_candidates.clear();
    M_opponents.clear();
    M_direct_size = M_leading_size = M_through_size = 0;
    for ( int i = 0; i < 11; ++i )
    {
        M_direct_pass[i].clear();
        M_leading_pass[i].clear();
        M_through_pass[i].clear();
    }
    M_courses.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorPass::generate( const WorldModel & wm )
{
    if ( M_update_time == wm.time() )
    {
        return;
    }
    M_update_time = wm.time();

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
        dlog.addText( Logger::PASS,
                      __FILE__" (generate) passer not found." );
        return;
    }

    updateReceivers( wm );

    if ( M_receiver_candidates.empty() )
    {
        dlog.addText( Logger::PASS,
                      __FILE__" (generate) no receiver." );
        return;
    }

    updateOpponents( wm );

    createCourses( wm );

    //
    // sort and filter
    //
#ifdef USE_FILTER
    for ( int i = 0; i < 11; ++i )
    {
        if ( ! M_direct_pass[i].empty() )
        {
            M_courses.push_back( M_direct_pass[i].front() );
        }

        // dlog.addText( Logger::PASS,
        //               __FILE__" (generate) unum=%d generated leading pass %zd",
        //               i + 1, M_leading_pass[i].size() );
        if ( ! M_leading_pass[i].empty() )
        {
#if 1
            if ( M_leading_pass[i].size() > 1 )
            {
                CooperativeAction::SafetyLevel max_level = CooperativeAction::Failure;
                for ( std::vector< CooperativeAction::Ptr >::iterator it = M_leading_pass[i].begin(),
                          end = M_leading_pass[i].end();
                      it != end;
                      ++it )
                {
                    if ( (*it)->safetyLevel() > max_level )
                    {
                        max_level = (*it)->safetyLevel();
                        if ( max_level == CooperativeAction::Safe )
                        {
                            break;
                        }
                    }
                }

                M_leading_pass[i].erase( std::remove_if( M_leading_pass[i].begin(),
                                                         M_leading_pass[i].end(),
                                                         SafetyLevelNotEqual( max_level ) ),
                                         M_leading_pass[i].end() );
            }
            dlog.addText( Logger::PASS,
                          __FILE__" (generate) unum=%d result leading pass %zd",
                          i + 1, M_leading_pass[i].size() );
#endif
#if 0
            const AbstractPlayerObject * p = wm.ourPlayer( i + 1 );
            if ( p )
            {
                Vector2D player_pos = p->inertiaFinalPoint();
                // for ( std::vector< CooperativeAction::Ptr >::iterator it = M_leading_pass[i].begin(),
                //           end = M_leading_pass[i].end();
                //       it != end;
                //       ++it )
                // {
                //     dlog.addText( Logger::PASS,
                //                   "leading %d (%.2f %.2f) safe=%d dist=%.3f",
                //                   (*it)->index(),
                //                   (*it)->targetBallPos().x, (*it)->targetBallPos().y,
                //                   (*it)->safetyLevel(),
                //                   (*it)->targetBallPos().dist( player_pos ) );
                // }

                std::vector< CooperativeAction::Ptr >::iterator best
                    = std::min_element( M_leading_pass[i].begin(), M_leading_pass[i].end(),
                                        LeadingPassSorter( player_pos ) );
                if ( best != M_leading_pass[i].end() )
                {
                    dlog.addText( Logger::PASS,
                                  "best leading %d (%.2f %.2f) safe=%d dist=%.3f",
                                  (*best)->index(),
                                  (*best)->targetBallPos().x, (*best)->targetBallPos().y,
                                  (*best)->safetyLevel(),
                                  (*best)->targetBallPos().dist( player_pos ) );
                    M_courses.push_back( *best );
                }
            }
#else
            M_courses.insert( M_courses.end(), M_leading_pass[i].begin(), M_leading_pass[i].end() );
#endif
        }

        // dlog.addText( Logger::PASS,
        //               __FILE__" (generate) unum=%d generated through pass %zd",
        //               i + 1, M_through_pass[i].size() );
#if 1
        if ( M_through_pass[i].size() > 1 )
        {
            CooperativeAction::SafetyLevel max_level = CooperativeAction::Failure;
            for ( std::vector< CooperativeAction::Ptr >::iterator it = M_through_pass[i].begin();
                  it != M_through_pass[i].end();
                  ++it )
            {
                if ( (*it)->safetyLevel() > max_level )
                {
                    max_level = (*it)->safetyLevel();
                    if ( max_level == CooperativeAction::Safe )
                    {
                        break;
                    }
                }
            }

            M_through_pass[i].erase( std::remove_if( M_through_pass[i].begin(),
                                                     M_through_pass[i].end(),
                                                     SafetyLevelNotEqual( max_level ) ),
                                     M_through_pass[i].end() );
            dlog.addText( Logger::PASS,
                          __FILE__" (generate) unum=%d result through pass %zd",
                          i + 1, M_through_pass[i].size() );
        }
#endif
        M_courses.insert( M_courses.end(), M_through_pass[i].begin(), M_through_pass[i].end() );
    }
#endif
    // remove unsafe pass candidates
    if ( wm.self().goalie()
         // || wm.gameMode().type() == GameMode::GoalKick_
         )
    {
        M_courses.erase( std::remove_if( M_courses.begin(),
                                         M_courses.end(),
                                         SafetyLevelNotEqual( CooperativeAction::Safe ) ),
                         M_courses.end() );
    }

    std::sort( M_courses.begin(), M_courses.end(),
               CooperativeAction::DistanceSorter( ServerParam::i().theirTeamGoalPos() ) );

#ifdef DEBUG_PROFILE
    if ( M_passer->unum() == wm.self().unum() )
    {
        dlog.addText( Logger::PASS,
                      __FILE__" (generate) PROFILE passer=self size=%d/%d D=%d L=%d T=%d elapsed %f [ms]",
                      (int)M_courses.size(),
                      M_total_count,
                      M_direct_size, M_leading_size, M_through_size,
                      timer.elapsedReal() );
    }
    else
    {
        dlog.addText( Logger::PASS,
                      __FILE__" (update) PROFILE passer=%d size=%d/%d D=%d L=%d T=%d elapsed %f [ms]",
                      M_passer->unum(),
                      (int)M_courses.size(),
                      M_total_count,
                      M_direct_size, M_leading_size, M_through_size,
                      timer.elapsedReal() );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorPass::updatePasser( const WorldModel & wm )
{
    if ( wm.self().isKickable()
         && ! wm.self().isFrozen() )
    {
        M_passer = &wm.self();
        M_start_time = wm.time();
        M_first_point = wm.ball().pos();
#ifdef DEBUG_UPDATE_PASSER
        dlog.addText( Logger::PASS,
                      __FILE__" (updatePasser) self kickable." );
#endif
        return;
    }

    int s_min = wm.interceptTable()->selfReachCycle();
    int t_min = wm.interceptTable()->teammateReachCycle();
    int o_min = wm.interceptTable()->opponentReachCycle();

    int our_min = std::min( s_min, t_min );
    if ( o_min < std::min( our_min - 4, (int)rint( our_min * 0.9 ) ) )
    {
        dlog.addText( Logger::PASS,
                      __FILE__" (updatePasser) opponent ball." );
        return;
    }

    if ( s_min <= t_min )
    {
        if ( s_min <= 2 )
        {
            M_passer = &wm.self();
            M_first_point = wm.ball().inertiaPoint( s_min );
        }
    }
    else
    {
        if ( t_min <= 2 )
        {
            M_passer = wm.interceptTable()->fastestTeammate();
            M_first_point = wm.ball().inertiaPoint( t_min );
        }
    }

    if ( ! M_passer )
    {
        dlog.addText( Logger::PASS,
                      __FILE__" (updatePasser) no passer." );
        return;
    }

    M_start_time = wm.time();
    if ( ! wm.gameMode().isServerCycleStoppedMode() )
    {
        M_start_time.addCycle( t_min );
    }

    if ( M_passer->unum() != wm.self().unum() )
    {
#if 1
        M_passer = static_cast< const AbstractPlayerObject * >( 0 );
        dlog.addText( Logger::PASS,
                      __FILE__" (updatePasser) passer is teammate." );
        return;
#else
        if ( M_first_point.dist2( wm.self().pos() ) > std::pow( 30.0, 2 ) )
        {
            M_passer = static_cast< const AbstractPlayerObject * >( 0 );
            dlog.addText( Logger::PASS,
                          __FILE__" (updatePasser) passer is too far." );
            return;
        }
#endif
    }

#ifdef DEBUG_UPDATE_PASSER
    dlog.addText( Logger::PASS,
                  __FILE__" (updatePasser) passer=%d(%.1f %.1f) reachStep=%d startPos=(%.1f %.1f)",
                  M_passer->unum(),
                  M_passer->pos().x, M_passer->pos().y,
                  t_min,
                  M_first_point.x, M_first_point.y );
#endif
}

namespace {

/*-------------------------------------------------------------------*/
/*!

 */
struct ReceiverAngleCompare {

    bool operator()( const GeneratorPass::Receiver & lhs,
                     const GeneratorPass::Receiver & rhs ) const
      {
          return lhs.angle_from_ball_.degree() < rhs.angle_from_ball_.degree();
      }
};

/*-------------------------------------------------------------------*/
/*!

 */
struct ReceiverDistCompare {

    const Vector2D pos_;

    ReceiverDistCompare( const Vector2D & pos )
        : pos_( pos )
      { }

    bool operator()( const GeneratorPass::Receiver & lhs,
                     const GeneratorPass::Receiver & rhs ) const
      {
          return lhs.pos_.dist2( pos_ ) < rhs.pos_.dist2( pos_ );
      }
};

}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorPass::updateReceivers( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();
    const double max_dist2 = std::pow( 40.0, 2 ); // Magic Number

    const bool is_self_passer = ( M_passer->unum() == wm.self().unum() );
#ifdef USE_PRE_CHECK_RECEIVER
    const bool can_check = can_check_receiver_without_opponent( wm );
#endif

    for ( AbstractPlayerObject::Cont::const_iterator p = wm.ourPlayers().begin(),
              end = wm.ourPlayers().end();
          p != end;
          ++p )
    {
        if ( *p == M_passer ) continue;

        if ( is_self_passer )
        {
            if ( (*p)->ghostCount() >= 2 ) continue;
            if ( (*p)->unum() == Unum_Unknown ) continue;
#ifdef USE_PRE_CHECK_RECEIVER
            if ( can_check )
            {
                if ( (*p)->posCount() > 20 ) continue;
            }
            else
            {
                if ( (*p)->ghostCount() >= 1 ) continue;

                if ( can_see_only_turn_neck( wm, **p ) )
                {
                    if ( (*p)->posCount() > 10 ) continue;
                }
                else
                {
                    if ( (*p)->seenPosCount() > 3 ) continue;
                }
            }
#else
            if ( (*p)->posCount() > 10 ) continue;
#endif
            if ( (*p)->isTackling() ) continue;

            double offside_buf = 0.0;
#if 0
            if ( (*p)->pointtoCount() < 5
                 && (*p)->pointtoAngle().abs() < 60.0 )
            {
                offside_buf = 1.0;
            }
#endif
            if ( (*p)->pos().x > wm.offsideLineX() + offside_buf )
            {
                dlog.addText( Logger::PASS,
                              "(updateReceiver) unum=%d (%.2f %.2f) > offside=%.2f",
                              (*p)->unum(),
                              (*p)->pos().x, (*p)->pos().y,
                              wm.offsideLineX() + offside_buf );
                continue;
            }
            // dlog.addText( Logger::PASS,
            //               "(updateReceiver) unum=%d (%.2f %.2f) < offside=%.2f",
            //               (*p)->unum(),
            //               (*p)->pos().x, (*p)->pos().y,
            //               wm.offsideLineX() + offside_buf );

            if ( (*p)->goalie()
                 && (*p)->pos().x < SP.ourPenaltyAreaLineX() + 15.0 )
            {
                continue;
            }
        }
        else
        {
            // ignore other players
            if ( (*p)->unum() != wm.self().unum() )
            {
                continue;
            }
        }

        if ( (*p)->pos().dist2( M_first_point ) > max_dist2 ) continue;

        M_receiver_candidates.push_back( Receiver( *p, M_first_point ) );
    }

    std::sort( M_receiver_candidates.begin(),
               M_receiver_candidates.end(),
               ReceiverDistCompare( SP.theirTeamGoalPos() ) );

    // std::sort( M_receiver_candidates.begin(),
    //            M_receiver_candidates.end(),
    //            ReceiverAngleCompare() );

#ifdef DEBUG_UPDATE_RECEIVERS
    for ( ReceiverCont::const_iterator p = M_receiver_candidates.begin();
          p != M_receiver_candidates.end();
          ++p )
    {
        dlog.addText( Logger::PASS,
                      "Pass receiver %d pos(%.1f %.1f) inertia=(%.1f %.1f) vel(%.2f %.2f)"
                      " penalty_dist=%.3f penalty_step=%d"
                      " angle=%.1f",
                      p->player_->unum(),
                      p->pos_.x, p->pos_.y,
                      p->inertia_pos_.x, p->inertia_pos_.y,
                      p->vel_.x, p->vel_.y,
                      p->penalty_distance_,
                      p->penalty_step_,
                      p->angle_from_ball_.degree() );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorPass::updateOpponents( const WorldModel & wm )
{
    for ( AbstractPlayerObject::Cont::const_iterator p = wm.theirPlayers().begin(),
              end = wm.theirPlayers().end();
          p != end;
          ++p )
    {
        M_opponents.push_back( Opponent( *p ) );
#ifdef DEBUG_UPDATE_OPPONENT
        const Opponent & o = M_opponents.back();
        dlog.addText( Logger::PASS,
                      "Pass opp %d pos(%.1f %.1f) vel(%.2f %.2f) bonus_dist=%.3f",
                      o.player_->unum(),
                      o.pos_.x, o.pos_.y,
                      o.vel_.x, o.vel_.y,
                      o.bonus_distance_ );
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorPass::createCourses( const WorldModel & wm )
{
    const ReceiverCont::iterator end = M_receiver_candidates.end();

    M_pass_type = ActPass::DIRECT;
    for ( ReceiverCont::iterator p = M_receiver_candidates.begin();
          p != end;
          ++p )
    {
        createDirectPass( wm, *p );
    }

    M_pass_type = ActPass::LEADING;
    for ( ReceiverCont::iterator p = M_receiver_candidates.begin();
          p != end;
          ++p )
    {
        if ( wm.ourStaminaCapacity( p->player_->unum() ) > 1.0 )
        {
            createLeadingPass( wm, *p );
        }
    }

    M_pass_type = ActPass::THROUGH;
    for ( ReceiverCont::iterator p = M_receiver_candidates.begin();
          p != end;
          ++p )
    {
        if ( wm.ourStaminaCapacity( p->player_->unum() ) > 1.0 )
        {
            createThroughPass( wm, *p );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorPass::createDirectPass( const WorldModel & wm,
                                 const Receiver & receiver )
{
    static const int MIN_RECEIVE_STEP = 3;
#ifdef CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT
    static const int MAX_RECEIVE_STEP = 15; // Magic Number
#endif

    static const double MIN_DIRECT_PASS_DIST
        = ServerParam::i().defaultKickableArea() * 2.2;
    static const double MAX_DIRECT_PASS_DIST
        = 0.8 * inertia_final_distance( ServerParam::i().ballSpeedMax(),
                                        ServerParam::i().ballDecay() );
    static const double MAX_RECEIVE_BALL_SPEED
        = ServerParam::i().ballSpeedMax()
        * std::pow( ServerParam::i().ballDecay(), MIN_RECEIVE_STEP );

    const ServerParam & SP = ServerParam::i();

    //
    // check receivable area
    //
    if ( receiver.pos_.x > SP.pitchHalfLength() - 1.5
         || receiver.pos_.x < - SP.pitchHalfLength() + 5.0
         || receiver.pos_.absY() > SP.pitchHalfWidth() - 1.5 )
    {
#ifdef DEBUG_DIRECT_PASS
        dlog.addText( Logger::PASS,
                      "%d: xxx (direct) unum=%d outOfBounds pos=(%.2f %.2f)",
                      M_total_count, receiver.player_->unum(),
                      receiver.pos_.x, receiver.pos_.y );
#endif
        return;
    }

    //
    // avoid dangerous area
    //
    if ( receiver.pos_.x < M_first_point.x + 1.0
         && receiver.pos_.dist2( SP.ourTeamGoalPos() ) < std::pow( 18.0, 2 ) )
    {
#ifdef DEBUG_DIRECT_PASS
        dlog.addText( Logger::PASS,
                      "%d: xxx (direct) unum=%d dangerous pos=(%.2f %.2f)",
                      M_total_count, receiver.player_->unum(),
                      receiver.pos_.x, receiver.pos_.y );
#endif
        return;
    }

    const PlayerType * ptype = receiver.player_->playerTypePtr();

    const double max_ball_speed = ( wm.gameMode().type() == GameMode::PlayOn
                                    ? SP.ballSpeedMax()
                                    : wm.self().isKickable()
                                    ? wm.self().kickRate() * SP.maxPower()
                                    : SP.kickPowerRate() * SP.maxPower() );
    const double min_ball_speed = SP.defaultRealSpeedMax();

    const Vector2D receive_point = receiver.inertia_pos_;

    const double ball_move_dist = M_first_point.dist( receive_point );

    if ( ball_move_dist < MIN_DIRECT_PASS_DIST
         || MAX_DIRECT_PASS_DIST < ball_move_dist )
    {
#ifdef DEBUG_DIRECT_PASS
        dlog.addText( Logger::PASS,
                      "%d: xxx (direct) unum=%d overBallMoveDist=%.3f minDist=%.3f maxDist=%.3f",
                      M_total_count, receiver.player_->unum(),
                      ball_move_dist,
                      MIN_DIRECT_PASS_DIST, MAX_DIRECT_PASS_DIST );
#endif
        return;
    }

    if ( wm.gameMode().type() == GameMode::GoalKick_
         && receive_point.x < SP.ourPenaltyAreaLineX() + 1.0
         && receive_point.absY() < SP.penaltyAreaHalfWidth() + 1.0 )
    {
#ifdef DEBUG_DIRECT_PASS
        dlog.addText( Logger::PASS,
                      "%d: xxx (direct) unum=%d, goal_kick",
                      M_total_count, receiver.player_->unum() );
#endif
        return;
    }

    //
    // decide ball speed range
    //

    const double max_receive_ball_speed
        = std::min( MAX_RECEIVE_BALL_SPEED,
                    ptype->kickableArea() + ( SP.maxDashPower()
                                              * ptype->dashPowerRate()
                                              * ptype->effortMax() ) * 1.8 );
    const double min_receive_ball_speed = ptype->realSpeedMax();

    const AngleDeg ball_move_angle = ( receive_point - M_first_point ).th();

    const int min_ball_step = SP.ballMoveStep( SP.ballSpeedMax(), ball_move_dist );


#ifdef DEBUG_PRINT_SUCCESS_PASS
    std::vector< int > success_counts;
#endif

    const int start_step = std::max( std::max( MIN_RECEIVE_STEP,
                                               min_ball_step ),
                                     receiver.penalty_step_ );
#ifdef CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT
    const int max_step = std::max( MAX_RECEIVE_STEP, start_step + 2 );
#else
    const int max_step = start_step + 2;
#endif

#ifdef DEBUG_DIRECT_PASS
    dlog.addText( Logger::PASS,
                  "=== (direct) unum=%d stepRange=[%d, %d]",
                  receiver.player_->unum(),
                  start_step, max_step );
#endif

    createPassCommon( wm,
                      receiver, receive_point,
                      start_step, max_step,
                      min_ball_speed, max_ball_speed,
                      min_receive_ball_speed, max_receive_ball_speed,
                      ball_move_dist, ball_move_angle,
                      "Direct" );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorPass::createLeadingPass( const WorldModel & wm,
                                  const Receiver & receiver )
{
    static const double OUR_GOAL_DIST_THR2 = std::pow( 16.0, 2 );

    static const int MIN_RECEIVE_STEP = 4;
#ifdef CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT
    static const int MAX_RECEIVE_STEP = 20;
#endif

    static const double MIN_LEADING_PASS_DIST = 3.0;
    static const double MAX_LEADING_PASS_DIST
        = 0.8 * inertia_final_distance( ServerParam::i().ballSpeedMax(),
                                        ServerParam::i().ballDecay() );
    static const double MAX_RECEIVE_BALL_SPEED
        = ServerParam::i().ballSpeedMax()
        * std::pow( ServerParam::i().ballDecay(), MIN_RECEIVE_STEP );

    static const int ANGLE_DIVS = 24;
    static const double ANGLE_STEP = 360.0 / ANGLE_DIVS;
    static const double DIST_DIVS = 4;
    static const double DIST_STEP = 1.1;

    const ServerParam & SP = ServerParam::i();
    const PlayerType * ptype = receiver.player_->playerTypePtr();

    const double max_ball_speed = ( wm.gameMode().type() == GameMode::PlayOn
                                    ? SP.ballSpeedMax()
                                    : wm.self().isKickable()
                                    ? wm.self().kickRate() * SP.maxPower()
                                    : SP.kickPowerRate() * SP.maxPower() );
    const double min_ball_speed = SP.defaultRealSpeedMax();

    const double max_receive_ball_speed
        = std::min( MAX_RECEIVE_BALL_SPEED,
                    ptype->kickableArea() + ( SP.maxDashPower()
                                              * ptype->dashPowerRate()
                                              * ptype->effortMax() ) * 1.5 );
    const double min_receive_ball_speed = 0.001;

    const Vector2D our_goal = SP.ourTeamGoalPos();

#ifdef DEBUG_PRINT_SUCCESS_PASS
    std::vector< int > success_counts;
    success_counts.reserve( 16 );
#endif

    //
    // distance loop
    //
    for ( int d = 1; d <= DIST_DIVS; ++d )
    {
        const double player_move_dist = DIST_STEP * d;
        const int a_step = ( player_move_dist * 2.0 * M_PI / ANGLE_DIVS < 0.6
                             ? 2
                             : 1 );
        // const int move_dist_penalty_step
        //     = static_cast< int >( std::floor( player_move_dist * 0.3 ) );

        //
        // angle loop
        //
        for ( int a = 0; a < ANGLE_DIVS; a += a_step )
        {
            ++M_total_count;

            const AngleDeg angle = receiver.angle_from_ball_ + ANGLE_STEP*a;
            const Vector2D receive_point
                = receiver.inertia_pos_
                + Vector2D::from_polar( player_move_dist, angle );

#ifdef DEBUG_LEADING_PASS
            dlog.addText( Logger::PASS,
                          "%d: >>>> (lead) unum=%d receivePoint=(%.1f %.1f)",
                          M_total_count,
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y );
#endif

            if ( receive_point.x > SP.pitchHalfLength() - 3.0
                 || receive_point.x < -SP.pitchHalfLength() + 5.0
                 || receive_point.absY() > SP.pitchHalfWidth() - 3.0 )
            {
#ifdef DEBUG_LEADING_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (lead) unum=%d outOfBounds pos=(%.2f %.2f)",
                              M_total_count, receiver.player_->unum(),
                              receive_point.x, receive_point.y );
                debug_paint_pass( M_total_count, receive_point, 0, "#F00" );
#endif
                continue;
            }

            if ( receive_point.x < M_first_point.x
                 && receive_point.dist2( our_goal ) < OUR_GOAL_DIST_THR2 )
            {
#ifdef DEBUG_LEADING_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (lead) unum=%d our goal is near pos=(%.2f %.2f)",
                              M_total_count, receiver.player_->unum(),
                              receive_point.x, receive_point.y );
                debug_paint_pass( M_total_count, receive_point, 0, "#F00" );
#endif
                continue;
            }

            if ( wm.gameMode().type() == GameMode::GoalKick_
                 && receive_point.x < SP.ourPenaltyAreaLineX() + 1.0
                 && receive_point.absY() < SP.penaltyAreaHalfWidth() + 1.0 )
            {
#ifdef DEBUG_LEADING_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (lead) unum=%d, goal_kick",
                              M_total_count, receiver.player_->unum() );
#endif
                return;
            }

            const double ball_move_dist = M_first_point.dist( receive_point );

            if ( ball_move_dist < MIN_LEADING_PASS_DIST
                 || MAX_LEADING_PASS_DIST < ball_move_dist )
            {
#ifdef DEBUG_LEADING_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (lead) unum=%d overBallMoveDist=%.3f minDist=%.3f maxDist=%.3f",
                              M_total_count, receiver.player_->unum(),
                              ball_move_dist,
                              MIN_LEADING_PASS_DIST, MAX_LEADING_PASS_DIST );
                debug_paint_pass( M_total_count, receive_point, 0.0, "#F00" );
#endif
                continue;
            }

            {
                const Receiver * nearest_receiver = getNearestReceiver( receive_point );
                if ( nearest_receiver
                     && nearest_receiver->player_ != receiver.player_ )
                {
#ifdef DEBUG_LEADING_PASS
                    dlog.addText( Logger::PASS,
                                  "%d: xxx (lead) unum=%d otherReceiver=%d pos=(%.2f %.2f)",
                                  M_total_count, receiver.player_->unum(),
                                  nearest_receiver_unum,
                                  receive_point.x, receive_point.y );
                    debug_paint_pass( M_total_count, receive_point, 0.0, "#F00" );
#endif
                    break;
                }
            }

            const int receiver_step = predictReceiverReachStep( receiver,
                                                                receive_point,
                                                                true );
            const AngleDeg ball_move_angle = ( receive_point - M_first_point ).th();

            const int min_ball_step = SP.ballMoveStep( SP.ballSpeedMax(), ball_move_dist );

#ifdef DEBUG_PRINT_SUCCESS_PASS
            success_counts.clear();
#endif

            const int start_step = std::max( std::max( MIN_RECEIVE_STEP,
                                                       min_ball_step ),
                                             receiver_step );

            // #ifdef DEBUG_LEADING_PASS
            //             dlog.addText( Logger::PASS,
            //                           "=== (lead) unum=%d MIN_RECEIVE_STEP=%d"
            //                           " min_ball_step=%d"
            //                           " receiver_step=%d",
            //                           receiver.player_->unum(),
            //                           MIN_RECEIVE_STEP, min_ball_step, receiver_step );
            // #endif

#ifdef CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT
            const int max_step = std::max( MAX_RECEIVE_STEP, start_step + 3 );
#else
            const int max_step = start_step + 3;
#endif

#ifdef DEBUG_LEADING_PASS
            dlog.addText( Logger::PASS,
                          "=== (lead) receiver=%d"
                          " receivePos=(%.1f %.1f)",
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y );
            dlog.addText( Logger::PASS,
                          "__ ballMove=%.3f moveAngle=%.1f",
                          ball_move_dist, ball_move_angle.degree() );
            dlog.addText( Logger::PASS,
                          "__ stepRange=[%d, %d] receiverStep=%d(penalty=%d)",
                          start_step, max_step, receiver_step, move_dist_penalty_step );
#endif

            createPassCommon( wm,
                              receiver, receive_point,
                              start_step, max_step,
                              min_ball_speed, max_ball_speed,
                              min_receive_ball_speed, max_receive_ball_speed,
                              ball_move_dist, ball_move_angle,
                              "Lead" );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorPass::createThroughPass( const WorldModel & wm,
                                  const Receiver & receiver )
{
    static const int MIN_RECEIVE_STEP = 6;
#ifdef CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT
    static const int MAX_RECEIVE_STEP = 35;
#endif

    static const double MIN_THROUGH_PASS_DIST = 5.0;
    static const double MAX_THROUGH_PASS_DIST
        = 0.9 * inertia_final_distance( ServerParam::i().ballSpeedMax(),
                                        ServerParam::i().ballDecay() );
    static const double MAX_RECEIVE_BALL_SPEED
        = ServerParam::i().ballSpeedMax()
        * std::pow( ServerParam::i().ballDecay(), MIN_RECEIVE_STEP );

    static const int ANGLE_DIVS = 14;
    static const double MIN_ANGLE = -40.0;
    static const double MAX_ANGLE = +40.0;
    static const double ANGLE_STEP = ( MAX_ANGLE - MIN_ANGLE ) / ANGLE_DIVS;

    static const double MIN_MOVE_DIST = 6.0;
    static const double MAX_MOVE_DIST = 30.0 + 0.001;
    static const double MOVE_DIST_STEP = 2.0;

    const ServerParam & SP = ServerParam::i();
    const PlayerType * ptype = receiver.player_->playerTypePtr();
    const AngleDeg receiver_vel_angle = receiver.vel_.th();

    const double min_receive_x = std::min( std::min( std::max( 10.0, M_first_point.x + 10.0 ),
                                                     wm.offsideLineX() - 10.0 ),
                                           SP.theirPenaltyAreaLineX() - 5.0 );

    if ( receiver.pos_.x < min_receive_x - MAX_MOVE_DIST
         || receiver.pos_.x < 1.0 )
    {
#ifdef DEBUG_THROUGH_PASS
        dlog.addText( Logger::PASS,
                      "%d: xxx (through) unum=%d too back.",
                      M_total_count, receiver.player_->unum() );
#endif
        return;
    }

    //
    // initialize ball speed range
    //

    const double max_ball_speed = ( wm.gameMode().type() == GameMode::PlayOn
                                    ? SP.ballSpeedMax()
                                    : wm.self().isKickable()
                                    ? wm.self().kickRate() * SP.maxPower()
                                    : SP.kickPowerRate() * SP.maxPower() );
    const double min_ball_speed = 1.4; //SP.defaultPlayerSpeedMax();

    const double max_receive_ball_speed
        = std::min( MAX_RECEIVE_BALL_SPEED,
                    ptype->kickableArea() + ( SP.maxDashPower()
                                              * ptype->dashPowerRate()
                                              * ptype->effortMax() ) * 1.5 );
    const double min_receive_ball_speed = 0.001;

    //
    // check communication
    //

    bool pass_requested = false;
    AngleDeg requested_move_angle = 0.0;
    if ( wm.audioMemory().passRequestTime().cycle() > wm.time().cycle() - 10 ) // Magic Number
    {
        for ( std::vector< AudioMemory::PassRequest >::const_iterator it = wm.audioMemory().passRequest().begin();
              it != wm.audioMemory().passRequest().end();
              ++it )
        {
            if ( it->sender_ == receiver.player_->unum() )
            {
                pass_requested = true;
                requested_move_angle = ( it->pos_ - receiver.inertia_pos_ ).th();
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: (through) receiver=%d pass requested",
                              M_total_count, receiver.player_->unum() );
#endif
                break;
            }
        }
    }

    //
    //
    //
#ifdef DEBUG_PRINT_SUCCESS_PASS
    std::vector< int > success_counts;
    success_counts.reserve( 16 );
#endif

    //
    // angle loop
    //
    for ( int a = 0; a <= ANGLE_DIVS; ++a )
    {
        const AngleDeg angle = MIN_ANGLE + ( ANGLE_STEP * a );
        const Vector2D unit_vec = Vector2D::from_polar( 1.0, angle );

        //
        // distance loop
        //
        for ( double move_dist = MIN_MOVE_DIST;
              move_dist < MAX_MOVE_DIST;
              move_dist += MOVE_DIST_STEP )
        {
            ++M_total_count;

            const Vector2D receive_point = receiver.inertia_pos_ + ( unit_vec * move_dist );

#ifdef DEBUG_THROUGH_PASS
            dlog.addText( Logger::PASS,
                          "%d: >>>> (through) receiver=%d receivePoint=(%.1f %.1f)",
                          M_total_count,
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y );
#endif

            if ( receive_point.x < min_receive_x )
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (through) unum=%d tooSmallX pos=(%.2f %.2f)",
                              M_total_count, receiver.player_->unum(),
                              receive_point.x, receive_point.y );
                debug_paint_pass( M_total_count, receive_point, 0.0, "#F00" );
#endif
                continue;
            }

            if ( receive_point.x > SP.pitchHalfLength() - 1.5
                 || receive_point.absY() > SP.pitchHalfWidth() - 1.5 )
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (through) unum=%d outOfBounds pos=(%.2f %.2f)",
                              M_total_count, receiver.player_->unum(),
                              receive_point.x, receive_point.y );
                debug_paint_pass( M_total_count, receive_point, 0.0, "#F00" );
#endif
                break;
            }

            const double ball_move_dist = M_first_point.dist( receive_point );

            if ( ball_move_dist < MIN_THROUGH_PASS_DIST
                 || MAX_THROUGH_PASS_DIST < ball_move_dist )
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (through) unum=%d overBallMoveDist=%.3f minDist=%.3f maxDist=%.3f",
                              M_total_count, receiver.player_->unum(),
                              ball_move_dist,
                              MIN_THROUGH_PASS_DIST, MAX_THROUGH_PASS_DIST );
                debug_paint_pass( M_total_count, receive_point, 0.0, "#F00" );
#endif
                continue;
            }

            const AngleDeg ball_move_angle = ( receive_point - M_first_point ).th();

            // add 2013-06-11
            if ( ( receiver.player_->angleFromBall() - ball_move_angle ).abs() > 100.0 )
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (through) unum=%d over angle diff ball_move=%.1f receiver=%.1f diff=%.1f",
                              M_total_count, receiver.player_->unum(),
                              ball_move_angle.degree(),  receiver.player_->angleFromBall(),
                              ( ball_move_angle.degree() -  receiver.player_->angleFromBall() ).abs() );
                debug_paint_pass( M_total_count, receive_point, 0.0, "#F00" );
#endif
                continue;
            }

            {
                const Receiver * nearest_receiver = getNearestReceiver( receive_point );
                if ( nearest_receiver
                     && nearest_receiver->player_ != receiver.player_
                     && ( nearest_receiver->pos_.dist( receive_point )
                          < receiver.pos_.dist( receive_point ) * 0.95 )
                     )
                {
#ifdef DEBUG_THROUGH_PASS
                    dlog.addText( Logger::PASS,
                                  "%d: xxx (through) unum=%d otherReceiver=%d pos=(%.2f %.2f)",
                                  M_total_count, receiver.player_->unum(),
                                  nearest_receiver->player_->unum(),
                                  receive_point.x, receive_point.y );
                    debug_paint_pass( M_total_count, receive_point, 0.0, "#FF0" );
#endif
                    break;
                }
            }

            const int receiver_step = predictReceiverReachStep( receiver,
                                                                receive_point,
                                                                false );

#ifdef DEBUG_PRINT_SUCCESS_PASS
            success_counts.clear();
#endif

            int start_step = receiver_step;
            if ( pass_requested
                 && ( requested_move_angle - angle ).abs() < 20.0 )
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: matched with requested pass. angle=%.1f",
                              M_total_count, angle.degree() );
#endif
            }
            // if ( receive_point.x > wm.offsideLineX() + 5.0
            //      || ball_move_angle.abs() < 15.0 )
            else if ( receiver.speed_ > 0.2
                      && ( receiver_vel_angle - angle ).abs() < 15.0 )
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: matched with receiver velocity. angle=%.1f",
                              M_total_count, angle.degree() );
#endif
                start_step += 1;
            }
            else
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: receiver step. one step penalty",
                              M_total_count );
#endif
                start_step += 1;
                if ( ( receive_point.x > SP.pitchHalfLength() - 5.0
                       || receive_point.absY() > SP.pitchHalfWidth() - 5.0 )
                     && ball_move_angle.abs() > 30.0
                     && start_step >= 10 )
                {
                    start_step += 1;
                }
            }

            const int min_ball_step = SP.ballMoveStep( SP.ballSpeedMax(), ball_move_dist );

            start_step = std::max( std::max( MIN_RECEIVE_STEP,
                                             min_ball_step ),
                                   start_step );

#ifdef CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT
            const int max_step = std::max( MAX_RECEIVE_STEP, start_step + 3 );
#else
            const int max_step = start_step + 3;
#endif

#ifdef DEBUG_THROUGH_PASS
            dlog.addText( Logger::PASS,
                          "%d: (through) receiver=%d"
                          " ballPos=(%.1f %.1f) receivePos=(%.1f %.1f)",
                          M_total_count,
                          receiver.player_->unum(),
                          M_first_point.x, M_first_point.y,
                          receive_point.x, receive_point.y );
            dlog.addText( Logger::PASS,
                          "%d: ballMove=%.3f moveAngle=%.1f",
                          M_total_count,
                          ball_move_dist, ball_move_angle.degree() );
            dlog.addText( Logger::PASS,
                          "%d: stepRange=[%d, %d] receiverMove=%.3f receiverStep=%d",
                          M_total_count,
                          start_step, max_step,
                          receiver.inertia_pos_.dist( receive_point ), receiver_step );
#endif

            createPassCommon( wm,
                              receiver, receive_point,
                              start_step, max_step,
                              min_ball_speed, max_ball_speed,
                              min_receive_ball_speed, max_receive_ball_speed,
                              ball_move_dist, ball_move_angle,
                              "Through" );
        }

    }

}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorPass::createPassCommon( const WorldModel & wm,
                                 const Receiver & receiver,
                                 const Vector2D & receive_point,
                                 const int min_step,
                                 const int max_step,
                                 const double & min_first_ball_speed,
                                 const double & max_first_ball_speed,
                                 const double & min_receive_ball_speed,
                                 const double & max_receive_ball_speed,
                                 const double & ball_move_dist,
                                 const AngleDeg & ball_move_angle,
                                 const char * description )
{
    const ServerParam & SP = ServerParam::i();

    for ( int step = min_step; step <= max_step; ++step )
    {
        ++M_total_count;

        const double first_ball_speed = SP.firstBallSpeed( ball_move_dist, step );

        //#if (defined DEBUG_PRINT_DIRECT_PASS) || (defined DEBUG_PRINT_LEADING_PASS) || (defined DEBUG_PRINT_THROUGH_PASS) || (defined DEBUG_PRINT_FAILED_PASS)
#ifdef DEBUG_PRINT_COMMON
        dlog.addText( Logger::PASS,
                      "%d: type=%c unum=%d recvPos=(%.2f %.2f) step=%d ballMoveDist=%.2f speed=%.3f",
                      M_total_count, to_pass_char( M_pass_type ),
                      receiver.player_->unum(),
                      receive_point.x, receive_point.y,
                      step,
                      ball_move_dist,
                      first_ball_speed );
#endif

        if ( first_ball_speed < min_first_ball_speed )
        {
#ifdef DEBUG_PRINT_FAILED_PASS
            dlog.addText( Logger::PASS,
                          "%d: xxx type=%c unum=%d (%.1f %.1f) step=%d firstSpeed=%.3f < min=%.3f",
                          M_total_count, to_pass_char( M_pass_type ),
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y,
                          step,
                          first_ball_speed, min_first_ball_speed );
#endif
            break;
        }

        if ( max_first_ball_speed < first_ball_speed )
        {
#ifdef DEBUG_PRINT_FAILED_PASS
            dlog.addText( Logger::PASS,
                          "%d: xxx type=%c unum=%d (%.1f %.1f) step=%d firstSpeed=%.3f > max=%.3f",
                          M_total_count, to_pass_char( M_pass_type ),
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y,
                          step,
                          first_ball_speed, max_first_ball_speed );
#endif

            continue;
        }

        const double receive_ball_speed = first_ball_speed * std::pow( SP.ballDecay(), step );
        if ( receive_ball_speed < min_receive_ball_speed )
        {
#ifdef DEBUG_PRINT_FAILED_PASS
            dlog.addText( Logger::PASS,
                          "%d: xxx type=%c unum=%d (%.1f %.1f) step=%d recvSpeed=%.3f < min=%.3f",
                          M_total_count, to_pass_char( M_pass_type ),
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y,
                          step,
                          receive_ball_speed, min_receive_ball_speed );
#endif
            break;
        }

        if ( max_receive_ball_speed < receive_ball_speed )
        {
#ifdef DEBUG_PRINT_FAILED_PASS
            dlog.addText( Logger::PASS,
                          "%d: xxx type=%c unum=%d (%.1f %.1f) step=%d recvSpeed=%.3f > max=%.3f",
                          M_total_count, to_pass_char( M_pass_type ),
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y,
                          step,
                          receive_ball_speed, max_receive_ball_speed );
#endif
            continue;
        }

        const int n_kick = FieldAnalyzer::predict_kick_count( wm,
                                                              M_passer,
                                                              first_ball_speed,
                                                              ball_move_angle );
        CooperativeAction::SafetyLevel safety_level = getSafetyLevel( wm,
                                                                      M_first_point,
                                                                      first_ball_speed,
                                                                      ball_move_angle,
                                                                      receive_point,
                                                                      n_kick,
                                                                      step );
        if ( safety_level != CooperativeAction::Failure )
        {
            // opponent can reach the ball faster than the receiver.
            // then, break the loop, because ball speed is decreasing in the loop.

            CooperativeAction::Ptr pass( new ActPass( M_passer->unum(),
                                                      receiver.player_->unum(),
                                                      receive_point,
                                                      ( receive_point - M_first_point ).setLengthVector( first_ball_speed ),
                                                      n_kick - 1 + step,
                                                      n_kick,
                                                      description ) );
            pass->setIndex( M_total_count );
            pass->setSafetyLevel( safety_level );

            switch ( M_pass_type ) {
            case ActPass::DIRECT:
                pass->setMode( ActPass::DIRECT );
                M_direct_size += 1;
                if ( 1 <= receiver.player_->unum() && receiver.player_->unum() <= 11 )
                {
                    M_direct_pass[receiver.player_->unum()-1].push_back( pass );
                }
                break;
            case ActPass::LEADING:
                pass->setMode( ActPass::LEADING );
                M_leading_size += 1;
                if ( 1 <= receiver.player_->unum() && receiver.player_->unum() <= 11 )
                {
                    M_leading_pass[receiver.player_->unum()-1].push_back( pass );
                }
                break;
            case ActPass::THROUGH:
                pass->setMode( ActPass::THROUGH );
                M_through_size += 1;
                if ( 1 <= receiver.player_->unum() && receiver.player_->unum() <= 11 )
                {
                    M_through_pass[receiver.player_->unum()-1].push_back( pass );
                }
                break;
            default:
                break;
            }
#ifndef USE_FILTER
            M_courses.push_back( pass );
#endif

#ifdef DEBUG_PRINT_SUCCESS_PASS
            dlog.addText( Logger::PASS,
                          "%d: ok type=%c safe=%d target=%d"
                          " n_kick=%d bstep=%d (%.1f %.1f)->(%.1f %.1f) "
                          " speed=%.3f->%.3f dir=%.1f",
                          M_total_count, to_pass_char( M_pass_type ), safety_level,
                          receiver.player_->unum(),
                          n_kick, step,
                          M_first_point.x, M_first_point.y,
                          receive_point.x, receive_point.y,
                          first_ball_speed,
                          first_ball_speed * std::pow( ServerParam::i().ballDecay(), step ),
                          ball_move_angle.degree() );
            debug_paint_pass( M_total_count, receive_point, safety_level, "#0F0");
#endif
        }
#ifdef DEBUG_PRINT_FAILED_PASS
        else
        {
            dlog.addText( Logger::PASS,
                          "%d: xxx type=%c safe=%d target=%d"
                          " n_kick=%d bstep=%d (%.1f %.1f)->(%.1f %.1f) "
                          " speed=%.3f->%.3f dir=%.1f",
                          M_total_count, to_pass_char( M_pass_type ), safety_level,
                          receiver.player_->unum(),
                          n_kick, step,
                          M_first_point.x, M_first_point.y,
                          receive_point.x, receive_point.y,
                          first_ball_speed,
                          first_ball_speed * std::pow( ServerParam::i().ballDecay(), step ),
                          ball_move_angle.degree() );
            debug_paint_pass( M_total_count, receive_point, safety_level, "#F00" );
        }
#endif
        break;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
const GeneratorPass::Receiver *
GeneratorPass::getNearestReceiver( const Vector2D & pos )
{
    const Receiver * receiver = static_cast< const Receiver * >( 0 );
    double min_dist2 = std::numeric_limits< double >::max();

    for ( ReceiverCont::iterator p = M_receiver_candidates.begin();
          p != M_receiver_candidates.end();
          ++p )
    {
        double d2 = p->pos_.dist2( pos );
        if ( d2 < min_dist2 )
        {
            min_dist2 = d2;
            receiver = &(*p);
        }
    }

    return receiver;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
GeneratorPass::predictReceiverReachStep( const Receiver & receiver,
                                         const Vector2D & pos,
                                         const bool use_penalty )
{
    const PlayerType * ptype = receiver.player_->playerTypePtr();
    double target_dist = receiver.inertia_pos_.dist( pos );

    int penalty_step = 0;

    int n_turn = ( receiver.player_->bodyCount() > 0
                   ? 1
                   : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                               receiver.player_->body(),
                                                               receiver.speed_,
                                                               target_dist,
                                                               ( pos - receiver.inertia_pos_ ).th(),
                                                               ptype->kickableArea(),
                                                               true ) ); // use back dash
    double dash_dist = target_dist;

    // if ( receiver.pos_.x < pos.x )
    // {
    //     dash_dist -= ptype->kickableArea() * 0.5;
    // }

    if ( use_penalty )
    {
        dash_dist += receiver.penalty_distance_;
    }

    // if ( M_pass_type == ActPass::THROUGH )
    // {
    //     dash_dist -= ptype->kickableArea() * 0.5;
    // }

    if ( M_pass_type == ActPass::LEADING )
    {
        // if ( pos.x > -20.0
        //      && dash_dist < ptype->kickableArea() * 1.5 )
        // {
        //     dash_dist -= ptype->kickableArea() * 0.5;
        // }

        // if ( pos.x < 30.0 )
        // {
        //     dash_dist += 0.3;
        // }

        //dash_dist *= 1.05;
        //dash_dist *= 1.1;
        Line2D ball_move_line( M_first_point, pos );
        double player_line_dist = ball_move_line.dist( receiver.pos_ );
        //penalty_step = static_cast< int >( std::floor( player_line_dist * 0.3 ) );
        penalty_step += std::min( 2, std::max( 0, ptype->cyclesToReachDistance( player_line_dist ) - 1 ) );

        // AngleDeg dash_angle = ( pos - receiver.pos_ ).th() ;

        // if ( dash_angle.abs() > 90.0
        //      || receiver.player_->bodyCount() > 1
        //      || ( dash_angle - receiver.player_->body() ).abs() > 30.0 )
        // {
        //     n_turn += 1;
        // }
    }

    int n_dash = ptype->cyclesToReachDistance( dash_dist );

    if ( n_turn > 0 )
    {
        penalty_step += 1;
    }

#ifdef DEBUG_PREDICT_RECEIVER
    dlog.addText( Logger::PASS,
                  "%d: receiver=%d receivePos=(%.1f %.1f) dist=%.2f dash=%.2f turn=%d dash=%d penalty=%d",
                  M_total_count,
                  receiver.player_->unum(),
                  pos.x, pos.y,
                  target_dist, dash_dist,
                  n_turn, n_dash, penalty_step );
#endif
    return ( n_turn == 0
             ? n_turn + n_dash + penalty_step
             : n_turn + n_dash + penalty_step ); // 1 step penalty for observation delay.
    // if ( ! use_penalty )
    // {
    //     return n_turn + n_dash;
    // }
    // return n_turn + n_dash + 1; // 1 step penalty for observation delay.
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::SafetyLevel
GeneratorPass::getSafetyLevel( const WorldModel & wm,
                               const Vector2D & first_ball_pos,
                               const double first_ball_speed,
                               const AngleDeg & ball_move_angle,
                               const Vector2D & receive_point,
                               const int n_kick,
                               const int ball_move_step )
{
    const Vector2D first_ball_vel = Vector2D::from_polar( first_ball_speed, ball_move_angle );
    //const double kick_decay = std::pow( KICK_COUNT_FACTOR, n_kick - 1 );

#ifdef DEBUG_PRINT_COMMON
    dlog.addText( Logger::PASS,
                  "%d: type=%c target=(%.1f %.1f) n_kick=%d bstep=%d",
                  M_total_count, to_pass_char( M_pass_type ),
                  receive_point.x, receive_point.y,
                  n_kick, ball_move_step );
#endif

    CooperativeAction::SafetyLevel result = CooperativeAction::Safe;

    for ( OpponentCont::const_iterator o = M_opponents.begin(),
              o_end = M_opponents.end();
          o != o_end;
          ++o )
    {
        CooperativeAction::SafetyLevel level = getOpponentSafetyLevel( wm,
                                                                       *o,
                                                                       first_ball_pos,
                                                                       first_ball_vel,
                                                                       first_ball_speed,
                                                                       ball_move_angle,
                                                                       receive_point,
                                                                       n_kick,
                                                                       ball_move_step );
        if ( result > level )
        {
            result = level;
            if ( result == CooperativeAction::Failure )
            {
                break;
            }
        }
    }

    //return ( 1.0 - failure_prob ) * kick_decay;
    return result;
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::SafetyLevel
GeneratorPass::getOpponentSafetyLevel( const WorldModel & wm,
                                       const Opponent & opponent,
                                       const Vector2D & first_ball_pos,
                                       const Vector2D & first_ball_vel,
                                       const double /*first_ball_speed*/,
                                       const rcsc::AngleDeg & ball_move_angle,
                                       const rcsc::Vector2D & receive_point,
                                       const int /*n_kick*/,
                                       const int ball_move_step )
{
    static const double CONTROL_AREA_BUF = 0.15;  // buffer for kick table

    const ServerParam & SP = ServerParam::i();
    const AbstractPlayerObject * pl = opponent.player_;
    const PlayerType * ptype = pl->playerTypePtr();

    // remove 2013-06-11
    //     if ( ( pl->angleFromBall() - ball_move_angle ).abs() > 105.0 )
    //     {
    // #ifdef DEBUG_PRINT_COMMON
    //         dlog.addText( Logger::PASS,
    //                       "%d: opponent[%d](%.1f %.1f) backside. never reach.",
    //                       M_total_count,
    //                       pl->unum(), opponent.pos_.x, opponent.pos_.y );
    // #endif
    //         return CooperativeAction::Safe;
    //     }

    int min_step = FieldAnalyzer::estimate_min_reach_cycle( opponent.pos_,
                                                            ptype->kickableArea() + 0.2,
                                                            ptype->realSpeedMax(),
                                                            first_ball_pos,
                                                            ball_move_angle );
    if ( min_step < 0 )
    {
#ifdef DEBUG_PRINT_COMMON
        dlog.addText( Logger::PASS,
                      "%d: opponent[%d](%.1f %.1f) never reach.",
                      M_total_count,
                      pl->unum(), opponent.pos_.x, opponent.pos_.y );
#endif
        return CooperativeAction::Safe;
    }

    if ( min_step == 0 )
    {
        min_step = 1;
    }

    const bool aggressive = ( M_pass_type == ActPass::THROUGH
                              || receive_point.x > 35.0
                              || receive_point.x > wm.offsideLineX() - 3.0 );

    CooperativeAction::SafetyLevel result = CooperativeAction::Safe;

    Vector2D ball_pos = inertia_n_step_point( first_ball_pos,
                                              first_ball_vel,
                                              min_step - 1,
                                              SP.ballDecay() );
    Vector2D ball_vel = first_ball_vel * std::pow( SP.ballDecay(), min_step - 1 );

    //const int max_step = n_kick - 1 + ball_move_step;
    const int max_step = ball_move_step;
    for ( int step = min_step; step <= max_step; ++step )
    {
        ball_pos += ball_vel;
        ball_vel *= SP.ballDecay();

        const bool goalie = ( pl->goalie()
                              && ball_pos.x > SP.theirPenaltyAreaLineX()
                              && ball_pos.absY() < SP.penaltyAreaHalfWidth() );
        double control_area = ( goalie
                                ? SP.catchableArea() + 0.1
                                : ptype->kickableArea() );
        const Vector2D opponent_pos = ptype->inertiaPoint( opponent.pos_, opponent.vel_, step );
        const double ball_dist = opponent_pos.dist( ball_pos );
        double dash_dist = ball_dist;

        //dash_dist -= opponent.bonus_distance_;

        if ( ! pl->isTackling()
             && dash_dist - control_area - CONTROL_AREA_BUF < 0.001 )
        {
#ifdef DEBUG_PRINT_COMMON
            dlog.addText( Logger::PASS,
                          "%d: opponent[%d](%.2f %.2f) step=%d controllable",
                          M_total_count,
                          pl->unum(), pl->pos().x, pl->pos().y, step );
#endif
            return CooperativeAction::Failure;
        }

        if ( pl->bodyCount() == 0 )
        {
            if ( step == 1
                 && ! pl->isTackling() )
            {
                Vector2D player_2_ball = ( ball_pos - opponent_pos ).rotatedVector( -pl->body() );
                if ( player_2_ball.x > 0.0 )
                {
                    double fail_prob = std::pow( player_2_ball.x, SP.tackleExponent() )
                        + std::pow( player_2_ball.absY(), SP.tackleExponent() );
                    if ( 1.0 - fail_prob > 0.9 )
                    {
#ifdef DEBUG_PRINT_COMMON
                        dlog.addText( Logger::PASS,
                                      "%d: opponent[%d](%.2f %.2f) step=%d tacklable by 1 step",
                                      M_total_count,
                                      pl->unum(), pl->pos().x, pl->pos().y, step );
#endif
                        return CooperativeAction::Failure;
                    }
                }
            }
            else
            {
                if ( dash_dist < 5.0 // magic number
                     && ( pl->body() - ( ball_pos - opponent_pos ).th() ).abs() < 90.0 )
                {
                    control_area = std::max( control_area, SP.tackleDist() - 0.2 );
                }
            }
        }

        if ( M_pass_type == ActPass::THROUGH
             && first_ball_vel.x > 2.0
             && ( receive_point.x > wm.offsideLineX()
                  || receive_point.x > 30.0 ) )
        {
            if ( step == 1 )
            {
                dash_dist -= control_area + CONTROL_AREA_BUF;
            }
            else
            {
                dash_dist -= std::max( 0.1, control_area + CONTROL_AREA_BUF - 0.075 * step );
                //dash_dist -= std::max( 0.1, control_area + CONTROL_AREA_BUF - 0.075 * step );
                //dash_dist -= std::max( 0.1, control_area + CONTROL_AREA_BUF - 0.1 * step );
                //dash_dist -= control_area * 0.5;
            }
        }
        else
        {
            if ( receive_point.x < 25.0 )
            {
                dash_dist -= control_area;
                //dash_dist -= 0.5;
                dash_dist -= std::min( 0.5, 0.1 * step );
                //dash_dist -= std::min( 0.5, 0.05 * step ); // 2013-06-07
            }
            else
            {
                dash_dist -= control_area;
                //dash_dist -= 0.2;
                dash_dist -= std::min( 0.2, 0.1 * step );
                //dash_dist -= std::min( 0.2, 0.05 * step ); // 2013-06-07
            }
        }


        if ( dash_dist > ptype->realSpeedMax() * ( step + std::min( pl->posCount(), 5 ) ) ) // + 1.0 )
        {
            // #ifdef DEBUG_PRINT_COMMON
            //             dlog.addText( Logger::PASS,
            //                           "%d: opponent[%d](%.2f %.2f) over movable area. step=%d dash_dist=%f max=%f",
            //                           M_total_count,
            //                           pl->unum(), pl->pos().x, pl->pos().y,
            //                           step,
            //                           dash_dist, ptype->realSpeedMax() * ( step + pl->posCount() ) + 1.0 );
            // #endif
            continue;
        }

        const int opponent_dash = ptype->cyclesToReachDistance( dash_dist );

        const int body_count_thr = ( aggressive ? 1 : 0 );
        const int opponent_turn = ( pl->bodyCount() > body_count_thr
                                    ? 0
                                    : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                                pl->body(),
                                                                                opponent.speed_,
                                                                                ball_dist,
                                                                                ( ball_pos - opponent_pos ).th(),
                                                                                control_area,
                                                                                true ) ); // use back dash
        int opponent_step = ( opponent_turn == 0
                              ? opponent_dash
                              : opponent_turn + opponent_dash + 1 );
        if ( pl->isTackling() )
        {
            opponent_step += 2; // magic number
        }
        else if ( pl->goalie() && ! goalie )
        {
            if ( opponent_step >= 25 )
            {
                // ignore goalie
                continue;
            }
            opponent_step += 1;
        }
        else if ( aggressive )
        {
            opponent_step -= bound( 0,
                                    //static_cast< int >( std::floor( pl->posCount() * 0.5 ) ),
                                    //static_cast< int >( std::floor( pl->posCount() * 0.7 ) ),
                                    static_cast< int >( std::floor( pl->posCount() * 0.3 ) ),
                                    //4 );
                                    2 );
            if ( ball_pos.x > opponent_pos.x + 15.0 )
            {
                if ( opponent_step > 15 ) opponent_step += 1;
                //if ( opponent_step > 20 ) opponent_step += 1;
            }
        }
        else
        {
            opponent_step -= bound( 0,
                                    static_cast< int >( std::floor( pl->posCount() * 0.7 ) ),
                                    5 );
        }

        CooperativeAction::SafetyLevel level = CooperativeAction::Safe;

        if ( opponent_step <= std::max( 0, step - 3 ) ) level = CooperativeAction::Failure;
        else if ( opponent_step <= std::max( 0, step - 2 ) ) level = CooperativeAction::Failure;
        else if ( opponent_step <= std::max( 0, step - 1 ) ) level = CooperativeAction::Failure;
        else if ( opponent_step <= step ) level = CooperativeAction::Failure;
        else if ( opponent_step <= step + 1 ) level = CooperativeAction::Dangerous;
        else if ( opponent_step <= step + 2 ) level = CooperativeAction::MaybeDangerous;
        else if ( opponent_step <= step + 3 ) level = CooperativeAction::Safe;
        else level = CooperativeAction::Safe;

        if ( step == 1
             && opponent_step == 1
             && opponent_dash == 1 )
        {
            level = CooperativeAction::Dangerous;
        }

        //         if ( attack )
        //         {
        // #ifdef DEBUG_PRINT_COMMON
        //             dlog.addText( Logger::PASS,
        //                           "%d: opponent[%d](%.2f %.2f) decay interfare prob. original=%f new=%f",
        //                           M_total_count,
        //                           pl->unum(), pl->pos().x, pl->pos().y,
        //                           prob, prob*OFFENSIVE_PROB_FACTOR );
        // #endif
        //             prob *= OFFENSIVE_PROB_FACTOR;
        //         }

#ifdef DEBUG_PRINT_COMMON
        dlog.addText( Logger::PASS,
                      "%d: bpos(%.2f %.2f) opponent[%d](%.2f %.2f) bstep=%d ostep=%d(t%d d%d) dash_dist=%.3f safe=%d",
                      M_total_count,
                      ball_pos.x, ball_pos.y,
                      pl->unum(), pl->pos().x, pl->pos().y,
                      step, opponent_step, opponent_turn, opponent_dash, dash_dist, level );
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
