// -*-c++-*-

/*!
  \file act_dribble.cpp
  \brief dribble object Source File.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "act_dribble.h"

#include <rcsc/common/server_param.h>
#include <rcsc/math_util.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
ActDribble::ActDribble( const int unum,
                        const rcsc::Vector2D & target_ball_pos,
                        const int duration_time,
                        const char * description )
  : CooperativeAction( CooperativeAction::Dribble,
                       unum,
                       target_ball_pos,
                       duration_time,
                       description )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::Ptr
ActDribble::create_normal( const int unum,
                           const Vector2D & target_ball_pos,
                           const double target_body_angle,
                           const Vector2D & first_ball_vel,
                           const int kick_count,
                           const int turn_count,
                           const int dash_count,
                           const char * description )
{
    CooperativeAction::Ptr ptr( new ActDribble( unum,
                                                target_ball_pos,
                                                kick_count + turn_count + dash_count,
                                                description ) );
    ptr->setMode( KICK_TURN_DASHES );
    ptr->setTargetPlayerUnum( unum );
    ptr->setTargetPlayerPos( target_ball_pos );
    ptr->setTargetBodyAngle( target_body_angle );
    ptr->setFirstBallVel( first_ball_vel );
    ptr->setFirstTurnMoment( 0.0 );
    ptr->setFirstDashPower( 0.0 );
    ptr->setFirstDashDir( 0.0 );
    ptr->setKickCount( kick_count );
    ptr->setTurnCount( turn_count );
    ptr->setDashCount( dash_count );

    return ptr;
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::Ptr
ActDribble::create_dash_only( const int unum,
                              const Vector2D & target_ball_pos,
                              const Vector2D & target_player_pos,
                              const double target_body_angle,
                              const rcsc::Vector2D & first_ball_vel,
                              const double dash_power,
                              const double dash_dir,
                              const int dash_count,
                              const char * description )
{
    CooperativeAction::Ptr ptr( new ActDribble( unum,
                                                target_ball_pos,
                                                dash_count,
                                                description ) );
    ptr->setMode( KEEP_DASHES );
    ptr->setTargetPlayerUnum( unum );
    ptr->setTargetPlayerPos( target_player_pos );
    ptr->setTargetBodyAngle( target_body_angle );
    ptr->setFirstBallVel( first_ball_vel );
    // ptr->setFirstTurnMoment( 0.0 );
    ptr->setFirstDashPower( dash_power );
    ptr->setFirstDashDir( dash_dir );
    // ptr->setKickCount( 0 ); // no kick
    // ptr->setTurnCount( 0 );
    ptr->setDashCount( dash_count );

    return ptr;
}

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::Ptr
ActDribble::create_omni( const int unum,
                         const rcsc::Vector2D & target_ball_pos,
                         const rcsc::Vector2D & target_player_pos,
                         const double target_body_angle,
                         const rcsc::Vector2D & first_ball_vel,
                         const double dash_power,
                         const double dash_dir,
                         const int dash_count,
                         const char * description )
{
    CooperativeAction::Ptr ptr( new ActDribble( unum,
                                                target_ball_pos,
                                                dash_count,
                                                description ) );
    ptr->setMode( OMNI_KICK_DASHES );
    ptr->setTargetPlayerUnum( unum );
    ptr->setTargetPlayerPos( target_player_pos );
    ptr->setTargetBodyAngle( target_body_angle );
    ptr->setFirstBallVel( first_ball_vel );
    ptr->setFirstTurnMoment( 0.0 );
    ptr->setFirstDashPower( dash_power );
    ptr->setFirstDashDir( dash_dir );
    ptr->setKickCount( 1 ); // 1 kick
    ptr->setTurnCount( 0 );
    ptr->setDashCount( dash_count );

    return ptr;
}
