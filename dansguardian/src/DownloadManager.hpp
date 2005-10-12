//Defines the DMPlugin base class, and the DMPluginLoader, which arbitrates between the DM config files and the available DMPlugin descendents

#ifndef __HPP_DOWNLOADMANAGER
#define __HPP_DOWNLOADMANAGER


// INCLUDES

#include "String.hpp"
#include "ConfigVar.hpp"
#include "DataBuffer.hpp"
#include "Socket.hpp"
#include "HTTPHeader.hpp"

#include <stdexcept>


// DECLARATIONS

class DMPlugin;

// class factory functions for DM plugins
typedef DMPlugin & dmcreate_t(ConfigVar &);
typedef void dmdestroy_t(DMPlugin *);

// the DMPlugin interface - inherit & implement this to make download managers
class DMPlugin
{
public:
	DMPlugin() {};
	DMPlugin(ConfigVar & definition) {};
	virtual ~ DMPlugin() {};
	virtual int in(class OptionContainer *o, class DataBuffer *d, Socket *sock, Socket *peersock,
		class HTTPHeader *requestheader, class HTTPHeader *docheader, bool wantall, int *headersent, bool *toobig) = 0;
	virtual int init() { return 0; };
	virtual int quit() { return 0; };
};

// Class which takes in a plugin name and configuration path, and can build a configured instance of the correct DMPlugin descendent
class DMPluginLoader
{
public:
	ConfigVar cv;
	bool is_good;

	DMPluginLoader();
	// constructor with plugin configuration
	DMPluginLoader(const char *pluginConfigPath);
	// copy constructor
	DMPluginLoader(const DMPluginLoader &a);

	~DMPluginLoader() {};

	// create/destroy the DMPlugin itself
	DMPlugin *create();
	void destroy(DMPlugin *object);

private:
	dmcreate_t *create_it;  // used to create said plugin
	dmdestroy_t *destroy_it;  // to destroy (delete) it

};

#endif
