// -*-c++-*-

/*!
  \file player_graph.h
  \brief player position graph class Header File
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

#ifndef PLAYER_GRAPH_H
#define PLAYER_GRAPH_H

#include <rcsc/geom/delaunay_triangulation.h>
#include <rcsc/player/abstract_player_object.h>

#include <boost/shared_ptr.hpp>

namespace rcsc {
class PlayerPredicate;
class WorldModel;
}

/*!
  \class PlayerGraph
  \brief player position graph using Delaunay triangulation
 */
class PlayerGraph {
public:
    struct Node {
    public:
        const rcsc::AbstractPlayerObject * player_;
        std::vector< const Node * > connection_;

        Node( const rcsc::AbstractPlayerObject * p )
            : player_( p )
          {
              connection_.reserve( 6 );
          }

        const rcsc::Vector2D & pos() const
          {
              return player_->pos();
          }

        rcsc::Vector2D randomPos() const;

        void addConnection( const Node * node )
          {
              connection_.push_back( node );
          }
    };

    struct Connection {
        const Node * first_;
        const Node * second_;

        Connection( const Node * first,
                    const Node * second )
            : first_( first )
            , second_( second )
          { }
    };

private:

    boost::shared_ptr< const rcsc::PlayerPredicate > M_predicate;

    std::vector< Node > M_nodes;
    std::vector< Connection > M_connections;
    rcsc::DelaunayTriangulation M_triangulation;

public:

    PlayerGraph();
    explicit
    PlayerGraph( const rcsc::PlayerPredicate * predicate );

    virtual
    ~PlayerGraph()
      { }

    void setPredicate( const rcsc::PlayerPredicate * predicate );


    const std::vector< Node > & nodes() const
      {
          return M_nodes;
      }

    const std::vector< Connection > & connections() const
      {
          return M_connections;
      }

    void update( const rcsc::WorldModel & wm );

private:

    void updateNode( const rcsc::WorldModel & wm );

};

#endif
