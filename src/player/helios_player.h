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

#ifndef HELIOS_PLAYER_H
#define HELIOS_PLAYER_H

#include "action_generator_holder.h"
#include "field_evaluator.h"
#include "communication.h"
#include "default_clang_holder.h"

#include <rcsc/player/player_agent.h>
#include <vector>

class HeliosPlayer
    : public rcsc::PlayerAgent {
private:

    Communication::Ptr M_communication;

    DefaultCLangHolder M_clang_holder;

public:

    HeliosPlayer();

    virtual
    ~HeliosPlayer();

protected:

    /*!
      You can override this method.
      But you must call PlayerAgent::initImpl() in this method.
    */
    virtual
    bool initImpl( rcsc::CmdLineParser & cmd_parser );


    virtual
    void handleExit();

    //! main decision
    virtual
    void actionImpl();

    //! communication decision
    virtual
    void communicationImpl();

    virtual
    void handleActionStart();
    virtual
    void handleActionEnd();

    virtual
    void handleInitMessage();
    virtual
    void handleServerParam();
    virtual
    void handlePlayerParam();
    virtual
    void handlePlayerType();

    virtual
    void handleOnlineCoachAudio();

private:

    FieldEvaluator::Ptr createFieldEvaluator() const;
    ActionGeneratorHolder::ConstPtr createActionGenerator() const;

    bool doPreprocess();
    bool doShoot();
    bool doTurnForShoot();
    bool doForceKick();
    bool doHeardPassReceive();

    void writeInterceptDecisionLog( const char * short_role_name );

};

#endif
