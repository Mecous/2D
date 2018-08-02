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

#ifndef DEFAULT_FREEFORM_MESSAGE_PARSER_H
#define DEFAULT_FREEFORM_MESSAGE_PARSER_H

#include <rcsc/common/freeform_message_parser.h>

#include <string>

namespace rcsc {
class WorldModel;
}

/*-------------------------------------------------------------------*/
/*!
  \class GoalieUnumMessageParser
  \brief goalies uniform number message parser
 */
class GoalieUnumMessageParser
    : public rcsc::FreeformMessageParser {
private:
    //! reference to the world model instance
    rcsc::WorldModel & M_world;

public:

    /*!
      \brief construct with world model
      \param wm reference to the world model instance
     */
    GoalieUnumMessageParser( rcsc::WorldModel & wm );


    /*!
      \brief analyzes freeform message.
      \retval bytes read if success
      \retval 0 message ID is not match. other parser should be tried.
      \retval -1 failed to parse
    */
    int parse( const char * msg );
};

/*-------------------------------------------------------------------*/
/*!
  \class OpponentPlayerTypeMessageParser
  \brief opponent player type message parser
 */
class OpponentPlayerTypeMessageParser
    : public rcsc::FreeformMessageParser {
private:
    //! reference to the world model instance
    rcsc::WorldModel & M_world;

public:

    /*!
      \brief construct with world model
      \param wm reference to the world model instance
     */
    OpponentPlayerTypeMessageParser( rcsc::WorldModel & wm );


    /*!
      \brief analyzes freeform message.
      \retval bytes read if success
      \retval 0 message ID is not match. other parser should be tried.
      \retval -1 failed to parse
    */
    int parse( const char * msg );
};

/*-------------------------------------------------------------------*/
/*!
  \class OpponentFormationMessageParser
  \brief opponent formation message parser
 */
class OpponentFormationMessageParser
    : public rcsc::FreeformMessageParser {
public:

    /*!
      \brief construct with world model
      \param wm reference to the world model instance
     */
    OpponentFormationMessageParser();


    /*!
      \brief analyzes freeform message.
      \retval bytes read if success
      \retval 0 message ID is not match. other parser should be tried.
      \retval -1 failed to parse
    */
    int parse( const char * msg );
};

#endif

/*-------------------------------------------------------------------*/
/*!
  \class CornerKickTypeMessageParser
  \brief cornerkick type  message parser
 */
class CornerKickTypeMessageParser
    : public rcsc::FreeformMessageParser {
private:
    //! reference to the world model instance
    rcsc::WorldModel & M_world;

public:

    /*!
      \brief construct with world model
      \param wm reference to the world model instance
     */
    CornerKickTypeMessageParser( rcsc::WorldModel & wm );


    /*!
      \brief analyzes freeform message.
      \retval bytes read if success
      \retval 0 message ID is not match. other parser should be tried.
      \retval -1 failed to parse
    */
    int parse( const char * msg );
};



/*-------------------------------------------------------------------*/
/*!
  \class OpponentWallDetectorMessageParser
  \brief cornerkick type  message parser
 */
class OpponentWallDetectorMessageParser
    : public rcsc::FreeformMessageParser {
private:
    //! reference to the world model instance
    rcsc::WorldModel & M_world;

public:

    /*!
      \brief construct with world model
      \param wm reference to the world model instance
     */
    OpponentWallDetectorMessageParser( rcsc::WorldModel & wm );


    /*!
      \brief analyzes freeform message.
      \retval bytes read if success
      \retval 0 message ID is not match. other parser should be tried.
      \retval -1 failed to parse
    */
    int parse( const char * msg );
};
