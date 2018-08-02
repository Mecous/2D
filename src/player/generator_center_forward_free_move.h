// -*-c++-*-

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

#ifndef GENERATOR_
#define GENERATOR_CENTER_FORWARD_FREE_MOVE_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/game_time.h>

#include <svmrank/svm_struct_api_types.h>

#include <vector>

namespace rcsc {
class AbstractPlayerObject;
class WorldModel;
}

class GeneratorCenterForwardFreeMove {
private:

    svmrank::STRUCTMODEL M_model;
    svmrank::STRUCT_LEARN_PARM M_learn_param;

    rcsc::GameTime M_update_time;
    rcsc::Vector2D M_best_point;

    // private for singleton
    GeneratorCenterForwardFreeMove();

    // not used
    GeneratorCenterForwardFreeMove( const GeneratorCenterForwardFreeMove & );
    GeneratorCenterForwardFreeMove & operator=( const GeneratorCenterForwardFreeMove & );

public:

    ~GeneratorCenterForwardFreeMove();

    static
    GeneratorCenterForwardFreeMove & instance();

    bool init();

    const rcsc::Vector2D & getBestPoint( const rcsc::WorldModel & wm )
      {
          generate( wm );
          return M_best_point;
      }
private:

    void clear();
    void generate( const rcsc::WorldModel & wm );
    rcsc::Vector2D generateBestPoint( const rcsc::WorldModel & wm );
    double evaluatePoint( const rcsc::WorldModel & wm,
                          const rcsc::Vector2D & pos,
                          const rcsc::Vector2D & home_pos,
                          const rcsc::Vector2D & ball_pos );
    void writeRankData( const rcsc::WorldModel & wm,
                        const double value,
                        const svmrank::WORD * words,
                        const int n_words );

};

#endif
