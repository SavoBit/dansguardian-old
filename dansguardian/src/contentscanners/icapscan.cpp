#include "../ContentScanner.hpp"
#include "../String.hpp"
#include "../DataBuffer.hpp"
#include "../Socket.hpp"
#include "../UDSocket.hpp"
#include "../HTTPHeader.hpp"
#include "../OptionContainer.hpp"
#include "../platform.h"
#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>  // for gethostby

// dgav.sf.net and openantivirus.org were a great help in providing example
// code to show how to connect to an ICAP server

class icapinstance : public CSPlugin { // class name is irrelevent
public:
    icapinstance( ConfigVar & definition );

    int scanMemory(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char* object, unsigned int objectsize);
    int scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename);

    int init(int dgversion);

// add class variables for storage here
// non virtual objects
private:
     String icapurl;
     String icapip;
     unsigned int icapport;
     int doScan(Socket & icapsock);
     bool doHeaders(Socket& icapsock, unsigned int objectsize, const char* filename);
};

extern OptionContainer o;

// class factory code *MUST* be included in every plugin

icapinstance::icapinstance( ConfigVar & definition ): CSPlugin( definition ) {
    cv = definition;
    return;
};

CSPlugin* icapcreate( ConfigVar & definition ) {
    return new icapinstance( definition ) ;
}

void icapdestroy(CSPlugin* p) {
    delete p;
}

// end of Class factory


int icapinstance::init(int dgversion) {
    if (!readStandardLists()) {  //always
        return DGCS_ERROR;       //include
    }                            //these
    icapurl = cv["icapurl"];  // format: icap://icapserver:1344/avscan
    if (icapurl.length() < 3) {
        #ifdef DGDEBUG
            std::cerr << "Error reading icapurl option." << std::endl;
        #endif
        syslog(LOG_ERR, "%s", "Error reading icapurl option.");
        return DGCS_ERROR;
        // it would be far better to do a test connection
    }
    String icaphost = icapurl.after("//");
    icapport = icaphost.after(":").before("/").toInteger();
    icaphost = icaphost.before(":");
    struct hostent *host;
    if ((host = gethostbyname(icaphost.toCharArray())) == 0) {
        #ifdef DGDEBUG
            std::cerr << "Error resolving icap host address." << std::endl;
        #endif
        syslog(LOG_ERR, "%s", "Error resolving icap host address.");
        return DGCS_ERROR;
    }
    icapip = inet_ntoa(*(struct in_addr *) host->h_addr_list[0]);
//    char *h_addr_list = (*host->h_addr_list);

 //  icapip = h_addr_list[0];
    #ifdef DGDEBUG
        std::cerr << "ICAP server address:" << icapip << std::endl;
    #endif
    return DGCS_OK;
}



int icapinstance::scanMemory(HTTPHeader *requestheader, HTTPHeader *docheader, const char* user, int filtergroup, const char* ip, const char *object, unsigned int objectsize) {
    lastvirusname = lastmessage = "";

    String filename = requestheader->url();
    filename = filename.after("://").after("/");

    Socket icapsock;

	if (not doHeaders(icapsock,objectsize,filename.toCharArray())) {
		icapsock.close();
		return DGCS_SCANERROR;
	}

    #ifdef DGDEBUG
        std::cerr << "About to send memory data to icap" << std::endl;
    #endif
    try {
        if (!icapsock.writeToSocket((char*)object, objectsize, 0, 60)) {
                throw exception();
        }
        #ifdef DGDEBUG
            std::cout << "total sent to icap:" << objectsize << std::endl;
        #endif
        icapsock.writeString("\r\n0\r\n\r\n");  // end marker
        #ifdef DGDEBUG
            std::cout << "memory was sent to icap" << std::endl;
        #endif
    } catch (exception& e) {
        #ifdef DGDEBUG
            std::cerr << "Exception memory file to ICAP:" << e.what() << std::endl;
        #endif
        icapsock.close();
        lastmessage = "Exception sending memory file to ICAP";
        syslog(LOG_ERR, "Exception sending memory file to ICAP: %s",e.what());
        return DGCS_SCANERROR;
    }
    
    return doScan(icapsock);
}


int icapinstance::scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename) {
    lastmessage = lastvirusname = "";
    char filesizehex[32];
    int filefd = open(filename, O_RDONLY);
    if (filefd < 0) {
        #ifdef DGDEBUG
            std::cerr << "Error opening file (" << filename <<"): "<< strerror(errno) << std::endl;
        #endif
        lastmessage = "Error opening file to send to ICAP";
        syslog(LOG_ERR, "Error opening file to send to ICAP: %s",strerror(errno));
        return DGCS_SCANERROR;
    }
    lseek(filefd, 0, SEEK_SET);
    unsigned int filesize = lseek(filefd, 0, SEEK_END);

    Socket icapsock;
    if (not doHeaders(icapsock,filesize,filename)) {
    	icapsock.close();
    	close(filefd);
    	return DGCS_SCANERROR;
    }

    #ifdef DGDEBUG
        std::cerr << "About to send file data to icap" << std::endl;
    #endif
    lseek(filefd, 0, SEEK_SET);
    unsigned int sent = 0;
    char *data = new char[256 * 1024];  // 256k
    try {
        while (sent < filesize) {
            int rc = readEINTR(filefd, data, 256 * 1024);
            #ifdef DGDEBUG
                std::cout << "reading icap file rc:" << rc << std::endl;
            #endif
            if (rc < 0) {
                #ifdef DGDEBUG
                    std::cout << "error reading icap file so throwing exception" << std::endl;
                #endif
                throw exception();
            }
            if (rc == 0) {
                #ifdef DGDEBUG
                    std::cout << "got zero bytes reading icap file" << std::endl;
                #endif
                break;  // should never happen
            }
            if (!icapsock.writeToSocket(data, rc, 0, 60)) {
                throw exception();
            }
            sent += rc;
            #ifdef DGDEBUG
                std::cout << "total sent to icap:" << sent << std::endl;
            #endif
        }
        icapsock.writeString("\r\n0\r\n\r\n");  // end marker
        #ifdef DGDEBUG
            std::cout << "file was sent to icap" << std::endl;
        #endif
    } catch (exception& e) {
        #ifdef DGDEBUG
            std::cerr << "Exception sending file to ICAP:" << e.what() << std::endl;
        #endif
        lastmessage = "Exception sending file to ICAP";
        syslog(LOG_ERR, "Exception sending file to ICAP: %s",e.what());
        delete[] data;
        close(filefd);
        return DGCS_SCANERROR;
    }
    close(filefd);
    
    return doScan(icapsock);
}

int icapinstance::doScan(Socket& icapsock) {
    char *data = new char[8192];
    try {
        String line;
        icapsock.getline(data, 8192, o.content_scanner_timeout);
        line = data;
        #ifdef DGDEBUG
            std::cout << "reply from icap:" << line << std::endl;
        #endif
        // reply is of the format:
        // ICAP/1.0 204 No Content Necessary (etc)

        String returncode = line.after(" ").before(" ");

        if (returncode == "204") {
            #ifdef DGDEBUG
                std::cerr << "ICAP says clean!" << std::endl;
            #endif
            icapsock.close();
            delete[] data;
            return DGCS_CLEAN;
        }
        else if (returncode == "200") {
            #ifdef DGDEBUG
                std::cerr << "ICAP says INFECTED!" << std::endl;
            #endif
            while(icapsock.getline(data, 8192, o.content_scanner_timeout) > 0) {
                line = data;
                #ifdef DGDEBUG
                    std::cout << "more reply from icap:" << line << std::endl;
                #endif
                if (line.contains("X-Infection-Found")) {
                    lastvirusname = line.after("Threat=").before(";");
                    icapsock.close();
                    delete[] data;
                    return DGCS_INFECTED;
                }
                //NOTE: If the socket gets closed by the other end here, it is
                //possible for this marker to never be found. This could also be
                //caused by poorly implemented ICAP servers/DoS (latter unlikely
                //cos the ICAP server in DG config. would need hijacking).
                //The reason for this is that getline returns '\n' when
                //reading from a closed socket, and this is not sufficient for
                //breaking this loop. We may need to be more clever and actually
                //obey the ICAP body size and/or enforce limits.
                //See Socket::getline() in Socket.cpp
                else if (line.startsWith("0")) { // end marker
                    break;
                }
            }
        }
        else if (returncode == "404") {
            #ifdef DGDEBUG
                std::cerr << "ICAP says no such service!" << std::endl;
            #endif
            icapsock.close();
            lastmessage = "ICAP reports no such service";
            syslog(LOG_ERR,"ICAP reports no such service; check your server URL");
            delete[] data;
            return DGCS_SCANERROR;
        }
        else {
            #ifdef DGDEBUG
                std::cerr << "ICAP returned unrecognised response code: " << returncode << std::endl;
            #endif
            icapsock.close();
            lastmessage = "ICAP returned unrecognised response code.";
            syslog(LOG_ERR,"ICAP returned unrecognised response code: %s",returncode.toCharArray());
            delete[] data;
            return DGCS_SCANERROR;
        }
        delete[] data;
    } catch (exception& e) {
        #ifdef DGDEBUG
            std::cerr << "Exception getting reply from ICAP:" << e.what() << std::endl;
        #endif
        icapsock.close();
        lastmessage = "Exception getting reply from ICAP.";
        syslog(LOG_ERR, "Exception getting reply from ICAP: %s",e.what());
        delete[] data;
        return DGCS_SCANERROR;
    }
    // it is generally NOT a good idea, when using virus scanning,
    // to continue as if nothing went wrong by default!
    icapsock.close();
    return DGCS_SCANERROR;
}

bool icapinstance::doHeaders(Socket& icapsock, unsigned int objectsize, const char* filename) {
    int rc = icapsock.connect(icapip.toCharArray(), icapport);
    if (rc) {
        #ifdef DGDEBUG
            std::cerr << "Error connecting to ICAP server" << std::endl;
        #endif
        lastmessage = "Error connecting to ICAP server";
        syslog(LOG_ERR, "Error connecting to ICAP server");
        return false;
    }
    char objectsizehex[32];
    String encapsulatedheader = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + String((long)objectsize) + "\r\n\r\n";
    snprintf(objectsizehex, sizeof(objectsizehex), "%x\r\n", objectsize);
    String httpresponseheader = "GET ";
    httpresponseheader += filename;
    httpresponseheader += " HTTP/1.1\r\n\r\n";
    String icapheader = "RESPMOD " + icapurl + " ICAP/1.0\r\nAllow: 204\r\nEncapsulated: " + "req-hdr=0, res-hdr=" + String(httpresponseheader.length()) + ", res-body=" + String(httpresponseheader.length() + encapsulatedheader.length()) + "\r\n\r\n";

    #ifdef DGDEBUG
        std::cerr << "About to send icapheader:\n" << icapheader << httpresponseheader << encapsulatedheader << objectsizehex << std::endl;
    #endif
    try {
        icapsock.writeString(icapheader.toCharArray());
        icapsock.writeString(httpresponseheader.toCharArray());
        icapsock.writeString(encapsulatedheader.toCharArray());
        icapsock.writeString(objectsizehex);
    } catch (exception& e) {
        #ifdef DGDEBUG
            std::cerr << "Exception sending headers to ICAP:" << e.what() << std::endl;
        #endif
        lastmessage = "Exception sending headers to ICAP";
        syslog(LOG_ERR, "Exception sending headers to ICAP: %s",e.what());
        return false;
    }
    return true;
}
