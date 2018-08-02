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

#include "action_chain_best_first_search.h"

#include "cooperative_action.h"

#include "statistics.h"

#include <rcsc/common/logger.h>
#include <rcsc/time/timer.h>

#include <queue>
#include <limits>

// #define DEBUG_PROFILE
// #define DEBUG_PRINT

// #define USE_LAST_DECISION

using namespace rcsc;

inline
bool
operator<( const ActionChainGraph::Sequence & lhs,
           const ActionChainGraph::Sequence & rhs )
{
    return lhs.hvalue_ < rhs.hvalue_;
}                            //???


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/


/*-------------------------------------------------------------------*/
/*!

 */
ActionChainBestFirstSearch::ActionChainBestFirstSearch( FieldEvaluator::Ptr evaluator,
                                                        const ActionGeneratorHolder::ConstPtr & generator,
                                                        const size_t max_depth,
                                                        const size_t max_traversal )
    : ActionChainGraph( evaluator, generator, max_depth, max_traversal ),
      M_last_search_time( -1, 0 ),
      M_last_target_player_unum( Unum_Unknown ),
      M_last_target_ball_pos( Vector2D::INVALIDATED )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainBestFirstSearch::search( const WorldModel & wm )
{
    clearResult();

    M_first_state = PredictState::ConstPtr( new PredictState( wm ) );

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    doSearch( wm );

    if ( M_best_sequence.chain_.empty() )
    {
        createDefaultHold( wm );
    }
    else
    {
        M_last_search_time = wm.time();
        M_last_target_player_unum = M_best_sequence.chain_.front().action().targetPlayerUnum();
        M_last_target_ball_pos =  M_best_sequence.chain_.front().action().targetBallPos();
    }

#ifdef DEBUG_PROFILE
    const double elapsed = timer.elapsedReal();
    dlog.addText( Logger::ACTION_CHAIN,
                  "(BestFirstSearch) PROFILE size=%d elapsed %f [ms]",
                  M_node_count, elapsed );
    Statistics::instance().setActionSearchData( M_node_count, elapsed );
#else
    Statistics::instance().setActionSearchData( M_node_count, 0.0 );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainBestFirstSearch::doSearch( const WorldModel & wm )
{
    std::priority_queue< Sequence > q;

    //q.push( Sequence( std::vector< ActionStatePair >(), 0.0, 0.0 ) );
    q.push( Sequence() );

    while ( 1 )
    {
        if ( q.empty() )
        {
            break;
        }

        Sequence best_seq = q.top();
        q.pop();

        if ( best_seq.chain_.size() >= M_max_depth
             || ( ! best_seq.chain_.empty()
                  && best_seq.chain_.back().action().isFinalAction() ) )
        {
            // over the max chain length
            // or, the last action is a final action type.
            continue;
        }

        // generate possible action-state pair at the tail of action chain
        std::vector< ActionStatePair > candidate_actions;
        {
            const PredictState * state = ( best_seq.chain_.empty()
                                           ? &(*M_first_state)
                                           : &(best_seq.chain_.back().state()) );
            M_action_generator->generate( &candidate_actions, *state, wm, best_seq.chain_ );

            //
            // if shoot action is generated, erase all other actions
            //
            if ( ! candidate_actions.empty()
                 && candidate_actions.front().action().type() == CooperativeAction::Shoot )
            {
                candidate_actions.erase( candidate_actions.begin() + 1, candidate_actions.end() );
            }
        }

#ifdef DEBUG_PRINT
        if ( best_seq.chain_.empty() )
        {
            dlog.addText( Logger::ACTION_CHAIN,
                          "(BestFirstSearch) >>>> generate (0 empty[-1]) candidate_size=%zd <<<<<",
                          candidate_actions.size() );
        }
        else
        {
            dlog.addText( Logger::ACTION_CHAIN,
                          "(BestFirstSearch) >>>> generate (%d %s[%d]) candidate_size=%d <<<<<",
                          best_seq.chain_.back().index(),
                          best_seq.chain_.back().action().description(),
                          best_seq.chain_.back().action().index(),
                          candidate_actions.size() );
        }
#endif

        for ( std::vector< ActionStatePair >::const_iterator it = candidate_actions.begin(),
                  end = candidate_actions.end();
              it != end;
              ++it )
        {
            ++M_node_count;

            Sequence candidate_seq = best_seq;
            candidate_seq.chain_.push_back( *it );

            if ( candidate_seq.chain_.size() == 1 )
            {
                double penalty = M_evaluator->getFirstActionPenalty( *M_first_state, *it );
                candidate_seq.chain_.back().setPenalty( penalty );
            }

            double value = M_evaluator->evaluate( *M_first_state, candidate_seq.chain_ );
#ifdef USE_LAST_DECISION
            if ( M_last_search_time.cycle() == wm.time().cycle() - 1
                 && M_last_target_player_unum != Unum_Unknown )
            {
                if ( candidate_seq.chain_.front().action().targetPlayerUnum() != M_last_target_player_unum )
                {
                    if ( value > 0.0 ) value *= 0.9;
                    else value /= 0.9;
                }
            }
#endif
            candidate_seq.value_ = value;
            candidate_seq.hvalue_ = value;
            candidate_seq.chain_.back().setValue( value );


            if ( dlog.isEnabled( Logger::PLAN ) )
            {
                debugPrintSequence( M_node_count, candidate_seq );
            }

            if ( candidate_seq.value_ > M_best_sequence.value_ )
            {
                dlog.addText( Logger::ACTION_CHAIN,
                              "(BestFirstSearch) <<<< updated best to index %d", M_node_count );
                M_best_sequence_index = M_node_count;
                M_best_sequence = candidate_seq;
            }

            if ( M_node_count >= M_max_traversal )
            {
                dlog.addText( Logger::ACTION_CHAIN,
                              "(BestFirstSearch) ***** over max node size *****" );
                return;
            }

            q.push( candidate_seq );
        }
    }

}
