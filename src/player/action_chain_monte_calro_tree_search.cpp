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

#include "action_chain_monte_calro_tree_search.h"

#include "cooperative_action.h"
#include "statistics.h"

#include "act_hold_ball.h"

#include <rcsc/common/logger.h>
#include <rcsc/time/timer.h>

#include <cmath>

// #define DEBUG_PRINT
// #define DEBUG_PROFILE

using namespace rcsc;


/*-------------------------------------------------------------------*/
/*!

 */
ActionChainMonteCalroTreeSearch::ActionChainMonteCalroTreeSearch( FieldEvaluator::Ptr evaluator,
                                                                  const ActionGeneratorHolder::ConstPtr & generator,
                                                                  const size_t max_depth,
                                                                  const size_t max_traversal )
    : ActionChainGraph( evaluator, generator, max_depth, max_traversal )
{
    M_target_point = ServerParam::i().theirTeamGoalPos();
    M_evaluation_variance = std::pow( 10.0, 2 );
    M_gamma = 1.0;

    M_expansion_count = 2;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainMonteCalroTreeSearch::search( const WorldModel & wm )
{

    clearResult();
    M_node_count = 0;

    M_first_state = PredictState::ConstPtr( new PredictState( wm ) );

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    doSearch( wm );

    if ( M_best_sequence.chain_.empty() )
    {
        createDefaultHold( wm );
    }

#ifdef DEBUG_PROFILE
    const double elapsed = timer.elapsedReal();
    dlog.addText( Logger::ACTION_CHAIN,
                  __FILE__": PROFILE size=%d elapsed %f [ms]",
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
ActionChainMonteCalroTreeSearch::doSearch( const WorldModel & wm )
{
    std::vector< ActionStatePair > sequence;

    CooperativeAction::Ptr action( new ActHoldBall( M_first_state->ballHolder().unum(),
                                                    M_first_state->ball().pos(),
                                                    1,
                                                    "dummyHold" ) );
    ActionStatePair root_state( action, M_first_state );
    boost::shared_ptr< Node > root_node( new Node( 0, root_state ) );
    root_node->count_ = 0;

    for ( size_t i = 0; i < M_max_traversal; ++i )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      ">>>>>>>>>>>>>>>>>>>> playout %d <<<<<<<<<<<<<<<<<<<<",
                      i + 1 );
#endif
        doPlayoutRecursive( wm, sequence, root_node );
    }

#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  __FILE__" (doSearch) build best sequence." );
#endif
    buildBestSequence( root_node, M_best_sequence );
}

/*-------------------------------------------------------------------*/
/*!

 */
double
ActionChainMonteCalroTreeSearch::doPlayoutRecursive( const WorldModel & wm,
                                                     std::vector< ActionStatePair > & sequence,
                                                     boost::shared_ptr< Node > node )
{
    node->count_ += 1;

    if ( ! node->expanded_
         && ( node->count_ >= M_expansion_count
              || node->index_ == 0 ) // root node
         )
    {
        if ( sequence.size() <= M_max_depth
             && node->children_.empty()
             && ! node->data_.action().isFinalAction() )
        {
            std::vector< ActionStatePair > candidates;
            M_action_generator->generate( &candidates,
                                          node->data_.state(),
                                          wm,
                                          sequence );
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          "(doPlayoutRecursive) generate children. parent=[%s(%d)] depth=%zd node=%zd children_size=%zd",
                          node->data_.action().description(), node->data_.action().index(),
                          sequence.size() + 1, node->index_, candidates.size() );
#endif
            for ( std::vector< ActionStatePair >::iterator it = candidates.begin(), end = candidates.end();
                  it != end;
                  ++it )
            {
                boost::shared_ptr< Node > child_node( new Node( M_node_count, *it ) );
                ++M_node_count;

                sequence.push_back( *it );
                evaluateNode( child_node, sequence );
                sequence.pop_back();

                node->children_.push_back( child_node );
#ifdef DEBUG_PRINT
                dlog.addText( Logger::ACTION_CHAIN,
                              "___%zd [%s(%d)] value=%lf ucb=%lf",
                              child_node->index_,
                              it->action().description(), it->action().index(),
                              child_node->original_value_, child_node->ucb_value_ );
#endif
                if ( it->action().type() == CooperativeAction::Shoot )
                {
                    break;
                }
            }
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          "(doPlayoutRecursive) generate children. done." );
#endif
        }
        node->expanded_ = true;
    }

    if ( node->children_.empty() )
    {
        return node->original_value_;
    }

    boost::shared_ptr< Node > best_ucb_child = node->findBestUCBChild();

    if ( ! best_ucb_child )
    {
        //std::cerr << __FILE__ << " (doSearch) unexpected null pointer." << std::endl;
        return node->original_value_;
    }

    sequence.push_back( best_ucb_child->data_ );

    double best_child_value = doPlayoutRecursive( wm, sequence, best_ucb_child );
    // double value = node->original_value_ + M_gamma * doSearch( wm, sequence, best_child );
    best_ucb_child->updateValue( best_child_value, node->count_ );

    double result_value = 0.0;

    double sum_children_value = 0.0;
    if ( ! node->children_.empty() )
    {
        for ( std::vector< boost::shared_ptr< Node > >::iterator it = node->children_.begin(), end = node->children_.end();
              it != end;
              ++it )
        {
            sum_children_value += (*it)->averageValue();
        }

        result_value = sum_children_value / node->children_.size();
    }
#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  "(doPlayoutRecursive) best_child_value=%f sum_children=%f result_value=%f.",
                  best_child_value, sum_children_value, result_value );
#endif

    sequence.pop_back();

    return result_value;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainMonteCalroTreeSearch::evaluateNode( boost::shared_ptr< Node > node,
                                               const std::vector< ActionStatePair > & sequence )
{
    if ( ! node )
    {
        return;
    }

#if 1
    node->original_value_ = M_evaluator->evaluate( *M_first_state, sequence );
    node->ucb_value_ = -1.0;

#elif 0
    (void)sequence;

    const PredictState & state = node->data_.state();

    node->ucb_value_ = -1.0;

    if ( state.ball().pos().x > ServerParam::i().pitchHalfLength() - 0.1
         && state.ball().pos().absY() < ServerParam::i().goalHalfWidth() + 2.0 )
    {
        node->original_value_ = 1.0;
        return;
    }

    node->original_value_ = std::exp( - state.ball().pos().dist2( M_target_point ) / ( 2.0 * M_evaluation_variance ) );
    if ( node->data_.action().safetyLevel() == CooperativeAction::Dangerous )
    {
        node->original_value_ *= 0.5;
    }
    else if ( node->data_.action().safetyLevel() == CooperativeAction::MaybeDangerous )
    {
        node->original_value_ *= 0.75;
    }
#else
    (void)sequence;

    const PredictState & state = node->data_.state();

    const double attack_third = ServerParam::i().pitchHalfLength() - ServerParam::i().pitchLength()/3.0;

    node->original_value_ = 0.0;
    node->ucb_value_ = -1.0;

    if ( state.ball().pos().x > ServerParam::i().pitchHalfLength() - 0.1
         && state.ball().pos().absY() < ServerParam::i().goalHalfWidth() + 2.0 )
    {
        node->original_value_ = 1.0;
    }
    else  if ( M_first_state->ball().pos().x < attack_third )
    {
        if ( state.ball().pos().x > attack_third )
        {
            node->original_value_ = 1.0;
        }
    }
    else // in attack third
    {
        if ( state.ball().pos().x > ServerParam::i().theirPenaltyAreaLineX()
             && state.ball().pos().absY() < ServerParam::i().goalHalfWidth() )
        {
            node->original_value_ = 1.0;
        }
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
boost::shared_ptr< ActionChainMonteCalroTreeSearch::Node >
ActionChainMonteCalroTreeSearch::Node::findBestUCBChild()
{
    if ( children_.empty() )
    {
        return boost::shared_ptr< Node >();
    }

    boost::shared_ptr< Node > best_node;
    double best_ucb = -100000.0;

    for ( std::vector< boost::shared_ptr< Node > >::iterator it = children_.begin(), end = children_.end();
          it != end;
          ++it )
    {
        if ( (*it)->ucb_value_ < 0.0 ) // infinite value
        {
            return *it;
        }

        if ( best_ucb < (*it)->ucb_value_ )
        {
            best_ucb = (*it)->ucb_value_;
            best_node = *it;
        }
    }

    return best_node;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainMonteCalroTreeSearch::Node::updateValue( const double value,
                                                    const int parent_count )
{
    cumulative_value_ += value;
    ucb_value_
        = cumulative_value_ / count_
        + std::sqrt( 2.0 * std::log( static_cast< double >( parent_count ) ) / count_ );

#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  "(updateValue) %zd [%s(%d)] value=%lf cumulative=%lf average=%f parent_count=%d this_count=%d ucb=%lf",
                  index_,
                  data_.action().description(), data_.action().index(),
                  value, cumulative_value_, averageValue(), parent_count, count_, ucb_value_ );
#endif
}


/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainMonteCalroTreeSearch::buildBestSequence( boost::shared_ptr< Node > node,
                                                    Sequence & sequence )
{
    boost::shared_ptr< Node > best_child;
    double best_value = -100000.0;

    for ( std::vector< boost::shared_ptr< Node > >::iterator it = node->children_.begin(), end = node->children_.end();
          it != end;
          ++it )
    {
        double value = (*it)->averageValue();
        if ( best_value < value )
        {
            best_child = *it;
            best_value = value;
        }
    }

    if ( best_child )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "___append action %zd average_value=%f ucb_value=%f [%s]",
                      best_child->index_,
                      best_child->averageValue(),
                      best_child->ucb_value_,
                      best_child->data_.action().description() );
#endif
        sequence.chain_.push_back( best_child->data_ );
        sequence.value_ = best_value;
        buildBestSequence( best_child, sequence );
    }
}
