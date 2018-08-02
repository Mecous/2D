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

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "field_evaluator2016.h"

#include "strategy.h"

#include "action_state_pair.h"
#include "cooperative_action.h"
#include "predict_state.h"

#include "field_analyzer.h"
#include "shoot_simulator.h"
#include "simple_pass_checker.h"

#include "act_dribble.h"

#include "options.h"

#include <rcsc/player/player_evaluator.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/math_util.h>

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <ctime>

// #ifdef DEBUG_PRINT
// #undef DEBUG_PRINT
// #endif
#define DEBUG_PRINT

// #define DEBUG_PRINT_PARAM

using namespace rcsc;

namespace {

static const double THEIR_GOAL_DIST_RATE = 3.28763719460485;
static const double NEAR_OFFSIDE_LINE_DIST_THRETHOLD = 0.15935006209734;
static const double NEAR_OFFSIDE_BONUS_RATE = 0.154296541439388;
static const double NEAR_DEFENSE_LINE_DIST_THRETHOLD = 4.01426633132956;
static const double ATTACK_LINE_X_THRESHOLD = 11.6216588720622;
static const double OVER_ATTACK_LINE_RATE = 0.137215779547104;
static const double OPP_DIST_THRETHOLD = 10.0;
static const double OPP_DIST_FACTOR = 2.5;
static const double PASS_COUNT_BONUS_RATE = 10.0;

// unum
// time
// ball-pos
// shoot-chance
// over-offside-line
// opponent-goal-dist
// our-goal-dist
// our-goalie
// over-attack-line
// congestion
// value
const size_t LOG_COLUMN_SIZE = 11;


const double GOAL_VALUE = 10000.0; // +1.0e+7
const double SHOOT_VALUE = 1000.0; // +1.0e+6
const double CONCEDED_VALUE = -10000.0; // -1.0e+7

enum {
    COL_UNUM = 0,
    COL_TIME,
    COL_BALL_POS,
    COL_SHOOT_CHANCE,
    COL_OVER_OFFSIDE,
    COL_OPP_GOAL_DIST,
    COL_OUR_GOAL_DIST,
    COL_OUR_GOALIE,
    COL_OVER_ATTACK,
    COL_CONGESTION,
    COL_VALUE,
};

const Sector2D shootable_sector( Vector2D( 58.0, 0.0 ),
                                 0.0, 20.0,
                                 137.5, -137.5 );
const Vector2D best_shoot_spot( 44.0, 0.0 );
}


static const int VALID_PLAYER_THRESHOLD = 8;
static const double PASS_BALL_FIRST_SPEED = 2.5;


/*-------------------------------------------------------------------*/
/*!

 */
static
double
get_opponent_dist( const PredictState & state,
                   const Vector2D & point,
                   int opponent_additional_chase_time = 0,
                   int valid_opponent_thr = -1 )
{
    static const double MAX_DIST = 65535.0;

    double min_dist = MAX_DIST;

    for ( PredictPlayerObject::Cont::const_iterator it = state.theirPlayers().begin(),
              end = state.theirPlayers().end();
          it != end;
          ++it )
    {
        if ( (*it)->goalie() ) continue;
        if ( (*it)->ghostCount() >= 3 ) continue;

        if ( valid_opponent_thr != -1 )
        {
            if ( (*it)->posCount() > valid_opponent_thr )
            {
                continue;
            }
        }

        double dist = (*it)->pos().dist( point );
        dist -= ( bound( 0, (*it)->posCount() - 2, 2 )
                  + opponent_additional_chase_time )
            * (*it)->playerTypePtr()->realSpeedMax();

        if ( dist < min_dist )
        {
            min_dist = dist;
        }
    }

    return min_dist;
}

/*-------------------------------------------------------------------*/
/*!

 */
FieldEvaluator2016::FieldEvaluator2016()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
FieldEvaluator2016::~FieldEvaluator2016()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::evaluate( const PredictState & first_state,
                              const std::vector< ActionStatePair > & path )
{
    if ( path.empty() )
    {
        return 0.0;
    }

    double value = 0.0;

    value = evaluateImpl( first_state, path );

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getFirstActionPenalty( const PredictState & first_state,
                                           const ActionStatePair & first_pair )
{
    return getFirstActionPenaltyImpl( first_state, first_pair );
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getFirstActionPenaltyImpl( const PredictState & first_state,
                                               const ActionStatePair & first_pair )
{
    double penalty = 0.0;

    const CooperativeAction & first_action = first_pair.action();
    const PredictState & next_state = first_pair.state();
    const bool in_shootable_sector = shootable_sector.contains( next_state.ball().pos() );
    const double next_goal_dist = next_state.ball().pos().dist( ServerParam::i().theirTeamGoalPos() );

    if ( first_action.safetyLevel() == CooperativeAction::Dangerous )
    {
        if ( first_action.type() == CooperativeAction::Dribble
             || first_action.type() == CooperativeAction::Hold )
        {
            if ( in_shootable_sector )
            {
                // no penalty
            }
            else if ( next_goal_dist < 20.0
                      || ( next_state.ball().pos().x > 36.0 && next_goal_dist < 25.0 ) )
            {
                penalty = -10.0;
                //penalty = -1.0;
            }
            else
            {
                penalty = -60.0;
            }

            if ( ! in_shootable_sector
                 && Strategy::i().opponentType() == Strategy::Type_Gliders )
            {
                penalty -= 5.0;
            }

#ifdef DEBUG_PRINT
            dlog.addText( Logger::PLAN,
                          "(eval) __ penalty for dangerous dribble %f", penalty );
#endif

        }
        else if ( first_action.type() == CooperativeAction::Pass )
        {
            if ( in_shootable_sector )
            {
                // no penalty
            }
            else if ( next_state.ball().pos().x < 5.0 )
            {
                penalty = -10.0;
            }
            else
            {
                //penalty = -10.0;
                penalty = -6.0;
            }
#ifdef DEBUG_PRINT
            dlog.addText( Logger::PLAN,
                          "(eval) __ penalty for dangerous pass %f", penalty );
#endif
        }
    }
    else if ( first_action.safetyLevel() == CooperativeAction::MaybeDangerous )
    {
        if ( first_action.type() == CooperativeAction::Dribble
             || first_action.type() == CooperativeAction::Hold )
        {
            if ( in_shootable_sector )
            {
                // no penalty
            }
            else if ( next_goal_dist < 20.0
                      || ( next_state.ball().pos().x > 36.0 && next_goal_dist < 25.0 ) )
            {
                // no penalty
            }
            else if ( next_state.ball().pos().x > 0.0 )
            {
                penalty = -1.0;
            }
            else if ( next_state.ball().pos().dist( ServerParam::i().ourTeamGoalPos() ) > 25.0 )
            {
                penalty = -10.0;
            }
            else
            {
                penalty = -4.0;
            }

            if ( ! in_shootable_sector
                 && Strategy::i().opponentType() == Strategy::Type_Gliders )
            {
                penalty -= 5.0;
            }

#ifdef DEBUG_PRINT
            dlog.addText( Logger::PLAN,
                          "(eval) __ penalty for maybe dangerous dribble %f", penalty );
#endif
        }
        else if ( first_action.type() == CooperativeAction::Pass )
        {
            if ( in_shootable_sector )
            {
                penalty = -0.001;
            }
            else
            {
                penalty = -1.0;
            }
#ifdef DEBUG_PRINT
            dlog.addText( Logger::PLAN,
                          "(eval) __ penalty for maybe dangerous pass %f", penalty );
#endif
        }
    }

    if ( first_action.type() == CooperativeAction::Dribble )
    {
        const bool keep_mode =  ( first_action.mode() == ActDribble::KEEP_DASHES
                                  || first_action.mode() == ActDribble::KEEP_KICK_DASHES
                                  || first_action.mode() == ActDribble::KEEP_KICK_TURN_DASHES
                                  || first_action.mode() == ActDribble::KEEP_TURN_KICK_DASHES
                                  || first_action.mode() == ActDribble::KEEP_COLLIDE_TURN_KICK_DASHES );


        const Vector2D ball_next = first_state.ball().pos() + first_action.firstBallVel();
        const double opponent_dist = get_opponent_dist( first_state, ball_next, 0, 3 );
        const double opponent_dist_thr = 3.0;
        if ( opponent_dist < opponent_dist_thr )
        {
            double value = 0.0;
            if ( keep_mode )
            {
                if ( in_shootable_sector )
                {
                    value = -0.01 * ( opponent_dist_thr - opponent_dist );
                }
                else
                {
                    //value = -5.0 * ( opponent_dist_thr - opponent_dist );
                    value = -10.0 * ( opponent_dist_thr - opponent_dist );
                }
            }
            else
            {
                if ( in_shootable_sector )
                {
                    value = -0.01 * ( opponent_dist_thr - opponent_dist );
                }
                else
                {
                    //value = -1.0 * ( opponent_dist_thr - opponent_dist );
                    value = -5.0 * ( opponent_dist_thr - opponent_dist );
                }
            }

            penalty += value;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::PLAN,
                          "(eval) __ penalty for dribble near opp %f (%f)", value, penalty );
#endif
        }

        if ( first_action.turnCount() > 0 )
        {
            double value = 0.0;
            if ( first_state.ball().pos().x > 25.0
                 && first_state.self().body().abs() < 90.0 )
            {
                double turn_angle = ( first_state.self().body() - AngleDeg( first_action.targetBodyAngle() ) ).abs();

                value += -1.0 * first_action.turnCount();
                value += -0.01 * turn_angle;
            }

            penalty += value;

#ifdef DEBUG_PRINT
            dlog.addText( Logger::PLAN,
                          "(eval) __ penalty for dribble turn %f (%f)", value, penalty );
#endif
        }
    }

    if ( first_action.type() == CooperativeAction::Pass
         && first_action.kickCount() > 1
         && next_state.ball().pos().dist2( ServerParam::i().theirTeamGoalPos() ) > std::pow( 15.0, 2 ) )
    {
        double dist = 1000.0;
        (void)first_state.getOpponentNearestTo( first_state.ball().pos(), 5, &dist );
        if ( dist < 3.0 )
        {
            double value = -first_action.kickCount() * ( 3.0 - dist ) * 5.0;
            penalty += value;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::PLAN,
                          "(eval) __ penalty for multikick pass %f (%f)",
                          value, penalty );
#endif

        }
    }

    //
    // goal dist
    //
    {
        double value = std::max( -0.05,
                                 -0.001 * next_state.ball().pos().dist( ServerParam::i().theirTeamGoalPos() ) );
        penalty += value;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PLAN,
                      "(eval) __ penalty for goal dist for first action %f (%f)",
                      value, penalty );
#endif
    }

#ifdef DEBUG_PRINT
    dlog.addText( Logger::PLAN,
                  "(eval) first action penalty %f", penalty );
#endif

    return penalty;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::evaluateImpl( const PredictState & first_state,
                                  const std::vector< ActionStatePair > & path ) const
{
    //
    // state evaluation
    //
    double result_value = getStateValue( path );

    //
    // add penalty values
    //
    {
        double length_penalty = getLengthPenalty( first_state, path );
        result_value += length_penalty;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PLAN,
                      "(eval) length penalty %f (%f)",
                      length_penalty, result_value );
#endif
    }
    //     {
    //             double opp_dist_penalty = getOpponentDistancePenalty( path );
    //             result_value += opp_dist_penalty;
    // #ifdef DEBUG_PRINT
    //             dlog.addText( Logger::PLAN,
    //                           "(eval) opponent dist penalty %f (%f)",
    //                           opp_dist_penalty, result_value );
    // #endif
    //     }
    {
        double action_type_penalty = getActionTypePenalty( first_state, path );
        result_value += action_type_penalty;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PLAN,
                      "(eval) action type penalty %f (%f)",
                      action_type_penalty, result_value );
#endif
    }

    //     if ( path.front().action().category() == CooperativeAction::Pass
    //          && path.front().action().kickCount() > 1 )
    //     {
    //         double decay = std::pow( 0.9, path.front().action().kickCount() - 1 );
    //         result_value *= decay;
    // #ifdef DEBUG_PRINT
    //         dlog.addText( Logger::PLAN,
    //                       "(eval) kick count decay %f (%f)",
    //                       decay, result_value );
    // #endif
    //     }


    {
        double first_action_penalty = path.front().penalty();
        result_value += first_action_penalty;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PLAN,
                      "(eval) first action penalty %f (%f)",
                      first_action_penalty, result_value );
#endif
    }

    if ( result_value < 0.0 )
    {
        return result_value;
    }

#if 0
    if ( Strategy::i().opponentType() == Strategy::Type_Gliders )
    {
        // no penalty
    }
    else
    {
        double penalty_rate = 1.0;
        const Vector2D ball_pos = path.back().state().ball().pos();
        if ( ball_pos.x < 48.0
                          && ball_pos.absY() > 23.0
             && ball_pos.dist2( ServerParam::i().theirTeamGoalPos() ) > std::pow( 20.0, 2 ) )
        {
            const double ball_move_dist2 = first_state.ball().pos().dist2( path.back().state().ball().pos() );
            penalty_rate = ( 1.0 - std::exp( -ball_move_dist2 / ( 2.0 * std::pow( 3.0, 2 ) ) ) * 0.5 );
            result_value *= penalty_rate;
        }
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PLAN,
                      "(eval) ball move dist penalty. rate = %f (%f)",
                      penalty_rate, result_value );
#endif
    }
#endif

    return result_value;
}

/*-------------------------------------------------------------------*/
/*!
 * \brief FieldEvaluator2016::getStateValue
 * \param path
 * \return
 */
double
FieldEvaluator2016::getStateValue( const std::vector< ActionStatePair > & path ) const
{
    const PredictState & state = path.back().state();
    const ServerParam & SP = ServerParam::i();

    //
    // ball is in opponent goal
    //
    if ( state.ball().pos().x > + ( SP.pitchHalfLength() - 0.1 )
         && state.ball().pos().absY() < SP.goalHalfWidth() + 2.0 )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PLAN,
                      "(eval) ball is in opponent goal %f", GOAL_VALUE );
#endif
        return GOAL_VALUE;
    }

    //
    // ball is in our goal
    //
    if ( state.ball().pos().x < - ( SP.pitchHalfLength() - 0.1 )
         && state.ball().pos().absY() < SP.goalHalfWidth() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PLAN,
                      "(eval) ball is in our goal" );
#endif

        return CONCEDED_VALUE;
    }


    //
    // out of pitch
    //
    if ( state.ball().pos().absX() > SP.pitchHalfLength()
         || state.ball().pos().absY() > SP.pitchHalfWidth() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PLAN,
                      "(eval) out of pitch" );
#endif
        return -50.0 + state.ball().pos().x;
    }


    //
    // opponent has ball
    //
    if ( state.ballHolder().side() != state.self().side() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PLAN,
                      "(eval) opponent has ball" );
#endif
        return -50.0 + state.ball().pos().x;
    }

    double result_value = 0.0;
    double value = 0.0;

    //
    // set ball position value
    //
    value = getBallPositionValue( path );
    result_value += value;
#ifdef DEBUG_PRINT
    dlog.addText( Logger::PLAN,
                  "(eval) ball pos (%f, %f)",
                  state.ball().pos().x, state.ball().pos().y );
    dlog.addText( Logger::PLAN,
                  "(eval) ball pos value (%f)", value );
#endif

    //
    // add bonus for goal, free situation near offside line
    //
    value = getShootChanceValue( path );
    result_value += value;
#ifdef DEBUG_PRINT
    dlog.addText( Logger::PLAN,
                  "(eval) bonus for shoot chance %f (%f)",
                  value, result_value );
#endif

    //
    // value for front space
    //
    //     value = getFrontSpaceValue( state, path );
    //     result_value += value;
    // #ifdef DEBUG_PRINT
    //     dlog.addText( Logger::PLAN,
    //                   "(eval) bonus for front space %f (%f)",
    //                   value, result_value );
    // #endif

    //
    // value for over offside line
    //
    //     value = getOverDefenseLineValue( path );
    //     result_value += value;
    // #ifdef DEBUG_PRINT
    //     dlog.addText( Logger::PLAN,
    //                   "(eval) bonus for over defense line %f (%f)",
    //                   value, result_value );
    // #endif

    //
    // add over attack line value
    //
    //     value = getOverAttackLineValue( state, path );
    //     result_value += value;
    // #ifdef DEBUG_PRINT
    //     dlog.addText( Logger::PLAN,
    //                   "(eval) bonus for over attack line %f (%f)",
    //                   value, result_value );
    // #endif

    //
    // add for the distance from opponent goal
    //
    value = getOpponentGoalDistanceValue( path );
    result_value += value;
#ifdef DEBUG_PRINT
    dlog.addText( Logger::PLAN,
                  "(eval) bonus for opponent goal dist %f (%f)",
                  value, result_value );
#endif

    //
    // add distance penalty when near our goal
    //
    value = getOurGoalDistanceValue( path );
    result_value += value;
#ifdef DEBUG_PRINT
    dlog.addText( Logger::PLAN,
                  "(eval) penalty for near our goal %f (%f)",
                  value, result_value );
#endif

    //
    // add congestion value
    //
    //     value = getCongestionValue( path );
    //     result_value += value;
    // #ifdef DEBUG_PRINT
    //     dlog.addText( Logger::PLAN,
    //                   "(eval) penalty for congestion %f (%f)",
    //                   value, result_value );
    // #endif

    //
    // add penalty center of field when our goalie not in penalty area
    //
    //     value = getBallPositionForOurGoalieValue( path );
    //     result_value += value;
    // #ifdef DEBUG_PRINT
    //     dlog.addText( Logger::PLAN,
    //                   "(eval) penalty for our goalie is in side area %f (%f)",
    //                   value, result_value );
    // #endif

    //
    // add pass count bonus
    //
    //     value = getPassCountValue( state, path );
    //     result_value += value;
    // #ifdef DEBUG_PRINT
    //     dlog.addText( Logger::PLAN,
    //                   "(eval) bonus for pass count %f (%f)",
    //                   value, result_value );
    // #endif

    return result_value;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getBallPositionValue( const std::vector< ActionStatePair > & path ) const
{
    const PredictState & state = path.back().state();
    double value = 0.0;
#if 0
    const Vector2D spot( ServerParam::i().pitchHalfLength() - ServerParam::DEFAULT_PENALTY_SPOT_DIST, 0.0 );
    const double dist_min = 25.0;
    const double dist_max = 30.0;

    const double spot_dist = spot.dist( state.ball().pos() );
    if ( spot_dist < dist_min )
    {
        value = ServerParam::i().pitchLength() * 1.5;
    }
    else if ( spot_dist < dist_max )
    {
        double rate = ( dist_max - spot_dist ) / ( dist_max - dist_min );
        value = ( state.ball().pos().x + ServerParam::i().pitchLength() ) * ( 1.0 - rate )
            + ServerParam::i().pitchLength() * 1.5 * rate;
    }
    else
    {
        value = state.ball().pos().x + ServerParam::i().pitchLength();
    }
#elif 1
    if ( state.ball().pos().x > 25.0 )
    {
        value = ServerParam::i().pitchHalfLength() + ServerParam::i().pitchLength();

        // penalty for the distance from corners
        double d2
            = std::pow( state.ball().pos().x - ServerParam::i().pitchHalfLength(), 2 )
            + std::pow( state.ball().pos().absY() - ServerParam::i().pitchHalfWidth(), 2 );
        if ( d2 < std::pow( 15.0, 2 ) )
        {
            //double dec = std::exp( -d2 / ( 2.0 * std::pow( 10.0, 2 ) ) ) * 30.0;
            double dec = std::exp( -d2 / ( 2.0 * std::pow( 10.0, 2 ) ) ) * 5.0;
            value -= dec;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::PLAN,
                          "(eval) corner penalty %f (%f)", -dec, value );
#endif
        }
    }
    else if ( state.ball().pos().x > 20.0 )
    {
        double rate = ( 25.0 - state.ball().pos().x ) / 5.0;
        value = ( state.ball().pos().x + ServerParam::i().pitchLength() ) * rate
            + ( ServerParam::i().pitchHalfLength() + ServerParam::i().pitchLength() ) * ( 1.0 - rate );
    }
    else
    {
        value = state.ball().pos().x + ServerParam::i().pitchLength();
    }
#else
    value = state.ball().pos().x + ServerParam::i().pitchLength();
#endif

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getShootChanceValue( const std::vector< ActionStatePair > & path ) const
{
    const PredictState & state = path.back().state();

    double value = 0.0;
    if ( ShootSimulator::can_shoot_from( state.ballHolder().unum() == state.self().unum(),
                                         state.ball().pos(),
                                         state.getPlayers( new OpponentOrUnknownPlayerPredicate( state.self().side() ) ),
                                         VALID_PLAYER_THRESHOLD ) )
    {
        value = SHOOT_VALUE;
    }

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getFrontSpaceValue( const std::vector< ActionStatePair > & path ) const
{
    const PredictState & state = path.back().state();

    const Vector2D pos = state.ballHolder().pos();

    double free_angle = 90.0;
    for ( PredictPlayerObject::Cont::const_iterator o = state.theirPlayers().begin(), end = state.theirPlayers().end();
          o != end;
          ++o )
    {
        if ( (*o)->goalie() ) continue;

        double dist = std::max( 0.1, (*o)->pos().dist( pos ) );
        double angle = ( (*o)->pos() - pos ).th().abs();
        angle -= std::fabs( AngleDeg::asin_deg( (*o)->playerTypePtr()->kickableArea() / dist ) );
        if ( angle < free_angle )
        {
            free_angle = angle;
        }
    }

    const double goal_line_dist
        = bound( 0.0,
                 ( ServerParam::i().theirTeamGoalLineX() - 10.0 ) - pos.x,
                 30.0 );

    const double area = goal_line_dist * AngleDeg( free_angle ).sin();

    return area * 3.0;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getOverDefenseLineValue( const std::vector< ActionStatePair > & path ) const
{
    const PredictState & state = path.back().state();

    double value = 0.0;

    if ( state.ball().pos().x > 0.0
         || state.ball().pos().x > state.theirDefensePlayerLineX() )
    {
        const ServerParam & SP = ServerParam::i();

        value = 50.5 - std::fabs( state.ball().pos().x - 45.0 );

        // if ( path.size() == 1 &&
        if ( state.ball().pos().x > SP.pitchHalfLength() - 10.0 )
        {
            value -= std::fabs( state.ball().pos().x - ( SP.pitchHalfLength() - 10.0 ) );
        }

        if ( value < 0.0 )
        {
            value = 0.0;
        }
    }

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getOpponentGoalDistanceValue( const std::vector< ActionStatePair > & path ) const
{
    const PredictState & state = path.back().state();

    double value = 0.0;

    const double their_goal_dist = ServerParam::i().theirTeamGoalPos().dist( state.ball().pos() );
    if ( their_goal_dist <= 30.0 )
    {
        value = ( 30.0 - their_goal_dist ) * THEIR_GOAL_DIST_RATE;
    }

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getOurGoalDistanceValue( const std::vector< ActionStatePair > & path ) const
{
    const PredictState & state = path.back().state();

    double value = 0.0;

    const double our_goal_dist = ServerParam::i().ourTeamGoalPos().dist( state.ball().pos() );
    if ( our_goal_dist <= 30.0 )
    {
        value = ( 30.0 - our_goal_dist ) * -3.28763719460485;
    }

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getLengthPenalty( const PredictState & first_state,
                                      const std::vector< ActionStatePair > & path ) const
{
    (void)first_state;

    const PredictState & last_state = path.back().state();

    //
    // penalty by path length
    //
    double path_length_penalty = 0.0;

    if ( path.size() >= 2 ) path_length_penalty -= 3.0;
    if ( path.size() >= 3 ) path_length_penalty -= 4.0;
    if ( path.size() >= 4 ) path_length_penalty -= 5.0;
    if ( path.size() >= 5 ) path_length_penalty -= 6.0;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::PLAN,
                  "(eval) __ penalty for path length %f: length=%d",
                  path_length_penalty,
                  static_cast< int >( path.size() ) );
#endif

    //
    // penalty by spent time
    //
    const double spent_time_penalty = -0.15 * last_state.spentTime();
    //const double spent_time_penalty = -0.3 * last_state.spentTime();


#ifdef DEBUG_PRINT
    dlog.addText( Logger::PLAN,
                  "(eval) __ penalty for spent time %f: time=%lu",
                  spent_time_penalty,
                  last_state.spentTime() );
#endif

    return path_length_penalty + spent_time_penalty;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getOpponentDistancePenalty( const std::vector< ActionStatePair > & path ) const
{
    //const double PATH_OPPENENT_DIST_PENALTY = -1.0e+4;
    const double PATH_OPPENENT_DIST_PENALTY = -1.0;

    const double PATH_OPPENENT_DIST_THRESHOLD = 5.0;
    const double PATH_OPPENENT_DIST_THRESHOLD_1ST = PATH_OPPENENT_DIST_THRESHOLD;
    const double PATH_OPPENENT_DIST_THRESHOLD_2ND = PATH_OPPENENT_DIST_THRESHOLD;
    // const double OPPONENT_ADDITIONAL_CHASE_RATE = 0.2;

    const PredictState & state = path.back().state();

    double penalty = 0.0;

    const int size = path.size();
    for ( int i = 1; i < size; ++i )
    {
        if ( state.spentTime() == 0 )
        {
            continue;
        }

        // if ( ( state.ball().pos() - ServerParam::i().theirTeamGoalPos() ).r() < 30.0
        //      || state.ball().pos().x > ServerParam::i().theirPenaltyAreaLineX()  )
        // {
        //     continue;
        // }

        //
        // get free radius
        //
        const int opponent_additional_chase_time = path[i - 1].action().durationTime();

        const double r = get_opponent_dist( path[i].state(),
                                            path[i].state().ball().pos(),
                                            opponent_additional_chase_time );

        double thr = 0.0;
        switch( i ) {
        case 1:
            thr = PATH_OPPENENT_DIST_THRESHOLD_1ST;
            break;

        case 2:
            thr = PATH_OPPENENT_DIST_THRESHOLD_2ND;
            break;

        default:
            thr = PATH_OPPENENT_DIST_THRESHOLD;
            break;
        }

        if ( r < thr )
        {

            penalty += ( PATH_OPPENENT_DIST_PENALTY * ( thr - r ) );
            // penalty += ( PATH_OPPENENT_DIST_PENALTY * ( thr - r ) )
            //            / ( path.size() - 1 );
        }
    }

    return penalty;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getActionTypePenalty( const PredictState & first_state,
                                          const std::vector< ActionStatePair > & path ) const
{
    double penalty = 0.0;

    const CooperativeAction & first_action = path.front().action();
    const PredictState & next_state = path.front().state();

    if ( path.size() >= 2 )
    {
        double value = 0.0;
        Vector2D prev_move = ( path.front().state().ball().pos() - first_state.ball().pos() );
        AngleDeg prev_move_angle = prev_move.th();
        for ( size_t i = 1; i < path.size(); ++i )
        {
            Vector2D next_move = ( path[i].state().ball().pos() - path[i-1].state().ball().pos() );
            AngleDeg next_move_angle = next_move.th();
            if ( path[i-1].action().type() == CooperativeAction::Dribble )
            {
                double angle_diff = ( prev_move_angle - next_move_angle ).abs();
                if ( angle_diff > 120.0 )
                {
                    value -= ( angle_diff - 120.0 );
                }
            }
            prev_move = next_move;
            prev_move_angle = next_move_angle;
        }
        penalty += value;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PLAN,
                      "(eval) __ penalty for move angle %f (%f)",
                      value, penalty );
#endif
    }

    if ( path.back().action().type() == CooperativeAction::Shoot )
    {
        const PredictState & shoot_state = ( path.size() == 1
                                             ? first_state
                                             : path[path.size()-2].state() );
        //double value = - shoot_state.ball().pos().dist( best_shoot_spot );
        double value = -0.5 * shoot_state.ball().pos().dist( Vector2D( 52.5, 0 ) );
        penalty += value;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PLAN,
                      "(eval) __ penalty for shoot spot distance %f (%f)",
                      value, penalty );
#endif
        if ( path.size() >= 2 )
        {
            double opponent_dist = get_opponent_dist( first_state,
                                                      path.front().state().ball().pos(),
                                                      0 );
            value = -1.0 * std::exp( -std::pow( opponent_dist, 2 ) / ( 2.0 * std::pow( 1.0, 2 ) ) );
            penalty += value;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::PLAN,
                          "(eval) __ penalty for opponent dist for first action %f (%f)",
                          value, penalty );
#endif
        }
    }
    else
    {
        if ( first_action.type() == CooperativeAction::Dribble )
        {
            // if ( next_state.ball().pos().x < next_state.offsideLineX() - 10.0
            //      && next_state.ball().pos().x < 36.0 )
            if ( next_state.ball().pos().x < 0.0
                 || Strategy::i().opponentType() == Strategy::Type_Gliders )
            {
                double opponent_dist = get_opponent_dist( first_state,
                                                          next_state.ball().pos(),
                                                          0 );
                if ( opponent_dist < 5.0 )
                {
                    double value = -5.0 * std::exp( -std::pow( opponent_dist, 2 ) / ( 2.0 * std::pow( 2.0, 2 ) ) );
                    penalty += value;
#ifdef DEBUG_PRINT
                    dlog.addText( Logger::PLAN,
                                  "(eval) __ penalty for opponent dist after dribble %f (%f)",
                                  value, penalty );
#endif
                }
            }
        }
    }

    return penalty;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getBallPositionForOurGoalieValue( const std::vector< ActionStatePair > & path ) const
{
    const PredictState & state = path.back().state();

    double value = 0.0;

    const AbstractPlayerObject * our_goalie = state.getOurGoalie();
    if ( our_goalie )
    {
        const bool our_goalie_is_in_side_area
            = ( our_goalie->pos().absY() > ServerParam::i().penaltyAreaHalfWidth() - 5.0
                && our_goalie->posCount() < VALID_PLAYER_THRESHOLD
                && our_goalie->pos().x < -20.0
                && state.ball().pos().x < -20.0 );

        if ( our_goalie_is_in_side_area )
        {
            double side_line_dist = 0.0;

            if ( our_goalie->pos().y > 0.0 )
            {
                side_line_dist = ( ServerParam::i().penaltyAreaHalfWidth() - state.ball().pos().y );
            }
            else
            {
                side_line_dist = ( state.ball().pos().y + ServerParam::i().penaltyAreaHalfWidth() );
            }

            value = -30.0 + side_line_dist * 10.0;
        }
    }

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getOverAttackLineValue( const std::vector< ActionStatePair > & path ) const
{
    const PredictState & state = path.back().state();

    double value = 0.0;

    if ( state.ball().pos().x >= ATTACK_LINE_X_THRESHOLD )
    {
        value = ( state.ball().pos().x - ATTACK_LINE_X_THRESHOLD ) * OVER_ATTACK_LINE_RATE;
    }

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getCongestionValue( const std::vector< ActionStatePair > & path ) const
{
    const PredictState & state = path.back().state();

    double value = 0.0;

    // if ( path.size() == 1
    //      && path.front().action().type() == CooperativeAction::Dribble )
    // {
    //     value = -70.0 * FieldAnalyzer::get_congestion( state, state.ball().pos() );
    // }
    // else
    {
        value = -50.0 * FieldAnalyzer::get_congestion( state, state.ball().pos() );
    }

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluator2016::getPassCountValue( const std::vector< ActionStatePair > & path ) const
{
    const PredictState & state = path.back().state();

    const SimplePassChecker pass_checker;
    const int pass_count = FieldAnalyzer::get_pass_count( state,
                                                          pass_checker,
                                                          PASS_BALL_FIRST_SPEED,
                                                          3 );

    return std::min( pass_count, 3 ) * 20.0;
}
