// -*-c++-*-

/*!
  \file predict_player_object.h
  \brief predict player object class Header File
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA, Hiroki SHIMORA

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

#ifndef RCSC_PLAYER_PREDICT_PLAYER_OBJECT_H
#define RCSC_PLAYER_PREDICT_PLAYER_OBJECT_H

#include <rcsc/player/abstract_player_object.h>

#include <rcsc/common/player_type.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/geom/angle_deg.h>
#include <rcsc/types.h>

#include <boost/shared_ptr.hpp>

#include <vector>

/*!
  \class PredictPlayerObject
  \brief predict player object class
*/
class PredictPlayerObject
    : public rcsc::AbstractPlayerObject {
public:

    typedef boost::shared_ptr< PredictPlayerObject > Ptr; //! pointer type alias
    typedef boost::shared_ptr< const PredictPlayerObject > ConstPtr; //! pointer type alias
    typedef std::vector< Ptr > Cont; //! container type alias

private:

    bool M_valid;
    bool M_is_self;
    int M_ghost_count;

public:

    /*!
      \brief initialize member variables
    */
    PredictPlayerObject()
        : AbstractPlayerObject( -1 ),
          M_valid( false ),
          M_is_self( false ),
          M_ghost_count( 0 )
      { }

    /*!
      \brief initialize member variables using abstract player object
      \param pl base player information
    */
    explicit
    PredictPlayerObject( const rcsc::AbstractPlayerObject & player )
        : AbstractPlayerObject( player.id() )
      {
          copyPlayerInfo( player );
      }

    /*!
      \brief initialize member variables using abstract player object
      \param player base player information
      \param override_pos position to overriding
    */
    PredictPlayerObject( const rcsc::AbstractPlayerObject & player,
                         const rcsc::Vector2D & override_pos )
        : AbstractPlayerObject( player.id() )
      {
          copyPlayerInfo( player );
          M_pos = override_pos;
      }

    /*!
      \brief initialize member variables using abstract player object
    */
    PredictPlayerObject( const rcsc::SideID side,
                         const int unum,
                         const int player_type,
                         const bool is_self,
                         const rcsc::Vector2D & pos,
                         const rcsc::AngleDeg body_dir,
                         const bool goalie = false );

    /*!
      \brief destructor. nothing to do
    */
    virtual
    ~PredictPlayerObject()
      { }

    /*!
      \brief create clone instance
      \return new instance pointer
     */
    Ptr clone() const;

private:

    /*!
      \brief initialize member variables using abstract player object
      \param pl base player information
    */
    void copyPlayerInfo( const AbstractPlayerObject & pl );

public:

    // ------------------------------------------
    /*!
      \brief check if this player is valid or not
      \return true if this player is valid
     */
    bool isValid() const
      {
          return M_valid;
      }

    // ------------------------------------------
    /*!
      \brief check if this player is self or not
      \return true if this player is self
     */
    virtual
    bool isSelf() const
      {
          return M_is_self;
      }

    /*!
      \brief check if this player is ghost object or not
      \return true if this player may be ghost object
     */
    virtual
    bool isGhost() const
      {
          return M_ghost_count > 0;
      }

    /*!
      \brief get the counter value as a ghost recognition
      \return count as a ghost recognition
     */
    virtual
    int ghostCount() const
      {
          return M_ghost_count;
      }

    /*!
      \brief check if player is tackling or not
      \return checked result
     */
    virtual
    bool isTackling() const
      {
          return false;
      }

    //
    //
    //

    /*!
      \brief set positon
      \param p new position
     */
    // void setPos( const rcsc::Vector2D & p )
    //   {
    //       M_pos = p;
    //   }

    // // ------------------------------------------
    // /*!
    //   \brief set distFromBall
    //   \param dist distance from ball
    //  */
    // virtual
    // void setDistFromBall( const double dist )
    //   {
    //       M_dist_from_ball = dist;
    //   }
};

#endif
