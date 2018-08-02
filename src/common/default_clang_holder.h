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

#ifndef DEFAULT_CLANG_HOLDER_H
#define DEFAULT_CLANG_HOLDER_H

#include <rcsc/clang/clang_action.h>
#include <rcsc/clang/clang_unum.h>
#include <rcsc/clang/clang_directive.h>
#include <rcsc/clang/clang_token.h>
#include <rcsc/clang/clang_message.h>
#include <rcsc/game_time.h>

#include <set>
#include <string>
#include <utility>

namespace rcsc {
class CLangToken;
class CLangDirective;
}

class DefaultCLangHolder {
private:

    rcsc::GameTime M_build_time; //!< last message build time
    rcsc::GameTime M_data_time; //!< last data time

    //
    // opponent player type
    //
    bool M_their_player_type_changed;
    int M_their_player_type[11];

    //
    // mark assignment
    //
    bool M_mark_assignment_changed;
    int M_mark_assignment[11][2];


public:

    DefaultCLangHolder();


    const rcsc::GameTime & buildTime() const
      {
          return M_build_time;
      }

    const rcsc::GameTime & dataTime() const
      {
          return M_data_time;
      }

    //
    //
    //
    bool theirPlayerTypeChanged() const
      {
          return M_their_player_type_changed;
      }

    int theirPlayerType( const int unum ) const;

    bool markAssignmentChanged() const
      {
          return M_mark_assignment_changed;
      }
    std::pair< int, int > markTargets( const int our_unum ) const;
    int firstMarkTarget( const int our_unum ) const;
    int secondMarkTarget( const int our_unum ) const;

    //
    //
    //

    void clearChangedFlags();


    //
    // opponent player type
    //
    void setTheirPlayerType( const int unum,
                             const int type );

    //
    // mark
    //
    void setMarkAssignment( const int our_unum,
                            const int their_unum_1 );
    void setMarkAssignment( const int our_unum,
                            const int their_unum_1,
                            const int their_unum_2 );
    void clearMarkAssignment( const int our_unum );

    /*!
      \create new clang info message object.
      \return new clang message object that can be registered to CoachAgent.
     */
    rcsc::CLangMessage * buildCLang( const std::string & team_name,
                                     const rcsc::GameTime & current );

    //
    // TODO: move to pimpl object.
    //
    bool buildDataFrom( const rcsc::GameTime & current,
                        const rcsc::CLangMessage::ConstPtr & msg );

    bool buildDataFrom( const rcsc::CLangToken::ConstPtr & tok );
    bool buildDataFrom( const rcsc::CLangDirective::ConstPtr & dir );
    bool buildDataFrom( const bool positive,
                        const bool our,
                        const rcsc::CLangUnumSet::Set & players,
                        const rcsc::CLangAction::ConstPtr & act );
    bool buildDataFrom( const bool positive,
                        const bool our,
                        const rcsc::CLangUnumSet::Set & players,
                        const boost::shared_ptr< const rcsc::CLangActionMark > & mark );
    bool buildDataFrom( const bool positive,
                        const bool our,
                        const rcsc::CLangUnumSet::Set & players,
                        const boost::shared_ptr< const rcsc::CLangActionHeteroType > & htype );

};

#endif
