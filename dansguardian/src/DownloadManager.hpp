#ifndef __HPP_DOWNLOADMANAGER
#define __HPP_DOWNLOADMANAGER

#include "String.hpp"
#include "ConfigVar.hpp"
#include "DataBuffer.hpp"
#include "Socket.hpp"
#include "HTTPHeader.hpp"
#include <stdexcept>
#include <ltdl.h>

class DMPlugin;
class DMPluginLoader;

class DMPlugin {
protected:
public:
    DMPlugin() {};
    DMPlugin( ConfigVar & definition );
    virtual ~DMPlugin() {};
    virtual int in(class OptionContainer *o, class DataBuffer *d, Socket *sock, Socket *peersock, class HTTPHeader *requestheader, class HTTPHeader *docheader, bool wantall, int *headersent, bool *toobig) = 0;
//    virtual int in(void) = 0;
    virtual int init(int dgversion) {return 0;};
    virtual int quit(void) {return 0;};
};

typedef DMPlugin* dmcreate_t(ConfigVar &);
typedef void dmdestroy_t(DMPlugin*);

class DMPluginLoader {
public:
    DMPluginLoader() throw(std::runtime_error);
    DMPluginLoader( const char *pluginConfigPath ) throw(std::runtime_error);
    DMPluginLoader( const DMPluginLoader & a ) throw(std::runtime_error);
                    // copy constructor
    ~DMPluginLoader();
    ConfigVar cv;

    DMPlugin * create();
    void destroy( DMPlugin *object );
    bool isGood;

private:
    lt_dlhandle handle;            // the handle of the module it's in
    dmcreate_t *  create_it;        // used to create said plugin
    dmdestroy_t * destroy_it;        // to destroy (delete) it
//    std::string pluginname;        // the name of the plugin

//    void setname( std::string pluginName );

};

#endif
