#ifndef __HPP_CONTENTSCANNER
#define __HPP_CONTENTSCANNER

#define DGCS_OK 0
#define DGCS_ERROR -1
#define DGCS_WARNING 1

#define DGCS_NOSCAN 0
#define DGCS_NEEDSCAN 1
#define DGCS_TESTERROR -1


#define DGCS_CLEAN 0
#define DGCS_SCANERROR -1
#define DGCS_INFECTED 1
#define DGCS_CURED 2 // mot used

#include "String.hpp"
#include "ConfigVar.hpp"
#include "DataBuffer.hpp"
#include "Socket.hpp"
#include "HTTPHeader.hpp"
#include "ListContainer.hpp"
#include <stdexcept>
#include <ltdl.h>

class CSPlugin;
class CSPluginLoader;

class CSPlugin {
protected:
public:
    CSPlugin() {};
    CSPlugin( ConfigVar & definition );
    virtual ~CSPlugin(){ };
    virtual int scanTest(HTTPHeader *requestheader, HTTPHeader *docheader, const char* user, int filtergroup, const char* ip);
    virtual int scanMemory(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char *object, unsigned int objectsize);
    virtual int scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char* filename) = 0;

    virtual String getLastMessage() {return lastmessage;};
    virtual String getLastVirusName() {return lastvirusname;};

    virtual int init(int dgversion);
    virtual int reload(int dgversion);
    virtual int quit(void) {return DGCS_OK;};

    // these are unlikely to need to be overwridden
    virtual bool readStandardLists();
    virtual bool readListFile(String *filename, ListContainer *list, bool startswith);
    virtual int makeTempFile(String *filename);
    virtual int writeMemoryTempFile(const char *object, unsigned int objectsize, String *filename);
    virtual int readEINTR(int fd, char *buf, unsigned int count);
    virtual int writeEINTR(int fd, char *buf, unsigned int count);

    ConfigVar cv;
    ListContainer exceptionvirusmimetypelist;
    ListContainer exceptionvirusextensionlist;
    ListContainer exceptionvirussitelist;
    ListContainer exceptionvirusurllist;
    String exceptionvirusmimetypelist_location;
    String exceptionvirusextensionlist_location;
    String exceptionvirussitelist_location;
    String exceptionvirusurllist_location;
    String lastmessage;
    String lastvirusname;

};

typedef CSPlugin* cscreate_t(ConfigVar &);
typedef void csdestroy_t(CSPlugin*);

class CSPluginLoader {
public:
    CSPluginLoader() throw(std::runtime_error);
    CSPluginLoader( const char *pluginConfigPath ) throw(std::runtime_error);
    CSPluginLoader( const CSPluginLoader & a ) throw(std::runtime_error);
                    // copy constructor
    ~CSPluginLoader();
    ConfigVar cv;

    CSPlugin * create();
    void destroy( CSPlugin *object );
    bool isGood;

private:
    lt_dlhandle handle;            // the handle of the module it's in
    cscreate_t *  create_it;        // used to create said plugin
    csdestroy_t * destroy_it;        // to destroy (delete) it
//    std::string pluginname;        // the name of the plugin

//    void setname( std::string pluginName );

};

#endif
