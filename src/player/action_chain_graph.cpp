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

#include "action_chain_graph.h"
#include "predict_state.h"
#include "act_hold_ball.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/time/timer.h>

#include <string>
#include <sstream>
#include <queue>
#include <utility>
#include <limits>
#include <cstdio>
#include <cmath>

using namespace rcsc;


// #define DEBUG_PRINT_CURRENT_STATE
// #define DEBUG_PAINT_EVALUATED_POINTS

// #define DEBUG_PROFILE

/*-------------------------------------------------------------------*/
/*!

 */
ActionChainGraph::ActionChainGraph( FieldEvaluator::Ptr evaluator,
                                    const ActionGeneratorHolder::ConstPtr & generator,
                                    const size_t max_depth,
                                    const size_t max_traversal )
    : M_evaluator( evaluator ),
      M_action_generator( generator ),
      M_max_depth( max_depth ),
      M_max_traversal( max_traversal ),
      M_node_count( 0 ),
      M_best_sequence_index( 0 )
{
    M_best_sequence.value_ = -std::numeric_limits< double >::max();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainGraph::setFieldEvaluator( FieldEvaluator::Ptr eval )
{
    M_evaluator = eval;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainGraph::clearResult()
{
    M_node_count = 0;
    M_best_sequence.chain_.clear();
    M_best_sequence.value_ = -std::numeric_limits< double >::max();
    M_best_sequence_index = 0;

    ActionStatePair::reset_index_count();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainGraph::createDefaultHold( const WorldModel & wm )
{
    if ( ! M_first_state )
    {
        M_first_state = PredictState::ConstPtr( new PredictState( wm ) );
    }

    PredictState::ConstPtr result_state( new PredictState( *M_first_state, 1 ) );

    int holder_unum = M_first_state->ballHolder().unum();

    CooperativeAction::Ptr action( new ActHoldBall( holder_unum,
                                                    M_first_state->ball().pos(),
                                                    1,
                                                    "defaultHold" ) );
    action->setSafetyLevel( CooperativeAction::Dangerous );

    ++M_node_count;
    M_best_sequence.chain_.push_back( ActionStatePair( action, result_state ) );

    double value = M_evaluator->evaluate( *M_first_state, M_best_sequence.chain_ );
    M_best_sequence.value_ = value;
    M_best_sequence.chain_.back().setValue( value );
    M_best_sequence_index = -1;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainGraph::debugSendBestSequence( PlayerAgent * agent ) const
{
    if ( ! M_first_state )
    {
        return;
    }

    const PredictState & current_state = *M_first_state;

    const std::vector< ActionStatePair > & path = bestSequence();

    for ( size_t i = 0; i < path.size(); ++i )
    {
        std::string action_string = "?";
        std::string target_string = "";

        const CooperativeAction & action = path[i].action();
        const PredictState * s0;
        const PredictState * s1;

        if ( i == 0 )
        {
            s0 = &current_state;
            s1 = &(path[0].state());
#if 0
            for ( PredictPlayerObject::Cont::const_iterator p = s1->ourPlayers().begin(), end = s1->ourPlayers().end();
                  p != end;
                  ++p )
            {
                agent->debugClient().addCircle( (*p)->pos(), 0.5 );
            }
#endif
        }
        else
        {
            s0 = &(path[i - 1].state());
            s1 = &(path[i].state());
        }


        switch( action.type() ) {
        case CooperativeAction::Hold:
            {
                action_string = "hold";
                break;
            }

        case CooperativeAction::Dribble:
            {
                action_string = "dribble";

                agent->debugClient().addLine( s0->ball().pos(), s1->ball().pos() );
                break;
            }


        case CooperativeAction::Pass:
            {
                action_string = "pass";

                std::ostringstream buf;
                buf << action.targetPlayerUnum();
                target_string = buf.str();

                agent->debugClient().addLine( s0->ball().pos(), s1->ball().pos() );
                break;
            }

        case CooperativeAction::Shoot:
            {
                action_string = "shoot";

                agent->debugClient().addLine( s0->ball().pos(),
                                              Vector2D( ServerParam::i().pitchHalfLength(),
                                                        0.0 ) );

                break;
            }

        case CooperativeAction::Move:
            {
                action_string = "move";
                break;
            }
        default:
            {
                action_string = "?";

                break;
            }
        }

        if ( action.description() )
        {
            action_string += '_';
            action_string += action.description();
            if ( ! target_string.empty() )
            {
                action_string += ':';
                action_string += target_string;
            }
        }

        agent->debugClient().addMessage( action_string );

        // dlog.addText( Logger::ACTION_CHAIN,
        //               "chain %d %s, time = %lu",
        //               i, action_string.c_str(), s1->spentTime() );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainGraph::debugPrintSequence( const size_t count,
                                      const Sequence & sequence )
{
    dlog.addText( Logger::PLAN,
                  "%zd: evaluation=%lf",
                  count, sequence.value_ );

    const PredictState & current_state = *M_first_state;

    const std::vector< ActionStatePair > & path = sequence.chain_;
    const size_t size = path.size();

    for ( size_t i = 0; i < size; ++i )
    {
        const CooperativeAction & a = path[i].action();
        const PredictState * s0;
        const PredictState * s1;

        if ( i == 0 )
        {
            s0 = &current_state;
            s1 = &(path[0].state());
        }
        else
        {
            s0 = &(path[i - 1].state());
            s1 = &(path[i].state());
        }


        switch ( a.type() ) {
        case CooperativeAction::Hold:
            {
                dlog.addText( Logger::PLAN,
                              "__ %d: %d hold (%s) t=%d from[%d](%.2f %.2f), safe=%d value=%lf",
                              i, path[i].index(),
                              a.description(), s1->spentTime(),
                              s0->ballHolder().unum(),
                              a.targetBallPos().x, a.targetBallPos().y,
                              a.safetyLevel(),
                              path[i].value() );
                break;
            }

        case CooperativeAction::Dribble:
            {
                dlog.addText( Logger::PLAN,
                              "__ %d: %d dribble (%s[%d]) t=%d from[%d](%.2f %.2f)-to(%.2f %.2f), safe=%d value=%lf",
                              i, path[i].index(),
                              a.description(), a.index(), s1->spentTime(),
                              s0->ballHolder().unum(),
                              s0->ball().pos().x, s0->ball().pos().y,
                              a.targetBallPos().x, a.targetBallPos().y,
                              a.safetyLevel(),
                              path[i].value() );
                break;
            }

        case CooperativeAction::Pass:
            {
                dlog.addText( Logger::PLAN,
                              "__ %d: %d pass (%s[%d]) k=%d t=%d from[%d](%.2f %.2f)-to[%d](%.2f %.2f), safe=%d value=%lf",
                              i, path[i].index(),
                              a.description(), a.index(), a.kickCount(), s1->spentTime(),
                              s0->ballHolder().unum(),
                              s0->ball().pos().x, s0->ball().pos().y,
                              s1->ballHolder().unum(),
                              a.targetBallPos().x, a.targetBallPos().y,
                              a.safetyLevel(),
                              path[i].value() );
                break;
            }

        case CooperativeAction::Shoot:
            {
                dlog.addText( Logger::PLAN,
                              "__ %d: %d shoot (%s) t=%d from[%d](%.2f %.2f)-to(%.2f %.2f), safe=%d value=%lf",
                              i, path[i].index(),
                              a.description(), s1->spentTime(),
                              s0->ballHolder().unum(),
                              s0->ball().pos().x, s0->ball().pos().y,
                              a.targetBallPos().x, a.targetBallPos().y,
                              a.safetyLevel(),
                              path[i].value() );

                break;
            }

        case CooperativeAction::Move:
            {
                const AbstractPlayerObject * pl0 = s0->ourPlayer( a.playerUnum() );
                const AbstractPlayerObject * pl1 = s1->ourPlayer( a.targetPlayerUnum() );
                if ( ! pl0 || ! pl1 )
                {
                    std::cerr << __FILE__ << ":" << __LINE__
                              << ": internal error, "
                              << "nonexistent player's move action ditected"
                              << std::endl;
                    break;
                }

                dlog.addText( Logger::PLAN,
                              "__ %d: %d move (%s) t=%d from[%d](%.2f %.2f)-to(%.2f %.2f), safe=%d value=%lf",
                              i, path[i].index(),
                              a.description(), s1->spentTime(),
                              a.playerUnum(),
                              pl0->pos().x, pl0->pos().y,
                              a.targetPlayerPos().x, a.targetPlayerPos().y,
                              a.safetyLevel(),
                              path[i].value() );
                break;
            }

        default:
            {
                const AbstractPlayerObject * pl = s0->ourPlayer( a.playerUnum() );
                if ( ! pl )
                {
                    std::cerr << __FILE__ << ":" << __LINE__
                              << ": internal error, "
                              << "unexistent player's unknown action ditected"
                              << std::endl;
                    break;
                }

                dlog.addText( Logger::PLAN,
                              "__ %d: %d ???? (%s) t=%d from[%d](%.2f %.2f)-to[%d](%.2f %.2f), safe=%d value=%lf",
                              i, path[i].index(),
                              a.description(), s1->spentTime(),
                              a.playerUnum(),
                              pl->pos().x, pl->pos().y,
                              a.targetPlayerUnum(),
                              a.targetBallPos().x, a.targetBallPos().y,
                              a.safetyLevel(),
                              path[i].value() );

                break;
            }
        }
    }
}
