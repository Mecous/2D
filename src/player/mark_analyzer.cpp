// -*-c++-*-

/*!
  \file mark_analyzer.cpp
  \brief mark target analyzer Source File
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
#include <config.h>
#endif

#include "mark_analyzer.h"

#include "strategy.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/player/player_predicate.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/world_model.h>
#include <rcsc/common/logger.h>
#include <rcsc/timer.h>

#include <algorithm>
#include <iostream>
#include <sstream>

using namespace rcsc;

// #define DEBUG_PROFILE
// #define DEBUG_PROFILE_LEVEL2
// #define DEBUG_PRINT_COACH

// #define DEBUG_PRINT_TARGET_CANDIDATES
// #define DEBUG_PRINT_LEVEL_2
// #define DEBUG_PRINT_LEVEL_3
// #define DEBUG_PRINT_COMBINATION
// #define DEBUG_EVAL

// #define DEBUG_PAINT_TARGET
// #define DEBUG_PRINT_RESULT

#define DEBUG_PAINT_RESULT

#define USE_X_SHIFT

namespace {

const double g_home_position_rate = 0.6;

/*-------------------------------------------------------------------*/
/*!
  \brief compare the distance from our goal and ball
*/
struct TargetSorter {
private:
    const Vector2D M_pos;
public:
    TargetSorter( const Vector2D & pos )
        : M_pos( pos )
      { }

    bool operator()( const MarkAnalyzer::Target * lhs,
                     const MarkAnalyzer::Target * rhs ) const
      {
          return lhs->player_->pos().dist2( M_pos )
              <  rhs->player_->pos().dist2( M_pos );
      }
};

/*-------------------------------------------------------------------*/
/*!
  \brief compare the distance from our goal (with scaled x)
*/
struct TargetGoalDistSorter {
private:
    static const double goal_x;
    static const double x_scale;
public:
    bool operator()( const MarkAnalyzer::Target * lhs,
                     const MarkAnalyzer::Target * rhs ) const
      {
          return ( std::pow( lhs->pos().x - goal_x, 2 ) * x_scale
                   + std::pow( lhs->pos().y, 2 ) )
              < ( std::pow( rhs->pos().x - goal_x, 2 ) * x_scale
                  + std::pow( rhs->pos().y, 2 ) );
      }
};

const double TargetGoalDistSorter::goal_x = -52.0;
const double TargetGoalDistSorter::x_scale = 1.2 * 1.2;

/*-------------------------------------------------------------------*/
/*!

 */
struct TargetMarkerDistSorter {
private:
    const MarkAnalyzer::Marker & marker_;
public:
    TargetMarkerDistSorter( const MarkAnalyzer::Marker & m )
        : marker_( m )
      { }

    bool operator()( const MarkAnalyzer::Target * lhs,
                     const MarkAnalyzer::Target * rhs ) const
      {
          return ( lhs->pos().dist2( marker_.pos() )
                   < rhs->pos().dist2( marker_.pos() ) );
      }
};

/*-------------------------------------------------------------------*/
/*!

 */
struct OpponentMarkerDistSorter {
private:
    const MarkAnalyzer::Marker & marker_;
public:
    OpponentMarkerDistSorter( const MarkAnalyzer::Marker & m )
        : marker_( m )
      { }

    bool operator()( const AbstractPlayerObject * lhs,
                     const AbstractPlayerObject * rhs ) const
      {
          return ( lhs->pos().dist2( marker_.pos() )
                   < rhs->pos().dist2( marker_.pos() ) );
      }
};

/*-------------------------------------------------------------------*/
/*!
  \brief compare the distance from our goal (with scaled x)
*/
struct MarkerGoalDistSorter {
private:
    static const double goal_x;
    static const double x_scale;
public:
    bool operator()( const MarkAnalyzer::Marker & lhs,
                     const MarkAnalyzer::Marker & rhs ) const
      {
          return ( std::pow( lhs.player_->pos().x - goal_x, 2.0 ) * x_scale
                   + std::pow( lhs.player_->pos().y, 2.0 ) )
              < ( std::pow( rhs.player_->pos().x - goal_x, 2.0 ) * x_scale
                  + std::pow( rhs.player_->pos().y, 2.0 ) );
      }
};

const double MarkerGoalDistSorter::goal_x = -47.0;
const double MarkerGoalDistSorter::x_scale = 1.5 * 1.5;

/*-------------------------------------------------------------------*/
/*!

 */
struct MarkerEqual {
    const AbstractPlayerObject * marker_;

    MarkerEqual( const AbstractPlayerObject * p )
    : marker_( p )
      { }

    bool operator()( const MarkAnalyzer::Marker * val ) const
      {
          return val->player_ == marker_;
      }
};

/*-------------------------------------------------------------------*/
/*!

 */
struct CombinationSorter {

    bool operator()( const MarkAnalyzer::Combination & lhs,
                     const MarkAnalyzer::Combination & rhs ) const
      {
          return lhs.score_ < rhs.score_;
      }
};

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
get_base_point( const WorldModel & wm )
{
    const int min_step = std::min( wm.interceptTable()->selfReachCycle(),
                                   std::min( wm.interceptTable()->teammateReachCycle(),
                                             wm.interceptTable()->opponentReachCycle() ) );
    const Vector2D ball_pos = wm.ball().inertiaPoint( min_step );
    return ( ServerParam::i().ourTeamGoalPos() * 0.6
             + ball_pos * 0.4 );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
debug_print_targets( const std::vector< MarkAnalyzer::Target * > & targets )
{
    dlog.addText( Logger::MARK,
                  "marker candidates:" );

    for ( std::vector< MarkAnalyzer::Target * >::const_iterator o = targets.begin(),
              end = targets.end();
          o != end;
          ++o )
    {
        if ( (*o)->markers_.empty() )
        {
            dlog.addText( Logger::MARK,
                          "_ opp %d (%.1f %.1f) : no marker",
                          (*o)->player_->unum(),
                          (*o)->player_->pos().x, (*o)->player_->pos().y );
            continue;
        }

        for ( std::vector< MarkAnalyzer::Marker >::const_iterator m = (*o)->markers_.begin();
              m != (*o)->markers_.end();
              ++m )
        {
            dlog.addText( Logger::MARK,
                          "_ opp %d (%.1f %.1f) : marker=%d (%.1f %.1f)",
                          (*o)->player_->unum(),
                          (*o)->player_->pos().x, (*o)->player_->pos().y,
                          m->player_->unum(),
                          m->player_->pos().x, m->player_->pos().y );
        }
    }
}

}

/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/


/*-------------------------------------------------------------------*/
/*!

 */
MarkAnalyzer::MarkAnalyzer()
    : M_strategic_marker_count( 0 )
{
    for ( int i = 0; i < 11; ++i )
    {
        M_last_assignment[i] = Unum_Unknown;

        for ( int j = 0; j < 11; ++j )
        {
            M_matching_count[i][j] = 0;
        }

        M_coach_assignment[i][0] = Unum_Unknown;
        M_coach_assignment[i][1] = Unum_Unknown;
        M_coach_assignment_target[i][0] = Unum_Unknown;
        M_coach_assignment_target[i][1] = Unum_Unknown;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
MarkAnalyzer &
MarkAnalyzer::instance()
{
    static MarkAnalyzer s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
const AbstractPlayerObject *
MarkAnalyzer::getTargetOf( const int marker_unum ) const
{
    for ( std::vector< Pair >::const_iterator it = M_pairs.begin(),
              end = M_pairs.end();
          it != end;
          ++it )
    {
        if ( it->marker_->unum() == marker_unum )
        {
            return it->target_;
        }
    }

    return static_cast< AbstractPlayerObject * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
const
AbstractPlayerObject *
MarkAnalyzer::getTargetOf( const AbstractPlayerObject * marker ) const
{
    for ( std::vector< Pair >::const_iterator it = M_pairs.begin(),
              end = M_pairs.end();
          it != end;
          ++it )
    {
        if ( it->marker_ == marker )
        {
            return it->target_;
        }
    }

    return static_cast< AbstractPlayerObject * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
const
AbstractPlayerObject *
MarkAnalyzer::getMarkerOf( const int target_unum ) const
{
    for ( std::vector< Pair >::const_iterator it = M_pairs.begin(),
              end = M_pairs.end();
          it != end;
          ++it )
    {
        if ( it->target_->unum() == target_unum )
        {
            return it->marker_;
        }
    }

    return static_cast< AbstractPlayerObject * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
const
AbstractPlayerObject *
MarkAnalyzer::getMarkerOf( const AbstractPlayerObject * target ) const
{
    for ( std::vector< Pair >::const_iterator it = M_pairs.begin(),
              end = M_pairs.end();
          it != end;
          ++it )
    {
        if ( it->target_ == target )
        {
            return it->marker_;
        }
    }

    return static_cast< AbstractPlayerObject * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
const MarkAnalyzer::Pair *
MarkAnalyzer::getPairOfMarker( const int unum ) const
{
    for ( std::vector< Pair >::const_iterator it = M_pairs.begin(),
              end = M_pairs.end();
          it != end;
          ++it )
    {
        if ( it->marker_->unum() == unum )
        {
            return &(*it);
        }
    }

    return static_cast< const Pair * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
const MarkAnalyzer::Pair *
MarkAnalyzer::getPairOfTarget( const int unum ) const
{
    for ( std::vector< Pair >::const_iterator it = M_pairs.begin(),
              end = M_pairs.end();
          it != end;
          ++it )
    {
        if ( it->target_->unum() == unum )
        {
            return &(*it);
        }
    }

    return static_cast< const Pair * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
const MarkAnalyzer::UnumPair *
MarkAnalyzer::getUnumPairOfMarker( const int unum ) const
{
    for ( std::vector< UnumPair >::const_iterator it = M_unum_pairs.begin(),
              end = M_unum_pairs.end();
          it != end;
          ++it )
    {
        if ( it->marker_ == unum )
        {
            return &(*it);
        }
    }

    return static_cast< const UnumPair * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
const MarkAnalyzer::UnumPair *
MarkAnalyzer::getUnumPairOfTarget( const int unum ) const
{
    for ( std::vector< UnumPair >::const_iterator it = M_unum_pairs.begin(),
              end = M_unum_pairs.end();
          it != end;
          ++it )
    {
        if ( it->target_ == unum )
        {
            return &(*it);
        }
    }

    return static_cast< const UnumPair * >( 0 );
}

/*-------------------------------------------------------------------*/
/*!

 */
int
MarkAnalyzer::getFirstTargetByCoach( const int our_unum ) const
{
    if ( our_unum < 1 || 11 < our_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (getFirstTargetByCoach) illegal unum " << our_unum
                  << std::endl;
        return Unum_Unknown;
    }

    return M_coach_assignment[our_unum - 1][0];
}

/*-------------------------------------------------------------------*/
/*!

 */
int
MarkAnalyzer::getSecondTargetByCoach( const int our_unum ) const
{
    if ( our_unum < 1 || 11 < our_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (getFirstTargetByCoach) illegal unum " << our_unum
                  << std::endl;
        return Unum_Unknown;
    }

    return M_coach_assignment[our_unum - 1][1];
}

/*-------------------------------------------------------------------*/
/*!

 */
int
MarkAnalyzer::getMarkerToFirstTargetByCoach( const int their_unum ) const
{
    if ( their_unum < 1 || 11 < their_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (getFirstTargetByCoach) illegal unum " << their_unum
                  << std::endl;
        return Unum_Unknown;
    }

    return M_coach_assignment_target[their_unum - 1][0];
}

/*-------------------------------------------------------------------*/
/*!

 */
int

MarkAnalyzer::getMarkerToSecondTargetByCoach( const int their_unum ) const
{
    if ( their_unum < 1 || 11 < their_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (getFirstTargetByCoach) illegal unum " << their_unum
                  << std::endl;
        return Unum_Unknown;
    }

    return M_coach_assignment_target[their_unum - 1][1];
}

/*-------------------------------------------------------------------*/
/*!

 */
int
MarkAnalyzer::isFirstMarkerByCoach( const int their_unum,
                                    const int our_unum ) const
{
    if ( their_unum < 1 || 11 < their_unum
         || our_unum < 1 || 11 < our_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (getFirstTargetByCoach) illegal unum " << their_unum
                  << ' ' << our_unum << std::endl;
        return Unum_Unknown;
    }

    return M_coach_assignment[our_unum - 1][0] == their_unum;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
MarkAnalyzer::isSecondMarkerByCoach( const int their_unum,
                                     const int our_unum ) const
{
    if ( their_unum < 1 || 11 < their_unum
         || our_unum < 1 || 11 < our_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (getFirstTargetByCoach) illegal unum " << their_unum
                  << ' ' << our_unum << std::endl;
        return Unum_Unknown;
    }

    return M_coach_assignment[our_unum - 1][1] == their_unum;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
MarkAnalyzer::setAssignmentByCoach( const int our_unum,
                                    const int first_target_unum,
                                    const int second_target_unum )
{
    if ( our_unum < 1 || 11 < our_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (getFirstTargetByCoach) illegal unum " << our_unum
                  << std::endl;
        return false;
    }

    if ( first_target_unum != Unum_Unknown
         && ( first_target_unum < 1 || 11 < first_target_unum ) )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (getFirstTargetByCoach) illegal first target "
                  << first_target_unum << std::endl;
        return false;
    }

    if ( second_target_unum != Unum_Unknown
         && ( second_target_unum < 1 || 11 < second_target_unum ) )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (getFirstTargetByCoach) illegal second target "
                  << second_target_unum << std::endl;
        return false;
    }

    bool changed = false;

    if ( M_coach_assignment[our_unum - 1][0] != first_target_unum )
    {
        M_coach_assignment_target[first_target_unum -1][0] = our_unum;
        M_coach_assignment[our_unum - 1][0] = first_target_unum;
        changed = true;
    }

    if ( M_coach_assignment[our_unum - 1][1] != second_target_unum )
    {
        M_coach_assignment_target[second_target_unum -1][1] = our_unum;
        M_coach_assignment[our_unum - 1][1] = second_target_unum;
        changed = true;
    }

    return changed;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
MarkAnalyzer::update( const WorldModel & wm )
{
    static GameTime s_update_time( 0, 0 );

    static std::vector< Combination > s_combinations;

    if ( s_update_time == wm.time() )
    {
        return;
    }

    s_update_time = wm.time();

    //
    // clear old variables
    //
    M_pairs.clear();
    M_unum_pairs.clear();

    if ( wm.gameMode().type() == GameMode::BeforeKickOff
         || wm.gameMode().type() == GameMode::AfterGoal_
         || wm.gameMode().isPenaltyKickMode() )
    {
        return;
    }

#ifdef DEBUG_PROFILE
    MSecTimer timer;
#endif

    //
    // create target opponent set
    //

    std::vector< Target * > target_opponents;

    if ( ! createMarkTargets( wm, target_opponents ) )
    {
        return;
    }

#ifdef DEBUG_PROFILE_LEVEL2
    dlog.addText( Logger::MARK,
                  __FILE__":(update) create target elapsed %.3f [ms]",
                  timer.elapsedReal() );
#endif

    //
    // create combinations
    //
    createCombination( target_opponents, s_combinations );

    //
    // evaluate all combinations
    //
    evaluate( wm, s_combinations );

    //
    // update unum pairs
    //
    updateUnumPairs( wm, s_combinations );

    //
    // clear variables
    //
    for ( std::vector< Target * >::iterator it = target_opponents.begin(),
              end = target_opponents.end();
          it != end;
          ++it )
    {
        delete *it;
    }

    target_opponents.clear();
    s_combinations.clear();

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::MARK,
                  __FILE__":(upate) elapsed %.3f [ms]",
                  timer.elapsedReal() );
#endif

#ifdef DEBUG_PRINT_RESULT
    dlog.addText( Logger::MARK,
                  __FILE__": matching count:" );
    for ( int i = 0; i < 11; ++i )
    {
        dlog.addText( Logger::MARK,
                      "%d :  1=%d 2=%d 3=%d 4=%d 5=%d 6=%d 7=%d 8=%d 9=%d 10=%d 11=%d",
                      i+1,
                      M_matching_count[i][0], M_matching_count[i][1], M_matching_count[i][2],
                      M_matching_count[i][3], M_matching_count[i][4], M_matching_count[i][5],
                      M_matching_count[i][6], M_matching_count[i][7], M_matching_count[i][8],
                      M_matching_count[i][9], M_matching_count[i][10] );
    }
#endif

#ifdef DEBUG_PRINT_COACH
    dlog.addText( Logger::MARK,
                  __FILE__": coach assignment:" );
    for ( int i = 0; i < 11; ++i )
    {
        dlog.addText( Logger::MARK,
                      "__ %d 1st=%d 2nd=%d",
                      i + 1,
                      M_coach_assignment[i][0], M_coach_assignment[i][1] );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
MarkAnalyzer::createMarkTargets( const WorldModel & wm,
                                 std::vector< Target * > & target_opponents )
{
    //
    // create marker candidates
    //
    std::vector< Marker > markers;
    if ( ! createMarkerCandidates( wm, markers ) )
    {
        return false;
    }

    //
    // create mark target candidates
    //
    if ( ! createTargetCandidates( wm, target_opponents ) )
    {
        return false;
    }

    dlog.addText( Logger::MARK,
                  __FILE__":(createMarkTargets) marker=%d target=%d",
                  markers.size(), target_opponents.size() );

    //
    // create target candidates for each marker
    //

    setMarkerToTarget( markers, target_opponents );

    //
    // remove targets that have no marker candidate.
    //
    for ( std::vector< Target * >::iterator it = target_opponents.begin(),
              end = target_opponents.end();
          it != end;
          ++it )
    {
        if ( (*it)->markers_.empty() )
        {
            delete *it;
            *it = static_cast< Target * >( 0 );
        }
    }

    target_opponents.erase( std::remove( target_opponents.begin(),
                                         target_opponents.end(),
                                         static_cast< Target * >( 0 ) ),
                            target_opponents.end() );

    {
        const Vector2D base_point = get_base_point( wm );
        std::sort( target_opponents.begin(),
                   target_opponents.end(),
                   TargetSorter( base_point ) ); //TargetGoalDistSorter() );
    }

#ifdef DEBUG_PRINT_TARGET_CANDIDATES
    debug_print_targets( target_opponents );
#endif
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
MarkAnalyzer::createMarkerCandidates( const WorldModel & wm,
                                      std::vector< Marker > & markers )
{
    const Strategy & st = Strategy::i();
    const bool playon = ( wm.gameMode().type() == GameMode::PlayOn );

    bool is_marker[11];

    markers.reserve( wm.ourPlayers().size() );

    M_strategic_marker_count = 0;
    for ( int unum = 1; unum <= 11; ++unum )
    {
        is_marker[unum-1] = false;

        if ( wm.ourCard( unum ) == RED )
        {
            continue;
        }

        if ( playon )
        {
            if ( st.isMarkerType( unum ) )
            {
                ++M_strategic_marker_count;
                is_marker[unum-1] = true;
            }
        }
        else
        {
            if ( st.isSetPlayMarkerType( unum ) )
            {
                ++M_strategic_marker_count;
                is_marker[unum-1] = true;
            }
        }
    }


    double x_shift = 0.0;
#ifdef USE_X_SHIFT
    //
    // use x-shift
    //
    {
        double our_defense_line_x = 0.0;
        for ( AbstractPlayerObject::Cont::const_iterator t = wm.ourPlayers().begin(),
                  end = wm.ourPlayers().end();
              t != end;
              ++t )
        {
            if ( (*t)->goalie() ) continue;

            Vector2D pos = ( (*t)->unum() != Unum_Unknown
                             ? Strategy::i().getPosition( (*t)->unum() )
                             : (*t)->pos() );
            pos *= g_home_position_rate;
            pos += (*t)->pos() * ( 1.0 - g_home_position_rate );

            if ( pos.x < our_defense_line_x )
            {
                our_defense_line_x = pos.x;
            }
        }

        double their_offense_line_x = 0.0;
        for ( AbstractPlayerObject::Cont::const_iterator o = wm.theirPlayers().begin(),
                  end = wm.theirPlayers().end();
              o != end;
              ++o )
        {
            if ( (*o)->goalie() ) continue;
            if ( (*o)->pos().x < their_offense_line_x )
            {
                their_offense_line_x = (*o)->pos().x;
            }
        }

        x_shift = their_offense_line_x - our_defense_line_x;

        dlog.addText( Logger::MARK,
                      __FILE__":(createMarkerCandidates) our_defense_line = %.2f", our_defense_line_x );
        dlog.addText( Logger::MARK,
                      __FILE__":(createMarkerCandidates) their_offense_line = %.2f", their_offense_line_x );
        dlog.addText( Logger::MARK,
                      __FILE__":(createMarkerCandidates) x_shift = %.2f", x_shift );
    }
#endif

    for ( AbstractPlayerObject::Cont::const_iterator t = wm.ourPlayers().begin(),
              end = wm.ourPlayers().end();
          t != end;
          ++t )
    {
        if ( (*t)->goalie() ) continue;
        if ( 1 <= (*t)->unum() && (*t)->unum() <= 11
             && ! is_marker[ (*t)->unum() - 1 ] )
        {
            continue;
        }

        Vector2D home_pos = ( (*t)->unum() != Unum_Unknown
                              ? st.getPosition( (*t)->unum() )
                              : (*t)->pos() );
        Vector2D pos = home_pos * g_home_position_rate
            + (*t)->pos() * ( 1.0 - g_home_position_rate );
        pos.x += x_shift;

        markers.push_back( Marker( *t, pos ) );
    }


    if ( markers.empty() )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(createMarkers) no marker player" );
        return false;
    }

    // sort by distance from our goal
    std::sort( markers.begin(), markers.end(), MarkerGoalDistSorter() );

#ifdef DEBUG_PAINT_RESULT
    for ( std::vector< Marker >::const_iterator m = markers.begin(), end = markers.end();
          m != end;
          ++m )
    {
        char msg[8];
        snprintf( msg, 8, "%d", m->player_->unum() );
        dlog.addRect( Logger::MARK,
                      m->pos().x - 0.3, m->pos().y - 0.3, 0.6, 0.6,
                      "#F00", true );
        dlog.addMessage( Logger::MARK,
                         m->pos(), msg, "#00F" );
        // dlog.addText( Logger::MARK,
        //               "marker %d (%.1f %.1f)",
        //               m->player_->unum(),
        //               m->pos().x, m->pos().y );
    }
#endif
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
MarkAnalyzer::createTargetCandidates( const WorldModel & wm,
                                      std::vector< Target * > & target_opponents )
{
#ifdef DEBUG_PRINT_TARGET_CANDIDATES
    dlog.addText( Logger::MARK,
                  __FILE__":(createTargetCandidates)" );
#endif

    target_opponents.reserve( 10 );

    for ( AbstractPlayerObject::Cont::const_iterator o = wm.theirPlayers().begin(),
              end = wm.theirPlayers().end();
          o != end;
          ++o )
    {
        target_opponents.push_back( new Target( *o ) );
    }

    if ( target_opponents.empty() )
    {
        dlog.addText( Logger::MARK,
                      __FILE__":(createTargetCandidates) no target" );
        return false;
    }

    //
    // sort by distance from the weighted point between our goal and ball pos.
    //
    {
        const Vector2D base_point = get_base_point( wm );
        std::sort( target_opponents.begin(),
                   target_opponents.end(),
                   TargetSorter( base_point ) ); //TargetGoalDistSorter() );
    }

    if ( target_opponents.size() > M_strategic_marker_count )
    {
        std::vector< Target * >::iterator it = target_opponents.begin();
        it += M_strategic_marker_count;
        while ( it != target_opponents.end() )
        {
            delete *it;
            *it = static_cast< Target * >( 0 );
            ++it;
        }
        target_opponents.erase( target_opponents.begin() + M_strategic_marker_count,
                                target_opponents.end() );
    }

#if 0
    // 2013-05-22
    for ( std::vector< Target * >::iterator it = target_opponents.begin();
          it != target_opponents.end();
          ++it )
    {
        //if ( (*it)->player_->pos().x > -10.0 ) // magic number
        if ( (*it)->player_->pos().x > 20.0
             || (*it)->player_->pos().x > wm.ball().pos().x + 30.0
             || (*it)->player_->pos().dist2( wm.ball().pos() ) > std::pow( 50.0, 2 ) )
        {
            delete *it;
            *it = static_cast< Target * >( 0 );
        }
    }
    target_opponents.erase( std::remove( target_opponents.begin(),
                                         target_opponents.end(),
                                         static_cast< Target * >( 0 ) ),
                            target_opponents.end() );

#endif

#ifdef DEBUG_PRINT_TARGET_CANDIDATES
    for ( std::vector< Target * >::const_iterator it = target_opponents.begin(),
              end = target_opponents.end();
          it != end;
          ++it )
    {
        dlog.addText( Logger::MARK,
                      "(MarkAnalyzer) target candidate %d (%.1f %.1f)",
                      (*it)->player_->unum(),
                      (*it)->player_->pos().x, (*it)->player_->pos().y );
    }
#endif
#ifdef DEBUG_PRINT_TARGET_CANDIDATES
    for ( std::vector< Target * >::const_iterator it = target_opponents.begin(),
              end = target_opponents.end();
          it != end;
          ++it )
    {
        char buf[8]; snprintf( buf, 8, "%d", (*it)->player_->unum() );
        dlog.addMessage( Logger::MARK,
                         (*it)->pos().x + 0.1, (*it)->pos().y + 0.1, buf );
        dlog.addRect( Logger::MARK,
                      (*it)->pos().x - 0.1, (*it)->pos().y - 0.1, 0.2, 0.2, "#0FF", true );
    }
#endif

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
MarkAnalyzer::setMarkerToTarget( const std::vector< Marker > & markers,
                                 std::vector< Target * > & target_opponents )
{
    const double dist_thr2 = std::pow( 30.0, 2 );
    const double x_thr = 30.0;
    const double y_thr = 20.0;
    const size_t max_partial_size = 5;

    const size_t partial_size = std::min( max_partial_size, target_opponents.size() );

    for ( std::vector< Marker >::const_iterator m = markers.begin(),
              m_end = markers.end();
          m != m_end;
          ++m )
    {
        std::partial_sort( target_opponents.begin(),
                           target_opponents.begin() + partial_size,
                           target_opponents.end(),
                           TargetMarkerDistSorter( *m ) );
#ifdef DEBUG_PRINT_LEVEL_2
        dlog.addText( Logger::MARK,
                      "__  marker %d real_pos=(%.1f %.1f) pos=(%.2f %.2f)",
                      m->player_->unum(),
                      m->player_->pos().x, m->player_->pos().y,
                      m->pos_.x, m->pos_.y );
#endif
        for ( size_t i = 0; i < partial_size; ++i )
        {
            Target * o = target_opponents[i];
#ifdef DEBUG_PRINT_LEVEL_3
            dlog.addText( Logger::MARK,
                          "____ target %d (%.1f %.1f)",
                          o->player_->unum(),
                          o->player_->pos().x,
                          o->player_->pos().y );
#endif
            if ( o->pos().x < m->pos().x + x_thr
                 && std::fabs( o->pos().y - m->pos().y ) < y_thr
                 && o->pos().dist2( m->pos() ) < dist_thr2 )
            {
#ifdef DEBUG_PRINT_LEVEL_3
                dlog.addText( Logger::MARK,
                              "______ candidate x_diff=%.3f y_diff=%.3f dist=%.3f",
                              o->pos().x - m->pos().x,
                              o->pos().y - m->pos().y,
                              std::sqrt( d2 ) );
#endif
                o->markers_.push_back( *m );
                o->markers_.back().target_ = o->player_;
            }
#ifdef DEBUG_PRINT_LEVEL_3
            else
            {
                dlog.addText( Logger::MARK,
                              "______ nocandidate x_diff=%.3f y_diff=%.3f dist=%.3f",
                              o->pos().x - m->pos().x,
                              o->pos().y - m->pos().y,
                              o->pos().dist( m->pos() ) );
            }
#endif
        }
    }

#if 0
    //
    // assign marker candidate enforcely.
    //
    for ( std::vector< Target * >::iterator o = target_opponents.begin(),
              o_end = target_opponents.end();
          o != o_end;
          ++o )
    {
        if ( ! (*o)->markers_.empty() ) continue;

        double min_dist2 = 1000000.0;
        std::vector< Marker >::const_iterator nearest_marker = markers.end(),
            for ( std::vector< Marker >::const_iterator m = markers.begin(),
                      m_end = markers.end();
                  m != m_end;
                  ++m )
            {
                double d2 = (*o)->pos().dist2( m->pos() );
                if ( d2 < min_dist2 )
                {
                    min_dist2 = d2;
                    nearest_marker = m;
                }
            }

        if ( nearest_marker != markers.end() )
        {
            (*o)->markers_.push_back( *nearest_marker );
            (*o)->markers_.back().target_ = (*o)->player_;
        }
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
MarkAnalyzer::createCombination( const std::vector< Target * > & target_opponents,
                                 std::vector< Combination > & combinations )
{
#ifdef DEBUG_PROFILE_LEVEL2
    Timer timer;
#endif

    std::vector< const Marker * > combination_stack;

    combinations.clear();

#ifdef DEBUG_PRINT_COMBINATION
    dlog.addText( Logger::MARK,
                  "(createCombination) recursive start. target_size=%d",
                  (int)target_opponents.size() );
#endif
    createCombination( target_opponents.begin(), target_opponents.end(),
                       combination_stack,
                       combinations );

#ifdef DEBUG_PRINT_COMBINATION
    dlog.addText( Logger::MARK,
                  "(createCombination) recursive end" );
#endif

#ifdef DEBUG_PROFILE_LEVEL2
    dlog.addText( Logger::MARK,
                  __FILE__":(createCombination) elapsed %.3f [ms]",
                  timer.elapsedReal() );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
MarkAnalyzer::createCombination( std::vector< Target * >::const_iterator first,
                                 std::vector< Target * >::const_iterator last,
                                 std::vector< const Marker * > & combination_stack,
                                 std::vector< Combination > & combinations )
{
#ifdef DEBUG_PRINT_COMBINATION
    {
        std::ostringstream os;
        for ( std::vector< const Marker * >::const_iterator mp = combination_stack.begin();
              mp != combination_stack.end();
              ++mp )
        {
            os << '(' << (*mp)->player_->unum() << ' ' << (*mp)->target_->unum() << ')';
        }
        dlog.addText( Logger::MARK,
                      "stack: %s",
                      os.str().c_str() );
    }
#endif


    if ( first == last )
    {
        combinations.push_back( Combination() );
        combinations.back().markers_ = combination_stack;

#ifdef DEBUG_PRINT_COMBINATION
        std::ostringstream os;
        for ( std::vector< const Marker * >::const_iterator mp = combination_stack.begin();
              mp != combination_stack.end();
              ++mp )
        {
            os << '(' << (*mp)->player_->unum() << ' ' << (*mp)->target_->unum() << ')';
        }
        dlog.addText( Logger::MARK,
                      "-> add combination: %s",
                      os.str().c_str() );
#endif
        return;
    }

    std::size_t prev_size = combinations.size();

    for ( std::vector< Marker >::const_iterator m = (*first)->markers_.begin(),
              m_end = (*first)->markers_.end();
          m != m_end;
          ++m )
    {
        if ( std::find_if( combination_stack.begin(), combination_stack.end(), MarkerEqual( m->player_ ) )
             == combination_stack.end() )
        {
            combination_stack.push_back( &(*m) );
            createCombination( first + 1, last, combination_stack, combinations );
            combination_stack.pop_back();
        }
#ifdef DEBUG_PRINT_COMBINATION
        else if ( ! combination_stack.empty() )
        {
            std::ostringstream os;
            for ( std::vector< const Marker * >::const_iterator mp = combination_stack.begin();
                  mp != combination_stack.end();
                  ++mp )
            {
                os << (*mp)->player_->unum() << ' ';
            }
            dlog.addText( Logger::MARK,
                          "xxx cancel: %s (%d)",
                          os.str().c_str(),
                          m->player_->unum() );
        }
#endif
    }

    if ( prev_size == combinations.size()
         && ! combination_stack.empty() )
    {
#ifdef DEBUG_PRINT_COMBINATION
        std::ostringstream os;
        for ( std::vector< const Marker * >::const_iterator p = combination_stack.begin();
              p != combination_stack.end();
              ++p )
        {
            os << '(' << (*p)->player_->unum() << ' ' << (*p)->target_->unum() << ')';
        }
        dlog.addText( Logger::MARK,
                      "-> add sub combination: %s",
                      os.str().c_str() );
#endif
        combinations.push_back( Combination() );
        combinations.back().markers_ = combination_stack;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
MarkAnalyzer::updateUnumPairs( const WorldModel & wm,
                               const std::vector< Combination > & combinations )
{
    std::vector< Combination >::const_iterator it = std::max_element( combinations.begin(),
                                                                      combinations.end(),
                                                                      CombinationSorter() );
    if ( it == combinations.end() )
    {
        return;
    }

    dlog.addText( Logger::MARK,
                  __FILE__":(updateUnumPairs) total_combination=%d, best_score=%f",
                  combinations.size(),
                  it->score_ );

    for ( int i = 0; i < 11; ++i )
    {
        M_last_assignment[i] = Unum_Unknown;
    }

    const bool playon = ( wm.gameMode().type() == GameMode::PlayOn );

    for ( std::vector< const Marker * >::const_iterator m = it->markers_.begin(),
              end = it->markers_.end();
          m != end;
          ++m )
    {
        M_pairs.push_back( Pair( (*m)->player_, (*m)->target_ ) );
        int t = (*m)->player_->unum();
        int o = (*m)->target_->unum();
        if ( t != Unum_Unknown
             && o != Unum_Unknown )
        {
            M_unum_pairs.push_back( UnumPair( t, o ) );
            if ( playon )
            {
                M_last_assignment[t-1] = o;
                M_matching_count[t-1][o-1] += 1;
            }
        }
#ifdef DEBUG_PRINT_RESULT
        dlog.addText( Logger::MARK,
                      "<<< pair (%d, %d) marker(%.1f %.1f)(%.1f %.1f) - target(%.1f %.1f)",
                      (*m)->player_->unum(),
                      (*m)->target_->unum(),
                      (*m)->player_->pos().x, (*m)->player_->pos().y,
                      (*m)->pos().x, (*m)->pos().y,
                      (*m)->target_->pos().x, (*m)->target_->pos().y );
#endif
#ifdef DEBUG_PAINT_RESULT
        dlog.addRect( Logger::MARK,
                      (*m)->pos().x - 0.2, (*m)->pos().y - 0.2, 0.4, 0.4,
                      "#F00", true );
        dlog.addRect( Logger::MARK,
                      (*m)->target_->pos().x - 0.2, (*m)->target_->pos().y - 0.2, 0.4, 0.4,
                      "#00F", true );
        dlog.addLine( Logger::MARK,
                      (*m)->pos(), (*m)->target_->pos(),
                      "#FF0" );
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
MarkAnalyzer::evaluate( const WorldModel & wm,
                        std::vector< Combination > & combinations )
{
    //evaluate2012( wm, combinations );
    //evaluate2013( wm, combinations );
    evaluate2015( wm, combinations );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
MarkAnalyzer::evaluate2012( const WorldModel & /*wm*/,
                            std::vector< Combination > & combinations )
{
    const Strategy & ST = Strategy::i();

#ifdef DEBUG_EVAL
    double best = -1000000.0;
#endif
    for ( std::vector< Combination >::iterator it = combinations.begin(),
              c_end = combinations.end();
          it != c_end;
          ++it )
    {
        double total_dist = 0.0;
        for ( std::vector< const Marker * >::const_iterator m = it->markers_.begin(),
                  m_end = it->markers_.end();
              m != m_end;
              ++m )
        {
            double dist = (*m)->pos().dist( (*m)->target_->pos() );

            const int unum = (*m)->player_->unum();
            if ( unum != Unum_Unknown
                 && M_last_assignment[unum-1] != (*m)->target_->unum() )
            {
                total_dist += 5.0; // magic number
                //total_dist += 10.0; // magic number
                //total_dist += 20.0; // magic number
            }

            if ( ST.roleType( unum ) == Formation::Defender )
            {
                //total_dist += dist * 1.3;
                if ( ST.getPositionType( unum ) == Position_Center )
                {
                    total_dist += dist * 2.0;
                }
                else
                {
                    total_dist += dist * 1.3;
                }
            }
            else
            {
                total_dist += dist;
            }
        }

        double weighted_dist = total_dist;
        if ( M_strategic_marker_count > it->markers_.size() )
        {
            for ( size_t i = it->markers_.size(); i < M_strategic_marker_count; ++i )
            {
                weighted_dist += 50.0;
            }
        }

        it->score_ = -weighted_dist / M_strategic_marker_count;

#ifdef DEBUG_EVAL
        std::ostringstream os;
        for ( std::vector< const Marker * >::const_iterator m = it->markers_.begin(),
                  m_end = it->markers_.end();
              m != m_end;
              ++m )
        {
            os << '(' << (*m)->player_->unum() << ',' << (*m)->target_->unum() << ')';
        }
        dlog.addText( Logger::MARK,
                      "** eval: %s : %lf (total_dist=%f weighted=%f) %s",
                      os.str().c_str(),
                      it->score_, total_dist, weighted_dist,
                      ( best < it->score_ ? "updated" : "" ) );
        if ( best < it->score_ )
        {
            best = it->score_;
        }
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
MarkAnalyzer::evaluate2013( const WorldModel & wm,
                            std::vector< Combination > & combinations )
{
    (void)wm;

    //
    // evaluation loop
    //

#ifdef DEBUG_EVAL
    double best = -1000000.0;
#endif
    int count = 1;
    for ( std::vector< Combination >::iterator it = combinations.begin(),
              c_end = combinations.end();
          it != c_end;
          ++it, ++count )
    {
        double total_dist = 0.0;
        for ( std::vector< const Marker * >::const_iterator m = it->markers_.begin(),
                  m_end = it->markers_.end();
              m != m_end;
              ++m )
        {
            Vector2D target_pos = (*m)->target_->pos();
            //target_pos.x += x_shift;

            double dist = (*m)->pos().dist( target_pos );

            // dlog.addText( Logger::MARK,
            //               "%d: marker=%d target=%d target_pos=(%.2f %.2f)  dist=%.3f",
            //               count, (*m)->player_->unum(), (*m)->target_->unum(),
            //               target_pos.x, target_pos.y, dist );

            const int unum = (*m)->player_->unum();
            if ( unum != Unum_Unknown
                 && M_last_assignment[unum-1] != (*m)->target_->unum() )
            {
                total_dist += 5.0; // magic number
                //total_dist += 10.0; // magic number
                //total_dist += 20.0; // magic number
            }

            if ( Strategy::i().roleType( unum ) == Formation::Defender )
            {
                //total_dist += dist * 1.3;
                if ( Strategy::i().getPositionType( unum ) == Position_Center )
                {
                    total_dist += dist * 2.0;
                }
                else
                {
                    total_dist += dist * 1.3;
                }
            }
            else
            {
                total_dist += dist;
            }
        }

        double weighted_dist = total_dist;
        if ( M_strategic_marker_count > it->markers_.size() )
        {
            for ( size_t i = it->markers_.size(); i < M_strategic_marker_count; ++i )
            {
                weighted_dist += 50.0;
            }
        }

        it->score_ = -weighted_dist / M_strategic_marker_count;

#ifdef DEBUG_EVAL
        std::ostringstream os;
        for ( std::vector< const Marker * >::const_iterator m = it->markers_.begin(),
                  m_end = it->markers_.end();
              m != m_end;
              ++m )
        {
            os << '(' << (*m)->player_->unum() << ',' << (*m)->target_->unum() << ')';
        }
        dlog.addText( Logger::MARK,
                      "%d: ** eval: %s : %lf (total_dist=%f weighted=%f) %s",
                      count, os.str().c_str(),
                      it->score_, total_dist, weighted_dist,
                      ( best < it->score_ ? "updated" : "" ) );
        if ( best < it->score_ )
        {
            best = it->score_;
        }
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
MarkAnalyzer::evaluate2015( const WorldModel & ,
                            std::vector< Combination > & combinations )
{
    //
    // evaluation loop
    //

    int count = 1;
    for ( std::vector< Combination >::iterator it = combinations.begin(),
              c_end = combinations.end();
          it != c_end;
          ++it, ++count )
    {
        double total_dist = 0.0;
        for ( std::vector< const Marker * >::const_iterator m = it->markers_.begin(),
                  m_end = it->markers_.end();
              m != m_end;
              ++m )
        {
            Vector2D target_pos = (*m)->target_->pos();
            //target_pos.x += x_shift;

            double dist = (*m)->pos().dist( target_pos );

            // dlog.addText( Logger::MARK,
            //               "%d: marker=%d target=%d target_pos=(%.2f %.2f)  dist=%.3f",
            //               count, (*m)->player_->unum(), (*m)->target_->unum(),
            //               target_pos.x, target_pos.y, dist );

            const int unum = (*m)->player_->unum();
            if ( unum != Unum_Unknown
                 && M_last_assignment[unum-1] != (*m)->target_->unum() )
            {
                total_dist += dist * 0.1; // magic number
            }

            if ( Strategy::i().roleType( unum ) == Formation::Defender )
            {
                if ( Strategy::i().getPositionType( unum ) == Position_Center )
                {
                    total_dist += dist * 2.0;
                }
                else
                {
                    total_dist += dist * 1.3;
                }
            }
            else
            {
                total_dist += dist;
            }
        }

        double weighted_dist = total_dist;
        if ( M_strategic_marker_count > it->markers_.size() )
        {
            for ( size_t i = it->markers_.size(); i < M_strategic_marker_count; ++i )
            {
                weighted_dist += 50.0;
            }
        }

        it->score_ = -weighted_dist / M_strategic_marker_count;
    }


#ifdef DEBUG_EVAL
    std::sort( combinations.begin(), combinations.end(), CombinationSorter() );
    for ( std::vector< Combination >::const_reverse_iterator it = combinations.rbegin(), end = combinations.rend();
          it != end;
          ++it )
    {
        std::ostringstream os;
        for ( std::vector< const Marker * >::const_iterator m = it->markers_.begin(),
                  m_end = it->markers_.end();
              m != m_end;
              ++m )
        {
            os << '(' << (*m)->player_->unum() << ',' << (*m)->target_->unum() << ')';
        }
        dlog.addText( Logger::MARK,
                      "# %lf %s",
                      it->score_, os.str().c_str() );
    }
#endif
}
