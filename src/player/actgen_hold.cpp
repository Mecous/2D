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

#include "actgen_hold.h"

#include "act_hold_ball.h"
#include "action_state_pair.h"
#include "predict_state.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

//#define DEBUG_PRINT

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
void
ActGen_Hold::generate( std::vector< ActionStatePair > * result,
                       const PredictState & state,
                       const WorldModel & wm,
                       const std::vector< ActionStatePair > & ) const
{
    if ( ! wm.self().isKickable()
         || wm.self().isFrozen() )
    {
        return;
    }

    if ( wm.self().pos().dist2( ServerParam::i().theirTeamGoalPos() ) > std::pow( 20.0, 2 ) )
    {
        return;
    }


    // if ( wm.self().pos().x < -10.0 )
    // {
    //     return;
    // }

    // if ( wm.self().pos().x < ServerParam::i().ourPenaltyAreaLineX() + 2.0
    //      && wm.self().pos().absY() < ServerParam::i().penaltyAreaHalfWidth() + 2.0 )
    // {
    //     return;
    // }

    const AbstractPlayerObject & holder = state.ballHolder();


    PredictState::ConstPtr result_state( new PredictState( state, 1 ) );

    CooperativeAction::Ptr action( new ActHoldBall( holder.unum(),
                                                    holder.pos(), //state.ball().pos(),
                                                    1,
                                                    "actgen" ) );
    action->setSafetyLevel( CooperativeAction::Safe );
    // if ( wm.opponentsFromSelf().empty()
    //      ||  wm.opponentsFromSelf().front()->distFromSelf() > 3.0 )
    // {
    //     action->setSafetyLevel( CooperativeAction::Safe );
    // }
    // else
    // {
    //     action->setSafetyLevel( CooperativeAction::MaybeDangerous );
    // }

    result->push_back( ActionStatePair( action, result_state ) );
}
