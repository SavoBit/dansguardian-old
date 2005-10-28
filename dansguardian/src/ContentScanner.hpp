// Defines the class interface to be implemented by ContentScanner plugins

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

#ifndef __HPP_CONTENTSCANNER
#define __HPP_CONTENTSCANNER


// INCLUDES
#include "platform.h"

#include "String.hpp"
#include "ConfigVar.hpp"
#include "DataBuffer.hpp"
#include "Socket.hpp"
#include "HTTPHeader.hpp"
#include "ListContainer.hpp"
#include "FDFuncs.hpp"
#include <stdexcept>


// DEFINES
#define DGCS_OK 0
#define DGCS_ERROR -1
#define DGCS_WARNING 1

#define DGCS_NOSCAN 0
#define DGCS_NEEDSCAN 1
#define DGCS_TESTERROR -1


#define DGCS_CLEAN 0
#define DGCS_SCANERROR -1
#define DGCS_INFECTED 1
#define DGCS_CURED 2  // not used


// DECLARATIONS

class CSPlugin;

// class factory functions for CS plugins
typedef CSPlugin* cscreate_t(ConfigVar &);
typedef void csdestroy_t(CSPlugin*);

// CS plugin interface proper - to be implemented by plugins themselves
class CSPlugin {
public:
	ConfigVar cv;
	String lastmessage;
	String lastvirusname;

	// empty constructor & constructor with CS plugin configuration passed in
	//CSPlugin() {};
	CSPlugin(ConfigVar &definition);

	virtual ~CSPlugin() {};
	
	// test whether a given request should be scanned or not, based on sent & received headers
	virtual int scanTest(HTTPHeader *requestheader, HTTPHeader *docheader, const char* user, int filtergroup, const char* ip);
	// scanning functions themselves
	virtual int scanMemory(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip,
		const char *object, unsigned int objectsize);
	virtual int scanFile(HTTPHeader *requestheader, HTTPHeader *docheader, const char *user, int filtergroup, const char *ip, const char* filename) = 0;

	virtual String getLastMessage() {return lastmessage;};
	virtual String getLastVirusName() {return lastvirusname;};

	// start, restart and stop the plugin
	virtual int init();
	virtual int reload();
	virtual int quit() {return DGCS_OK;};

private:
	// lists of all the various things we may not want to scan
	ListContainer exceptionvirusmimetypelist;
	ListContainer exceptionvirusextensionlist;
	ListContainer exceptionvirussitelist;
	ListContainer exceptionvirusurllist;

protected:
	// the following are unlikely to need to be overridden
	
	// read in scan exception lists
	virtual bool readStandardLists();
	// make & write to temp files, primarily for plugins with no direct memory scanning capability (e.g. clamdscan)
	virtual int makeTempFile(String *filename);
	virtual int writeMemoryTempFile(const char *object, unsigned int objectsize, String *filename);
};

// Class which takes in a plugin name and configuration path, and can build a configured instance of the correct CSPlugin descendent
class CSPluginLoader {
public:
	bool is_good;
	ConfigVar cv;

	CSPluginLoader();
	CSPluginLoader( const char *pluginConfigPath );
	// copy constructor
	CSPluginLoader( const CSPluginLoader &a );
	~CSPluginLoader();

	CSPlugin *create();
	void destroy( CSPlugin *object );

private:
	cscreate_t *create_it;  // used to create said plugin
	csdestroy_t *destroy_it;  // to destroy (delete) it
};

#endif
