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

#ifndef ACTGEN_VORONOI_PASS_H
#define ACTGEN_VORONOI_PASS_H

#include "action_generator.h"

#include "predict_player_object.h"

#include <rcsc/geom/vector_2d.h>

class ActGen_VoronoiPass
    : public ActionGenerator {

public:

    virtual
    void generate( std::vector< ActionStatePair > * result,
                   const PredictState & state,
                   const rcsc::WorldModel & wm,
                   const std::vector< ActionStatePair > & path ) const;

private:
    bool createReceiverCandidates( PredictPlayerObject::Cont & receivers,
                                   const PredictState & state,
                                   const rcsc::WorldModel & wm,
                                   const std::vector< ActionStatePair > & path ) const;
    void generateActions( std::vector< ActionStatePair > * result,
                          const PredictState & state,
                          const std::vector< rcsc::Vector2D > & candidates,
                          const PredictPlayerObject::Cont & receivers ) const;
};

#endif
