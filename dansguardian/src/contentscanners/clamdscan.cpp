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

class instance : public CSPlugin { // class name is irrelevent
public:
    instance( ConfigVar & definition );

// we are not replacing scanTest or scanMemory

    int scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename);

    int init(int dgversion);

// add class variables for storage here
// non virtual objects
private:
     String udspath;
};

extern OptionContainer o;

// class factory code *MUST* be included in every plugin

instance::instance( ConfigVar & definition ): CSPlugin( definition ) {
    cv = definition;
    return;
};

extern "C" CSPlugin* create( ConfigVar & definition ) {
    return new instance( definition ) ;
}

extern "C" void destroy(CSPlugin* p) {
    delete p;
}

// end of Class factory


int instance::init(int dgversion) {
    if (!readStandardLists()) {  //always
        return DGCS_ERROR;       //include
    }                            //these
    udspath = cv["clamdudsfile"];
    if (udspath.length() < 3) {
        #ifdef DGDEBUG
            std::cerr << "Error reading clamdudsfile option." << std::endl;
        #endif
        syslog(LOG_ERR, "%s", "Error reading clamdudsfile option.");
        return DGCS_ERROR;
        // it would be far better to do a test connection to the file but
        // could not be arsed for now
    }
    return DGCS_OK;
}


// no need to replace the inheritied scanMemory() which just calls scanFile()
// there is no capability to scan memory with clamdscan as we pass it
// a file name to scan.  So we save the memory to disk and pass that.
// Then delete the temp file.

int instance::scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename) {
    lastmessage = lastvirusname = "";
    String command = "CONTSCAN ";
    command += filename;
    command += "\r\n";
    #ifdef DGDEBUG
        std::cerr << "clamdscan command:" << command << std::endl;
    #endif
    UDSocket stripedsocks;
    if (stripedsocks.getFD() < 0) {
        syslog(LOG_ERR, "%s","Error creating clamdscan socket");
        return DGCS_SCANERROR;
    }
    if (stripedsocks.bind(udspath.toCharArray()) < 0) {
        syslog(LOG_ERR, "%s","Error binding to clamdscan socket");
        stripedsocks.close();
        return DGCS_SCANERROR;
    }
    stripedsocks.writeString(command.toCharArray());
    char *buff = new char[4096];
    int rc;
    try {
        rc = stripedsocks.getline(buff, 4096, o.content_scanner_timeout);
    } catch (exception& e) {
        delete[] buff;
        stripedsocks.close();
        syslog(LOG_ERR, "%s","Error reading clamdscan socket");
        return DGCS_SCANERROR;
    }
    String reply = buff;
    delete[] buff;
    reply.removeWhiteSpace();
    #ifdef DGDEBUG
        std::cout << "Got from clamdscan:" << reply << std::endl;
    #endif
    stripedsocks.close();
    if (reply.endsWith("ERROR")) {
        lastmessage = reply;
        return DGCS_SCANERROR;
    }
    else if (reply.endsWith("FOUND")) {
        lastvirusname = reply.after(": ").before(" FOUND");
        // format is:
        // /foo/path/file: foovirus FOUND
        #ifdef DGDEBUG
            std::cerr << "clamdscan INFECTED! with:" << lastvirusname << std::endl;
        #endif
        return DGCS_INFECTED;
    }
    // must be clean
    #ifdef DGDEBUG
        std::cerr << "clamdscan - he say yes (clean)" << std::endl;
    #endif
    return DGCS_CLEAN;
}
