// -*-c++-*-

/*!
  \file bhv_self_pass_penalty_kick.cpp
  \brief pass to self action
*/

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

#include "bhv_self_pass_penalty_kick.h"

#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/body_intercept.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/soccer_intention.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/soccer_math.h>
#include <rcsc/math_util.h>
#include <rcsc/timer.h>

#include "neck_offensive_intercept_neck.h"

using namespace rcsc;

#define DEBUG_PROFILE
// #define DEBUG_PRINT

/*-------------------------------------------------------------------*/
class IntentionSelfPassPenaltyKick
    : public SoccerIntention {
private:

    const Vector2D M_target_point;
    int M_step;
    int M_count;

public:

    IntentionSelfPassPenaltyKick( const Vector2D & target_point,
                       const int step )
        : M_target_point( target_point )
        , M_step( step )
        , M_count( 0 )
      { }

    bool finished( const PlayerAgent * agent );

    bool execute( PlayerAgent * agent );

};

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionSelfPassPenaltyKick::finished( const PlayerAgent * agent )
{
    if ( M_step <= 0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() empty queue" );
        return true;
    }

    const WorldModel & wm = agent->world();

    if ( M_count >= 1
         && wm.self().isKickable() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() kickable" );
        return true;
    }

    if ( wm.kickableOpponent() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() exist kickable player" );
        return true;
    }

    Vector2D ball_rel = ( wm.ball().pos() - wm.self().pos() );
    ball_rel.rotate( - wm.self().body() );
    if ( ball_rel.x < 0.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished() ball is back. rel=(%.2f %.2f)",
                      ball_rel.x, ball_rel.y );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionSelfPassPenaltyKick::execute( PlayerAgent * agent )
{
    --M_step;
    ++M_count;

    if ( M_count <= 3 )
    {
        if ( Body_GoToPoint( M_target_point, 0.3,
                             ServerParam::i().maxDashPower()
                             ).execute( agent ) )
        {
            agent->setNeckAction( new Neck_ScanField() );

            agent->debugClient().addMessage( "I_SelfPassPKGoTo" );
            agent->debugClient().setTarget( M_target_point );
            dlog.addText( Logger::TEAM,
                          __FILE__": intention. go to (%.1f %1.f) ",
                          M_target_point.x, M_target_point.y );
            return true;
        }
    }
    else
    {
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );

        agent->debugClient().addMessage( "I_SelfPassPKIntercept" );
        agent->debugClient().setTarget( M_target_point );
        dlog.addText( Logger::TEAM,
                      __FILE__": intention. intercept (%.1f %1.f) ",
                      M_target_point.x, M_target_point.y );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SelfPassPenaltyKick::execute( PlayerAgent * agent )
{
    if ( ! agent->world().self().isKickable() )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << " not ball kickable!"
                  << std::endl;
        dlog.addText( Logger::TEAM,
                      __FILE__":  not kickable" );
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_SelfPassPenaltyKick" );

    const WorldModel & wm = agent->world();

#ifdef DEBUG_PROFILE
    MSecTimer timer;
#endif

    const Ray2D body_ray( wm.self().pos(), wm.self().body() );
    const double goal_area_x
        = ServerParam::i().pitchHalfLength()
        - ServerParam::i().goalAreaLength() * 0.5;
    const Rect2D target_area = Rect2D::from_center( goal_area_x,
                                                    0.0,
                                                    ServerParam::i().penaltyAreaLength(),
                                                    ServerParam::i().pitchWidth() );

    bool done_straight = false;

    if ( wm.self().pos().x < 35.0
         || wm.self().pos().absY() < ServerParam::i().goalHalfWidth() )
    {
        if ( wm.self().body().abs() < 15.0
             || target_area.intersection( body_ray, NULL, NULL ) > 0 )
        {
            done_straight = true;
            agent->debugClient().addRectangle( target_area );
            if ( doKickDashes( agent ) )
            {
#ifdef DEBUG_PROFILE
                dlog.addText( Logger::TEAM,
                              __FILE__": self_pass_penalty_kick_elapsed(1)=%.3f [ms]",
                              timer.elapsedReal() );
#endif
                return true;
            }
        }
    }

    //     //for ( double r = 0.0; r < 1.2; r += 0.1 )
    //     for ( double r = 0.9; r >= 0.0; r -= 0.1 )
    //     {
    //         Vector2D target_point( 42.0, wm.self().pos().y * r );
    //         if ( doKickTurnDashes( agent, target_point ) )
    //         {
    // #ifdef DEBUG_PROFILE
    //             dlog.addText( Logger::TEAM,
    //                                 __FILE__": self_pass_penalty_kick_elapsed(2)=%.3f [ms]",
    //                                 timer.elapsedReal() );
    // #endif
    //             return true;
    //         }
    //     }

    const double target_x = 42.0;
    std::vector< double > target_y;

    if ( wm.self().pos().x > 20.0 )
    {
        for ( double y = 0.0;
              y < wm.self().pos().absY() && y < 30.0;
              y += 5.0 )
        {
            target_y.push_back( y );
        }
        target_y.push_back( wm.self().pos().absY() );
    }
    else
    {
        for ( double y = wm.self().pos().absY();
              y > 0.0;
              y -= 5.0 )
        {
            target_y.push_back( y );
        }
        target_y.push_back( 0.0 );
    }

    for ( std::vector< double >::iterator y = target_y.begin();
          y != target_y.end();
          ++y )
    {
        Vector2D target_point( target_x, *y );
        if ( wm.self().pos().y < 0.0 ) target_point.y *= -1.0;
        //if ( std::fabs( target_point.y - wm.self().pos().y ) > 16.0 ) continue;
        if ( ( target_point - wm.self().pos() ).th().abs() > 40.0 ) continue;

#ifdef DEBUG_PROFILE
        dlog.addText( Logger::TEAM,
                      __FILE__": doKickTurnDashes target=(%.2f %.2f)",
                      target_point.x, target_point.y );
#endif

        if ( doKickTurnDashes( agent, target_point ) )
        {
#ifdef DEBUG_PROFILE
            dlog.addText( Logger::TEAM,
                          __FILE__": self_pass_penalty_kick_elapsed(2)=%.3f [ms]",
                          timer.elapsedReal() );
#endif
            return true;
        }
    }

    if ( ! done_straight )
    {
        if ( wm.self().body().abs() < 15.0
             || target_area.intersection( body_ray, NULL, NULL ) > 0 )
        {
            done_straight = true;
            agent->debugClient().addRectangle( target_area );
            if ( doKickDashes( agent ) )
            {
#ifdef DEBUG_PROFILE
                dlog.addText( Logger::TEAM,
                              __FILE__": self_pass_elapsed(3)=%.3f [ms]",
                              timer.elapsedReal() );
#endif
                return true;
            }
        }
    }

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::TEAM,
                  __FILE__": self_pass_elapsed(4)=%.3f [ms]",
                  timer.elapsedReal() );
#endif
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SelfPassPenaltyKick::doKickDashes( PlayerAgent * agent )
{
    static std::vector< Vector2D > self_state;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM,
                  "SelfPassPK (doKickDashes)" );
#endif

    const WorldModel & wm = agent->world();

    const int min_dash = ( wm.ball().pos().x < 32.5
                           ? 6
                           : 4 );

    createSelfCache( agent, 0, 5,
                     wm.self().body(),
                     self_state ); // no_turn, max_dash=18

    int n_dash = self_state.size();

    if ( n_dash < min_dash )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      __FILE__": too short dash step %d",
                      n_dash );
#endif
        return false;
    }

    int dash_dec = 2;
    for ( ; n_dash >= min_dash; n_dash -= dash_dec )
    {
        const Vector2D receive_pos = self_state[n_dash - 1];

        if ( ! canKick( wm, 0, n_dash, receive_pos ) )
        {
            continue;
        }

        if ( ! checkOpponent( agent, 0, n_dash, receive_pos ) )
        {
            continue;
        }

        if ( doKick( agent, 0, n_dash, receive_pos ) )
        {
            return true;
        }

        if ( n_dash <= 8 )
        {
            dash_dec = 1;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SelfPassPenaltyKick::doKickTurnDashes( PlayerAgent * agent,
                                const Vector2D & target_point )
{
    static std::vector< Vector2D > self_state;

    const WorldModel & wm = agent->world();

    const int min_dash = ( wm.ball().pos().x < 32.5
                           ? 6
                           : 4 );

    const PlayerType & ptype = wm.self().playerType();
    const Vector2D my_pos = wm.self().inertiaFinalPoint();
    const AngleDeg target_angle = ( target_point - my_pos ).th();

    //
    // check the required turn step
    //
    const double angle_diff = ( target_angle - wm.self().body() ).abs();
    if ( angle_diff > 120.0 )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      "xx (doKickTurnDashes) target_point=(%.1f %.1f)"
                      " too big angle_diff=%.1f",
                      target_point.x, target_point.y,
                      angle_diff );
#endif
        return false;
    }

    {
        double turn_margin = 180.0;
        double target_dist = my_pos.dist( target_point );
        if ( ptype.kickableArea() < target_dist )
        {
            turn_margin = AngleDeg::asin_deg( wm.self().playerType().kickableArea() / target_dist );
        }
        turn_margin = std::max( turn_margin, 15.0 );

        if ( turn_margin > angle_diff )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          "xx (doKickTurnDashes) target_point=(%.1f %.1f)"
                          " too small angle_diff=%.1f < turn_margin=%.1f",
                          target_point.x, target_point.y,
                          angle_diff, turn_margin );
#endif
            return false;
        }
    }

    if ( angle_diff
         > ptype.effectiveTurn( ServerParam::i().maxMoment(),
                                wm.self().vel().r() * ptype.playerDecay() ) )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      "xx (doKickTurnDashes) target_point=(%.1f %.1f)"
                      " cannot turn by one step."
                      " angle_diff=%.1f",
                      target_point.x, target_point.y,
                      angle_diff );
#endif
        return false;
    }

    int n_turn = 1;

    createSelfCache( agent,
                     n_turn, 5, // turn=1, max_dash=12
                     target_angle,
                     self_state );

    int n_dash = self_state.size() - n_turn;

    if ( n_dash < min_dash )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      "xx (doKickTurnDashes) too short dash step %d",
                      n_dash );
#endif
        return false;
    }

    int dash_dec = 2;
    for ( ; n_dash >= min_dash; n_dash -= dash_dec )
    {
        const Vector2D receive_pos = self_state[n_turn + n_dash - 1];

        if ( ! canKick( wm, 1, n_dash, receive_pos ) )
        {
            continue;
        }

        if ( ! checkOpponent( agent, 1, n_dash, receive_pos ) )
        {
            continue;
        }

        if ( doKick( agent, 1, n_dash, receive_pos ) )
        {
            return true;
        }

        if ( n_dash <= 8 )
        {
            dash_dec = 1;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SelfPassPenaltyKick::createSelfCache( PlayerAgent * agent,
                               const int n_turn,
                               const int n_dash,
                               const AngleDeg & accel_angle,
                               std::vector< Vector2D > & self_state )
{
    self_state.clear();
    self_state.reserve( n_turn + n_dash );

    const WorldModel & wm = agent->world();
    const PlayerType & ptype = wm.self().playerType();

    const double dash_power = ServerParam::i().maxDashPower();
    const double stamina_thr = ( wm.self().staminaModel().capacityIsEmpty()
                                 ? -ptype.extraStamina()
                                 : ServerParam::i().recoverDecThrValue() + 350.0 );

    StaminaModel stamina_model = wm.self().staminaModel();

    Vector2D my_pos = wm.self().pos();
    Vector2D my_vel = wm.self().vel();

    my_pos += my_vel;
    my_vel *= ptype.playerDecay();

    for ( int i = 0; i < n_turn; ++i )
    {
        my_pos += my_vel;
        my_vel *= ptype.playerDecay();
        self_state.push_back( my_pos );
    }

    stamina_model.simulateWaits( ptype, 1 + n_turn );

    for ( int i = 0; i < n_dash; ++i )
    {
        if ( stamina_model.stamina() < stamina_thr )
        {
            break;
        }

        double available_stamina =  std::max( 0.0, stamina_model.stamina() - stamina_thr );
        double consumed_stamina = dash_power;
        consumed_stamina = std::min( available_stamina,
                                     consumed_stamina );
        double used_power = consumed_stamina;
        double max_accel_mag = ( std::fabs( used_power )
                                 * ptype.dashPowerRate()
                                 * stamina_model.effort() );
        double accel_mag = max_accel_mag;
        if ( ptype.normalizeAccel( my_vel,
                                   accel_angle,
                                   &accel_mag ) )
        {
            used_power *= accel_mag / max_accel_mag;
        }

        Vector2D dash_accel
            = Vector2D::polar2vector( std::fabs( used_power )
                                      * stamina_model.effort()
                                      * ptype.dashPowerRate(),
                                      accel_angle );
        my_vel += dash_accel;
        my_pos += my_vel;

        if ( my_pos.x > ServerParam::i().pitchHalfLength() - 3.5 )
        {
            break;
        }

        const AngleDeg target_angle = ( my_pos - wm.ball().pos() ).th();

        if ( my_pos.absY() > ServerParam::i().pitchHalfWidth() - 3.5
             &&  ( ( my_pos.y > 0.0 && target_angle.degree() > 0.0 )
                   || ( my_pos.y < 0.0 && target_angle.degree() < 0.0 )
                   )
             )
        {
            break;
        }

        my_vel *= ptype.playerDecay();

        stamina_model.simulateDash( ptype, used_power );

        self_state.push_back( my_pos );
    }

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SelfPassPenaltyKick::checkOpponent( PlayerAgent * agent,
                                        const int n_turn,
                                        const int n_dash,
                                        const Vector2D & receive_pos )
{
    const WorldModel & wm = agent->world();

    const double max_moment = ServerParam::i().maxMoment();

    const AngleDeg target_angle = ( receive_pos - wm.ball().pos() ).th();

    const int pos_count_penalty = ( receive_pos.x > wm.offsideLineX()
                                    ? 1
                                    : 0 );

    /*
      const PlayerPtrCont::const_iterator o_end = wm.opponentsFromSelf().end();
      for ( PlayerPtrCont::const_iterator o = wm.opponentsFromSelf().begin();
      o != o_end;
      ++o )
      {
    */
    const AbstractPlayerObject * opp_goalie = wm.getTheirGoalie();
    if ( opp_goalie )
    {

        const PlayerType * player_type = opp_goalie->playerTypePtr();

        const double control_area = ServerParam::i().catchableArea();

        const Vector2D & opos = ( opp_goalie->seenPosCount() <= opp_goalie->posCount()
                                  ? opp_goalie->seenPos()
                                  : opp_goalie->pos() );
        const int vel_count = std::min( opp_goalie->seenVelCount(), opp_goalie->velCount() );
        const Vector2D & ovel = ( opp_goalie->seenVelCount() <= opp_goalie->velCount()
                                  ? opp_goalie->seenVel()
                                  : opp_goalie->vel() );

        Vector2D opp_pos = ( vel_count <= 1
                             ? inertia_n_step_point( opos, ovel,
                                                     n_turn + n_dash + 1,
                                                     player_type->playerDecay() )
                             : opos + ovel );
        Vector2D opp_to_target = receive_pos - opp_pos;

        double opp_to_target_dist = opp_to_target.r();

        int opp_turn_step = 0;
        if ( opp_goalie->bodyCount() <= 5
             || ( vel_count <= 5
                  && opp_goalie->distFromSelf() < 5.0 ) )
        {
            double angle_diff = ( opp_goalie->bodyCount() <= 1
                                  ? ( opp_to_target.th() - opp_goalie->body() ).abs()
                                  : ( opp_to_target.th() - ovel.th() ).abs() );

            double turn_margin = 180.0;
            if ( control_area < opp_to_target_dist )
            {
                turn_margin = AngleDeg::asin_deg( control_area / opp_to_target_dist );
            }
            turn_margin = std::max( turn_margin, 15.0 );

            double opp_speed = ovel.r();

#if 0
            dlog.addText( Logger::TEAM,
                          "____ (checkOpponent) angle_diff=%.3f turn_margin=%.3f opp_speed=%.3f",
                          angle_diff, turn_margin, opp_speed );
#endif

            while ( angle_diff > turn_margin )
            {
                angle_diff -= player_type->effectiveTurn( max_moment, opp_speed );
                opp_speed *= opp_goalie->playerTypePtr()->playerDecay();
                ++opp_turn_step;
            }
        }

        opp_to_target_dist -= control_area;
        opp_to_target_dist -= 0.2;
        opp_to_target_dist -= opp_goalie->distFromSelf() * 0.01;

        if ( opp_to_target_dist < 0.0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          "xxxx (checkOpponent) n_turn=%d n_dash=%d"
                          " opponent %d(%.1f %.1f) is already at receive point"
                          " (%.1f %.1f)",
                          n_turn,
                          n_dash,
                          opp_goalie->unum(),
                          opp_goalie->pos().x, opp_goalie->pos().y,
                          receive_pos.x, receive_pos.y );
#endif
            return false;
        }

        int opp_reach_step = player_type->cyclesToReachDistance( opp_to_target_dist );
        opp_reach_step += opp_turn_step;
        //         if ( opp_turn_step > 0 )
        //         {
        //             opp_reach_step += 1;
        //         }

        //if ( ( ( receive_pos - (*o)->pos() ).th() - target_angle ).abs() < 90.0 )
        if ( ( ( opp_goalie->pos() - wm.ball().pos() ).th() - target_angle ).abs() < 90.0 )
        {
            opp_reach_step -= bound( 0, opp_goalie->posCount(), 5 );
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          "____ (checkOpponent) step bonus(1) small angle diff" );
#endif
        }
        else
        {
            opp_reach_step -= bound( 0, opp_goalie->posCount() - pos_count_penalty, 1 );
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          "____ (checkOpponent) step bonus(1) big angle diff" );
#endif
        }

        opp_reach_step -= 1;

        if ( ( receive_pos.x < opp_goalie->pos().x // receive_pos.x < wm.offsideLineX()
               && opp_reach_step <= n_turn + n_dash + 2 )
             || opp_reach_step <= n_turn + n_dash )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          "xxxx (checkOpponent) n_turn=%d n_dash=%d"
                          " opponent %d (%.1f %.1f) can reach faster then self."
                          " target=(%.1f %.1f) opp_step=%d opp_turn=%d",
                          n_turn,
                          n_dash,
                          opp_goalie->unum(),
                          opp_goalie->pos().x, opp_goalie->pos().y,
                          receive_pos.x, receive_pos.y,
                          opp_reach_step,
                          opp_turn_step );
#endif
            return false;
        }
#ifdef DEBUG_PRINT
        else
        {
            dlog.addText( Logger::TEAM,
                          "____ ok (checkOpponent) n_turn=%d n_dash=%d"
                          " opponent %d (%.1f %.1f)"
                          " target=(%.1f %.1f) opp_step=%d opp_turn=%d",
                          n_turn,
                          n_dash,
                          opp_goalie->unum(),
                          opp_goalie->pos().x, opp_goalie->pos().y,
                          receive_pos.x, receive_pos.y,
                          opp_reach_step,
                          opp_turn_step );
        }
#endif

    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SelfPassPenaltyKick::canKick( const WorldModel & wm,
                                  const int n_turn,
                                  const int n_dash,
                                  const Vector2D & receive_pos )
{
    const AngleDeg target_angle = ( receive_pos - wm.ball().pos() ).th();

    //
    // check kick possibility
    //
    double first_speed = calc_first_term_geom_series( wm.ball().pos().dist( receive_pos ),
                                                      ServerParam::i().ballDecay(),
                                                      1 + n_turn + n_dash );
    Vector2D max_vel = KickTable::calc_max_velocity( target_angle,
                                                     wm.self().kickRate(),
                                                     wm.ball().vel() );
    if ( max_vel.r() < first_speed )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      "____ selfPass canKick n_turn=%d n_dash=%d cannot kick by one step."
                      " first_speed=%.2f > max_speed=%.2f",
                      n_turn,
                      n_dash,
                      first_speed,
                      max_vel.r() );
#endif
        return false;
    }

    //
    // check collision
    //
    const Vector2D my_next = wm.self().pos() + wm.self().vel();
    const Vector2D ball_next = wm.ball().pos()
        + ( receive_pos - wm.ball().pos() ).setLengthVector( first_speed );

    if ( my_next.dist( ball_next ) < ( wm.self().playerType().playerSize()
                                       + ServerParam::i().ballSize() + 0.1 ) )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      "____ selfPass n_turn=%d n_dash=%d maybe collision. first_speed=%.2f",
                      n_turn,
                      n_dash,
                      first_speed );
#endif
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SelfPassPenaltyKick::doKick( PlayerAgent * agent,
                                 const int n_turn,
                                 const int n_dash,
                                 const Vector2D & receive_pos )
{
    const WorldModel & wm = agent->world();

    double first_speed = calc_first_term_geom_series( wm.ball().pos().dist( receive_pos ),
                                                      ServerParam::i().ballDecay(),
                                                      1 + n_turn + n_dash );

    //     AngleDeg target_angle = ( receive_pos - wm.ball().pos() ).th();
    //     Vector2D max_vel = KickTable::calc_max_velocity( target_angle,
    //                                                                  wm.self().kickRate(),
    //                                                                  wm.ball().vel() );
    //     if ( max_vel.r() < first_speed )
    //     {
    //         dlog.addText( Logger::TEAM,
    //                             "__ selfPass cannot kick by one step. first_speed=%.2f > max_speed=%.2f",
    //                             first_speed,
    //                             max_vel.r() );
    //         return false;
    //     }

    //     Vector2D ball_next = wm.ball().pos()
    //         + ( receive_pos - wm.ball().pos() ).setLengthVector( first_speed );
    //     Vector2D my_next = wm.self().pos() + wm.self().vel();
    //     if ( my_next.dist( ball_next ) < ( wm.self().playerType().playerSize()
    //                                        + ServerParam::i().ballSize() + 0.1 ) )
    //     {
    //         dlog.addText( Logger::TEAM,
    //                             "__ selfPass maybe collision. first_speed=%.2f",
    //                             first_speed );
    //         return false;
    //     }

    if ( Body_SmartKick( receive_pos,
                         first_speed,
                         first_speed * 0.99,
                         1 ).execute( agent ) )
    {
        agent->setViewAction( new View_Wide() );
        agent->setNeckAction( new Neck_TurnToBallOrScan( 2 ) );

#ifdef DEBUG_PRINT
        dlog.addText( Logger::TEAM,
                      "__ selfPass n_turn=%d n_dash=%d receive_pos=(%.1f %.1f) first_speed=%.2f",
                      n_turn,
                      n_dash,
                      receive_pos.x, receive_pos.y,
                      first_speed );
#endif

        agent->debugClient().addMessage( "SelfPassPK%d+%d", n_turn, n_dash );
        agent->debugClient().setTarget( receive_pos );

        agent->setIntention( new IntentionSelfPassPenaltyKick( receive_pos, n_turn + n_dash ) );

        return true;
    }

#ifdef DEBUG_PRINT
    dlog.addText( Logger::TEAM,
                  "__ selfPass failed smart kick. n_turn=%d n_dash=%d"
                  " receive_pos=(%.1f %.1f) first_speed=%.2f",
                  n_turn,
                  n_dash,
                  receive_pos.x, receive_pos.y,
                  first_speed );
#endif
    return false;
}
