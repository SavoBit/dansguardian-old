#include "../DownloadManager.hpp"
#include "../String.hpp"
#include "../DataBuffer.hpp"
#include "../Socket.hpp"
#include "../HTTPHeader.hpp"
#include "../OptionContainer.hpp"
#include "../platform.h"
#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


class dminstance : public DMPlugin { // class name is irrelevent
public:
    dminstance( ConfigVar & definition );
    int in(OptionContainer *o, DataBuffer *d, Socket *sock, Socket *peersock, HTTPHeader *requestheader, HTTPHeader *docheader, bool wantall, int *headersent, bool *toobig);
// uncomment these if you wish to replace the default inherited functions
//    int init(int dgversion);
//    int quit(void);
//    add class variables for storage here
private:
     ConfigVar cv;
};


dminstance::dminstance( ConfigVar & definition ): DMPlugin( definition ) {
    cv = definition;
    return;
};

// class factory code *MUST* be included in every plugin

extern "C" DMPlugin* create( ConfigVar & definition ) {
    return new dminstance( definition ) ;
}

extern "C" void destroy(DMPlugin* p) {
    delete p;
}

// end of Class factory


// uncomment these if you wish to replace the default inherited functions
//int dminstance::init(int dgversion) {
//    return 0;
//}
//int dminstance::quit(void) {
//    return 0;
//}

// < 0 = error
// = 0 = ok
// > 0 = warning


int dminstance::in(OptionContainer *o, DataBuffer *d, Socket *sock, Socket *peersock, class HTTPHeader *requestheader, class HTTPHeader *docheader, bool wantall, int *headersent, bool *toobig) {

    //DataBuffer *d = where to stick the data back into
    //Socket *sock = where to read from
    //Socket *peersock = browser to send stuff to for keeping it alive
    //HTTPHeader *requestheader = header client used to request
    //HTTPHeader *docheader = header used for sending first line of reply
    //bool wantall = to determine if just content filter or a full scan
    //int *headersent = to use to send the first line of header if needed
    //                  or to mark the header has already been sent
    //bool *toobig = flag to modify to say if it could not all be downloaded

    #ifdef DGDEBUG
        std::cout << "Inside default download manager plugin" << std::endl;
    #endif

//  To access settings for the plugin use the following example:
//    std::cout << "cvtest:" << cv["dummy"] << std::endl;

    int rc, newsize;

    char* block;  // buffer for storing a grabbed block from the
                  // imput stream
    char* temp;

    bool swappedtodisk = false;

    struct timeval themdays;
    struct timeval nowadays;
    struct timezone notused;
    gettimeofday(&themdays, &notused);

    while(true) {
        // send x-header keep-alive here
        if (o->trickle_delay > 0) {
            gettimeofday(&nowadays, &notused);
            if (nowadays.tv_sec - themdays.tv_sec > o->trickle_delay) {
                themdays.tv_sec = nowadays.tv_sec;
                if ((*headersent) < 1) {
                    #ifdef DGDEBUG
                        std::cout << "sending first line of header first" << std::endl;
                    #endif
                    docheader->out(peersock, __DGHEADER_SENDFIRSTLINE);
                    (*headersent) = 1;
                }
                #ifdef DGDEBUG
                    std::cout << "trickle delay - sending X-DGKeepAlive: on" << std::endl;
                #endif
                peersock->writeString("X-DGKeepAlive: on\r\n");
            }
        }

        if (wantall) {
            if (!swappedtodisk) {
                if (d->buffer_length > o->max_content_ramcache_scan_size) {
                    if (d->buffer_length > o->max_content_filecache_scan_size) {
                        (*toobig) = true;
                        break;
                    }
                    else {
                        d->tempfilefd = d->getTempFileFD();
                        if (d->tempfilefd < 0) {
                            #ifdef DGDEBUG
                                std::cerr << "error buffering to disk so skipping disk buffering" << std::endl;
                            #endif
                            syslog(LOG_ERR, "%s","error buffering to disk so skipping disk buffering");

                            (*toobig) = true;
                            break;
                        }
                        d->writeEINTR(d->tempfilefd, d->data, d->buffer_length);
                        swappedtodisk = true;
                        d->tempfilesize = d->buffer_length;
                        #ifdef DGDEBUG
                            std::cout << "swapping to disk" << std::endl;
                        #endif
                    }

                }
            }
        }
        else {
            if (d->buffer_length > o->max_content_filter_size) {
                (*toobig) = true;
                break;
            }
        }

        if (d->buffer_length >= 32768) {
            newsize = d->buffer_length;
        }
        else {
            newsize = 32768;
        }

        if (!swappedtodisk) {
            block = new char[newsize];
            try {
                sock->checkForInput(d->timeout);
            }  catch (exception& e) {
                delete[] block;
                break;
            }
        // improved more efficient socket read which uses the buffer better
            rc = d->bufferReadFromSocket(sock, block, newsize, d->timeout);
            // grab a block of input, doubled each time

            if (rc < 1) {
                delete[] block;
                break;  // an error occured so end the while()
                    // or none received so pipe is closed
            }
            if (rc > 0) {
               temp = new char[d->buffer_length + rc];  // replacement store
               memcpy(temp, d->data, d->buffer_length);  // copy the current data
               memcpy(temp + d->buffer_length, block, rc);  // copy the new data
               delete[] d->data;  // delete the current data block
               d->data = temp;
               d->buffer_length += rc;  // update data size counter
            }
            delete[] block;
        }
        else {
            try {
                sock->checkForInput(d->timeout);
            }  catch (exception& e) {
                break;
            }
            rc = d->bufferReadFromSocket(sock, d->data, d->buffer_length, d->timeout);
            if (rc < 1) {
                break;
            }
            if (rc > 0) {
                lseek(d->tempfilefd, 0, SEEK_END);  // not really needed
                d->writeEINTR(d->tempfilefd, d->data, rc);
                d->tempfilesize += rc;
                #ifdef DGDEBUG
                    std::cout << "written to disk:" << rc << " total:" << d->tempfilesize << std::endl;
                #endif
            }
        }
    }

    if (!toobig && !swappedtodisk) {  // won't deflate stuff swapped to disk
        if (d->decompress.contains("deflate")) {
            #ifdef DGDEBUG
                std::cout << "zlib format" << std::endl;
            #endif
            d->zlibinflate(false);  // incoming stream was zlib compressed
        }
        else if (d->decompress.contains("gzip")) {
            #ifdef DGDEBUG
                std::cout << "gzip format" << std::endl;
            #endif
            d->zlibinflate(true);  // incoming stream was gzip compressed
        }
    }
    d->bytesalreadysent = 0;
    return 0;
}
