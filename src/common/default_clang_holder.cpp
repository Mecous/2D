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

#include "default_clang_holder.h"

#include <rcsc/common/player_param.h>
#include <rcsc/clang/clang.h>
#include <rcsc/types.h>

#include <algorithm>
#include <iostream>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
DefaultCLangHolder::DefaultCLangHolder()
    : M_build_time( -1, 0 ),
      M_data_time( -1, 0 ),
      M_their_player_type_changed( false ),
      M_mark_assignment_changed( false )
{
    for ( int i = 0; i < 11; ++i )
    {
        M_their_player_type[i] = Hetero_Default;
        M_mark_assignment[i][0] = Unum_Unknown;
        M_mark_assignment[i][1] = Unum_Unknown;
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
int
DefaultCLangHolder::theirPlayerType( const int unum ) const
{
    if ( unum < 1 || 11 < unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(opponentPlayerType) illegal uniform number "
                  << unum << std::endl;
        return Hetero_Unknown;
    }

    return M_their_player_type[unum - 1];
}

/*-------------------------------------------------------------------*/
/*!

 */
std::pair< int, int >
DefaultCLangHolder::markTargets( const int our_unum ) const
{
    if ( our_unum < 1 || 11 < our_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(markAssignment) illegal uniform number "
                  << our_unum << std::endl;
        return std::make_pair( Unum_Unknown, Unum_Unknown );
    }

    return std::make_pair( M_mark_assignment[our_unum - 1][0],
                           M_mark_assignment[our_unum - 1][1] );
}

/*-------------------------------------------------------------------*/
/*!

 */
int
DefaultCLangHolder::firstMarkTarget( const int our_unum ) const
{
    if ( our_unum < 1 || 11 < our_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(markAssignment) illegal uniform number "
                  << our_unum << std::endl;
        return Unum_Unknown;
    }

    return M_mark_assignment[our_unum - 1][0];
}

/*-------------------------------------------------------------------*/
/*!

 */
int
DefaultCLangHolder::secondMarkTarget( const int our_unum ) const
{
    if ( our_unum < 1 || 11 < our_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(markAssignment) illegal uniform number "
                  << our_unum << std::endl;
        return Unum_Unknown;
    }

    return M_mark_assignment[our_unum - 1][1];
}

/*-------------------------------------------------------------------*/
/*!

 */
void
DefaultCLangHolder::clearChangedFlags()
{
    M_their_player_type_changed = false;
    M_mark_assignment_changed = false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
DefaultCLangHolder::setTheirPlayerType( const int unum,
                                        const int type )
{
    if ( unum < 1 || 11 < unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(setTheirPlayerType) illegal uniform number "
                  << unum << std::endl;
        return;
    }

    if ( type != Hetero_Unknown
         && ( type < 0
              || PlayerParam::i().playerTypes() <= type ) )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(setTheirPlayerType) illegal player type id "
                  << type << std::endl;
        return;
    }

    if ( type != Hetero_Unknown
         && M_their_player_type[unum - 1] != type )
    {
        // std::cerr << __FILE__ << ' ' << __LINE__
        //           << ":(setTheirPlayerType) updated. opponent=" << unum
        //           << " old=" << M_their_player_type[unum-1]
        //           << " new=" << type << std::endl;
        M_their_player_type_changed = true;
    }

    M_their_player_type[unum - 1] = type;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
DefaultCLangHolder::setMarkAssignment( const int our_unum,
                                       const int their_unum_1 )
{
    if ( their_unum_1 == Unum_Unknown )
    {
        return;
    }

    if ( our_unum < 1 || 11 < our_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(setMarkAssignment) illegal teammate uniform number "
                  << our_unum << std::endl;
        return;
    }

    if ( their_unum_1 < 1 || 11 < their_unum_1 )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(setMarkAssignment) illegal opponent uniform number " << their_unum_1
                  << std::endl;
        return;
    }

    if ( M_mark_assignment[our_unum - 1][0] == their_unum_1
         && M_mark_assignment[our_unum - 1][1] == Unum_Unknown )
    {
        // no change
        return;
    }

    M_mark_assignment[our_unum - 1][0] = their_unum_1;
    M_mark_assignment[our_unum - 1][1] = Unum_Unknown;
    M_mark_assignment_changed = true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
DefaultCLangHolder::setMarkAssignment( const int our_unum,
                                       const int their_unum_1,
                                       const int their_unum_2 )
{
    if ( our_unum < 1 || 11 < our_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": illegal teammate uniform number " << our_unum
                  << std::endl;
        return;
    }

    if ( their_unum_1 != Unum_Unknown
         && ( their_unum_1 < 1 || 11 < their_unum_1 ) )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": illegal opponent uniform number(1) " << their_unum_1 << std::endl;
        return;
    }

    if ( their_unum_2 != Unum_Unknown
         && ( their_unum_2 < 1 || 11 < their_unum_2 ) )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": illegal opponent uniform number(2) " << their_unum_2 << std::endl;
        return;
    }

    if ( M_mark_assignment[our_unum - 1][0] == their_unum_1
         && M_mark_assignment[our_unum - 1][1] == their_unum_2 )
    {
        // no change
        return;
    }

    M_mark_assignment[our_unum - 1][0] = their_unum_1;
    M_mark_assignment[our_unum - 1][1] = their_unum_2;
    M_mark_assignment_changed = true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
DefaultCLangHolder::clearMarkAssignment( const int our_unum )
{
    if ( our_unum < 1 || 11 < our_unum )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(clearMarkAssignment) illegal uniform number "
                  << our_unum << std::endl;
        return;
    }

    if ( M_mark_assignment[our_unum - 1][0] != Unum_Unknown
         || M_mark_assignment[our_unum - 1][1] != Unum_Unknown )
    {
        M_mark_assignment_changed = true;
    }

    M_mark_assignment[our_unum - 1][0] = Unum_Unknown;
    M_mark_assignment[our_unum - 1][1] = Unum_Unknown;
}

/*-------------------------------------------------------------------*/
/*!

 */
CLangMessage *
DefaultCLangHolder::buildCLang( const std::string & team_name,
                                const GameTime & current )
{
    M_build_time = current;

    CLangTokenRule * tok = new CLangTokenRule();

    tok->setTTL( 6000 );
    tok->setCondition( new CLangConditionBool( true ) );

    //
    // opponent player type
    //
    if ( theirPlayerTypeChanged() )
    {
        std::cerr << team_name << " coach: " << current
                  << " send player_types: ";
        for ( int unum = 1; unum <= 11; ++unum )
        {
            CLangDirectiveCommon * dir = new CLangDirectiveCommon();
            dir->setPositive( true );
            dir->setOur( false );
            dir->setPlayers( new CLangUnumSet( unum ) );
            dir->addAction( new CLangActionHeteroType( M_their_player_type[unum - 1] ) );

            tok->addDirective( dir );

            std::cerr << '(' << unum << ' ' << M_their_player_type[unum - 1] << ')';
        }
        std::cerr << std::endl;
    }


    //
    // mark assignment
    //
    if ( markAssignmentChanged() )
    {
        std::cerr << team_name << " coach: " << current
                  << " send mark: ";
        for ( int unum = 1; unum <= 11; ++unum )
        {
            CLangDirectiveCommon * dir = new CLangDirectiveCommon();
            dir->setOur( true );
            dir->setPlayers( new CLangUnumSet( unum ) );

            CLangActionMark * act = new CLangActionMark();
            if ( M_mark_assignment[unum - 1][0] != Unum_Unknown )
            {
                std::cerr << '(' << unum << ' ' << M_mark_assignment[unum - 1][0];
                dir->setPositive( true );
                act->addPlayer( M_mark_assignment[unum - 1][0] );
                if ( M_mark_assignment[unum - 1][1] != Unum_Unknown )
                {
                    std::cerr << ' ' << M_mark_assignment[unum - 1][1];
                    act->addPlayer( M_mark_assignment[unum - 1][1] );
                }
                std::cerr << ')';
            }
            else
            {
                std::cerr << "(" << unum << " -1)";
                dir->setPositive( false );
                act->addPlayer( 0 ); // all
            }
            dir->addAction( act );

            tok->addDirective( dir );
        }
        std::cerr << std::endl;
    }

    CLangInfoMessage * info = new CLangInfoMessage();
    info->addToken( tok );

    clearChangedFlags();
    return info;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
DefaultCLangHolder::buildDataFrom( const GameTime & current,
                                   const CLangMessage::ConstPtr & msg )
{
    if ( ! msg )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** message. NULL object." << std::endl;
        return false;
    }

    boost::shared_ptr< const CLangInfoMessage > info;

    try
    {
        info = boost::dynamic_pointer_cast< const CLangInfoMessage >( msg );

    }
    catch ( std::exception & e )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** message. " << e.what() << std::endl;
        return false;
    }

    for ( CLangToken::Cont::const_iterator tok = info->tokens().begin(),
              end = info->tokens().end();
          tok != end;
          ++tok )
    {
        if ( ! buildDataFrom( *tok ) )
        {
            return false;
        }
    }

    M_data_time = current;
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
DefaultCLangHolder::buildDataFrom( const CLangToken::ConstPtr & tok )
{
    if ( ! tok )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** token. NULL object." << std::endl;
        return false;
    }

    boost::shared_ptr< const CLangTokenRule > rule;

    try
    {
        rule = boost::dynamic_pointer_cast< const CLangTokenRule >( tok );
    }
    catch ( std::exception & e )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** token. " << e.what() << std::endl;
        return false;
    }

    for ( CLangDirective::Cont::const_iterator dir = rule->directives().begin(),
              end = rule->directives().end();
          dir != end;
          ++dir )
    {
        if ( ! buildDataFrom( *dir ) )
        {
            return false;
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
DefaultCLangHolder::buildDataFrom( const CLangDirective::ConstPtr & dir )
{
    if ( ! dir )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** directive. NULL object." << std::endl;
        return false;
    }

    boost::shared_ptr< const CLangDirectiveCommon > com;

    try
    {
        com = boost::dynamic_pointer_cast< const CLangDirectiveCommon >( dir );
    }
    catch ( std::exception & e )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** directive. " << e.what() << std::endl;
        return false;
    }

    bool positive = com->isPositive();
    bool our = com->isOur();

    if ( ! com->players() )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** directive. NULL unum_set." << std::endl;
        return false;
    }

    const CLangUnumSet::Set & players = com->players()->entries();


    for ( CLangAction::Cont::const_iterator act = com->actions().begin(),
              end = com->actions().end();
          act != end;
          ++act )
    {
        if ( ! buildDataFrom( positive, our, players, *act ) )
        {
            return false;
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
DefaultCLangHolder::buildDataFrom( const bool positive,
                                   const bool our,
                                   const CLangUnumSet::Set & players,
                                   const CLangAction::ConstPtr & act )
{
    if ( ! act )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** action. NULL object." << std::endl;
        return false;
    }

    switch ( act->type() ) {
    case CLangAction::MARK:
        try
        {
            boost::shared_ptr< const CLangActionMark > mark
                = boost::dynamic_pointer_cast< const CLangActionMark >( act );
            return buildDataFrom( positive, our, players, mark );
        }
        catch ( std::exception & e )
        {
            std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** action case mark. " << e.what() << std::endl;
        }
        break;
    case CLangAction::HTYPE:
        try
        {
            boost::shared_ptr< const CLangActionHeteroType > htype
                = boost::dynamic_pointer_cast< const CLangActionHeteroType >( act );
            return buildDataFrom( positive, our, players, htype );
        }
        catch ( std::exception & e )
        {
            std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** action case htype. " << e.what() << std::endl;
        }
        break;
    default:
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) unsupported action." << std::endl;
        break;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
DefaultCLangHolder::buildDataFrom( const bool positive,
                                   const bool our,
                                   const CLangUnumSet::Set & players,
                                   const boost::shared_ptr< const CLangActionMark > & mark )
{
    if ( ! mark )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** mark action. NULL object." << std::endl;
        return false;
    }

    if ( ! mark->targetPlayers() )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** mark action. NULL target players." << std::endl;
        return false;
    }

    if ( ! our )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***WARNING*** mark action. their mark is not supported."
                  << std::endl;
        return true;
    }

    const CLangUnumSet::Set & targets = mark->targetPlayers()->entries();

    // const CLangUnumSet::Set & target_players = mark->targetPlayers()->entries();
    for ( CLangUnumSet::Set::const_iterator p = players.begin();
          p != players.end();
          ++p )
    {
        if ( positive )
        {
            if ( targets.size() == 2 )
            {
                CLangUnumSet::Set::const_iterator u1 = targets.begin();
                CLangUnumSet::Set::const_iterator u2 = u1; ++u2;
                setMarkAssignment( *p, *u1, *u2 );
            }
            else if ( targets.size() == 1 )
            {
                setMarkAssignment( *p, *targets.begin() );
            }
        }
        else
        {
            setMarkAssignment( *p, Unum_Unknown, Unum_Unknown );
        }
        // std::cout << "mark directive: "
        //           << ( positive ? "do " : "dont " )
        //           << ( our ? "our " : "opp " )
        //           << *p << ' '
        //           << *mark->targetPlayers()
        //           << std::endl;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
DefaultCLangHolder::buildDataFrom( const bool positive,
                                   const bool our,
                                   const CLangUnumSet::Set & players,
                                   const boost::shared_ptr< const CLangActionHeteroType > & htype )
{
    if ( ! htype )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** htype action. NULL object." << std::endl;
        return false;
    }

    if ( ! positive )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** htype action. dont." << std::endl;
        return true;
    }

    if ( our )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** htype action. our." << std::endl;
        return true;
    }

    if ( players.empty() )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***WARNING*** no target player." << std::endl;
        return true;
    }

    const int type = htype->playerType();

    if ( type != Hetero_Unknown
         && ( type < 0 || PlayerParam::i().playerTypes() <= type ) )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ":(buildDataFrom) ***ERROR*** htype action. illegal player type id "
                  << type << std::endl;
        return true;
    }

    if ( type == Hetero_Unknown )
    {
        // update is not necessary.
        return true;
    }

    for ( CLangUnumSet::Set::const_iterator unum = players.begin();
          unum != players.end();
          ++unum )
    {
        if ( *unum == 0 )
        {
            std::fill_n( M_their_player_type, 11, type );
        }
        else
        {
            setTheirPlayerType( *unum, type );
        }
    }

    return true;
}
