//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@/jadeb//.com).
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

#ifndef __HPP_DATABUFFER
#define __HPP_DATABUFFER

#include <exception>
#include "Socket.hpp"
#include "String.hpp"
//#include "OptionContainer.hpp"


class DataBuffer {

public:
    char* buffer[1024];
    char* data;
    unsigned int buffer_length;
    char* compresseddata;
    unsigned int compressed_buffer_length;
    DataBuffer();
    ~DataBuffer();
    void read(Socket *sock, int length) throw(exception);
    int length() {return buffer_length;}
    void copytomemory(char* location);
    bool in(Socket *sock, Socket *peersock, class HTTPHeader *requestheader, class HTTPHeader *docheader, bool runav, int *headersent);  // gives true if it pauses due to too much data
    void out(Socket *sock) throw(exception);
    void setTimeout(int t);
    void setDecompress(String d);
    bool contentRegExp(int filtergroup);
    void swapbacktocompressed();

//private:
    int timeout;
    int tempfilefd;
    unsigned int tempfilesize;
    unsigned int bytesalreadysent;
    String tempfilepath;
    bool preservetemp;
    bool dontsendbody;  // used for fancy download manager for example
    String decompress;
    void zlibinflate(bool header);
    int bufferReadFromSocket(Socket* sock, char* buffer, int size, int sockettimeout);
    int bufferReadFromSocket(Socket* sock, char* buffer, int size, int sockettimeout, int timeout);

    int getTempFileFD();
    int readEINTR(int fd, char *buf, unsigned int count);
    int writeEINTR(int fd, char *buf, unsigned int count);

};

#endif
