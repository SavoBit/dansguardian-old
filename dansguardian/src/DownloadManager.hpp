#ifndef __HPP_DOWNLOADMANAGER
#define __HPP_DOWNLOADMANAGER

#include "String.hpp"
#include "ConfigVar.hpp"
#include "DataBuffer.hpp"
#include "Socket.hpp"
#include "HTTPHeader.hpp"
#include <stdexcept>

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

typedef DMPlugin& dmcreate_t(ConfigVar &);
typedef void dmdestroy_t(DMPlugin *);

class DMPluginLoader {
public:
    DMPluginLoader();
    DMPluginLoader( const char *pluginConfigPath );
    DMPluginLoader( const DMPluginLoader & a );
                    // copy constructor
    ~DMPluginLoader();
    ConfigVar cv;

    DMPlugin * create();
    void destroy( DMPlugin *object );
    bool isGood;

private:
    dmcreate_t *  create_it;        // used to create said plugin
    dmdestroy_t * destroy_it;        // to destroy (delete) it

};

#endif
