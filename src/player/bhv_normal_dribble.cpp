// -*-c++-*-

/*!
  \file bhv_normal_dribble.cpp
  \brief normal dribble action that uses DribbleTable
*/

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

#include "bhv_normal_dribble.h"

#include "action_chain_holder.h"
#include "action_chain_graph.h"
#include "cooperative_action.h"

#include "act_dribble.h"
#include "generator_short_dribble.h"

#include "neck_offensive_intercept_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/view_synch.h>

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

class IntentionNormalDribble
    : public SoccerIntention {
private:
    const Vector2D M_target_point; //!< trapped ball position
    const AngleDeg M_dash_dir; //!< dash direction relative to the body angle

    int M_kick_step; //!< remained kick step
    int M_turn_step; //!< remained turn step
    int M_dash_step; //!< remained dash step

    GameTime M_last_execute_time; //!< last executed time

    NeckAction::Ptr M_neck_action;
    ViewAction::Ptr M_view_action;

public:

    IntentionNormalDribble( const Vector2D & target_point,
                            const AngleDeg & dash_dir,
                            const int n_kick,
                            const int n_turn,
                            const int n_dash,
                            const GameTime & start_time ,
                            NeckAction::Ptr neck = NeckAction::Ptr(),
                            ViewAction::Ptr view = ViewAction::Ptr() )
        : M_target_point( target_point ),
          M_dash_dir( dash_dir ),
          M_kick_step( n_kick ),
          M_turn_step( n_turn ),
          M_dash_step( n_dash ),
          M_last_execute_time( start_time ),
          M_neck_action( neck ),
          M_view_action( view )
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

    bool checkOpponent( const WorldModel & wm );
    bool doKick( PlayerAgent * agent );
    bool doTurn( PlayerAgent * agent );
    bool doDash( PlayerAgent * agent );
    bool doAdjustDash( PlayerAgent * agent );
};

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionNormalDribble::finished( const PlayerAgent * agent )
{
    if ( M_kick_step + M_turn_step + M_dash_step == 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (finished) check finished. empty queue" );
        return true;
    }

    const WorldModel & wm = agent->world();

    if ( M_last_execute_time.cycle() + 1 != wm.time().cycle() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (finished). last execute time is not match" );
        return true;
    }

    if ( wm.self().collidesWithBall()
         || wm.self().collidesWithPlayer() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (finished) collision occured." );
        return true;
    }

    if ( wm.kickableTeammate()
         || wm.kickableOpponent() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (finished). exist other kickable player" );
        return true;
    }

    if ( wm.ball().pos().dist2( M_target_point ) < std::pow( 0.1, 2 )
         && wm.self().pos().dist2( M_target_point ) < std::pow( 0.1, 2 ) )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (finished). reached target point" );
        return true;
    }

    if ( M_kick_step == 0 )
    {
        const Vector2D ball_final_pos = wm.ball().inertiaPoint( M_turn_step + M_dash_step );
        if ( ball_final_pos.dist2( M_target_point ) > std::pow( 1.0, 2 ) )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (finished). ball will reach the different position" );
            return true;
        }
    }

    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    if ( ball_next.absX() > ServerParam::i().pitchHalfLength() - 0.5
         || ball_next.absY() > ServerParam::i().pitchHalfWidth() - 0.5 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (finished) ball will be out of pitch. stop intention." );
        return true;
    }

    if ( wm.audioMemory().passRequestTime() == agent->world().time() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (finished). heard pass request." );
        return true;
    }

    // playmode is checked in PlayerAgent::parse()
    // and intention queue is totally managed at there.

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (finished). not finished yet." );

    return false;
}


/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionNormalDribble::execute( PlayerAgent * agent )
{
    if ( M_kick_step + M_turn_step + M_dash_step == 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) empty queue." );
        return false;
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (intention:execute) kick=%d turn=%d dash=%d dash_dir=%.1f",
                  M_kick_step, M_turn_step, M_dash_step, M_dash_dir.degree() );

    const WorldModel & wm = agent->world();

#if 1
    //
    // compare the current queue with other chain action candidates
    //
    if ( wm.self().isKickable()
         && M_kick_step <= 0
         && M_turn_step <= 0
         && M_dash_step <= 5
         //&& M_dash_dir.abs() < 0.001
         && wm.interceptTable()->opponentReachStep() > 1 )
    {
        double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );
        CooperativeAction::Ptr current_action = ActDribble::create_dash_only( wm.self().unum(),
                                                                              M_target_point,
                                                                              M_target_point,
                                                                              wm.self().body().degree(),
                                                                              wm.ball().vel(),
                                                                              dash_power,
                                                                              M_dash_dir.degree(),
                                                                              M_dash_step,
                                                                              "queuedDribble" );
        current_action->setIndex( 0 );

        GeneratorShortDribble::instance().setQueuedAction( wm, current_action );

        ActionChainHolder::instance().update( wm );
        const ActionChainGraph & search_result = ActionChainHolder::i().graph();
        const CooperativeAction & first_action = search_result.bestFirstAction();

        if ( first_action.type() != CooperativeAction::Dribble
             || ! first_action.targetBallPos().equals( current_action->targetBallPos() ) )
        {
            agent->debugClient().addMessage( "CancelDribbleQ" );
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention:execute) cancel. select other action." );
            return false;
        }
    }
#endif

    //
    //
    //

    if ( M_kick_step == 0
         && checkOpponent( wm ) )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute). exist opponent. cancel intention." );
        return false;
    }

    //
    // execute action
    //

    if ( M_kick_step > 0 )
    {
        if ( ! doKick( agent ) )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention:exuecute) failed to kick. clear intention" );
            this->clear();
            return false;
        }
        agent->debugClient().addMessage( "NormalDribbleQKick%d:%d", M_turn_step, M_dash_step );
        --M_kick_step;
    }
    else if ( M_turn_step > 0 )
    {
        if ( ! doTurn( agent ) )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention:exuecute) failed to turn. clear intention" );
            this->clear();
            return false;
        }
        agent->debugClient().addMessage( "NormalDribbleQTurn%d:%d", M_turn_step + 1, M_dash_step );
        --M_turn_step;
    }
    else if ( M_dash_step > 0 )
    {
        if ( ! doDash( agent ) )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention:execute) failed to dash.  clear intention" );
            this->clear();
            return false;
        }
        agent->debugClient().addMessage( "NormalDribbleQDash%d:%d", M_turn_step, M_dash_step + 1 );
        --M_dash_step;
    }
    else
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute). No command queue" );
        this->clear();
        return false;
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (intention:execute). done" );
    agent->debugClient().setTarget( M_target_point );


    if ( ! M_view_action )
    {
        if ( wm.gameMode().type() != GameMode::PlayOn
             || M_turn_step > 0
             || M_dash_step <= 1 )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (intention:execute) default view synch" );
            agent->debugClient().addMessage( "ViewSynch" );
            agent->setViewAction( new View_Synch() );
        }
        else if ( M_dash_step >= 5 )
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
    }
    else
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) registered view" );
        agent->debugClient().addMessage( "ViewRegisterd" );
        agent->setViewAction( M_view_action->clone() );
    }

    if ( ! M_neck_action )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) default turn_neck scan field" );
        agent->debugClient().addMessage( "NeckScan" );
        if ( wm.ball().velCount() >= 2 )
        {
            agent->setNeckAction( new Neck_TurnToBall() );
        }
        else
        {
            ActionChainHolder::instance().update( wm );

            //agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        }
    }
    else
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (intention:execute) registered turn_neck" );
        agent->debugClient().addMessage( "NeckRegistered" );
        agent->setNeckAction( M_neck_action->clone() );
    }

    M_last_execute_time = wm.time();

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionNormalDribble::checkOpponent( const WorldModel & wm )
{
    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();

    /*--------------------------------------------------------*/
    // exist near opponent goalie in NEXT cycle
    if ( ball_next.x > ServerParam::i().theirPenaltyAreaLineX()
         && ball_next.absY() < ServerParam::i().penaltyAreaHalfWidth() )
    {
        const AbstractPlayerObject * opp_goalie = wm.getTheirGoalie();
        if ( opp_goalie
             && opp_goalie->distFromBall() < ( ServerParam::i().catchableArea()
                                               + opp_goalie->playerTypePtr()->realSpeedMax() )
             )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": existOpponent(). but exist near opponent goalie" );
            this->clear();
            return true;
        }
    }

    const PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 5 );

    if ( ! nearest_opp )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": existOppnent(). No opponent" );
        return false;
    }

    /*--------------------------------------------------------*/
    // exist very close opponent at CURRENT cycle
    if (  nearest_opp->distFromBall() < ServerParam::i().defaultKickableArea() + 0.2 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": existOpponent(). but exist kickable opp" );
        this->clear();
        return true;
    }

    /*--------------------------------------------------------*/
    // exist near opponent at NEXT cycle
    if ( nearest_opp->pos().dist( ball_next )
         < ServerParam::i().defaultPlayerSpeedMax() + ServerParam::i().defaultKickableArea() + 0.3 )
    {
        const Vector2D opp_next = nearest_opp->pos() + nearest_opp->vel();
        // oppopnent angle is known
        if ( nearest_opp->bodyCount() == 0
             || nearest_opp->vel().r() > 0.2 )
        {
            Line2D opp_line( opp_next,
                             ( nearest_opp->bodyCount() == 0
                               ? nearest_opp->body()
                               : nearest_opp->vel().th() ) );
            if ( opp_line.dist( ball_next ) > 1.2 )
            {
                // never reach
                dlog.addText( Logger::DRIBBLE,
                              __FILE__": existOpponent(). opp never reach." );
            }
            else if ( opp_next.dist( ball_next ) < 0.6 + 1.2 )
            {
                dlog.addText( Logger::DRIBBLE,
                              __FILE__": existOpponent(). but opp may reachable(1)." );
                this->clear();
                return true;
            }

            dlog.addText( Logger::DRIBBLE,
                          __FILE__": existOpponent(). opp angle is known. opp may not be reached." );
        }
        // opponent angle is not known
        else
        {
            if ( opp_next.dist( ball_next ) < 1.2 + 1.2 ) //0.6 + 1.2 )
            {
                dlog.addText( Logger::DRIBBLE,
                              __FILE__": existOpponent(). but opp may reachable(2)." );
                this->clear();
                return true;
            }
        }

        dlog.addText( Logger::DRIBBLE,
                      __FILE__": existOpponent(). exist near opp. but avoidable?" );
    }

    return false;
}


/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionNormalDribble::doKick( PlayerAgent * agent )
{
    if ( M_kick_step <= 0 )
    {
        return false;
    }

    const WorldModel & wm = agent->world();

    if ( ! wm.self().isKickable() )
    {
        return false;
    }

    const ServerParam & SP = ServerParam::i();

    const Vector2D ball_move = M_target_point - wm.ball().pos();
    const double first_speed = SP.firstBallSpeed( ball_move.r(),
                                                  M_kick_step + M_turn_step + M_dash_step );
    const Vector2D ball_first_vel = ball_move.setLengthVector( first_speed );

    if ( M_kick_step == 1 )
    {
        const Vector2D one_step_max_vel = KickTable::calc_max_velocity( ball_first_vel.th(),
                                                                        wm.self().kickRate(),
                                                                        wm.ball().vel() );
        if ( one_step_max_vel.r2() < std::pow( first_speed, 2 ) )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": cannot kick by 1 step" );
            return false;
        }
    }

    //
    // check collision
    //
    const Vector2D my_next = wm.self().pos() + wm.self().vel();
    const Vector2D ball_next = wm.ball().pos() + ball_first_vel;

    if ( my_next.dist2( ball_next ) < std::pow( wm.self().playerType().playerSize()
                                                + SP.ballSize()
                                                + 0.1,
                                                2 ) )
    {
        if ( M_kick_step == 1 )
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": collision" );
            return false;
        }
        else
        {
            if ( ! Body_SmartKick( M_target_point, first_speed, first_speed * 0.8, 2
                                   ).execute( agent ) )
            {
                dlog.addText( Logger::DRIBBLE,
                              __FILE__": collision. smart kick." );
                return false;
            }
            return true;
        }
    }

    //
    // check opponent kickable area
    //

    for ( PlayerObject::Cont::const_iterator o = wm.opponentsFromSelf().begin(),
              end = wm.opponentsFromSelf().end();
          o != end;
          ++o )
    {
        const PlayerType * ptype = (*o)->playerTypePtr();
        Vector2D o_next = (*o)->pos() + (*o)->vel();

        const double control_area = ( ( (*o)->goalie()
                                        && ball_next.x > SP.theirPenaltyAreaLineX()
                                        && ball_next.absY() < SP.penaltyAreaHalfWidth() )
                                      ? SP.catchableArea()
                                      : ptype->kickableArea() );

        if ( ball_next.dist2( o_next ) < std::pow( control_area + 0.1, 2 ) )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx SelfPass (canKick) opponent may be kickable(1) dist=%.3f < control=%.3f + 0.1",
                          M_total_count, ball_next.dist( o_next ), control_area );
            debug_paint_failed( M_total_count, receive_pos );
#endif
            return false;
        }

        if ( (*o)->bodyCount() <= 1 )
        {
            o_next += Vector2D::from_polar( SP.maxDashPower() * ptype->dashPowerRate() * ptype->effortMax(),
                                            (*o)->body() );
        }
        else
        {
            o_next += (*o)->vel().setLengthVector( SP.maxDashPower()
                                                   * ptype->dashPowerRate()
                                                   * ptype->effortMax() );
        }

        if ( ball_next.dist2( o_next ) < std::pow( control_area, 2 ) )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d xxx SelfPass (canKick) opponent may be kickable(2) dist=%.3f < control=%.3f",
                          M_total_count, ball_next.dist( o_next ), control_area );
            debug_paint_failed( M_total_count, receive_pos );
#endif
            return false;
        }

    }

    const Vector2D kick_accel = ball_first_vel - wm.ball().vel();
    const double kick_power = kick_accel.r() / wm.self().kickRate();
    const AngleDeg kick_angle = kick_accel.th() - wm.self().body();

    agent->doKick( kick_power, kick_angle );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionNormalDribble::doTurn( PlayerAgent * agent )
{
    if ( M_turn_step <= 0 )
    {
        return false;
    }

    const double default_dist_thr = 0.5;

    const WorldModel & wm = agent->world();

    Vector2D my_inertia = wm.self().inertiaPoint( M_turn_step + M_dash_step );
    Vector2D ball_pos = wm.ball().inertiaPoint( M_turn_step + M_dash_step );
    //
    // TODO: adjust target angle for KeepDribble
    //
    AngleDeg target_angle = ( ball_pos - my_inertia ).th();
    AngleDeg angle_diff = target_angle - wm.self().body();

    double target_dist = ( ball_pos - my_inertia ).r();
    double angle_margin
        = std::max( 15.0,
                    std::fabs( AngleDeg::atan2_deg( default_dist_thr,
                                                    target_dist ) ) );

    if ( angle_diff.abs() < angle_margin )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (doTurn) but already facing. diff = %.1f  margin=%.1f",
                      angle_diff.degree(), angle_margin );
        this->clear();
        return false;
    }

    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (doTurn) turn to (%.2f, %.2f) moment=%f",
                  M_target_point.x, M_target_point.y, angle_diff.degree() );

    agent->doTurn( angle_diff );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionNormalDribble::doDash( PlayerAgent * agent )
{
    if ( M_dash_step <= 0 )
    {
        return false;
    }

    return doAdjustDash( agent );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
IntentionNormalDribble::doAdjustDash( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const AngleDeg base_dash_dir = ServerParam::i().discretizeDashAngle( M_dash_dir.degree() );
    const AngleDeg base_dash_angle = wm.self().body() + base_dash_dir;

    {
        const Vector2D ball_final = wm.ball().inertiaPoint( M_dash_step );
        const Vector2D my_inertia = wm.self().inertiaPoint( M_dash_step );
        const Vector2D ball_rel = ( ball_final - my_inertia ).rotatedVector( - base_dash_angle );

        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (doAdjustDash) dash=%d angle=%.1f ball_rel=(%.3f %.3f)",
                      M_dash_step,
                      base_dash_angle.degree(),
                      ball_rel.x, ball_rel.y );

        if ( ball_rel.absY() > 1.0 ) // magic number
        {
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (doAdjustDash) too big y difference %.3f",
                          ball_rel.absY() );
            return false;
        }
    }

    const double angle_step = std::max( 15.0, ServerParam::i().dashAngleStep() );

    std::vector< double > angles;
    angles.reserve( 19 );

    angles.push_back( 0.0 );
    for ( int i = 1; i <= 9; ++i )
    {
        double add_angle = angle_step * i;
        if ( add_angle > 100.0 )
        {
            break;
        }
        angles.push_back( +add_angle );
        angles.push_back( -add_angle );
    }

    const double max_dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );
    const double power_step = max_dash_power * 0.1;

    const Vector2D ball_next = wm.ball().pos() + wm.ball().vel();
    const double collide_thr = wm.self().playerType().playerSize() + ServerParam::i().ballSize() + 0.15;

    double min_dist2 = 10000000.0;
    double best_dir = -360.0;
    double best_power = max_dash_power;

    for ( std::vector< double >::const_iterator a = angles.begin(), end = angles.end();
          a != end;
          ++a )
    {
        const AngleDeg dash_dir = base_dash_dir + *a;
        const double dash_dir_rate = ServerParam::i().dashDirRate( dash_dir.degree() );
        const AngleDeg dash_angle = wm.self().body() + dash_dir;

        for ( int p = 0; p < 5; ++p )
        {
            double dash_power = max_dash_power - ( power_step * p );
            double accel_mag = dash_power * wm.self().dashRate() * dash_dir_rate;
            Vector2D dash_accel = Vector2D::polar2vector( accel_mag, dash_angle );
            Vector2D my_vel = wm.self().vel() + dash_accel;
            Vector2D my_next = wm.self().pos() + my_vel;

            double ball_dist = my_next.dist( ball_next );

            if ( ball_dist < collide_thr )
            {
                continue;
            }

            Vector2D ball_rel = ( ball_next - my_next ).rotatedVector( - base_dash_angle );
            if ( ball_rel.absY() > wm.self().playerType().kickableArea() - 0.1 )
            {
                continue;
            }

            if ( M_dash_step <= 1
                 && ball_dist > wm.self().playerType().kickableArea() - 0.15 )
            {
                continue;
            }

            double final_dist2 = M_target_point.dist2( my_next );
            if ( final_dist2 < min_dist2 )
            {
                min_dist2 = final_dist2;
                best_dir = dash_dir.degree();
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
    // if ( std::fabs( best_power - ServerParam::i().maxDashPower() ) > 1.0
    //      || std::fabs( best_dir - M_dash_dir.degree() ) > 1.0 )
    // {
    //     std::cerr << wm.time() << " IntentionNormalDribble::doAdjustDash"
    //               << " power=" << best_power
    //               << " dir=" << best_dir << std::endl;
    // }
    agent->doDash( best_power, best_dir );

    return true;
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*!

*/
Bhv_NormalDribble::Bhv_NormalDribble( const CooperativeAction & action,
                                      NeckAction::Ptr neck,
                                      ViewAction::Ptr view )
    : M_target_point( action.targetBallPos() ),
      M_first_ball_vel( action.firstBallVel() ),
      M_first_turn_moment( action.firstTurnMoment() ),
      M_first_dash_power( action.firstDashPower() ),
      M_first_dash_dir( action.firstDashDir() ),
      M_total_step( action.durationTime() ),
      M_kick_step( action.kickCount() ),
      M_turn_step( action.turnCount() ),
      M_dash_step( action.dashCount() ),
      M_neck_action( neck ),
      M_view_action( view )
{
    if ( action.type() != CooperativeAction::Dribble )
    {
        M_target_point = Vector2D::INVALIDATED;
        M_total_step = 0;
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_NormalDribble::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( ! wm.self().isKickable() )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute). no kickable..." );
        return false;
    }

    if ( ! M_target_point.isValid()
         || M_total_step <= 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute). illegal target point or illegal total step" );
#ifdef DEBUG_PRINT
        std::cerr << wm.self().unum() << ' ' << wm.time()
                  << " Bhv_NormalDribble::execute() illegal target point or illegal total step"
                  << std::endl;
#endif
        return false;
    }

    const ServerParam & SP = ServerParam::i();


    if ( M_turn_step > 0
         && std::fabs( M_first_turn_moment ) > 0.5 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute). first turn: moment=%.2f, n_turn=%d n_dash=%d",
                      M_first_turn_moment,
                      M_turn_step, M_dash_step );
        agent->doTurn( M_first_turn_moment );
        --M_turn_step;
    }
    else if ( M_kick_step > 0 )
    {
        Vector2D kick_accel = M_first_ball_vel - wm.ball().vel();
        double kick_power = kick_accel.r() / wm.self().kickRate();

        if ( M_kick_step == 1 )
        {
            AngleDeg kick_angle = kick_accel.th() - wm.self().body();

            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (execute). first kick(1): power=%.3f angle=%.1f, n_turn=%d n_dash=%d",
                          kick_power, kick_angle.degree(),
                          M_turn_step, M_dash_step );
            dlog.addCircle( Logger::DRIBBLE,
                            wm.ball().pos() + M_first_ball_vel,
                            0.1,
                            "#00f" );
            if ( kick_power > SP.maxPower() + 1.0e-5 )
            {
                dlog.addText( Logger::DRIBBLE,
                              __FILE__": (execute) over the max power. %.3f", kick_power );
                std::cerr << __FILE__ << ' ' << __LINE__ << ':'
                          << wm.self().unum() << ' ' << wm.time()
                          << " over the max power " << kick_power << std::endl;
            }

            agent->doKick( kick_power, kick_angle );
        }
        else if ( kick_power < SP.maxPower() + 1.0e-5 )
        {
            AngleDeg kick_angle = kick_accel.th() - wm.self().body();
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (execute) first kick(2): power=%.3f angle=%.1f, n_kick=%d n_turn=%d n_dash=%d",
                          kick_power, kick_angle.degree(),
                          M_kick_step, M_turn_step, M_dash_step );
            dlog.addCircle( Logger::DRIBBLE,
                            wm.ball().pos() + M_first_ball_vel,
                            0.1,
                            "#00f" );

            agent->doKick( std::min( kick_power, SP.maxPower() ), kick_angle );
        }
        else
        {
            double first_ball_speed = M_first_ball_vel.r();
            if ( ! Body_SmartKick( M_target_point, first_ball_speed, first_ball_speed * 0.9, 2
                                   ).execute( agent ) )
            {
                dlog.addText( Logger::DRIBBLE,
                              __FILE__":(execute) failed first smart kick." );
                return false;
            }
            dlog.addText( Logger::DRIBBLE,
                          __FILE__":(execute) smart kick." );
        }

        --M_kick_step;
    }
    else if ( M_dash_step > 0 )
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute). first dash: dash power=%.1f dir=%.1f",
                      M_first_dash_power, M_first_dash_dir.degree() );
        agent->doDash( M_first_dash_power, M_first_dash_dir );
        --M_dash_step;
    }
    else
    {
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (execute). no action steps" );
        std::cerr << __FILE__ << ':' << __LINE__ << ':'
                  << wm.self().unum() << ' ' << wm.time()
                  << " no action step." << std::endl;
        return false;
    }

    agent->setIntention( new IntentionNormalDribble( M_target_point,
                                                     M_first_dash_dir,
                                                     M_kick_step,
                                                     M_turn_step,
                                                     M_dash_step,
                                                     wm.time() ) );

   if ( ! M_view_action )
   {
       dlog.addText( Logger::DRIBBLE,
                     __FILE__": (execute) view synch" );
       agent->debugClient().addMessage( "ViewSynch" );
       agent->setViewAction( new View_Synch() );
   }
   else
   {
       dlog.addText( Logger::DRIBBLE,
                     __FILE__": (execute) registered view" );
       agent->setViewAction( M_view_action->clone() );
   }

   if ( ! M_neck_action )
   {
       dlog.addText( Logger::DRIBBLE,
                     __FILE__": (execute) neck scan field" );
       agent->setNeckAction( new Neck_TurnToBallOrScan( -1 ) );
   }
   else
   {
       dlog.addText( Logger::DRIBBLE,
                     __FILE__": (execute) registered neck" );
       agent->setNeckAction( M_neck_action->clone() );
   }

    return true;
}
