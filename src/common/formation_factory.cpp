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

#include "formation_factory.h"

#include "options.h"

#include <rcsc/formation/formation.h>
#include <rcsc/common/server_param.h>

#include <fstream>
#include <sstream>
#include <iterator>

// TODO: replace with boost::filesystem
#include <sys/types.h> // opendir, readdir, closedir
#include <dirent.h> // opendir, readdir, closedir

using namespace rcsc;

namespace {

std::string
append_last_separator( std::string dir )
{
    if ( dir.empty() )
    {
        dir = "./";
    }
    else if ( *dir.rbegin() != '/' )
    {
        dir += '/';
    }
    return dir;
}

}

/*-------------------------------------------------------------------*/
/*!

 */
FormationFactory::FormationFactory()
    : M_goalie_unum( Unum_Unknown )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
FormationFactory::~FormationFactory()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FormationFactory::init()
{
    if ( ! readAllFormations( Options::instance().formationConfDir() ) )
    {
        return false;
    }

    //
    // set default formations
    //
    if ( ! setFormationFromAllMap( Options::BEFORE_KICK_OFF_BASENAME + ".conf",
                                   &M_before_kick_off_formation )
         || ! setFormationFromAllMap( Options::NORMAL_FORMATION_BASENAME + ".conf",
                                      &M_normal_formation )
         || ! setFormationFromAllMap( Options::DEFENSE_FORMATION_BASENAME + ".conf",
                                      &M_defense_formation )
         || ! setFormationFromAllMap( Options::OFFENSE_FORMATION_BASENAME + ".conf",
                                      &M_offense_formation )
         || ! setFormationFromAllMap( Options::GOAL_KICK_OPP_FORMATION_BASENAME + ".conf",
                                      &M_goal_kick_opp_formation )
         || ! setFormationFromAllMap( Options::GOAL_KICK_OUR_FORMATION_BASENAME + ".conf",
                                      &M_goal_kick_our_formation )
         || ! setFormationFromAllMap( Options::GOALIE_CATCH_OPP_FORMATION_BASENAME + ".conf",
                                      &M_goalie_catch_opp_formation )
         || ! setFormationFromAllMap( Options::GOALIE_CATCH_OUR_FORMATION_BASENAME + ".conf",
                                      &M_goalie_catch_our_formation )
         || ! setFormationFromAllMap( Options::KICKIN_OUR_FORMATION_BASENAME + ".conf",
                                      &M_kickin_our_formation )
         || ! setFormationFromAllMap( Options::SETPLAY_OPP_FORMATION_BASENAME + ".conf",
                                      &M_setplay_opp_formation )
         || ! setFormationFromAllMap( Options::SETPLAY_OUR_FORMATION_BASENAME + ".conf",
                                      &M_setplay_our_formation )
         || ! setFormationFromAllMap( Options::INDIRECT_FREEKICK_OPP_FORMATION_BASENAME + ".conf",
                                      &M_indirect_freekick_opp_formation )
         || ! setFormationFromAllMap( Options::INDIRECT_FREEKICK_OUR_FORMATION_BASENAME + ".conf",
                                      &M_indirect_freekick_our_formation )
         || ! setFormationFromAllMap( Options::CORNER_KICK_OUR_POST_FORMATION_BASENAME + ".conf",
                                      &M_corner_kick_our_post_formation )
         || ! setFormationFromAllMap( Options::CORNER_KICK_OUR_PRE_FORMATION_BASENAME + ".conf",
                                      &M_corner_kick_our_pre_formation )
         || ! setFormationFromAllMap( Options::AGENT2D_SETPLAY_DEFENSE_FORMATION_BASENAME + ".conf",
                                      &M_agent2d_setplay_defense_formation )
         || ! setFormationFromAllMap( Options::GOALIE_POSITION_CONF,
                                      &M_goalie_position ) )
    {
        return false;
    }

    if ( ! readFormationConf( Options::instance().formationConfFile() ) )
    {
        return false;
    }

    if ( ! readOverwriteFormationConf( Options::instance().overwriteFormationConfFile() ) )
    {
        return false;
    }

    if ( ! readHeteroOrder( Options::instance().heteroConfFile() ) )
    {
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
FormationFactory::createFormation( const std::string & filepath )
{
    Formation::Ptr f;

    std::ifstream fin( filepath.c_str() );
    if ( ! fin.is_open() )
    {
        std::cerr << __FILE__ << ':' << __LINE__ << ':'
                  << " ***ERROR*** failed to open file [" << filepath << "]"
                  << std::endl;
        return f;
    }

    f = Formation::create( fin );

    if ( ! f )
    {
        std::cerr << __FILE__ << ':' << __LINE__ << ':'
                  << " ***ERROR*** failed to create formation [" << filepath << "]"
                  << std::endl;
        return f;
    }

    //
    // read data from file
    //
    if ( ! f->read( fin ) )
    {
        std::cerr << __FILE__ << ':' << __LINE__ << ':'
                  << " ***ERROR*** failed to read formation [" << filepath << "]"
                  << std::endl;
        f.reset();
        return f;
    }

    //
    // check goalie number
    //
    for ( int unum = 1; unum <= 11; ++unum )
    {
        const std::string role_name = f->getRoleName( unum );
        if ( role_name == "Savior"
             || role_name == "Goalie" )
        {
            if ( M_goalie_unum == Unum_Unknown )
            {
                M_goalie_unum = unum;
            }

            if ( M_goalie_unum != unum )
            {
                std::cerr << __FILE__ << ':' << __LINE__ << ':'
                          << " ***ERROR*** Illegal goalie's uniform number"
                          << " read unum=" << unum
                          << " expected=" << M_goalie_unum
                          << std::endl;
                f.reset();
                return f;
            }
        }
    }

    return f;
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
FormationFactory::getFormation( const std::string & filename ) const
{
    std::map< std::string, rcsc::Formation::ConstPtr >::const_iterator it = M_all_formation_map.find( filename );
    if ( it != M_all_formation_map.end() )
    {
        return it->second;
    }

    std::cerr << "formation [" << filename << "] not found." << std::endl;
    return Formation::ConstPtr();
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
FormationFactory::getFormation( const SideID our_side,
                                const std::string & team_name,
                                const GameMode & game_mode,
                                const Vector2D & ball_pos,
                                const SituationType current_situation,
                                const bool goalie ) const
{
    //
    // play on
    //
    if ( game_mode.type() == GameMode::PlayOn )
    {
        if ( goalie )
        {
            return getGoaliePosition();
        }

        switch ( current_situation ) {
        case Defense_Situation:
            return getDefenseFormation( team_name );
        case Offense_Situation:
            return getOffenseFormation( team_name );
        default:
            break;
        }
        return getNormalFormation( team_name );
    }

    //
    // kick in, corner kick
    //
    if ( game_mode.type() == GameMode::KickIn_
         || game_mode.type() == GameMode::CornerKick_ )
    {
        if ( our_side == game_mode.side() )
        {
            // our kick-in or corner-kick
            return getOurKickInFormation( team_name );
        }
        else
        {
            return getTheirSetplayFormation( team_name );

        }
    }

    //
    // our indirect free kick
    //
    if ( ( game_mode.type() == GameMode::BackPass_
           && game_mode.side() != our_side )
         || ( game_mode.type() == GameMode::IndFreeKick_
              && game_mode.side() == our_side ) )
    {
        return getOurIndirectFreekickFormation( team_name );
    }

    //
    // opponent indirect free kick
    //
    if ( ( game_mode.type() == GameMode::BackPass_
           && game_mode.side() == our_side )
         || ( game_mode.type() == GameMode::IndFreeKick_
              && game_mode.side() != our_side ) )
    {
        return getTheirIndirectFreekickFormation( team_name );
    }

    //
    // after foul
    //
    if ( game_mode.type() == GameMode::FoulCharge_
         || game_mode.type() == GameMode::FoulPush_ )
    {
        if ( game_mode.side() == our_side )
        {
            //
            // opponent (indirect) free kick
            //
            if ( ball_pos.x < ServerParam::i().ourPenaltyAreaLineX() + 1.0
                 && ball_pos.absY() < ServerParam::i().penaltyAreaHalfWidth() + 1.0 )
            {
                return getTheirIndirectFreekickFormation( team_name );
            }
            else
            {
                return getTheirSetplayFormation( team_name );
            }
        }
        else
        {
            //
            // our (indirect) free kick
            //
            if ( ball_pos.x > ServerParam::i().theirPenaltyAreaLineX()
                 && ball_pos.absY() < ServerParam::i().penaltyAreaHalfWidth() )
            {
                return getOurIndirectFreekickFormation( team_name );
            }
            else
            {
                return getOurSetplayFormation( team_name );
            }
        }
    }

    //
    // goal kick
    //
    if ( game_mode.type() == GameMode::GoalKick_ )
    {
        if ( game_mode.side() == our_side )
        {
            return getOurGoalKickFormation( team_name );
        }
        else
        {
            return getTheirGoalKickFormation( team_name );
        }
    }

    //
    // goalie catch
    //
    if ( game_mode.type() == GameMode::GoalieCatch_ )
    {
        if ( game_mode.side() == our_side )
        {
            return getOurGoalieCatchFormation( team_name );
        }
        else
        {
            return getTheirGoalieCatchFormation( team_name );
        }
    }

    //
    // before kick off
    //
    if ( game_mode.type() == GameMode::BeforeKickOff
         || game_mode.type() == GameMode::AfterGoal_ )
    {
        return getBeforeKickOffFormation( team_name );
    }

    //
    // other set play
    //
    if ( game_mode.isOurSetPlay( our_side ) )
    {
        return getOurSetplayFormation( team_name );
    }

    if ( game_mode.type() != GameMode::PlayOn )
    {
        return getTheirSetplayFormation( team_name );
    }

    //
    // unknown
    //

    if ( goalie )
    {
        return getGoaliePosition();
    }

    switch ( current_situation ) {
    case Defense_Situation:
        return getDefenseFormation( team_name );
    case Offense_Situation:
        return getOffenseFormation( team_name );
    default:
        break;
    }

    return getNormalFormation( team_name );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FormationFactory::readAllFormations( const std::string & formation_dir )
{
    const std::string dir = append_last_separator( formation_dir );

    DIR* dp = opendir( dir.c_str() );
    if ( ! dp )
    {
        std::cerr << __FILE__ << ": (readAllFormations) "
                  << "*** ERROR *** could not read the directory"
                  << " [" << dir << "]" << std::endl;
        return false;
    }

    struct dirent* dent;
    do
    {
        dent = readdir( dp );
        if ( dent )
        {
            std::string filename = dent->d_name;
            if ( filename.length() > 5
                 && filename.compare( filename.length() - 5, 5, ".conf" ) == 0 )
            {
                rcsc::Formation::ConstPtr f = createFormation( dir + filename );
                if ( ! f )
                {
                    std::cerr << __FILE__ << ": (readAllFormations) "
                              << "*** ERROR *** could not read the formation file"
                              << " [" << filename << "]" << std::endl;
                    closedir( dp );
                    return false;
                }

                //std::cerr << "OK read formation [" << filename << "]"
                //          << std::endl;
                M_all_formation_map[filename] = f;
            }
        }
    }
    while ( dent );
    closedir( dp );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FormationFactory::setFormationFromAllMap( const std::string & filename,
                                          rcsc::Formation::ConstPtr * ptr )
{
    Map::iterator it = M_all_formation_map.find( filename );
    if ( it == M_all_formation_map.end() )
    {
        std::cerr << "ERROR: [" << filename << "]"
                  << " has not been registered." << std::endl;
        return false;
    }

    *ptr = it->second;
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FormationFactory::readFormationConf( const std::string & conf_file )
{
    std::ifstream fin( conf_file.c_str() );
    if ( ! fin.is_open() )
    {
        std::cerr << __FILE__ << ':'
                  << "(readFormationConf)"
                  << " ***ERROR*** could not open the file ["
                  << conf_file << "]" << std::endl;
        return false;
    }

    std::string line_buf;
    while ( std::getline( fin, line_buf ) )
    {
        if ( line_buf.empty()
             || line_buf[0] == '#' )
        {
            continue;
        }

        char team_name[32];
        char type_name[32];

        if ( std::sscanf( line_buf.c_str(),
                          " %31s %31s ",
                          team_name, type_name ) != 2 )
        {
            std::cerr << __FILE__ << ':'
                      << "(readFormationConf)"
                      << " *** ERROR *** could not read the line "
                      << '[' << line_buf << ']' << std::endl;
            return false;
        }

        if ( ! assignFormation( team_name,
                                Options::NORMAL_FORMATION_BASENAME, type_name,
                                M_normal_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::OFFENSE_FORMATION_BASENAME, type_name,
                                M_offense_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::DEFENSE_FORMATION_BASENAME, type_name,
                                M_defense_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::BEFORE_KICK_OFF_BASENAME, type_name,
                                M_before_kick_off_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::GOAL_KICK_OPP_FORMATION_BASENAME, type_name,
                                M_goal_kick_opp_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::GOAL_KICK_OUR_FORMATION_BASENAME, type_name,
                                M_goal_kick_our_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::GOALIE_CATCH_OPP_FORMATION_BASENAME, type_name,
                                M_goalie_catch_opp_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::GOALIE_CATCH_OUR_FORMATION_BASENAME, type_name,
                                M_goalie_catch_our_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::KICKIN_OUR_FORMATION_BASENAME, type_name,
                                M_kickin_our_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::SETPLAY_OPP_FORMATION_BASENAME, type_name,
                                M_setplay_opp_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::SETPLAY_OUR_FORMATION_BASENAME, type_name,
                                M_setplay_our_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::INDIRECT_FREEKICK_OPP_FORMATION_BASENAME, type_name,
                                M_indirect_freekick_opp_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::INDIRECT_FREEKICK_OUR_FORMATION_BASENAME, type_name,
                                M_indirect_freekick_our_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::CORNER_KICK_OUR_PRE_FORMATION_BASENAME, type_name,
                                M_corner_kick_pre_formation_map ) )
        {
            return false;
        }
        if ( ! assignFormation( team_name,
                                Options::CORNER_KICK_OUR_POST_FORMATION_BASENAME, type_name,
                                M_corner_kick_post_formation_map ) )
        {
            return false;
        }

        M_assignment_map[team_name] = type_name;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FormationFactory::assignFormation( const std::string & team_name,
                                   const std::string & basename,
                                   const std::string & type_name,
                                   Map & target_formation_map )
{
    if ( target_formation_map.find( team_name ) != target_formation_map.end() )
    {
        std::cerr << __FILE__ << ':'
                  << "(assignFormation)"
                  << " *** ERROR *** already exists:"
                  << " team=[" << team_name << "]"
                  << " type=[" << basename << "-" << type_name << "]"
                  << std::endl;
        return false;
    }

    const std::string filename = basename + "-" + type_name + ".conf";

    Map::iterator it = M_all_formation_map.find( filename );
    if ( it == M_all_formation_map.end() )
    {
        std::cerr <<  __FILE__ << ':'
                  << "(assignFormation)"
                  << " *** ERROR *** [" << filename << "] not found" << std::endl;
        return false;
    }

    target_formation_map[team_name] = it->second;
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FormationFactory::readOverwriteFormationConf( const std::string & conf_file )
{
    std::ifstream fin( conf_file.c_str() );
    if ( ! fin.is_open() )
    {
        std::cerr << __FILE__ << ':'
                  << "(readOverwriteFormationConf)"
                  << " ***ERROR*** could not open the file ["
                  << conf_file << "]" << std::endl;
        return false;
    }

    std::string line_buf;
    while ( std::getline( fin, line_buf ) )
    {
        if ( line_buf.empty()
             || line_buf[0] == '#' )
        {
            continue;
        }

        char team_name[32];
        char type_name[32];

        if ( std::sscanf( line_buf.c_str(),
                          " %31s %31s ",
                          team_name, type_name ) != 2 )
        {
            std::cerr << __FILE__ << ':'
                      << "(readOverwriteFormationConf)"
                      << " *** ERROR *** could not read the line "
                      << '[' << line_buf << ']' << std::endl;
            return false;
        }

        if ( ! overwriteAllFormations( team_name, type_name ) )
        {
            return false;
        }

        M_overwrite_assignment_map[team_name] = type_name;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FormationFactory::overwriteAllFormations( const std::string & team_name,
                                          const std::string & type_name )
{
    bool result = false;
    bool corner_kick_pre = false;
    bool corner_kick_post = false;

    if ( overwriteFormation( team_name,
                             Options::NORMAL_FORMATION_BASENAME,
                             type_name,
                             M_normal_formation_map ) )
    {
        result = true;
    }
    if ( overwriteFormation( team_name,
                             Options::OFFENSE_FORMATION_BASENAME,
                             type_name,
                             M_offense_formation_map ) )    {
        result = true;
    }
    if ( overwriteFormation( team_name,
                             Options::DEFENSE_FORMATION_BASENAME,
                             type_name,
                             M_defense_formation_map ) )
    {
        result = true;
    }
    if ( overwriteFormation( team_name,
                             Options::BEFORE_KICK_OFF_BASENAME,
                             type_name,
                             M_before_kick_off_formation_map ) )
    {
        result = true;
    }
    if ( overwriteFormation( team_name,
                             Options::GOAL_KICK_OPP_FORMATION_BASENAME,
                             type_name,
                             M_goal_kick_opp_formation_map ) )
    {
        result = true;
    }
    if ( overwriteFormation( team_name,
                             Options::GOAL_KICK_OUR_FORMATION_BASENAME,
                             type_name,
                             M_goal_kick_our_formation_map ) )
    {
        result = true;
    }
    if ( overwriteFormation( team_name,
                             Options::GOALIE_CATCH_OPP_FORMATION_BASENAME,
                             type_name,
                             M_goalie_catch_opp_formation_map ) )
    {
        result = true;
    }
    if ( overwriteFormation( team_name,
                             Options::GOALIE_CATCH_OUR_FORMATION_BASENAME,
                             type_name,
                             M_goalie_catch_our_formation_map ) )
    {
        result = true;
    }
    if ( overwriteFormation( team_name,
                             Options::KICKIN_OUR_FORMATION_BASENAME,
                             type_name,
                             M_kickin_our_formation_map ) )
    {
        result = true;
    }
    if ( overwriteFormation( team_name,
                             Options::SETPLAY_OPP_FORMATION_BASENAME,
                             type_name,
                             M_setplay_opp_formation_map ) )
    {
        result = true;
    }
    if ( overwriteFormation( team_name,
                             Options::SETPLAY_OUR_FORMATION_BASENAME,
                             type_name,
                             M_setplay_our_formation_map ) )
    {
        result = true;
    }
    if ( overwriteFormation( team_name,
                             Options::INDIRECT_FREEKICK_OPP_FORMATION_BASENAME,
                             type_name,
                             M_indirect_freekick_opp_formation_map ) )
    {
        result = true;
    }
    if ( overwriteFormation( team_name,
                             Options::INDIRECT_FREEKICK_OUR_FORMATION_BASENAME,
                             type_name,
                             M_indirect_freekick_our_formation_map ) )
    {
        result = true;
    }
    if ( overwriteFormation( team_name,
                             Options::CORNER_KICK_OUR_PRE_FORMATION_BASENAME,
                             type_name,
                             M_corner_kick_pre_formation_map ) )
    {
        result = true;
        corner_kick_pre = true;
    }
    if ( overwriteFormation( team_name,
                             Options::CORNER_KICK_OUR_POST_FORMATION_BASENAME,
                             type_name,
                             M_corner_kick_post_formation_map ) )
    {
        result = true;
        corner_kick_post = true;
    }

    if ( ! result )
    {
        std::cerr << __FILE__ << ':'
                  << "(overwriteAllFormation)"
                  << " *** ERROR *** type=[" << type_name << "] not found."
                  << std::endl;
        return false;
    }

    if ( corner_kick_pre && ! corner_kick_post )
    {
        std::cerr << __FILE__ << ':'
                  << "(overwriteAllFormation)"
                  << " *** ERROR *** corner-kick-post-[" << type_name << "] not found."
                  << std::endl;
        return false;
    }

    if ( ! corner_kick_pre && corner_kick_post )
    {
        std::cerr << __FILE__ << ':'
                  << "(overwriteAllFormation)"
                  << " *** ERROR *** corner-kick-pre-[" << type_name << "] not found."
                  << std::endl;
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FormationFactory::overwriteFormation( const std::string & team_name,
                                      const std::string & basename,
                                      const std::string & type_name,
                                      Map & target_formation_map )
{
#if 0
    if ( target_formation_map.find( team_name ) != target_formation_map.end() )
    {
        std::cerr << __FILE__ << ':'
                  << "(overwriteFormation)"
                  << " *** ERROR *** default formation not found for "
                  << " team=[" << team_name << "]"
                  << " type=[" << basename << "]"
                  << std::endl;
    }
#endif

    const std::string filename = basename + "-" + type_name + ".conf";

    Map::iterator it = M_all_formation_map.find( filename );
    if ( it == M_all_formation_map.end() )
    {
        //std::cerr <<  __FILE__ << ':'
        //           << "(overwriteFormation)"
        //           << " *** ERROR *** [" << filename << "] not found" << std::endl;
        return false;
    }

    // std::cerr << "team=[" << team_name << "]"
    //           << " type=[" << type_name << "]"
    //           << " " << basename
    //           << std::endl;

    target_formation_map[team_name] = it->second;
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
FormationFactory::readHeteroOrder( const std::string & conf_file )
{
    std::ifstream fin( conf_file.c_str() );
    if ( ! fin.is_open() )
    {
        std::cerr << __FILE__ << ':'
                  << "(readHeteroOrder)"
                  << " ***ERROR*** could not open the file ["
                  << conf_file << "]" << std::endl;
        return false;
    }

    std::string line_buf;
    while ( std::getline( fin, line_buf ) )
    {
        if ( line_buf.empty()
             || line_buf[0] == '#' )
        {
            continue;
        }

        std::string type_name;
        int num;

        std::istringstream istr( line_buf );

        if ( ! ( istr >> type_name ) )
        {
            std::cerr << __FILE__ << ':'
                      << "(readHeteroOrder)"
                      << " ***ERROR*** illegal line [" << line_buf
                      << "]" << std::endl;
            return false;
        }

        std::vector< int > & v = M_hetero_order_map[type_name];
        if ( ! v.empty() )
        {
            std::cerr << __FILE__ << ':'
                      << "(readHeteroOrder)"
                      << " ***ERROR*** type [" << type_name << "] already exists" << std::endl;
            return false;
        }

        while ( istr >> num )
        {
            if ( num < 1 || 11 < num )
            {
                std::cerr << __FILE__ << ':'
                          << "(readHeteroOrder)"
                          << " ***ERROR*** illegal number [" << line_buf << "]" << std::endl;
                return false;
            }

            if ( std::find( v.begin(), v.end(), num ) != v.end() )
            {
                std::cerr << __FILE__ << ':'
                          << "(readHeteroOrder)"
                          << " ***ERROR*** duplicated [" << line_buf << "]" << std::endl;
                return false;
            }

            v.push_back( num );
            if ( v.size() > 11 )
            {
                std::cerr << __FILE__ << ':'
                          << "(readHeteroOrder)"
                          << " ***ERROR*** illegal size [" << line_buf << "]" << std::endl;
                return false;
            }
        }

        // std::cerr << "Hetero order: type=[" << type_name << "] order=[";
        // std::copy( v.begin(), v.end(), std::ostream_iterator< int >( std::cerr, "," ) );
        // std::cerr << "]" << std::endl;
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
FormationFactory::getBeforeKickOffFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_before_kick_off_formation_map.find( team_name );
    if ( it != M_before_kick_off_formation_map.end() )
    {
        return it->second;
    }

    return getBeforeKickOffFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
FormationFactory::getNormalFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_normal_formation_map.find( team_name );
    if ( it != M_normal_formation_map.end() )
    {
        return it->second;
    }

    return getNormalFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
FormationFactory::getDefenseFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_defense_formation_map.find( team_name );
    if ( it != M_defense_formation_map.end() )
    {
        return it->second;
    }

    return getDefenseFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
FormationFactory::getOffenseFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_offense_formation_map.find( team_name );
    if ( it != M_offense_formation_map.end() )
    {
        return it->second;
    }

    return getOffenseFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
rcsc::Formation::ConstPtr
FormationFactory::getOurGoalKickFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_goal_kick_our_formation_map.find( team_name );
    if ( it != M_goal_kick_our_formation_map.end() )
    {
        return it->second;
    }

    return getOurGoalKickFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
rcsc::Formation::ConstPtr
FormationFactory::getTheirGoalKickFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_goal_kick_opp_formation_map.find( team_name );
    if ( it != M_goal_kick_opp_formation_map.end() )
    {
        return it->second;
    }

    return getTheirGoalKickFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
rcsc::Formation::ConstPtr
FormationFactory::getTheirGoalieCatchFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_goalie_catch_opp_formation_map.find( team_name );
    if ( it != M_goalie_catch_opp_formation_map.end() )
    {
        return it->second;
    }

    return getTheirGoalieCatchFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
rcsc::Formation::ConstPtr
FormationFactory::getOurGoalieCatchFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_goalie_catch_our_formation_map.find( team_name );
    if ( it != M_goalie_catch_our_formation_map.end() )
    {
        return it->second;
    }

    return getOurGoalieCatchFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
rcsc::Formation::ConstPtr
FormationFactory::getOurKickInFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_kickin_our_formation_map.find( team_name );
    if ( it != M_kickin_our_formation_map.end() )
    {
        return it->second;
    }

    return getOurKickInFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
rcsc::Formation::ConstPtr
FormationFactory::getTheirSetplayFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_setplay_opp_formation_map.find( team_name );
    if ( it != M_setplay_opp_formation_map.end() )
    {
        return it->second;
    }

    return getTheirSetplayFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
rcsc::Formation::ConstPtr
FormationFactory::getOurSetplayFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_setplay_our_formation_map.find( team_name );
    if ( it != M_setplay_our_formation_map.end() )
    {
        return it->second;
    }

    return getOurSetplayFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
rcsc::Formation::ConstPtr
FormationFactory::getTheirIndirectFreekickFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_indirect_freekick_opp_formation_map.find( team_name );
    if ( it != M_indirect_freekick_opp_formation_map.end() )
    {
        return it->second;
    }

    return getTheirIndirectFreekickFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
rcsc::Formation::ConstPtr
FormationFactory::getOurIndirectFreekickFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_indirect_freekick_our_formation_map.find( team_name );
    if ( it != M_indirect_freekick_our_formation_map.end() )
    {
        return it->second;
    }

    return getOurIndirectFreekickFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
FormationFactory::getCornerKickPreFormation( const std::string & team_name ) const
{
    Map::const_iterator it = M_corner_kick_pre_formation_map.find( team_name );
    if ( it != M_corner_kick_pre_formation_map.end() )
    {
        return it->second;
    }

    // return the default type
    return getCornerKickOurPreFormation();
}

/*-------------------------------------------------------------------*/
/*!

 */
Formation::ConstPtr
FormationFactory::getCornerKickPostFormation( const std::string & team_name,
                                              const std::string & /*cornerkick_type*/ ) const
{
    Map::const_iterator it = M_corner_kick_post_formation_map.find( team_name );
    if ( it != M_corner_kick_post_formation_map.end() )
    {
        return it->second;
    }

    // return the default type
    return getCornerKickOurPostFormation();
}
