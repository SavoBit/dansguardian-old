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

class csinstance : public CSPlugin { // class name is irrelevent
public:
    csinstance( ConfigVar & definition );

    int scanMemory(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char* object, unsigned int objectsize);
    int scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename);

    int init(int dgversion);

// add class variables for storage here
// non virtual objects
private:
     String icapurl;
     String icapip;
     unsigned int icapport;
};

extern OptionContainer o;

// class factory code *MUST* be included in every plugin

csinstance::csinstance( ConfigVar & definition ): CSPlugin( definition ) {
    cv = definition;
    return;
};

extern "C" CSPlugin* create( ConfigVar & definition ) {
    return new csinstance( definition ) ;
}

extern "C" void destroy(CSPlugin* p) {
    delete p;
}

// end of Class factory


int csinstance::init(int dgversion) {
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



int csinstance::scanMemory(HTTPHeader *requestheader, HTTPHeader *docheader, const char* user, int filtergroup, const char* ip, const char *object, unsigned int objectsize) {
    lastvirusname = lastmessage = "";

    String filename = requestheader->url();
    filename = filename.after("://").after("/");

    Socket icapsock;
    int rc = icapsock.connect(icapip.toCharArray(), icapport);
    if (rc) {
        #ifdef DGDEBUG
            std::cerr << "Error connecting to ICAP server" << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Error connecting to ICAP server");
        return DGCS_SCANERROR;
    }
    char filesizehex[32];
    String encapsulatedheader = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + String((long)objectsize) + "\r\n\r\n";
    snprintf(filesizehex, sizeof(filesizehex), "%x\r\n", objectsize);
    String httpresponseheader = "GET ";
    httpresponseheader += filename;
    httpresponseheader += " HTTP/1.1\r\n\r\n";
    String icapheader = "RESPMOD " + icapurl + " ICAP/1.0\r\nAllow: 204\r\nEncapsulated: " + "req-hdr=0, res-hdr=" + String(httpresponseheader.length()) + ", res-body=" + String(httpresponseheader.length() + encapsulatedheader.length()) + "\r\n\r\n";

    #ifdef DGDEBUG
        std::cerr << "About to send icapheader:\n" << icapheader << httpresponseheader << encapsulatedheader << filesizehex << std::endl;
    #endif
    try {
        icapsock.writeString(icapheader.toCharArray());
        icapsock.writeString(httpresponseheader.toCharArray());
        icapsock.writeString(encapsulatedheader.toCharArray());
        icapsock.writeString(filesizehex);
    } catch (exception& e) {
        #ifdef DGDEBUG
            std::cerr << "Exception sending headers to ICAP:" << e.what() << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Exception sending headers to ICAP.");
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
        syslog(LOG_ERR, "%s","Exception sending memory to ICAP.");
        return DGCS_SCANERROR;
    }
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
                    delete[] data;
                    return DGCS_INFECTED;
                }
                else if (line.startsWith("0")) { // end marker
                    break;
                }
            }
        }
        delete[] data;
    } catch (exception& e) {
        #ifdef DGDEBUG
            std::cerr << "Exception getting reply from ICAP:" << e.what() << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Exception getting reply from ICAP.");
        delete[] data;
        return DGCS_SCANERROR;
    }
    return DGCS_CLEAN;
}


int csinstance::scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename) {
    lastmessage = lastvirusname = "";

    Socket icapsock;
    int rc = icapsock.connect(icapip.toCharArray(), icapport);
    if (rc) {
        #ifdef DGDEBUG
            std::cerr << "Error connecting to ICAP server" << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Error connecting to ICAP server");
        return DGCS_SCANERROR;
    }

    char filesizehex[32];

    int filefd = open(filename, O_RDONLY);
    if (filefd < 0) {
        #ifdef DGDEBUG
            std::cerr << "Error opening file:" << filename << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Error opening file to send to icap");
        return DGCS_SCANERROR;
    }
    lseek(filefd, 0, SEEK_SET);
    unsigned int filesize = lseek(filefd, 0, SEEK_END);


    String encapsulatedheader = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + String((long)filesize) + "\r\n\r\n";
    snprintf(filesizehex, sizeof(filesizehex), "%x\r\n", filesize);
    String httpresponseheader = "GET ";
    httpresponseheader += filename;
    httpresponseheader += " HTTP/1.1\r\n\r\n";
    String icapheader = "RESPMOD " + icapurl + " ICAP/1.0\r\nAllow: 204\r\nEncapsulated: " + "req-hdr=0, res-hdr=" + String(httpresponseheader.length()) + ", res-body=" + String(httpresponseheader.length() + encapsulatedheader.length()) + "\r\n\r\n";

    #ifdef DGDEBUG
        std::cerr << "About to send icapheader:\n" << icapheader << httpresponseheader << encapsulatedheader << filesizehex << std::endl;
    #endif
    try {
        icapsock.writeString(icapheader.toCharArray());
        icapsock.writeString(httpresponseheader.toCharArray());
        icapsock.writeString(encapsulatedheader.toCharArray());
        icapsock.writeString(filesizehex);
    } catch (exception& e) {
        #ifdef DGDEBUG
            std::cerr << "Exception sending headers to ICAP:" << e.what() << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Exception sending headers to ICAP.");
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
            rc = readEINTR(filefd, data, 256 * 1024);
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
        syslog(LOG_ERR, "%s","Exception sending file to ICAP.");
        delete[] data;
        close(filefd);
        return DGCS_SCANERROR;
    }
    close(filefd);

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
                    delete[] data;
                    return DGCS_INFECTED;
                }
                else if (line.startsWith("0")) { // end marker
                    break;
                }
            }
        }
        delete[] data;
    } catch (exception& e) {
        #ifdef DGDEBUG
            std::cerr << "Exception getting reply from ICAP:" << e.what() << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Exception getting reply from ICAP.");
        delete[] data;
        return DGCS_SCANERROR;
    }
    return DGCS_CLEAN;
}
