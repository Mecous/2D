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

#include "field_evaluator_svmrank.h"

#include "strategy.h"

#include "action_state_pair.h"
#include "cooperative_action.h"
#include "predict_state.h"

#include "act_pass.h"

#include "field_analyzer.h"
#include "shoot_simulator.h"
#include "simple_pass_checker.h"

#include "options.h"

#include <rcsc/player/player_evaluator.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/math_util.h>

#include <svmrank/svm_struct_api.h>

#include <sstream>

using namespace rcsc;

namespace {

enum Features {
    FIRST_BALL_X = 0, // 1
    FIRST_BALL_Y, // 2
    FIRST_BALL_ABSY, // 3
    LAST_BALL_X, // 4
    LAST_BALL_Y, // 5
    LAST_BALL_ABSY, // 6
    OFFSIDE_LINE_X, // 7
    THEIR_DEFENSE_LINE_X, // 8
    FIRST_DRIBBLE, // 9
    FIRST_DRIBBLE_DANGEROUS, // 10
    FIRST_DRIBBLE_MAYBE_DANGEROUS, // 11
    FIRST_PASS_DANGEROUS, // 12
    FIRST_PASS_MAYBE_DANGEROUS, // 13
    FIRST_PASS_DIRECT, // 14
    FIRST_PASS_LEADING, // 15
    FIRST_PASS_THROUGH, // 16
    FIRST_PASS_CROSS, // 17
    FIRST_PASS_RECEIVER_MOVE_DIST, // 18
    FIRST_PASS_RECEIVER_MOVE_ANGLE_ABS, // 19
    SECOND_STATE_SHOOTABLE_SECTOR, // 20
    FIRST_PASS_KICK_COUNT_PENALTY, // 21
    FIRST_DRIBBLE_OPPONENT_DIST_IN_OUR_FIELD, // 22
    SECOND_STATE_GOAL_DIST, // 23
    BALL_IN_THEIR_GOAL, // 24
    BALL_IN_OUR_GOAL, // 25
    LAST_BALL_DIST_FROM_PENALTY_SPOT, // 26
    LAST_STATE_SHOOTABLE, // 27
    LAST_STATE_CONGESTION, // 28
    SHOOT_DIST, // 29
    FIRST_STATE_OPPONENT_DIST_FOR_SHOOT, // 30
    SEQUENCE_SIZE, // 31
    SPENT_TIME, // 32

    // DUMMY_33,
    // DUMMY_34,
    // DUMMY_35,
    // DUMMY_36,
    // DUMMY_37,
    // DUMMY_38,
    // DUMMY_39,
    // DUMMY_41,
    // DUMMY_42,
    // DUMMY_43,
    // DUMMY_44,
    // DUMMY_45,
    // DUMMY_46,
    // DUMMY_47,
    // DUMMY_48,
    // DUMMY_49,
    // DUMMY_50,
    // DUMMY_51,
    // DUMMY_52,
    // DUMMY_53,
    // DUMMY_54,
    // DUMMY_55,
    // DUMMY_56,
    // DUMMY_57,
    // DUMMY_58,
    // DUMMY_59,
    // DUMMY_60,
    // DUMMY_61,
    // DUMMY_62,
    // DUMMY_63,
    // DUMMY_64,
    // DUMMY_65,
    // DUMMY_66,
    // DUMMY_67,
    // DUMMY_68,
    // DUMMY_69,
    // DUMMY_70,
    // DUMMY_71,
    // DUMMY_72,
    // DUMMY_73,
    // DUMMY_74,
    // DUMMY_75,
    // DUMMY_76,
    // DUMMY_77,
    // DUMMY_78,
    // DUMMY_79,
    // DUMMY_80,
    // DUMMY_81,
    // DUMMY_82,
    // DUMMY_83,
    // DUMMY_84,
    // DUMMY_85,
    // DUMMY_86,
    // DUMMY_87,
    // DUMMY_88,
    // DUMMY_89,
    // DUMMY_90,
    // DUMMY_91,
    // DUMMY_92,
    // DUMMY_93,
    // DUMMY_94,
    // DUMMY_95,
    // DUMMY_96,
    // DUMMY_97,
    // DUMMY_98,
    // DUMMY_99,

    FEATURE_SIZE,
};

const Sector2D shootable_sector( Vector2D( 58.0, 0.0 ),
                                 0.0, 20.0,
                                 137.5, -137.5 );
const int VALID_PLAYER_THRESHOLD = 8;

/*-------------------------------------------------------------------*/
/*!

 */
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

}


/*-------------------------------------------------------------------*/
/*!

 */
FieldEvaluatorSVMRank::FieldEvaluatorSVMRank()
{
    M_model = svmrank::read_struct_model( Options::i().svmrankEvaluatorModel().c_str(),
                                          &M_learn_param );
    if ( M_model.svm_model == NULL )
    {
        std::cerr << "ERROR: failed to read svmrank model file ["
                  << Options::i().svmrankEvaluatorModel() << "]" << std::endl;
        return;
    }

    if ( M_model.svm_model->kernel_parm.kernel_type == svmrank::LINEAR )
    {
        // Linear Kernel: compute weight vector
        svmrank::add_weight_vector_to_linear_model( M_model.svm_model );
        M_model.w = M_model.svm_model->lin_weights;
    }

}

/*-------------------------------------------------------------------*/
/*!

 */
FieldEvaluatorSVMRank::~FieldEvaluatorSVMRank()
{
    if ( M_model.svm_model != NULL )
    {
        svmrank::free_struct_model( M_model );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FieldEvaluatorSVMRank::isValid() const
{
    if ( M_model.svm_model == NULL )
    {
        return false;
    }
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluatorSVMRank::getFirstActionPenalty( const PredictState & /*first_state*/,
                                              const ActionStatePair & /*first_pair*/ )
{
    return 0.0;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
FieldEvaluatorSVMRank::evaluate( const PredictState & first_state,
                                 const std::vector< ActionStatePair > & path )
{
    if ( path.empty() )
    {
        return 0.0;
    }

    std::vector< double > features;
    features.resize( FEATURE_SIZE, 0.0 );

    createFeatureVector( first_state, path, features );

    svmrank::WORD words[FEATURE_SIZE + 1];
    for ( size_t i = 0; i < FEATURE_SIZE; ++i )
    {
        words[i].wnum = (int32_t)(i + 1);
        words[i].weight = (float)features[i];
    }
    words[FEATURE_SIZE].wnum = 0;
    words[FEATURE_SIZE].weight = 0;

    svmrank::DOC doc;

    doc.docnum = 1;
    doc.queryid = 1;
    doc.costfactor = 1.0;
    doc.slackid = 0;
    doc.kernelid = -1;
    doc.fvec = svmrank::create_svector( words, NULL, 1.0 );

    if ( ! doc.fvec )
    {
        return 0.0;
    }

    double value = svmrank::classify_example( M_model.svm_model, &doc );

    svmrank::free_svector( doc.fvec );

    if ( path.size() == 1
         && path.front().action().type() == CooperativeAction::Hold
         && ! std::strcmp( path.front().action().description(), "defaultHold" ) )
    {
        // no data to be written
    }
    else
    {
#if 0
        debugWriteFeatures( features );
#endif
        writeRankData( first_state.self().unum(), first_state.currentTime(),
                       value, features );
    }

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldEvaluatorSVMRank::writeRankData( const int unum,
                                      const GameTime & current,
                                      const double value,
                                      const std::vector< double > & features )
{
    static std::string s_query_id = "";
    static GameTime s_time( 0, 0 );

    if ( current.cycle() >= 10000
         || current.stopped() > 0 )
    {
        return;
    }

    if ( current != s_time )
    {
        s_time = current;

        // qid: hour + minutes + unum + cycle
        //  example 2015 07 08 1410 02 0200

        char time_str[64];

        time_t t = std::time( 0 );
        tm * tm = std::localtime( &t );
        if ( tm )
        {
            std::strftime( time_str, sizeof( time_str ), "%Y%m%d%H%M", tm );
        }

        char unum_cycle[16];
        snprintf( unum_cycle, sizeof( unum_cycle ),
                  "%02d%04ld", unum, current.cycle() );

        s_query_id = time_str;
        s_query_id += unum_cycle;

        dlog.addText( Logger::PLAN,
                      "(eval) # query %s", s_query_id.c_str() );
    }


    std::ostringstream ostr;
    ostr << value << " qid:" << s_query_id;
    for ( size_t i = 0; i < features.size(); ++i )
    {
        ostr << ' '
             << i + 1 << ':' << features[i];
    }

    dlog.addText( Logger::PLAN,
                  "(rank) %s", ostr.str().c_str() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldEvaluatorSVMRank::createFeatureVector( const PredictState & first_state,
                                            const std::vector< ActionStatePair > & path,
                                            std::vector< double > & features )
{
    features[ FIRST_BALL_X ] = first_state.ball().pos().x;
    features[ FIRST_BALL_Y ] = first_state.ball().pos().y;
    features[ FIRST_BALL_ABSY ] = first_state.ball().pos().absY();
    features[ LAST_BALL_X ] = path.back().state().ball().pos().x;
    features[ LAST_BALL_Y ] = path.back().state().ball().pos().y;
    features[ LAST_BALL_ABSY ] = path.back().state().ball().pos().absY();
    features[ OFFSIDE_LINE_X ] = first_state.offsideLineX();
    features[ THEIR_DEFENSE_LINE_X ] = first_state.theirDefensePlayerLineX();

    setFirstActionFeatures( first_state, path, features );
    setLastActionFeatures( first_state, path, features );
    setLastStateFeatures( path, features );
    setSequenceFeatures( path, features );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldEvaluatorSVMRank::setFirstActionFeatures( const PredictState & first_state,
                                               const std::vector< ActionStatePair > & path,
                                               std::vector< double > & features )
{
    const CooperativeAction & first_action = path.front().action();
    const PredictState & next_state = path.front().state();
    const bool in_shootable_sector = shootable_sector.contains( next_state.ball().pos() );
    //const double next_goal_dist = next_state.ball().pos().dist( ServerParam::i().theirTeamGoalPos() );

    if ( first_action.type() == CooperativeAction::Dribble
         || first_action.type() == CooperativeAction::Hold )
    {
        features[ FIRST_DRIBBLE ] = 1.0;

        if ( first_action.safetyLevel() == CooperativeAction::Dangerous )
        {
            features[ FIRST_DRIBBLE_DANGEROUS ] = 1.0;
        }
        else if ( first_action.safetyLevel() == CooperativeAction::MaybeDangerous )
        {
            features[ FIRST_DRIBBLE_MAYBE_DANGEROUS ] = 1.0;
        }
    }

    //
    //
    //
    if ( first_action.type() == CooperativeAction::Pass )
    {
        if ( first_action.mode() == ActPass::THROUGH ) features[ FIRST_PASS_THROUGH ] = 1.0;
        if ( first_action.mode() == ActPass::LEADING ) features[ FIRST_PASS_LEADING ] = 1.0;
        if ( first_action.mode() == ActPass::DIRECT ) features[ FIRST_PASS_DIRECT ] = 1.0;
        if ( first_action.mode() == ActPass::CROSS ) features[ FIRST_PASS_CROSS ] = 1.0;

        if ( first_action.safetyLevel() == CooperativeAction::Dangerous )
        {
            features[ FIRST_PASS_DANGEROUS ] = 1.0;
        }
        else if ( first_action.safetyLevel() == CooperativeAction::MaybeDangerous )
        {
            features[ FIRST_PASS_MAYBE_DANGEROUS ] = 1.0;
        }

        const AbstractPlayerObject * receiver = first_state.ourPlayer( first_action.targetPlayerUnum() );
        if ( receiver )
        {
            features[ FIRST_PASS_RECEIVER_MOVE_DIST ] = next_state.ball().pos().dist( receiver->pos() );
            features[ FIRST_PASS_RECEIVER_MOVE_ANGLE_ABS ] = ( next_state.ball().pos() - receiver->pos() ).th().abs();
        }
        else
        {
            features[ FIRST_PASS_RECEIVER_MOVE_DIST ] = 1000.0;
            features[ FIRST_PASS_RECEIVER_MOVE_ANGLE_ABS ] = 180.0;
        }
    }

    if ( in_shootable_sector )
    {
        features[ SECOND_STATE_SHOOTABLE_SECTOR ] = 1.0;
    }

    //
    //
    //
    if ( first_action.type() == CooperativeAction::Dribble
         && next_state.ball().pos().x < 0.0 )
    {
        double opponent_dist = get_opponent_dist( first_state,
                                                  next_state.ball().pos(),
                                                  0 );
        if ( opponent_dist < 5.0 )
        {
            double f = std::exp( -std::pow( opponent_dist, 2 )
                                 / ( 2.0 * std::pow( 2.0, 2 ) ) );
            features[ FIRST_DRIBBLE_OPPONENT_DIST_IN_OUR_FIELD ] = f;
        }
    }

    //
    //
    //
    if ( first_action.type() == CooperativeAction::Pass
         && first_action.kickCount() > 1
         && next_state.ball().pos().dist2( ServerParam::i().theirTeamGoalPos() ) > std::pow( 15.0, 2 ) )
    {
        double dist = 1000.0;
        (void)first_state.getOpponentNearestTo( first_state.ball().pos(), 5, &dist );
        features[ FIRST_PASS_KICK_COUNT_PENALTY ] = std::exp( -std::pow( dist, 2 ) / ( 2.0 * std::pow( 3.0, 2 ) ) );
    }

    //
    //
    //
    features[ SECOND_STATE_GOAL_DIST ]
        = next_state.ball().pos().dist( ServerParam::i().theirTeamGoalPos() );
}


/*-------------------------------------------------------------------*/
/*!

 */
void
FieldEvaluatorSVMRank::setLastStateFeatures( const std::vector< ActionStatePair > & path,
                                             std::vector< double > & features )
{
    const PredictState & state = path.back().state();
    const ServerParam & SP = ServerParam::i();

    //
    // ball is in opponent goal
    //
    if ( state.ball().pos().x > + ( SP.pitchHalfLength() - 0.1 )
         && state.ball().pos().absY() < SP.goalHalfWidth() + 2.0 )
    {
        features[ BALL_IN_THEIR_GOAL ] = 1.0;
    }

    //
    // ball is in our goal
    //
    if ( state.ball().pos().x < - ( SP.pitchHalfLength() - 0.1 )
         && state.ball().pos().absY() < SP.goalHalfWidth() )
    {
        features[ BALL_IN_OUR_GOAL ] = 1.0;
    }

    //
    // distance from penalty spot
    //
    {
        const Vector2D spot( SP.pitchHalfLength()
                             - ServerParam::DEFAULT_PENALTY_SPOT_DIST,
                             0.0 );
        const double spot_dist = spot.dist( state.ball().pos() );

        features[ LAST_BALL_DIST_FROM_PENALTY_SPOT ] = spot_dist;
    }

    //
    // shoot chance
    //
    if ( ShootSimulator::can_shoot_from( state.ballHolder().unum() == state.self().unum(),
                                         state.ball().pos(),
                                         state.getPlayers( new OpponentOrUnknownPlayerPredicate( state.self().side() ) ),
                                         VALID_PLAYER_THRESHOLD ) )
    {
        features[ LAST_STATE_SHOOTABLE ] = 1.0;
    }


    //
    // congestion
    //
    features[ LAST_STATE_CONGESTION ]
        = FieldAnalyzer::get_congestion( state,
                                         path.back().state().ball().pos() );


}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldEvaluatorSVMRank::setSequenceFeatures( const std::vector< ActionStatePair > & path,
                                            std::vector< double > & features )
{
    features[ SEQUENCE_SIZE ] = path.size();
    features[ SPENT_TIME ] = path.back().state().spentTime();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldEvaluatorSVMRank::setLastActionFeatures( const PredictState & first_state,
                                              const std::vector< ActionStatePair > & path,
                                              std::vector< double > & features )
{
    if ( path.back().action().type() == CooperativeAction::Shoot )
    {
        const PredictState & shoot_state = ( path.size() == 1
                                             ? first_state
                                             : path[path.size()-2].state() );
        double shoot_dist = shoot_state.ball().pos().dist( Vector2D( 52.5, 0 ) );
        features[ SHOOT_DIST ] = std::max( 0.0, 25.0 - shoot_dist );

        if ( path.size() >= 2 )
        {
            double opponent_dist = get_opponent_dist( first_state,
                                                      first_state.ball().pos(),
                                                      0 );
            double f = std::exp( -std::pow( opponent_dist, 2 )
                                 / ( 2.0 * std::pow( 1.0, 2 ) ) );
            features[ FIRST_STATE_OPPONENT_DIST_FOR_SHOOT ] = f;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
FieldEvaluatorSVMRank::debugWriteFeatures( const std::vector< double > & features )
{
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_BALL_X) %f", FIRST_BALL_X, features[ FIRST_BALL_X ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_BALL_Y) %f", FIRST_BALL_Y, features[ FIRST_BALL_Y ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_BALL_ABSY) %f", FIRST_BALL_ABSY,  features[ FIRST_BALL_ABSY ] );
    dlog.addText( Logger::PLAN, "(eval) %d (LAST_BALL_X) %f", LAST_BALL_X, features[ LAST_BALL_X ] );
    dlog.addText( Logger::PLAN, "(eval) %d (LAST_BALL_Y) %f", LAST_BALL_Y, features[ LAST_BALL_Y ] );
    dlog.addText( Logger::PLAN, "(eval) %d (LAST_BALL_ABSY) %f", LAST_BALL_ABSY, features[ LAST_BALL_ABSY ] );
    dlog.addText( Logger::PLAN, "(eval) %d (OFFSIDE_LINE_X) %f", OFFSIDE_LINE_X, features[ OFFSIDE_LINE_X ] );
    dlog.addText( Logger::PLAN, "(eval) %d (THEIR_DEFENSE_LINE_X) %f", THEIR_DEFENSE_LINE_X, features[ THEIR_DEFENSE_LINE_X ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_DRIBBLE) %f", FIRST_DRIBBLE, features[ FIRST_DRIBBLE ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_DRIBBLE_DANGEROUS) %f", FIRST_DRIBBLE_DANGEROUS, features[ FIRST_DRIBBLE_DANGEROUS ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_DRIBBLE_MAYBE_DANGEROUS) %f", FIRST_DRIBBLE_MAYBE_DANGEROUS, features[ FIRST_DRIBBLE_MAYBE_DANGEROUS ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_PASS_DANGEROUS) %f", FIRST_PASS_DANGEROUS, features[ FIRST_PASS_DANGEROUS ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_PASS_MAYBE_DANGEROUS) %f", FIRST_PASS_MAYBE_DANGEROUS, features[ FIRST_PASS_MAYBE_DANGEROUS ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_PASS_DIRECT) %f", FIRST_PASS_DIRECT, features[ FIRST_PASS_DIRECT ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_PASS_LEADING) %f", FIRST_PASS_LEADING, features[ FIRST_PASS_LEADING ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_PASS_THROUGH) %f", FIRST_PASS_THROUGH, features[ FIRST_PASS_THROUGH ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_PASS_CROSS) %f", FIRST_PASS_CROSS, features[ FIRST_PASS_CROSS ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_PASS_RECEIVER_MOVE_DIST) %f", FIRST_PASS_RECEIVER_MOVE_DIST, features[ FIRST_PASS_RECEIVER_MOVE_DIST ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_PASS_RECEIVER_MOVE_ANGLE_ABS) %f", FIRST_PASS_RECEIVER_MOVE_ANGLE_ABS, features[ FIRST_PASS_RECEIVER_MOVE_ANGLE_ABS ] );
    dlog.addText( Logger::PLAN, "(eval) %d (SECOND_STATE_SHOOTABLE_SECTOR) %f", SECOND_STATE_SHOOTABLE_SECTOR, features[ SECOND_STATE_SHOOTABLE_SECTOR ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_PASS_KICK_COUNT_PENALTY) %f", FIRST_PASS_KICK_COUNT_PENALTY, features[ FIRST_PASS_KICK_COUNT_PENALTY ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_DRIBBLE_OPPONENT_DIST_IN_OUR_FIELD) %f", FIRST_DRIBBLE_OPPONENT_DIST_IN_OUR_FIELD, features[ FIRST_DRIBBLE_OPPONENT_DIST_IN_OUR_FIELD ] );
    dlog.addText( Logger::PLAN, "(eval) %d (SECOND_STATE_GOAL_DIST) %f", SECOND_STATE_GOAL_DIST, features[ SECOND_STATE_GOAL_DIST ] );
    dlog.addText( Logger::PLAN, "(eval) %d (BALL_IN_THEIR_GOAL) %f", BALL_IN_THEIR_GOAL, features[ BALL_IN_THEIR_GOAL ] );
    dlog.addText( Logger::PLAN, "(eval) %d (BALL_IN_OUR_GOAL) %f", BALL_IN_OUR_GOAL, features[ BALL_IN_OUR_GOAL ] );
    dlog.addText( Logger::PLAN, "(eval) %d (LAST_BALL_DIST_FROM_PENALTY_SPOT) %f", LAST_BALL_DIST_FROM_PENALTY_SPOT, features[ LAST_BALL_DIST_FROM_PENALTY_SPOT ] );
    dlog.addText( Logger::PLAN, "(eval) %d (LAST_STATE_SHOOTABLE) %f", LAST_STATE_SHOOTABLE, features[ LAST_STATE_SHOOTABLE ] );
    dlog.addText( Logger::PLAN, "(eval) %d (LAST_STATE_CONGESTION) %f", LAST_STATE_CONGESTION, features[ LAST_STATE_CONGESTION ] );
    dlog.addText( Logger::PLAN, "(eval) %d (SHOOT_DIST) %f", SHOOT_DIST, features[ SHOOT_DIST ] );
    dlog.addText( Logger::PLAN, "(eval) %d (FIRST_STATE_OPPONENT_DIST_FOR_SHOOT) %f", FIRST_STATE_OPPONENT_DIST_FOR_SHOOT, features[ FIRST_STATE_OPPONENT_DIST_FOR_SHOOT ] );
    dlog.addText( Logger::PLAN, "(eval) %d (SEQUENCE_SIZE) %f", SEQUENCE_SIZE, features[ SEQUENCE_SIZE ] );
    dlog.addText( Logger::PLAN, "(eval) %d (SPENT_TIME) %f", SPENT_TIME, features[ SPENT_TIME ] );
}
