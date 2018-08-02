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

#ifndef DEFAULT_FREEFORM_MESSAGE_H
#define DEFAULT_FREEFORM_MESSAGE_H

#include <rcsc/common/freeform_message.h>


/*-------------------------------------------------------------------*/

/*!
  \class GoalieMessage
  \brief goalie's uniform number
*/
class GoalieUnumMessage
    : public rcsc::FreeformMessage {
private:

    std::string M_message;

    // not used
    GoalieUnumMessage();
public:

    GoalieUnumMessage( const int our_unum,
                       const int their_unum );

    virtual
    int length() const
      {
          return M_message.length();
      }

    virtual
    bool append( std::string & to ) const;

    virtual
    std::ostream & printDebug( std::ostream & os ) const;

};

/*-------------------------------------------------------------------*/

/*!
  \class OpponentPlayerTypeMessage
  \brief estimated opponent player type message
*/
class OpponentPlayerTypeMessage
    : public rcsc::FreeformMessage {
private:

    //! array of opponent player type id
    int M_player_type_id[11];
    //! built message string
    std::string M_message;

public:

    /*!
      \brief construct with all opponentnplayer type id
      \param t1 player type id
      \param t2 player type id
      \param t3 player type id
      \param t4 player type id
      \param t5 player type id
      \param t6 player type id
      \param t7 player type id
      \param t8 player type id
      \param t9 player type id
      \param t10 player type id
      \param t11 player type id
     */
    OpponentPlayerTypeMessage( const int t1,
                               const int t2,
                               const int t3,
                               const int t4,
                               const int t5,
                               const int t6,
                               const int t7,
                               const int t8,
                               const int t9,
                               const int t10,
                               const int t11 );

    /*!
      \brief set player type id. NOTE: if you use this method, you have to call buildMessage() manually.
      \param unum opponent's uniform number.
      \param id player type id.
     */
    void setPlayerType( const int unum,
                        const int id );


    /*!
      \brief (re)build cached message string.
     */
    void buildMessage();

    /*!
      \brief get the length of this message
      \return the length of encoded message
    */
    virtual
    int length() const
      {
          return M_message.length();
      }

    /*!
      \brief append the freeform message to 'to'.
      \param to reference to the result message string.
      \return result of message appending.
    */
    virtual
    bool append( std::string & to ) const;

    /*!
      \brief output a debug message.
      \param os reference to the output stream.
      \return reference to the output stream.
     */
    virtual
    std::ostream & printDebug( std::ostream & os ) const;
};


/*-------------------------------------------------------------------*/

/*!
  \class OpponentFormationMessage
  \brief estimated opponent formation message
*/
class OpponentFormationMessage
    : public rcsc::FreeformMessage {
private:

    //! array of opponent role type id
    int M_role_types[11];
    //! cached message
    std::string M_message;

    // not used
    OpponentFormationMessage();
public:

    OpponentFormationMessage( const int t1,
                              const int t2,
                              const int t3,
                              const int t4,
                              const int t5,
                              const int t6,
                              const int t7,
                              const int t8,
                              const int t9,
                              const int t10,
                              const int t11 );

    void buildMessage();

    virtual
    int length() const
      {
          return M_message.length();
      }

    virtual
    bool append( std::string & to ) const;

    virtual
    std::ostream & printDebug( std::ostream & os ) const;

};


#if 0
/*-------------------------------------------------------------------*/

/*!
  \class OpponentCoordinationMessage
  \brief estimated opponent coordination message
*/
class OpponentCoordinationMessage
    : public rcsc::FreeformMessage {
public:

    /*!
      \brief get the length of this message
      \return the length of encoded message
    */
    int length() const;

    /*!
      \brief append the freeform message to 'to'.
      \param to reference to the result message string.
      \return result of message appending.
    */
    bool append( std::string & to ) const;

    /*!
      \brief output a debug message.
      \param os reference to the output stream.
      \return reference to the output stream.
     */
    std::ostream & printDebug( std::ostream & os ) const;

};
#endif

/*-------------------------------------------------------------------*/
/*!
  \class CornerKickTypeMessage
  \brief opponent cornerkick analyzed message
*/
class CornerKickTypeMessage
    : public rcsc::FreeformMessage {
private:

    std::string M_message;

    // not used
    CornerKickTypeMessage();
public:

    CornerKickTypeMessage( const std::string & cornerkick_type );

    virtual
    int length() const
      {
          return M_message.length();
      }

    virtual
    bool append( std::string & to ) const;

    virtual
    std::ostream & printDebug( std::ostream & os ) const;

};

#endif


/*-------------------------------------------------------------------*/

/*!
  \class WallDetectorMessage
  \brief opponent cornerkick analyzed message
*/
class OpponentWallDetectorMessage
    : public rcsc::FreeformMessage {
private:

    std::string M_message;

    // not used
    OpponentWallDetectorMessage();
public:

    OpponentWallDetectorMessage( const std::string defense_type );

    virtual
    int length() const
      {
          return M_message.length();
      }

    virtual
    bool append( std::string & to ) const;

    virtual
    std::ostream & printDebug( std::ostream & os ) const;

};
