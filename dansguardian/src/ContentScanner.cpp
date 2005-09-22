#include "platform.h"
#include "ContentScanner.hpp"
#include "ConfigVar.hpp"
#include "OptionContainer.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <cerrno>

extern bool isDaemonised;
extern OptionContainer o;

#ifdef __CLAMD
extern cscreate_t clamdcreate;
extern csdestroy_t clamddestroy;
#endif

#ifdef __CLAMAV
extern cscreate_t clamavcreate;
extern csdestroy_t clamavdestroy;
#endif

#ifdef __ICAP
extern cscreate_t icapcreate;
extern csdestroy_t icapdestroy;
#endif

#ifdef __KAVAV
extern cscreate_t kavavcreate;
extern csdestroy_t kavavdestroy;
#endif

#ifdef __KAVD
extern cscreate_t kavdcreate;
extern csdestroy_t kavddestroy;
#endif

CSPlugin::CSPlugin( ConfigVar & definition )
{
#ifdef DGDEBUG
    std::cerr << "Default CS ctor called" << std::endl;
#endif
}

int CSPlugin::init(int dgversion) {
    if (!readStandardLists()) {  //always
        return DGCS_ERROR;       //include
    }                            // these
    return DGCS_OK;
}

int CSPlugin::reload(int dgversion) { //assumes init has been run sucessfully
    if (quit() == DGCS_OK) {
        return init(dgversion);
    }
    return DGCS_ERROR;
}

// returns FD in int and saves filename to String pointer
// filename is not used as input
int CSPlugin::makeTempFile(String *filename) {
    int tempfilefd;
    String tempfilepath = o.download_dir.c_str();
    tempfilepath += "/tfXXXXXX";
    char *tempfilepatharray = new char[tempfilepath.length()+1];
    strcpy(tempfilepatharray,tempfilepath.toCharArray());
    if ((tempfilefd = mkstemp(tempfilepatharray))<1) {
        #ifdef DGDEBUG
            std::cerr << "error creating cs temp " << tempfilepath <<": " << strerror(errno) << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Could not create cs temp file.");
        tempfilefd = -1;
    }
    else {
        (*filename) = tempfilepatharray;
    }
    delete[] tempfilepatharray;
    return tempfilefd;
}


int CSPlugin::writeMemoryTempFile(const char *object, unsigned int objectsize, String *filename) {
    int tempfd = makeTempFile(filename);  // String gets modified
    if (tempfd < 0) {
        #ifdef DGDEBUG
            std::cerr << "Error creating temp file in writeMemoryTempFile." << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Error creating temp file in writeMemoryTempFile.");
        return DGCS_ERROR;
    }
    errno = 0;
    #ifdef DGDEBUG
        std::cout << "About to writeMemoryTempFile " << (*filename) << " size: " << objectsize << std::endl;
    #endif

    while (true) {
        if (write(tempfd, object, objectsize) < 0) {
            if (errno == EINTR) {
                continue;  // was interupted by a signal so restart
            }
        }
        break;  // end the while
    }
    close(tempfd);  // finished writing so close file
    return DGCS_OK; // all ok
}

int CSPlugin::scanMemory(HTTPHeader *requestheader, HTTPHeader *docheader, const char* user, int filtergroup, const char* ip, const char *object, unsigned int objectsize) {
    // there is no capability to scan memory with some AV as we pass it
    // a file name to scan.  So we save the memory to disk and pass that.
    // Then delete the temp file.
    String tempfilepath;
    if (writeMemoryTempFile(object, objectsize, &tempfilepath) != DGCS_OK) {
        #ifdef DGDEBUG
            std::cerr << "Error creating/writing temp file for scanMemory." << std::endl;
        #endif
        syslog(LOG_ERR, "%s","Error creating/writing temp file for scanMemory.");
        return DGCS_SCANERROR;
    }
    int rc = scanFile(requestheader, docheader, user, filtergroup, ip, tempfilepath.toCharArray());
    unlink(tempfilepath.toCharArray());  // delete temp file
    return rc;
}

bool CSPlugin::readStandardLists() {
    exceptionvirusmimetypelist_location = cv["exceptionvirusmimetypelist"];
    exceptionvirusextensionlist_location = cv["exceptionvirusextensionlist"];
    exceptionvirussitelist_location = cv["exceptionvirussitelist"];
    exceptionvirusurllist_location = cv["exceptionvirusurllist"];
    exceptionvirusmimetypelist.reset();  // incase this is a reload
    exceptionvirusextensionlist.reset();
    exceptionvirussitelist.reset();
    exceptionvirusurllist.reset();
    if (!readListFile(&exceptionvirusmimetypelist_location, &exceptionvirusmimetypelist, false)) {
        if (!isDaemonised) {
            std::cerr << "Error opening exceptionvirusmimetypelist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening exceptionvirusmimetypelist");
        return false;
    }
    exceptionvirusmimetypelist.endsWithSort();
    if (!readListFile(&exceptionvirusextensionlist_location, &exceptionvirusextensionlist, false)) {
        if (!isDaemonised) {
            std::cerr << "Error opening exceptionvirusextensionlist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening exceptionvirusextensionlist");
        return false;
    }
    exceptionvirusextensionlist.endsWithSort();
    if (!readListFile(&exceptionvirussitelist_location, &exceptionvirussitelist, false)) {
        if (!isDaemonised) {
            std::cerr << "Error opening exceptionvirussitelist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening exceptionvirussitelist");
        return false;
    }
    exceptionvirussitelist.endsWithSort();
    if (!readListFile(&exceptionvirusurllist_location, &exceptionvirusurllist, false)) {
        if (!isDaemonised) {
            std::cerr << "Error opening exceptionvirusurllist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening exceptionvirusurllist");
        return false;
    }
    exceptionvirusurllist.startsWithSort();
    return true;
}

bool CSPlugin::readListFile(String *filename, ListContainer *list, bool startswith) {
    return list->readItemList(filename->toCharArray(), startswith, 0);
}


int CSPlugin::scanTest(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip) {
    String mimetype = docheader->getcontenttype();
    String disposition = docheader->disposition();
    String url = requestheader->url();
    String urld = requestheader->decode(url);
    urld.removeWhiteSpace();
    urld.toLower();
    urld.removePTP();
    String domain, tempurl, foundurl, path, extension;
    int fl;
    char *i;
    if (urld.contains("/")) {
        domain = urld.before("/");
        String path = "/";
        path += urld.after("/");
        urld = urld.before("/");
        path.hexDecode();
        path.realPath();
        urld += path;  // will resolve ../ and %2e2e/ and // etc
    }
    else {
        domain = urld;
    }

    if (disposition.length() > 2) {
        extension = disposition;
    }
    else {
        if (!path.contains("?")) {
            extension = path;
            while(extension.contains("/")) {
                extension = extension.after("/");
            }
        }
        else if (mimetype.contains("application/")) {
            extension = path;
            if (extension.contains("?")) {
                extension = extension.before("?");
            }
        }
    }

    // don't scan our web server
    if (domain.startsWith(o.ada.toCharArray())) {
        return DGCS_NOSCAN;
    }


    //exceptionvirusextensionlist
    if (extension.contains(".")) {
        i = exceptionvirusextensionlist.findEndsWith(extension.toCharArray());
        if (i != NULL) {
            return DGCS_NOSCAN;  // match
        }
    }


    //exceptionvirusmimetypelist
    i = exceptionvirusmimetypelist.findInList(mimetype.toCharArray());
    if (i != NULL) {
        return DGCS_NOSCAN;  // match
    }


    // exceptionvirussitelist
    tempurl = domain;
    while (tempurl.contains(".")) {
        i = exceptionvirussitelist.findInList(tempurl.toCharArray());
        if (i != NULL) {
            return DGCS_NOSCAN;  // exact match
        }
        tempurl = tempurl.after(".");  // check for being in higher level domains
    }
    if (tempurl.length() > 1) {  // allows matching of .tld
        tempurl = "." + tempurl;
        i = exceptionvirussitelist.findInList(tempurl.toCharArray());
        if (i != NULL) {
            return DGCS_NOSCAN;  // exact match
        }
    }


    // exceptionvirusurllist
    tempurl = url; // exceptionvirusurllist
    if (tempurl.endsWith("/")) {
        tempurl.chop();  // chop off trailing / if any
    }
    while (tempurl.before("/").contains(".")) {
        i = exceptionvirusurllist.findStartsWith(tempurl.toCharArray());
        if (i != NULL) {
            foundurl = i;
            fl = foundurl.length();
            if (tempurl.length() > fl) {
                unsigned char c = tempurl[fl];
                if (c == '/' || c == '?' || c == '&' || c == '=') {
                    return DGCS_NOSCAN; // matches /blah/ or /blah/foo but not /blahfoo
                }
            }
            else {
                return DGCS_NOSCAN;  // exact match
            }
        }
        tempurl = tempurl.after(".");  // check for being in higher level domains
    }

#ifdef DGDEBUG
    std::cout << "URL " << url << " is going to need AV scanning." << std::endl;
#endif

    return DGCS_NEEDSCAN;
}

int CSPlugin::readEINTR(int fd, char *buf, unsigned int count) {
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

int CSPlugin::writeEINTR(int fd, char *buf, unsigned int count) {
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


CSPluginLoader::CSPluginLoader( )
{
    create_it  = NULL;
    destroy_it = NULL;
    isGood = false;
}

CSPluginLoader::CSPluginLoader( const CSPluginLoader & a )
{
    create_it  = a.create_it;        // used to create said plugin
    destroy_it = a.destroy_it;        // to destroy (delete) it
    isGood     = a.isGood;
}

CSPluginLoader::~CSPluginLoader()
{
    return;
}

CSPluginLoader::CSPluginLoader( const char * pluginConfigPath )
{
    isGood = false;

    if (cv.readVar(pluginConfigPath, "=" ) > 0) {
        if (!isDaemonised) {
            std::cerr << "Unable to load plugin config: " << pluginConfigPath << std::endl;
        }
        syslog( LOG_ERR, "Unable to load plugin config %s\n", pluginConfigPath);
        return;
    }

    String plugname = cv["plugname"];

    if (plugname.length() < 1) {
        if (!isDaemonised) {
            std::cerr << "Unable read plugin config plugname variable: " << pluginConfigPath << std::endl;
        }
        syslog( LOG_ERR, "Unable read plugin config plugname variable %s\n", pluginConfigPath);
        return;
    }

#ifdef __CLAMD
    if (plugname == "clamdscan") {
#ifdef DGDEBUG
        std::cout << "Enabling ClamDscan CS plugin" << std::endl;
#endif
        create_it  = (cscreate_t*) clamdcreate;
        destroy_it = (csdestroy_t*) clamddestroy;
        isGood = true;
        return;
    }
#endif

#ifdef __CLAMAV
    if (plugname == "clamav") {
#ifdef DGDEBUG
        std::cout << "Enabling ClamAV CS plugin" << std::endl;
#endif
        create_it  = (cscreate_t*) clamavcreate;
        destroy_it = (csdestroy_t*) clamavdestroy;
        isGood = true;
        return;
    }
#endif

#ifdef __KAVAV
    if (plugname == "kavav") {
#ifdef DGDEBUG
        std::cout << "Enabling KAVClient CS plugin" << std::endl;
#endif
        create_it  = (cscreate_t*) kavavcreate;
        destroy_it = (csdestroy_t*) kavavdestroy;
        isGood = true;
        return;
    }
#endif

#ifdef __KAVD
    if (plugname == "kavdscan") {
#ifdef DGDEBUG
        std::cout << "Enabling KAVDscan CS plugin" << std::endl;
#endif
        create_it  = (cscreate_t*) kavdcreate;
        destroy_it = (csdestroy_t*) kavddestroy;
        isGood = true;
        return;
    }
#endif

#ifdef __ICAP
    if (plugname == "icapscan") {
#ifdef DGDEBUG
        std::cout << "Enabling ICAPscan CS plugin" << std::endl;
#endif
        create_it  = (cscreate_t*) icapcreate;
        destroy_it = (csdestroy_t*) icapdestroy;
        isGood = true;
        return;
    }
#endif

    create_it  = NULL;
    destroy_it = NULL;
    if (!isDaemonised) {
        std::cerr << "Unable to load plugin: " << pluginConfigPath << std::endl;
    }
    syslog( LOG_ERR, "Unable to load plugin %s\n", pluginConfigPath);
    return;
}


CSPlugin * CSPluginLoader::create()
{
    if ( create_it ){
        return create_it( cv );
    }
    return NULL;
}

void CSPluginLoader::destroy( CSPlugin * object )
{
    if ( object ){
        if ( destroy_it ){
            destroy_it( object );
        }
    }
    return;
}

