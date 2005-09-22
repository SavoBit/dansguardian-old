#include "../ContentScanner.hpp"
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
#include <clamav.h>

class clamavinstance : public CSPlugin { // class name is irrelevent
public:
    clamavinstance( ConfigVar & definition );

    // we are replacing the inherited scanMemory as it has support for it
    int scanMemory(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char* object, unsigned int objectsize);
    int scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename);

    int scanTest(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip);

    int init(int dgversion);
    int quit(void);

private:
    int dorc(int rc, const char *vn);
    
    struct cl_node *root;
    struct cl_limits limits;

};

extern OptionContainer o;

// class factory code *MUST* be included in every plugin

clamavinstance::clamavinstance( ConfigVar & definition ): CSPlugin( definition ) {
    cv = definition;
    return;
};

CSPlugin* clamavcreate( ConfigVar & definition ) {
    return new clamavinstance( definition ) ;
}

void clamavdestroy(CSPlugin* p) {
    delete p;
}

// end of Class factory



int clamavinstance::init(int dgversion) {
    if (!readStandardLists()) {  //always
        return DGCS_ERROR;       //include
    }                            //these
    root = NULL;
    limits.maxfiles = cv["maxfiles"].toInteger();
    if (limits.maxfiles < 1) {
        limits.maxfiles = 1000;
    }
    limits.maxfilesize = o.max_content_filecache_scan_size + 1024*1024;
    limits.maxreclevel = cv["maxreclevel"].toInteger();
    if (limits.maxreclevel < 1) {
        limits.maxreclevel = 5;
    }
    limits.maxratio = cv["maxratio"].toInteger();
    if (limits.maxratio < 1) {
        limits.maxratio = 200;
    }
    #ifdef DGDEBUG
        std::cerr << "maxfiles:" << limits.maxfiles << " maxfilesize:" << limits.maxfilesize << " maxreclevel:" << limits.maxreclevel << " maxratio:" << limits.maxratio << std::endl;
    #endif
    unsigned int virnum = 0;
    int rc = cl_loaddbdir(cl_retdbdir(), &root, &virnum);
    #ifdef DGDEBUG
        std::cout << "root: " << root << " virnum: " << virnum << std::endl;
    #endif
    if (rc != 0) {
        #ifdef DGDEBUG
            std::cerr << "Error loading clamav db:" << cl_strerror(rc) << std::endl;
        #endif
        syslog(LOG_ERR, "%s", "Error loading clamav db");
        syslog(LOG_ERR, "%s", cl_strerror(rc));
        return DGCS_ERROR;
    }
    rc = cl_build(root);
    if (rc != 0) {
        #ifdef DGDEBUG
            std::cerr << "Error building clamav db:" << cl_strerror(rc) << std::endl;
        #endif
        syslog(LOG_ERR, "%s", "Error building clamav db");
        syslog(LOG_ERR, "%s", cl_strerror(rc));
        return DGCS_ERROR;
    }
    return DGCS_OK;
}

int clamavinstance::quit(void) {
    cl_free(root);
    return DGCS_OK;
}
// < 0 = error
// = 0 = ok
// > 0 = warning


int clamavinstance::dorc(int rc, const char *vn) {
    if (rc == CL_VIRUS) {
        lastvirusname = vn;
        #ifdef DGDEBUG
            std::cerr << "INFECTED! with:" << lastvirusname << std::endl;
        #endif
        return DGCS_INFECTED;
    }
    else if (rc != CL_CLEAN) {
        lastmessage = cl_strerror(rc);
        #ifdef DGDEBUG
            std::cerr << "ClamAV error:" << lastmessage << std::endl;
        #endif
        return DGCS_SCANERROR;
    }
    #ifdef DGDEBUG
        std::cerr << "ClamAV - he say yes (clean)" << std::endl;
    #endif
    return DGCS_CLEAN;
}

int clamavinstance::scanMemory(HTTPHeader *requestheader, HTTPHeader *docheader, const char* user, int filtergroup, const char* ip, const char *object, unsigned int objectsize) {
    lastmessage = lastvirusname = "";
    const char *vn = "";
    int rc = cl_scanbuff(object, objectsize-1, &vn, root);
    return dorc(rc, vn);
}
//Return values:
//0 = object clean
//1 = object infected
//2 = object cured - not used
//-1 = error (DG will assume ok)

int clamavinstance::scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *filename) {
    lastmessage = lastvirusname = "";
    const char *vn = "";
    int rc = cl_scanfile(filename, &vn, NULL, root, &limits, CL_SCAN_STDOPT /*CL_ARCHIVE | CL_OLE2 | CL_MAIL | CL_OLE2 | CL_SCAN_PE | CL_SCAN_BLOCKBROKEN | CL_SCAN_HTML*/);
    return dorc(rc, vn);
}
//Return values:
//0 = object clean
//1 = object infected
//2 = object cured - not used
//-1 = error (DG will assume ok)

int clamavinstance::scanTest(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip) {
    return CSPlugin::scanTest(requestheader, docheader, user, filtergroup, ip);
}
