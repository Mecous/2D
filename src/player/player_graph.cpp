// -*-c++-*-

/*!
  \file player_graph.cpp
  \brief player position graph Source File
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "player_graph.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/player_predicate.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>

#include <cstdlib>

using namespace rcsc;


/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
PlayerGraph::Node::randomPos() const
{
    Vector2D p = this->pos();
    p.x += -0.001 + 0.002 * ( static_cast< double >( std::rand() ) / static_cast< double >( RAND_MAX ) );
    p.y += -0.001 + 0.002 * ( static_cast< double >( std::rand() ) / static_cast< double >( RAND_MAX ) );

    return p;
}

/*-------------------------------------------------------------------*/
/*!

 */
PlayerGraph::PlayerGraph()
{
    M_nodes.reserve( 24 );
}

/*-------------------------------------------------------------------*/
/*!

 */
PlayerGraph::PlayerGraph( const PlayerPredicate * predicate )
    : M_predicate( predicate )
{
    M_nodes.reserve( 22 );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PlayerGraph::setPredicate( const PlayerPredicate * predicate )
{
    M_predicate = boost::shared_ptr< const PlayerPredicate >( predicate );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PlayerGraph::updateNode( const WorldModel & wm )
{
    if ( ! M_predicate )
    {
        return;
    }

    AbstractPlayerObject::Cont players;
    wm.getPlayers( players, M_predicate );

    for ( AbstractPlayerObject::Cont::const_iterator p = players.begin(),
              end = players.end();
          p != end;
          ++p )
    {
        M_nodes.push_back( Node( *p ) );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
PlayerGraph::update( const WorldModel & wm )
{
    static GameTime s_update_time( 0, 0 );

    if ( s_update_time == wm.time() )
    {
        return;
    }
    s_update_time = wm.time();

    M_nodes.clear();
    M_connections.clear();

    updateNode( wm );

    if ( M_nodes.size() < 3 )
    {
        if ( M_nodes.size() == 2 )
        {
            Node * first = &(M_nodes.front());
            Node * second = &(M_nodes.back());

            first->addConnection( second );
            second->addConnection( first );

            M_connections.push_back( Connection( first, second ) );
        }
        return;
    }

    std::map< int, Node * > node_map; //!< key: triangulation vertex id

    //
    // create triangulation
    //
    M_triangulation.clear();
    for ( std::vector< Node >::iterator n = M_nodes.begin(),
              end = M_nodes.end();
          n != end;
          ++n )
    {
        int id = M_triangulation.addVertex( n->randomPos() );
        if ( id >= 0 )
        {
            node_map.insert( std::pair< int, Node * >( id, &(*n) ) );
        }
    }

    M_triangulation.compute();

    //
    // create connections
    //
    for ( DelaunayTriangulation::EdgeCont::const_iterator e = M_triangulation.edges().begin(),
              end = M_triangulation.edges().end();
          e != end;
          ++e )
    {
        const DelaunayTriangulation::Vertex * v0 = e->second->vertex( 0 );
        const DelaunayTriangulation::Vertex * v1 = e->second->vertex( 1 );

        if ( ! v0 || ! v1 )
        {
            continue;
        }

        std::map< int, Node * >::iterator n0 = node_map.find( v0->id() );
        std::map< int, Node * >::iterator n1 = node_map.find( v1->id() );

        if ( n0 == node_map.end()
             || n1 == node_map.end() )
        {
            continue;
        }

        Node * first = n0->second;
        Node * second = n1->second;

        first->addConnection( second );
        second->addConnection( first );

        M_connections.push_back( Connection( first, second ) );
    }
}
