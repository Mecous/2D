// -*-c++-*-

/*!
  \file action_chain_monte_calro_tree_search.h
  \brief action sequence search algorithm: best first search
*/

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

#ifndef ACTION_CHAIN_MONTE_CALRO_TREE_SEARCH_H
#define ACTION_CHAIN_MONTE_CALRO_TREE_SEARCH_H

#include "action_chain_graph.h"

#include <boost/shared_ptr.hpp>

class ActionChainMonteCalroTreeSearch
    : public ActionChainGraph {
private:

    struct Node {
        size_t index_;
        ActionStatePair data_;
        std::vector< boost::shared_ptr< Node > > children_;
        bool expanded_;
        int count_;
        double original_value_; //!< original evaluation
        double cumulative_value_;
        double ucb_value_; //!< negative value means infinite value.

        Node( const size_t index,
              ActionStatePair data )
            : index_( index ),
              data_( data ),
              expanded_( false ),
              count_( 0 ),
              original_value_( 0.0 ),
              cumulative_value_( 0.0 ),
              ucb_value_( -1.0 )
          { }

        boost::shared_ptr< Node > findBestUCBChild();
        void updateValue( const double value,
                          const int total_count );

        double averageValue() const
          {
              return ( count_ == 0
                       ? original_value_
                       : cumulative_value_ / count_ );
          }
    };

    rcsc::Vector2D M_target_point;
    double M_evaluation_variance;
    double M_gamma;

    int M_expansion_count;

public:

    ActionChainMonteCalroTreeSearch( FieldEvaluator::Ptr evaluator,
                                     const ActionGeneratorHolder::ConstPtr & generator,
                                     const size_t max_depth,
                                     const size_t max_traversal );

    virtual
    void search( const rcsc::WorldModel & wm );


private:

    void doSearch( const rcsc::WorldModel & wm );

    double doPlayoutRecursive( const rcsc::WorldModel & wm,
                               std::vector< ActionStatePair > & sequence,
                               boost::shared_ptr< Node > node );

    void evaluateNode( boost::shared_ptr< Node > node,
                       const std::vector< ActionStatePair > & sequence );

    void buildBestSequence( boost::shared_ptr< Node > node,
                            Sequence & sequence );

};

#endif
