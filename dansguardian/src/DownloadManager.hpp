//Defines the DMPlugin base class, and the DMPluginLoader, which arbitrates between the DM config files and the available DMPlugin descendents

//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.

//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef __HPP_DOWNLOADMANAGER
#define __HPP_DOWNLOADMANAGER


// INCLUDES

#include "platform.h"

#include "String.hpp"
#include "ConfigVar.hpp"
#include "DataBuffer.hpp"
#include "Socket.hpp"
#include "HTTPHeader.hpp"
#include "ListContainer.hpp"

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
	DMPlugin(ConfigVar &definition);
	virtual ~ DMPlugin() {};
	
	// download the body for the given request
	virtual int in(DataBuffer *d, Socket *sock, Socket *peersock,
		HTTPHeader *requestheader, HTTPHeader *docheader, bool wantall, int *headersent, bool *toobig) = 0;
	
	// plugin initialise/quit routines.
	// if lastplugin is true, this is being loaded as the fallback option,
	// and needn't load in purely request matching related options.
	virtual int init(bool lastplugin);
	virtual int quit() { return 0; };

	// will this download manager handle this request?
	virtual bool willHandle(HTTPHeader *requestheader, HTTPHeader *docheader);

private:
	// regular expression for matching supported user agents
	RegExp ua_match;
	// if there isn't one, set this flag
	bool alwaysmatchua;

protected:
	// our configuration values
	// derived classes could definitely have a use for these
	ConfigVar cv;

	// standard lists
	ListContainer mimetypelist;
	ListContainer extensionlist;
	// .. and their enable flags
	bool mimelistenabled;
	bool extensionlistenabled;

	// read managedmimetypelist and managedextensionlist
	bool readStandardLists();
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
