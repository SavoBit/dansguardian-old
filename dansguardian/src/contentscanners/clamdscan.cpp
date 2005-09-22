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
#include <sys/stat.h>
#include <unistd.h>

class clamdinstance : public CSPlugin { // class name is irrelevent
public:
    clamdinstance( ConfigVar & definition );

// we are not replacing scanTest or scanMemory

    int scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename);

    int init(int dgversion);

// add class variables for storage here
// non virtual objects
private:
     String udspath;
};

extern OptionContainer o;

clamdinstance::clamdinstance( ConfigVar & definition ): CSPlugin( definition ) {
    cv = definition;
    return;
};

// class factory code *MUST* be included in every plugin

CSPlugin* clamdcreate( ConfigVar & definition ) {
#ifdef DGDEBUG
        std::cout << "Creating ClamD CS plugin" << std::endl;
#endif
    return new clamdinstance( definition ) ;
}

void clamddestroy(CSPlugin* p) {
#ifdef DGDEBUG
        std::cout << "Destroying ClamD CS plugin" << std::endl;
#endif
    delete p;
}

// end of Class factory


int clamdinstance::init(int dgversion) {
    if (!readStandardLists()) {  //always
        return DGCS_ERROR;       //include
    }                            //these
    udspath = cv["clamdudsfile"];
    if (udspath.length() < 3) {
        #ifdef DGDEBUG
            std::cerr << "Error reading clamdudsfile option." << std::endl;
        #endif
        syslog(LOG_ERR, "Error reading clamdudsfile option.");
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

int clamdinstance::scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename) {
    lastmessage = lastvirusname = "";
    // mkstemp seems to only set owner permissions, so our AV daemon won't be
    // able to read the file, unless it's running as the same user as us. that's
    // not usually very convenient. so instead, just allow group read on the
    // file, and tell users to make sure the daemongroup option is friendly to
    // the AV daemon's group membership.
    // TODO? chmod can error out with EINTR, we may wish to ignore this
    if (chmod(filename,S_IRGRP) != 0) {
        lastmessage = "Error giving ClamD read access to temp file";
        syslog(LOG_ERR,"Could not change file ownership to give ClamD read access: %s",strerror(errno));
        return DGCS_SCANERROR;
    };
    String command = "CONTSCAN ";
    command += filename;
    command += "\r\n";
    #ifdef DGDEBUG
        std::cerr << "clamdscan command:" << command << std::endl;
    #endif
    UDSocket stripedsocks;
    if (stripedsocks.getFD() < 0) {
        lastmessage = "Error opening socket to talk to ClamD";
        syslog(LOG_ERR, "Error creating socket for talking to ClamD");
        return DGCS_SCANERROR;
    }
    if (stripedsocks.connect(udspath.toCharArray()) < 0) {
        lastmessage = "Error connecting to ClamD socket";
        syslog(LOG_ERR, "Error connecting to ClamD socket");
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
        lastmessage = "Exception whist reading ClamD socket";
        syslog(LOG_ERR, "Exception whilst reading ClamD socket: %s",e.what());
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
    // Note: we should really check what the output of a "clean" message actually looks like,
    // and check explicitly for that, but the ClamD documentation is sparse on output formats.
    #ifdef DGDEBUG
        std::cerr << "clamdscan - he say yes (clean)" << std::endl;
    #endif
    return DGCS_CLEAN;
}
