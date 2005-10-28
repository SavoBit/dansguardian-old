// Implements DMPluginLoader - there is (currently) no direct DMPlugin base implementation, only descendents

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


// INCLUDES

#include "platform.h"

#include "DownloadManager.hpp"
#include "ConfigVar.hpp"
#include "OptionContainer.hpp"
#include "RegExp.hpp"

#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>


// GLOBALS

extern bool is_daemonised;

extern dmcreate_t defaultdmcreate;
extern dmdestroy_t defaultdmdestroy;

// find the class factory functions for any DM plugins we've been configured to build

#ifdef __FANCYDM
extern dmcreate_t fancydmcreate;
extern dmdestroy_t fancydmdestroy;
#endif


// IMPLEMENTATION

//
// DMPlugin
//

// constructor
DMPlugin::DMPlugin(ConfigVar &definition):cv(definition), alwaysmatchua(false)
{
}

// default initialisation procedure
int DMPlugin::init()
{
	// compile regex for matching supported user agents
	String r = cv["useragentregexp"];
	if (r.length() > 0) {
#ifdef DGDEBUG
		std::cout<<"useragent regexp: "<<r<<std::endl;
#endif
		ua_match.comp(r.toCharArray());
	} else {
		// no useragent regex? then default to .*
#ifdef DGDEBUG
		std::cout<<"no useragent regular expression; defaulting to .*"<<std::endl;
#endif
		alwaysmatchua = true;
	}
	if (!readStandardLists())
		return -1;
	return 0;
}

// default method for deciding whether we will handle a request
bool DMPlugin::willHandle(HTTPHeader *requestheader, HTTPHeader *docheader)
{
	// match user agent first (quick)
	if (!(alwaysmatchua || ua_match.match(requestheader->userAgent().toCharArray())))
		return false;
	
	// then check standard lists (mimetypes & extensions)

	// mimetypes
	String mimetype = "";
	if (mimelistenabled) {
		mimetype = docheader->getContentType();
#ifdef DGDEBUG
		std::cout<<"mimetype: "<<mimetype<<std::endl;
#endif
		if (mimetypelist.findInList(mimetype.toCharArray()) == NULL)
			return false;
	}
	
	if (extensionlistenabled) {
		// determine the extension
		String path = requestheader->decode(requestheader->url());
		path.removeWhiteSpace();
		path.toLower();
		path.removePTP();
		path = path.after("/");
		path.hexDecode();
		path.realPath();
		String disposition = docheader->disposition();
		String extension;
		if (disposition.length() > 2) {
			extension = disposition;
		} else {
			if (!path.contains("?")) {
				extension = path;
			}
			else {
				if (mimetype.length() == 0)
					mimetype = docheader->getContentType();
				if (mimetype.contains("application/")) {
					extension = path;
					if (extension.contains("?")) {
						extension = extension.before("?");
					}
				}
			}
		}
	#ifdef DGDEBUG
		std::cout<<"extension: "<<extension<<std::endl;
	#endif
		// check the extension list
		if (extension.contains(".") && (extensionlist.findEndsWith(extension.toCharArray()) == NULL))
				return false;
	}

	return true;
}

// read in all the lists of various things we wish to handle
bool DMPlugin::readStandardLists()
{
	mimetypelist.reset();  // incase this is a reload
	extensionlist.reset();

	String filename = cv["managedmimetypelist"];
	if (filename.length() > 0) {
		if (!mimetypelist.readItemList(filename.toCharArray(), false, 0)) {
			if (!is_daemonised) {
				std::cerr << "Error opening managedmimetypelist" << std::endl;
			}
			syslog(LOG_ERR, "Error opening managedmimetypelist");
			return false;
		}
		mimetypelist.endsWithSort();
		mimelistenabled = true;
	} else {
		mimelistenabled = false;
	}
	
	filename = cv["managedextensionlist"];
	if (filename.length() > 0) {
		if (!extensionlist.readItemList(filename.toCharArray(), false, 0)) {
			if (!is_daemonised) {
				std::cerr << "Error opening managedextensionlist" << std::endl;
			}
			syslog(LOG_ERR, "Error opening managedextensionlist");
			return false;
		}
		extensionlist.endsWithSort();
		extensionlistenabled = true;
	} else {
		extensionlistenabled = false;
	}
	
	return true;
}

//
// DMPluginLoader
//

// constructor
DMPluginLoader::DMPluginLoader()
{
	create_it = NULL;
	destroy_it = NULL;
	is_good = false;
}

// copy constructor
DMPluginLoader::DMPluginLoader(const DMPluginLoader & a)
{
	create_it = a.create_it;
	destroy_it = a.destroy_it;
	is_good = a.is_good;
}

// call the class factory create func for the found DM plugin, passing in its configuration
DMPlugin *DMPluginLoader::create()
{
#ifdef DGDEBUG
	std::cout << "Creating DM" << std::endl;
#endif
	if (create_it) {
		return &create_it(cv);
	}
	return NULL;
}

// same as above, but destroy the plugin, not create it
void DMPluginLoader::destroy(DMPlugin * object)
{
	if (object) {
		if (destroy_it) {
			destroy_it(object);
		}
	}
	return;
}

// take in a DM plugin configuration file, find the DMPlugin descendent matching the value of plugname, and store its class factory funcs for later use
DMPluginLoader::DMPluginLoader(const char *pluginConfigPath)
{
	is_good = false;

	if (cv.readVar(pluginConfigPath, "=") > 0) {
		if (!is_daemonised) {
			std::cerr << "Unable to load plugin config: " << pluginConfigPath << std::endl;
		}
		syslog(LOG_ERR, "Unable to load plugin config %s\n", pluginConfigPath);
		return;
	}

	String plugname = cv["plugname"];

	if (plugname.length() < 1) {
		if (!is_daemonised) {
			std::cerr << "Unable read plugin config plugname variable: " << pluginConfigPath << std::endl;
		}
		syslog(LOG_ERR, "Unable read plugin config plugname variable %s\n", pluginConfigPath);
		return;
	}

	if (plugname == "default") {
#ifdef DGDEBUG
		std::cout << "Enabling default DM plugin" << std::endl;
#endif
		create_it = (dmcreate_t *) defaultdmcreate;
		destroy_it = (dmdestroy_t *) defaultdmdestroy;
		is_good = true;
		return;
	}
#ifdef __FANCYDM
	if (plugname == "fancy") {
#ifdef DGDEBUG
		std::cout << "Enabling fancy DM plugin" << std::endl;
#endif
		create_it = (dmcreate_t *) fancydmcreate;
		destroy_it = (dmdestroy_t *) fancydmdestroy;
		is_good = true;
		return;
	}
#endif

	create_it = NULL;
	destroy_it = NULL;
	if (!is_daemonised) {
		std::cerr << "Unable to load plugin: " << plugname << std::endl;
	}
	syslog(LOG_ERR, "Unable to load plugin %s\n", plugname.toCharArray());
	return;
}
