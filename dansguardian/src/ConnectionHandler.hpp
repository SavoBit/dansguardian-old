//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@jadeb//.com).
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

#ifndef __HPP_CONNECTIONHANDLER
#define __HPP_CONNECTIONHANDLER
#include <iostream>
#include <string>
#include "OptionContainer.hpp"
#include "Socket.hpp"
#include "HTTPHeader.hpp"
#include "Socket.hpp"
#include "NaughtyFilter.hpp"

class ConnectionHandler {

public:
    void handleConnection(int peerfd, String ip, int port);

private:
    void doTheLogMan(std::string who, std::string from, std::string where, std::string what, std::string how, int size, struct timeval *thestart, bool cachehit, int code, std::string mimetype);
    std::string miniURLEncode(std::string s);
    void decideHowToLog(std::string who, std::string from, std::string where, unsigned int port, std::string what, std::string how, int size, int loglevel, bool isnaughtly, bool isexception, int logexceptions, bool istext, struct timeval *thestart, bool cachehit, int code, std::string mimetype, bool wasinfected, bool wasscanned);
    bool wasClean(String url);
    void addToClean(String url);
    void requestChecks(HTTPHeader *header, NaughtyFilter *checkme, String *urld, std::string *clientip, std::string *clientuser, int filtergroup, bool *ispostblock);
    bool isIPHostnameStrip(String url);
    int determineGroup(std::string *user);
    bool denyAccess (Socket *peerconn, Socket *proxysock, HTTPHeader *header, HTTPHeader *docheader, String *url, NaughtyFilter *checkme, std::string *clientuser, std::string *clientip, int filtergroup, bool ispostblock, int headersent);
    String hashedURL(String *url, int filtergroup, std::string *clientip);
    String hashedCookie(String *url, int filtergroup, std::string *clientip, int bypasstimestamp);
    void contentFilter(HTTPHeader *docheader, HTTPHeader *header, DataBuffer *docbody, Socket *proxysock, Socket *peerconn, int *headersent, bool *pausedtoobig, int *docsize, NaughtyFilter *checkme, bool runav, bool wasclean, bool cachehit, int filtergroup, std::deque<bool> *sendtoscanner, std::string *clientuser, std::string *clientip, bool *wasinfected, bool *wasscanned);
    unsigned int sendFile(Socket *peerconn, String &filename, String &filemime, String &filedis);
    int readEINTR(int fd, char *buf, unsigned int count);
};
#endif
