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
    int scanMemory(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char* object, unsigned int objectsize);
    int scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename);

    int init(int dgversion);
    int quit(void);

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

int instance::scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename) {
    lastvirusname = lastmessage = "";
    String command = "SCAN bPQRSTUW ";
    command += filename;
    command += "\r\n";
    #ifdef DGDEBUG
        std::cerr << "kavdscan command:" << command << std::endl;
    #endif
    UDSocket stripedsocks;
    if (stripedsocks.getFD() < 0) {
        syslog(LOG_ERR, "%s","Error creating kavdscan socket");
        return DGCS_SCANERROR;
    }
    if (stripedsocks.bind(udspath.toCharArray()) < 0) {
        syslog(LOG_ERR, "%s","Error binding to kavdscan socket");
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
