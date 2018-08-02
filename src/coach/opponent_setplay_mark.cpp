// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Tomoharu NAKASHIMA

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

#include "opponent_setplay_mark.h"
#include "coach_strategy.h"

#include <algorithm>
#include <vector>
#include <map>
#include <string>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/coach/coach_agent.h>
#include <rcsc/coach/coach_world_model.h>
#include <rcsc/timer.h>

using namespace rcsc;

// MAGIC numbers
#define OPPONENT_DEFENCE_LIMIT 25.0
#define MARKING_MARGIN2 3.0*3.0
#define UPPER_VELOCITY_FOR_STAYING 0.5
#define INF (1 << 30)
#define MAX_V 3000

/*-------------------------------------------------------------------*/
/*!

 */
OpponentSetplayMark::OpponentSetplayMark()
    : M_time_o( 0 ),
      M_time_s( 0 ),
      M_max_cycle( 0 )
{
    M_opp_type_map["WrightEagle"] = Type_Gliders;
    M_opp_type_map["FCP_GPR_2014"] = Type_Normal;
    M_opp_type_map["Gliders2014"] = Type_Gliders;
    M_opp_type_map["Gliders2015"] = Type_Gliders;
    M_opp_type_map["Oxsy"] = Type_Oxsy;
    M_opp_type_map["Yushan2014"] = Type_Normal;
    M_opp_type_map["Yushan2015"] = Type_Normal;
    M_opp_type_map["Others"] = Type_Normal;
    M_current_opp_type = Type_Normal;



    for ( int i = 0; i < 11; ++i )
    {
        M_candidate_unum_marking_setplay[i] = Unum_Unknown;
        M_pre_candidate_unum_setplay[i] = Unum_Unknown;
    }
}



/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentSetplayMark::analyze( CoachAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  "%s:%d: OpponentSetplayMark::analyze",
                  __FILE__, __LINE__ );



    const CoachWorldModel & wm = agent->world();

    // send the opponent mark unum on setplay information in a freeform format
    if ( wm.gameMode().type() == GameMode::PlayOn )
    {
        M_max_cycle = -1;
    }

    std::map< std::string, OpponentType >::const_iterator it = M_opp_type_map.find( wm.theirTeamName() );

    if ( it == M_opp_type_map.end() )
        M_current_opp_type = Type_Normal;
    else
        M_current_opp_type = it->second;


    if ( agent->config().useFreeform()
         && wm.canSendFreeform()
         && wm.gameMode().isTheirSetPlay( wm.ourSide() )
         && ( ( M_time_o != 0 && M_time_o != wm.time().cycle() )
              || ( M_time_s != 0 && M_time_s != wm.time().stopped() ) )
         //&& ! wm.existKickablePlayer()
         )
    {
        std::vector<int> our_unum;
        std::vector<int> our_setplay_marker;
        std::vector<int> their_unum;
        std::vector<int> their_defender_unum;

        int opp_min = estimateOppMin( wm );

        if( wm.getSetPlayCount() < 1
            || M_max_cycle < opp_min )
        {
            M_max_cycle = opp_min;
        }

        /*
        if( M_current_opp_type == Type_WrightEagle )
        {
            if( !createCandidates( wm,
                                   opp_min,
                                   our_unum,
                                   our_setplay_marker,
                                   their_unum,
                                   their_defender_unum ) )
            {
                return findOpponentToMark( agent );
            }
        }
        else
        {*/
        if( !createCandidates( wm,
                               opp_min,
                               our_unum,
                               their_unum ) )
        {
            return findOpponentToMark( agent );
        }
        // }

        bool isAssigned =  assignedMarker( agent,
                                           opp_min,
                                           our_unum,
                                           their_unum );

        if( isAssigned )
        {

            if( wm.getSetPlayCount() > 1 )
                compareCandidate( wm,
                                  opp_min );

            for( int i = 1; i <= 11; ++i )
            {
                int mate_unum = i;
                int opp_unum = M_candidate_unum_marking_setplay[mate_unum - 1];
                if( opp_unum < 1 || opp_unum > 11 )
                    opp_unum = Unum_Unknown;
                CoachAnalyzerManager::instance().setMarkAssignment( mate_unum,
                                                                    opp_unum,
                                                                    Unum_Unknown);
                agent->debugClient().addMessage( "target: %d %d",
                                                 mate_unum,opp_unum );
                if( opp_unum >= 1 && opp_unum <= 11 )
                    agent->debugClient().addLine( wm.opponent( opp_unum )->pos(), wm.teammate( mate_unum )->pos(),"#999" );
                M_pre_candidate_unum_setplay[mate_unum - 1] = opp_unum;
                M_candidate_unum_marking_setplay[mate_unum - 1] = Unum_Unknown;
            }
        }
        else
        {
            findOpponentToMark( agent );
        }
    }

    if ( wm.gameMode().isServerCycleStoppedMode()
         && wm.gameMode().isTheirSetPlay( wm.ourSide() ) )
    {
        M_time_s = wm.time().stopped();
    }

    M_time_o = wm.time().cycle();

    dlog.addText( Logger::ANALYZER,
                  __FILE__":(finishAssigned)" );


    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
void
OpponentSetplayMark::add_edge( int from,
                               int to,
                               int cap,
                               int cost,
                               std::vector< std::vector<OpponentSetplayMark::edge> > & G )
{
    // G[from].push_back( (OpponentSetplayMark::edge){ to, cap, cost, G[to].size() } );
    // G[to].push_back( (OpponentSetplayMark::edge){ from, 0, -cost, G[from].size() - 1 } );
    G[from].push_back( edge( to, cap, cost, G[to].size() ) );
    G[to].push_back( edge( from, 0, -cost, G[from].size() - 1 ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
int
OpponentSetplayMark::min_cost_flow( int s,
                                    int t,
                                    int f,
                                    std::vector< std::vector<OpponentSetplayMark::edge> > & G )
{
    int res = 0;
    int V = 24;
    int dist[MAX_V];
    int prevv[MAX_V], preve[MAX_V];

    while( f > 0 )
    {
        std::fill( dist, dist + V, INF );
        dist[s] = 0;
        bool update = true;
        while( update )
        {
            update = false;
            for( int v = 0; v < V; v++ ){
                if( dist[v] == INF ) continue;
                for( int i = 0; i < (int)G[v].size(); i++ )
                {
                    edge &e = G[v][i];
                    if( e.cap > 0 && dist[e.to] > dist[v] + e.cost )
                    {
                        dist[e.to] = dist[v] + e.cost;
                        prevv[e.to] = v;
                        preve[e.to] = i;
                        update = true;
                    }
                }
            }
        }

        if( dist[t] == INF )
        {
            return -1;
        }

        int d = f;
        int pre = 0;
        for( int v = t; v != s; v = prevv[v] )
        {
            d = std::min( d, G[prevv[v]][preve[v]].cap );
            if( v > 0 && v < 12 )
                M_candidate_unum_marking_setplay[v-1] = pre - 11;
            else if( v >= 12 )
                pre = v;
        }
        f -= d;
        res += d * dist[t];
        for( int v = t; v != s; v = prevv[v] )
        {
            edge &e = G[prevv[v]][preve[v]];
            e.cap -= d;
            G[v][e.rev_index].cap += d;
        }
    }
    return res;
}


/*-------------------------------------------------------------------*/
/*!

 */

bool
OpponentSetplayMark::assignedMarker( CoachAgent * agent,
                                     int opp_min,
                                     std::vector<int> & our_unum,
                                     std::vector<int> & their_unum )
{
    const CoachWorldModel & wm = agent->world();

    int max_cycle = M_max_cycle;
    double rate;
    MSecTimer timer;

    std::vector< std::vector< edge > > G ( 24, std::vector< edge >( 24 ) );

    dlog.addText( Logger::TEAM,
                  __FILE__": OppMarkAssign" );

    if( max_cycle < 10 )
    {
        max_cycle = 10;
    }

    rate = std::min( 0.40, (double)opp_min / max_cycle);

    for( int i = 0; i < (int)our_unum.size(); ++i )
    {
        const CoachPlayerObject * p = wm.teammate( our_unum[i] );
        if ( ! p ) continue;
        if ( p->goalie() ) continue;
        add_edge( 0, our_unum[i], 1, 0, G );
    }

    for( int i = 0; i < (int)their_unum.size(); ++i )
    {
        const CoachPlayerObject * opp = wm.opponent( their_unum[i] );
        if ( ! opp ) continue;
        if( opp->goalie() ) continue;
        add_edge( their_unum[i] + 11, 23, 1, 0, G);
    }

    //重み付き二部グラフの作成

    for( int i = 0; i < (int)our_unum.size(); ++i)
    {
        const CoachPlayerObject * p = wm.teammate( our_unum[i] );
        if ( ! p )continue;
        if ( p->goalie() )
        {
            M_candidate_unum_marking_setplay[ p->unum() - 1 ] = Unum_Unknown;
            continue;
        }
        const Vector2D home_pos = CoachStrategy::i().getPosition( p->unum() );

        for( int j = 0; j < (int)their_unum.size(); ++j)
        {
            if( their_unum[j] == Unum_Unknown )
            {
                continue;
            }

            const CoachPlayerObject * opp = wm.opponent( their_unum[j] );

            if ( ! opp )
                continue;
            if( opp->goalie() )
                continue;

            Vector2D opp_pos = predictOpponentPos( wm,
                                                   opp_min,
                                                   opp->unum() );
            if( !opp_pos.isValid() ) continue;

            if( CoachStrategy::i().getRoleName( p->unum() ) == "CenterBack"
                || CoachStrategy::i().getRoleName( p->unum() ) == "SideBack" )
            {
                rate = 1.0;
            }
            else
            {
                rate = std::min( 0.40, (double)opp_min / max_cycle );
            }

            double opp_dist = ( opp_pos.dist( home_pos ) * rate + opp_pos.dist( p->pos() ) * ( 1- rate ) ) * 100.0;
            int num = 1;
            // 輸送量を変えるならここ

            add_edge( our_unum[i], their_unum[j] + 11, num, (int)opp_dist, G );

        }
    }

    //最小費用流を解く


    min_cost_flow( 0, 23, (int)our_unum.size(), G );

    return true;
}



/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentSetplayMark::findOpponentToMark( CoachAgent * agent )
{


    dlog.addText( Logger::ANALYZER,
                  __FILE__":(findOpponentToMark)" );

    const CoachWorldModel & wm = agent->world();

    bool is_marking[11];
    bool is_marking_opp[11];
    //  int mark_unum[11];
    int mark_count = 0;

    // int unum_possible_kicker = Unum_Unknown;
    // const CoachPlayerObject * opp_possible_kicker = wm.currentState().fastestInterceptOpponent();

    // if ( opp_possible_kicker )
    // {
    //     unum_possible_kicker = opp_possible_kicker->unum();
    // }

    for ( int i = 0; i < 11; ++i )
    {
        M_candidate_unum_marking_setplay[i] = Unum_Unknown;
        is_marking[i] = false;
        is_marking_opp[i] = false;
    }

    for ( int k = 0; k < 11; ++k )
    {
        double dist = 0.0;
        int mark_unum = 0;
        int target_unum = 0;
        double min_dist = 1000000.0;

        //	    std::cout<< "mark_count:" << mark_count << std::endl;

        for( CoachPlayerObject::Cont::const_iterator it = wm.teammates().begin(),
                 end = wm.teammates().end();
             it != end;
             ++it )
        {
            const CoachPlayerObject * p = *it;

            if ( ! p )
                continue;

            if ( p->goalie() )
            {
                is_marking[ p->unum() - 1 ] = true;
                continue;
            }

            if( is_marking[ p->unum() - 1 ]  )
                continue;

            for( int i = 0; i < 11; ++i)
            {
                const CoachPlayerObject * opp = wm.opponent( i + 1 );

                if ( ! opp )
                    continue;

                if ( opp->goalie() )
                {
                    is_marking_opp[ i ] = true;
                    continue;
                }

                if( is_marking_opp[ i ] )
                    continue;

                dist = (int) std::floor( std::fabs( p->pos().dist( opp->pos()  ) ) );


                if( std::min( min_dist, dist ) == dist )
                {
                    min_dist = dist;
                    mark_unum = p->unum() - 1;
                    target_unum = i;
                }

            }
        }

        is_marking[ mark_unum ] = true;
        CoachAnalyzerManager::instance().setMarkAssignment( mark_unum + 1,
                                                            target_unum + 1,
                                                            Unum_Unknown);
        M_candidate_unum_marking_setplay[ mark_unum ] = target_unum + 1;

        is_marking_opp[ target_unum ] = true;
        mark_count += 1;
        min_dist = 1000000.0;

        if( mark_count >= 10 )
            break;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!



bool
OpponentSetplayMark::assignedMarker( CoachAgent * agent,
                                     int opp_min,
                                     std::vector<int> & our_unum,
                                     std::vector<int> & their_unum )
{
    if( our_unum.size() != their_unum.size() ) return false;
    const CoachWorldModel & wm = agent->world();
    int size = (int)our_unum.size();
    int path[2][size];
    int type_matrix[size][size];
    int cost_matrix[size][size];
    int max_cycle = M_max_cycle;
    double rate;
    MSecTimer timer;
    std::vector <int> RowCover;
    std::vector <int> ColCover;
    RowCover.reserve( size );
    ColCover.reserve( size );

    dlog.addText( Logger::TEAM,
                  __FILE__": OppMarkAssign" );

    if( max_cycle < 10 )
    {
        max_cycle = 10;
    }

    rate = std::min( 0.40, (double)opp_min / max_cycle);

    for( int i = 0; i < size; ++i )
    {
        path[0][i] = 0;
        path[1][i] = 0;
        RowCover.push_back( 0 );
        ColCover.push_back( 0 );
        for(int j = 0; j < size; ++j )
        {
            cost_matrix[i][j] = 0;
            type_matrix[i][j] = 0;
        }
    }

    //キョリ行列の作成,各行の最小値の発見
    for( int i = 0; i < size; ++i)
    {
        const CoachPlayerObject * p = wm.teammate( our_unum[i] );
        if ( ! p )continue;
        if ( p->goalie() )
        {
            M_candidate_unum_marking_setplay[ p->unum() - 1 ] = Unum_Unknown;
            continue;
        }

        const Vector2D home_pos = CoachStrategy::i().getPosition( p->unum() );

        double min_in_row = 0.0;
        for( int j = 0; j < size; ++j)
        {
            if( their_unum[j] == Unum_Unknown )
            {
                cost_matrix[i][j] = 70000;
                continue;
            }

            const CoachPlayerObject * opp = wm.opponent( their_unum[j] );

            if ( ! opp )
                continue;
            if( opp->goalie() )
                continue;

            Vector2D opp_pos = predictOpponentPos( wm,
                                                   opp_min,
                                                   opp->unum() );
            if( !opp_pos.isValid() ) continue;

            if( CoachStrategy::i().getRoleName( p->unum() ) == "CenterBack"
                || CoachStrategy::i().getRoleName( p->unum() ) == "SideBack" )
            {
                rate = 1.0;
            }
            else
            {
                rate = std::min( 0.40, (double)opp_min / max_cycle);
            }

            double opp_dist = ( opp_pos.dist( home_pos ) * rate + opp_pos.dist( p->pos() ) * ( 1- rate ) ) * 100.0;

            if( j == 0 )
                min_in_row = (int)opp_dist;

            cost_matrix[i][j] = (int)opp_dist;
            if( cost_matrix[i][j] < min_in_row )
                min_in_row = cost_matrix[i][j];
        }
        for( int j = 0; j < size; ++j )
        {
            cost_matrix[i][j] -= min_in_row;
        }
    }

    for( int i = 0; i < size; ++i )
    {
        for( int j = 0; j < size; ++j )
        {
            if( cost_matrix[i][j] == 0 && RowCover[i] == 0 && ColCover[j] == 0 )
            {
                type_matrix[i][j] = 1;
                RowCover[i] = 1;
                ColCover[j] = 1;
            }
        }
    }

    bool isFinish = false;
    int step = 0;
    int row = 0;
    int col = 0;
    bool done = 0;
    int i = 0;
    int j = 0;
    int r = 0;
    int c = 0;
    int minval = 0;
    int mate_unum = 0;
    int opp_unum = 0;
    int colnum = 0;
    bool is_star_in_row = false;
    int path_count = 0;
    bool isBreak = false;
    int k = 0;

    std::pair<int,int> rc;


    while( !isFinish )
    {
        ++k;
        if( k > 70 )
            return false;

        switch( step )
        {
            if( timer.elapsedReal() >= 100.0 )
            {
                std::cout<<"time too much " << timer.elapsedReal() * 100 << std::endl;
                return false;
            }
        case 0:

            colnum = 0;

            for( i = 0; i < size; ++i )
            {
                for( j = 0; j < size; ++j )
                {
                    if( type_matrix[i][j] == 1 )
                        ColCover[j] = 1;
                }
            }

            for( i = 0; i < size; ++i )
                if( ColCover[i] == 1 )
                    ++colnum;

            if( colnum >= size )
            {
                step = 4;
            }
            else
                step = 1;

            break;
        case 1:


            done = false;

            while( !done )
            {
                //find_a_zero;
                row = -1;
                col = -1;
                for( i = 0; i < size; ++i )
                {
                    for( j = 0; j < size; ++j )
                    {
                        if( cost_matrix[i][j] == 0 && RowCover[i] == 0 && ColCover[j] == 0 )
                        {
                            row = i;
                            col = j;
                            isBreak = true;
                            break;
                        }
                    }
                    if( isBreak )
                    {
                        isBreak = false;
                        break;
                    }
                }


                if( row == -1 )
                {
                    done = true;
                    step = 3;
                }
                else
                {
                    type_matrix[row][col] = 2;
                    //is_star_in_row
                    for( i = 0; i < size; ++i )
                    {
                        if( type_matrix[row][i] == 1 )
                        {
                            is_star_in_row = true;
                            col = i;
                        }
                    }
                    if( is_star_in_row )
                    {
                        RowCover[row] = 1;
                        ColCover[col] = 0;
                        is_star_in_row = false;
                    }
                    else
                    {
                        done = true;
                        step = 2;
                        rc.first = row;
                        rc.second = col;
                    }
                }
            }

            break;

        case 2:

            r = -1;
            c = -1;
            path_count = 1;
            path[0][ path_count - 1] = rc.first;
            path[1][ path_count - 1] = rc.second;
            done = false;

            while ( !done )
            {

                // find star in col
                r = -1;
                for( i = 0; i < size; ++i )
                {
                    if ( type_matrix[i][ path[1][path_count-1] ] == 1 )
                        r  = i;
                }
                if( r > -1 )
                {
                    ++path_count;
                    path[0][path_count - 1] = r;
                    path[1][path_count - 1] = path[1][path_count-2];

                }
                else
                    done = true;

                if( !done )
                {
                    //find prime in row
                    for( i = 0; i < size; ++i )
                    {
                        if ( type_matrix[ path[0][path_count-1] ][i] == 2 )
                            c = i;
                    }

                    ++path_count;
                    path[1][path_count - 1] = c;
                    path[0][path_count - 1] = path[0][path_count-2];
                }
            }

            //augment_path
            for( i = 0; i < path_count; ++i )
            {
                if( type_matrix[ path[0][i] ][ path[1][i] ] == 1 )
                {

                    type_matrix[ path[0][i] ][ path[1][i] ] = 0;
                }
                else
                {
                    type_matrix[ path[0][i] ][ path[1][i] ] = 1;

                }
            }


            //clearCovers
            for( i = 0; i < size; ++i )
            {
                RowCover[i] = 0;
                ColCover[i] = 0;
            }

            //erase_primes
            for( i = 0; i < size; ++i )
            {
                for( j = 0; j < size; ++j )
                {
                    if( type_matrix[i][j] == 2 )
                        type_matrix[i][j] = 0;
                }
            }

            step = 0;
            break;

        case 3:

            minval = 10000000;

            for( i = 0; i < size; ++i )
            {
                for( j = 0; j < size; ++j )
                {
                    if( RowCover[i] == 0 && ColCover[j] == 0 && minval > cost_matrix[i][j] )
                        minval = cost_matrix[i][j];
                }
            }

            for( i = 0; i < size; ++i )
            {
                for( j = 0; j < size; ++j )
                {
                    if( RowCover[i] == 1 )
                        cost_matrix[i][j] += minval;
                    if( ColCover[j] == 0 )
                        cost_matrix[i][j] -= minval;
                }
            }

            step = 1;
            break;

        case 4:

            for( i = 0; i < size; ++i )
            {
                for( j = 0; j < size; ++j )
                {
                    if( type_matrix[i][j] == 1 )
                    {
                        mate_unum = our_unum[i];
                        opp_unum = their_unum[j];
                        M_candidate_unum_marking_setplay[mate_unum - 1] = opp_unum;
                    }
                }
            }

            isFinish = true;
            done = true;
            break;
        }
    }
    return true;
}

*/
/*-------------------------------------------------------------------*/
/*!

 */

void
OpponentSetplayMark::assignedSetPlayMarker( rcsc::CoachAgent * agent,
                                            int opp_min,
                                            std::vector< int > & our_unum,
                                            std::vector< int > & their_unum )
{
    const CoachWorldModel & wm = agent->world();
    int size = our_unum.size();
    bool is_marking_opp[size];
    double min_dist = 1000.0;
    int unum_min_dist = Unum_Unknown;
    int max_cycle = M_max_cycle;
    double rate;
    if( max_cycle < 10 )
    {
        max_cycle = 10;
    }

    rate = opp_min / max_cycle;

    for ( int i = 0; i < size; ++i )
    {
        is_marking_opp[i] = false;
    }

    for ( std::vector<int>::iterator opp_it = their_unum.begin();
          opp_it != their_unum.end();
          ++opp_it )
    {
        Vector2D opp_pos = predictOpponentPos( wm,
                                               opp_min,
                                               *opp_it );
        if( !opp_pos.isValid() ) continue;

        double dist = opp_pos.dist( wm.ball().pos() );

        if( min_dist > dist )
        {
            min_dist = dist;
            unum_min_dist = *opp_it;
        }
    }

    for ( std::vector<int>::iterator opp_it = their_unum.begin();
          opp_it != their_unum.end();
          ++opp_it )
    {
        Vector2D opp_pos = predictOpponentPos( wm,
                                               opp_min,
                                               *opp_it );
        if( !opp_pos.isValid() ) continue;

        if( wm.ball().pos().dist( opp_pos ) < 15.0
            || ( unum_min_dist == *opp_it && M_current_opp_type != Type_Normal ) )
        {
            min_dist = 1000.0;
            int candidate_marker = Unum_Unknown;
            for ( int i = 0; i < size; ++i )
            {
                const CoachPlayerObject * mate = wm.teammate( our_unum[i] );
                const double circle_r = ServerParam::i().centerCircleR() + mate->playerTypePtr()->playerSize() + 0.001;
                Vector2D home_pos = CoachStrategy::i().getPosition( our_unum[i] );
                double dist = 0;
                if( is_marking_opp[i] ) continue;

                if( home_pos.dist( opp_pos ) > 15.0 )
                {
                    Circle2D move_horizon( home_pos, 15.0 );
                    Segment2D pass_line( opp_pos, wm.ball().pos() );
                    Vector2D sol1, sol2;
                    Vector2D cut_point = Vector2D::INVALIDATED;
                    int n_sol = move_horizon.intersection( pass_line, &sol1, &sol2 );
                    if ( n_sol == 0 ) continue;
                    else if ( n_sol == 1 )
                    {
                        cut_point = sol1;
                    }
                    else
                    {
                        cut_point = ( opp_pos.dist2( sol1 )
                                      < opp_pos.dist2( sol2 )
                                      ? sol1
                                      : sol2 );
                    }
                    if( !cut_point.isValid() ) continue;
                    else if( cut_point.dist( wm.ball().pos() ) < circle_r ) continue;
                    dist = cut_point.dist( home_pos ) * rate + cut_point.dist( mate->pos() ) * ( 1 - rate );
                }
                else
                {
                    dist = opp_pos.dist( home_pos ) * rate + opp_pos.dist( mate->pos() ) * ( 1- rate );
                }

                if( dist < min_dist )
                {
                    min_dist = dist;
                    candidate_marker = i;
                }
            }

            if( candidate_marker != Unum_Unknown )
            {
                int mate_unum = our_unum[candidate_marker];
                M_candidate_unum_marking_setplay[mate_unum-1] = *opp_it;
                is_marking_opp[candidate_marker] = true;
            }
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentSetplayMark::createCandidates( const CoachWorldModel & wm,
                                       int opp_min,
                                       std::vector< int > & our_unum,
                                       std::vector< int > & our_setplay_marker,
                                       std::vector< int > & their_unum,
                                       std::vector< int > & their_defender_unum  )
{
#ifdef DEBUG_PRINT_TARGET_CANDIDATES
    dlog.addText( Logger::MARK,
                  __FILE__":(createTargetCandidates)" );
#endif

    std::vector< int > their_forward_unum;

    our_unum.reserve( 11 );
    our_setplay_marker.reserve( 11 );
    their_unum.reserve( 11 );
    their_defender_unum.reserve( 11 );
    their_forward_unum.reserve( 11 );

    for( int unum = 1; unum <= 11; ++unum )
    {
        const CoachPlayerObject * p = wm.teammate( unum );
        if ( ! p )continue;
        if ( p->goalie() )
            continue;

        bool isMarker = CoachStrategy::i().isMarkerType( unum );
        bool isSetPlayMarker = CoachStrategy::i().isSetPlayMarkerType( unum );

        if( !isMarker )
        {
            if( isSetPlayMarker )
                our_setplay_marker.push_back( unum );
            continue;
        }
        our_unum.push_back( unum );
    }

    int candidate_kicker = Unum_Unknown;
    double min_dist = 1000.0;
    double their_defense_line = -ServerParam::i().pitchHalfLength();
    double their_forward_line = ServerParam::i().pitchHalfLength();

    for( int unum = 1; unum <= 11; ++unum )
    {
        Vector2D opp_pos = predictOpponentPos( wm,
                                               opp_min,
                                               unum );
        if( !opp_pos.isValid() ) continue;

        double dist = wm.ball().pos().dist( opp_pos );
        if( min_dist > dist
            && dist < 5.0 )
        {
            min_dist = dist;
            candidate_kicker = unum;
        }

        if( opp_pos.x > their_defense_line )
            their_defense_line = opp_pos.x;
        if( opp_pos.x < their_forward_line )
            their_forward_line = opp_pos.x;
    }

    for( int unum = 1; unum <= 11; ++unum )
    {

        if( unum == candidate_kicker
            && M_current_opp_type == Type_Gliders ) continue;

        Vector2D opp_pos = predictOpponentPos( wm,
                                               opp_min,
                                               unum );
        if( !opp_pos.isValid() ) continue;

        Vector2D sector_point_df( their_defense_line + 0.1, opp_pos.y );
        Sector2D sector_df( sector_point_df, 0.0,
                            ( opp_pos - sector_point_df ).r(),
                            AngleDeg( 90.0 ), AngleDeg( -90.0 ) );

        Vector2D sector_point_fw( their_forward_line - 0.1, opp_pos.y );
        Sector2D sector_fw( sector_point_fw, 0.0,
                            ( opp_pos - sector_point_fw ).r(),
                            AngleDeg( -90.0 ), AngleDeg( 90.0 ) );


        bool opp_is_defender = true;
        bool opp_is_forward = true;

        for( int unum2 = 1; unum2 <= 11; ++unum2 )
        {
            Vector2D opp_pos2 =  predictOpponentPos( wm,
                                                     opp_min,
                                                     unum2 );
            if( !opp_pos2.isValid() ) continue;

            if( unum == unum2 ) continue;

            if ( sector_df.contains( opp_pos2 )
                 || ( wm.ball().pos().dist( opp_pos ) < 10.0
                      && ( opp_pos.y - wm.ball().pos().y ) > -5.0 ) )// ServerParam::i().penaltyAreaHalfWidth() < opp_pos.absY() )
            {
                opp_is_defender =  false;
                //their_unum.push_back( unum );
                //break;
            }
            if ( sector_fw.contains( opp_pos2 ) )
            {
                opp_is_forward = false;
            }
        }

        if ( opp_is_forward )
            their_forward_unum.push_back( unum );
        else if ( opp_is_defender )
            their_defender_unum.push_back( unum );
        else
            their_unum.push_back( unum );
    }

    int their_size = (int)their_unum.size() + (int)their_forward_unum.size();
    int our_size = (int)our_unum.size();

    if( (int)their_forward_unum.size() > our_size )
        return false;

    if( their_size == our_size )
    {
        for ( std::vector<int>::iterator fw = their_forward_unum.begin();
              fw != their_forward_unum.end();
              ++fw )
        {
            their_unum.push_back( *fw );
        }
        return true;
    }
    else if( their_size >= our_size )
    {
        std::vector< std::pair<double,int> > ball_dist;
        ball_dist.reserve( 11 );

        for ( std::vector<int>::iterator it = our_setplay_marker.begin();
              it != our_setplay_marker.end();
              ++it )
        {
            const CoachPlayerObject * mate = wm.teammate( *it );

            if( !mate )
                continue;
            double dist = wm.ball().pos().dist( mate->pos() );
            ball_dist.push_back( std::make_pair( dist, *it ) );
        }

        std::sort( ball_dist.begin(), ball_dist.end() );

        for( std::vector< std::pair< double,int> >::iterator it = ball_dist.begin();
             it != ball_dist.end();
             ++it )
        {
            const CoachPlayerObject * mate = wm.teammate( (it)->second );
            if( !mate ) continue;
            /*
              double thr = 13.0;
              if( M_current_opp_type != Type_Normal
              && wm.ball().pos().x > 0.0 )
              thr = 30.0;
            */
            if( mate->pos().dist( wm.ball().pos() ) < 15.0 )
            {
                our_unum.push_back( (it)->second );
                std::vector< int >::iterator clear = find( our_setplay_marker.begin(),our_setplay_marker.end() , (it)->second );
                if( clear != our_setplay_marker.end()
                    || (it)->second == *clear )
                    our_setplay_marker.erase( clear );

                our_size = (int)our_unum.size();
                if( our_size == their_size )
                {
                    for ( std::vector<int>::iterator fw = their_forward_unum.begin();
                          fw != their_forward_unum.end();
                          ++fw )
                    {
                        their_unum.push_back( *fw );
                    }
                    return true;
                }
            }
        }

        ball_dist.clear();

        for ( std::vector<int>::iterator it = their_unum.begin();
              it != their_unum.end();
              ++it )
        {
            Vector2D opp_pos = predictOpponentPos( wm,
                                                   opp_min,
                                                   *it );
            if( !opp_pos.isValid() ) continue;
            double dist =  wm.ball().pos().dist( opp_pos ) + ServerParam::i().ourTeamGoalPos().dist2( wm.ball().pos() );
            ball_dist.push_back( std::make_pair( dist, *it ) );
        }

        std::sort( ball_dist.begin(), ball_dist.end() );
        their_unum.clear();

        for( std::vector< std::pair< double,int> >::iterator it = ball_dist.begin();
             it != ball_dist.end();
             ++it )
        {
            their_unum.push_back( (it)->second );
            their_size = (int)their_forward_unum.size() + (int)their_unum.size();
            if( their_size == our_size )
            {
                 for ( std::vector<int>::iterator fw = their_forward_unum.begin();
                          fw != their_forward_unum.end();
                       ++fw )
                 {
                     their_unum.push_back( *fw );
                 }
                 return true;
            }
        }
    }
    else
    {
        int diff_size = our_size - their_size;
        for( int i = 0; i < diff_size; ++i )
        {
            their_unum.push_back( Unum_Unknown );
        }
        their_size = (int)their_forward_unum.size() + (int)their_unum.size();
        if( their_size == our_size )
        {
            for ( std::vector<int>::iterator fw = their_forward_unum.begin();
                  fw != their_forward_unum.end();
                  ++fw )
            {
                their_unum.push_back( *fw );
            }
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */

int
OpponentSetplayMark::estimateOppMin( const CoachWorldModel & wm )
{
    int opp_min = 100;
    int tmp_min = 100;
    bool is_reached = false;

    for( int i = 1; i <= 11; ++i )
    {
        int opp_unum = i;
        const CoachPlayerObject * opp = wm.opponent( opp_unum );
        if ( ! opp ) continue;

        double dist = wm.ball().pos().dist( opp->pos() + opp->vel() );
        if( dist < 2.0 )
        {
            is_reached = true;
            break;
        }

        int tmp_cycle = opp->playerTypePtr()->cyclesToReachDistance( dist );
        Vector2D to_ball = wm.ball().pos() - opp->pos();

        if( opp_min > tmp_cycle )
        {
            tmp_min = tmp_cycle;
            if(  opp->vel().r() > 0.005//0.01~0.001あたりが最適？
                 && ( to_ball.x * opp->vel().x > 0
                      || to_ball.y * opp->vel().y > 0 ) )
            {
                opp_min = tmp_cycle;
            }
        }
    }

    if( is_reached )
    {
        for( int i = 0; i < 11; ++i )
        {
            const CoachPlayerObject * opp = wm.opponent( i + 1 );

            if( !opp ) continue;
            if( opp->goalie() ) continue;

            Vector2D to_ball = wm.ball().pos() - opp->pos();
            if( opp->vel().r() > 0.005
                && to_ball.x * opp->vel().x > 0
                && to_ball.y * opp->vel().y > 0 )
            {
                double dist = wm.ball().pos().dist( opp->pos() );
                if( dist > 30.0 ) continue;
                if( dist > 5.0 )
                    dist -= 5.0;
                int tmp_cycle = opp->playerTypePtr()->cyclesToReachDistance( dist );
                if( opp_min > tmp_cycle )
                    opp_min = tmp_cycle;
            }
        }

        if( opp_min > 20 )
            opp_min = 5;
    }

    if( opp_min >= 100 )
    {
        opp_min = tmp_min;
    }

    return opp_min;
}

/*-------------------------------------------------------------------*/
/*!

 */

Vector2D
OpponentSetplayMark::predictOpponentPos( const CoachWorldModel & wm,
                                       int opp_min,
                                       int unum )
{
    const CoachPlayerObject * opp =  wm.opponent( unum );
    if( !opp ) return Vector2D::INVALIDATED;
    if( opp->goalie() ) return Vector2D::INVALIDATED;

    Vector2D opp_pos = opp->pos();
    if( opp_min <= 1
        || opp->vel().r() < 0.2 )
    {
        opp_pos += opp->vel();
    }
    else
    {
        Vector2D addvel = opp->vel().setLengthVector( opp->playerTypePtr()->realSpeedMax() ) * opp_min;
        if( ( opp_pos + addvel ).dist( wm.ball().pos() ) > 15.0 )
        {
            addvel = opp->vel() * opp_min;
        }
        opp_pos += addvel;
    }

    if( opp_pos.absX() > 52.4
        || opp_pos.absY() > 34.0 )
    {
        Line2D line( opp_pos, opp->pos() );
        if( ( opp_pos.absX() - 52.4 ) > ( opp_pos.absY() - 34.0 ) )
        {
            double new_y = line.getY( sign( opp_pos.x ) * 52.4 );
            opp_pos.x = sign( opp_pos.x ) * 52.4;
            opp_pos.y = new_y;
        }
        else
        {
            double new_x = line.getX( sign( opp_pos.y ) * 34.0 );
            opp_pos.x = new_x;
            opp_pos.y = sign( opp_pos.y ) * 34.0;
        }
    }
    if( opp->pos().dist( wm.ball().pos() ) < 3.0
        && ( opp->pos().absX() > 52.4
             || opp->pos().absY() > 34.0 ) )
    {
        opp_pos = opp->pos();
    }

    return opp_pos;
}

/*-------------------------------------------------------------------*/
/*!

 */

bool
OpponentSetplayMark::createCandidates( const CoachWorldModel & wm,
                                       int opp_min,
                                       std::vector< int > & our_unum,
                                       std::vector< int > & their_unum )
{
#ifdef DEBUG_PRINT_TARGET_CANDIDATES
    dlog.addText( Logger::MARK,
                  __FILE__":(createTargetCandidates)" );
#endif
    std::vector< int > their_forward_unum;
    std::vector< std::pair<double,int> > their_defender_unum;
    their_defender_unum.reserve( 11 );
    our_unum.reserve( 11 );
    their_unum.reserve( 11 );
    their_forward_unum.reserve( 11 );

    /*
    double thr = 13.0;
    if( M_current_opp_type != Type_Normal )
        thr = 20.0;
    */
    for( int unum = 1; unum <= 11; ++unum )
    {
        const CoachPlayerObject * p = wm.teammate( unum );
        if ( ! p )continue;
        if ( p->goalie() )
            continue;

        bool isSetPlayMarker = CoachStrategy::i().isSetPlayMarkerType( unum );

        if( !isSetPlayMarker
            ||  ( wm.ball().pos().x < 0
                  && CoachStrategy::i().getRoleName( unum ) == "CenterForward" ) )
        {
            continue;
        }
        our_unum.push_back( unum );
    }

    int candidate_kicker = Unum_Unknown;
    double min_dist = 1000.0;
    double their_defense_line = -ServerParam::i().pitchHalfLength();
    double their_forward_line = ServerParam::i().pitchHalfLength();

    for( int unum = 1; unum <= 11; ++unum )
    {
        Vector2D opp_pos = predictOpponentPos( wm,
                                               opp_min,
                                               unum );
        if( !opp_pos.isValid() ) continue;

        double dist = wm.ball().pos().dist( opp_pos );
        if( min_dist > dist
            && dist < 5.0 )
        {
            min_dist = dist;
            candidate_kicker = unum;
        }

        if( opp_pos.x > their_defense_line )
            their_defense_line = opp_pos.x;
        if( opp_pos.x < their_forward_line )
            their_forward_line = opp_pos.x;
    }

    for( int unum = 1; unum <= 11; ++unum )
    {

        if( unum == candidate_kicker
            && M_current_opp_type == Type_Gliders ) continue;

        Vector2D opp_pos = predictOpponentPos( wm,
                                               opp_min,
                                               unum );
        if( !opp_pos.isValid() ) continue;

        Vector2D sector_point_df( their_defense_line + 0.1, opp_pos.y );
        Sector2D sector_df( sector_point_df, 0.0,
                            ( opp_pos - sector_point_df ).r(),
                            AngleDeg( 90.0 ), AngleDeg( -90.0 ) );

        Vector2D sector_point_fw( their_forward_line - 0.1, opp_pos.y );
        Sector2D sector_fw( sector_point_fw, 0.0,
                            ( opp_pos - sector_point_fw ).r(),
                            AngleDeg( -90.0 ), AngleDeg( 90.0 ) );


        bool opp_is_defender = true;
        bool opp_is_forward = true;

        for( int unum2 = 1; unum2 <= 11; ++unum2 )
        {
            Vector2D opp_pos2 =  predictOpponentPos( wm,
                                                     opp_min,
                                                     unum2 );
            if( !opp_pos2.isValid() ) continue;

            if( unum == unum2 ) continue;

            if ( sector_df.contains( opp_pos2 )
                 || ( wm.ball().pos().dist( opp_pos ) < 10.0
                      && ( opp_pos.y - wm.ball().pos().y ) > -5.0 ) )
            {
                opp_is_defender =  false;
            }
            if ( sector_fw.contains( opp_pos2 ) )
            {
                opp_is_forward = false;
            }
        }

        if ( opp_is_forward )
            their_forward_unum.push_back( unum );
        else if ( opp_is_defender )
        {
            double dist = wm.ball().pos().dist( opp_pos );
            their_defender_unum.push_back( std::make_pair( dist, unum ) );
        }
        else
            their_unum.push_back( unum );
    }

    int their_size = (int)their_unum.size() + (int)their_forward_unum.size();
    int our_size = (int)our_unum.size();

    if( their_size <= our_size )
    {
        for ( std::vector<int>::iterator fw = their_forward_unum.begin();
              fw != their_forward_unum.end();
              ++fw )
        {
            their_unum.push_back( *fw );
        }
        return true;
    }
    else if( their_size >= our_size )
    {
        std::vector< std::pair<double,int> > ball_dist;
        ball_dist.reserve( (int)their_unum.size() );
        for ( std::vector<int>::iterator it = their_unum.begin();
              it != their_unum.end();
              ++it )
        {
            Vector2D opp_pos = predictOpponentPos( wm,
                                                   opp_min,
                                                   *it );
            if( !opp_pos.isValid() ) continue;
            double dist = wm.ball().pos().dist( opp_pos ) + ServerParam::i().ourTeamGoalPos().dist2( wm.ball().pos() );
            ball_dist.push_back( std::make_pair( dist, *it ) );
        }

        std::sort( ball_dist.begin(), ball_dist.end() );
        their_unum.clear();

        for( std::vector< std::pair< double,int> >::iterator it = ball_dist.begin();
             it != ball_dist.end();
             ++it )
        {
            their_unum.push_back( (it)->second );
            their_size = (int)their_forward_unum.size() + (int)their_unum.size();
            if( their_size == our_size )
            {
                for ( std::vector<int>::iterator fw = their_forward_unum.begin();
                      fw != their_forward_unum.end();
                      ++fw )
                {
                    their_unum.push_back( *fw );
                }
                return true;
            }
        }
    }
    /*
    else
    {
        std::sort( their_defender_unum.begin(), their_defender_unum.end() );
        for( std::vector< std::pair< double,int> >::iterator it = their_defender_unum.begin();
             it != their_defender_unum.end();
             ++it )
        {
            if( it->first < thr
                || ( M_current_opp_type == Type_Oxsy && it == their_defender_unum.begin() ) )
            {
                their_unum.push_back( it->second );
                their_size = (int)their_forward_unum.size() + (int)their_unum.size();
            }
            if( their_size == our_size )
            {
                for ( std::vector<int>::iterator fw = their_forward_unum.begin();
                      fw != their_forward_unum.end();
                      ++fw )
                {
                    their_unum.push_back( *fw );
                }
                return true;
            }
        }
        int diff_size = our_size - their_size;
        for( int i = 0; i < diff_size; ++i )
        {
            their_unum.push_back( Unum_Unknown );
        }
    }
    */
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */

void
OpponentSetplayMark::compareCandidate( const  CoachWorldModel & wm,
                                      int opp_min )
{
    int pre_num = 0;
    int num = 0;
    double sum = 0;
    double pre_sum = 0;
    bool isTarget[11];
    bool isPreTarget[11];

    for( int i = 0; i < 11; ++i )
    {
        isTarget[i] = false;
        isPreTarget[i] = false;
    }

    for( int i = 0; i < 11; ++i )
    {
        int opp_unum = M_candidate_unum_marking_setplay[i];
        if( opp_unum >= 1 && opp_unum <= 11 )
            isTarget[opp_unum-1] = true;
        opp_unum = M_pre_candidate_unum_setplay[i];
        if( opp_unum >= 1 && opp_unum <= 11 )
            isPreTarget[opp_unum-1] = true;
    }

    for( int i = 0; i < 11; ++i )
    {
        if( isTarget[i] != isPreTarget[i] )
            return;
    }

    for( int i = 1; i <= 11; ++i )
    {
        int mate_unum = i;
        int opp_unum = M_candidate_unum_marking_setplay[mate_unum - 1];

        if( opp_unum < 1 || opp_unum > 11 )
            opp_unum = Unum_Unknown;

        if( opp_unum != Unum_Unknown )
        {
            Vector2D opp_pos = predictOpponentPos( wm,
                                                   opp_min,
                                                   opp_unum );
            if( !opp_pos.isValid() ) continue;
            ++num;

            Vector2D home_pos = CoachStrategy::i().getPosition( i );
            sum += home_pos.dist( opp_pos );
        }
    }

    if( num != 0 )
        sum /= num;

    for( int i = 1;i <= 11; ++i )
    {
        int mate_unum = i;
        int opp_unum = M_pre_candidate_unum_setplay[mate_unum -1];

        if( opp_unum < 1 || opp_unum > 11 )
            opp_unum = Unum_Unknown;

        if( opp_unum != Unum_Unknown )
        {
            Vector2D opp_pos = predictOpponentPos( wm,
                                                   opp_min,
                                                   opp_unum );
            if( !opp_pos.isValid() ) continue;
            ++pre_num;

            Vector2D home_pos = CoachStrategy::i().getPosition( i );
            pre_sum += home_pos.dist( opp_pos );
        }
    }

    if( pre_num != 0 )
        pre_sum /= pre_num;

    if( sum > pre_sum )
    {
        for( int i = 0; i < 11; ++i )
        {
            M_candidate_unum_marking_setplay[i] = M_pre_candidate_unum_setplay[i];
        }
    }
}

#if 0

/*-------------------------------------------------------------------*/
/*!

 */
int
OpponentSetplayMark::predict_player_turn_cycle( const PlayerType * ptype,
                                                const AngleDeg & player_body,
                                                const double & player_speed,
                                                const double & target_dist,
                                                const AngleDeg & target_angle,
                                                const double & dist_thr,
                                                const bool use_back_dash )
{
    const ServerParam & SP = ServerParam::i();

    int n_turn = 0;

    double angle_diff = ( target_angle - player_body ).abs();

    if ( use_back_dash
         && target_dist < 5.0 // Magic Number
                          && angle_diff > 90.0
         && SP.minDashPower() < -SP.maxDashPower() + 1.0 )
    {
        angle_diff = std::fabs( angle_diff - 180.0 );    // assume backward dash
    }

    double turn_margin = 180.0;
    if ( dist_thr < target_dist )
    {
        turn_margin = std::max( 15.0, // Magic Number
                                AngleDeg::asin_deg( dist_thr / target_dist ) );
    }

    double speed = player_speed;
    while ( angle_diff > turn_margin )
    {
        angle_diff -= ptype->effectiveTurn( SP.maxMoment(), speed );
        speed *= ptype->playerDecay();
        ++n_turn;
    }

#ifdef DEBUG_PREDICT_PLAYER_TURN_CYCLE
    dlog.addText( Logger::ANALYZER,
                  "(predict_player_turn_cycle) angleDiff=%.3f turnMargin=%.3f speed=%.2f n_turn=%d",
                  angle_diff, turn_margin, player_speed, n_turn );
#endif

    return n_turn;
}


/*-------------------------------------------------------------------*/
/*
 */

int
OpponentSetplayMark::predict_self_reach_cycle( const CoachPlayerObject *p,
                                               const Vector2D & target_point,
                                               const double & dist_thr,
                                               const int wait_cycle,
                                               const bool save_recovery,
                                               StaminaModel  stamina )
{
    //const CoachWorldModel & wm = agent->world();

    const ServerParam & SP = ServerParam::i();
    const PlayerType *ptype = p->playerTypePtr();
    const double recover_dec_thr = SP.recoverDecThrValue();

    const double first_speed = p->vel().r() * std::pow( ptype->playerDecay(), wait_cycle );

    StaminaModel first_stamina_model = p->staminaModel();

    if ( ptype->inertiaPoint( p->pos(),p->vel(),wait_cycle ).dist2( target_point ) < std::pow( dist_thr, 2 ) )
    {
        return 0;
    }

    // if ( wait_cycle > 0 )
    // {
    //        first_stamina_model.simulateWaits( ptype, wait_cycle );
    // }

    for ( int cycle = std::max( 0, wait_cycle ); cycle < 30; ++cycle )
    {
        const Vector2D inertia_pos = ptype->inertiaPoint( p->pos(),p->vel(),wait_cycle );
        const double target_dist = inertia_pos.dist( target_point );

        if ( target_dist < dist_thr )
        {
            return cycle;
        }

        double dash_dist = target_dist - dist_thr * 0.5;

        if ( dash_dist > ptype->realSpeedMax() * ( cycle - wait_cycle ) )
        {
            continue;
        }

        AngleDeg target_angle = ( target_point - inertia_pos ).th();

        //
        // turn
        //

        int n_turn = predict_player_turn_cycle( ptype,
                                                p->body(),
                                                first_speed,
                                                target_dist,
                                                target_angle,
                                                dist_thr,
                                                false );
        if ( wait_cycle + n_turn >= cycle )
        {
            continue;
        }

        StaminaModel stamina_model = first_stamina_model;

        // if ( n_turn > 0 )
        // {
        //   stamina_model.simulateWaits( &ptype, n_turn );
        // }

        //
        // dash
        //

        int n_dash = ptype->cyclesToReachDistance( dash_dist );
        if ( wait_cycle + n_turn + n_dash > cycle )
        {
            continue;
        }

        double speed = first_speed * std::pow( ptype->playerDecay(), n_turn );
        double reach_dist = 0.0;

        n_dash = 0;
        while ( wait_cycle + n_turn + n_dash < cycle )
        {
            double dash_power = std::min( SP.maxDashPower(), stamina_model.stamina() );
            if ( save_recovery
                 && stamina_model.stamina() - dash_power < recover_dec_thr )
            {
                dash_power = std::max( 0.0, stamina_model.stamina() - recover_dec_thr );
                if ( dash_power < 1.0 )
                {
                    break;
                }
            }

            double accel = dash_power * ptype->dashPowerRate() * stamina_model.effort();
            speed += accel;
            if ( speed > ptype->playerSpeedMax() )
            {
                speed = ptype->playerSpeedMax();
            }

            reach_dist += speed;
            speed *= ptype->playerDecay();

            // stamina_model.simulateDash( &ptype, dash_power );

            ++n_dash;

            if ( reach_dist >= dash_dist )
            {
                break;
            }
        }

        if ( reach_dist >= dash_dist )
        {
            stamina = stamina_model;

            return wait_cycle + n_turn + n_dash;
        }
    }

    return 1000;
}

#endif
