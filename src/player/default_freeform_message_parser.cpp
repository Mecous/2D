// -*-c++-*-

/*!
  \file default_freeform_message_parser.cpp
  \brief freeform message parser Source File
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

#include "default_freeform_message_parser.h"

#include "strategy.h"

#include <rcsc/common/player_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/world_model.h>

#include <cstring>

//#define DEBUG_PRINT

#include <sstream>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

*/
GoalieUnumMessageParser::GoalieUnumMessageParser( WorldModel & wm )
    : FreeformMessageParser( "gn" ),
      M_world( wm )
{

}

/*-------------------------------------------------------------------*/
/*!

*/
int
GoalieUnumMessageParser::parse( const char * msg )
{
    int our_unum = Unum_Unknown;
    int opp_unum = Unum_Unknown;
    int n_read = 0;

    if ( std::sscanf( msg, " ( gn %x %x ) %n",
                      &our_unum, &opp_unum, &n_read ) != 2 )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": could not read the message. [" << msg << "]"
                  << std::endl;
        return -1;
    }

    if ( our_unum < 1 || 11 < our_unum
         || opp_unum < 1 || 11 < opp_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (GoalieUnumMessage) illegal value. our=" << our_unum
                  << " their_unum=" << opp_unum
                  << " [" << msg << "]" << std::endl;
        return n_read;
    }

#ifdef DEBUG_PRINT
    std::cerr << M_world.ourTeamName() << ' ' << M_world.self().unum()
              << ": " << M_world.time()
              << " updated goalie unum: our=" << our_unum << " their=" << opp_unum
              << std::endl;
#endif

    M_world.setOurGoalieUnum( our_unum );
    M_world.setTheirGoalieUnum( opp_unum );

    return n_read;
}

/*-------------------------------------------------------------------*/
/*!

*/
OpponentPlayerTypeMessageParser::OpponentPlayerTypeMessageParser( WorldModel & world )
    : FreeformMessageParser( "pt" ),
      M_world( world )
{

}

/*-------------------------------------------------------------------*/
/*!

*/
int
OpponentPlayerTypeMessageParser::parse( const char * msg )
{
    char buf[16];
    int n_read = 0;

    std::memset( buf, 0, 16 );

    if ( std::sscanf( msg, " ( pt %15[^)] ) %n", buf, &n_read ) != 1 )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": could not read the message. [" << msg << "]"
                  << std::endl;
        return -1;
    }

    if ( std::strlen( buf ) != 11 )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (OpponentPlayerTypeMesageParser) illegal message [" << buf << "]"
                  << std::endl;
        return -1;
    }

    const int player_types = PlayerParam::i().playerTypes();

    std::ostringstream ostr;
#ifdef DEBUG_PRINT
    std::cerr << "parsed player types: ";
#endif

    for ( int i = 0; i < 11; ++i )
    {
        if ( buf[i] == '-' )
        {
            M_world.setTheirPlayerType( i + 1, Hetero_Unknown );
            ostr << '(' << i + 1 << ' ' << Hetero_Unknown << ')';
#ifdef DEBUG_PRINT
            std::cerr << '(' << i + 1 << ' ' << Hetero_Unknown << ')';
#endif
        }
        else
        {
            int id = static_cast< int >( buf[i] ) - static_cast< int >( 'A' );
            if ( id < 0 || player_types <= id )
            {
                std::cerr << __FILE__ << ' ' << __LINE__
                          << ": illegal player type id."
                          << " char=" << buf[i]
                          << " id=" << id
                          << std::endl;
                return -1;
            }

            M_world.setTheirPlayerType( i + 1, id );
            ostr << '(' << i + 1 << ' ' << id << ')';
#ifdef DEBUG_PRINT
            std::cerr << '(' << i + 1 << ' ' << id << ')';
#endif
        }
    }

    dlog.addText( Logger::SENSOR,
                  __FILE__": opponent player type %s", ostr.str().c_str() );
#ifdef DEBUG_PRINT
    std::cerr << std::endl;
#endif

    return n_read;
}

/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

/*-------------------------------------------------------------------*/
/*!

*/
OpponentFormationMessageParser::OpponentFormationMessageParser()
    : FreeformMessageParser( "of" )
{

}

/*-------------------------------------------------------------------*/
/*!

*/
int
OpponentFormationMessageParser::parse( const char * msg )
{
    char buf[16];
    int n_read = 0;

    std::memset( buf, 0, 16 );

    if ( std::sscanf( msg, " ( of %15[^)] ) %n", buf, &n_read ) != 1 )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": could not read the message. [" << msg << "]"
                  << std::endl;
        return -1;
    }

    if ( std::strlen( buf ) != 11 )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (OpponentFormationMessageParser) illegal message [" << buf << "]"
                  << std::endl;
        return -1;
    }

    std::ostringstream ostr;
#ifdef DEBUG_PRINT
    std::cerr << "parsed roles: ";
#endif

    for ( int i = 0; i < 11; ++i )
    {
        switch ( buf[i] ) {
        case 'F':
            break;
        case 'M':
            break;
        case 'D':
            break;
        case 'U':
            break;
        default:
            std::cerr << __FILE__ << ' ' << __LINE__
                      << ": illegal role id. [" << buf[i] << "]"
                      << std::endl;
            break;
        }

        ostr << '(' << i + 1 << ' ' << buf[i] << ')';
#ifdef DEBUG_PRINT
        std::cerr << '(' << i + 1 << ' ' << buf[i] << ')';
#endif
    }

    dlog.addText( Logger::SENSOR,
                  __FILE__": opponent formation %s", ostr.str().c_str() );
#ifdef DEBUG_PRINT
    std::cerr << std::endl;
#endif

    return n_read;
}

/*-------------------------------------------------------------------*/
/*!

*/
CornerKickTypeMessageParser::CornerKickTypeMessageParser( WorldModel & wm )
    : FreeformMessageParser( "ct" ),
      M_world( wm )
{

}

/*-------------------------------------------------------------------*/
/*!

*/
int
CornerKickTypeMessageParser::parse( const char * msg )
{
    char buf[16];
    int n_read = 0;

    std::memset( buf, 0, 16 );

    if ( std::sscanf( msg, " ( ct %15[^)] ) %n",
                      buf, &n_read ) != 1 )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": could not read the message. [" << msg << "]"
                  << std::endl;
        return -1;
    }

    if ( std::strlen( buf ) != 6
         && std::strlen( buf ) != 7 )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (CornerKickTypeMessage) illegal value. type=" << buf
                  << " [" << msg << "]" << std::endl;
        return n_read;
    }

#ifdef DEBUG_PRINT
    std::cerr << M_world.ourTeamName() << ' ' << M_world.self().unum()
              << ": " << M_world.time()
              << " updated corner_kick type:" << buf
              << std::endl;
#endif

    Strategy::instance().setCornerKickType( std::string( buf ) );

    return n_read;
}



/*-------------------------------------------------------------------*/
/*!

*/
OpponentWallDetectorMessageParser::OpponentWallDetectorMessageParser( WorldModel & wm )
    : FreeformMessageParser( "wd" ),
      M_world( wm )
{

}

/*-------------------------------------------------------------------*/
/*!

*/
int
OpponentWallDetectorMessageParser::parse( const char * msg )
{
    char buf[16];
    int n_read = 0;

    std::memset( buf, 0, 16 );

    if ( std::sscanf( msg, " ( wd %15[^)] ) %n",
                      buf, &n_read ) != 1 )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": could not read the message. [" << msg << "]"
                  << std::endl;
        return -1;
    }

    if ( std::strlen( buf ) != 4
         && std::strlen( buf ) != 5
         && std::strlen( buf ) != 7 )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": (WallDetecterMessage) illegal value. type=" << buf
                  << " [" << msg << "]" << std::endl;
        return n_read;
    }

#ifdef DEBUG_PRINT
    std::cerr << M_world.ourTeamName() << ' ' << M_world.self().unum()
              << ": " << M_world.time()
              << " updated opponent defense type:" << buf
              << std::endl;
#endif

    Strategy::instance().setDefenseType( std::string( buf ) );

    return n_read;
}
