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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "actgen_self_pass.h"

#include "generator_self_pass.h"

#include "action_state_pair.h"
#include "predict_state.h"
#include <rcsc/player/world_model.h>
#include <rcsc/common/logger.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
void
ActGen_SelfPass::generate( std::vector< ActionStatePair > * result,
                           const PredictState & state,
                           const WorldModel & wm,
                           const std::vector< ActionStatePair > & path ) const
{
    // generate only first actions
    if ( ! path.empty() )
    {
        return;
    }

    //
    // get dribble table
    //
    const std::vector< CooperativeAction::Ptr > & cont = GeneratorSelfPass::instance().courses( wm );

    //
    // add candidates
    //
    for ( std::vector< CooperativeAction::Ptr >::const_iterator act = cont.begin(),
              end = cont.end();
          act != end;
          ++act )
    {
        result->push_back( ActionStatePair( *act,
                                            new PredictState( state,
                                                              (*act)->durationTime(),
                                                              (*act)->targetPlayerUnum(),
                                                              (*act)->targetBallPos() ) ) );
    }
}
