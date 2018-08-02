// -*-c++-*-

/*!
  \file action_chain_best_first_search.h
  \brief action sequence search algorithm: best first search
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

#ifndef ACTION_CHAIN_BEST_FIRST_SEARCH_H
#define ACTION_CHAIN_BEST_FIRST_SEARCH_H

#include "action_chain_graph.h"

class ActionChainBestFirstSearch
    : public ActionChainGraph {
private:

    rcsc::GameTime M_last_search_time;
    int M_last_target_player_unum;
    rcsc::Vector2D M_last_target_ball_pos;

public:

    ActionChainBestFirstSearch( FieldEvaluator::Ptr evaluator,
                                const ActionGeneratorHolder::ConstPtr & generator,
                                const size_t max_depth,
                                const size_t max_traversal );

    virtual
    void search( const rcsc::WorldModel & wm );


private:

    void doSearch( const rcsc::WorldModel & wm );

};

#endif
