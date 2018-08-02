// -*-c++-*-

/*!
  \file generator_keep_dribble.cpp
  \brief keep dribble generator Source File
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

#include "generator_keep_dribble.h"

#include "act_dribble.h"
#include "field_analyzer.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/logger.h>
#include <rcsc/geom/matrix_2d.h>
#include <rcsc/timer.h>

#include <algorithm>
#include <limits>

#include <cmath>

// #define DEBUG_PROFILE

// #define DEBUG_PRINT_COMMON

// #define DEBUG_PRINT_SIMULATE_DASHES
// #define DEBUG_PRINT_SIMULATE_KICK_DASHES
// #define DEBUG_PRINT_SIMULATE_TURN_KICK_DASHES
// #define DEBUG_PRINT_SIMULATE_KICK_TURN_DASHES
// #define DEBUG_PRINT_SIMULATE_COLLIDE_TURN_KICK_DASHES

// #define DEBUG_PRINT_RESULTS
// #define DEBUG_PRINT_ERASE

// #define DEBUG_PRINT_SIMULATE_KICK_DASHES_IMPL
// #define DEBUG_PRINT_SAFETY_LEVEL
// #define DEBUG_PRINT_OPPONENT_CHECK

using namespace rcsc;

namespace {

/*

 */
enum {
    REASON_SUCCESS,
    REASON_OUT_OF_PITCH,
    REASON_COLLISION,
    REASON_NOKICKABLE_FRONT,
    REASON_NOKICKABLE_BACK,
    REASON_OPPONENT,
};

/*

 */
int g_kick_dashes_impl_reason = REASON_SUCCESS;

/*-------------------------------------------------------------------*/
/*!

 */
const char *
mode_string( const int mode )
{
    switch ( mode ) {
    case ActDribble::KICK_TURN_DASHES:
        return "KTD";
    case ActDribble::OMNI_KICK_DASHES:
        return "Omni";
    case ActDribble::KEEP_DASHES:
        return "KeepD";
    case ActDribble::KEEP_KICK_DASHES:
        return "KeepKD";
    case ActDribble::KEEP_KICK_TURN_DASHES:
        return "KeepKTD";
    case ActDribble::KEEP_TURN_KICK_DASHES:
        return "KeepTKD";
    case ActDribble::KEEP_COLLIDE_TURN_KICK_DASHES:
        return "KeepCTKD";
    default:
        break;
    };

    return "Unknown";
}

/*-------------------------------------------------------------------*/
/*!

 */
const char *
reason_string( const int reason )
{
    switch ( reason ) {
    case REASON_SUCCESS:
        return "Success";
    case REASON_OUT_OF_PITCH:
        return "out-of-pitch";
    case REASON_COLLISION:
        return "collision";
    case REASON_NOKICKABLE_FRONT:
        return "nokickable-front";
    case REASON_NOKICKABLE_BACK:
        return "nokickable-back";
    case REASON_OPPONENT:
        return "opponent";
    default:
        break;
    }
    return "Unknown";
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
is_bad_dash_angle( const WorldModel & wm,
                   const AngleDeg & dash_angle )
{
    if ( wm.self().pos().x < 3.0
         && dash_angle.abs() > 100.0 )
    {
#ifdef DEBUG_PRINT_COMMON
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(is_bad_dash_angle) (1) dash_angle=%.1f",
                      dash_angle.degree() );
#endif
        return true;
    }

    if ( wm.self().pos().x < -36.0
         && wm.self().pos().absY() < 20.0
         && dash_angle.abs() > 45.0 )
    {
#ifdef DEBUG_PRINT_COMMON
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(is_bad_dash_angle) cancel(2) dash_angle=%.1f",
                      dash_angle.degree() );
#endif
        return true;
    }

    if ( wm.self().pos().x > ServerParam::i().pitchHalfLength() - 1.0
         && dash_angle.abs() < 90.0 )
    {
#ifdef DEBUG_PRINT_COMMON
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(is_bad_dash_angle) cancel(3) dash_angle=%.1f self_x=%.1f",
                      dash_angle.degree(), wm.self().pos().x );
#endif
        return true;
    }

    if ( wm.self().pos().y > ServerParam::i().pitchHalfWidth() - 3.0
         && dash_angle.degree() > 0.0 )
    {
#ifdef DEBUG_PRINT_COMMON
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(is_bad_dash_angle) cancel(4) dash_angle=%.1f self_y=%.1f",
                      dash_angle.degree(), wm.self().pos().y );
#endif
        return true;
    }

    if ( wm.self().pos().y < -ServerParam::i().pitchHalfWidth() + 3.0
         && dash_angle.degree() < 0.0 )
    {
#ifdef DEBUG_PRINT_COMMON
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(is_bad_dash_angle) cancel(5) dash_angle=%.1f self_y=%.1f",
                      dash_angle.degree(), wm.self().pos().y );
#endif
        return true;
    }

    return false;
}

enum InterceptType {
    INTERCEPT,
    MAY_TACKLE,
    NO_INTERCEPT,
};

/*-------------------------------------------------------------------*/
/*!

 */
InterceptType
check_opponent_intercept_next_cycle( const WorldModel & wm,
                                     const Vector2D & ball_next )
{
    const double speed_rate = ( ball_next.x > 30.0
                                ? 0.3
                                : ball_next.x > 0.0
                                ? 0.7
                                : 1.0 );

    const bool penalty_area = ( ball_next.x > ServerParam::i().theirPenaltyAreaLineX()
                                && ball_next.absY() < ServerParam::i().penaltyAreaHalfWidth() );
    const bool shootable_area = ( ServerParam::i().theirTeamGoalPos().dist2( ball_next )
                                  < std::pow( 15.0, 2 ) );
    Matrix2D rotate_matrix;
    Vector2D self_next;
    Vector2D ball_rel;
    if ( shootable_area )
    {
        self_next = wm.self().pos() + wm.self().vel();
        rotate_matrix = Matrix2D::make_rotation( -wm.self().body() );
        ball_rel = rotate_matrix.transform( ball_next - self_next );
    }

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        if ( (*o)->distFromSelf() > 3.0 ) break;
        if ( (*o)->posCount() > 3 ) continue;
        if ( (*o)->isGhost() ) continue;
        if ( (*o)->isTackling() ) continue;

        const PlayerType * ptype = (*o)->playerTypePtr();
        const double control_area = ( penalty_area && (*o)->goalie()
                                      ? ptype->maxCatchableDist()
                                      : (*o)->playerTypePtr()->kickableArea() );

        Vector2D opponent_pos = (*o)->pos() + (*o)->vel();

        if ( opponent_pos.dist2( ball_next ) < std::pow( control_area, 2 ) )
        {
#ifdef DEBUG_PRINT_OPPONENT_CHECK
            dlog.addText( Logger::DRIBBLE,
                          "(GeneratorKeepDribble) opponent[%d] kickable. ball=(%.2f %.2f)",
                          (*o)->unum(), ball_next.x, ball_next.y );
#endif
            return INTERCEPT;
        }

        if ( shootable_area )
        {
            Vector2D opponent_rel = rotate_matrix.transform( opponent_pos - self_next );
            if ( opponent_rel.x < 0.0
                 && opponent_rel.absY() < ptype->playerSize() + wm.self().playerType().playerSize() + 0.1 )
            {
#ifdef DEBUG_PRINT_OPPONENT_CHECK
                dlog.addText( Logger::DRIBBLE,
                              "(GeneratorKeepDribble) opponent[%d] backside in shootable area. ignored",
                              (*o)->unum() );
#endif
                continue;
            }
        }

        double one_step_speed = ( ptype->dashPowerRate()
                                  * ServerParam::i().maxDashPower()
                                  * ptype->effortMax() )
            * speed_rate;

        if ( (*o)->bodyCount() <= 1 )
        {
            Vector2D player_to_ball = ( ball_next - opponent_pos ).rotatedVector( -(*o)->body() );
            if ( player_to_ball.absY() > ServerParam::i().tackleWidth() )
            {
                continue;
            }

            if ( player_to_ball.x > 0.0 )
            {
                double x_dist = std::max( 0.0, player_to_ball.x - one_step_speed );
                double foul_fail_prob
                    = std::pow( x_dist / ServerParam::i().tackleDist(),
                                ServerParam::i().foulExponent() )
                    + std::pow( player_to_ball.absY() / ServerParam::i().tackleWidth(),
                                ServerParam::i().foulExponent() );
                if ( foul_fail_prob < 1.0 )
                {
                    if (  1.0 - foul_fail_prob > 0.9 )
                    {
#ifdef DEBUG_PRINT_OPPONENT_CHECK
                        dlog.addText( Logger::DRIBBLE,
                                      "(GeneratorKeepDribble) opponent[%d] will tackle. prob=%.3f ball=(%.2f %.2f)",
                                      (*o)->unum(), 1.0 - foul_fail_prob, ball_next.x, ball_next.y );
#endif
                        return INTERCEPT;
                    }

#ifdef DEBUG_PRINT_OPPONENT_CHECK
                    dlog.addText( Logger::DRIBBLE,
                                  "(GeneratorKeepDribble) opponent[%d] may tackle. prob=%.3f ball=(%.2f %.2f)",
                                  (*o)->unum(), 1.0 - foul_fail_prob, ball_next.x, ball_next.y );
#endif
                    return MAY_TACKLE;
                }
            }
        }
    }

    return NO_INTERCEPT;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
opponent_may_tackle_after_wait( const WorldModel & wm )
{
    const double tackle_dist = ServerParam::i().tackleDist();
    const double tackle_width = ServerParam::i().tackleWidth();
    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromBall().begin(),
              end = wm.opponentsFromBall().end();
          o != end;
          ++o )
    {
        if ( (*o)->distFromSelf() >= 3.0 ) break;
        if ( (*o)->posCount() >= 5 ) continue;
        if ( (*o)->isGhost() ) continue;
        if ( (*o)->isTackling() ) continue;

        const PlayerType * ptype = (*o)->playerTypePtr();
        const Vector2D opponent_pos = (*o)->pos() + (*o)->vel();

        if ( (*o)->bodyCount() <= 1 )
        {
            Vector2D player_to_ball = ( ball_next - opponent_pos ).rotatedVector( -(*o)->body() );

            if ( player_to_ball.absY() > tackle_width - 0.1 )
            {
                continue;
            }

            const double one_step_speed = ( ptype->dashPowerRate()
                                            * ServerParam::i().maxDashPower()
                                            * ptype->effortMax() );

            if ( player_to_ball.x < one_step_speed + tackle_dist*0.9 )
            {
                dlog.addText( Logger::DRIBBLE,
                              "(GeneratorKeepDribble) opponent[%d] may tackle after wait. y=%.3f x=%.3f",
                              (*o)->unum(),
                              player_to_ball.y, player_to_ball.x );
                return true;
            }
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
opponent_may_tackle_after_collide_turn( const WorldModel & wm )
{
    const double tackle_dist = ServerParam::i().tackleDist();
    const double tackle_width = ServerParam::i().tackleWidth();
    const Vector2D ball_pos = wm.self().pos()
        + ( wm.ball().pos() - wm.self().pos() ).setLengthVector( ServerParam::i().ballSize()
                                                                 + wm.self().playerType().playerSize() );

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromBall().begin(),
              end = wm.opponentsFromBall().end();
          o != end;
          ++o )
    {
        if ( (*o)->distFromSelf() >= 5.0 ) break;
        if ( (*o)->posCount() >= 10 ) continue;
        if ( (*o)->isGhost() ) continue;
        if ( (*o)->isTackling() ) continue;

        const PlayerType * ptype = (*o)->playerTypePtr();
        const Vector2D opponent_pos = (*o)->pos() + (*o)->vel();
        const double ball_dist2 = opponent_pos.dist2( ball_pos );

        if ( ball_dist2 < std::pow( tackle_dist + 0.1, 2 ) )
        {
            dlog.addText( Logger::DRIBBLE,
                          "(GeneratorKeepDribble) opponent[%d] may tackle(1)",
                          (*o)->unum() );
            return true;
        }

        const double one_step_speed = ( ptype->dashPowerRate()
                                        * ServerParam::i().maxDashPower()
                                        * ptype->effortMax() );

        if ( (*o)->bodyCount() <= 1 )
        {
            Vector2D player_to_ball = ( ball_pos - opponent_pos ).rotatedVector( -(*o)->body() );
            const double one_step_speed = ( ptype->dashPowerRate()
                                            * ServerParam::i().maxDashPower()
                                            * ptype->effortMax() ) * 2.0;

            double dash_step = 2.0;
            if ( player_to_ball.absY() > tackle_width )
            {
                dash_step -= 1.0;
            }

            double two_step_dist = one_step_speed * dash_step;

            if ( player_to_ball.x > 0.0
                 && player_to_ball.x < two_step_dist + tackle_dist*0.9 )
            {
                dlog.addText( Logger::DRIBBLE,
                              "(GeneratorKeepDribble) opponent[%d] may tackle(2). y=%.3f x=%.3f",
                              (*o)->unum(),
                              player_to_ball.y, player_to_ball.x );
                return true;
            }

            Vector2D player_to_self = ( wm.self().pos() - opponent_pos ).rotatedVector( -(*o)->body() );
            if ( player_to_self.x > 0.0
                 && player_to_self.absY() < wm.self().playerType().playerSize() + ptype->playerSize() + 0.1
                 && player_to_self.x < two_step_dist + wm.self().playerType().playerSize() + ptype->playerSize() + 0.1 )
            {
                dlog.addText( Logger::DRIBBLE,
                              "(GeneratorKeepDribble) opponent[%d] may collide. y=%.3f x=%.3f",
                              (*o)->unum(),
                              player_to_self.y, player_to_self.x );
                return true;
            }
        }
        else
        {
            if ( ball_dist2 < one_step_speed*2.0 + tackle_dist*0.9 )
            {
                dlog.addText( Logger::DRIBBLE,
                              "(GeneratorKeepDribble) opponent[%d] may tackle(3)",
                              (*o)->unum() );
                return true;
            }
        }
    }

    return false;
}

/*

 */
struct CandidateSorter {
    bool operator()( const GeneratorKeepDribble::Candidate & lhs,
                     const GeneratorKeepDribble::Candidate & rhs ) const
      {
          if ( lhs.action_->safetyLevel() == rhs.action_->safetyLevel() )
          {
              if ( lhs.action_->dashCount() == rhs.action_->dashCount() )
              {
                  return lhs.opponent_dist_ > rhs.opponent_dist_;
              }
              return lhs.action_->dashCount() > rhs.action_->dashCount();
          }
          return lhs.action_->safetyLevel() > rhs.action_->safetyLevel();
      }
};

/*-------------------------------------------------------------------*/
/*!

 */
void
erase_redundant_candidates( std::vector< GeneratorKeepDribble::Candidate > & candidates )
{
    if ( candidates.empty() )
    {
        return;
    }

    std::sort( candidates.begin(), candidates.end(), CandidateSorter() );

    CooperativeAction::SafetyLevel level = CooperativeAction::Failure;

    std::vector< GeneratorKeepDribble::Candidate >::iterator it = candidates.begin();
    while ( it != candidates.end() )
    {
        if ( it->action_->safetyLevel() == level )
        {
#ifdef DEBUG_PRINT_ERASE
            dlog.addText( Logger::DRIBBLE,
                          "(GeneratorKeepDribble) erased %d mode=%s dash=%d move=%.3f safe=%d",
                          it->action_->index(),
                          mode_string( it->action_->mode() ),
                          it->action_->dashCount(),
                          it->self_move_dist_,
                          it->action_->safetyLevel() );
#endif
            it = candidates.erase( it );
        }
        else
        {
#ifdef DEBUG_PRINT_ERASE
            dlog.addText( Logger::DRIBBLE,
                          "(GeneratorKeepDribble) saved %d mode=%s dash=%d move=%.3f safe=%d",
                          it->action_->index(),
                          mode_string( it->action_->mode() ),
                          it->action_->dashCount(),
                          it->self_move_dist_,
                          it->action_->safetyLevel() );
#endif
            level = it->action_->safetyLevel();
            ++it;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
const double congestion_factor = 1.0 / ( 2.0 * std::pow( 5.0, 2 ) );

double
calc_congestion( const WorldModel & wm,
                 const CooperativeAction & act )
{
    double value = 0.0;

    for ( PlayerObject::Cont::const_iterator p = wm.opponents().begin(), end = wm.opponents().end();
          p != end;
          ++p )
    {
        double d = (*p)->pos().dist( act.targetBallPos() );
        d -= (*p)->playerTypePtr()->kickableArea();
        d -= 0.4 * bound( 0, (*p)->posCount() - 1, 2 ); // Magic Number
        d = std::max( 0.0001, d );

        value += std::exp( -std::pow( d, 2 ) * congestion_factor );
    }

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::Ptr
get_best( std::vector< GeneratorKeepDribble::Candidate > & candidates )
{
    if ( candidates.empty() )
    {
        return CooperativeAction::Ptr();
    }

    std::vector< GeneratorKeepDribble::Candidate >::iterator best = candidates.begin();

    for ( std::vector< GeneratorKeepDribble::Candidate >::iterator it = candidates.begin() + 1,
              end = candidates.end();
          it != end;
          ++it )
    {
        if ( it->action_->dashCount() > best->action_->dashCount() )
        {
            best = it;
        }
        else if ( it->action_->dashCount() == best->action_->dashCount() )
        {
            if ( it->action_->safetyLevel() > best->action_->safetyLevel() )
            {
                best = it;
            }
            else if ( it->action_->safetyLevel() == best->action_->safetyLevel() )
            {
                if ( best->action_->safetyLevel() <= CooperativeAction::MaybeDangerous )
                {
                    if ( it->opponent_dist_ > best->opponent_dist_ )
                    {
                        best = it;
                    }
                }
                else
                {
                    if ( it->opponent_dist_ > best->opponent_dist_ )
                    {
                        best = it;
                    }
                }
            }
        }
    }

    return best->action_;
}
}


/*-------------------------------------------------------------------*/
/*!

 */
GeneratorKeepDribble::GeneratorKeepDribble()
    : M_update_time( -1, 0 ),
      M_total_count( 0 )
{
    M_kick_dashes_candidates.reserve( 32 );
    M_candidates.reserve( 256 );
#ifdef DEBUG_PRINT_RESULTS
    M_results.reserve( 256 );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorKeepDribble &
GeneratorKeepDribble::instance()
{
    static GeneratorKeepDribble s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorKeepDribble::clear()
{
    M_total_count = 0;

    M_dash_only_best.reset();
    M_kick_dashes_best.reset();
    M_kick_dashes_candidates.clear();

    M_candidates.clear();
#ifdef DEBUG_PRINT_RESULTS
    M_results.clear();
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorKeepDribble::generate( const WorldModel & wm )
{
    if ( M_update_time == wm.time() )
    {
        return;
    }
    M_update_time = wm.time();

    clear();

    if ( wm.gameMode().type() != GameMode::PlayOn
         && ! wm.gameMode().isPenaltyKickMode() )
    {
        // not a situation for dribble
        return;
    }

    if ( ! wm.self().isKickable()
         || wm.self().isFrozen() )
    {
        // never kickable
        return;
    }

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    generateImpl( wm );

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (generate) PROFILE elapsed %.3f [ms] trial=%d best=%zd all=%zd",
                  timer.elapsedReal(),
                  M_total_count,
                  M_candidates.size(),
                  M_results.size() );
#endif
#ifdef DEBUG_PRINT_RESULTS
    debugPrintResults( wm );
#endif
}


/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorKeepDribble::debugPrintResults( const WorldModel & wm )
{
    dlog.addText( Logger::DRIBBLE,
                  "(KeepDribble) candidate size = %zd", M_candidates.size() );
    dlog.addText( Logger::DRIBBLE,
                  "(KeepDribble) all candidate size = %zd", M_results.size() );

    for ( std::vector< Candidate >::const_iterator c = M_candidates.begin();
          c != M_candidates.end();
          ++c )
    {
        dlog.addText( Logger::DRIBBLE,
                      "__ (%s) index=%d angle=%.1f dash=%d",
                      mode_string( c->action_->mode() ), c->action_->index(),
                      ( c->action_->targetBallPos() - wm.self().pos() ).th().degree(),
                      c->action_->dashCount() );
    }

    for ( std::vector< boost::shared_ptr< Result > >::const_iterator result = M_results.begin();
          result != M_results.end();
          ++result )
    {
        const Result & r = **result;
        if ( ! r.action_ ) continue;

        const CooperativeAction & a = *r.action_;
        double angle = ( r.states_.size() < 2
                         ? -360.0
                         : ( r.states_.back().self_pos_ - r.states_.front().self_pos_ ).th().degree() );

        dlog.addText( Logger::DRIBBLE,
                      "(%s) : %d k=%d t=%d d=%d angle=%.0f"
                      //" power=%.1f dir=%.1f"
                      " target_pos=(%.3f %.3f) safe=%d",
                      mode_string( a.mode() ),
                      a.index(),
                      a.kickCount(),
                      a.turnCount(),
                      a.dashCount(),
                      angle,
                      // a.firstDashPower(), a.firstDashDir(),
                      a.targetBallPos().x, a.targetBallPos().y,
                      a.safetyLevel() );

        //if ( a.index() == 99999 )
        if ( 0 )
        {
            const ServerParam & SP = ServerParam::i();
            const PlayerType & ptype = wm.self().playerType();

            int count = 1;
            for ( std::vector< State >::const_iterator s = r.states_.begin();
                  s != r.states_.end();
                  ++s, ++count )
            {
                char msg[8]; snprintf( msg, 8, "%d", count );
                dlog.addCircle( Logger::DRIBBLE,
                                s->self_pos_, ptype.kickableArea(),
                                255, count*16, 0 );
                dlog.addCircle( Logger::DRIBBLE,
                                s->self_pos_, ptype.playerSize(),
                                0, count*16, 255 );
                dlog.addMessage( Logger::DRIBBLE,
                                 s->self_pos_, msg,
                                 255, count*10, 0 );
                dlog.addCircle( Logger::DRIBBLE,
                                s->ball_pos_, SP.ballSize(), "#F0F" );
                dlog.addMessage( Logger::DRIBBLE,
                                 s->ball_pos_, msg, "#F0F" );
                dlog.addText( Logger::DRIBBLE,
                              ">>> step=%d ball_dist=%.3f kickable=%.3f",
                              count, s->self_pos_.dist( s->ball_pos_ ), ptype.kickableArea() );
            }
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorKeepDribble::generateImpl( const WorldModel & wm )
{
    const int angle_div = 20;
    const double angle_step = 360.0 / angle_div;

    const PlayerType & ptype = wm.self().playerType();
    const double self_current_speed = wm.self().vel().r();

    const bool tackle_after_wait = opponent_may_tackle_after_wait( wm );
    const bool tackle_after_collide = opponent_may_tackle_after_collide_turn( wm );


    //
    // angle loop
    //
    for ( int a = 0; a < angle_div; ++a )
    {
        AngleDeg dash_angle = wm.self().body() + ( angle_step * a );

#ifdef DEBUG_PRINT_COMMON
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (generateImpl) dash_angle=%.1f dir=%.1f",
                      dash_angle.degree(), angle_step*a );
#endif
        //
        // angle filter
        //
        if ( is_bad_dash_angle( wm, dash_angle ) )
        {
            continue;
        }

        if ( a == 0 )
        {
            // no turn
            if ( ! tackle_after_wait )
            {
                simulateDashes( wm );
            }
            if ( ! simulateKickDashes( wm ) )
            {
                simulateKick2Dashes( wm );
            }
        }
        else
        {
            // player has to turn to the target direction
            double dir_diff = ( dash_angle - wm.self().body() ).abs();

            if ( dir_diff < ptype.effectiveTurn( ServerParam::i().maxMoment(), self_current_speed ) )
            {
                // player can turn to the target direction at the current time
                // check if player can turn first
                if ( tackle_after_wait
                     || ! simulateTurnKickDashes( wm, dash_angle ) )
                {
                    // if player cannot kick the ball after one turn, first kick pattern is tried
                    simulateKickTurnsDashes( wm, 1, dash_angle );
                }
            }
            else
            {
                int n_turn = 0;
                double self_speed = self_current_speed;
                while ( dir_diff > 5.0 )
                {
                    self_speed *= ptype.playerDecay();
                    dir_diff -= ptype.effectiveTurn( ServerParam::i().maxMoment(), self_speed );
                    ++n_turn;
                }

                if ( n_turn == 1 )
                {
                    // player can turn to the direction after one kick
                    simulateKickTurnsDashes( wm, n_turn, dash_angle );
                }
                else if ( ! tackle_after_collide )
                {
                    // player cannot turn to the direction after one kick
                    // player tries to collide with ball in order to decrease the player's speed
                    simulateCollideTurnKickDashes( wm, dash_angle );
                }
                else if ( n_turn == 2 )
                {
                    // try several turns
                    simulateKickTurnsDashes( wm, n_turn, dash_angle );
                }
            }
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
GeneratorKeepDribble::simulateDashes( const WorldModel & wm )
{
#ifdef DEBUG_PRINT_SIMULATE_DASHES
    dlog.addText( Logger::DRIBBLE,
                  "========== (simulateDashes) ==========" );
#endif

    const InterceptType intercept = check_opponent_intercept_next_cycle( wm, wm.ball().pos() + wm.ball().vel() );
    if ( intercept == INTERCEPT )
    {
        return false;
    }

    const ServerParam & SP = ServerParam::i();
    const double pitch_x = SP.pitchHalfLength() - 0.5;
    const double pitch_y = SP.pitchHalfWidth() - 0.5;

    const PlayerType & ptype = wm.self().playerType();
    const double max_side_len = ptype.kickableArea() + ( SP.maxDashPower()
                                                         * ptype.dashPowerRate()
                                                         * ( SP.dashDirRate( 45.0 ) * std::cos( M_PI*0.25 ) )
                                                         * ptype.effortMax() );
    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -wm.self().body() );

    StaminaModel stamina_model = wm.self().staminaModel();

    Vector2D self_pos = wm.self().pos();
    Vector2D self_vel = wm.self().vel();
    Vector2D ball_pos = wm.ball().pos();
    Vector2D ball_vel = wm.ball().vel();

    CooperativeAction::SafetyLevel worst_safety_level = CooperativeAction::Safe;
    double first_dash_power = -1.0;
    double first_dash_dir = -360.0;
    int backside_count = 0;
    const double slow_dash_power_thr = 0.9 * stamina_model.getSafetyDashPower( ptype, SP.maxDashPower() );
    const double backside_x_thr = -ptype.playerSize();

    std::vector< Candidate > candidates;
    candidates.reserve( 10 );

#ifdef DEBUG_PRINT_RESULTS
    boost::shared_ptr< Result > result( new Result() );
#endif

    for ( int n_dash = 1; n_dash <= 10; ++n_dash )
    {
        ++M_total_count;

#ifdef DEBUG_PRINT_SIMULATE_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "%d: (simulateDashes) n_dash=%d", M_total_count, n_dash );
#endif
        //
        // update ball
        //
        ball_pos += ball_vel;
        if ( ball_pos.absX() > pitch_x
             || ball_pos.absY() > pitch_y )
        {
            // out of the pitch
#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "%d: xx n_dash=%d bpos=(%.1f %.1f) out of pitch",
                          M_total_count,
                          n_dash, ball_pos.x, ball_pos.y );
#endif
            break;
        }
        ball_vel *= SP.ballDecay();

        const Vector2D ball_rel_before_dash = rotate_matrix.transform( ball_pos - self_pos );
        if ( ball_rel_before_dash.absY() > max_side_len )
        {
            // never get the ball
#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "%d: xx n_dash=%d y_diff(%.1f) > max_side(%.1f). never get the ball only by dash",
                          M_total_count,
                          n_dash, ball_rel_before_dash.absY(), max_side_len );
#endif
            break;
        }

        //
        // check safety level
        //
        CooperativeAction::SafetyLevel safety_level = getSafetyLevel( wm, ball_pos, n_dash );
#ifdef DEBUG_PRINT_SIMULATE_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "%d: __ n_dash=%d safe=%d", M_total_count, n_dash, safety_level );
#endif
        if ( safety_level == CooperativeAction::Failure )
        {
#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "%d: xx n_dash=%d failure level", M_total_count, n_dash );
#endif
            break;
        }

        //
        // simulate various one step dash
        //
        double dash_power = 0.0;
        double dash_dir = 0.0;

        if ( ! simulateOneDash( wm, n_dash, ball_pos, &self_pos, &self_vel, &stamina_model,
                                &dash_power, &dash_dir,
                                rotate_matrix ) )
        {
#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "%d: xx n_dash=%d one step dash not found", M_total_count, n_dash );
#endif
            break;
        }

        const Vector2D ball_rel_after_dash = rotate_matrix.transform( ball_pos - self_pos );
#ifdef DEBUG_PRINT_SIMULATE_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "%d: __ n_dash=%d ball_rel(%.1f %.1f) power=%.1f dir=%.1f",
                      M_total_count, n_dash,
                      ball_rel_after_dash.x, ball_rel_after_dash.y, dash_power, dash_dir );
#endif

        //
        // prevent back side keep
        //
        if ( ball_rel_after_dash.x < backside_x_thr
             && ( std::fabs( dash_dir ) > 40.0
                  || dash_power < slow_dash_power_thr ) )
        {
            ++backside_count;
            if ( n_dash <=2 || backside_count >= 2 )
            {
#ifdef DEBUG_PRINT_SIMULATE_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: xx n_dash=%d Detect backside keep. ball_rel=(%.2f %.2f) dash_power=%.1f dash_dir=%.1f",
                              M_total_count, n_dash,
                              ball_rel_after_dash.x, ball_rel_after_dash.y, dash_power, dash_dir );
#endif
                break;
            }
        }

        if ( n_dash == 1 )
        {
            first_dash_power = dash_power;
            first_dash_dir = dash_dir;
        }

        if ( worst_safety_level > safety_level )
        {
            worst_safety_level = safety_level;
        }
        if ( intercept == MAY_TACKLE )
        {
            worst_safety_level = CooperativeAction::Dangerous;
        }

        //
        // register candidate action
        //
        Candidate candidate;
        candidate.action_ = ActDribble::create_dash_only( wm.self().unum(),
                                                          ball_pos,
                                                          self_pos,
                                                          wm.self().body().degree(),
                                                          wm.ball().vel(),
                                                          first_dash_power,
                                                          first_dash_dir,
                                                          n_dash,
                                                          "keepDribbleD" );
        candidate.action_->setIndex( M_total_count );
        candidate.action_->setMode( ActDribble::KEEP_DASHES );
        candidate.action_->setSafetyLevel( worst_safety_level );
        candidate.opponent_dist_ = wm.getDistOpponentNearestTo( candidate.action_->targetBallPos(), 5 );
        candidate.self_move_dist_ = wm.self().pos().dist( candidate.action_->targetPlayerPos() );

        candidates.push_back( candidate );
#ifdef DEBUG_PRINT_RESULTS
        result->add( ball_pos, self_pos, dash_power, dash_dir );
        boost::shared_ptr< Result > new_result( new Result( *result ) );
        new_result->action_ = candidate.action_;
        M_results.push_back( new_result );
#endif
    }

    if ( candidates.empty() )
    {
#ifdef DEBUG_PRINT_SIMULATE_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "(simulateDashOnly) xx NO candidate" );
#endif
        return false;
    }

    erase_redundant_candidates( candidates );

    M_dash_only_best = get_best( candidates );
    M_candidates.insert( M_candidates.end(), candidates.begin(), candidates.end() );

    if ( M_dash_only_best
         && M_dash_only_best->safetyLevel() == CooperativeAction::Safe )
    {
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
GeneratorKeepDribble::simulateOneDash( const WorldModel & wm,
                                       const int dash_count,
                                       const Vector2D & ball_pos,
                                       Vector2D * self_pos,
                                       Vector2D * self_vel,
                                       StaminaModel * stamina_model,
                                       double * result_dash_power,
                                       double * result_dash_dir,
                                       const rcsc::Matrix2D & rot_to_body )
{
    static const double dir_step = std::max( 15.0, ServerParam::i().dashAngleStep() );
    static const double min_dash_angle = std::max( -45.0, ServerParam::i().minDashAngle() );
    static const double max_dash_angle = std::min( +45.0, ServerParam::i().maxDashAngle() ) + 0.001;

    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    const double kickable_area_side_thr = ptype.kickableArea() - 0.25;
    const double kickable_area2_front = std::pow( ptype.kickableArea() - 0.15, 2 );
    const double kickable_area2_back = std::pow( ptype.kickableArea() - 0.2, 2 );
    const double collide_dist2 = ( dash_count > 2
                                   ? 0.0
                                   : std::pow( ptype.playerSize() + SP.ballSize() + 0.2 - 0.1*dash_count, 2 ) );

    const double max_power = stamina_model->getSafetyDashPower( ptype, SP.maxDashPower() );

    Vector2D best_self_pos = *self_pos;
    Vector2D best_self_vel;
    double best_dash_dir = -360.0;
    double best_dash_power = -1.0;

    Vector2D best_self_rel = rot_to_body.transform( best_self_pos - wm.self().pos() );

    for ( double dash_dir = min_dash_angle;
          dash_dir < max_dash_angle;
          dash_dir += dir_step )
    {
        const Vector2D unit_vec = Vector2D::polar2vector( 1.0, wm.self().body() + dash_dir );
        const double dir_rate = SP.dashDirRate( dash_dir );

#ifdef DEBUG_PRINT_SIMULATE_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "__ (simulateOneDash) n_dash=%d dash_dir=%.1f",
                      dash_count, dash_dir );
#endif
        for ( double power_decay = 1.0; power_decay > 0.79; power_decay -= 0.1 )
        {
            Vector2D tmp_self_pos = *self_pos;
            Vector2D tmp_self_vel = *self_vel;

            Vector2D dash_accel = unit_vec * ( max_power * power_decay
                                               * dir_rate
                                               * ptype.dashPowerRate()
                                               * stamina_model->effort() );
            tmp_self_vel += dash_accel;
            tmp_self_pos += tmp_self_vel;

            double d2 = tmp_self_pos.dist2( ball_pos );
#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "____ n_dash=%d dir=%.1f power=%.1f self=(%.3f %.3f) ball_dist=%.3f kickable=%.3f",
                          dash_count,
                          dash_dir, max_power * power_decay,
                          tmp_self_pos.x, tmp_self_pos.y, std::sqrt( d2 ),
                          std::sqrt( kickable_area2_front ) );
#endif
            if ( d2 < collide_dist2 )
            {
                // collision
                continue;
            }

            const Vector2D ball_rel = rot_to_body.transform( ball_pos - tmp_self_pos );

            if ( dash_count == 1
                 && ball_rel.absY() > kickable_area_side_thr )
            {
                continue;
            }

            if ( ball_rel.x > 0.0 )
            {
                if ( d2 > kickable_area2_front )
                {
                    continue;
                }
            }
            else
            {
                if ( d2 > kickable_area2_back )
                {
                    continue;
                }
            }

#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "____ nocollide and kickable" );
#endif
            const Vector2D tmp_rel = rot_to_body.transform( tmp_self_pos - wm.self().pos() );
            if ( tmp_rel.x > best_self_rel.x )
            {
                best_self_pos = tmp_self_pos;
                best_self_vel = tmp_self_vel;
                best_dash_power = max_power * power_decay;
                best_dash_dir = dash_dir;
                best_self_rel = tmp_rel;
#ifdef DEBUG_PRINT_SIMULATE_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "__ok updated" );
#endif
            }

            break;
        }
    }

    if ( best_dash_power < 0.0 )
    {
        return false;
    }

    *self_pos = best_self_pos;
    *self_vel = best_self_vel;
    *self_vel *= ptype.playerDecay();

    stamina_model->simulateDash( ptype, best_dash_power );

    *result_dash_power = best_dash_power;
    *result_dash_dir = best_dash_dir;

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
GeneratorKeepDribble::simulateKickDashes( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    const double ball_speed_max2 = std::pow( SP.ballSpeedMax(), 2 );
    const double max_kick_effect2 = std::pow( wm.self().kickRate() * SP.maxPower(), 2 );

    const double keep_y_dist = ( ptype.playerSize()
                                 + SP.ballSize()
                                 + std::min( ptype.kickableMargin() * 0.5, 0.2 ) );
    const double target_x_min = 0.0;
    const double target_x_max = std::sqrt( std::pow( ptype.kickableArea(), 2 )
                                           - std::pow( keep_y_dist, 2 ) );

    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -wm.self().body() );
    const Matrix2D inverse_matrix = Matrix2D::make_rotation( wm.self().body() );

    const Vector2D first_ball_rel_pos = rotate_matrix.transform( wm.ball().pos() - wm.self().pos() );
    const Vector2D first_ball_rel_vel = rotate_matrix.transform( wm.ball().vel() );

    //
    // create self positions relative to the current state
    //   [0] : after kick
    //   [1] : after 1st dash
    //   ... dashes
    //
    std::vector< Vector2D > self_cache;
    self_cache.reserve( 12 );
    createSelfCacheRelative( ptype, 1, 0, 10,
                             rotate_matrix.transform( wm.self().vel() ),
                             wm.self().staminaModel(),
                             self_cache );
    eraseIllegalSelfCache( wm.self().pos(), inverse_matrix, self_cache );

    double target_y[2];
    target_y[0] = self_cache.back().y - keep_y_dist;
    target_y[1] = self_cache.back().y + keep_y_dist;

#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES
    dlog.addText( Logger::DRIBBLE,
                  "========== (simulateKickDashes) ==========" );
    dlog.addText( Logger::DRIBBLE,
                  "== kickable_area=%.3f max_kick_effect=%.3f",
                  ptype.kickableArea(), wm.self().kickRate() * SP.maxPower() );
    dlog.addText( Logger::DRIBBLE,
                  "== keep_y_dist=%.3f target_y[0]=%.3f target_y[1]=%.3f",
                  keep_y_dist, target_y[0], target_y[1] );
    dlog.addText( Logger::DRIBBLE,
                  "== target_min_x=%.2f target_max_x=%.2f",
                  target_x_min, target_x_max );
#endif

    //
    // simulate kick patterns
    //

    std::vector< Candidate > candidates;
    candidates.reserve( 32 );

    for ( int iy = 0; iy < 2; ++iy )
    {
        const double ball_vel_y = SP.firstBallSpeed( target_y[iy] - first_ball_rel_pos.y, self_cache.size() );

#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "----- iy=%d ball_vel_y=%.3f y_move=%.3f-----",
                      iy, ball_vel_y, target_y[iy] - first_ball_rel_pos.y );
#endif

        for ( double target_x = target_x_max; target_x > target_x_min - 0.1; target_x -= 0.1 )
        {
            ++M_total_count;

            const Vector2D ball_vel_by_kick( ( self_cache[1].x + target_x - first_ball_rel_pos.x )
                                             / ( 1.0 + SP.ballDecay() ),
                                             ball_vel_y );

#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES
            {
                Vector2D self_pos = wm.self().pos() + inverse_matrix.transform( self_cache[0] );
                Vector2D ball_next_rel = first_ball_rel_pos + ball_vel_by_kick;
                Vector2D target = wm.self().pos() + inverse_matrix.transform( ball_next_rel );
                Vector2D bvel = inverse_matrix.transform( ball_vel_by_kick );
                dlog.addText( Logger::DRIBBLE,
                              "%d: iy=%d target_x=%.2f first_vel=(%.2f %.2f) speed=%.3f dir=%.1f",
                              M_total_count,
                              iy, target_x, bvel.x, bvel.y, bvel.r(), bvel.th().degree() );
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ ball[0]=(%.2f %.2f) self[0]=(%.2f %.2f)",
                              M_total_count,
                              target.x, target.y, self_pos.x, self_pos.y );
            }
#endif

            if ( ball_vel_by_kick.r2() > ball_speed_max2 )
            {
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ over max speed %.3f",
                              M_total_count, ball_vel_by_kick.r() );
#endif
                continue;
            }

            if ( ( ball_vel_by_kick - first_ball_rel_vel ).r2() > max_kick_effect2 )
            {
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ over max accel. required=%.3f max=%.3f",
                              M_total_count,
                              ( ball_vel_by_kick - first_ball_rel_vel ).r(),
                              std::sqrt( max_kick_effect2 ) );
#endif
                continue;
            }

            const Vector2D next_ball_pos = wm.ball().pos() + inverse_matrix.transform( ball_vel_by_kick );
            const InterceptType intercept = check_opponent_intercept_next_cycle( wm, next_ball_pos );

            if ( intercept == INTERCEPT )
            {
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ iy=%d opponent kickable. ball_next=(%.2f %.2f)",
                              M_total_count, iy, next_ball_pos.x, next_ball_pos.y );

#endif
                continue;
            }

            CooperativeAction::Ptr ptr = simulateKickDashesImpl( wm,
                                                                 self_cache,
                                                                 wm.self().pos(),
                                                                 wm.ball().pos(),
                                                                 first_ball_rel_pos,
                                                                 ball_vel_by_kick,
                                                                 0, // turn step after kick
                                                                 inverse_matrix );
            if ( ptr )
            {
                ptr->setIndex( M_total_count );
                ptr->setMode( ActDribble::KEEP_KICK_DASHES );
                ptr->setDescription( "keepDribbleKD" );
                if ( intercept == MAY_TACKLE )
                {
                    ptr->setSafetyLevel( CooperativeAction::Dangerous );
                }

                Candidate candidate;
                candidate.action_ = ptr;
                candidate.opponent_dist_ = wm.getDistOpponentNearestTo( ptr->targetBallPos(), 5 );
                candidate.self_move_dist_ = wm.self().pos().dist( ptr->targetPlayerPos() );

                candidates.push_back( candidate );

                M_kick_dashes_candidates.push_back( ptr );
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ register n_dash=%d",
                              M_total_count, ptr->dashCount() );
#endif
            }
        }
    }

#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES
    dlog.addText( Logger::DRIBBLE,
                  "(simulateKickDashes) candidate size = %zd", candidates.size() );
#endif
    if ( candidates.empty() )
    {
        return false;
    }

    //
    //
    //
    erase_redundant_candidates( candidates );

    M_kick_dashes_best = get_best( candidates );
    M_candidates.insert( M_candidates.end(), candidates.begin(), candidates.end() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorKeepDribble::simulateKick2Dashes( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();
    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -wm.self().body() );
    const Matrix2D inverse_matrix = Matrix2D::make_rotation( wm.self().body() );

#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES
    dlog.addText( Logger::DRIBBLE,
                  "========== (simulateKick2Dashes) ==========" );
#endif

    std::vector< Vector2D > self_cache;
    self_cache.reserve( 12 );
    {
        StaminaModel stamina_model = wm.self().staminaModel();
        stamina_model.simulateWait( ptype ); // 1 kick

        createSelfCacheRelative( ptype,
                                 1, // 1 kick,
                                 0,
                                 10, // dashes
                                 rotate_matrix.transform( wm.self().vel() * ptype.playerDecay() ),
                                 stamina_model,
                                 self_cache );
        eraseIllegalSelfCache( wm.self().pos(), inverse_matrix, self_cache );
    }

    const Vector2D self_next = wm.self().pos() + wm.self().vel();
    const double keep_y_dist = ( ptype.playerSize()
                                 + SP.ballSize()
                                 + std::min( ptype.kickableMargin() * 0.5, 0.2 ) );
    const double target_x_max = std::sqrt( std::pow( ptype.kickableArea(), 2 )
                                           - std::pow( keep_y_dist, 2 ) );

    double target_y[2];
    target_y[0] = self_cache.back().y - keep_y_dist;
    target_y[1] = self_cache.back().y + keep_y_dist;

    std::vector< Candidate > candidates;
    candidates.reserve( 32 );

    for ( int iy = 0; iy < 2; ++iy )
    {
        const Vector2D first_ball_pos = self_next + inverse_matrix.transform( Vector2D( 0.0, target_y[iy] ) );
        const Vector2D first_ball_vel = first_ball_pos - wm.ball().pos();
        if ( first_ball_vel.r2() > std::pow( SP.ballSpeedMax(), 2 )
             || ( first_ball_vel - wm.ball().vel() ).r2() > std::pow( wm.self().kickRate() * SP.maxPower(), 2 ) )
        {
            // nerver kickable
            continue;
        }

        const InterceptType intercept = check_opponent_intercept_next_cycle( wm, first_ball_pos );

        if ( intercept == INTERCEPT )
        {
            // opponent can control the ball
            continue;
        }

        const Vector2D first_ball_rel_pos = rotate_matrix.transform( first_ball_pos - self_next );
        const Vector2D first_ball_rel_vel = rotate_matrix.transform( first_ball_vel );
        const double kprate = kick_rate( first_ball_rel_pos.r(),
                                         first_ball_rel_pos.th().abs(),
                                         ptype.kickPowerRate(),
                                         ServerParam::i().ballSize(),
                                         ptype.playerSize(),
                                         ptype.kickableMargin() );
        const double max_kick_effect2 = std::pow( kprate * ServerParam::i().maxPower(), 2 );
        const double ball_vel_y = ServerParam::i().firstBallSpeed( target_y[iy] - first_ball_rel_pos.y,
                                                                   self_cache.size() );

        for ( double target_x = target_x_max; target_x > 0.1; target_x -= 0.1 )
        {
            ++M_total_count;

            const Vector2D ball_vel_by_kick( ( self_cache[1].x + target_x - first_ball_rel_pos.x )
                                             / ( 1.0 + ServerParam::i().ballDecay() ),
                                             ball_vel_y );
            if ( ball_vel_by_kick.r2() > std::pow( SP.ballSpeedMax(), 2 ) )
            {
                continue;
            }

            if ( ( ball_vel_by_kick - first_ball_rel_vel ).r2() > max_kick_effect2 )
            {
                continue;
            }

            CooperativeAction::Ptr ptr = simulateKickDashesImpl( wm,
                                                                 self_cache,
                                                                 self_next,
                                                                 first_ball_pos,
                                                                 first_ball_rel_pos,
                                                                 ball_vel_by_kick,
                                                                 0,
                                                                 inverse_matrix );
            if ( ptr )
            {
                ptr->setIndex( M_total_count );
                ptr->setMode( ActDribble::KEEP_KICK_DASHES );
                ptr->setFirstBallVel( first_ball_vel );
                ptr->setKickCount( 2 );
                ptr->setDescription( "keepDribbleK2D" );

                if ( intercept == MAY_TACKLE )
                {
                    ptr->setSafetyLevel( CooperativeAction::Dangerous );
                }

                Candidate candidate;
                candidate.action_ = ptr;
                candidate.opponent_dist_ = wm.getDistOpponentNearestTo( ptr->targetBallPos(), 5 );
                candidate.self_move_dist_ = wm.self().pos().dist( ptr->targetPlayerPos() );

                candidates.push_back( candidate );

                M_kick_dashes_candidates.push_back( ptr );
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ register kick=2 dash=%d safe=%d",
                              M_total_count, ptr->dashCount(), ptr->safetyLevel() );
#endif
            }
        }
    }


#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES
    dlog.addText( Logger::DRIBBLE,
                  "(simulateKick2Dashes) candidate size = %zd",
                  candidates.size() );
#endif
    if ( candidates.empty() )
    {
        return;
    }

    erase_redundant_candidates( candidates );

    M_candidates.insert( M_candidates.end(), candidates.begin(), candidates.end() );
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::Ptr
GeneratorKeepDribble::simulateKickDashesImpl( const WorldModel & wm,
                                              const std::vector< Vector2D > & self_cache,
                                              const Vector2D & first_self_pos,
                                              const Vector2D & first_ball_pos,
                                              const Vector2D & first_ball_rel_pos,
                                              const Vector2D & ball_vel_by_kick,
                                              const size_t turn_step_after_kick,
                                              const Matrix2D & inverse_matrix )
{
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES_IMPL
    dlog.addText( Logger::DRIBBLE,
                  "%d: (impl) self=(%.2f %.2f) ball=(%.2f %.2f) rpos=(%.2f %.2f) vel=(%.2f %.2f)r=%.3f th=%.1f turn=%zd",
                  M_total_count,
                  first_self_pos.x, first_self_pos.y,
                  first_ball_pos.x, first_ball_pos.y,
                  first_ball_rel_pos.x, first_ball_rel_pos.y,
                  ball_vel_by_kick.x, ball_vel_by_kick.y,
                  ball_vel_by_kick.r(), ball_vel_by_kick.th().degree(),
                  turn_step_after_kick );
#else
    (void)first_self_pos;
#endif

    const ServerParam & SP = ServerParam::i();

    const double pitch_x = SP.pitchHalfLength() - 0.5;
    const double pitch_y = SP.pitchHalfWidth() - 0.5;

    const PlayerType & ptype = wm.self().playerType();
    //const double kickable_area2_front = std::pow( ptype.kickableArea() + 0.1, 2 );
    //const double kickable_area2_front = std::pow( ptype.kickableArea(), 2 );
    //const double kickable_area2_front = std::pow( ptype.kickableArea() - 0.1, 2 );
    const double kickable_area2_front = std::pow( ptype.kickableArea() - 0.15, 2 );
    //const double kickable_area2_front = std::pow( ptype.kickableArea() - 0.2, 2 );
    //const double kickable_area2_back = std::pow( ptype.kickableArea() - 0.2, 2 );
    const double kickable_area2_back = std::pow( ptype.kickableArea() + 0.1, 2 );

    CooperativeAction::SafetyLevel best_safety_level = CooperativeAction::Failure;
    size_t best_step = 0;
    Vector2D best_global_pos;

    Vector2D ball_rel_pos = first_ball_rel_pos;
    Vector2D ball_rel_vel = ball_vel_by_kick;

    Vector2D ball_global_pos = first_ball_pos;
    Vector2D ball_global_vel = inverse_matrix.transform( ball_vel_by_kick );

    const size_t nokickable_count_thr = ( turn_step_after_kick == 0
                                          ? 2
                                          : 1 );
    size_t front_nokickable_count = 0;
    bool last_state_nokickable = false;

#ifdef DEBUG_PRINT_RESULTS
    boost::shared_ptr< Result > result( new Result() );
#endif
    for ( size_t step = 0; step < self_cache.size(); ++step )
    {
        ball_rel_pos += ball_rel_vel;
        ball_rel_vel *= SP.ballDecay();
        ball_global_pos += ball_global_vel;
        ball_global_vel *= SP.ballDecay();

#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES_IMPL
        Vector2D self_gpos = first_self_pos + inverse_matrix.transform( self_cache[step] );
        dlog.addText( Logger::DRIBBLE,
                      "%d: ____ step=%zd self=(%.2f %.2f) ball=(%.2f %.2f) dist=%.3f kickable=%.3f",
                      M_total_count,
                      step + 1,
                      self_gpos.x, self_gpos.y,
                      ball_global_pos.x, ball_global_pos.y,
                      self_cache[step].dist( ball_rel_pos ),
                      std::sqrt( kickable_area2_front ) );
#endif

        if ( ball_global_pos.absX() > pitch_x
             || ball_global_pos.absY() > pitch_y )
        {
            // out of the pitch
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES_IMPL
            dlog.addText( Logger::DRIBBLE,
                          "%d: ____ xx out of pitch", M_total_count );
#endif
            g_kick_dashes_impl_reason = REASON_OUT_OF_PITCH;
            break;
        }

        double d2 = self_cache[step].dist2( ball_rel_pos );

        if ( d2 < std::pow( std::max( 0.0, ptype.playerSize() + SP.ballSize() + 0.15 - 0.1*(turn_step_after_kick+step) ), 2 ) )
        {
            // collision
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES_IMPL
            dlog.addText( Logger::DRIBBLE,
                          "%d: ____ xx step=%zd collision dist=%.3f",
                          M_total_count, step + 1, std::sqrt( d2 ) );
#endif
            g_kick_dashes_impl_reason = REASON_COLLISION;
            break;
        }

        bool nokickable = false;
        if ( step > turn_step_after_kick )
        {
            if ( ball_rel_pos.x > self_cache[step].x )
            {
                if ( d2 > kickable_area2_front )
                {
                    nokickable = true;
                    ++front_nokickable_count;
                    if ( front_nokickable_count >= nokickable_count_thr )
                    {
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES_IMPL
                        dlog.addText( Logger::DRIBBLE,
                                      "%d: ____ xx step=%zd no kickable dist=%.3f  count=%d",
                                      M_total_count, step + 1, std::sqrt( d2 ), front_nokickable_count );
#endif
                        g_kick_dashes_impl_reason = REASON_NOKICKABLE_FRONT;
                        break;
                    }
                }
            }
            else // back
            {
                if ( d2 > kickable_area2_back )
                {
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES_IMPL
                    dlog.addText( Logger::DRIBBLE,
                                  "%d: ____ xx step=%zd no kickable dist=%.3f  count=%d",
                                  M_total_count, step + 1, std::sqrt( d2 ), front_nokickable_count );
#endif
                    g_kick_dashes_impl_reason = REASON_NOKICKABLE_BACK;
                    break;
                }
            }
        }

        // check opponent

        CooperativeAction::SafetyLevel level = getSafetyLevel( wm, ball_global_pos,
                                                               step + turn_step_after_kick + 1 );
        if ( level == CooperativeAction::Failure )
        {
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES_IMPL
            dlog.addText( Logger::DRIBBLE,
                          "%d: ____ break: step=%zd detect failure",
                          M_total_count, step + 1 );
#endif
            g_kick_dashes_impl_reason = REASON_OPPONENT;
            break;
        }

        if ( best_safety_level == CooperativeAction::Failure )
        {
            best_safety_level = level;
        }
        else if ( best_safety_level > level )
        {
            if ( best_step >= 1 + turn_step_after_kick ) // at least 1 dash
            {
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES_IMPL
                dlog.addText( Logger::DRIBBLE,
                              "%d: ____ break: step=%zd safety level down safe=%d ",
                              M_total_count, step, level );
#endif
                break;
            }
            best_safety_level = level;
        }

        last_state_nokickable = nokickable;

        if ( ! nokickable )
        {
            best_step = step;
            best_global_pos = ball_global_pos;
        }
#ifdef DEBUG_PRINT_RESULTS
        result->add( ball_global_pos,
                     first_self_pos + inverse_matrix.transform( self_cache[step] ) );
#endif
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES_IMPL
        dlog.addText( Logger::DRIBBLE,
                      "%d: ____ update. step=%zd safe=%d kickable=%s",
                      M_total_count, best_step + 1, best_safety_level, ( nokickable ? "no" : "yes" ) );
#endif
    }

    CooperativeAction::Ptr ptr;

    if ( front_nokickable_count >= nokickable_count_thr
         || last_state_nokickable )
    {
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES_IMPL
        dlog.addText( Logger::DRIBBLE,
                      "%d: xx over no kickable count",
                      M_total_count );
#endif
        return ptr;
    }

    if ( best_step >= 1 + turn_step_after_kick )
    {
#ifdef DEBUG_PRINT_SIMULATE_KICK_DASHES_IMPL
        Vector2D bvel = inverse_matrix.transform( ball_vel_by_kick );
        dlog.addText( Logger::DRIBBLE,
                      "(simulateKickDashesImpl) final_pos=(%.2f %.2f) vel=(%.2f %.2f) step=%d safe=%d",
                      best_global_pos.x, best_global_pos.y,
                      bvel.x, bvel.y,
                      best_step + 1,
                      best_safety_level );
#endif
        ptr = ActDribble::create_normal( wm.self().unum(),
                                         best_global_pos,
                                         ( self_cache[best_step] - first_self_pos ).th().degree(),
                                         inverse_matrix.transform( ball_vel_by_kick ),
                                         1, // kick_count
                                         turn_step_after_kick,
                                         best_step,
                                         "keepDribble" );
        ptr->setTargetPlayerPos( first_self_pos + inverse_matrix.transform( self_cache[best_step] ) );
        ptr->setSafetyLevel( best_safety_level );
#ifdef DEBUG_PRINT_RESULTS
        result->action_ = ptr;
        M_results.push_back( result );
#endif
    }

    return ptr;
}

namespace {
/*-------------------------------------------------------------------*/
/*!

 */
bool
simulate_first_turn( const PlayerType & ptype,
                     Vector2D * self_pos,
                     Vector2D * self_vel,
                     Vector2D * ball_pos,
                     Vector2D * ball_vel )
{
    const ServerParam & SP = ServerParam::i();

    *self_pos += *self_vel;
    *ball_pos += *ball_vel;

    if ( ball_pos->absX() > SP.pitchHalfLength() - 0.5
         || ball_pos->absY() > SP.pitchHalfWidth() - 0.5 )
    {
#ifdef DEBUG_PRINT_SIMULATE_TURN_KICK_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "(simulate_first_turn) out of pitch" );
#endif
        return false;
    }

    const double d2 = self_pos->dist2( *ball_pos );
    if ( d2 > std::pow( ptype.kickableArea()
                        - self_vel->r() * SP.playerRand()
                        - ball_vel->r() * SP.ballRand()
                        - 0.2,
                        2 ) )
    {
        // no kickable without kick
#ifdef DEBUG_PRINT_SIMULATE_TURN_KICK_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "(simulate_first_turn) no kickable" );
#endif
        return false;
    }

    *self_vel *= ptype.playerDecay();
    *ball_vel *= SP.ballDecay();

    // collision
    if ( d2 < std::pow( ptype.playerSize() + SP.ballSize() - 0.1, 2 ) )
    {
#ifdef DEBUG_PRINT_SIMULATE_TURN_KICK_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "(simulate_first_turn) collision" );
#endif
        *self_vel *= -0.1;
        *ball_vel *= -0.1;
        *ball_pos = *self_pos + ( *ball_pos - *self_pos ).setLengthVector( ptype.playerSize() + SP.ballSize() );
    }

    return true;
}
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
GeneratorKeepDribble::simulateTurnKickDashes( const WorldModel & wm,
                                              const AngleDeg & dash_angle )
{
#ifdef DEBUG_PRINT_SIMULATE_TURN_KICK_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "========== (simulateTurnKickDashes) dash_angle=%.1f ==========",
                      dash_angle.degree() );
#endif

    if ( wm.interceptTable()->opponentReachStep() <= 1 )
    {
#ifdef DEBUG_PRINT_SIMULATE_TURN_KICK_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "xx opponent step <= 1" );
#endif
        return false;
    }

    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    Vector2D self_pos_after_turn = wm.self().pos();
    Vector2D self_vel_after_turn = wm.self().vel();

    Vector2D ball_pos_after_turn = wm.ball().pos();
    Vector2D ball_vel_after_turn = wm.ball().vel();

    //
    // one turn, check kickable state and collision state
    //
    if ( ! simulate_first_turn( ptype,
                                &self_pos_after_turn, &self_vel_after_turn,
                                &ball_pos_after_turn, &ball_vel_after_turn ) )
    {
#ifdef DEBUG_PRINT_SIMULATE_TURN_KICK_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "xx cannot perform the first turn." );
#endif
        return false;
    }
#ifdef DEBUG_PRINT_SIMULATE_TURN_KICK_DASHES
    dlog.addText( Logger::DRIBBLE,
                  "(simulateTurnKickDashes) turned: self pos=(%.2f %.2f) vel=(%.2f %.2f)",
                  self_pos_after_turn.x, self_pos_after_turn.y,
                  self_vel_after_turn.x, self_vel_after_turn.y );
    dlog.addText( Logger::DRIBBLE,
                  "(simulateTurnKickDashes) turned: ball pos=(%.2f %.2f) vel=(%.2f %.2f)",
                  ball_pos_after_turn.x, ball_pos_after_turn.y,
                  ball_vel_after_turn.x, ball_vel_after_turn.y );
#endif

    //
    // preparer parameters for kick and dash simulation
    //
    const double kprate = kick_rate( self_pos_after_turn.dist( ball_pos_after_turn ),
                                     ( ( ball_pos_after_turn - self_pos_after_turn ).th() - dash_angle ).abs(),
                                     ptype.kickPowerRate(),
                                     SP.ballSize(),
                                     ptype.playerSize(),
                                     ptype.kickableMargin() )
        * 0.9;
    const double ball_speed_max2 = std::pow( SP.ballSpeedMax(), 2 );
    const double max_kick_effect2 = std::pow( kprate * SP.maxPower(), 2 );

    const double keep_y_dist = ( ptype.playerSize()
                                 + SP.ballSize()
                                 + std::min( ptype.kickableMargin() * 0.5, 0.2 ) );
    const double target_x_min = 0.0;
    const double target_x_max = std::sqrt( std::pow( ptype.kickableArea(), 2 )
                                           - std::pow( keep_y_dist, 2 ) );

    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -dash_angle );
    const Matrix2D inverse_matrix = Matrix2D::make_rotation( dash_angle );

    const Vector2D first_ball_rel_pos = rotate_matrix.transform( ball_pos_after_turn - self_pos_after_turn );
    const Vector2D first_ball_rel_vel = rotate_matrix.transform( ball_vel_after_turn );

    //
    // create self positions relative to the first kick state
    //   [0]: after kick
    //   [1]: after 1st dash
    //   ...
    //
    std::vector< Vector2D > self_cache;
    self_cache.reserve( 12 );
    {
        StaminaModel stamina_model = wm.self().staminaModel();
        stamina_model.simulateWait( ptype ); // 1 turn

        createSelfCacheRelative( ptype,
                                 1, // kick
                                 0, // turn
                                 10, // dash
                                 rotate_matrix.transform( self_vel_after_turn ),
                                 stamina_model,
                                 self_cache );
        eraseIllegalSelfCache( wm.self().pos(), inverse_matrix, self_cache );
    }

    double target_y[2];
    target_y[0] = self_cache.back().y - keep_y_dist;
    target_y[1] = self_cache.back().y + keep_y_dist;

    std::vector< Candidate > candidates;
    candidates.reserve( 32 );

    for ( int iy = 0; iy < 2; ++iy )
    {
        const double ball_vel_y = SP.firstBallSpeed( target_y[iy] - first_ball_rel_pos.y, self_cache.size() );

        for ( double target_x = target_x_max; target_x > target_x_min - 0.1; target_x -= 0.1 )
        {
            ++M_total_count;

            const Vector2D ball_vel_by_kick( ( self_cache[1].x + target_x - first_ball_rel_pos.x )
                                             / ( 1.0 + SP.ballDecay() ),
                                             ball_vel_y );

#ifdef DEBUG_PRINT_SIMULATE_TURN_KICK_DASHES
            Vector2D bvel = inverse_matrix.transform( ball_vel_by_kick );
            dlog.addText( Logger::DRIBBLE,
                          "%d: (simulateTurnKickDashes) target_x=%.3f target_iy=%d ball_vel=(%.2f %.2f)",
                          M_total_count, target_x, iy,
                          bvel.x, bvel.y );
#endif
            if ( ball_vel_by_kick.r2() > ball_speed_max2 )
            {
#ifdef DEBUG_PRINT_SIMULATE_TURN_KICK_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ over max speed %.3f",
                              M_total_count, ball_vel_by_kick.r() );
#endif
                continue;
            }

            if ( ( ball_vel_by_kick - first_ball_rel_vel ).r2() > max_kick_effect2 )
            {
#ifdef DEBUG_PRINT_SIMULATE_TURN_KICK_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ over max accel. required=%.3f max=%.3f",
                              M_total_count,
                              ( ball_vel_by_kick - first_ball_rel_vel ).r(),
                              std::sqrt( max_kick_effect2 ) );

#endif
                continue;
            }

            CooperativeAction::Ptr ptr = simulateKickDashesImpl( wm,
                                                                 self_cache,
                                                                 self_pos_after_turn,
                                                                 ball_pos_after_turn,
                                                                 first_ball_rel_pos,
                                                                 ball_vel_by_kick,
                                                                 0, // turn step after kick
                                                                 inverse_matrix );
            if ( ptr
                 && ptr->dashCount() >= 3 )
            {
                ptr->setIndex( M_total_count );
                ptr->setMode( ActDribble::KEEP_TURN_KICK_DASHES );
                ptr->setTurnCount( 1 );
                ptr->setFirstTurnMoment( ( dash_angle - wm.self().body() ).degree() );
                ptr->setDescription( "keepDribbleTKD" );

                Candidate candidate;
                candidate.action_ = ptr;
                candidate.opponent_dist_ = wm.getDistOpponentNearestTo( ptr->targetBallPos(), 5 );
                candidate.self_move_dist_ = wm.self().pos().dist( ptr->targetPlayerPos() );

                candidates.push_back( candidate );
#ifdef DEBUG_PRINT_SIMULATE_TURN_KICK_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ register turn=1 kick=1 dash=%d",
                              M_total_count, ptr->dashCount() );
#endif
            }
        }

    }

#ifdef DEBUG_PRINT_SIMULATE_TURN_KICK_DASHES
    dlog.addText( Logger::DRIBBLE,
                  "(simulateTurnKickDashes) dash_angle=%.1f candidate size = %zd",
                  dash_angle.degree(), candidates.size() );
#endif
    if ( candidates.empty() )
    {
        return false;
    }

    //
    //
    //
    erase_redundant_candidates( candidates );

    CooperativeAction::Ptr best = get_best( candidates );

    M_candidates.insert( M_candidates.end(), candidates.begin(), candidates.end() );

    if ( best
         && best->safetyLevel() == CooperativeAction::Safe )
    {
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorKeepDribble::createSelfCacheRelative( const PlayerType & ptype,
                                               const int kick_count,
                                               const int turn_count,
                                               const int dash_count,
                                               const Vector2D & first_rel_vel,
                                               const StaminaModel & first_stamina_model,
                                               std::vector< Vector2D > & self_cache )
{
    const ServerParam & SP = ServerParam::i();

    self_cache.clear();

    StaminaModel stamina_model = first_stamina_model;

    Vector2D self_pos( 0.0, 0.0 );
    Vector2D self_vel = first_rel_vel;

    //
    // wait for kicks
    //
    for ( int i = 0; i < kick_count; ++i )
    {
        self_pos += self_vel;
        self_vel *= ptype.playerDecay();
        stamina_model.simulateWait( ptype );

        self_cache.push_back( self_pos );
    }

    //
    // wait for turns
    //
    for ( int i = 0; i < turn_count; ++i )
    {
        self_pos += self_vel;
        self_vel *= ptype.playerDecay();
        stamina_model.simulateWait( ptype );

        self_cache.push_back( self_pos );
    }

    //
    // dash simulation
    //

    for ( int i = 0; i < dash_count; ++i )
    {
        double dash_power = stamina_model.getSafetyDashPower( ptype, SP.maxDashPower(), 100.0 );
        double dash_accel = dash_power * ptype.dashPowerRate() * stamina_model.effort();

        self_vel.x += dash_accel;
        self_pos += self_vel;
        self_vel *= ptype.playerDecay();

        stamina_model.simulateDash( ptype, dash_power );

        self_cache.push_back( self_pos );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorKeepDribble::eraseIllegalSelfCache( const Vector2D & first_self_pos,
                                             const Matrix2D & inverse_matrix,
                                             std::vector< Vector2D > & self_cache )
{
    while ( ! self_cache.empty() )
    {
        Vector2D pos = first_self_pos + inverse_matrix.transform( self_cache.back() );

        if ( pos.absX() > ServerParam::i().pitchHalfLength() - 0.5
             || pos.absY() > ServerParam::i().pitchHalfWidth() - 0.5 )
        {
            self_cache.pop_back();
        }
        else
        {
            break;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorKeepDribble::simulateKickTurnsDashes( const WorldModel & wm,
                                               const int n_turn,
                                               const AngleDeg & dash_angle )
{
    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    const double ball_speed_max2 = std::pow( SP.ballSpeedMax(), 2 );
    const double max_kick_effect2 = std::pow( wm.self().kickRate() * SP.maxPower(), 2 );


    const double keep_y_dist = ( ptype.playerSize()
                                 + SP.ballSize()
                                 + std::min( ptype.kickableMargin() * 0.5, 0.2 ) );
    const double target_x_min = -0.1;
    const double target_x_max = std::sqrt( std::pow( ptype.kickableArea(), 2 )
                                           - std::pow( keep_y_dist, 2 ) );

    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -dash_angle );
    const Matrix2D inverse_matrix = Matrix2D::make_rotation( dash_angle );

    const Vector2D first_ball_rel_pos = rotate_matrix.transform( wm.ball().pos() - wm.self().pos() );
    const Vector2D first_ball_rel_vel = rotate_matrix.transform( wm.ball().vel() );

#ifdef DEBUG_PRINT_SIMULATE_KICK_TURN_DASHES
    dlog.addText( Logger::DRIBBLE,
                  "========== (simulateKickTurnsDashes) n_turn=%d dash_angle=%.1f ==========",
                  n_turn, dash_angle.degree() );
#endif

    //
    // create self position relative to the first state
    //   [0]: after kick
    //   [1]: after turn
    //   [2]: after 1st dash
    //   ... dashes
    //
    std::vector< Vector2D > self_cache;
    self_cache.reserve( 12 );
    createSelfCacheRelative( ptype,
                             1, // kick
                             n_turn, // turn
                             10, // dash
                             rotate_matrix.transform( wm.self().vel() ),
                             wm.self().staminaModel(),
                             self_cache );
    eraseIllegalSelfCache( wm.self().pos(), inverse_matrix, self_cache );

    double target_y[2];
    target_y[0] = self_cache.back().y - keep_y_dist;
    target_y[1] = self_cache.back().y + keep_y_dist;

    std::vector< Candidate > candidates;
    candidates.reserve( 32 );

    //
    // kick, turn and dash simulation
    //

    for ( int iy = 0; iy < 2; ++iy )
    {
        const double ball_vel_y = SP.firstBallSpeed( target_y[iy] - first_ball_rel_pos.y, self_cache.size() );

        for ( double target_x = target_x_max; target_x > target_x_min; target_x -= 0.1 )
        {
            ++M_total_count;

            const double ball_vel_x
                = ( self_cache[2].x + target_x - first_ball_rel_pos.x )
                / ( ( 1.0 - std::pow( SP.ballDecay(), 3 ) )
                    / ( 1.0 - SP.ballDecay() ) );
            const Vector2D ball_vel_by_kick( ball_vel_x, ball_vel_y );

            if ( ball_vel_by_kick.r2() > ball_speed_max2 )
            {
#ifdef DEBUG_PRINT_SIMULATE_KICK_TURN_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ iy=%d over max speed %.3f",
                              M_total_count, iy, ball_vel_by_kick.r() );
#endif
                continue;
            }

            if ( ( ball_vel_by_kick - first_ball_rel_vel ).r2() > max_kick_effect2 )
            {
#ifdef DEBUG_PRINT_SIMULATE_KICK_TURN_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ iy=%d over max accel. required=%.3f max=%.3f",
                              M_total_count, iy,
                              ( ball_vel_by_kick - first_ball_rel_vel ).r(),
                              std::sqrt( max_kick_effect2 ) );

#endif
                continue;
            }

            const Vector2D next_ball_pos = wm.ball().pos() + inverse_matrix.transform( ball_vel_by_kick );
            const InterceptType intercept = check_opponent_intercept_next_cycle( wm, next_ball_pos );

            if ( intercept == INTERCEPT )
            {
#ifdef DEBUG_PRINT_SIMULATE_KICK_TURN_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ iy=%d maybe opponent kickable. ball_next=(%.2f %.2f)",
                              M_total_count, iy, next_ball_pos.x, next_ball_pos.y );

#endif
                continue;
            }

            CooperativeAction::Ptr ptr = simulateKickDashesImpl( wm,
                                                                 self_cache,
                                                                 wm.self().pos(),
                                                                 wm.ball().pos(),
                                                                 first_ball_rel_pos,
                                                                 ball_vel_by_kick,
                                                                 n_turn, // turn step after kick
                                                                 inverse_matrix );
            if ( ptr )
            {
                ptr->setIndex( M_total_count );
                ptr->setMode( ActDribble::KEEP_KICK_TURN_DASHES );
                ptr->setTargetBodyAngle( dash_angle.degree() );
                ptr->setDescription( "keepDribbleKTD" );

                if ( intercept == MAY_TACKLE )
                {
                    ptr->setSafetyLevel( CooperativeAction::Dangerous );
                }

                Candidate candidate;
                candidate.action_ = ptr;
                candidate.opponent_dist_ = wm.getDistOpponentNearestTo( ptr->targetBallPos(), 5 );
                candidate.self_move_dist_ = wm.self().pos().dist( ptr->targetPlayerPos() );

                candidates.push_back( candidate );
#ifdef DEBUG_PRINT_SIMULATE_KICK_TURN_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: ok (KTD) iy=%d n_turn=%d dash_angle=%.0f (k:1 t:1 d:%d) target=(%.2f %.2f)",
                              M_total_count, iy,
                              n_turn, dash_angle.degree(),
                              ptr->dashCount(),
                              ptr->targetBallPos().x, ptr->targetBallPos().y );
#endif
            }
#ifdef DEBUG_PRINT_SIMULATE_KICK_TURN_DASHES
            else
            {
                dlog.addText( Logger::DRIBBLE,
                              "%d: xx (KTD) iy=%d n_turn=%d dash_angle=%.0f target_x=%.2f [%s]",
                              M_total_count, iy, n_turn, dash_angle.degree(), target_x,
                              reason_string( g_kick_dashes_impl_reason ) );
            }
#endif
        }
    }

#ifdef DEBUG_PRINT_SIMULATE_KICK_TURN_DASHES
    dlog.addText( Logger::DRIBBLE,
                  "(simulateKickTurnsDashes) dash_angle=%.1f candidate size = %zd",
                  dash_angle.degree(), candidates.size() );
#endif
    if ( candidates.empty() )
    {
        return;
    }

    //
    //
    //
    erase_redundant_candidates( candidates );
    M_candidates.insert( M_candidates.end(), candidates.begin(), candidates.end() );
}

namespace {
/*-------------------------------------------------------------------*/
/*!

 */
bool
simulate_collide_kick_and_turn( const WorldModel & wm,
                                Vector2D * result_self_pos,
                                Vector2D * result_self_vel,
                                Vector2D * result_ball_pos,
                                Vector2D * result_ball_vel,
                                Vector2D * result_kick_accel )
{
    static GameTime s_last_time( -1, 0 );
    static bool s_last_result = false;
    static Vector2D s_result_self_pos;
    static Vector2D s_result_self_vel;
    static Vector2D s_result_ball_pos;
    static Vector2D s_result_ball_vel;
    static Vector2D s_result_kick_accel;

    if ( s_last_time == wm.time() )
    {
        *result_self_pos = s_result_self_pos;
        *result_self_vel = s_result_self_vel;
        *result_ball_pos = s_result_ball_pos;
        *result_ball_vel = s_result_ball_vel;
        *result_kick_accel = s_result_kick_accel;
        return s_last_result;
    }
    s_last_time = wm.time();
    s_last_result = false;

    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    Vector2D self_pos = wm.self().pos() + wm.self().vel();

    if ( self_pos.absX() > SP.pitchHalfLength() - ptype.playerSize() - 0.1
         || self_pos.absY() > SP.pitchHalfWidth() - - ptype.playerSize() - 0.1 )
    {
        return s_last_result;
    }

    Vector2D self_vel = wm.self().vel() * ptype.playerDecay();
    Vector2D ball_pos = wm.ball().pos();
    Vector2D ball_vel = wm.ball().vel();

    //
    // check if the collision kick can be performed or not.
    //
    Vector2D required_ball_vel = self_pos - wm.ball().pos();
    Vector2D kick_accel = required_ball_vel - wm.ball().vel();

    double kick_accel_len = kick_accel.r();
    double max_kick_effect = wm.self().kickRate() * SP.maxPower();

    if ( kick_accel_len - ptype.playerSize() - SP.ballSize() > max_kick_effect - 0.1 )
    {
#ifdef DEBUG_PRINT_SIMULATE_COLLIDE_TURN_KICK_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "(simulate_collide_kick_and_turn) cannot collide with ball. kick_accel=%.2f kick_effect=%.3f",
                      kick_accel.r(), max_kick_effect );
#endif
        return false;
    }

    if ( kick_accel_len > max_kick_effect )
    {
        kick_accel *= max_kick_effect;
        kick_accel /= kick_accel_len;
    }
    else
    {
        kick_accel *= ( kick_accel_len - 0.1 ) / kick_accel_len;
    }

    ball_vel += kick_accel;

    //
    // apply the collision effect
    //
    self_vel *= -0.1;
    ball_vel *= -0.1;
    ball_pos = self_pos + ( ball_pos - self_pos ).setLengthVector( ptype.playerSize() + SP.ballSize() );

    //
    // apply the turn wait
    //

    self_pos += self_vel;
    self_vel *= ptype.playerDecay();
    ball_pos += ball_vel;
    ball_vel *= SP.ballDecay();

    //
    // check opponent
    //
    for ( PlayerObject::Cont::const_iterator p = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          p != end;
          ++p )
    {
        if ( (*p)->distFromSelf() > 3.0 ) break;
        if ( (*p)->posCount() > 5 ) continue;
        if ( (*p)->isGhost() ) continue;
        if ( (*p)->isTackling() ) continue;

        Vector2D pos = (*p)->pos() + (*p)->vel();
        if ( pos.dist2( ball_pos ) < std::pow( (*p)->playerTypePtr()->kickableArea(), 2 ) )
        {
#ifdef DEBUG_PRINT_SIMULATE_COLLIDE_TURN_KICK_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "(simulate_collide_kick_and_turn) kickable opponent %d(%.2f %.2f) ball=(%.2f %.2f)",
                          (*p)->unum(), pos.x, pos.y,
                          ball_pos.x, ball_pos.y );
#endif
            return false;
        }

    }

    //
    // set results
    //

    s_last_result = true;
    s_result_self_pos = self_pos;
    s_result_self_vel = self_vel;
    s_result_ball_pos = ball_pos;
    s_result_ball_vel = ball_vel;
    s_result_kick_accel = kick_accel;

    *result_self_pos = self_pos;
    *result_self_vel = self_vel;
    *result_ball_pos = ball_pos;
    *result_ball_vel = ball_vel;
    *result_kick_accel = kick_accel;

    return true;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorKeepDribble::simulateCollideTurnKickDashes( const WorldModel & wm,
                                                     const AngleDeg & dash_angle )
{
#ifdef DEBUG_PRINT_SIMULATE_COLLIDE_TURN_KICK_DASHES
    dlog.addText( Logger::DRIBBLE,
                  "========== (simulateCollideTurnKickDashes) dash_angle=%.1f ==========",
                  dash_angle.degree() );
#endif

    Vector2D self_pos_after_turn;
    Vector2D self_vel_after_turn;
    Vector2D ball_pos_after_turn;
    Vector2D ball_vel_after_turn;
    Vector2D collide_kick_accel;

    if ( ! simulate_collide_kick_and_turn( wm,
                                           &self_pos_after_turn,
                                           &self_vel_after_turn,
                                           &ball_pos_after_turn,
                                           &ball_vel_after_turn,
                                           &collide_kick_accel ) )
    {
        return;
    }

#ifdef DEBUG_PRINT_SIMULATE_COLLIDE_TURN_KICK_DASHES
    dlog.addText( Logger::DRIBBLE,
                  "(simulateCollideTurnKickDashes) self pos=(%.2f %.2f) vel=(%.2f %.2f)",
                  self_pos_after_turn.x, self_pos_after_turn.y,
                  self_vel_after_turn.x, self_vel_after_turn.y );
    dlog.addText( Logger::DRIBBLE,
                  "(simulateCollideTurnKickDashes) ball pos=(%.2f %.2f) vel=(%.2f %.2f)",
                  ball_pos_after_turn.x, ball_pos_after_turn.y,
                  ball_vel_after_turn.x, ball_vel_after_turn.y );
#endif

    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    const double kprate = kick_rate( self_pos_after_turn.dist( ball_pos_after_turn ),
                                     ( ( ball_pos_after_turn - self_pos_after_turn ).th() - dash_angle ).abs(),
                                     ptype.kickPowerRate(),
                                     SP.ballSize(),
                                     ptype.playerSize(),
                                     ptype.kickableMargin() )
        * 0.9;

    const double ball_speed_max2 = std::pow( SP.ballSpeedMax(), 2 );
    const double max_kick_effect2 = std::pow( kprate * SP.maxPower(), 2 );

    const double keep_y_dist = ( ptype.playerSize()
                                 + SP.ballSize()
                                 + std::min( ptype.kickableMargin() * 0.5, 0.2 ) );
    const double target_x_min = 0.0;
    const double target_x_max = std::sqrt( std::pow( ptype.kickableArea(), 2 )
                                           - std::pow( keep_y_dist, 2 ) );

    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -dash_angle );
    const Matrix2D inverse_matrix = Matrix2D::make_rotation( dash_angle );

    const Vector2D first_ball_rel_pos = rotate_matrix.transform( ball_pos_after_turn - self_pos_after_turn );
    const Vector2D first_ball_rel_vel = rotate_matrix.transform( ball_vel_after_turn );

    //
    // create self positions relative to the first kick state
    //   [0]: after kick
    //   [1]: after 1st dash
    //   ...
    //
    std::vector< Vector2D > self_cache;
    self_cache.reserve( 12 );
    {
        StaminaModel stamina_model = wm.self().staminaModel();
        stamina_model.simulateWaits( ptype, 2 ); // 1 kick and 1 turn

        createSelfCacheRelative( ptype,
                                 1, // kick
                                 0, // turn
                                 10, // dash
                                 rotate_matrix.transform( self_vel_after_turn ),
                                 stamina_model,
                                 self_cache );
        eraseIllegalSelfCache( wm.self().pos(), inverse_matrix, self_cache );
    }

    double target_y[2];
    target_y[0] = self_cache.back().y - keep_y_dist;
    target_y[1] = self_cache.back().y + keep_y_dist;

    std::vector< Candidate > candidates;
    candidates.reserve( 32 );

#ifdef DEBUG_PRINT_SIMULATE_COLLIDE_TURN_KICK_DASHES
    std::vector< Vector2D > candidate_ball_vel;
    candidate_ball_vel.reserve( 32 );
#endif

    for ( int iy = 0; iy < 2; ++iy )
    {
        const double ball_vel_y = SP.firstBallSpeed( target_y[iy] - first_ball_rel_pos.y, self_cache.size() );

        for ( double target_x = target_x_max; target_x > target_x_min - 0.1; target_x -= 0.1 )
        {
            ++M_total_count;

            const Vector2D ball_vel_by_kick( ( self_cache[1].x + target_x - first_ball_rel_pos.x )
                                             / ( 1.0 + SP.ballDecay() ),
                                             ball_vel_y );

#ifdef DEBUG_PRINT_SIMULATE_COLLIDE_TURN_KICK_DASHES
            Vector2D bvel = inverse_matrix.transform( ball_vel_by_kick );
            dlog.addText( Logger::DRIBBLE,
                          "%d: (simulateCollideTurnKickDashes) target_x=%.3f target_iy=%d ball_vel=(%.2f %.2f)",
                          M_total_count, target_x, iy,
                          bvel.x, bvel.y );
#endif
            if ( ball_vel_by_kick.r2() > ball_speed_max2 )
            {
#ifdef DEBUG_PRINT_SIMULATE_COLLIDE_TURN_KICK_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ over max speed %.3f",
                              M_total_count, ball_vel_by_kick.r() );
#endif
                continue;
            }

            if ( ( ball_vel_by_kick - first_ball_rel_vel ).r2() > max_kick_effect2 )
            {
#ifdef DEBUG_PRINT_SIMULATE_COLLIDE_TURN_KICK_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ over max accel. required=%.3f max=%.3f",
                              M_total_count,
                              ( ball_vel_by_kick - first_ball_rel_vel ).r(),
                              std::sqrt( max_kick_effect2 ) );

#endif
                continue;
            }

            CooperativeAction::Ptr ptr = simulateKickDashesImpl( wm,
                                                                 self_cache,
                                                                 self_pos_after_turn,
                                                                 ball_pos_after_turn,
                                                                 first_ball_rel_pos,
                                                                 ball_vel_by_kick,
                                                                 0, // turn step after kick
                                                                 inverse_matrix );
            if ( ptr
                 && ptr->dashCount() >= 3 )
            {
                ptr->setIndex( M_total_count );
                ptr->setMode( ActDribble::KEEP_COLLIDE_TURN_KICK_DASHES );
                ptr->setKickCount( 2 );
                ptr->setFirstBallVel( wm.ball().vel() + collide_kick_accel );
                ptr->setTurnCount( 1 );
                ptr->setTargetBodyAngle( dash_angle.degree() );
                ptr->setDescription( "keepDribbleCTKD" );

                Candidate candidate;
                candidate.action_ = ptr;
                candidate.opponent_dist_ = wm.getDistOpponentNearestTo( ptr->targetBallPos(), 5 );
                candidate.self_move_dist_ = wm.self().pos().dist( ptr->targetPlayerPos() );

                candidates.push_back( candidate );
#ifdef DEBUG_PRINT_SIMULATE_COLLIDE_TURN_KICK_DASHES
                dlog.addText( Logger::DRIBBLE,
                              "%d: __ register turn=1 kick=1 dash=%d",
                              M_total_count, ptr->dashCount() );
                candidate_ball_vel.push_back( inverse_matrix.transform( ball_vel_by_kick ) );
#endif
            }
        }

    }

#ifdef DEBUG_PRINT_SIMULATE_COLLIDE_TURN_KICK_DASHES
    dlog.addText( Logger::DRIBBLE,
                  "(simulateCollideKickTurnDashes) dash_angle=%.1f candidate size = %zd",
                  dash_angle.degree(), candidates.size() );
#endif
    if ( candidates.empty() )
    {
        return;
    }

    //
    //
    //
    erase_redundant_candidates( candidates );
    M_candidates.insert( M_candidates.end(), candidates.begin(), candidates.end() );
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::SafetyLevel
GeneratorKeepDribble::getSafetyLevel( const WorldModel & wm,
                                      const Vector2D & ball_pos,
                                      const int step )
{
    const ServerParam & SP = ServerParam::i();

    const bool penalty_area = ( ball_pos.x > SP.theirPenaltyAreaLineX()
                                && ball_pos.absY() < SP.penaltyAreaHalfWidth() );
    // const bool over_penalty_line = ( ball_pos.x > SP.theirPenaltyAreaLineX()
    //                                  || ServerParam::i().theirTeamGoalPos().dist2( ball_pos ) < std::pow( 20.0, 2 ) );

    CooperativeAction::SafetyLevel result = CooperativeAction::Safe;

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        const PlayerType * ptype = (*o)->playerTypePtr();
        const double actual_control_area = ( (*o)->goalie() && penalty_area
                                             ? SP.catchableArea() + 0.1
                                             : ptype->kickableArea() + 0.1 );
        const double control_area = ( penalty_area
                                      ? actual_control_area
                                      : std::max( SP.tackleDist() - 0.1, actual_control_area ) );

        Vector2D opponent_pos = (*o)->inertiaPoint( step );


        double ball_dist = opponent_pos.dist( ball_pos );
        double move_dist = ball_dist - control_area;

        if ( ! (*o)->isTackling()
             && move_dist < 0.001 )
        {
            return CooperativeAction::Failure;
        }

        if ( move_dist > ptype->realSpeedMax() * ( step + (*o)->posCount() ) + control_area )
        {
            continue;
        }

        int n_dash = ptype->cyclesToReachDistance( move_dist );
        int n_turn = ( (*o)->bodyCount() > 1
                       ? 0
                       : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                   (*o)->body(),
                                                                   (*o)->vel().r(),
                                                                   ball_dist,
                                                                   ( ball_pos - opponent_pos ).th(),
                                                                   actual_control_area,
                                                                   true ) ); // use back dash
        int opponent_step = ( n_turn == 0
                              ? n_dash
                              : n_turn + n_dash + 1 );

        if ( (*o)->isTackling() )
        {
            opponent_step += std::max( 0, ServerParam::i().tackleCycles() - (*o)->tackleCount() - 2 );
        }

        if ( ball_pos.x < 20.0 )
        {
            opponent_step -= std::min( 8, static_cast< int >( std::floor( (*o)->posCount() * 0.75 ) ) );
        }
        else
        {
            opponent_step -= std::min( 3, static_cast< int >( std::floor( (*o)->posCount() * 0.6 ) ) );
        }

        CooperativeAction::SafetyLevel level = CooperativeAction::Failure;

        if ( opponent_step <= step - 1 )
        {
            level = CooperativeAction::Failure;
        }
        else if ( opponent_step <= step )
        {
            if ( penalty_area
                 && ! (*o)->goalie() )
            {
                level = CooperativeAction::Dangerous;
            }
            else
            {
                level = CooperativeAction::Failure;
            }
        }
        // else if ( opponent_step <= step + 1 ) level = CooperativeAction::Dangerous;
        // else if ( opponent_step <= step + 2 ) level = CooperativeAction::MaybeDangerous;
        // else if ( opponent_step <= step + 3 ) level = CooperativeAction::MaybeDangerous;
        else if ( opponent_step <= step + 3 ) level = CooperativeAction::Dangerous;
        else if ( opponent_step <= step + 5 ) level = CooperativeAction::MaybeDangerous;
        else level = CooperativeAction::Safe;

#ifdef DEBUG_PRINT_SAFETY_LEVEL
        dlog.addText( Logger::DRIBBLE,
                      "%d: (CheckOpponent) unum=%d ball_dist=%.2f move_dist=%.2f step=%d(t=%d,d=%d) bstep=%d safe=%d",
                      M_total_count,
                      (*o)->unum(), ball_dist, move_dist,
                      opponent_step, n_turn, n_dash,
                      step, level );
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
