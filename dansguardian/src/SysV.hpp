//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@?? jadeb.com).
//For support go to http://groups.yahoo.com/group/dansguardian

//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef __HPP_SYSV
#define __HPP_SYSV
#include <sys/types.h>
#include <string>
#include "OptionContainer.hpp"

class SysV {

public:
    int kill(std::string pidfile);
    int showPID(std::string pidfile);
    int openPIDFile(std::string pidfile);
    int writePIDFile(int pidfilefd);
    bool amIRunning(std::string pidfile);
    int hup(std::string pidfile);
    int usr1(std::string pidfile);

private:
    pid_t getPID(std::string pidfile);
    pid_t getPIDFromFile(std::string pidfile);
    pid_t getPIDFromCommand(const char* command);
    bool confirmName(pid_t p);
};

#endif
