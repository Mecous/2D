// -*-c++-*-

/*!
  \file cooperative_action.h
  \brief cooperative action type Header File.
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 3 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifndef COOPERATIVE_ACTION_H
#define COOPERATIVE_ACTION_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>
#include <rcsc/types.h>

#include <boost/shared_ptr.hpp>

/*!
  \class CooperativeAction
  \brief abstract cooperative action class
 */
class CooperativeAction {
public:

    typedef boost::shared_ptr< CooperativeAction > Ptr; //!< pointer type
    typedef boost::shared_ptr< const CooperativeAction > ConstPtr; //!< const pointer type

    /*!
      \enum Type
      \brief action types
     */
    enum Type {
        Hold,
        Dribble,
        Pass,
        Shoot,
        Clear,
        Move,

        NoAction,
    };

    enum SafetyLevel {
        Failure = 0,
        Dangerous = 1,
        MaybeDangerous = 2,
        Safe = 3,
    };

    static const double ERROR_ANGLE;

private:

    Type M_type; //!< action type
    int M_index; //!< index number
    int M_mode; //!< action mode flag. arbitrary value can be used for each action type.

    int M_player_unum; //!< action taker player's uniform number
    int M_target_player_unum; //!< action target player's uniform number

    rcsc::Vector2D M_target_ball_pos; //!< result ball position
    rcsc::Vector2D M_target_player_pos; //!< result player position
    double M_target_body_angle; //!< result player body angle (if necessary)

    rcsc::Vector2D M_first_ball_vel; //!< ball position after the first action (if necesary)
    double M_first_turn_moment; //!< first turn moment (if necessary)
    double M_first_dash_power; //!< first dash speed (if necessary)
    double M_first_dash_dir; //!< first dash direction relative to player's body (if necessary)

    int M_duration_time; //!< action duration period

    int M_kick_count; //!< kick count (if necessary)
    int M_turn_count; //!< dash count (if necessary)
    int M_dash_count; //!< dash count (if necessary)

    SafetyLevel M_safety_level; //!< estimated action safety level

    const char * M_description; //!< description message for debugging purpose

    // not used
    CooperativeAction();
    CooperativeAction( const CooperativeAction & );
    CooperativeAction & operator=( const CooperativeAction & );
protected:

    /*!
      \brief construct with necessary variables
      \param type action category type
      \param player_unum action taker player's unum
      \param target_ball_pos target ball position after this action
      \param duration_time this action's duration time steps
      \param description description message (must be a literal character string)
     */
    CooperativeAction( const Type type,
                       const int player_unum,
                       const rcsc::Vector2D & target_ball_pos,
                       const int duration_time,
                       const char * description );

public:

    /*!
      \brief virtual destructor.
     */
    virtual
    ~CooperativeAction()
      { }

    //
    // setter method
    //

    void setIndex( const int i ) { M_index = i; }
    void setMode( const int mode ) { M_mode = mode; }

    void setTargetBallPos( const rcsc::Vector2D & pos ) { M_target_ball_pos = pos; }

    void setTargetPlayerUnum( const int unum ) { M_target_player_unum = unum; }
    void setTargetPlayerPos( const rcsc::Vector2D & pos ) { M_target_player_pos = pos; }
    void setTargetBodyAngle( const double angle ) { M_target_body_angle = angle; }

    void setFirstBallVel( const rcsc::Vector2D & vel ) { M_first_ball_vel = vel; }
    void setFirstTurnMoment( const double moment ) { M_first_turn_moment = moment; }
    void setFirstDashPower( const double power ) { M_first_dash_power = power; }
    void setFirstDashDir( const double dir ) { M_first_dash_dir = dir; }

    void setKickCount( const int count ) { M_kick_count = count; }
    void setTurnCount( const int count ) { M_turn_count = count; }
    void setDashCount( const int count ) { M_dash_count = count; }

    void setSafetyLevel( const SafetyLevel level ) { M_safety_level = level; }

    void setDescription( const char * description ) { M_description = description; }

    //
    // accessor method
    //

    Type type() const { return M_type; }
    int index() const { return M_index; }
    int mode() const { return M_mode; }

    int playerUnum() const { return M_player_unum; }
    int targetPlayerUnum() const { return M_target_player_unum; }

    const rcsc::Vector2D & targetBallPos() const { return M_target_ball_pos; }
    const rcsc::Vector2D & targetPlayerPos() const { return M_target_player_pos; }
    double targetBodyAngle() const { return M_target_body_angle; }

    const rcsc::Vector2D & firstBallVel() const { return M_first_ball_vel; }
    double firstTurnMoment() const { return M_first_turn_moment; }
    double firstDashPower() const { return M_first_dash_power; }
    double firstDashDir() const { return M_first_dash_dir; }

    int durationTime() const { return M_duration_time; }

    int kickCount() const { return M_kick_count; }
    int turnCount() const { return M_turn_count; }
    int dashCount() const { return M_dash_count; }

    SafetyLevel safetyLevel() const { return M_safety_level; }

    const char * description() const { return M_description; }

    //
    //
    //

    bool isFinalAction() const
      {
        return M_type == Shoot
            || M_type == Clear;
      }

    //
    //
    //

    struct DistanceSorter {
        const rcsc::Vector2D pos_;

    private:
        DistanceSorter();
    public:

        DistanceSorter( const rcsc::Vector2D & pos )
            : pos_( pos )
          { }

        bool operator()( const Ptr & lhs,
                         const Ptr & rhs ) const
          {
              return lhs->targetBallPos().dist2( pos_ ) < rhs->targetBallPos().dist2( pos_ );
          }
    };

    struct BasicSorter {
    private:
        const rcsc::Vector2D pos_;

    public:
        BasicSorter( const rcsc::Vector2D & pos )
            : pos_( pos )
          { }

        bool operator()( const Ptr & lhs,
                         const Ptr & rhs ) const

          {
              return ( lhs->safetyLevel() == rhs->safetyLevel()
                       ? lhs->targetBallPos().dist2( pos_ ) < rhs->targetBallPos().dist2( pos_ )
                       : lhs->safetyLevel() > rhs->safetyLevel() );
          }
    };

    struct SafetyLevelSorter {
        bool operator()( const Ptr & lhs,
                         const Ptr & rhs ) const
          {
              return lhs->safetyLevel() > rhs->safetyLevel();
          }
    };
};


#endif
