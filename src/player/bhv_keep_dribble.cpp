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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_keep_dribble.h"

#include "action_chain_holder.h"
#include "action_chain_graph.h"
#include "cooperative_action.h"

#include "act_dribble.h"
#include "generator_short_dribble.h"
#include "generator_keep_dribble.h"

//#include "neck_offensive_intercept_neck.h"
#include "neck_turn_to_receiver.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_kick_to_relative.h>
#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/view_synch.h>
#include <rcsc/action/kick_table.h>

#include <rcsc/player/intercept_table.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/soccer_intention.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/server_param.h>
#include <rcsc/soccer_math.h>
#include <rcsc/math_util.h>

//#define DEBUG_PRINT

using namespace rcsc;

namespace {

inline
double
get_omni_min_dash_angle()
{
    const double dash_angle_step = std::max( 15.0, ServerParam::i().dashAngleStep() );
    return ( -180.0 < ServerParam::i().minDashAngle()
             && ServerParam::i().maxDashAngle() < 180.0 )
        ? ServerParam::i().minDashAngle()
        : dash_angle_step * static_cast< int >( -180.0 / dash_angle_step );
}

double
get_omni_max_dash_angle()
{
    const double dash_angle_step = std::max( 15.0, ServerParam::i().dashAngleStep() );
    return ( -180.0 < ServerParam::i().minDashAngle()
             && ServerParam::i().maxDashAngle() < 180.0 )
        ? ServerParam::i().maxDashAngle() + dash_angle_step * 0.5
        : dash_angle_step * static_cast< int >( 180.0 / dash_angle_step ) - 0.001;
}

}


class IntentionKeepDribble
    : public SoccerIntention {
private:

    int M_mode;
    Vector2D M_target_ball_pos;
    Vector2D M_target_player_pos;
    Vector2D M_first_ball_vel;
    double M_dash_dir; // dash direction relative to player's body angle

    int M_kick_step;
    int M_turn_step;
    int M_dash_step;

    GameTime M_last_execute_time; //!< last executed time
    int M_performed_dash_count;

public:

    IntentionKeepDribble( const int mode,
                           const Vector2D & target_ball_pos,
                           const Vector2D & target_player_pos,
                           const Vector2D & first_ball_vel,
                           const double dash_dir,
                           const int n_kick,
                           const int n_turn,
                           const int n_dash,
                           const GameTime & start_time )
        : M_mode( mode ),
          M_target_ball_pos( target_ball_pos ),
          M_target_player_pos( target_player_pos ),
          M_first_ball_vel( first_ball_vel ),
          M_dash_dir( dash_dir ),
          M_kick_step( n_kick ),
          M_turn_step( n_turn ),
          M_dash_step( n_dash ),
          M_last_execute_time( start_time ),
          M_performed_dash_count( 0 )
      { }

    bool finished( const PlayerAgent * agent );

    bool execute( PlayerAgent * agent );

private:
    /*!
      \brief clear the action queue
    */
    void clear()
      {
          M_kick_step = M_turn_step = M_dash_step = 0;
      }

    bool checkOpponentOneStep( const WorldModel & wm );
    bool checkOpponentTwoStep( const WorldModel & wm );
    bool existOpponentOnSegment( const WorldModel & wm );

    void setTurnNeckCheckOpponent( PlayerAgent * agent );

    bool doAdjustDash( PlayerAgent * agent );
    bool doDashes( PlayerAgent * agent );
    bool doTurnDashes( PlayerAgent * agent );
    bool doKick( PlayerAgent * agent );

    bool doKeepDashes( PlayerAgent * agent );
    bool doKeepKickDashes( PlayerAgent * agent );
    bool doKeepKickTurnDashes( PlayerAgent * agent );
    bool doKeepTurnKickDashes( PlayerAgent * agent );
    bool doKeepCollideTurnKickDashes( PlayerAgent * agent );
};

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionKeepDribble::finished( const PlayerAgent * agent )
{
    dlog.addText( Logger::DRIBBLE,
                  "(IntentionKeepDribble::finished) kick=%d turn=%d dash=%d",
                  M_kick_step, M_turn_step, M_dash_step );

    if ( M_kick_step + M_turn_step + M_dash_step == 0 )
    {
        return true;
    }

    const WorldModel & wm = agent->world();

    if ( M_last_execute_time.cycle() + 1 != wm.time().cycle() )
    {
        dlog.addText( Logger::DRIBBLE,
                      "(IntentionKeepDribble::finished) last execute time is not match" );
        this->clear();
        return true;
    }

    if ( wm.self().collidesWithPlayer() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(finished) collides with other player." );
        this->clear();
        return true;
    }

    if ( M_kick_step == 0
         && M_turn_step == 0
         && wm.self().collidesWithBall() )
    {
        dlog.addText( Logger::DRIBBLE,
                      "(IntentionKeepDribble::finished) collides with ball." );
        this->clear();
        return true;
    }

    if ( wm.kickableTeammate()
         || wm.kickableOpponent() )
    {
        dlog.addText( Logger::DRIBBLE,
                      "(IntentionKeepDribble::finished) exist other kickable player" );
        this->clear();
        return true;
    }

    if ( wm.ball().pos().dist2( M_target_ball_pos ) < std::pow( 0.1, 2 ) )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(finished). already target point" );
        this->clear();
        return true;
    }

    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    if ( ball_next.absX() > ServerParam::i().pitchHalfLength() - 0.5
         || ball_next.absY() > ServerParam::i().pitchHalfWidth() - 0.5 )
    {
        dlog.addText( Logger::DRIBBLE,
                      "(IntentionKeepDribble::finished) ball will be out of pitch. stop intention." );
        this->clear();
        return true;
    }

    if ( wm.audioMemory().passRequestTime() == agent->world().time() )
    {
        dlog.addText( Logger::DRIBBLE,
                      "(IntentionKeepDribble::finished) heard pass request." );
        this->clear();
        return true;
    }

    if ( M_turn_step == 0 )
    {
        const Vector2D self_pos = wm.self().inertiaPoint( M_kick_step + M_turn_step + M_dash_step );
        const AngleDeg target_angle = ( M_target_player_pos - self_pos ).th();
        if ( ( target_angle - wm.self().body() ).abs() > 15.0 )
        {
            dlog.addText( Logger::DRIBBLE,
                          "(IntentionKeepDribble::finished) over angle threshold." );
            this->clear();
            return true;
        }
    }

    if ( existOpponentOnSegment( wm ) )
    {
        dlog.addText( Logger::DRIBBLE,
                      "(IntentionKeepDribble::finished) exist opponent %d on segment. cancel intention." );
        this->clear();
        return true;
    }

    if ( M_kick_step == 0
         || ( M_mode == ActDribble::KEEP_COLLIDE_TURN_KICK_DASHES
              && M_turn_step > 0 ) )
    {
        if ( checkOpponentOneStep( wm )
             || checkOpponentTwoStep( wm ) )
        {
            dlog.addText( Logger::DRIBBLE,
                          "(IntentionKeepDribble::finished) exist opponent. cancel intention." );
            this->clear();
            return true;
        }
    }

    if ( wm.self().isKickable() )
    {
        ActionChainHolder::instance().update( wm );
        const std::vector< ActionStatePair > & best = ActionChainHolder::i().graph().bestSequence();
        if ( best.size() == 2
             && best.back().action().type() == CooperativeAction::Shoot )
        {
            dlog.addText( Logger::DRIBBLE,
                      "(IntentionKeepDribble::finished) found shoot action sequence." );
            this->clear();
            return true;
        }
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__":(finished) not finished yet." );

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionKeepDribble::checkOpponentOneStep( const WorldModel & wm )
{
    if ( ! wm.self().isKickable() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::DRIBBLE,
                      "(IntentionKeepDribble::checkOpponentOneStep) I am not kickable" );
#endif
        return false;
    }

    const double tackle_dist = ServerParam::i().tackleDist();
    const double tackle_width = ServerParam::i().tackleWidth();

    const Vector2D ball_next
        = ( M_kick_step == 0
            ? wm.ball().pos() + wm.ball().vel()
            : wm.ball().pos()
            + ( M_target_ball_pos - wm.ball().pos() )
            .setLengthVector( ServerParam::i().firstBallSpeed( wm.ball().pos().dist( M_target_ball_pos ),
                                                               M_turn_step + M_dash_step ) ) );
    const bool aggressive_mode = ( ball_next.x > wm.offsideLineX()
                                   || ball_next.x > ServerParam::i().theirPenaltyAreaLineX() + 2.0
                                   || ServerParam::i().theirTeamGoalPos().dist2( ball_next ) < std::pow( 16.0, 2 ) );

    dlog.addText( Logger::DRIBBLE,
                  "(IntentionKeepDribble::checkOpponentOneStep)"
                  " ball_next=(%.2f %.2f)",
                  ball_next.x, ball_next.y );


    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromBall().begin(),
              end = wm.opponentsFromBall().end();
          o != end;
          ++o )
    {
        if ( (*o)->posCount() >= 10 ) continue;
        if ( (*o)->isGhost() ) continue;
        if ( (*o)->isTackling() ) continue;
        if ( (*o)->distFromSelf() >= 5.0 ) break;

        const PlayerType * ptype = (*o)->playerTypePtr();
        const Vector2D opponent_pos = (*o)->pos() + (*o)->vel();
        const double opponent_speed = (*o)->vel().r();
        const double ball_dist = opponent_pos.dist( ball_next );

        const double control_area = ( (*o)->goalie()
                                      && ball_next.x > ServerParam::i().theirPenaltyAreaLineX()
                                      && ball_next.absY() < ServerParam::i().penaltyAreaHalfWidth()
                                      ? ptype->reliableCatchableDist()
                                      : ptype->kickableArea() );

        if ( ball_dist < control_area + 0.15 )
        {
            dlog.addText( Logger::DRIBBLE,
                          "(IntentionKeepDribble::checkOpponentOneStep)"
                          " opponent[%d](%.1f %.1f) maybe kickable(1)",
                          (*o)->unum(), (*o)->pos().x, (*o)->pos().y );
            return true;
        }

        const double one_step_speed = ( ptype->dashPowerRate()
                                        * ServerParam::i().maxDashPower()
                                        * ptype->effortMax() );

        // dlog.addText( Logger::DRIBBLE,
        //               "(IntentionDribble::checkOpponent) opponent[%d] (%.2f %.2f)",
        //               (*o)->unum(), opponent_pos.x, opponent_pos.y );
        // dlog.addText( Logger::DRIBBLE,
        //               "__ ball_dist=%.3f dash+tackle=%.3f bodyCount=%d",
        //               ball_dist, one_step_speed + tackle_dist,
        //               (*o)->bodyCount() );

        //
        // check kick or tackle
        //
        if ( ball_dist < one_step_speed + tackle_dist )
        {
            dlog.addText( Logger::DRIBBLE,
                          "(IntentionKeepDribble::checkOpponentOneStep)"
                          " opponent[%d] check in detail",
                          (*o)->unum() );
            if ( aggressive_mode )
            {
                if ( ( (*o)->angleFromSelf() - wm.ball().angleFromSelf() ).abs() > 150.0 )
                {
                    dlog.addText( Logger::DRIBBLE,
                                  "(IntentionKeepDribble::checkOpponentOneStep)"
                                  " aggressive opponent[%d] backside angle_diff=%.1f",
                                  (*o)->unum(),
                                  ( (*o)->angleFromSelf() - wm.ball().angleFromSelf() ).abs() );
                    continue;
                }

                if ( (*o)->bodyCount() <= 1 )
                {
                    dlog.addText( Logger::DRIBBLE,
                                  "(IntentionKeepDribble::checkOpponentOneStep)"
                                  " opponent[%d] consider body count=%d",
                                  (*o)->unum(), (*o)->bodyCount() );

                    Vector2D player_to_ball = ( ball_next - opponent_pos ).rotatedVector( -(*o)->body() );

                    if ( player_to_ball.x < 0.0 )
                    {
                        dlog.addText( Logger::DRIBBLE,
                                      "(IntentionKeepDribble::checkOpponentOneStep)"
                                      " aggressive opponent[%d] never reach(1)"
                                      " player_to_ball=(%.1f %.1f)",
                                      (*o)->unum(), player_to_ball.x, player_to_ball.y );
                        continue;
                    }

                    if ( player_to_ball.absY() > tackle_width - 0.10 )
                    {
                        dlog.addText( Logger::DRIBBLE,
                                      "(IntentionKeepDribble::checkOpponentOneStep)"
                                      " aggressive opponent[%d] never reach(2)"
                                      " player_to_ball=(%.1f %.1f)",
                                      (*o)->unum(), player_to_ball.x, player_to_ball.y );
                        continue;
                    }

                    // if ( (*o)->bodyCount() == 0
                    //      && player_to_ball.x > 0.0
                    //      && player_to_ball.x < one_step_speed + tackle_dist*0.9
                    //      && player_to_ball.absY() < tackle_width - 0.10 )
                    // {
                    //     dlog.addText( Logger::DRIBBLE,
                    //                   "(IntentionKeepDribble::checkOpponentOneStep)"
                    //                   " aggressive opponent[%d] maybe tackle"
                    //                   " player_to_ball=(%.1f %.1f) dash+tackle=%.3f",
                    //                   (*o)->unum(), player_to_ball.x, player_to_ball.y,
                    //                   one_step_speed + tackle_dist*0.9 );
                    //     return true;
                    // }

                    if ( player_to_ball.x > 0.0
                         && player_to_ball.x < one_step_speed + control_area )
                    {
                        dlog.addText( Logger::DRIBBLE,
                                      "(IntentionKeepDribble::checkOpponentOneStep)"
                                      " aggressive opponent[%d] maybe kickable(1)"
                                      " player_to_ball=(%.1f %.1f) dash+ctrl=%.3f",
                                      (*o)->unum(), player_to_ball.x, player_to_ball.y,
                                      one_step_speed + control_area );
                        return true;
                    }

                    if ( player_to_ball.x > 0.0 )
                    {
                        double tackle_fail_prob
                            = std::pow( player_to_ball.x / tackle_dist, ServerParam::i().tackleExponent() )
                            + std::pow( player_to_ball.absY() / tackle_width, ServerParam::i().tackleExponent() );
                        if ( tackle_fail_prob < 1.0
                             && 1.0 - tackle_fail_prob > 0.9 )
                        {
                            dlog.addText( Logger::DRIBBLE,
                                          "(IntentionKeepDribble::checkOpponentOneStep)"
                                          " aggressive opponent[%d] will tackle"
                                          " player_to_ball=(%.2f %.2f)",
                                          (*o)->unum(), player_to_ball.x, player_to_ball.y );
                            return true;
                        }

                        dlog.addText( Logger::DRIBBLE,
                                      "(IntentionKeepDribble::checkOpponentOneStep)"
                                      " aggressive opponent[%d] check one dash. speed=%.3f thr=%.3f",
                                      (*o)->unum(),
                                      opponent_speed, ptype->realSpeedMax() * ptype->playerDecay() * 0.7 );

                        // fast running
                        if ( opponent_speed > ptype->realSpeedMax() * ptype->playerDecay() * 0.7 )
                        {
                            double x_dist = std::max( 0.0, player_to_ball.x - one_step_speed );
                            tackle_fail_prob
                                = std::pow( x_dist / tackle_dist, ServerParam::i().tackleExponent() )
                                + std::pow( player_to_ball.absY() / tackle_width, ServerParam::i().tackleExponent() );
                            if ( tackle_fail_prob < 1.0
                                 && 1.0 - tackle_fail_prob > 0.9 )
                            {
                                dlog.addText( Logger::DRIBBLE,
                                              "(IntentionKeepDribble::checkOpponentOneStep)"
                                              " aggressive opponent[%d] will tackle (2)"
                                              " player_to_ball=(%.2f %.2f)",
                                              (*o)->unum(), player_to_ball.x, player_to_ball.y );
                                return true;
                            }
                        }
                    }
                }
                else
                {
                    if ( ball_dist < one_step_speed + control_area - 0.2 )
                    {
                        dlog.addText( Logger::DRIBBLE,
                                      "(IntentionKeepDribble::checkOpponentOneStep)"
                                      " aggressive opponent[%d] maybe kickable(2)"
                                      " ball_dist=%.3f dash+ctrl=%.3f",
                                      (*o)->unum(),
                                      ball_dist, one_step_speed + control_area );
                        return true;
                    }
                }
            }
            else
            {
                if ( (*o)->bodyCount() == 0 )
                {
                    Vector2D player_to_ball = ( ball_next - opponent_pos ).rotatedVector( -(*o)->body() );

                    if ( player_to_ball.x < 0.0 )
                    {
                        dlog.addText( Logger::DRIBBLE,
                                      "(IntentionKeepDribble::checkOpponentOneStep)"
                                      " no-aggressive opponent[%d] never reach(1)"
                                      " player_to_ball=(%.1f %.1f)",
                                      (*o)->unum(), player_to_ball.x, player_to_ball.y );
                        continue;
                    }

                    if ( player_to_ball.absY() > tackle_width - 0.10 )
                    {
                        dlog.addText( Logger::DRIBBLE,
                                      "(IntentionKeepDribble::checkOpponentOneStep)"
                                      " no-aggressive opponent[%d] never reach(2)"
                                      " player_to_ball=(%.1f %.1f)",
                                      (*o)->unum(), player_to_ball.x, player_to_ball.y );
                        continue;
                    }

                    if ( player_to_ball.x > 0.0
                         && player_to_ball.x < one_step_speed + tackle_dist*0.9 )
                    {
                        dlog.addText( Logger::DRIBBLE,
                                      "(IntentionKeepDribble::checkOpponentOneStep)"
                                      " no-aggressive opponent[%d] maybe tackle(1)"
                                      " player_to_ball=(%.1f %.1f) dash+ctrl=%.3f",
                                      (*o)->unum(), player_to_ball.x, player_to_ball.y,
                                      one_step_speed + control_area );
                        return true;
                    }
                }
                else
                {
                    dlog.addText( Logger::DRIBBLE,
                                  "(IntentionKeepDribble::checkOpponentOneStep)"
                                  " no-aggressive opponent[%d] maybe tackle(2)"
                                  " ball_dist=%.2f",
                                  (*o)->unum(), ball_dist );
                    this->clear();
                    return true;
                }
            }
        }

    }

    dlog.addText( Logger::DRIBBLE,
                  "(IntentionKeepDribble::checkOpponentOneStep) OK" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionKeepDribble::checkOpponentTwoStep( const WorldModel & wm )
{
    if ( ! wm.self().isKickable() )
    {
        return false;
    }

    if ( M_kick_step > 0 )
    {
        return false;
    }

    const Vector2D ball_pos = ( M_kick_step == 0
                                ? wm.ball().inertiaPoint( 2 )
                                : wm.ball().pos() + ( M_target_ball_pos - wm.ball().pos() ).setLengthVector( 1.0 ) );

    if ( ball_pos.x > wm.offsideLineX()
         || ball_pos.x > ServerParam::i().theirPenaltyAreaLineX() + 2.0
         || ServerParam::i().theirTeamGoalPos().dist2( ball_pos ) < std::pow( 16.0, 2 ) )
    {
        dlog.addText( Logger::DRIBBLE,
                      "(IntentionKeepDribble::checkOpponentTwoStep)"
                      " in the attack zone. skip." );
        return false;
    }

    const double tackle_dist = ServerParam::i().tackleDist();
    const double tackle_width = ServerParam::i().tackleWidth();

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromBall().begin(),
              end = wm.opponentsFromBall().end();
          o != end;
          ++o )
    {
        if ( (*o)->posCount() >= 10 ) continue;
        if ( (*o)->isGhost() ) continue;
        if ( (*o)->isTackling() ) continue;
        if ( (*o)->distFromSelf() >= 5.0 ) break;

        const PlayerType * ptype = (*o)->playerTypePtr();
        const Vector2D opponent_pos = (*o)->pos() + (*o)->vel();
        const double ball_dist = opponent_pos.dist( ball_pos );

        // const double control_area = ( (*o)->goalie()
        //                               && ball_next.x > ServerParam::i().theirPenaltyAreaLineX()
        //                               && ball_next.absY() < ServerParam::i().penaltyAreaHalfWidth()
        //                               ? ptype->reliableCatchableDist()
        //                               : ptype->kickableArea() );

        const double one_step_speed = ( ptype->dashPowerRate()
                                        * ServerParam::i().maxDashPower()
                                        * ptype->effortMax() );
        if ( ball_dist < one_step_speed + tackle_dist )
        {
            dlog.addText( Logger::DRIBBLE,
                          "(IntentionKeepDribble::checkOpponentTwoStep)"
                          " opponent[%d] maybe tackle(1)",
                          (*o)->unum() );
            return true;
        }

        if ( (*o)->bodyCount() <= 1 )
        {
            Vector2D player_to_ball = ( ball_pos - opponent_pos ).rotatedVector( -(*o)->body() );
            if ( player_to_ball.absY() < tackle_width - 0.10
                 && player_to_ball.x > 0.0
                 && player_to_ball.x < one_step_speed*2.0 + tackle_dist )
            {
                dlog.addText( Logger::DRIBBLE,
                              "(IntentionKeepDribble::checkOpponentTwoStep)"
                              " opponent[%d] maybe tackle(2)",
                              (*o)->unum() );
                return true;
            }
        }
        else
        {
            if ( ball_dist < one_step_speed*2.0 + tackle_dist )
            {
                dlog.addText( Logger::DRIBBLE,
                              "(IntentionKeepDribble::checkOpponentTwoStep)"
                              " opponent[%d] maybe tackle(3)",
                              (*o)->unum() );
                return true;
            }
        }
    }

    dlog.addText( Logger::DRIBBLE,
                  "(IntentionKeepDribble::checkOpponentTwoStep) OK" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionKeepDribble::existOpponentOnSegment( const WorldModel & wm )
{
    Vector2D rel = M_target_ball_pos - wm.ball().pos();
    double len = rel.r();
    if ( len > 1.0 )
    {
        rel *= 1.0 / len;
    }

    const Segment2D self_move( wm.self().pos() + wm.self().vel(), M_target_player_pos );
    const Segment2D ball_move( wm.ball().pos() + rel, M_target_ball_pos );

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromBall().begin(),
              end = wm.opponentsFromBall().end();
          o != end;
          ++o )
    {
        if ( (*o)->distFromSelf() > 20.0 ) continue;
        if ( (*o)->posCount() >= 5 ) continue;
        if ( (*o)->isGhost() ) continue;
        if ( (*o)->isTackling() ) continue;

        const PlayerType * ptype = (*o)->playerTypePtr();
        Vector2D opponent_pos = (*o)->pos() + (*o)->vel();

        if ( ball_move.dist( opponent_pos ) < ptype->kickableArea() + 0.1 )
        {
            dlog.addText( Logger::DRIBBLE,
                          "(IntentionKeepDribble::existOpponentOnSegment)"
                          " detect opponent %d on ball line",
                          (*o)->unum() );
            return true;
        }

        if ( self_move.dist( opponent_pos ) < wm.self().playerType().playerSize() + ptype->playerSize() )
        {
            dlog.addText( Logger::DRIBBLE,
                          "(IntentionKeepDribble::existOpponentOnSegment)"
                          " detect opponent %d on my line",
                          (*o)->unum() );
            return true;
        }
    }

    dlog.addText( Logger::DRIBBLE,
                  "(IntentionKeepDribble::existOpponentOnSegment) OK" );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionKeepDribble::execute( PlayerAgent * agent )
{
    if ( M_kick_step + M_turn_step + M_dash_step == 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) empty queue." );
        return false;
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (intention:execute) kick=%d turn=%d dash=%d",
                  M_kick_step, M_turn_step, M_dash_step );

    const WorldModel & wm = agent->world();

#if 0
    //
    // compare the current queue with other chain action candidates
    //
    if ( wm.self().isKickable()
         && M_kick_step <= 0
         && M_turn_step <= 0
         && M_dash_step <= 5
         && wm.interceptTable()->opponentReachStep() >= 2 )
    {
        ActionChainHolder::instance().update( wm );
        const CooperativeAction & first_action = ActionChainHolder::i().graph().getFirstAction();

        if ( first_action.type() != CooperativeAction::Dribble
             || first_action.mode() != ActDribble::KEEP_DASHES )
        {
            agent->debugClient().addMessage( "CancelDribbleQ" );
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention:execute) cancel. select other action." );
            return false;
        }
    }
#endif

    //
    // body action
    //

    bool result = false;
    int kick_step = M_kick_step;
    int turn_step = M_turn_step;

    switch ( M_mode ) {
    case ActDribble::KEEP_DASHES:
        result = doKeepDashes( agent );
        break;
    case ActDribble::KEEP_KICK_DASHES:
        result = doKeepKickDashes( agent );
        break;
    case ActDribble::KEEP_KICK_TURN_DASHES:
        result = doKeepKickTurnDashes( agent );
        break;
    case ActDribble::KEEP_TURN_KICK_DASHES:
        result = doKeepTurnKickDashes( agent );
        break;
    case ActDribble::KEEP_COLLIDE_TURN_KICK_DASHES:
        result = doKeepCollideTurnKickDashes( agent );
        break;

    case ActDribble::KICK_TURN_DASHES:
    case ActDribble::OMNI_KICK_DASHES:
    default:
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) unsupported mode %d", M_mode );
        break;
    }

    if ( ! result )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) failed" );
        return false;
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (intention:execute) done body action" );
    agent->debugClient().setTarget( M_target_ball_pos );

    //
    // set view width
    //
    if ( wm.gameMode().type() != GameMode::PlayOn
         || kick_step > 0
         || turn_step > 0
         || M_dash_step <= 1
         || wm.interceptTable()->opponentReachStep() <= 4 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) default view synch" );
        agent->debugClient().addMessage( "ViewSynch" );
        agent->setViewAction( new View_Synch() );
    }
    else if ( M_dash_step >= 5
              && wm.interceptTable()->opponentReachStep() >= 5 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) default view wide" );
        agent->debugClient().addMessage( "ViewWide" );
        agent->setViewAction( new View_Wide() );
    }
    else
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) default view normal" );
        agent->debugClient().addMessage( "ViewNormal" );
        agent->setViewAction( new View_Normal() );
    }

    //
    // set turn neck
    //

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (intention:execute) default turn_neck scan field" );
    if ( wm.ball().velCount() >= 2
         && wm.ball().distFromSelf() > 2.0 )
    {
        agent->debugClient().addMessage( "NeckBall" );
        agent->setNeckAction( new Neck_TurnToBall() );
    }
    else if ( M_dash_step <= 1 )
    {
        ActionChainHolder::instance().update( wm );
        agent->debugClient().addMessage( "NeckReceiver" );
        //agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        agent->setNeckAction( new Neck_TurnToReceiver() );
    }
    else if ( kick_step > 0 || turn_step > 0 )
    {
        setTurnNeckCheckOpponent( agent );
    }
    else
    {
        agent->debugClient().addMessage( "NeckScan" );
        agent->setNeckAction( new Neck_ScanField() );
    }

    //
    // update time
    //

    M_last_execute_time = wm.time();

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
IntentionKeepDribble::setTurnNeckCheckOpponent( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int total_step = M_kick_step + M_turn_step + M_dash_step;
    const AbstractPlayerObject * target_player = static_cast< AbstractPlayerObject * >( 0 );
    int max_pos_count = 0;

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              o_end = wm.opponentsFromSelf().end();
          o != o_end;
          ++o )
    {
        if ( (*o)->posCount() <= 1 ) continue;
        if ( (*o)->distFromSelf() > 15.0 ) break;;

        double opponent_move_dist = (*o)->pos().dist( M_target_ball_pos );
        int opponent_move_step = (*o)->playerTypePtr()->cyclesToReachDistance( opponent_move_dist );
        if ( opponent_move_step - (*o)->posCount() > total_step + 3 )
        {
            continue;
        }

        if ( max_pos_count < (*o)->posCount() )
        {
            max_pos_count = (*o)->posCount();
            target_player = *o;
            dlog.addText( Logger::DRIBBLE,
                          __FILE__":(setFirstTurnNeck) detect opponent %d", (*o)->unum() );
        }
    }

    if ( ! target_player )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(Intentinon::setTurnNeckCheckOpponent) scan field" );
        agent->debugClient().addMessage( "KeepDribbleNeckScan" );
        agent->setNeckAction( new Neck_ScanField() );
    }
    else
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(setTurnNeckCheckOpponent) check player %c %d",
                      side_char( target_player->side() ),
                      target_player->unum() );
        agent->debugClient().addMessage( "KeepDribbleLookPlayer:%c%d",
                                         side_char( target_player->side() ),
                                         target_player->unum() );
        agent->setNeckAction( new Neck_TurnToPlayerOrScan( target_player, 0 ) );
    }

}

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionKeepDribble::doAdjustDash( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const PlayerType & ptype = wm.self().playerType();

    const AngleDeg dash_dir = ( M_dash_dir == CooperativeAction::ERROR_ANGLE
                                ? 0.0
                                : M_dash_dir );

    const AngleDeg base_dash_dir = ServerParam::i().discretizeDashAngle( dash_dir.degree() );
    const AngleDeg base_dash_angle = wm.self().body() + base_dash_dir;
    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -base_dash_angle );

    {
        const Vector2D ball_final = wm.ball().inertiaPoint( M_dash_step );
        const Vector2D my_inertia = wm.self().inertiaPoint( M_dash_step );
        const Vector2D ball_rel = rotate_matrix.transform( ball_final - my_inertia );

        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (doAdjustDash) dash=%d angle=%.1f ball_rel=(%.3f %.3f)",
                      M_dash_step,
                      base_dash_angle.degree(),
                      ball_rel.x, ball_rel.y );

        if ( ball_rel.absY() > ptype.kickableArea() + 0.2 ) // magic number
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (doAdjustDash) too big y difference %.3f",
                          ball_rel.absY() );
            return false;
        }
    }

    const Vector2D target_point = ( M_target_player_pos.isValid()
                                    ? M_target_player_pos
                                    : M_target_ball_pos );

    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    const double kickable_area2 = std::pow( ptype.kickableArea() - 0.1, 2 );
    const double collide_thr2 = std::pow( ptype.playerSize() + ServerParam::i().ballSize() + 0.15, 2 );

    const double max_dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );

    const double dir_step = std::max( 15.0, ServerParam::i().dashAngleStep() );
    const double min_dash_angle = ( M_mode == ActDribble::OMNI_KICK_DASHES
                                    ? get_omni_min_dash_angle()
                                    : std::max( -45.0, ServerParam::i().minDashAngle() ) );
    const double max_dash_angle = ( M_mode == ActDribble::OMNI_KICK_DASHES
                                    ? get_omni_max_dash_angle()
                                    : std::min( +45.0, ServerParam::i().maxDashAngle() ) + 0.001 );

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (doAdjustDash) base_dash_dir=%.1f angle=%.1f n_dash=%d",
                  base_dash_dir.degree(), base_dash_angle.degree(), M_dash_step );

    double min_dist2 = 10000000.0;
    //double max_move_x = 0.0;
    double best_dir = -360.0;
    double best_power = max_dash_power;

    for ( double dash_dir = min_dash_angle;
          dash_dir < max_dash_angle;
          dash_dir += dir_step )
    {
        if ( ( base_dash_dir - dash_dir ).abs() > 100.0 )
        {
            continue;
        }

        const double dash_dir_rate = ServerParam::i().dashDirRate( dash_dir );
        const AngleDeg dash_angle = wm.self().body() + dash_dir;
        const Vector2D dash_vec = Vector2D::from_polar( 1.0, dash_angle );

        for ( double power_decay = 1.0; power_decay > 0.49; power_decay -= 0.1 )
        {
            double dash_power = max_dash_power * power_decay;
            double accel_len = dash_power * wm.self().dashRate() * dash_dir_rate;
            Vector2D dash_accel = dash_vec * accel_len;
            Vector2D my_vel = wm.self().vel() + dash_accel;
            Vector2D my_next = wm.self().pos() + my_vel;

            double ball_dist2 = my_next.dist2( ball_next );

            if ( ball_dist2 < collide_thr2 )
            {
                continue;
            }

            if ( my_next.absX() > ServerParam::i().pitchHalfLength() - 0.5
                 || my_next.absY() > ServerParam::i().pitchHalfWidth() - 0.5 )
            {
                continue;
            }

            Vector2D ball_rel = rotate_matrix.transform( ball_next - my_next );
            if ( ball_rel.absY() > ptype.kickableArea() )
            {
                continue;
            }

            if ( M_dash_step <= 1 )
            {
                if ( ball_dist2 > kickable_area2 )
                {
                    dlog.addText( Logger::DRIBBLE,
                                  "(IntentionKeepDribble::doAdjustDash) power=%.1f dir=%.1f ball_dist=%.3f > kickable=%.3f",
                                  dash_power, dash_dir, std::sqrt( ball_dist2 ),
                                  ptype.kickableArea() );
                    continue;
                }
            }
            else
            {
                if ( ball_rel.x < 0.0
                     && ball_dist2 > kickable_area2 )
                {
                    continue;
                }
            }

            //Vector2D move_rel = rotate_matrix.transform( my_next - wm.self().pos() );
            double target_dist2 = target_point.dist2( my_next );
            // dlog.addText( Logger::DRIBBLE,
            //               "__ dir=%.1f(angle=%.1f) power=%.1f target_dist=%.3f move_x=%.3f move_y=%.3f",
            //               dash_dir, dash_angle.degree(), dash_power,
            //               target_point.dist( my_next ),
            //               move_rel.x, move_rel.y );
            if ( target_dist2 < min_dist2 ) //if ( move_rel.x > max_move_x )
            {
                // dlog.addText( Logger::DRIBBLE, "<< updated" );
                min_dist2 = target_dist2;
                //max_move_x = move_rel.x;
                best_dir = dash_dir;
                best_power = dash_power;
            }
        }
    }

    if ( best_dir == -360.0 )
    {
        return false;
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (doAdjustDash) power=%.1f dir=%.1f",
                  best_power, best_dir );
    agent->debugClient().addMessage( "DribbleQ:AdjustDash(%.0f,%.0f):%d",
                                     best_power, best_dir, M_dash_step );
    agent->doDash( best_power, best_dir );
    --M_dash_step;
    ++M_performed_dash_count;
    return true;
}

namespace {
bool
kickable_after_two_dash( const WorldModel & wm )
{
    const PlayerType & ptype = wm.self().playerType();
    const Matrix2D rotate_matrix = Matrix2D::make_rotation( -wm.self().body() );

    StaminaModel stamina_model = wm.self().staminaModel();
    Vector2D self_pos = wm.self().pos();
    Vector2D self_vel = wm.self().vel();
    Vector2D ball_pos = wm.ball().pos();
    Vector2D ball_vel = wm.ball().vel();

    const Vector2D accel_vec = Vector2D::from_polar( 1.0, wm.self().body() );

    int nokickable_count = 0;
    for ( int i = 1; i <= 3; ++i )
    {
        double dash_power = stamina_model.getSafetyDashPower( ptype, ServerParam::i().maxDashPower(), 100.0 );
        double dash_accel = dash_power * ptype.dashPowerRate() * stamina_model.effort();

        self_vel += accel_vec * dash_accel;
        self_pos += self_vel;
        self_vel *= ptype.playerDecay();
        ball_pos + ball_vel;
        ball_vel *= ServerParam::i().ballDecay();
        stamina_model.simulateDash( ptype, dash_power );

        Vector2D ball_rel = rotate_matrix.transform( ball_pos - self_pos );
        if ( ball_rel.absY() > ptype.kickableArea() )
        {
            return false;
        }

        if ( ball_rel.x > 0.0
             && ball_rel.r2() > std::pow( ptype.kickableArea() - 0.1, 2 ) )
        {
            ++nokickable_count;
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention) nokickable_count=%d dash=%d ball_rel=(%.3f %.3f) kickable=%.3f",
                          nokickable_count, i, ball_rel.x, ball_rel.y, ptype.kickableArea() );
            if ( nokickable_count >= 2 )
            {
                dlog.addText( Logger::DRIBBLE,
                              __FILE__": (intention) xxx over nokickable count thrheshold" );
                return false;
            }
        }
    }

    return true;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionKeepDribble::doDashes( PlayerAgent * agent )
{
    if ( M_dash_step <= 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:doDashes) no turn step" );
        return false;
    }

    if ( ! agent->world().self().isKickable() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:doDashes) no kickable. ball_dist=%.3f kickable=%.3f",
                      agent->world().ball().distFromSelf(),
                      agent->world().self().playerType().kickableArea() );
        return doAdjustDash( agent );
    }

    CooperativeAction::Ptr action = GeneratorKeepDribble::instance().getDashOnlyBest( agent->world() );

    if ( ! action )
    {
        if ( // M_performed_dash_count <= 1 &&
             kickable_after_two_dash( agent->world() ) )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention:doDashes) no dash only keep dribble candidate. try adjust dash" );
            return doAdjustDash( agent );
        }
        dlog.addText( Logger::DRIBBLE,
                  __FILE__": (intention:doDashes) no dash only keep dribble candidate." );
        return false;
    }


    AngleDeg dash_dir = ( action->firstDashDir() == CooperativeAction::ERROR_ANGLE
                          ? 0.0
                          : action->firstDashDir() );

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (intention:doDashes) count=%d dash_power=%.1f dash_dir=%.1f",
                  M_dash_step,
                  action->firstDashPower(), dash_dir.degree() );

    agent->debugClient().addMessage( "DribbleQ:Dash(%.0f,%.0f):%d",
                                     action->firstDashPower(), dash_dir.degree(), M_dash_step );
    agent->doDash( action->firstDashPower(), dash_dir );
    --M_dash_step;
    ++M_performed_dash_count;
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionKeepDribble::doTurnDashes( PlayerAgent * agent )
{
    if ( M_turn_step <= 0 )
    {
        return doDashes( agent );
    }

    const WorldModel & wm = agent->world();

    Vector2D target_self_pos = ( M_target_player_pos.isValid()
                                 ? M_target_player_pos
                                 : M_target_ball_pos );
    Vector2D target_ball_rel = M_target_ball_pos - target_self_pos;
    //Vector2D ball_pos = wm.ball().inertiaPoint( M_turn_step + M_dash_step );
    Vector2D adjusted_target = M_target_ball_pos - target_ball_rel;
    Vector2D self_pos = wm.self().inertiaPoint( M_turn_step + M_dash_step );
    AngleDeg target_body_angle = ( adjusted_target - self_pos ).th();
    AngleDeg turn_moment = ( target_body_angle - wm.self().body() ).degree();

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (intention:doTurnDashes) turn_step=%d target_body_angle=%.1f moment=%.1f",
                  M_turn_step,
                  target_body_angle.degree(), turn_moment.degree() );
    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (intention:doTurnDashes) target_self_pos=(%.2f %.2f)",
                  target_self_pos.x, target_self_pos.y );
    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (intention:doTurnDashes) target_ball_rel=(%.2f %.2f)",
                  target_ball_rel.x, target_ball_rel.y );
    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (intention:doTurnDashes) adjusted_target=(%.2f %.2f)",
                  adjusted_target.x, adjusted_target.y );
    agent->debugClient().addMessage( "DribbleQ:Turn:%d", M_turn_step + M_dash_step );

    agent->doTurn( turn_moment );
    --M_turn_step;
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionKeepDribble::doKick( PlayerAgent * agent )
{
    if ( M_kick_step <= 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention:doKick) no kick step" );
        return false;
    }

    const WorldModel & wm = agent->world();

    if ( ! wm.self().isKickable() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:doDashes) no kickable. ball_dist=%.3f kickable=%.3f",
                      wm.ball().distFromSelf(),
                      wm.self().playerType().kickableArea() );
        return false;
    }

    if ( M_kick_step == 1 )
    {
        int best_diff = 1000;
        CooperativeAction::Ptr action;
        const std::vector< CooperativeAction::Ptr > & candidates
            = GeneratorKeepDribble::instance().getKickDashesCandidates( wm );
        for ( std::vector< CooperativeAction::Ptr >::const_iterator it = candidates.begin(),
                  end = candidates.end();
              it != end;
              ++it )
        {
            int kick_diff = std::abs( (*it)->kickCount() - M_kick_step );
            int dash_diff = std::abs( (*it)->dashCount() - M_dash_step );
            if ( kick_diff + dash_diff < best_diff )
            {
                best_diff = kick_diff + dash_diff;
                action = *it;
                if ( kick_diff == 0 && dash_diff == 0 )
                {
                    break;
                }
            }
        }

        // CooperativeAction::Ptr action = GeneratorKeepDribble::instance().getKickDashesBest( wm );

        if ( ! action )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__":(intention:doKick) action not generated" );
            return false;
        }

        Vector2D kick_accel = action->firstBallVel() - wm.ball().vel();
        double kick_power = std::min( kick_accel.r() / wm.self().kickRate(), ServerParam::i().maxPower() );
        AngleDeg kick_dir = kick_accel.th() - wm.self().body();
        Vector2D result_vel = wm.ball().vel() + Vector2D::from_polar( kick_power * wm.self().kickRate(),
                                                                      wm.self().body() + kick_dir );
        Vector2D next_pos = wm.ball().pos() + result_vel;

        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(intention:doKick) index=%d kick=%d turn=%d dash=%d",
                      action->index(),
                      action->kickCount(), action->turnCount(), action->dashCount() );
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(intention:doKick) result_vel=(%.2f %.2f) next_ball_pos=(%.2f %.2f)",
                      result_vel.x, result_vel.y, next_pos.x, next_pos.y );
        dlog.addRect( Logger::DRIBBLE,
                      next_pos.x - 0.05, next_pos.y - 0.05, 0.1, 0.1, "#00B" );
        if ( action->kickCount() >= 2 )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__":(intention:doKick) overwrite kick count %d",
                          action->kickCount() );
        }

        agent->debugClient().addMessage( "DribbleQ:Kick:%d", M_kick_step + M_turn_step + M_dash_step );

        M_kick_step = action->kickCount();
        agent->doKick( kick_power, kick_dir );
    }
    else
    {
        double keep_dist = wm.self().playerType().playerSize()
            + wm.self().playerType().kickableMargin() * 0.5
            + ServerParam::i().ballSize();
        AngleDeg keep_relative_angle = ( M_target_ball_pos - wm.self().pos() ).th() - wm.self().body();

        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(intention:doKick) multi kick. keep_relative_angle=%.1f",
                      keep_relative_angle.degree() );
        agent->debugClient().addMessage( "DribbleQ:KickMulti:%d", M_kick_step + M_turn_step + M_dash_step );
        Body_KickToRelative( keep_dist, keep_relative_angle, false ).execute( agent );
    }

    --M_kick_step;
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionKeepDribble::doKeepDashes( PlayerAgent * agent )
{
    dlog.addText( Logger::DRIBBLE,
                  __FILE__":(intention:doKeepDashes) dash=%d",
                  M_dash_step );

    if ( M_kick_step > 0
         || M_turn_step > 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(intention:doKeepDashes) ERROR. kick=%d turn=%d",
                      M_kick_step, M_turn_step );
        return false;
    }

    return doDashes( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionKeepDribble::doKeepKickDashes( PlayerAgent * agent )
{
    dlog.addText( Logger::DRIBBLE,
                  __FILE__":(intention:doKeepKickDashes) dash=%d",
                  M_dash_step );

    if ( M_kick_step == 1 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(intention:doKeepKickDashes) kick=%d.", M_kick_step );
        return doKick( agent );
    }

    if ( M_kick_step > 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(intention:doKeepKickDashes) kick=%d. KEEP_KICK_DASHES does not support multi kicks.",
                      M_kick_step );
        return false;
    }

    if ( M_turn_step > 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(intention:doKeepKickDashes) turn=%d. KEEP_KICKDASHES does not support turn action.",
                      M_turn_step );
        return false;
    }

    return doDashes( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionKeepDribble::doKeepKickTurnDashes( PlayerAgent * agent )
{
    dlog.addText( Logger::DRIBBLE,
                  __FILE__":(intention:doKeepKickTurnDashes) turn=%d dash=%d",
                  M_turn_step, M_dash_step );

    if ( M_kick_step > 1 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(intention:doKeepKickTurnDashes) kick=%d. KEEP_KICK_TURN_DASHES does not support multi kicks.",
                      M_kick_step );
        return false;
    }

    return doTurnDashes( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionKeepDribble::doKeepTurnKickDashes( PlayerAgent * agent )
{
    dlog.addText( Logger::DRIBBLE,
                  __FILE__":(intention:doKeepTurnKickDashes) kick=%d dash=%d",
                  M_kick_step, M_dash_step );

    if ( M_kick_step >= 2 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(intention:doKeepTurnKickDashes) kick=%d. KEEP_TURN_KICK_DASHES does not support multi kicks.",
                      M_kick_step );
        return false;
    }

    if ( M_turn_step > 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(intention:doKeepKickTurnDashes) kick=%d. KEEP_KICK_TURN_DASHES does not support multi turns.",
                      M_turn_step );
        return false;
    }

    if ( M_kick_step > 0 )
    {
        return doKick( agent );
    }

    return doDashes( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionKeepDribble::doKeepCollideTurnKickDashes( PlayerAgent * agent )
{
    dlog.addText( Logger::DRIBBLE,
                  __FILE__":(intention:doKeepCollideTurnKickDashes) kick=%d turn=%d dash=%d",
                  M_kick_step, M_turn_step, M_dash_step );

    if ( M_kick_step >= 2 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(intention:doKeepCollideTurnKickDashes) kick=%d. KEEP_COLLIDE_TURN_KICK_DASHES does not support multi kicks.",
                      M_kick_step );
        return false;
    }

    if ( M_turn_step >= 2 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(intention:doKeepCollideTurnKickDashes) turn=%d. KEEP_COLLIDE_TURN_KICK_DASHES does not support multi turns.",
                      M_turn_step );
        return false;
    }

    const WorldModel & wm = agent->world();

    if ( M_turn_step > 0 )
    {
        Vector2D target_self_pos = ( M_target_player_pos.isValid()
                                     ? M_target_player_pos
                                     : M_target_ball_pos );
        Vector2D target_ball_rel = M_target_ball_pos - target_self_pos;
        Vector2D adjusted_target = M_target_ball_pos - target_ball_rel;
        Vector2D self_pos = wm.self().inertiaPoint( M_turn_step + M_dash_step );
        AngleDeg target_body_angle = ( adjusted_target - self_pos ).th();
        AngleDeg turn_moment = ( target_body_angle - wm.self().body() ).degree();

        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:doKeepCollideTurnKickDashes) target_body_angle=%.1f moment=%.1f",
                      target_body_angle.degree(), turn_moment.degree() );
        agent->debugClient().addMessage( "DribbleQ:Turn[Collide]:%d", M_kick_step + M_turn_step + M_dash_step );

        agent->doTurn( turn_moment );
        --M_turn_step;
        return true;
    }
    else if ( M_kick_step > 0 )
    {
        int best_dash_step_diff = 1000;
        CooperativeAction::Ptr action;
        const std::vector< CooperativeAction::Ptr > & candidates
            = GeneratorKeepDribble::instance().getKickDashesCandidates( wm );
        for ( std::vector< CooperativeAction::Ptr >::const_iterator it = candidates.begin(),
                  end = candidates.end();
              it != end;
              ++it )
        {
            int diff = std::abs( (*it)->dashCount() - M_dash_step );
            if ( diff < best_dash_step_diff )
            {
                best_dash_step_diff = diff;
                action = *it;
                if ( diff == 0 )
                {
                    break;
                }
            }
        }

        // CooperativeAction::Ptr action = GeneratorKeepDribble::instance().getKickDashesBest( wm );

        if ( ! action )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__":(intention:doKeepCollideTurnKickDashes) action not generated" );
            return false;
        }

        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(intention:doKeepCollideTurnKickDashes) kick index=%d kick_count=%d",
                      action->index(), action->kickCount() );
        M_kick_step = action->kickCount();

        Vector2D kick_accel = action->firstBallVel() - wm.ball().vel();
        double kick_power = std::min( kick_accel.r() / wm.self().kickRate(), ServerParam::i().maxPower() );
        AngleDeg kick_dir = kick_accel.th() - wm.self().body();
        agent->debugClient().addMessage( "DribbleQ:Kick[Collide]:%d", M_kick_step + M_turn_step + M_dash_step );

        agent->doKick( kick_power, kick_dir );
        --M_kick_step;
        return true;
    }

    return doDashes( agent );
}

/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*!

 */
Bhv_KeepDribble::Bhv_KeepDribble( const CooperativeAction & action )
    : M_action_index( action.index() ),
      M_mode( action.mode() ),
      M_target_ball_pos( action.targetBallPos() ),
      M_target_player_pos( action.targetPlayerPos() ),
      M_first_ball_vel( action.firstBallVel() ),
      M_first_turn_moment( action.firstTurnMoment() ),
      M_first_dash_power( action.firstDashPower() ),
      M_first_dash_dir( action.firstDashDir() ),
      M_total_step( action.durationTime() ),
      M_kick_step( action.kickCount() ),
      M_turn_step( action.turnCount() ),
      M_dash_step( action.dashCount() )
{
    if ( action.type() != CooperativeAction::Dribble )
    {
        M_target_ball_pos = Vector2D::INVALIDATED;
        M_total_step = 0;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_KeepDribble::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( ! wm.self().isKickable() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute). no kickable..." );
        return false;
    }

    if ( ! M_target_ball_pos.isValid()
         || M_total_step <= 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute). illegal target point or illegal total step" );
#ifdef DEBUG_PRINT
        std::cerr << wm.self().unum() << ' ' << wm.time()
                  << " Bhv_KeepDribble::execute() illegal target point or illegal total step"
                  << std::endl;
#endif
        return false;
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (execute) index=%d mode=%d", M_action_index, M_mode );

    bool result = false;
    switch ( M_mode ) {
    case ActDribble::KEEP_DASHES:
        result = doKeepDashes( agent );
        break;
    case ActDribble::KEEP_KICK_DASHES:
        result = doKeepKickDashes( agent );
        break;
    case ActDribble::KEEP_KICK_TURN_DASHES:
        result = doKeepKickTurnDashes( agent );
        break;
    case ActDribble::KEEP_TURN_KICK_DASHES:
        result = doKeepTurnKickDashes( agent );
        break;
    case ActDribble::KEEP_COLLIDE_TURN_KICK_DASHES:
        result = doKeepCollideTurnKickDashes( agent );
        break;

    case ActDribble::KICK_TURN_DASHES:
    case ActDribble::OMNI_KICK_DASHES:
    default:
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute) unsupported mode %d", M_mode );
        break;
    }

    if ( ! result )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute) cannot perform the action" );
        return false;
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (execute) view synch, neck scan field, set intention" );

    agent->setViewAction( new View_Synch() );

    setFirstTurnNeck( agent );

    agent->setIntention( new IntentionKeepDribble( M_mode,
                                                    M_target_ball_pos,
                                                    M_target_player_pos,
                                                    M_first_ball_vel,
                                                    M_first_dash_dir,
                                                    M_kick_step,
                                                    M_turn_step,
                                                    M_dash_step,
                                                    wm.time() ) );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_KeepDribble::setFirstTurnNeck( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const AbstractPlayerObject * target_player = static_cast< AbstractPlayerObject * >( 0 );
    int max_pos_count = 0;

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              o_end = wm.opponentsFromSelf().end();
          o != o_end;
          ++o )
    {
        if ( (*o)->posCount() <= 1 ) continue;
        if ( (*o)->distFromSelf() > 15.0 ) break;;

        double opponent_move_dist = (*o)->pos().dist( M_target_ball_pos );
        int opponent_move_step = (*o)->playerTypePtr()->cyclesToReachDistance( opponent_move_dist );
        if ( opponent_move_step - (*o)->posCount() > M_total_step + 3 )
        {
            continue;
        }

        if ( max_pos_count < (*o)->posCount() )
        {
            max_pos_count = (*o)->posCount();
            target_player = *o;
            dlog.addText( Logger::DRIBBLE,
                          __FILE__":(setFirstTurnNeck) detect opponent %d", (*o)->unum() );
        }
    }

    if ( ! target_player )
    {
        const std::vector< ActionStatePair > & seq = ActionChainHolder::i().graph().bestSequence();

        if ( seq.size() >= 2
             && seq[1].action().targetPlayerUnum() != wm.self().unum() )
        {
            target_player = wm.ourPlayer( seq[1].action().targetPlayerUnum() );
            if ( target_player )
            {
                dlog.addText( Logger::DRIBBLE,
                              __FILE__":(setFirstTurnNeck) detect teammate %d", target_player->unum() );
            }
        }
    }


    if ( ! target_player )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(setFirstTurnNeck) scan field" );
        agent->debugClient().addMessage( "KeepDribbleNeckScan" );
        agent->setNeckAction( new Neck_ScanField() );
    }
    else
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(setFirstTurnNeck) check player %c %d",
                      side_char( target_player->side() ),
                      target_player->unum() );
        agent->debugClient().addMessage( "KeepDribbleLookPlayer:%c%d",
                                         side_char( target_player->side() ),
                                         target_player->unum() );
        agent->setNeckAction( new Neck_TurnToPlayerOrScan( target_player, 0 ) );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_KeepDribble::doFirstKick( PlayerAgent * agent )
{
    if ( M_kick_step <= 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(doFirstKick) ERROR no kick" );
        return false;
    }

    if ( ! M_first_ball_vel.isValid() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(doFirstKick) ERROR illegal first vel" );
        return false;
    }

    const WorldModel & wm = agent->world();

    if ( M_kick_step <= 2 )
    {
        Vector2D kick_accel = M_first_ball_vel - wm.ball().vel();
        double accel_len = kick_accel.r();
        if ( accel_len > ServerParam::i().ballAccelMax() )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (doFirstKick) over max accel %f", accel_len );
            return false;
        }

        double kick_power = accel_len / wm.self().kickRate();

        if ( kick_power > ServerParam::i().maxPower() + 1.0e-5 )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__":(doFirstKick) over the max power. %.3f", kick_power );
            std::cerr << wm.ourTeamName() << ' ' << wm.self().unum() << ' ' << wm.time()
                      << " (Bhv_KeepDribble::doFirstKick) over the max power "
                      << kick_power << std::endl;
            kick_power = ServerParam::i().maxPower();
        }

        AngleDeg kick_dir = kick_accel.th() - wm.self().body();

        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(doFirstKick) first kick. power=%.3f dir=%.1f",
                      kick_power, kick_dir.degree() );
        agent->doKick( kick_power, kick_dir );
    }
    else
    {
        double keep_dist = wm.self().playerType().playerSize()
            + wm.self().playerType().kickableMargin() * 0.5
            + ServerParam::i().ballSize();
        AngleDeg keep_relative_angle = M_first_ball_vel.th() - wm.self().body();

        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(doFirstKick) multi kick. keep_relative_angle=%.1f",
                      keep_relative_angle.degree() );
        Body_KickToRelative( keep_dist, keep_relative_angle, false ).execute( agent );
    }

    --M_kick_step;
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_KeepDribble::doKeepDashes( PlayerAgent * agent )
{
    if ( M_kick_step > 0
         || M_turn_step > 0
         || M_dash_step <= 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(doKeepDashes) ERROR. illegal action count. kick=%d turn=%d dash=%d",
                      M_kick_step, M_turn_step, M_dash_step );
        return false;
    }

    if ( M_first_dash_power < 0.0 || ServerParam::i().maxDashPower() < M_first_dash_power )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(doKeepDashes) ERROR. illegal dash power %.3f",
                      M_first_dash_power );
        return false;
    }

    AngleDeg dash_dir = ( M_first_dash_dir == CooperativeAction::ERROR_ANGLE
                          ? 0.0
                          : M_first_dash_dir );
    dlog.addText( Logger::DRIBBLE,
                  __FILE__":(doKeepDashes) first dash. dash_power=%.1f dir=%.1f",
                  M_first_dash_power, dash_dir.degree() );

    agent->debugClient().addMessage( "KeepDribbleD:(%.0f %.0f):%d",
                                     M_first_dash_power, dash_dir.degree(),
                                     M_kick_step + M_turn_step + M_dash_step );

    agent->doDash( M_first_dash_power, dash_dir );

    --M_dash_step;
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_KeepDribble::doKeepKickDashes( PlayerAgent * agent )
{
    if ( M_kick_step >= 3
         || M_turn_step > 0
         || M_dash_step <= 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(doKeepKickDashes) ERROR. illegal action count. kick=%d turn=%d dash=%d",
                      M_kick_step, M_turn_step, M_dash_step );
        return false;
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (doKeepKickDashes)" );

    agent->debugClient().addMessage( "KeepDribbleKD:%d",
                                     M_kick_step + M_turn_step + M_dash_step );

    return doFirstKick( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_KeepDribble::doKeepKickTurnDashes( PlayerAgent * agent )
{
    if ( M_kick_step != 1
         || M_turn_step <= 0
         || M_dash_step <= 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(doKeepKickTurnDashes) ERROR. illegal action count. kick=%d turn=%d dash=%d",
                      M_kick_step, M_turn_step, M_dash_step );
        return false;
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (doKeepKickTurnDashes)" );
    agent->debugClient().addMessage( "KeepDribbleKTD:%d",
                                     M_kick_step + M_turn_step + M_dash_step );

    return doFirstKick( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_KeepDribble::doKeepTurnKickDashes( PlayerAgent * agent )
{
    if ( M_kick_step != 1
         || M_turn_step != 1
         || M_dash_step <= 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(doKeepTurnKickDashes) ERROR. illegal action count. kick=%d turn=%d dash=%d",
                      M_kick_step, M_turn_step, M_dash_step );
        return false;
    }

    if ( std::fabs( M_first_turn_moment ) < 1.0e-5 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(doKeepTurnKickDashes) ERROR. no turn moment" );
        return false;
    }


    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (doKeepTurnKickDashes)" );
    agent->debugClient().addMessage( "KeepDribbleTurn:%d",
                                     M_kick_step + M_turn_step + M_dash_step );

    agent->doTurn( M_first_turn_moment );
    --M_turn_step;
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_KeepDribble::doKeepCollideTurnKickDashes( PlayerAgent * agent )
{
    if ( M_kick_step != 2
         || M_turn_step != 1
         || M_dash_step <= 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(doKeepCollideTurnKickDashes) ERROR. illegal action count. kick=%d turn=%d dash=%d",
                      M_kick_step, M_turn_step, M_dash_step );
        return false;
    }

    if ( ! M_first_ball_vel.isValid() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__":(doKeepCollideTurnKickDashes) ERROR illegal first vel" );
        return false;
    }

    const WorldModel & wm = agent->world();

    Vector2D kick_accel = M_first_ball_vel - wm.ball().vel();
    double kick_power = std::min( kick_accel.r() / wm.self().kickRate(),
                                  ServerParam::i().maxPower() );
    AngleDeg kick_angle = kick_accel.th() - wm.self().body();

    dlog.addText( Logger::DRIBBLE,
                  __FILE__":(doKeepCollideTurnKickDashes) collision kick. power=%.3f angle=%.1f",
                  kick_power, kick_angle.degree() );
    agent->debugClient().addMessage( "KeepDribbleCTKD:%d",
                                     M_kick_step + M_turn_step + M_dash_step );

    agent->doKick( kick_power, kick_angle );
    --M_kick_step;
    return true;
}
