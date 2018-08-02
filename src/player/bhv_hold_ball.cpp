// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

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

#include "bhv_hold_ball.h"

#include "action_chain_graph.h"
#include "action_chain_holder.h"

#include "field_analyzer.h"

#include "bhv_clear_ball.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_scan_field.h>
//#include <rcsc/action/body_clear_ball.h>
#include <rcsc/action/body_hold_ball.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_player_or_scan.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>

#include <rcsc/action/kick_table.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/soccer_intention.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

// #define DEBUG_PRINT

using namespace rcsc;

namespace {

/*-------------------------------------------------------------------*/
/*!

 */
inline
bool
is_kickable_after_turn( const WorldModel & wm )
{
    double kickable2 = std::pow( wm.self().playerType().kickableArea()
                                 - wm.self().vel().r() * ServerParam::i().playerRand()
                                 - wm.ball().vel().r() * ServerParam::i().ballRand()
                                 - 0.15,
                                 2 );
    Vector2D self_next = wm.self().pos() + wm.self().vel();
    Vector2D ball_next = wm.ball().pos() + wm.ball().vel();

    return self_next.dist2( ball_next ) < kickable2;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
set_turn_neck( PlayerAgent * agent,
               const int target_unum )
{
    const WorldModel & wm = agent->world();

    if ( target_unum != Unum_Unknown
         && target_unum != wm.self().unum() )
    {
        const AbstractPlayerObject * target_player = wm.ourPlayer( target_unum );
        if ( target_player )
        {
            agent->debugClient().addMessage( "HoldBall:NeckTarget" );
            dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                          __FILE__" (set_turn_neck) target_player=%d",
                          target_unum );
            agent->setNeckAction( new Neck_TurnToPlayerOrScan( target_player, 0 ) );
            return;
        }
    }

    if ( wm.ball().pos().dist( ServerParam::i().theirTeamGoalPos() ) < 18.0 )
    {
        agent->debugClient().addMessage( "HoldBall:NeckGoalie" );
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      __FILE__" (set_turn_neck) check goalie" );
        agent->setNeckAction( new Neck_TurnToGoalieOrScan( 0 ) );
        return;
    }

    agent->debugClient().addMessage( "HoldBall:NeckScan" );
    dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                  __FILE__" (set_turn_neck) default scan field" );
    agent->setNeckAction( new Neck_ScanField() );
}

/*!

*/
class IntentionTurnTo
    : public SoccerIntention {
private:
    int M_step;
    int M_target_unum;
    Vector2D M_target_point;

public:

    IntentionTurnTo( const int target_unum,
                     const Vector2D & target_point )
        : M_step( 0 ),
          M_target_unum( target_unum ),
          M_target_point( target_point )
      { }

    bool finished( const PlayerAgent * agent );

    bool execute( PlayerAgent * agent );
};


/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionTurnTo::finished( const PlayerAgent * agent )
{
    ++M_step;

    dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                  __FILE__": (finished) step=%d",
                  M_step );

    if ( M_step >= 2 )
    {
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      __FILE__": (finished) time over" );
        return true;
    }

    const WorldModel & wm = agent->world();

    //
    // check kickable
    //

    if ( ! wm.self().isKickable() )
    {
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      __FILE__": (finished) no kickable" );
        return true;
    }

    //
    // check opponent
    //

    if ( wm.kickableOpponent() )
    {
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      __FILE__": (finished) exist kickable opponent" );
        return true;
    }

    //
    // check kickable after turn
    //

    if ( ! is_kickable_after_turn( wm ) )
    {
        // unkickable if turn is performed.
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      __FILE__": (finished) unkickable at next cycle." );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionTurnTo::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                  __FILE__": (intention) target_point=(%.1f %.1f)",
                  M_target_point.x, M_target_point.y );
    agent->debugClient().addMessage( "IntentionTurnTo" );

    Body_TurnToPoint( M_target_point ).execute( agent );

    set_turn_neck( agent, M_target_unum );

    return true;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
Bhv_HoldBall::Bhv_HoldBall()
    : M_chain_graph( ActionChainHolder::i().graph() )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_HoldBall::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.gameMode().type() != GameMode::PlayOn )
    {
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      __FILE__" (Bhv_HoldBall) no play_on" );
        return false;
    }

    const CooperativeAction & first_action = M_chain_graph.bestFirstAction();

    if ( first_action.type() != CooperativeAction::Hold )
    {
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      __FILE__" (Bhv_HoldBall) first action category is NOT hold." );
        return false;
    }

    //
    // if possible, turn to the target
    //

    const std::pair< int, Vector2D > target = getChainTarget( wm );

    if ( target.second.isValid() )
    {
        const AngleDeg target_angle = ( target.second - wm.self().pos() ).th();

        if ( ( target_angle - wm.self().body() ).abs() > 90.0 )
        {
            if ( doTurnTo( agent, target.first, target.second ) )
            {
                return true;
            }

            if ( doKeepBall( agent, target.first, target.second ) )
            {
                return true;
            }
        }
    }

    const ServerParam & SP = ServerParam::i();

    if ( wm.ball().pos().x < -SP.pitchHalfLength() + 8.0
         && wm.ball().pos().absY() < SP.goalHalfWidth() + 1.0 )
    {
        agent->debugClient().addMessage( "HoldBall:Clear" );
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      __FILE__" (Bhv_ChainAction) clear ball" );
        Bhv_ClearBall().execute( agent );
        //agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    agent->debugClient().addMessage( "HoldBall:Hold" );
    dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                  __FILE__" (Bhv_ChainAction) hold" );
#if 0
    //
    // TODO: consider strategic positions
    //

    const double min_x = wm.ourOffensePlayerLineX() - 10.0;

    Vector2D body_point( 0.0, 0.0 );

    int count = 0;
    for ( PlayerPtrCont::const_iterator t = wm.teammatesFromSelf().begin(),
              t_end = wm.teammatesFromSelf().end();
          t != t_end;
          ++t )
    {
        if ( (*t)->goalie() ) continue;
        if ( (*t)->pos().x < min_x ) continue;

        body_point += (*t)->pos();
        ++count;
    }

    if ( count > 0 )
    {
        body_point /= static_cast< double >( count );
    }

    Body_HoldBall( true, body_point ).execute( agent );
#else
    Body_HoldBall().execute( agent );
#endif
    set_turn_neck( agent, target.first );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
std::pair< int, Vector2D >
Bhv_HoldBall::getChainTarget( const WorldModel & wm )
{
    if ( M_chain_graph.bestSequence().size() > 1 )
    {
        for ( std::vector< ActionStatePair >::const_iterator it = M_chain_graph.bestSequence().begin() + 1,
                  end = M_chain_graph.bestSequence().end();
              it != end;
              ++it )
        {
            if ( it->action().targetPlayerUnum() != wm.self().unum() )
            {
                const AbstractPlayerObject * p = wm.ourPlayer( it->action().targetPlayerUnum() );
                if ( p )
                {
                    return std::pair< int, Vector2D >( p->unum(), p->pos() );
                }
            }
        }
    }

    return std::pair< int, Vector2D >( Unum_Unknown, Vector2D( 42.0, 0.0 ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_HoldBall::doTurnTo( PlayerAgent * agent,
                         const int target_unum,
                         const Vector2D & target_point )
{
    const WorldModel & wm = agent->world();

    dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                  __FILE__" (doTurnTo) target %d (%.1f %.1f)",
                  target_unum,
                  target_point.x, target_point.y );

    if ( ! is_kickable_after_turn( wm ) )
    {
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      __FILE__" (doTurnTo) no kickable if no kick" );
        return false;
    }

    //
    // check oppornent interfare possibility
    //

    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    const int o_step = Bhv_HoldBall::predict_opponents_reach_step( wm, 1, ball_next );

    if ( o_step <= 1 )
    {
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      __FILE__" (doTurnTo) opponent will be reach." );
        return false;
    }

    Body_TurnToPoint( target_point ).execute( agent );
    set_turn_neck( agent, target_unum );
    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_HoldBall::doKeepBall( PlayerAgent * agent,
                          const int target_unum,
                          const Vector2D & target_point )
{
    const WorldModel & wm = agent->world();

    const Vector2D ball_vel = Bhv_HoldBall::get_keep_ball_vel( wm );

    if ( ! ball_vel.isValid() )
    {
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      __FILE__": (doKeepBall) no candidate." );

        return false;
    }

    //
    // perform first kick
    //

    const Vector2D kick_accel = ball_vel - wm.ball().vel();
    const double kick_power = kick_accel.r() / wm.self().kickRate();
    const AngleDeg kick_angle = kick_accel.th() - wm.self().body();

    if ( kick_power > ServerParam::i().maxPower() )
    {
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      __FILE__": (doKeepBall) over kick power" );
        return false;
    }

    dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                  __FILE__": (doKeepBall) "
                  " bvel=(%.2f %.2f)"
                  " kick_power=%.1f kick_angle=%.1f",
                  ball_vel.x, ball_vel.y,
                  kick_power,
                  kick_angle.degree() );

    dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                  __FILE__": (doKeepBall) target=%d pos=(%.1f %.1f)",
                  target_unum, target_point.x, target_point.y );
#ifdef DEBUG_PRINT
    {
        Vector2D bpos = wm.ball().pos();
        bpos += ball_vel;
        dlog.addRect( Logger::ACTION_CHAIN | Logger::TEAM,
                      bpos.x - 0.1, bpos.y - 0.1, 0.2, 0.2, "#F00" );
        bpos += ball_vel * ServerParam::i().ballDecay();
        dlog.addRect( Logger::ACTION_CHAIN | Logger::TEAM,
                      bpos.x - 0.1, bpos.y - 0.1, 0.2, 0.2, "#F00" );
    }
#endif

    agent->debugClient().addMessage( "HoldBall:KeepBall" );
    agent->debugClient().setTarget( target_point );


    agent->doKick( kick_power, kick_angle );
    set_turn_neck( agent, target_unum );

    agent->setIntention( new IntentionTurnTo( target_unum, target_point ) );

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
Vector2D
Bhv_HoldBall::get_keep_ball_vel( const WorldModel & wm )
{
    static GameTime s_update_time( 0, 0 );
    static Vector2D s_best_ball_vel( 0.0, 0.0 );

    if ( s_update_time == wm.time() )
    {
        return s_best_ball_vel;
    }
    s_update_time = wm.time();

    const int ANGLE_DIVS = 16;

    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();
    const double collide_dist2 = std::pow( ptype.playerSize() + SP.ballSize(), 2 );
    const double keep_dist = ( ptype.playerSize()
                               + ServerParam::i().ballSize()
                               + std::min( 0.2, ptype.kickableMargin() * 0.6 ) );

    const Vector2D self_next1 = wm.self().pos() + wm.self().vel();
    const Vector2D self_next2 = self_next1 + ( wm.self().vel() * ptype.playerDecay() );

    const Vector2D nearest_opponent_pos = ( wm.opponentsFromBall().empty()
                                            ? Vector2D( -10000.0, 0.0 )
                                            : wm.opponentsFromBall().front()->pos() );

    //
    // create keep target point
    //

    Vector2D best_ball_vel = Vector2D::INVALIDATED;
    int best_opponent_step = 0;
    //double best_ball_speed = 1000.0;
    double best_opponent_dist2 = 0.0;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                  __FILE__"(get_keep_ball_vel)" );
#endif


    for ( int a = 0; a < ANGLE_DIVS; ++a )
    {
        Vector2D ball_next2 = ( self_next2
                                + Vector2D::from_polar( keep_dist, 360.0/ANGLE_DIVS * a ) );
        if ( ball_next2.absX() > SP.pitchHalfLength() - 0.2
             || ball_next2.absY() > SP.pitchHalfWidth() - 0.2 )
        {
            continue;
        }

        const Vector2D ball_move = ball_next2 - wm.ball().pos();
        const double ball_move_dist = ball_move.r();
        const double ball_speed = ball_move_dist / ( 1.0 + SP.ballDecay() );

        const Vector2D max_vel = KickTable::calc_max_velocity( ball_move.th(),
                                                               wm.self().kickRate(),
                                                               wm.ball().vel() );
        if ( max_vel.r2() < std::pow( ball_speed, 2 ) )
        {
            continue;
        }

        const Vector2D ball_vel = ball_move * ( ball_speed / ball_move_dist );

        // const Vector2D kick_accel = ( ball_vel - wm.ball().vel() ).r();
        // if ( kick_accel > SP.ballAccelMax()
        //      || kick_accel > wm.self().kickRate() * SP.maxPower() )
        // {
        //     continue;
        // }

        const Vector2D ball_next1 = wm.ball().pos() + ball_vel;

        if ( self_next1.dist2( ball_next1 ) < collide_dist2 )
        {
            ball_next2 = ball_next1;
            ball_next2 += ball_vel * ( SP.ballDecay() * -0.1 );
        }

        //
        // check opponent
        //

        const int min_step = Bhv_HoldBall::predict_opponents_reach_step( wm, ball_next1, ball_next2 );


#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      "%d: ball_move th=%.1f speed=%.2f max=%.2f",
                      a,
                      ball_move.th().degree(),
                      ball_speed,
                      max_vel.r() );
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      "%d: ball_next=(%.2f %.2f) ball_next2=(%.2f %.2f)",
                      a,
                      ball_next1.x, ball_next1.y,
                      ball_next2.x, ball_next2.y );
        dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                      "%d: opponent_step=%d",
                      a, min_step );
#endif

        if ( min_step > best_opponent_step )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                          "%d: update(1)", a );
#endif
            best_ball_vel = ball_vel;
            best_opponent_step = min_step;
            //best_ball_speed = ball_speed;
            best_opponent_dist2 = nearest_opponent_pos.dist2( ball_next2 );
        }
        else if ( min_step == best_opponent_step )
        {
            //if ( best_ball_speed > ball_speed )
            double d2 = nearest_opponent_pos.dist2( ball_next2 );
            if ( d2 > best_opponent_dist2 )
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                              "%d: update(2)", a );
#endif
                best_ball_vel = ball_vel;
                best_opponent_step = min_step;
                //best_ball_speed = ball_speed;
                best_opponent_dist2 = d2;
            }
        }
    }

    s_best_ball_vel = best_ball_vel;
    return s_best_ball_vel;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_HoldBall::predict_opponents_reach_step( const WorldModel & wm,
                                             const int ball_step,
                                             const Vector2D & ball_pos )
{
    int min_step = 1000;

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        if ( (*o)->distFromSelf() > 5.0 )
        {
            break;
        }

        if ( (*o)->isTackling() )
        {
            continue;
        }

        int o_step = Bhv_HoldBall::predict_opponent_reach_step( *o, ball_step, ball_pos );

        if ( o_step < min_step )
        {
            min_step = o_step;
        }
    }

    return min_step;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_HoldBall::predict_opponents_reach_step( const WorldModel & wm,
                                             const Vector2D & ball_next1,
                                             const Vector2D & ball_next2 )
{
    int min_step = 1000;

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        if ( (*o)->distFromSelf() > 5.0 )
        {
            break;
        }

        if ( (*o)->isTackling() )
        {
            continue;
        }

        int o_step = Bhv_HoldBall::predict_opponent_reach_step( *o, 1, ball_next1 );
        if ( o_step <= 1 )
        {
            return 1;
        }

        o_step = Bhv_HoldBall::predict_opponent_reach_step( *o, 2, ball_next2 );

        if ( o_step < min_step )
        {
            min_step = o_step;
        }
    }

#ifdef DEBUG_PRINT_OPPONENT
    dlog.addText( Logger::ACTION_CHAIN | Logger::TEAM,
                  __FILE__":(predict_opponents_reach_step)"
                  " bpos1=(%.2f %.2f) bpos2=(%.2f %.2f)"
                  " opponent_step=%d",
                  ball_next1.x, ball_next1.y,
                  ball_next2.x, ball_next2.y,
                  min_step );
#endif

    return min_step;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_HoldBall::predict_opponent_reach_step( const PlayerObject * opponent,
                                            const int ball_step,
                                            const Vector2D & ball_pos )
{
    const ServerParam & SP = ServerParam::i();

    const PlayerType * ptype = opponent->playerTypePtr();
    const double control_area = ( opponent->goalie()
                                  && ball_pos.x > SP.theirPenaltyAreaLineX()
                                  && ball_pos.absY() < SP.penaltyAreaHalfWidth()
                                  ? SP.catchableArea()
                                  : ptype->kickableArea() );

    const Vector2D opponent_pos = opponent->inertiaPoint( ball_step );
    const double ball_dist = opponent_pos.dist( ball_pos );
    double dash_dist = ball_dist - control_area;

    if ( dash_dist < 0.001 )
    {
        return ball_step;
    }

    int o_dash = ptype->cyclesToReachDistance( dash_dist );
    if ( o_dash > ball_step )
    {
        return o_dash + 1;
    }

    int o_turn = 0;
    if ( opponent->bodyCount() == 0 )
    {
        o_turn = FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                           opponent->body(), // body dir
                                                           opponent->vel().r(),
                                                           ball_dist,
                                                           ( ball_pos - opponent_pos ).th(),
                                                           control_area,
                                                           true ); // use back dash
    }
    else if ( opponent->velCount() <= 1 )
    {
        o_turn = FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                           opponent->vel().th(), // vel dir
                                                           opponent->vel().r(),
                                                           ball_dist,
                                                           ( ball_pos - opponent_pos ).th(),
                                                           control_area,
                                                           true ); // use back dash
    }
    else
    {
        o_turn = 1;
    }

    return ( o_turn == 0
             ? o_dash
             : o_turn + o_dash + 1 );

}
