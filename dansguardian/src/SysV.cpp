//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@ //jadeb.com).
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

#include "platform.h"
#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <fcntl.h>
#include <csignal>
#include "SysV.hpp"
#include <climits>
#include "String.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <cerrno>

extern OptionContainer o;

int SysV::kill(std::string pidfile) {
    pid_t p = getPID(pidfile);
    if (p > 1) {
        int rc = ::kill(p, SIGTERM);
        if (rc == -1) {
            std::cerr << "Error trying to kill pid:" << p << std::endl;
            if (errno == EPERM) {
                std::cerr << "Permission denied." << std::endl;
            }
            return 1;
        }
        unlink(pidfile.c_str());
        unlink(o.ipc_filename.c_str());
        unlink(o.urlipc_filename.c_str());
        return 0;
    }
    std::cerr << "No DansGuardian process found." << std::endl;
    return 1;
}


int SysV::hup(std::string pidfile) {
    pid_t p = getPID(pidfile);
    if (p > 1) {
        int rc = ::kill(p, SIGHUP);
        if (rc == -1) {
            std::cerr << "Error trying to hup pid:" << p << std::endl;
            if (errno == EPERM) {
                std::cerr << "Permission denied." << std::endl;
            }
            return 1;
        }
        return 0;
    }
    std::cerr << "No DansGuardian process found." << std::endl;
    return 1;
}

int SysV::usr1(std::string pidfile) {
    pid_t p = getPID(pidfile);
    if (p > 1) {
        int rc = ::kill(p, SIGUSR1);
        if (rc == -1) {
            std::cerr << "Error trying to sig1 pid:" << p << std::endl;
            if (errno == EPERM) {
                std::cerr << "Permission denied." << std::endl;
            }
            return 1;
        }
        return 0;
    }
    std::cerr << "No DansGuardian process found." << std::endl;
    return 1;
}


int SysV::showPID(std::string pidfile) {
    pid_t p = getPID(pidfile);
    if (p > 1) {
        std::cout << "Parent DansGuardian pid:" << p << std::endl;
        return 0;
    }
    std::cerr << "No DansGuardian process found." << std::endl;
    return 1;
}


int SysV::openPIDFile(std::string pidfile) {
    unlink(pidfile.c_str());
    return open(pidfile.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}


int SysV::writePIDFile(int pidfilefd) {
    pid_t p = getpid();
    char pidbuff[32];
    sprintf(pidbuff, "%d", (int)p);  // Messy, but it works!
    int len = strlen(pidbuff) + 1;
    pidbuff[len - 1] = '\n';
    int rc = write(pidfilefd, pidbuff, len);
    if (rc < len) {  // failed to write
        close(pidfilefd);
        return 1;
    }
    close(pidfilefd);
    return 0;
}


pid_t SysV::getPIDFromFile(std::string pidfile) {
    int handle = open(pidfile.c_str(), O_RDONLY);
    if (handle < 0) {  // Unable to open the pid file.
        return -1;
    }
    char pidbuff[32];
    int rc = read(handle, pidbuff, sizeof(pidbuff)-1);
    if (rc < 1) {  // pid file must be at least 1 byte long
        close(handle);
        return -1;
    }
    pidbuff[rc] = '\0';
    close(handle);
    return atoi(pidbuff);  // convert the string to a pid
}


pid_t SysV::getPIDFromCommand(const char* command) {
    return -1;  // ******** NOT IMPLEMENTED YET ************
                // only needed if the pid file gets deleted.

                //I KNOW HOW TO DO THIS IN A PORTABLE LINUX/BSD WAY
                //BUT I HAVE NOT HAD THE TIME TO ADD IT YET AS THIS
                //IS FUNCTIONAL ENOUGH TO WORK
}


bool SysV::confirmName(pid_t p) {
    int rc = ::kill(p, 0);  // just a test
    if (rc != 0) {
        if (errno == EPERM) {
            return true;  // we got no perms to test it but it must be there
        }
        return false;  // no process running at that pid
    }

    // ******** NOT FULLY IMPLEMENTED YET ************

                //I KNOW HOW TO DO THIS IN A PORTABLE LINUX/BSD WAY
                //BUT I HAVE NOT HAD THE TIME TO ADD IT YET AS THIS
                //IS FUNCTIONAL ENOUGH TO WORK

    return true;
}

pid_t SysV::getPID(std::string pidfile) {
    pid_t p = getPIDFromFile(pidfile);
    if (p > 1) {
        if (confirmName(p)) {  // is that pid really DG and running?
            return p;  // it is so return it
        }
    }
    pid_t t = getPIDFromCommand("dansguardian");  // pid file method failed
                                           // so try a search from the
                                           // command
    return t;  // if it failed t will be -1
}


bool SysV::amIRunning(std::string pidfile) {
    if (getPID(pidfile) > 1) {
        return true;
    }
    return false;
}
