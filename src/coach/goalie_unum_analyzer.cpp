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
#include "config.h"
#endif

#include "goalie_unum_analyzer.h"

#include "coach_analyzer_manager.h"

#include <rcsc/coach/coach_agent.h>
#include <rcsc/coach/coach_world_model.h>

#include <rcsc/common/logger.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
GoalieUnumAnalyzer::GoalieUnumAnalyzer()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
GoalieUnumAnalyzer::analyze( CoachAgent * agent )
{
    static int s_our_unum = Unum_Unknown;
    static int s_their_unum = Unum_Unknown;

    const CoachWorldModel & wm = agent->world();

    int our_unum = Unum_Unknown;
    int their_unum = Unum_Unknown;

    for ( CoachPlayerObject::Cont::const_iterator p = wm.teammates().begin(),
              end = wm.teammates().end();
          p != end;
          ++p )
    {
        if ( (*p)->goalie() )
        {
            our_unum = (*p)->unum();
            break;
        }
    }

    for ( CoachPlayerObject::Cont::const_iterator p = wm.opponents().begin(),
              end = wm.opponents().end();
          p != end;
          ++p )
    {
        if ( (*p)->goalie() )
        {
            their_unum = (*p)->unum();
            break;
        }
    }

    if ( our_unum != Unum_Unknown
         && their_unum != Unum_Unknown )
    {
        if ( s_our_unum != our_unum
             || s_their_unum != their_unum )
        {
            s_our_unum = our_unum;
            s_our_unum = their_unum;
            CoachAnalyzerManager::instance().setGoalieUnum( our_unum, their_unum );
        }
    }
    return true;
}
