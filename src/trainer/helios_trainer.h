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

#ifndef HELIOS_TRAINER_H
#define HELIOS_TRAINER_H

#include <rcsc/trainer/trainer_agent.h>

class HeliosTrainer
    : public rcsc::TrainerAgent {
private:

  int M_count;
  int M_time;
  bool M_isSetplay;
  int M_x_cordinate;

  std::vector<double> M_opp_pos_x;
  std::vector<double> M_opp_pos_y;
  std::vector<double> M_mate_pos_x;
  std::vector<double> M_mate_pos_y;
  std::vector<double> M_ball_pos_x;
  std::vector<double> M_ball_pos_y;

  std::vector< std::vector<double> > M_ball_pos;
  std::vector< std::vector< std::vector<double> > > M_mate_pos;
  std::vector< std::vector< std::vector<double> > > M_opp_pos;

public:

    HeliosTrainer();

    virtual
    ~HeliosTrainer();

protected:

    /*!
      You can override this method.
      But you must call TrainerAgent::doInit() in this method.
    */
    virtual
    bool initImpl( rcsc::CmdLineParser & cmd_parser );

    //! main decision
    virtual
    void actionImpl();


    virtual
    void handleInitMessage();
    virtual
    void handleServerParam();
    virtual
    void handlePlayerParam();
    virtual
    void handlePlayerType();

private:

    void sampleAction();
    void recoverForever();
    void doSubstitute();
    void doKeepaway();
    void doSetplay();
    bool readSetplayCoordinate( const std::string & coordinate_dir );

};

#endif
