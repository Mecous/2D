// -*-c++-*-

/*!
  \file act_pass.cpp
  \brief pass action object Source File.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "act_pass.h"

#include <rcsc/common/server_param.h>
#include <rcsc/math_util.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
ActPass::ActPass( const int passer,
                  const int receiver,
                  const Vector2D & receive_point,
                  const Vector2D & first_ball_vel,
                  const int duration_step,
                  const int kick_count,
                  const char * description )
    : CooperativeAction( CooperativeAction::Pass,
                         passer,
                         receive_point,
                         duration_step,
                         description )
{
    setTargetPlayerUnum( receiver );
    setFirstBallVel( first_ball_vel );
    setKickCount( kick_count );
}
