//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@//jadeb/.com).
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

#ifndef __HPP_FATCONTROLLER
#define __HPP_FATCONTROLLER
#include "OptionContainer.hpp"
#include "UDSocket.hpp"
#include <string>

extern "C" {

    void sig_chld(int signo);  // The kernal knows nothing of objects so
                               // we have to have a lump of c
    void sig_term(int signo);  // This is so we can kill our children
    void sig_termsafe(int signo);  // This is so we can kill our children safer
    void sig_hup(int signo);  // This is so we know if we should re-read
                              // our config.

    void sig_usr1(int signo);  // This is so we know if we should re-read
                              // our config but not kill current connections
}

static volatile bool ttg = false;

static volatile bool reloadconfig = false;

static volatile bool gentlereload = false;

static volatile bool sig_term_killall = false;

class FatController {



public:
    int controlIt(int pidfilefd);
    bool testProxy(std::string proxyip, int proxyport, bool report);

private:
    int logListener(std::string log_location, int logconerror);
    bool daemonise(int pidfilefd);
    int urlListListener(int logconerror);
    int preFork(int num);
    void mopUpAfterKids();
    bool checkForInput(int fd, int timeout);
    bool checkKidReadyStatus(int tofind);
    int getFreeChild();
    void tellChildAccept(int num);
    void cullChildren(int num);
    void killAllChildren();
    void hupAllChildren();
    void tidyUpForChild();
    int handleConnections(int pipe);
    void addChild(int pos, int fd, pid_t child_pid);
    int sendReadyStatus(int pipe);
    bool getFDFromParent(int fd);
    int getChildSlot();
    int getLineFromFD(int fd, char* buff, int size, int timeout);
    void deleteChild(int child_pid, int stat);
    bool writeToFd(int fd, const char* buff, int len, int timeout);
    bool readyForOutput(int fd, int timeout);
    int selectEINTR(int numfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, struct timeval * timeout);
    bool dropPrivCompletely();
    void flushURLCache();

    int numchildren;  // to keep count of our children
    int busychildren;  // to keep count of our children
    int freechildren;  // to keep count of our children
    int waitingfor;  // num procs waiting for to be preforked
    int* childrenpids;  // so when one exits we know who
    int* childrenstates;  // so we know what they're up to
    struct pollfd* pids;
    int failurecount;
    Socket serversock;  // the socket we will listen on for connections
    UDSocket ipcsock;  // the unix domain socket to be used for ipc with
                       // the forked children
    UDSocket urllistsock;
    Socket peersock;  // the socket which will contain the connection
    // note - remove peersock
    // note - remove previous note
    // note - stop writing notes to yourself to remove notes

    int peersockfd;  // fd which will contain the connection
    String peersockip; // which will contain the connection ip
    int peersockport;  // port connection originates

};

#endif
