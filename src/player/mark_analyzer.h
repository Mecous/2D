// -*-c++-*-

/*!
  \file mark_analyzer.h
  \brief mark assignment analyzer Header File
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

#ifndef MARK_ANALYZER_H
#define MARK_ANALYZER_H

#include <rcsc/player/abstract_player_object.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <vector>

namespace rcsc {
class WorldModel;
}

class MarkAnalyzer {
public:

    struct Marker {
        const rcsc::AbstractPlayerObject * player_; //!< pointer to the marker teammate
        const rcsc::AbstractPlayerObject * target_; //!< pointer to the target opponent
        rcsc::Vector2D pos_; //!< base position for the marker teammate

        Marker( const rcsc::AbstractPlayerObject * player,
                const rcsc::Vector2D & pos )
            : player_( player ),
              target_( static_cast< const rcsc::AbstractPlayerObject * >( 0 ) ),
              pos_( pos )
          { }

        const rcsc::Vector2D & pos() const
          {
              return pos_;
          }

    private:
        // not used
        Marker();
    };

    struct Target {
        const rcsc::AbstractPlayerObject * player_; //!< pointer to the target opponent
        std::vector< Marker > markers_; //!< candidate markers

        Target()
          { }

        Target( const rcsc::AbstractPlayerObject * player )
            : player_( player )
          { }

        const rcsc::Vector2D & pos() const
          {
              return player_->pos();
          }
    };

    struct Combination {
        std::vector< const Marker * > markers_; //!< assigned marker list
        double score_; //!< evaluated score

        Combination()
            : score_( 0.0 )
          { }
    };

    struct Pair {
        const rcsc::AbstractPlayerObject * marker_; //!< pointer to the marker teammate
        const rcsc::AbstractPlayerObject * target_; //!< pointer to the target opponent

        Pair( const rcsc::AbstractPlayerObject * marker,
              const rcsc::AbstractPlayerObject * target )
            : marker_( marker ),
              target_( target )
          { }

    private:
        // not used
        Pair();
    };

    struct UnumPair {
        int marker_;
        int target_;

        UnumPair( const int marker,
                  const int target )
            : marker_( marker ),
              target_( target )
          { }

    private:
        // not used
        UnumPair();
    };

private:

    size_t M_strategic_marker_count;

    // pair of the pointer to the mark target opponent and the marker teammate
    std::vector< Pair > M_pairs;
    std::vector< UnumPair > M_unum_pairs;

    int M_last_assignment[11];
    int M_matching_count[11][11];

    // mark assignment by coach's directive, marker to target
    int M_coach_assignment[11][2];

    // mark assignment by coach's directive, target to marker
    int M_coach_assignment_target[11][2];

    // private for singleton
    MarkAnalyzer();

    // not used
    MarkAnalyzer( const MarkAnalyzer & );
    const MarkAnalyzer & operator=( const MarkAnalyzer & );
public:

    static
    MarkAnalyzer & instance();

    static
    const MarkAnalyzer & i()
      {
          return instance();
      }

    //
    // getter methods.
    //

    const rcsc::AbstractPlayerObject * getTargetOf( const int marker_unum ) const;
    const rcsc::AbstractPlayerObject * getTargetOf( const rcsc::AbstractPlayerObject * marker ) const;

    const rcsc::AbstractPlayerObject * getMarkerOf( const int target_unum ) const;
    const rcsc::AbstractPlayerObject * getMarkerOf( const rcsc::AbstractPlayerObject * target ) const;

    const Pair * getPairOfMarker( const int unum ) const;
    const Pair * getPairOfTarget( const int unum ) const;

    const UnumPair * getUnumPairOfMarker( const int unum ) const;
    const UnumPair * getUnumPairOfTarget( const int unum ) const;


    int getFirstTargetByCoach( const int our_unum ) const;
    int getSecondTargetByCoach( const int our_unum ) const;

    int getMarkerToFirstTargetByCoach( const int their_unum ) const;
    int getMarkerToSecondTargetByCoach( const int their_unum ) const;

    int isFirstMarkerByCoach( const int their_unum,
                              const int our_unum ) const;
    int isSecondMarkerByCoach( const int their_unum,
                               const int our_unum ) const;

    /*!
      \brief update mark assignment by coach's directive
      \return true if assignment is changed.
     */
    bool setAssignmentByCoach( const int our_unum,
                               const int first_target_unum,
                               const int second_target_unum );

    /*!
      \brief update for the current state
     */
    void update( const rcsc::WorldModel & wm );

private:

    bool createMarkTargets( const rcsc::WorldModel & wm,
                            std::vector< Target * > & target_opponents );
    bool createMarkerCandidates( const rcsc::WorldModel & wm,
                                 std::vector< Marker > & markers );
    bool createTargetCandidates( const rcsc::WorldModel & wm,
                                 std::vector< Target * > & target_opponents );
    void setMarkerToTarget( const std::vector< Marker > & markers,
                            std::vector< Target * > & target_opponents );

    void createCombination( const std::vector< Target * > & target_opponents,
                            std::vector< Combination > & combinations );
    /*!
      \brief recursive function
     */
    void createCombination( std::vector< Target * >::const_iterator first,
                            std::vector< Target * >::const_iterator last,
                            std::vector< const Marker * > & combination_stack,
                            std::vector< Combination > & combinations );

    void updateUnumPairs( const rcsc::WorldModel & wm,
                          const std::vector< Combination > & combinations );

    void evaluate( const rcsc::WorldModel & wm,
                   std::vector< Combination > & combinations );
    void evaluate2012( const rcsc::WorldModel & wm,
                       std::vector< Combination > & combinations );
    void evaluate2013( const rcsc::WorldModel & wm,
                       std::vector< Combination > & combinations );
    void evaluate2015( const rcsc::WorldModel & wm,
                       std::vector< Combination > & combinations );
};

#endif
