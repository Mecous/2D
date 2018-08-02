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

#include "actgen_simple_dribble.h"

#include "act_dribble.h"
#include "field_analyzer.h"

#include "action_state_pair.h"
#include "predict_state.h"

#include <rcsc/player/world_model.h>
#include <rcsc/common/logger.h>
#include <rcsc/timer.h>

#include <limits>

// #define DEBUG_PRINT

using namespace rcsc;


namespace {
struct PointSorter {
    const Vector2D pos_;
    PointSorter( const Vector2D & pos )
        : pos_( pos )
      { }
    bool operator()( const Vector2D & lhs,
                     const Vector2D & rhs ) const
      {
          return pos_.dist2( lhs ) < pos_.dist2( rhs );
      }
};

struct Candidate {
    Vector2D pos_;
    int holder_reach_step_;
    Candidate( const Vector2D & pos,
               const int holder_reach_step )
        : pos_( pos ),
          holder_reach_step_( holder_reach_step )
      { }
};

struct CandidateSorter {
    const Vector2D pos_;
    CandidateSorter( const Vector2D & pos )
        : pos_( pos )
      { }
    bool operator()( const Candidate & lhs,
                     const Candidate & rhs ) const
      {
          return pos_.dist2( lhs.pos_ ) < pos_.dist2( rhs.pos_ );
      }
};
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActGen_SimpleDribble::generate( std::vector< ActionStatePair > * result,
                                const PredictState & state,
                                const WorldModel & current_wm,
                                const std::vector< ActionStatePair > & path ) const
{
    generateImplAngles( result, state, current_wm, path );
    //generateImplVoronoiTargets( result, state, current_wm, path );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActGen_SimpleDribble::generateImplAngles( std::vector< ActionStatePair > * result,
                                          const PredictState & state,
                                          const WorldModel & current_wm,
                                          const std::vector< ActionStatePair > & path ) const
{
    static GameTime s_last_call_time( 0, 0 );
    static int s_action_count = 0;

    if ( current_wm.time() != s_last_call_time )
    {
        s_action_count = 0;
        s_last_call_time = current_wm.time();
    }

    if ( path.empty() )
    {
        return;
    }

    if ( path.back().action().type() == CooperativeAction::Hold )
    {
        return;
    }

    const AbstractPlayerObject & holder = state.ballHolder();

    const int ANGLE_DIVS = 16;
    const double ANGLE_STEP = 360.0 / ANGLE_DIVS;
    const int DIST_FIRST = ( path.size() >= 2 ? 2 : 0 );
    enum {
        DIST_DIVS = 3
    };
    //const double DIST_STEP = 2.0; //1.75
    const double DIST_TABLE[DIST_DIVS] = { 2.0, 4.0, 6.0 };

    const ServerParam & SP = ServerParam::i();

    const double max_x = SP.pitchHalfLength() - 1.0;
    const double max_y = SP.pitchHalfWidth() - 1.0;

    const int bonus_step = 2;
    //const int bonus_step = std::max( 10, static_cast< int >( path.back().state().spentTime() ) / 2 );
    //const int bonus_step = std::max( 10, static_cast< int >( path.back().state().spentTime() ) / 3 );

    const PlayerType * ptype = holder.playerTypePtr();

    std::vector< Vector2D > points;
    points.reserve( ANGLE_DIVS * DIST_DIVS );

    for ( int a = 0; a < ANGLE_DIVS; ++a )
    {
        const AngleDeg target_angle = ANGLE_STEP * a;

        if ( holder.pos().x < 16.0
             && target_angle.abs() > 100.0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          __FILE__" angle=%.0f danger angle(1)",
                          target_angle.degree() );
#endif
            continue;
        }

        if ( holder.pos().x < -36.0
             && holder.pos().absY() < 20.0
             && target_angle.abs() > 45.0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          __FILE__": angle=%.0f danger angle(2)",
                          target_angle.degree() );
#endif
            continue;
        }

        const Vector2D unit_vec = Vector2D::from_polar( 1.0, target_angle );
        for ( int d = DIST_FIRST; d < DIST_DIVS; ++d )
        {
            const double holder_move_dist = DIST_TABLE[d];
            const Vector2D target_point
                = holder.pos()
                + unit_vec.setLengthVector( holder_move_dist );

            if ( target_point.absX() > max_x
                 || target_point.absY() > max_y )
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::ACTION_CHAIN,
                              __FILE__": angle=%.0f dist=%.2f (%.2f %.2f) out of pitch.",
                              target_angle.degree(), holder_move_dist,
                              target_point.x, target_point.y );
#endif
                continue;
            }

            points.push_back( target_point );
        }
    }

    //
    // sort by distance from ...
    //
    std::sort( points.begin(), points.end(), PointSorter( Vector2D( 44.0, 0.0 ) ) );

    //
    //
    //
    int count = 0;
    for ( size_t i = 0; i < points.size(); ++i )
    {
        const Vector2D target_point = points[i];
        const double holder_move_dist = holder.pos().dist( target_point );
        const int holder_reach_step
            = 1 + 1  // kick + turn
            + ptype->cyclesToReachDistance( holder_move_dist - ptype->kickableArea() * 0.5 );

        //
        // check opponent
        //
        bool exist_opponent = false;
        for ( PredictPlayerObject::Cont::const_iterator o = state.theirPlayers().begin();
              o != state.theirPlayers().end();
              ++o )
        {
            // if ( (*o)->goalie() ) continue;

            double opp_move_dist = (*o)->pos().dist( target_point );
            int o_step
                = 1 + 1 // observation + turn step
                + (*o)->playerTypePtr()->cyclesToReachDistance( opp_move_dist - ptype->kickableArea() );

            if ( (*o)->goalie() )
            {
                if ( o_step < holder_reach_step )
                {
                    exist_opponent = true;
                    break;
                }
            }
            else
            {
                if ( o_step - bonus_step <= holder_reach_step )
                {
                    exist_opponent = true;
                    break;
                }
            }
        }

        if ( exist_opponent )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          __FILE__": angle=%.0f dist=%.2f (%.2f %.2f) exist opponent.",
                          target_angle.degree(), holder_move_dist,
                          target_point.x, target_point.y );
#endif
            continue;
        }


        PredictState::ConstPtr result_state( new PredictState( state,
                                                               holder_reach_step,
                                                               holder.unum(),
                                                               target_point ) );
        CooperativeAction::Ptr action = ActDribble::create_normal( holder.unum(),
                                                                   target_point,
                                                                   ( target_point - holder.pos() ).th().degree(),
                                                                   Vector2D(), // dummy first speed
                                                                   1,
                                                                   1,
                                                                   holder_reach_step,
                                                                   "Simple" );

        ++s_action_count;
        action->setIndex( s_action_count );
        action->setSafetyLevel( CooperativeAction::MaybeDangerous );
        result->push_back( ActionStatePair( action, result_state ) );

        if ( ++count >= 2 )
        {
            break;
        }
    }

#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  __FILE__": Dribble path=%d, holder=%d generated=%d/%d",
                  path.size(), holder.unum(), count, s_action_count );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActGen_SimpleDribble::generateImplVoronoiTargets( std::vector< ActionStatePair > * result,
                                                  const PredictState & state,
                                                  const WorldModel & current_wm,
                                                  const std::vector< ActionStatePair > & path ) const
{
    static GameTime s_last_call_time( 0, 0 );
    static int s_action_count = 0;

    if ( current_wm.time() != s_last_call_time )
    {
        s_action_count = 0;
        s_last_call_time = current_wm.time();
    }

    if ( path.empty() )
    {
        return;
    }

    if ( path.back().action().type() == CooperativeAction::Hold )
    {
        return;
    }

    const PlayerType * ptype = state.ballHolder().playerTypePtr();
    const double min_dist_thr2 = std::pow( 3.0, 2 );
    const double max_dist_thr2 = std::pow( 15.0, 2 );

    int count = 0;
    for ( std::vector< Vector2D >::const_iterator it = FieldAnalyzer::i().voronoiTargetPoints().begin(),
              end = FieldAnalyzer::i().voronoiTargetPoints().end();
          it != end;
          ++it )
    {
        const double d2 = state.ball().pos().dist2( *it );
        if ( d2 < min_dist_thr2 || max_dist_thr2 < d2 )
        {
            continue;
        }

        const double holder_move_dist = state.ballHolder().pos().dist( *it );
        const int holder_reach_step
            = 1 + 1  // kick + turn
            + ptype->cyclesToReachDistance( holder_move_dist - ptype->kickableArea() * 0.5 );

        //
        // check opponent
        //
        bool exist_opponent = false;
        for ( PredictPlayerObject::Cont::const_iterator o = state.theirPlayers().begin();
              o != state.theirPlayers().end();
              ++o )
        {
            double opp_move_dist = (*o)->pos().dist( *it );
            int o_step
                = 1 + 1 // observation and turn step
                + (*o)->playerTypePtr()->cyclesToReachDistance( opp_move_dist - ptype->kickableArea() );

            if ( o_step > 6 ) continue;
            if ( o_step < holder_reach_step )
            {
                exist_opponent = true;
                break;
            }
        }

        if ( exist_opponent )
        {
            continue;
        }

        PredictState::ConstPtr result_state( new PredictState( state,
                                                               holder_reach_step,
                                                               state.ballHolder().unum(),
                                                               *it ) );
        CooperativeAction::Ptr action = ActDribble::create_normal( state.ballHolder().unum(),
                                                                   *it,
                                                                   ( *it - state.ballHolder().pos() ).th().degree(),
                                                                   Vector2D(), // dummy first speed
                                                                   1,
                                                                   1,
                                                                   holder_reach_step - 2,
                                                                   "Simple" );
        action->setIndex( s_action_count );
        action->setSafetyLevel( CooperativeAction::MaybeDangerous );
        result->push_back( ActionStatePair( action, result_state ) );

        ++s_action_count;
        ++count;
        if ( count > 10 )
        {
            break;
        }
    }


#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  __FILE__": Dribble path=%d, holder=%d generated=%d/%d",
                  path.size(),
                  holder.unum(),
                  count, s_action_count );
#endif
}
