// -*-c++-*-

/*!
  \file default_freeform_message.cpp
  \brief freeform message builder Source File
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

#include "default_freeform_message.h"

#include <rcsc/common/player_param.h>
#include <rcsc/types.h>

#include <cstring>
#include <cstdio>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

*/
GoalieUnumMessage::GoalieUnumMessage( const int our_unum,
                                      const int their_unum )
    : FreeformMessage( "gn" )
{
    char buf[4];

    int our = ( our_unum < 1 || 11 < our_unum
                ? 0
                : our_unum );
    int opp = ( their_unum < 1 || 11 < their_unum
                ? 0
                : their_unum );

    snprintf( buf, 4, "%x %x", our, opp );
    M_message = buf;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
GoalieUnumMessage::append( std::string & to ) const
{
    if ( M_message.empty() )
    {
        return false;
    }

    to += '(';
    to += this->type();
    to += ' ';
    to += M_message;
    to += ')';

    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
std::ostream &
GoalieUnumMessage::printDebug( std::ostream & os ) const
{
    os << "[GoalieUnum]";
    return os;
}

/*-------------------------------------------------------------------*/
/*!

 */
OpponentPlayerTypeMessage::OpponentPlayerTypeMessage( const int t1,
                                                      const int t2,
                                                      const int t3,
                                                      const int t4,
                                                      const int t5,
                                                      const int t6,
                                                      const int t7,
                                                      const int t8,
                                                      const int t9,
                                                      const int t10,
                                                      const int t11 )
    : FreeformMessage( "pt" )
{
    int i = 0;
    M_player_type_id[i++] = t1;
    M_player_type_id[i++] = t2;
    M_player_type_id[i++] = t3;
    M_player_type_id[i++] = t4;
    M_player_type_id[i++] = t5;
    M_player_type_id[i++] = t6;
    M_player_type_id[i++] = t7;
    M_player_type_id[i++] = t8;
    M_player_type_id[i++] = t9;
    M_player_type_id[i++] = t10;
    M_player_type_id[i++] = t11;

    buildMessage();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
OpponentPlayerTypeMessage::setPlayerType( const int unum,
                                          const int id )
{
    if ( unum < 1 || 11 < unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": illegal uniform number " << unum
                  << std::endl;
        return;
    }

    M_player_type_id[unum-1] = id;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
OpponentPlayerTypeMessage::buildMessage()
{
    M_message.clear();

    const int player_types = PlayerParam::i().playerTypes();
    for ( int i = 0; i < 11; ++i )
    {
        int id = M_player_type_id[i];

        if ( id < Hetero_Unknown || player_types <= id )
        {
            std::cerr << __FILE__ << ' ' << __LINE__
                      << ": illegal player type id " << id
                      << std::endl;
            id = Hetero_Unknown;
        }

        if ( id == Hetero_Unknown )
        {
            M_message += '-';
        }
        else
        {
            M_message += static_cast< char >( 'A' + id );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentPlayerTypeMessage::append( std::string & to ) const
{
    if ( M_message.empty() )
    {
        return false;
    }

    to += '(';
    to += this->type();
    to += ' ';
    to += M_message;
    to += ')';

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
std::ostream &
OpponentPlayerTypeMessage::printDebug( std::ostream & os ) const
{
    os << "[PlayerType]";
    return os;
}

/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

#include "coach_analyzer_manager.h"


/*-------------------------------------------------------------------*/
/*!

 */
OpponentFormationMessage::OpponentFormationMessage( const int t1,
                                                    const int t2,
                                                    const int t3,
                                                    const int t4,
                                                    const int t5,
                                                    const int t6,
                                                    const int t7,
                                                    const int t8,
                                                    const int t9,
                                                    const int t10,
                                                    const int t11 )
    : FreeformMessage( "of" )
{
    int i = 0;
    M_role_types[i++] = t1;
    M_role_types[i++] = t2;
    M_role_types[i++] = t3;
    M_role_types[i++] = t4;
    M_role_types[i++] = t5;
    M_role_types[i++] = t6;
    M_role_types[i++] = t7;
    M_role_types[i++] = t8;
    M_role_types[i++] = t9;
    M_role_types[i++] = t10;
    M_role_types[i++] = t11;

    buildMessage();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
OpponentFormationMessage::buildMessage()
{
    M_message.clear();

    for ( int i = 0; i < 11; ++i )
    {
        int id = M_role_types[i];

        if ( id == CoachAnalyzerManager::FORWARD )
        {
            M_message += 'F';
        }
        else if ( id == CoachAnalyzerManager::MIDFIELDER )
        {
            M_message += 'M';
        }
        else if ( id == CoachAnalyzerManager::OTHER )
        {
            M_message += 'D';
        }
        else if ( id == CoachAnalyzerManager::UNKNOWN )
        {
            M_message += 'U';
        }
        else
        {
            std::cerr << __FILE__ << ' ' << __LINE__
                      << ": illegal role type " << id
                      << std::endl;
            M_message += 'U';
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentFormationMessage::append( std::string & to ) const
{
    if ( M_message.empty() )
    {
        return false;
    }

    to += '(';
    to += this->type();
    to += ' ';
    to += M_message;
    to += ')';

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
std::ostream &
OpponentFormationMessage::printDebug( std::ostream & os ) const
{
    os << "[OppFormation:" << M_message << ']';
    return os;
}

/*-------------------------------------------------------------------*/
/*!

*/
CornerKickTypeMessage::CornerKickTypeMessage( const std::string & cornerkick_type )
    : FreeformMessage( "ct" )
{
    char buf[16];

    if( cornerkick_type == "marked" )
    {
        snprintf( buf, 7, "%s", cornerkick_type.c_str() );
    }
    else if( cornerkick_type == "agent2d")
    {
        snprintf( buf, 8, "%s", "agent2d" );
    }
    else
    {
        snprintf( buf, 8, "%s", "default" );
    }

    M_message = buf;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
CornerKickTypeMessage::append( std::string & to ) const
{
    if ( M_message.empty() )
    {
        return false;
    }

    to += '(';
    to += this->type();
    to += ' ';
    to += M_message;
    to += ')';

    return true;
}


/*-------------------------------------------------------------------*/
/*!

 */
std::ostream &
CornerKickTypeMessage::printDebug( std::ostream & os ) const
{
    os << "[CornerKickType:" << M_message << ']';
    return os;
}


/*-------------------------------------------------------------------*/
/*!

*/
OpponentWallDetectorMessage::OpponentWallDetectorMessage( const std::string defense_type )
    : FreeformMessage( "wd" )
{
    char buf[16];

    if( defense_type == "wall" )
    {
        snprintf( buf, 5, "%s", defense_type.c_str() );
    }
    else if( defense_type == "default")
    {
        snprintf( buf, 8, "%s", defense_type.c_str() );
    }
    else
    {
        snprintf( buf, 6, "%s", "other" );
    }

    M_message = buf;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
OpponentWallDetectorMessage::append( std::string & to ) const
{
    if ( M_message.empty() )
    {
        return false;
    }

    to += '(';
    to += this->type();
    to += ' ';
    to += M_message;
    to += ')';

    return true;
}



/*-------------------------------------------------------------------*/
/*!

 */
std::ostream &
OpponentWallDetectorMessage::printDebug( std::ostream & os ) const
{
    os << "[WallDetecter:" << M_message << ']';
    return os;
}
