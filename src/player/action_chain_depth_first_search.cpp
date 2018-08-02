// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA, Hidehisa AKIYAMA

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

#include "action_chain_depth_first_search.h"

#include "cooperative_action.h"

#include <rcsc/common/logger.h>
#include <rcsc/time/timer.h>

// #define DEBUG_PROFILE
// #define DEBUG_PAINT_EVALUATED_POINTS

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
ActionChainDepthFirstSearch::ActionChainDepthFirstSearch( FieldEvaluator::Ptr evaluator,
                                                          const ActionGeneratorHolder::ConstPtr & generator,
                                                          const size_t max_depth,
                                                          const size_t max_traversal )
    : ActionChainGraph( evaluator, generator, max_depth, max_traversal )
{
#ifdef DEBUG_PAINT_EVALUATED_POINTS
    g_evaluated_points.clear();
    g_evaluated_points.reserve( max_evaluate_size + 1 );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainDepthFirstSearch::search( const WorldModel & wm )
{
    clearResult();

    M_first_state = PredictState::ConstPtr( new PredictState( wm ) );

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    // do search
    {
        std::vector< ActionStatePair > empty_path;
        doRecursiveSearch( wm, *M_first_state, empty_path, &M_best_sequence );
    }

    if ( M_best_sequence.chain_.empty() )
    {
        if ( wm.self().isKickable() )
        {
            createDefaultHold( wm );
        }
    }

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::ACTION_CHAIN,
                  __FILE__": PROFILE size=%d elapsed %f [ms]",
                  M_node_count, timer.elapsedReal() );
#endif

#ifdef DEBUG_PAINT_EVALUATED_POINTS
    debug_paint_evaluated_points();
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
ActionChainDepthFirstSearch::doRecursiveSearch( const WorldModel & wm,
                                                const PredictState & state,
                                                const std::vector< ActionStatePair > & path,
                                                Sequence * result )
{
    if ( path.size() > M_max_depth )
    {
        return false;
    }

    //
    // check evaluation limit
    //
    if ( M_node_count >= M_max_traversal )
    {
        dlog.addText( Logger::ACTION_CHAIN,
                      "cut by max evaluate size %zd", M_node_count );
        return false;
    }

    //
    // check current state
    //
    Sequence best_result;
    best_result.chain_ = path;
    best_result.value_ = M_evaluator->evaluate( *M_first_state, path );

    ++M_node_count;

    if ( dlog.isEnabled( Logger::PLAN ) )
    {
        debugPrintSequence( M_node_count, best_result );
    }

#ifdef DEBUG_PAINT_EVALUATED_POINTS
    g_evaluated_points.push_back( EvalPoint( M_node_count, state.ball().pos(), best_result.value_ ) );
#endif

    //
    // generate candidate actions
    //
    std::vector< ActionStatePair > candidate_actions;
    if ( path.empty()
         || ! path.back().action().isFinalAction() )
    {
        M_action_generator->generate( &candidate_actions, state, wm, path );
    }


    //
    // test each candidate
    //
    for ( std::vector< ActionStatePair >::const_iterator it = candidate_actions.begin(),
              end = candidate_actions.end();
          it != end;
          ++it )
    {
        std::vector< ActionStatePair > new_path = path;
        Sequence candidate_result;

        new_path.push_back( *it );

        if ( doRecursiveSearch( wm, (*it).state(), new_path, &candidate_result ) )
        {
            if ( dlog.isEnabled( Logger::PLAN ) )
            {
                debugPrintSequence( M_node_count, candidate_result );
            }

#ifdef DEBUG_PAINT_EVALUATED_POINTS
            g_evaluated_points.push_back( EvalPoint( -1, it->state().ball().pos(), ev ) );
#endif
            if ( candidate_result.value_ > best_result.value_ )
            {
                best_result = candidate_result;
                M_best_sequence_index = M_node_count;
            }
        }
    }

    *result = best_result;

    return true;
}
