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

#include "bhv_tactical_tackle.h"

#include "generator_tackle.h"
#include "field_analyzer.h"
#include "strategy.h"

#include "bhv_tackle_intercept.h"

#include <rcsc/action/body_turn_to_point.h>
#include <rcsc/action/neck_turn_to_point.h>

#include <rcsc/player/intercept_table.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/world_model.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/geom/matrix_2d.h>

using namespace rcsc;

namespace {

/*-------------------------------------------------------------------*/
/*!

 */
bool
is_foul_situation( const PlayerAgent * agent )
{
    if ( agent->config().version() < 14.0 )
    {
        return false;
    }

    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    if ( wm.self().card() != NO_CARD )
    {
        // TODO: consider very dangerous situation
        return false;
    }

    // in our penalty area
    if ( wm.ball().pos().x < SP.ourPenaltyAreaLineX() + 0.5
         && wm.ball().pos().absY() < SP.penaltyAreaHalfWidth() + 0.5 )
    {
        // TODO: consider dangerous situation
        return false;
    }

    if ( wm.self().tackleProbability() > 0.9 )
    {
        return false;
    }

    if ( wm.self().tackleProbability() > wm.self().foulProbability() )
    {
        return false;
    }

#if 0
    const AbstractPlayerObject * ball_holder = wm.interceptTable()->fastestOpponent();
    if ( ball_holder
         && ball_holder->pos().dist2( SP.ourTeamGoalPos() ) > wm.self().pos().dist2( SP.ourTeamGoalPos() ) )
    {
        return false;
    }

    if ( wm.ball().pos().x > 0.0 )
    {
        return false;
    }
#endif

    return true;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TacticalTackle::execute( PlayerAgent * agent )
{
    update( agent );

    if ( M_ball_moving_to_our_goal )
    {
        if ( doPrepareTackle( agent ) )
        {
            return true;
        }
    }

    if ( ! M_tackle_situation
         || M_success_probability < 0.001 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": no tackle situation. prob=%f",
                      M_success_probability );
        return false;
    }

    updateBlocker( agent );

    const double min_prob = getMinProbability( agent );

    if ( M_success_probability < min_prob )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": success_prob=%.3f < min_prob=%.3f",
                      M_success_probability, min_prob );
        return false;
    }

    return doTackle( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_TacticalTackle::update( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.self().isKickable() )
    {
        return;
    }

    if ( wm.ball().ghostCount() > 0 )
    {
        return;
    }

    if ( wm.gameMode().type() != GameMode::PlayOn
         && ! wm.gameMode().isPenaltyKickMode() )
    {
        return;
    }

    M_use_foul = is_foul_situation( agent );
    M_success_probability = ( M_use_foul
                              ? wm.self().foulProbability()
                              : wm.self().tackleProbability() );

    M_ball_moving_to_our_goal = FieldAnalyzer::is_ball_moving_to_our_goal( wm );

    if ( M_ball_moving_to_our_goal )
    {
        M_tackle_situation = true;
        M_opponent_ball = true;
        dlog.addText( Logger::TEAM,
                      __FILE__":(update) ball is moving to our goal" );
        return;
    }

    if ( wm.kickableOpponent() )
    {
        M_tackle_situation = true;
        M_opponent_ball = true;
        dlog.addText( Logger::TEAM,
                      __FILE__":(update) kickable opponent" );
        return;
    }

    const ServerParam & SP = ServerParam::i();

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    const PlayerObject * opp_fastest = wm.interceptTable()->fastestOpponent();

    if ( opp_fastest
         && opp_fastest->goalie()
         && wm.gameMode().isPenaltyKickMode()
         && opp_fastest->pos().dist( wm.ball().pos() ) >= 3.0 ) // MAGIC NUMBER
    {
        M_tackle_situation = false;
        M_opponent_ball = false;

        dlog.addText( Logger::TEAM,
                      __FILE__":(update) penalty shootouts. not a tackle situation" );
        return;
    }

    if ( opp_fastest
         && wm.gameMode().isPenaltyKickMode()
         && ! opp_fastest->goalie() )
    {
        const AbstractPlayerObject * opponent_goalie = wm.getTheirGoalie();

        if ( opponent_goalie )
        {
            std::map< const AbstractPlayerObject*, int >::const_iterator player_map_it
                = wm.interceptTable()->playerMap().find( opponent_goalie );

            if ( player_map_it != wm.interceptTable()->playerMap().end() )
            {
                // considering only opponent goalie in penalty-kick mode
                opp_min = player_map_it->second;

                dlog.addText( Logger::TEAM,
                              __FILE__":(update) replaced min_opp with goalie's reach cycle (%d).",
                              opp_min );
            }
            else
            {
                opp_min = 1000000;  // practically canceling the fastest non-goalie opponent player

                dlog.addText( Logger::TEAM,
                              __FILE__":%d: (update) set opp_min as 1000000 so as not to consider the fastest opponent.",
                              __LINE__ );
            }
        }
        else
        {
            opp_min = 1000000;  // practically canceling the fastest non-goalie opponent player

            dlog.addText( Logger::TEAM,
                          __FILE__":%d (update) set opp_min as 1000000 so as not to consider the fastest opponent.",
                          __LINE__);
        }
    }

    const int intercept_step = std::min( self_min, std::min( mate_min, opp_min ) );
    const Vector2D ball_pos = wm.ball().inertiaPoint( intercept_step );

    if ( ball_pos.absX() > SP.pitchHalfLength() + 2.0
         || ball_pos.absY() > SP.pitchHalfWidth() + 2.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(update) false. out of pitch" );
        M_tackle_situation = false;
        M_opponent_ball = false;
        return;
    }


    if ( wm.ball().pos().x < SP.ourPenaltyAreaLineX() + 3.0
         || wm.ball().pos().dist2( SP.ourTeamGoalPos() ) < std::pow( 20.0, 2 ) )
    {
        if ( opp_min < self_min
             && opp_min < mate_min )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(update) true(1) their ball" );
            M_tackle_situation = true;
            M_opponent_ball = true;
            return;
        }
    }
    else
    {
        if ( opp_min < self_min - 3
             && opp_min < mate_min - 3 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__":(update) true(2) their ball" );
            M_tackle_situation = true;
            M_opponent_ball = true;
            return;
        }
    }

    if ( self_min >= 5
         && wm.ball().pos().dist2( SP.theirTeamGoalPos() ) < std::pow( 10.0, 2 )
         && ( ( SP.theirTeamGoalPos() - wm.self().pos() ).th() - wm.self().body() ).abs() < 45.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(update) true(3) tackle shoot" );
        M_tackle_situation = true;
        M_tackle_shoot = true;
        return;
    }

    if ( opp_fastest
         && opp_fastest->goalie()
         && opp_min <= self_min
         && wm.ball().pos().dist2( SP.theirTeamGoalPos() ) < std::pow( 15.0, 2 ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(update) true(4) tackle shoot" );
        M_tackle_situation = true;
        M_tackle_shoot = true;
        return;
    }

    if ( opp_fastest
         && opp_min <= self_min - 1
         && wm.ball().pos().dist2( SP.theirTeamGoalPos() ) < std::pow( 15.0, 2 ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(update) true(4-2) tackle shoot" );
        M_tackle_situation = true;
        M_tackle_shoot = true;
        return;
    }


    if ( wm.lastKickerSide() == wm.theirSide()
         && opp_min <= 5
         && opp_min <= mate_min
         && self_min >= 10 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(update) true(5)" );
        M_tackle_situation = true;
        return;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(update) end" );
}


/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_TacticalTackle::updateBlocker( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const PlayerObject * opponent = wm.kickableOpponent();
    if ( ! opponent )
    {
        return;
    }

    const Segment2D goal_line( opponent->pos(), ServerParam::i().ourTeamGoalPos() );
    const Vector2D goal_l( -ServerParam::i().pitchHalfLength(), -ServerParam::i().goalHalfWidth() );
    const Vector2D goal_r( -ServerParam::i().pitchHalfLength(), +ServerParam::i().goalHalfWidth() );
    const AngleDeg goal_l_angle = ( goal_l - opponent->pos() ).th();
    const AngleDeg goal_r_angle = ( goal_r - opponent->pos() ).th();

    for ( PlayerObject::Cont::const_iterator t = wm.teammates().begin(), end = wm.teammates().end();
          t != end;
          ++t )
    {
        if ( (*t)->goalie() ) continue;
        if ( (*t)->posCount() >= 3 ) continue;
        if ( (*t)->pos().x > opponent->pos().x ) continue;
        if ( (*t)->distFromBall() > 4.0 ) continue;

        const AngleDeg angle_from_opponent = ( (*t)->pos() - opponent->pos() ).th();
        if ( angle_from_opponent.isWithin( goal_r_angle, goal_l_angle )
             || goal_line.dist( (*t)->pos() ) < (*t)->playerTypePtr()->kickableArea() )
        {
            dlog.addText( Logger::ROLE,
                          __FILE__": (updateBlocker) blocked by %d", (*t)->unum() );
            M_blocker = *t;
            break;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Bhv_TacticalTackle::getMinProbability( const PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    if ( M_ball_moving_to_our_goal )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(getMinProbability) ball is moving to our goal." );
        return 0.05;
    }

    if ( wm.ball().pos().x > wm.ourDefenseLineX() + 10.0
         && Strategy::i().roleType( wm.self().unum() ) != Formation::Defender )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__":(getMinProbability) ball is in safety zone?" );
        return 0.8;
    }

    //
    // check if opponent ball holder is blocked by other teammates or not
    //
    if ( M_blocker )
    {
        return 0.99;
        // if ( wm.kickableOpponent()
        //      && wm.kickableOpponent()->vel().r2() < std::pow( 0.1, 2 ) )
        // {
        //     dlog.addText( Logger::ROLE,
        //                   __FILE__": (getMinProbability) exist blocker, but kicker is moving" );
        //     return 0.99;
        // }
        // dlog.addText( Logger::ROLE,
        //               __FILE__": (getMinProbability) exist blocker" );
        // return 0.6;
    }

    const int our_min = std::min( wm.interceptTable()->selfReachCycle(),
                                  std::min( wm.interceptTable()->teammateReachCycle(),
                                            wm.interceptTable()->goalieReachCycle() ) );
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    const Vector2D ball_pos = wm.ball().inertiaPoint( opp_min );

    double min_prob = 0.85;

    if ( ( opp_min <= our_min - 3
           || ( opp_min <= 4 && our_min > opp_min ) )
         && ball_pos.dist2( SP.ourTeamGoalPos() ) < std::pow( 13.0, 2 ) )
    {
        min_prob = 0.5;
        dlog.addText( Logger::TEAM,
                      __FILE__":(getMinProbability) ball is in dangerous zone and their ball [%.2f]",
                      min_prob );
        return min_prob;
    }

    if ( wm.ball().pos().dist2( SP.ourTeamGoalPos() ) < std::pow( 18.0, 2 ) )
    {
        min_prob = 0.8;
        dlog.addText( Logger::TEAM,
                      __FILE__":(getMinProbability) ball is in dangerous zone. [%.2f]",
                      min_prob );
        return min_prob;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__":(getMinProbability) default. [%.2f]", min_prob );
    return min_prob;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TacticalTackle::doTackle( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const GeneratorTackle::Result & result = GeneratorTackle::instance().bestResult( wm );

    Vector2D ball_next = wm.ball().pos() + result.ball_vel_;

    dlog.addText( Logger::TEAM,
                  __FILE__": (doTackle) %s prob=%f angle=%.0f resultVel=(%.2f %.2f) ballNext=(%.2f %.2f)",
                  M_use_foul ? "foul" : "tackle",
                  M_success_probability,
                  result.tackle_angle_.degree(),
                  result.ball_vel_.x, result.ball_vel_.y,
                  ball_next.x, ball_next.y );
    agent->debugClient().addMessage( "Tactical%s%.2f:%.0f",
                                     M_use_foul ? "Foul" : "Tackle",
                                     M_success_probability,
                                     result.tackle_angle_.degree() );

    double tackle_dir = ( result.tackle_angle_ - wm.self().body() ).degree();

    agent->doTackle( tackle_dir, M_use_foul );
    agent->setNeckAction( new Neck_TurnToPoint( ball_next ) );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TacticalTackle::doPrepareTackle( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();

    if ( ball_next.x < -ServerParam::i().pitchHalfLength() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doPrepareTackle) ball will be in the goal in the next step." );
        return false;
    }

    if ( Bhv_TackleIntercept().execute( agent ) )
    {
        return true;
    }

    return false;
#if 0
    const double current_prob = wm.self().tackleProbability();

    double dash_power = 0.0;
    double dash_dir = 0.0;
    const double dash_tackle_prob = FieldAnalyzer::get_tackle_probability_after_dash( wm,
                                                                                      &dash_power,
                                                                                      &dash_dir );
    const double turn_tackle_prob = FieldAnalyzer::get_tackle_probability_after_turn( wm );

    dlog.addText( Logger::TEAM,
                  __FILE__": (doPrepareTackle) prob: current=%.3f after_turn=%.3f after_dash=%.3f",
                  current_prob, turn_tackle_prob, dash_tackle_prob );

    if ( turn_tackle_prob > current_prob
         && turn_tackle_prob > dash_tackle_prob )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doPrepareTackle) turn to prepare tackle" );
        agent->debugClient().addMessage( "PrepareTackle:Turn" );
        Body_TurnToPoint( ball_next, 1 ).execute( agent );
        agent->setNeckAction( new Neck_TurnToPoint( ball_next ) );
        return true;
    }

    if ( dash_tackle_prob > current_prob
         && dash_tackle_prob > turn_tackle_prob )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doPrepareTackle) dash to prepare tackle" );
        agent->debugClient().addMessage( "PrepareTackle:Dash" );
        agent->doDash( dash_power, dash_dir );
        agent->setNeckAction( new Neck_TurnToPoint( ball_next ) );
        return true;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (doPrepareTackle) false" );

    return false;
#endif
}
