// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

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

#include "field_evaluator.h"
#include "predict_state.h"
#include "cooperative_action.h"
#include "action_state_pair.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/abstract_player_object.h>
#include <rcsc/color/thermo_color_provider.h>
#include <rcsc/color/rgb_color.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/vector_2d.h>

#include <map>
#include <vector>
#include <utility>

// #define FIELD_EVALUATOR_DEBUG_LOG

using namespace rcsc;

static const double GRID_SIZE = 5.0;
static const double RECT_SIZE_RATE = 0.2;

void
FieldEvaluator::writeDebugLog( const WorldModel & wm ) const
{
#ifdef FIELD_EVALUATOR_DEBUG_LOG
    const double w = GRID_SIZE * RECT_SIZE_RATE;

    const ServerParam & SP = ServerParam::i();
    const ThermoColorProvider color_provider;

    const int n_x = SP.pitchHalfLength() / GRID_SIZE + 1;
    const int n_y = SP.pitchHalfWidth() / GRID_SIZE + 1;

    const PredictState::Ptr current_state( new PredictState( wm ) );

    std::multimap< double, Vector2D > evaluated_points;

    for ( int x = -n_x; x <= n_x; ++ x )
    {
        for ( int y = -n_y; y <= n_y; ++ y )
        {
            const Vector2D pos( x * GRID_SIZE, y * GRID_SIZE );

            const AbstractPlayerObject * nearest
                = current_state->getOurPlayerNearestTo( pos,
                                                        SP.halfTime(),
                                                        static_cast< double * >( 0 ) );
            if ( ! nearest )
            {
                nearest = &(current_state->self());
            }

            PredictState s( *current_state,
                            1,
                            nearest->unum(),
                            pos );

            const double ev = this->evaluate( s, std::vector< ActionStatePair >() );
            evaluated_points.insert( std::pair< double, Vector2D >( ev, pos ) );
        }
    }

    const size_t n_points = evaluated_points.size();
    int n = 0;
    int nth = 0;
    double old_ev = 0.0;

    for( std::multimap< double, Vector2D >::const_iterator it = evaluated_points.begin(),
             end = evaluated_points.end();
         it != end;
         ++ it )
    {
        const double ev = (*it).first;
        const Vector2D & pos = (*it).second;

        if ( ev != old_ev
             || n == 0 )
        {
            nth = n;
        }

        const double value = static_cast< double >( nth ) / n_points;
        const RGBColor c = color_provider.convertToColor( value );

        dlog.addRect( Logger::ACTION_CHAIN,
                      Rect2D::from_center( pos, w, w ),
                      c.name().c_str(),
                      true );

        ++ n;
        old_ev = ev;
    }
#else
    static_cast< void >( wm );
#endif
}
