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
#include <unistd.h>
#include <kavclient.h>

class csinstance : public CSPlugin { // class name is irrelevent
public:
    csinstance( ConfigVar & definition );
    int scanMemory(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char* object, unsigned int objectsize);
    int scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename);

    int init(int dgversion);
    int quit(void);

private:
     String udspath;
     kav_ctx kavcon;
     int dorc(int rc);
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
    kavcon = kav_new();
    if (kavcon == NULL) {
        #ifdef DGDEBUG
            std::cout << "kav_new() error" << std::endl;
            syslog(LOG_ERR, "%s","kav_new() error");
        #endif
        return DGCS_ERROR;
    }
    udspath = cv["kavudsfile"];
    if (udspath.length() < 3 || udspath == "default") {
        #ifdef DGDEBUG
            std::cout << "kavudsfile - using default option." << std::endl;
        #endif
        // it would be far better to do a test connection to the file but
        // could not be arsed for now
        kav_set_socketpath(kavcon, NULL);
    }
    else {
        kav_set_socketpath(kavcon, udspath.toCharArray());
    }
    kav_set_timeout(kavcon, o.content_scanner_timeout);
    return DGCS_OK;
}

int csinstance::quit(void) {
    if (kavcon != NULL) {
        kav_free(kavcon);  // return to kavcon 5 ;-)
    }
    return DGCS_OK;
}

int csinstance::dorc(int rc) {
    switch(rc) {
        case KAV_STATUS_CLEAN:
            #ifdef DGDEBUG
                std::cerr << "KAV - he say yes (clean)" << std::endl;
            #endif
            return DGCS_CLEAN;
            break;  // not needed
        case KAV_STATUS_CURED:
        case KAV_STATUS_INFECTED_DELETED:
        case KAV_STATUS_VIRUSES_FOUND:
        case KAV_STATUS_CORRUPTED_VIRUSES_FOUND:
        case KAV_STATUS_SUSPICIOUS_FOUND:
            #ifdef DGDEBUG
                std::cerr << "INFECTED!" << std::endl;
            #endif
            return DGCS_INFECTED;
            break;  // not needed
        default:
            #ifdef DGDEBUG
                std::cerr << "KAV error" << std::endl;
            #endif
    }
    return DGCS_SCANERROR;
}


int csinstance::scanMemory(HTTPHeader *requestheader, HTTPHeader *docheader, const char* user, int filtergroup, const char* ip, const char *object, unsigned int objectsize) {
    lastvirusname = lastmessage = "";
    if (kav_check_mem(kavcon, object, objectsize) != 0) {
        lastmessage = kav_strerror(kav_get_error(kavcon));
        return DGCS_SCANERROR;
    }
    lastvirusname = kav_result_get_report(kavcon);
    return dorc(kav_result_get_status(kavcon));
}


int csinstance::scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename) {
    lastvirusname = lastmessage = "";
    if (kav_check_file(kavcon, filename) != 0) {
        lastmessage = kav_strerror(kav_get_error(kavcon));
        return DGCS_SCANERROR;
    }
    lastvirusname = kav_result_get_report(kavcon);
    return dorc(kav_result_get_status(kavcon));
}
