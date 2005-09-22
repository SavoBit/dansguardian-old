//TODO: Replace error reporting with detailed entries in syslog(LOG_ERR), short entries in lastmessage.

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

class kavdinstance : public CSPlugin { // class name is irrelevent
public:
    kavdinstance( ConfigVar & definition );
    int scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename);

    int init(int dgversion);

private:
    String udspath;
};

extern OptionContainer o;

// class factory code *MUST* be included in every plugin

kavdinstance::kavdinstance( ConfigVar & definition ): CSPlugin( definition ) {
    cv = definition;
    return;
};

CSPlugin* kavdcreate( ConfigVar & definition ) {
    return new kavdinstance( definition ) ;
}

void kavddestroy(CSPlugin* p) {
    delete p;
}

// end of Class factory


int kavdinstance::init(int dgversion) {
    if (!readStandardLists()) {  //always
        return DGCS_ERROR;       //include
    }                            //these
    udspath = cv["kavdudsfile"];
    if (udspath.length() < 3) {
        #ifdef DGDEBUG
            std::cerr << "Error reading kavdudsfile option." << std::endl;
        #endif
        syslog(LOG_ERR, "%s", "Error reading kavdudsfile option.");
        return DGCS_ERROR;
        // it would be far better to do a test connection to the file but
        // could not be arsed for now
    }
    return DGCS_OK;
}


// no need to replace the inheritied scanMemory() which just calls scanFile()
// there is no capability to scan memory with kavdscan as we pass it
// a file name to scan.  So we save the memory to disk and pass that.
// Then delete the temp file.

int kavdinstance::scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename) {
    lastvirusname = lastmessage = "";
    // mkstemp seems to only set owner permissions, so our AV daemon won't be
    // able to read the file, unless it's running as the same user as us. that's
    // not usually very convenient. so instead, just allow group read on the
    // file, and tell users to make sure the daemongroup option is friendly to
    // the AV daemon's group membership.
    // chmod can error with EINTR, ignore this?
    if (chmod(filename,S_IRGRP) != 0) {
        syslog(LOG_ERR,"Could not change file ownership to give kavd read access: %s",strerror(errno));
        return DGCS_SCANERROR;
    };
    String command = "SCAN bPQRSTUW ";
    command += filename;
    command += "\r\n";
    #ifdef DGDEBUG
        std::cerr << "kavdscan command:" << command << std::endl;
    #endif
    UDSocket stripedsocks;
    if (stripedsocks.getFD() < 0) {
        syslog(LOG_ERR, "%s","Error creating socket for talking to kavdscan");
        return DGCS_SCANERROR;
    }
    if (stripedsocks.connect(udspath.toCharArray()) < 0) {
        syslog(LOG_ERR, "%s","Error connecting to kavdscan socket");
        stripedsocks.close();
        return DGCS_SCANERROR;
    }
    char *buff = new char[4096];
    memset(buff, 0, 4096);
    int rc;
    try {
        // read kaspersky kavdscan (AV Enging Server) - format: 2xx greeting
        rc = stripedsocks.getline(buff, 4096, o.content_scanner_timeout);
    } catch (exception& e) {}
    if (buff[0] != '2') {
        delete[] buff;
        stripedsocks.close();
        syslog(LOG_ERR, "%s","kavdscan did not return ok");
        return DGCS_SCANERROR;
    }
    try {
        stripedsocks.writeString(command.toCharArray());
    }
    catch (exception& e) {
        delete[] buff;
        stripedsocks.close();
        syslog(LOG_ERR, "%s","unable to write to kavdscan");
        return DGCS_SCANERROR;
    }
    try {
        rc = stripedsocks.getline(buff, 4096, o.content_scanner_timeout);
    } catch (exception& e) {
        delete[] buff;
        stripedsocks.close();
        syslog(LOG_ERR, "%s","Error reading kavdscan socket");
        return DGCS_SCANERROR;
    }
    String reply = buff;
    delete[] buff;
    reply.removeWhiteSpace();
    #ifdef DGDEBUG
        std::cout << "Got from kavdscan:" << reply << std::endl;
    #endif
    stripedsocks.close();
    if (reply[0] == '2') {  // clean
        #ifdef DGDEBUG
            std::cerr << "kavdscan - clean" << std::endl;
        #endif
        return DGCS_CLEAN;
    }
    if (reply.startsWith("322")) {  // infected
        lastvirusname = reply.after(" ").before(" ");
        // format: 322 nastyvirus blah
    }
    // must be an error then
    lastmessage = reply;
    return DGCS_SCANERROR;
}
