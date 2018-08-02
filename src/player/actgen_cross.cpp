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

#include "actgen_cross.h"

#include "generator_cross.h"

#include "action_state_pair.h"
#include "predict_state.h"

#include <rcsc/common/logger.h>

//#define DEBUG_PRINT

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
void
ActGen_Cross::generate( std::vector< ActionStatePair > * result,
                        const PredictState & state,
                        const WorldModel & wm,
                        const std::vector< ActionStatePair > & path ) const
{
    // generate only first actions
    if ( ! path.empty() )
    {
        return;
    }

    const GeneratorCross::Cont & courses = GeneratorCross::instance().courses( wm );

    //
    // add pass course candidates
    //
    for ( GeneratorCross::Cont::const_iterator it = courses.begin(), end = courses.end();
          it != end;
          ++it )
    {
        if ( it->action_->targetPlayerUnum() == Unum_Unknown )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          __FILE__": can't cross from %d,"
                          " target invalid",
                          state.ballHolder()->unum() );
#endif
            continue;
        }

        const AbstractPlayerObject * target_player = state.ourPlayer( it->action_->targetPlayerUnum() );

        if ( ! target_player )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          __FILE__": can't cross from %d to %d, NULL target player",
                          state.ballHolder()->unum(),
                          it->action_->targetPlayerUnum() );
#endif
            continue;
        }

        result->push_back( ActionStatePair( it->action_,
                                            new PredictState( state,
                                                              std::max( it->action_->durationTime() - 3, 1 ),
                                                              it->action_->targetPlayerUnum(),
                                                              it->action_->targetBallPos() ) ) );
    }
}
