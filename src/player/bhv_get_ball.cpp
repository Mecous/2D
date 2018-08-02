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

#include "bhv_get_ball.h"

#include "defense_system.h"

#include "neck_check_ball_owner.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/view_synch.h>

#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/player_agent.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/soccer_math.h>
#include <rcsc/timer.h>

using namespace rcsc;

// #define DEBUG_PROFILE
// #define DEBUG_PRINT

namespace {

inline
void
debug_paint_target( const int count,
                    const Vector2D & pos )
{
    char msg[8];
    snprintf( msg, 8, "%d", count );
    dlog.addMessage( Logger::BLOCK,
                     pos, msg );
    dlog.addRect( Logger::BLOCK,
                  pos.x - 0.1, pos.y - 0.1, 0.2, 0.2,
                  "#F00" );
}

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_GetBall::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::BLOCK,
                  __FILE__": Bhv_GetBall" );

    const WorldModel & wm = agent->world();

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    //
    // check ball owner
    //
    if ( //mate_min <= self_min - 3
         //|| ( mate_min == 1 && self_min >= 3 )
        mate_min == 0
        || wm.kickableTeammate() )
    {
        dlog.addText( Logger::BLOCK,
                      __FILE__": teammate is faster than me." );
        return false;
    }

    //
    // intercept
    //
    if ( ! wm.kickableTeammate()
         && self_min <= mate_min + 1
         && self_min <= opp_min )
    {
        agent->debugClient().addMessage( "GetBallIntercept" );
        dlog.addText( Logger::BLOCK,
                      __FILE__":(execute) get ball intercept" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    if ( self_min <= 3
         && wm.ball().pos().x < -ServerParam::i().pitchHalfLength() + 3.0
         && wm.ball().pos().absY() < ServerParam::i().penaltyAreaHalfWidth() )
    {
        agent->debugClient().addMessage( "GetBallInterceptBack" );
        dlog.addText( Logger::BLOCK,
                      __FILE__":(execute) get ball intercept back" );
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }


    const Vector2D opp_trap_pos = DefenseSystem::get_block_opponent_trap_point( wm );
    const Vector2D center_pos = ( M_center_pos.isValid()
                                  ? M_center_pos
                                  : DefenseSystem::get_block_center_point( wm ) );

    const Line2D block_line( opp_trap_pos, center_pos );
    const Vector2D my_final = wm.self().inertiaFinalPoint();
    const double block_line_dist = block_line.dist( my_final );

    //double dist_thr = wm.self().playerType().kickableArea() - 0.2;
    double dist_thr = ( block_line_dist < 0.2
                        ? wm.self().playerType().kickableArea() - 0.2
                        : 0.5 );
    bool save_recovery = true;

    Param param;
    simulate( wm, opp_trap_pos, center_pos,
              M_bounding_rect, dist_thr, save_recovery,
              &param );

    agent->debugClient().addLine( opp_trap_pos, center_pos );
    agent->debugClient().addRectangle( M_bounding_rect );

    if ( ! param.point_.isValid() )
    {
        dlog.addText( Logger::BLOCK,
                      __FILE__":(execute) cannot find target point." );
        return false;
    }

    Vector2D target_point = param.point_;
    if ( param.cycle_ <= 0 )
    {
        target_point = opp_trap_pos * 0.3 + param.point_ * 0.7;
        dlog.addText( Logger::BLOCK,
                      __FILE__":(execute) cycle=0 set mid point" );
    }

    if ( wm.self().pos().dist( target_point ) < dist_thr + 0.2 )
    {
        dist_thr = 0.4;
    }

    dlog.addText( Logger::BLOCK,
                      __FILE__":(execute) move target=(%.1f %.1f) thr=%.3f cycle=%d stamina=%.1f",
                      param.point_.x, param.point_.y, dist_thr, param.cycle_, param.stamina_ );

    //
    // execute action
    //
    if ( Body_GoToPoint( target_point,
                         dist_thr,
                         ServerParam::i().maxDashPower(),
                         -1.0, // dash speed
                         std::max( 2, param.cycle_ ),
                         save_recovery,
                         15.0, // dir threshold
                         1.0, // omni dash threshold
                         false // no back dash
                         ).execute( agent ) )
    {
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );
        agent->debugClient().addMessage( "GetBallGoTo" );
        dlog.addText( Logger::BLOCK,
                      __FILE__": go to point (%.1f %.1f) thr=%.3f cycle=%d stamina=%.1f",
                      target_point.x, target_point.y,
                      dist_thr,
                      param.cycle_,
                      param.stamina_ );
    }
    else if ( opp_trap_pos.x < -30.0
              && Body_GoToPoint( target_point,
                                 dist_thr - 0.2, // reduced dist thr
                                 ServerParam::i().maxDashPower(),
                                 -1.0, // dash speed
                                 std::max( 2, param.cycle_ ),
                                 save_recovery,
                                 15.0, // dir threshold
                                 1.0, // omni dash threshold
                                 false // no back dash
                                 ).execute( agent ) )
    {
        agent->debugClient().setTarget( target_point );
        agent->debugClient().addCircle( target_point, dist_thr );
        agent->debugClient().addMessage( "GetBallGoTo2" );
        dlog.addText( Logger::BLOCK,
                      __FILE__": go to point(2) (%.1f %.1f) thr=%.3f cycle=%d stamina=%.1f",
                      target_point.x, target_point.y,
                      dist_thr,
                      param.cycle_,
                      param.stamina_ );
    }
    else
    {
        //AngleDeg body_angle = wm.ball().angleFromSelf() + 90.0;
        const PlayerObject * opponent = wm.interceptTable()->firstOpponent();
        AngleDeg body_angle = ( opponent
                                ? ( opponent->pos() + opponent->vel() - my_final ).th() + 90.0
                                : wm.ball().angleFromSelf() + 90.0 );
        if ( body_angle.abs() < 90.0 )
        {
            body_angle += 180.0;
        }
#if 0
        // The following side dash behavior makes performance worse.
        // Do NOT use this or improved the behavior.
        if ( ( body_angle - wm.self().body() ).abs() < 1.0 )
        {
            Vector2D opponent_rel = opponent->pos() + opponent->vel() - my_final;
            opponent_rel.rotate( -body_angle );
            AngleDeg dash_dir = ( opponent_rel.y < 0.0
                                  ? -90.0
                                  : +90.0 );
            double dash_power = wm.self().staminaModel().getSafetyDashPower( wm.self().playerType(),
                                                                             ServerParam::i().maxDashPower() );
            dash_power = std::min( dash_power, opponent_rel.absY() / wm.self().dashRate() );
            agent->doDash( dash_power, dash_dir );

            agent->debugClient().setTarget( target_point );
            agent->debugClient().addCircle( target_point, dist_thr );
            agent->debugClient().addMessage( "GetBallSideDash%.0f:%.0f",
                                             dash_power, dash_dir.degree() );
            dlog.addText( Logger::BLOCK,
                          __FILE__": side dash power=%.1f dir=%.1f",
                          dash_power, dash_dir.degree() );
        }
        else
#endif
        {
            Body_TurnToAngle( body_angle ).execute( agent );

            agent->debugClient().setTarget( target_point );
            agent->debugClient().addCircle( target_point, dist_thr );
            agent->debugClient().addMessage( "GetBallTurnTo%.0f",
                                             body_angle.degree() );
            dlog.addText( Logger::BLOCK,
                          __FILE__": turn angle=%.1f %.1f",
                          body_angle.degree() );
        }
    }

    if ( wm.ball().distFromSelf() < 4.0 )
    {
        agent->setViewAction( new View_Synch() );
    }
    agent->setNeckAction( new Neck_CheckBallOwner() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_GetBall::simulate( const WorldModel & wm,
                       const Vector2D & opp_trap_pos,
                       const Vector2D & center_pos,
                       const Rect2D & bounding_rect,
                       const double & dist_thr,
                       const bool save_recovery,
                       Param * param )
{
    const PlayerObject * opp = wm.interceptTable()->fastestOpponent();

    if ( ! opp )
    {
        dlog.addText( Logger::BLOCK,
                      __FILE__":(simulate) no fastest opponent" );
        return;
    }

#ifdef DEBUG_PROFILE
    MSecTimer timer;
#endif

    dlog.addText( Logger::BLOCK,
                  __FILE__":(simulate) center=(%.1f %.1f) dist_thr=%.2f",
                  center_pos.x, center_pos.y,
                  dist_thr );

    const double pitch_half_length = ServerParam::i().pitchHalfLength() - 1.0;
    const double pitch_half_width = ServerParam::i().pitchHalfWidth() - 1.0;

    const PlayerType * opp_type = opp->playerTypePtr();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    bool on_block_line = false;
    {
        const Segment2D block_segment( opp_trap_pos, center_pos );
        const Vector2D my_inertia = wm.self().inertiaFinalPoint();
        if ( block_segment.contains( my_inertia )
             && block_segment.dist( my_inertia ) < 0.5 )
        {
            on_block_line = true;
            dlog.addText( Logger::BLOCK,
                          __FILE__":(simulate) on block line after inertia move" );
        }
    }

    const Vector2D unit_vec = ( center_pos - opp_trap_pos ).setLengthVector( 1.0 );

    const int opp_penalty_step = ( on_block_line
                                   ? 3
                                   : opp_min == 0
                                   ? 0
                                   : 1 );

    Param best;

    const double min_length = 0.3;
    const double max_length = std::min( 30.0, opp_trap_pos.dist( center_pos ) ) + 1.0;
    double dist_step = 0.3;

    int count = 0;
    for ( double len = min_length;
          len < max_length;
          len += dist_step, ++count )
    {
        Vector2D target_point = opp_trap_pos + unit_vec * len;
        if ( len >= 1.5 ) dist_step = 1.0;

#ifdef DEBUG_PRINT
        dlog.addText( Logger::BLOCK,
                      "(%d) len=%.2f pos=(%.1f %.1f)",
                      count, len, target_point.x, target_point.y );
#endif
        //
        // region check
        //
        if ( ! bounding_rect.contains( target_point ) )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::BLOCK,
                          "xx out of the bounding rect (%.1f %.1f)(%.1f %.1f)",
                          bounding_rect.left(), bounding_rect.top(),
                          bounding_rect.right(), bounding_rect.bottom() );
#endif
            continue;
        }

        if ( target_point.absX() > pitch_half_length
             || target_point.absY() > pitch_half_width )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::BLOCK,
                          "xx out of the pitch" );
#endif
            continue;
        }

        //
        // predict self cycle
        //
        double stamina = 0.0;
        int n_turn = 0;
        int self_cycle = predictSelfReachCycle( wm,
                                                target_point,
                                                dist_thr,
                                                save_recovery,
                                                &n_turn,
                                                &stamina );
        if ( self_cycle > 100 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::BLOCK,
                          "xx cannot reach" );
#endif
            continue;
        }

        double opp_dist = std::max( 0.0,
                                    opp_trap_pos.dist( target_point ) - opp_type->kickableArea() * 0.8 );
        int opp_cycle
            = opp_min
            + opp_penalty_step
            + opp_type->cyclesToReachDistance( opp_dist )
            - std::min( 1, opp->posCount() );
#ifdef DEBUG_PRINT
        debug_paint_target( count, target_point );
        dlog.addText( Logger::BLOCK,
                      "__ pos=(%.1f %.1f) opp_dist=%.3f opp_cycle=%d(%d:%d) self_cycle=%d(t:%d) stamina=%.1f",
                      target_point.x, target_point.y,
                      opp_dist,
                      opp_cycle, opp_min, opp_penalty_step,
                      self_cycle, n_turn,
                      stamina );
#endif
        if ( self_cycle <= opp_cycle - 2
             || ( self_cycle <= 3 && self_cycle <= opp_cycle - 1 )
             || ( self_cycle <= 2 && self_cycle <= opp_cycle )
//         if ( self_cycle <= opp_cycle - 2
//              || ( self_cycle <= 1 && self_cycle < opp_cycle )
//              || ( opp_cycle >= 8 && self_cycle <= opp_cycle - 1 )
//              || ( opp_cycle >= 12 && self_cycle <= opp_cycle )
//              || ( opp_cycle >= 16 && self_cycle <= opp_cycle + 1 )
             )
        {
            best.point_ = target_point;
            best.turn_ = n_turn;
            best.cycle_ = self_cycle;
            best.stamina_ = stamina;
            break;
        }
    }

#if 1
    if ( ! on_block_line
         && best.turn_ > 0 )
    {
        simulateNoTurn( wm, opp_trap_pos, center_pos,
                        bounding_rect, dist_thr, save_recovery,
                        &best );
    }
#endif


    if ( best.point_.isValid() )
    {
        *param = best;
#ifdef DEBUG_PROFILE
        dlog.addText( Logger::BLOCK,
                      __FILE__": simulate() elapsed %.3f [ms]. reach_point=(%.1f %.1f) reach_cycle=%d",
                      timer.elapsedReal(),
                      best.point_.x, best.point_.y,
                      best.cycle_ );
#endif
    }
#ifdef DEBUG_PROFILE
    else
    {
        dlog.addText( Logger::BLOCK,
                      __FILE__": simulate() elapsed %.3f [ms]. XXX no reach point XXX",
                      timer.elapsedReal() );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_GetBall::simulateNoTurn( const WorldModel & wm,
                             const Vector2D & opponent_trap_pos,
                             const Vector2D & center_pos,
                             const Rect2D & bounding_rect,
                             const double & dist_thr,
                             const bool save_recovery,
                             Param * param )
{
    const PlayerObject * opponent = wm.interceptTable()->fastestOpponent();

    if ( ! opponent )
    {
        dlog.addText( Logger::BLOCK,
                      __FILE__":(simulate) no fastest opponent" );
        return;
    }

    const Segment2D block_segment( opponent_trap_pos, center_pos );
    const Line2D my_move_line( wm.self().pos(), wm.self().body() );

    const Vector2D intersect_point = block_segment.intersection( my_move_line );
    if ( ! intersect_point.isValid()
         || ! bounding_rect.contains( intersect_point )
         || ( ( intersect_point - wm.self().pos() ).th() - wm.self().body() ).abs() > 5.0 )
    {
        dlog.addText( Logger::BLOCK,
                      __FILE__": (simulateNoTurn) no intersection" );
        return;
    }

    {
        //const Vector2D ball_pos = wm.ball().inertiaPoint( wm.interceptTable()->opponentReachCycle() );

        if ( param->point_.isValid() // already solution exists
             && param->point_.dist2( intersect_point ) > std::pow( 3.0, 2 )
             //|| my_move_line.dist( ball_pos ) > wm.self().playerType().kickableArea() + 0.3 )
             )
        {
            dlog.addText( Logger::BLOCK,
                          __FILE__": (simulateNoTurn) intersection too far" );
            return;
        }
    }

    const PlayerType * opponent_type = opponent->playerTypePtr();
    const double opponent_dist = std::max( 0.0,
                                           opponent_trap_pos.dist( intersect_point )
                                           - opponent_type->kickableArea() * 0.8 );
    const int opponent_step
        = wm.interceptTable()->opponentReachCycle()
        + opponent_type->cyclesToReachDistance( opponent_dist )
        + 1; // magic number

    const int max_step = std::min( param->cycle_ + 2, opponent_step );

    dlog.addText( Logger::BLOCK,
                  __FILE__": (simulateNoTurn) opponent_trap=(%.1f %.1f)",
                  opponent_trap_pos.x, opponent_trap_pos.y );
    dlog.addText( Logger::BLOCK,
                  __FILE__": (simulateNoTurn) intersection=(%.1f %.1f) opponent_step=%d",
                  intersect_point.x, intersect_point.y,
                  opponent_step );

    dlog.addText( Logger::BLOCK,
                  __FILE__": (simulateNoTurn) max_step=%d param_step=%d",
                  max_step, param->cycle_ );
    //
    //
    //
    for ( int n_dash = 1; n_dash <= max_step; ++n_dash )
    {
        const Vector2D inertia_pos = wm.self().inertiaPoint( n_dash );
        const double target_dist = inertia_pos.dist( intersect_point );

        const int dash_step = wm.self().playerType().cyclesToReachDistance( target_dist - dist_thr*0.5 );

        if ( dash_step <= n_dash )
        {
            StaminaModel stamina = wm.self().staminaModel();
            stamina.simulateDashes( wm.self().playerType(),
                                    n_dash,
                                    ServerParam::i().maxDashPower() );
            if ( save_recovery
                 && stamina.recovery() < ServerParam::i().recoverInit() )
            {
                dlog.addText( Logger::BLOCK,
                              __FILE__": (simulateNoTurn) dash=%d recover decay",
                              n_dash );
            }
            else
            {
                dlog.addText( Logger::BLOCK,
                              __FILE__": (simulateNoTurn) dash=%d found stamina=%.1f",
                              n_dash, stamina.stamina() );
                debug_paint_target( -1, intersect_point );

                param->point_ = intersect_point;
                param->turn_ = 0;
                param->cycle_ = n_dash;
                param->stamina_ = stamina.stamina();
            }

            break;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
int
Bhv_GetBall::predictSelfReachCycle( const WorldModel & wm,
                                    const Vector2D & target_point,
                                    const double & dist_thr,
                                    const bool save_recovery,
                                    int * turn_step,
                                    double * stamina )
{
    const ServerParam & param = ServerParam::i();
    const PlayerType & self_type = wm.self().playerType();
    const double max_moment = param.maxMoment();
    const double recover_dec_thr = param.staminaMax() * param.recoverDecThr();

    const double first_my_speed = wm.self().vel().r();

    for ( int cycle = 0; cycle < 100; ++cycle )
    {
        const Vector2D inertia_pos = wm.self().inertiaPoint( cycle );
        double target_dist = ( target_point - inertia_pos ).r();
        if ( target_dist - dist_thr > self_type.realSpeedMax() * cycle )
        {
            continue;
        }

        int n_turn = 0;
        int n_dash = 0;

        AngleDeg target_angle = ( target_point - inertia_pos ).th();
        double my_speed = first_my_speed;

        //
        // turn
        //
        double angle_diff = ( target_angle - wm.self().body() ).abs();
        double turn_margin = 180.0;
        if ( dist_thr < target_dist )
        {
            turn_margin = std::max( 15.0,
                                    AngleDeg::asin_deg( dist_thr / target_dist ) );
        }

        while ( angle_diff > turn_margin )
        {
            angle_diff -= self_type.effectiveTurn( max_moment, my_speed );
            my_speed *= self_type.playerDecay();
            ++n_turn;
        }

        StaminaModel stamina_model = wm.self().staminaModel();

#if 0
        // TODO: stop dash
        if ( n_turn >= 3 )
        {
            Vector2D vel = wm.self().vel();
            vel.rotate( - wm.self().body() );
            Vector2D stop_accel( -vel.x, 0.0 );

            double dash_power
                = stop_accel.x / ( self_type.dashPowerRate() * my_effort );
            double stop_stamina = stop_dash_power;
            if ( stop_dash_power < 0.0 )
            {
                stop_stamina *= -2.0;
            }

            if ( save_recovery )
            {
                if ( stop_stamina > my_stamina - recover_dec_thr )
                {
                    stop_stamina = std::max( 0.0, my_stamina - recover_dec_thr );
                }
            }
            else if ( stop_stamina > my_stamina )
            {
                stop_stamina = my_stamina;
            }

            if ( stop_dash_power < 0.0 )
            {
                dash_power = -stop_stamina * 0.5;
            }
            else
            {
                dash_power = stop_stamina;
            }

            stop_accel.x = dash_power * self_type.dashPowerRate() * my_effort;
            vel += stop_accel;
            my_speed = vel.r();
        }
#endif

        AngleDeg dash_angle = wm.self().body();
        if ( n_turn > 0 )
        {
            angle_diff = std::max( 0.0, angle_diff );
            dash_angle = target_angle;
            if ( ( target_angle - wm.self().body() ).degree() > 0.0 )
            {
                dash_angle -= angle_diff;
            }
            else
            {
                dash_angle += angle_diff;
            }

            stamina_model.simulateWaits( self_type, n_turn );
        }

        //
        // dash
        //

        Vector2D my_pos = inertia_pos;
        Vector2D vel = wm.self().vel() * std::pow( self_type.playerDecay(), n_turn );
        while ( n_turn + n_dash < cycle
                && target_dist > dist_thr )
        {
            double dash_power = std::min( param.maxDashPower(),
                                          stamina_model.stamina() + self_type.extraStamina() );
            if ( save_recovery
                 && stamina_model.stamina() - dash_power < recover_dec_thr )
            {
                dash_power = std::max( 0.0, stamina_model.stamina() - recover_dec_thr );
            }

            Vector2D accel = Vector2D::polar2vector( dash_power
                                                     * self_type.dashPowerRate()
                                                     * stamina_model.effort(),
                                                     dash_angle );
            vel += accel;
            double speed = vel.r();
            if ( speed > self_type.playerSpeedMax() )
            {
                vel *= self_type.playerSpeedMax() / speed;
            }

            my_pos += vel;
            vel *= self_type.playerDecay();

            stamina_model.simulateDash( self_type, dash_power );

            target_dist = my_pos.dist( target_point );
            ++n_dash;
        }

        if ( target_dist <= dist_thr
             || inertia_pos.dist2( target_point ) < inertia_pos.dist2( my_pos ) )
        {
            if ( turn_step )
            {
                *turn_step = n_turn;
            }

            if ( stamina )
            {
                *stamina = stamina_model.stamina();
            }

            //             dlog.addText( Logger::BLOCK,
            //                           "____ cycle=%d n_turn=%d n_dash=%d stamina=%.1f",
            //                           cycle,
            //                           n_turn, n_dash,
            //                           my_stamina );
            return n_turn + n_dash;
        }
    }

    return 1000;
}
