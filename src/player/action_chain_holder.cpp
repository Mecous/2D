// -*-c++-*-

/*!
  \file action_chain_holder.cpp
  \brief holder of calculated chain action Source File
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA, Hidehisa Akiyama

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
#include "config.h"
#endif

#include "action_chain_holder.h"

#include "action_chain_best_first_search.h"
#include "action_chain_depth_first_search.h"
#include "action_chain_monte_calro_tree_search.h"

#include "options.h"

#include <rcsc/player/world_model.h>
#include <rcsc/param/cmd_line_parser.h>
#include <rcsc/param/param_map.h>


using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
ActionChainHolder::ActionChainHolder()
    : M_graph()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
ActionChainHolder &
ActionChainHolder::instance()
{
    static ActionChainHolder s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainHolder::init( FieldEvaluator::Ptr evaluator,
                         const ActionGeneratorHolder::ConstPtr & generator )
{
    // std::cout << "ActionSequence\n"
    //           << "  search method = " << Options::i().chainSearchMethod() << '\n'
    //           << "  evaluator = " << Options::i().evaluatorName() << '\n'
    //           << "  max_chain_length = " << Options::i().maxChainLength() << '\n'
    //           << "  max_evaluate_size = " << Options::i().maxEvaluateSize() << '\n'
    //           << std::flush;

#if 1
    M_graph = ActionChainGraph::Ptr( new ActionChainBestFirstSearch( evaluator,
                                                                     generator,
                                                                     Options::i().maxChainLength(),
                                                                     Options::i().maxEvaluateSize() ) );
#else
    if ( Options::i().chainSearchMethod() == "BestFirstSearch" )
    {
        M_graph = ActionChainGraph::Ptr( new ActionChainBestFirstSearch( evaluator,
                                                                         generator,
                                                                         Options::i().maxChainLength(),
                                                                         Options::i().maxEvaluateSize() ) );
    }
    else if ( Options::i().chainSearchMethod() == "DepthFirstSearch" )
    {
        M_graph = ActionChainGraph::Ptr( new ActionChainDepthFirstSearch( evaluator,
                                                                          generator,
                                                                          Options::i().maxChainLength(),
                                                                          Options::i().maxEvaluateSize() ) );
    }
    else if ( Options::i().chainSearchMethod() == "MonteCarloTreeSearch" )
    {
        M_graph = ActionChainGraph::Ptr( new ActionChainMonteCalroTreeSearch( evaluator,
                                                                              generator,
                                                                              Options::i().maxChainLength(),
                                                                              Options::i().maxEvaluateSize() ) );
    }
    else
    {
        std::cerr << "Unknown search method ["
                  << Options::i().chainSearchMethod() << ']'
                  << " use the BestFirstSearch instead." << std::endl;
        M_graph = ActionChainGraph::Ptr( new ActionChainBestFirstSearch( evaluator,
                                                                         generator,
                                                                         Options::i().maxChainLength(),
                                                                         Options::i().maxEvaluateSize() ) );
    }
#endif
}


/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainHolder::setFieldEvaluator( FieldEvaluator::Ptr eval )
{
    M_graph->setFieldEvaluator( eval );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActionChainHolder::update( const WorldModel & wm )
{
    static GameTime s_update_time( -1, 0 );

    if ( s_update_time == wm.time() )
    {
        return;
    }
    s_update_time = wm.time();

    M_graph->search( wm );
}
