//Please refer to http://dansguardian.org/?page=copyright
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

#define __DGHEADER_SENDALL 0
#define __DGHEADER_SENDFIRSTLINE 1
#define __DGHEADER_SENDREST 2

#include "platform.h"
#include "DataBuffer.hpp"
#include <syslog.h>
#include "String.hpp"
#include "OptionContainer.hpp"
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <zlib.h>
#include <cerrno>
#include <fstream>
#include <sys/time.h>
#include <sys/types.h>

#ifdef __GCCVER3
    #include <istream>
#else
    #include <istream.h>
#endif

extern OptionContainer o;

DataBuffer::DataBuffer()
:data(new char[0]),buffer_length(0),compresseddata(NULL),compressed_buffer_length(0),timeout(20),tempfilefd(-1),tempfilesize(0),bytesalreadysent(0),preservetemp(false),dontsendbody(false) {}
// set buffer length to zero and initialise the char* memory block to hold
// the data - initialisation/creation function


DataBuffer::~DataBuffer() {
    delete[] data;
    if (compresseddata != NULL) {
        delete[] compresseddata;
        compresseddata = NULL;
    }
    if (tempfilefd > -1) {
        close(tempfilefd);
        if (!preservetemp) {
            unlink(tempfilepath.toCharArray());
        }
        tempfilefd = -1;
        tempfilesize = 0;
    }
}
// delete the memory block when the class is destryed

#include "HTTPHeader.hpp"

bool DataBuffer::in(Socket *sock, Socket *peersock, HTTPHeader *requestheader, HTTPHeader *docheader, bool runav, int *headersent) {
    //Socket *sock = where to read from
    //Socket *peersock = browser to send stuff to for keeping it alive
    //HTTPHeader *requestheader = header client used to request
    //HTTPHeader *docheader = header used for sending first line of reply
    //bool runav = to determine if limit is av or not
    //int *headersent = to use to send the first line of header if needed
    //                  or to mark that the header has already been sent

    bool toobig = false;  // so we know if we only partially downloaded from
    // squid so later, if allowed, we can send the rest

    String useragent = requestheader->userAgent();  // match user agent to
    // download manager so browsers potentially can have a prettier
    // version and software updates can have a compatible version

    #ifdef DGDEBUG
        std::cerr << "User agent:" << useragent << std::endl;
    #endif

    int rc = 0;
    for(unsigned int i = 0; i < o.dmplugins_regexp.size(); i++) {
        #ifdef DGDEBUG
            std::cerr << "Matching download manager number:" << i << std::endl;
        #endif
        if ( (i + 1) == o.dmplugins_regexp.size()) {
            #ifdef DGDEBUG
                std::cerr << "Got to final download manager so defaulting to always match." << std::endl;
            #endif
            rc = o.dmplugins[i]->in(&o, this, sock, peersock, requestheader, docheader, runav, headersent, &toobig);
            break;
        }
        else {
            o.dmplugins_regexp[i].match(useragent.toCharArray());
            if (o.dmplugins_regexp[i].matched()) {
                rc = o.dmplugins[i]->in(&o, this, sock, peersock, requestheader, docheader, runav, headersent, &toobig);
                break;
            }
        }
    }
    // we should check rc and log on error/warn
    // note for later - Tue 16th November 2004
    return toobig;
}

void DataBuffer::read(Socket *sock, int l) throw (exception)  {
    delete[] data;  // delete the current data store (should be emtpy anyway)
    data = new char[l + 2];  // create a new store large enough
    int rc;

    rc = (*sock).readFromSocketn(data, l, 0, timeout); // read in the [POST] data

    if (rc < 0) {
        throw exception();  // danger, danger Will Robinson
    }

  // The above should be all that's needed - but wait there's more!
  // Normal data appended to the header by POST is actually 2 bytes longer
  // than the Content-Length header says.  It contains a carrage return and
  // a new line character.  Simple - just add a fudgefactor of 2.
  // No.  Because when uploading a file via a form the POST data is
  // *exactly* as stated and trying to read even 1 more byte will cause the
  // read to hang.  Also Netscape 4.7x it does it differently.
  // So we need to check the status of the connection to see if there really
  // are more bytes to read.

  if ((*sock).checkForInput()) {
      rc = (*sock).readFromSocket(data + l, 2, 0, timeout);
      // if an error occured (rc < 1) we ignore it and try and continue
      if (rc > 0) {
          l += 2;  // adjust the length
      }
  }

  buffer_length = l;  // update data size counter

}

void DataBuffer::out(Socket *sock) throw(exception) {
    if (dontsendbody) {
        return;
    }
    (*sock).readyForOutput(timeout);  // exceptions on timeout or error

    if (tempfilefd > -1) {  // must have been too big for ram so stream
                            // from disk in blocks
        #ifdef DGDEBUG
            std::cout << "sending body from temp file" << std::endl;
        #endif
        unsigned int sent = bytesalreadysent;
        int rc;
        lseek(tempfilefd, bytesalreadysent, SEEK_SET);
        while (sent < tempfilesize) {
            rc = readEINTR(tempfilefd, data, buffer_length);
            #ifdef DGDEBUG
                std::cout << "reading temp file rc:" << rc << std::endl;
            #endif
            if (rc < 0) {
                #ifdef DGDEBUG
                    std::cout << "error reading temp file so throwing exception" << std::endl;
                #endif
                throw exception();
            }
            if (rc == 0) {
                #ifdef DGDEBUG
                    std::cout << "got zero bytes reading temp file" << std::endl;
                #endif
                break;  // should never happen
            }
            // as it's cached to disk the buffer must be reasonably big
            if (!(*sock).writeToSocket(data, rc, 0, timeout)) {
                throw exception();
            }
            sent += rc;
            #ifdef DGDEBUG
                std::cout << "total sent from temp:" << sent << std::endl;
            #endif
        }
    }
    else {
        // need exception or something for a bad write
        if (!(*sock).writeToSocket(data + bytesalreadysent, buffer_length - bytesalreadysent, 0, timeout)) {
            throw exception();
        }  // write the data block out to the stream
    }
}

void DataBuffer::copytomemory(char* location) {
  memcpy(location, data, buffer_length);  // doh?  what does this do?
}

void DataBuffer::setTimeout(int t) {
    timeout = t;
}

void DataBuffer::setDecompress(String d) {
    decompress = d;
}


void DataBuffer::zlibinflate(bool header) {
    if (buffer_length < 12) {
        return; // it can't possibly be zlib'd
    }
    #ifdef DGDEBUG
        std::cout << "compressed size:" << buffer_length << std::endl;
    #endif

    #if ZLIB_VERNUM < 0x1210
        #warning ************************************
        #warning For gzip support you need zlib 1.2.1
        #warning or later to be installed.
        #warning You can ignore this warning but
        #warning internet bandwidth may be wasted.
        #warning ************************************
        if (header) {
            return;
        }
    #endif

//    ofstream logfile("/tmp/zlib", ios::out);
//    logfile.write(data, buffer_length);
//    logfile.close();

    int newsize = buffer_length * 5;  // good estimate of deflated HTML

    char* block;
    char* temp;
    block = new char[newsize];
    int err;
    int bout;
    int bytesgot = 0;

    z_stream d_stream;
    d_stream.zalloc = (alloc_func)0;
    d_stream.zfree = (free_func)0;
    d_stream.opaque = (voidpf)0;
    d_stream.next_in = (Bytef*)data;
    d_stream.avail_in = buffer_length;
    d_stream.next_out = (Bytef*)block;
    d_stream.avail_out = newsize;

    if (header) {
        err = inflateInit2(&d_stream, 15 + 32);
    }
    else {
        err = inflateInit2(&d_stream, -15);
    }


    if (err != Z_OK) {  // was a problem so just return
        delete[] block; // don't forget to free claimed memory
        #ifdef DGDEBUG
            std::cerr << "bad init inflate:" << err << std::endl;
        #endif
        return;
    }
    while (true) {
        #ifdef DGDEBUG
            std::cerr << "inflate loop" << std::endl;
        #endif
        err = inflate(&d_stream, Z_NO_FLUSH);
        bout = newsize - bytesgot - d_stream.avail_out;
        bytesgot += bout;
        if (err == Z_STREAM_END) {
            break;
        }
        if (err != Z_OK) {  // was a problem so just return
            delete[] block; // don't forget to free claimed memory
            #ifdef DGDEBUG
                std::cerr << "bad inflate:" << String(err) << std::endl;

            #endif
            return;
        }

        newsize = bytesgot * 2;
        temp = new char[newsize];
        memcpy(temp, block, bytesgot);
        delete[] block;
        block = temp;

        d_stream.next_out = (Bytef*)(block + bytesgot);
        d_stream.avail_out = newsize - bytesgot;
    }
    compresseddata = data;
    compressed_buffer_length = buffer_length;
    data = block;
    buffer_length = bytesgot;
    // I could create a new block the exact size and memcpy
    // it over to save RAM but RAM is cheap and this saves CPU
//    data = new char[bytesgot];
//    memcpy(data, block, bytesgot);
//    delete[] block;

}

// Does a regexp search and replace.
bool DataBuffer::contentRegExp(int filtergroup) {

    #ifdef DGDEBUG
        std::cout << "Starting content reg exp replace" << std::endl;
    #endif
    bool contentmodified = false;
    unsigned int i;
    int j;
    unsigned int s =  (*o.fg[filtergroup]).content_regexp_list_comp.size();
    int matches;
    String exp;
    String replacement;
    int replen;
    int sizediff;
    char* newblock;
    char* dstpos;
    unsigned int srcoff;
    unsigned int nextoffset;
    for (i = 0; i < s; i++) {
        if ((*o.fg[filtergroup]).content_regexp_list_comp[i].match(data)) {
            replacement = (*o.fg[filtergroup]).content_regexp_list_rep[i];
            replen = replacement.length();
            matches = (*o.fg[filtergroup]).content_regexp_list_comp[i].numberOfMatches();
            sizediff = matches * replen;
            for (j = 0; j < matches; j++) {
                sizediff -= (*o.fg[filtergroup]).content_regexp_list_comp[i].length(j);
            }
            newblock = new char[buffer_length + sizediff];
            srcoff = 0;
            dstpos = newblock;

            #ifdef DGDEBUG
                std::cout << "content matches:" << matches << std::endl;
            #endif

            for (j = 0; j < matches; j++) {
                nextoffset = (*o.fg[filtergroup]).content_regexp_list_comp[i].offset(j);
                if (nextoffset > srcoff) {
                    memcpy(dstpos, data + srcoff, nextoffset - srcoff);
                    dstpos += nextoffset - srcoff;
                    srcoff = nextoffset;
                }
                memcpy(dstpos, replacement.toCharArray(), replen);
                dstpos += replen;
                srcoff += (*o.fg[filtergroup]).content_regexp_list_comp[i].length(j);
            }
            if (srcoff < buffer_length) {
                memcpy(dstpos, data + srcoff,  buffer_length - srcoff);
            }
            delete[] data;
            data = newblock;
            buffer_length = buffer_length + sizediff;
            contentmodified = true;
        }
    }
    return contentmodified;
}

void DataBuffer::swapbacktocompressed() {
    if (compresseddata != NULL && compressed_buffer_length > 0) {
        delete[] data;
        buffer_length = compressed_buffer_length;
        data = compresseddata;
        compresseddata = NULL;
        compressed_buffer_length = 0;
    } // drop the decompressed version if there
}

int DataBuffer::bufferReadFromSocket(Socket *sock, char *buffer, int size, int sockettimeout, int timeout) {
    // a much more efficient reader that does not assume the contents of
    // the buffer gets filled thus reducing memcpy()ing and new()ing
    int pos = 0;
    int rc;
    struct timeval starttime;
    struct timeval nowadays;
    struct timezone notused;
    gettimeofday(&starttime, &notused);
    while(pos < size) {
        rc = sock->readFromSocket(&buffer[pos], size - pos, 0, sockettimeout);
        if (rc < 1) {
            // none recieved or an error
            if (pos > 0) {
                return pos;  // some was recieved previous into buffer
            }
            return rc;  // just return with the return code
        }
        pos += rc;
        gettimeofday(&nowadays, &notused);
        if (nowadays.tv_sec - starttime.tv_sec > timeout) {
            #ifdef DGDEBUG
                std::cout << "buffered socket read more than timeout" << std::endl;
            #endif
            return pos;  // just return how much got so far then
        }
    }
    return size;  // full buffer
}

int DataBuffer::bufferReadFromSocket(Socket *sock, char *buffer, int size, int sockettimeout) {
    // a much more efficient reader that does not assume the contents of
    // the buffer gets filled thus reducing memcpy()ing and new()ing
    int pos = 0;
    int rc;
    while(pos < size) {
        rc = sock->readFromSocket(&buffer[pos], size - pos, 0, sockettimeout);
        if (rc < 1) {
            // none recieved or an error
            if (pos > 0) {
                return pos;  // some was recieved previous into buffer
            }
            return rc;  // just return with the return code
        }
        pos += rc;
    }
    return size;  // full buffer
}

int DataBuffer::getTempFileFD() {
    if (tempfilefd > -1) {
        return tempfilefd;
    }
    tempfilepath = o.download_dir.c_str();
    tempfilepath += "/tfXXXXXX";
    char *tempfilepatharray = new char[tempfilepath.length() + 1];
    strcpy(tempfilepatharray, tempfilepath.toCharArray());
    if ((tempfilefd = mkstemp(tempfilepatharray)) < 0) {
        #ifdef DGDEBUG
            std::cerr << "error creating temp: " << tempfilepath << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Could not create temp file to store download for scanning.");
        tempfilefd = -1;
        tempfilepath = "";
    }
    else {
        tempfilepath = tempfilepatharray;
    }
    delete[] tempfilepatharray;
    return tempfilefd;
}

int DataBuffer::readEINTR(int fd, char *buf, unsigned int count) {
    int rc;
    errno=0;
    while (true) {  // using the while as a restart point with continue
        rc = ::read(fd, buf, count);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;  // was interupted by a signal so restart
            }
        }
        break;  // end the while
    }
    return rc;  // return status
}

int DataBuffer::writeEINTR(int fd, char *buf, unsigned int count) {
    int rc;
    errno=0;
    while (true) {  // using the while as a restart point with continue
        rc = write(fd, buf, count);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;  // was interupted by a signal so restart
            }
        }
        break;  // end the while
    }
    return rc;  // return status
}
