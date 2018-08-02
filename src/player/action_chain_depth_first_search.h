// -*-c++-*-

/*!
  \file action_chain_depth_first_search.h
  \brief action sequence search algorithm: depth first search
*/

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

#ifndef ACTION_CHAIN_DEPTH_FIRST_SEARCH_H
#define ACTION_CHAIN_DEPTH_FIRST_SEARCH_H

#include "action_chain_graph.h"

class ActionChainDepthFirstSearch
    : public ActionChainGraph {
private:

public:

    ActionChainDepthFirstSearch( FieldEvaluator::Ptr evaluator,
                                 const ActionGeneratorHolder::ConstPtr & generator,
                                 const size_t max_depth,
                                 const size_t max_traversal );

    virtual
    void search( const rcsc::WorldModel & wm );


private:

    bool doRecursiveSearch( const rcsc::WorldModel & wm,
                            const PredictState & state,
                            const std::vector< ActionStatePair > & path,
                            Sequence * result );

};

#endif
