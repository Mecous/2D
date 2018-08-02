// -*-c++-*-

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
#include <config.h>
#endif

#include "generator_center_forward_free_move.h"

#include "options.h"
#include "strategy.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/color/thermo_color_provider.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/timer.h>

#include <svmrank/svm_struct_api.h>

#include <sstream>

// #define DEBUG_PROFILE
// #define DEBUG_PRINT

using namespace rcsc;


/*-------------------------------------------------------------------*/
/*!

 */
namespace {

const int COUNT_THR = 10;

/*-------------------------------------------------------------------*/
/*!

 */
double
get_opponent_dist( const WorldModel & wm,
                   const Vector2D & pos )
{
    double min_dist2 = 40000.0;

    for ( PlayerObject::Cont::const_iterator it = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          it != end;
          ++it )
    {
        if ( (*it)->posCount() > COUNT_THR )
        {
            continue;
        }

        double d2 = (*it)->pos().dist2( pos );
        if ( d2 < min_dist2 )
        {
            min_dist2 = d2;
        }
    }

    return std::sqrt( min_dist2 );
}

/*-------------------------------------------------------------------*/
/*!

 */
const PlayerObject *
get_teammate_nearest_to( const WorldModel & wm,
                         const Vector2D & pos,
                         double * dist )
{
    const PlayerObject * p = static_cast< const PlayerObject * >( 0 );
    double min_dist2 = 40000.0;

    for ( PlayerObject::Cont::const_iterator it = wm.teammatesFromSelf().begin(),
              end = wm.teammatesFromSelf().end();
          it != end;
          ++it )
    {
        if ( (*it)->posCount() > COUNT_THR )
        {
            continue;
        }

        double d2 = (*it)->pos().dist2( pos );
        if ( d2 < min_dist2 )
        {
            p = *it;
            min_dist2 = d2;
        }
    }

    if ( p
         && dist )
    {
        *dist = std::sqrt( min_dist2 );
    }

    return p;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
get_player_congestion( const PlayerObject::Cont & players,
                       const Vector2D & pos )

{
    const double factor = 1.0 / ( 2.0 * std::pow( 2.5, 2 ) );
    double congestion = 0.0;

    for ( PlayerObject::Cont::const_iterator o = players.begin(), end = players.end();
          o != end;
          ++o )
    {
        if ( (*o)->goalie() ) continue;
        if ( (*o)->ghostCount() >= 3 ) continue;

        congestion += std::exp( - (*o)->pos().dist2( pos ) * factor );
    }

    return congestion;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
debug_paint_points( const std::vector< std::pair< double, Vector2D > > & points )
{
    if ( points.empty() )
    {
        return;
    }

    double min_score = +1000000.0; //+boost::numerical_limit< double >::max();
    double max_score = -1000000.0; //-boost::numerical_limit< double >::max();

    for ( std::vector< std::pair< double, Vector2D > >::const_iterator it = points.begin(), end = points.end();
          it != end;
          ++it )
    {
        if ( it->first < min_score ) min_score = it->first;
        if ( it->first > max_score ) max_score = it->first;
    }

    const double range = ( max_score - min_score );

    ThermoColorProvider color;
    int count = 1;
    for ( std::vector< std::pair< double, Vector2D > >::const_iterator it = points.begin(), end = points.end();
          it != end;
          ++it, ++count )
    {
        char msg[16]; snprintf( msg, 16, "%d:%.3f", count, it->first );
        RGBColor c = color.convertToColor( ( it->first - min_score ) / range );
        //dlog.addText( Logger::ROLE, "value_rate = %.3f", ( it->first - min_score ) / range );
        dlog.addRect( Logger::ROLE,
                      it->second.x - 0.1, it->second.y - 0.1, 0.2, 0.2,
                      c.name().c_str(), true );
        dlog.addMessage( Logger::ROLE,
                         it->second.x + 0.1, it->second.y + 0.1,
                         msg );
    }
}

}


/*-------------------------------------------------------------------*/
/*!

 */
GeneratorCenterForwardFreeMove::GeneratorCenterForwardFreeMove()
    : M_update_time( -1, 0 ),
      M_best_point( Vector2D::INVALIDATED )
{
    M_model.svm_model = NULL;

    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorCenterForwardFreeMove::~GeneratorCenterForwardFreeMove()
{
    if ( M_model.svm_model != NULL )
    {
        svmrank::free_struct_model( M_model );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
GeneratorCenterForwardFreeMove &
GeneratorCenterForwardFreeMove::instance()
{
    static GeneratorCenterForwardFreeMove s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
GeneratorCenterForwardFreeMove::init()
{
    const std::string model = Options::i().centerForwardFreeMoveModel();

    M_model = svmrank::read_struct_model( model.c_str(), &M_learn_param );
    if ( M_model.svm_model == NULL )
    {
        std::cerr << __FILE__ << " ***ERROR*** failed to read svmrank model file ["
                  << Options::i().centerForwardFreeMoveModel() << "]" << std::endl;
        return false;
    }

    if ( M_model.svm_model->kernel_parm.kernel_type == svmrank::LINEAR )
    {
        // Linear Kernel: compute weight vector
        svmrank::add_weight_vector_to_linear_model( M_model.svm_model );
        M_model.w = M_model.svm_model->lin_weights;
    }

    //std::cerr << "read model [" << model << "]" << std::endl;

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCenterForwardFreeMove::clear()
{
    M_best_point = Vector2D::INVALIDATED;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCenterForwardFreeMove::generate( const WorldModel & wm )
{
    if ( M_model.svm_model == NULL )
    {
        dlog.addText( Logger::ROLE,
                      __FILE__": (generate) NULL rank model" );
        return;
    }

    if ( M_update_time == wm.time() )
    {
        return;
    }
    M_update_time = wm.time();

    clear();

    if ( wm.self().isKickable()
         || wm.gameMode().isPenaltyKickMode()
         || wm.gameMode().isTheirSetPlay( wm.ourSide() ) )
    {
        // dlog.addText( Logger::ROLE,
        //               __FILE__": (generate) no free move situation" );
        return;
    }

    // check ball owner

    const int self_step = wm.interceptTable()->selfReachCycle();
    const int teammate_step = wm.interceptTable()->teammateReachCycle();
    const int opponent_step = wm.interceptTable()->opponentReachCycle();

    if ( ! wm.kickableTeammate()
         && self_step <= teammate_step )
    {
        // dlog.addText( Logger::ROLE,
        //               __FILE__": (generate) my ball" );
        return;
    }

    if ( opponent_step < teammate_step - 2 )
    {
        // dlog.addText( Logger::ROLE,
        //               __FILE__": (generate) opponent ball" );
        return;
    }

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    M_best_point = generateBestPoint( wm );

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::ROLE,
                  __FILE__": (generate) elapsed %.3f [ms]",
                  timer.elapsedReal() );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
GeneratorCenterForwardFreeMove::generateBestPoint( const WorldModel & wm )
{
    const Vector2D home_pos = Strategy::i().getPosition( wm.self().unum() );
    const int teammate_step = wm.interceptTable()->teammateReachStep();
    const Vector2D ball_pos = wm.ball().inertiaPoint( teammate_step );

#if 1
    const double x_step = 1.0;
    const double y_step = 1.0;
    const int x_range = 5;
    const int y_range = 7;
#else
    const double x_step = 3.0;
    const double y_step = 3.0;
    const int x_range = 1;
    const int y_range = 2;
#endif

    const double max_x = std::max( ball_pos.x, wm.offsideLineX() );
    const double max_y = ServerParam::i().penaltyAreaHalfWidth() - 1.0;
    const double ball_dist_thr2 = std::pow( 2.0, 2 );

    double best_value = -100000000.0;
    Vector2D best_point = Vector2D::INVALIDATED;

#ifdef DEBUG_PRINT
    std::vector< std::pair< double, rcsc::Vector2D > > points;
    points.reserve( 11 * 15 );
#endif

    int count = 0;
    for ( int ix = -x_range; ix <= x_range; ++ix )
    {
        for ( int iy = -y_range; iy <= y_range; ++iy )
        {
            const Vector2D pos( home_pos.x + x_step*ix,
                                home_pos.y + y_step*iy );
            if ( pos.x > max_x
                 || pos.absY() > max_y
                 || pos.dist2( ball_pos ) < ball_dist_thr2 )
            {
                // dlog.addText( Logger::ROLE,
                //               "# (%.1f %.1f) skip",
                //               pos.x, pos.y );
                continue;
            }

            ++count;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ROLE,
                          "#%d (%.1f,%.1f)",
                          count, pos.x, pos.y );
#endif
            double value = evaluatePoint( wm, pos, home_pos, ball_pos );
            if ( value > best_value )
            {
                best_value = value;
                best_point = pos;
                // dlog.addText( Logger::ROLE,
                //               ">>> update best point" );
            }
#ifdef DEBUG_PRINT
            points.push_back( std::pair< double, Vector2D >( value, pos ) );
#endif
        }
    }

#ifdef DEBUG_PRINT
    dlog.addText( Logger::ROLE,
                  __FILE__": (generate) %zd points",
                  points.size() );
    debug_paint_points( points );
#endif

    return best_point;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
GeneratorCenterForwardFreeMove::evaluatePoint( const WorldModel & wm,
                                               const Vector2D & pos,
                                               const Vector2D & home_pos,
                                               const Vector2D & ball_pos )
{
    svmrank::WORD words[32];
    int32_t fcount = 0;

    // 1  pos_x;
    // 2  pos_absy_;
    // 3  opponent_dist_; // nearest opponent distance, exclude goalie
    // 4  opponents_in_front_space_; // nr of oppoinents in the front space, exclude goalie
    // 5  opponent_congestion_;
    // 6  teammate_dist_;
    // 7  teammate_ydiff_abs_;
    // 8  teammate_congestion_;
    // 9  home_pos_dist_;
    // 10 ball_x_;
    // 11 ball_absy_;
    // 12 ball_dist_;
    // 13 ball_ydiff_abs_;
    // 14 ball_dir_;
    // 15 offside_line_x_;
    // 16 defense_line_x_;
    // 17 move_dist_;
    // 18 move_dir_;
    // 19 move_dir_from_self_body_;
    // 20 opponents_on_pass_course_;

    // 1  pos_x;
    words[fcount].wnum = fcount + 1;
    words[fcount].weight = pos.x;

    // 2  pos_absy_;
    ++fcount;
    words[fcount].wnum = fcount + 1;
    words[fcount].weight = pos.absY();

    // 3  opponent_dist_; // nearest opponent distance, exclude goalie
    ++fcount;
    words[fcount].wnum = fcount + 1;
    words[fcount].weight = get_opponent_dist( wm, pos );

    // 4  opponents_in_front_space_; // nr of oppoinents in the front space, exclude goalie
    {
        const Sector2D front_space_sector( Vector2D( pos.x - 5.0, pos.y ),
                                           4.0, 20.0, -15.0, 15.0 );
        ++fcount;
        words[fcount].wnum = fcount + 1;
        words[fcount].weight = wm.countOpponentsIn( front_space_sector, COUNT_THR, false );
    }

    // 5  opponent_congestion_;
    ++fcount;
    words[fcount].wnum = fcount + 1;
    words[fcount].weight = get_player_congestion( wm.opponentsFromSelf(), pos );

    // 6  teammate_dist_;
    // 7  teammate_ydiff_abs_;
    {
        double d = 200.0;
        const PlayerObject * t = get_teammate_nearest_to( wm, pos, &d );
        if ( t )
        {
            ++fcount;
            words[fcount].wnum = fcount;
            words[fcount].weight = d;

            ++fcount;
            words[fcount].wnum = fcount;
            words[fcount].weight = std::fabs( t->pos().y - pos.y );
        }
        else
        {
            ++fcount;
            words[fcount].wnum = fcount;
            words[fcount].weight = 200.0;

            ++fcount;
            words[fcount].wnum = fcount;
            words[fcount].weight = 200.0;
        }
    }

    // 8  teammate_congestion_;
    ++fcount;
    words[fcount].wnum = fcount;
    words[fcount].weight = get_player_congestion( wm.teammatesFromSelf(), pos );

    // 9  home_pos_dist_;
    ++fcount;
    words[fcount].wnum = fcount;
    words[fcount].weight = home_pos.dist( pos );

    // 10 ball_x_;
    ++fcount;
    words[fcount].wnum = fcount;
    words[fcount].weight = ball_pos.x;

    // 11 ball_absy_;
    ++fcount;
    words[fcount].wnum = fcount;
    words[fcount].weight = ball_pos.absY();

    // 12 ball_dist_;
    ++fcount;
    words[fcount].wnum = fcount;
    words[fcount].weight = ball_pos.dist( pos );

    // 13 ball_ydiff_abs_;
    ++fcount;
    words[fcount].wnum = fcount;
    words[fcount].weight = std::fabs( ball_pos.y - pos.y );

    // 14 ball_dir_;
    ++fcount;
    words[fcount].wnum = fcount;
    words[fcount].weight = ( ball_pos - pos ).th().abs();

    // 15 offside_line_x_;
    ++fcount;
    words[fcount].wnum = fcount;
    words[fcount].weight = wm.offsideLineX();

    // 16 defense_line_x_;
    ++fcount;
    words[fcount].wnum = fcount;
    words[fcount].weight = wm.theirDefenseLineX();

    // 17 move_dist_;
    ++fcount;
    words[fcount].wnum = fcount;
    words[fcount].weight = wm.self().pos().dist( pos );

    // 18 move_dir_;
    // 19 move_dir_from_self_body_;
    {
        const Vector2D self_pos = wm.self().inertiaFinalPoint();
        const AngleDeg move_dir = ( pos - self_pos ).th();

        ++fcount;
        words[fcount].wnum = fcount;
        words[fcount].weight = move_dir.abs();

        ++fcount;
        words[fcount].wnum = fcount;
        words[fcount].weight = ( move_dir - wm.self().body() ).abs();
    }

    // 20 opponents_on_pass_course_;
    {
        const AngleDeg ball_move_angle = ( pos - ball_pos ).th();
        const Sector2D pass_cone( ball_pos,
                                  1.0, ball_pos.dist( pos ) + 3.0, // radius
                                  ball_move_angle - 10.0, ball_move_angle + 10.0 );

        ++fcount;
        words[fcount].wnum = fcount + 1;
        words[fcount].weight = wm.countOpponentsIn( pass_cone, COUNT_THR, true );
    }

    //
    //
    //
    ++fcount;
    words[fcount].wnum = 0;
    words[fcount].weight = 0;

    svmrank::DOC doc;

    doc.docnum = 1;
    doc.queryid = 1;
    doc.costfactor = 1.0;
    doc.slackid = 0;
    doc.kernelid = -1;
    doc.fvec = svmrank::create_svector( words, NULL, 1.0 );

    if ( ! doc.fvec )
    {
        return 0.0;
    }

    // dlog.addText( Logger::ROLE,
    //               "CenterForwardFreeMove start evaluation. features=%d",
    //               fcount );
    // dlog.flush();

    double value = svmrank::classify_example( M_model.svm_model, &doc );

    svmrank::free_svector( doc.fvec );

#ifdef DEBUG_PRINT
    writeRankData( wm, value, words, sizeof( words ) );
#endif
    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
GeneratorCenterForwardFreeMove::writeRankData( const WorldModel & wm,
                                               const double value,
                                               const svmrank::WORD * words,
                                               const int n_words )
{
    static std::string s_query_id = "";
    static GameTime s_time( 0, 0 );

    if ( wm.time().cycle() >= 10000
         || wm.time().stopped() > 0 )
    {
        return;
    }

    if ( wm.time() != s_time )
    {
        s_time = wm.time();

        // qid: hour + minutes + unum + cycle
        //  example 2015 07 08 1410 02 0200

        char time_str[64];

        time_t t = std::time( 0 );
        tm * tm = std::localtime( &t );
        if ( tm )
        {
            std::strftime( time_str, sizeof( time_str ), "%Y%m%d%H%M", tm );
        }

        char unum_cycle[16];
        snprintf( unum_cycle, sizeof( unum_cycle ),
                  "%02d%04ld", wm.self().unum(), wm.time().cycle() );

        s_query_id = time_str;
        s_query_id += unum_cycle;

        // dlog.addText( Logger::ROLE,
        //               "# query %s CenterForwardFreeMove",
        //               s_query_id.c_str() );
    }

    std::ostringstream ostr;
    ostr << value << " qid:" << s_query_id;

    for ( int i = 0; i < n_words; ++i )
    {
        if ( words[i].wnum == 0 ) break;

        ostr << ' ' << i + 1 << ':' << words[i].weight;
    }

    dlog.addText( Logger::ROLE,
                  "%s", ostr.str().c_str() );

}
