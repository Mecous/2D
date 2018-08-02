// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa Akiyama

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

#include "field_evaluator_default.h"

#include "action_state_pair.h"
#include "cooperative_action.h"
#include "predict_state.h"

#include "shoot_simulator.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

#include <limits>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
FieldEvaluatorDefault::FieldEvaluatorDefault()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
FieldEvaluatorDefault::~FieldEvaluatorDefault()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluatorDefault::evaluate( const PredictState & /*first_state*/,
                                 const std::vector< ActionStatePair > & path )
{
    const ServerParam & SP = ServerParam::i();

#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  "========= (evaluate_state) ==========" );
#endif

    if ( path.empty() )
    {
        return -1.0e+7;
    }

    const PredictState & state = path.back().state();

    //
    // ball is in opponent goal
    //
    if ( state.ball().pos().x > + ( SP.pitchHalfLength() - 0.1 )
         && state.ball().pos().absY() < SP.goalHalfWidth() + 2.0 )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) *** in opponent goal" );
#endif
        return +1.0e+7;
    }

    //
    // ball is in our goal
    //
    if ( state.ball().pos().x < - ( SP.pitchHalfLength() - 0.1 )
         && state.ball().pos().absY() < SP.goalHalfWidth() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) XXX in our goal" );
#endif
        return -1.0e+7;
    }


    //
    // out of pitch
    //
    if ( state.ball().pos().absX() > SP.pitchHalfLength()
         || state.ball().pos().absY() > SP.pitchHalfWidth() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) XXX out of pitch" );
#endif

        return -50.0 + state.ball().pos().x;
    }


    //
    // opponent has ball
    //
    if ( state.ballHolder().side() != state.self().side() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) opponent has ball" );
#endif
        return -50.0 + state.ball().pos().x;
    }


    double result_value = 0.0;

    {
        double value = calcBasicValue( state );
        result_value += value;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) basic value %f (%f)", result_value, value );
#endif
    }

    {
        double value = calcShootBonus( state );
        result_value += value;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) shoot bonus value %f (%f)", result_value, value );
#endif
    }

    {
        double rate = 1.0;
        const CooperativeAction & first_action = path.front().action();

        if ( first_action.safetyLevel() == CooperativeAction::Dangerous )
        {
            if ( first_action.type() == CooperativeAction::Dribble
                 || first_action.type() == CooperativeAction::Hold )
            {
                rate = 0.7;
            }
            else if ( first_action.type() == CooperativeAction::Pass )
            {
                rate = 0.95;
            }
        }
        else if ( first_action.safetyLevel() == CooperativeAction::MaybeDangerous )
        {
            if ( first_action.type() == CooperativeAction::Dribble
                 || first_action.type() == CooperativeAction::Hold )
            {
                rate = 0.9;
            }
        }

        result_value *= rate;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) safety level penalty %f (%f)", result_value, rate );
#endif
    }

    return result_value;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluatorDefault::calcBasicValue( const PredictState & state ) const
{
    double point = state.ball().pos().x + ServerParam::i().pitchLength();
    point += std::max( 0.0,
                       40.0 - ServerParam::i().theirTeamGoalPos().dist( state.ball().pos() ) );
    point -= std::max( 0.0,
                       40.0 - ServerParam::i().ourTeamGoalPos().dist( state.ball().pos() ) );


#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  "__ : ball pos (%f, %f) value=%f",
                  state.ball().pos().x, state.ball().pos().y,
                  point );
#endif

    return point;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluatorDefault::calcShootBonus( const PredictState & state ) const
{
    double bonus = 0.0;

    if ( ShootSimulator::can_shoot_from( state.ballHolder().unum() == state.self().unum(),
                                         state.ball().pos(),
                                         state.getPlayers( new OpponentOrUnknownPlayerPredicate( state.self().side() ) ),
                                         8 ) ) // reliability count threshold
    {
        bonus += 1.0e+6;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "__ bonus for shoot %f (%f)", 1.0e+6, bonus );
#endif

        if ( state.ballHolder().unum() == state.self().unum() )
        {
            bonus += 5.0e+5;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          "__ bonus for self shoot %f (%f)", 5.0e+5, bonus );
#endif
        }
    }

    return bonus;
}
