// Implements CSPlugin and CSPluginLoader classes

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


// GLOBALS
extern bool is_daemonised;
extern OptionContainer o;

// find the class factory functions for the CS plugins we've been configured to build

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


// IMPLEMENTATION

// CSPlugin class

CSPlugin::CSPlugin(ConfigVar & definition)
{
	cv = definition;
}

// start the plugin - i.e. read in the configuration
int CSPlugin::init()
{
	if (!readStandardLists()) {	//always
		return DGCS_ERROR;  //include
	}			// these
	return DGCS_OK;
}

// reload the configuration
int CSPlugin::reload()
{				//assumes init has been run sucessfully
	if (quit() == DGCS_OK) {
		return init();
	}
	return DGCS_ERROR;
}

// make a temporary file for storing data which is to be scanned
// returns FD in int and saves filename to String pointer
// filename is not used as input
int CSPlugin::makeTempFile(String * filename)
{
	int tempfilefd;
	String tempfilepath = o.download_dir.c_str();
	tempfilepath += "/tfXXXXXX";
	char *tempfilepatharray = new char[tempfilepath.length() + 1];
	strcpy(tempfilepatharray, tempfilepath.toCharArray());
	if ((tempfilefd = mkstemp(tempfilepatharray)) < 1) {
#ifdef DGDEBUG
		std::cerr << "error creating cs temp " << tempfilepath << ": " << strerror(errno) << std::endl;
#endif
		syslog(LOG_ERR, "%s", "Could not create cs temp file.");
		tempfilefd = -1;
	} else {
		(*filename) = tempfilepatharray;
	}
	delete[]tempfilepatharray;
	return tempfilefd;
}

// write a temporary file containing the memory buffer which is to be scanned
// if your CS plugin does not have the ability to scan memory directly (e.g. clamdscan), this gets used by the default scanMemory to turn it into a file
int CSPlugin::writeMemoryTempFile(const char *object, unsigned int objectsize, String * filename)
{
	int tempfd = makeTempFile(filename);  // String gets modified
	if (tempfd < 0) {
#ifdef DGDEBUG
		std::cerr << "Error creating temp file in writeMemoryTempFile." << std::endl;
#endif
		syslog(LOG_ERR, "%s", "Error creating temp file in writeMemoryTempFile.");
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
	return DGCS_OK;  // all ok
}

// default implementation of scanMemory, which defers to scanFile.
int CSPlugin::scanMemory(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user, int filtergroup,
	const char *ip, const char *object, unsigned int objectsize)
{
	// there is no capability to scan memory with some AV as we pass it
	// a file name to scan.  So we save the memory to disk and pass that.
	// Then delete the temp file.
	String tempfilepath;
	if (writeMemoryTempFile(object, objectsize, &tempfilepath) != DGCS_OK) {
#ifdef DGDEBUG
		std::cerr << "Error creating/writing temp file for scanMemory." << std::endl;
#endif
		syslog(LOG_ERR, "%s", "Error creating/writing temp file for scanMemory.");
		return DGCS_SCANERROR;
	}
	int rc = scanFile(requestheader, docheader, user, filtergroup, ip, tempfilepath.toCharArray());
	unlink(tempfilepath.toCharArray());  // delete temp file
	return rc;
}

// read in all the lists of various things we do not wish to scan
bool CSPlugin::readStandardLists()
{
	exceptionvirusmimetypelist.reset();  // incase this is a reload
	exceptionvirusextensionlist.reset();
	exceptionvirussitelist.reset();
	exceptionvirusurllist.reset();
	if (!exceptionvirusmimetypelist.readItemList(cv["exceptionvirusmimetypelist"].toCharArray(), false, 0)) {
		if (!is_daemonised) {
			std::cerr << "Error opening exceptionvirusmimetypelist" << std::endl;
		}
		syslog(LOG_ERR, "%s", "Error opening exceptionvirusmimetypelist");
		return false;
	}
	exceptionvirusmimetypelist.endsWithSort();
	if (!exceptionvirusextensionlist.readItemList(cv["exceptionvirusextensionlist"].toCharArray(), false, 0)) {
		if (!is_daemonised) {
			std::cerr << "Error opening exceptionvirusextensionlist" << std::endl;
		}
		syslog(LOG_ERR, "%s", "Error opening exceptionvirusextensionlist");
		return false;
	}
	exceptionvirusextensionlist.endsWithSort();
	if (!exceptionvirussitelist.readItemList(cv["exceptionvirussitelist"].toCharArray(), false, 0)) {
		if (!is_daemonised) {
			std::cerr << "Error opening exceptionvirussitelist" << std::endl;
		}
		syslog(LOG_ERR, "%s", "Error opening exceptionvirussitelist");
		return false;
	}
	exceptionvirussitelist.endsWithSort();
	if (!exceptionvirusurllist.readItemList(cv["exceptionvirusurllist"].toCharArray(), true, 0)) {
		if (!is_daemonised) {
			std::cerr << "Error opening exceptionvirusurllist" << std::endl;
		}
		syslog(LOG_ERR, "%s", "Error opening exceptionvirusurllist");
		return false;
	}
	exceptionvirusurllist.startsWithSort();
	return true;
}

// test whether or not a request should be scanned based on sent & received headers
int CSPlugin::scanTest(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user, int filtergroup, const char *ip)
{
	char *i;

	//exceptionvirusmimetypelist
	String mimetype = docheader->getContentType();
	i = exceptionvirusmimetypelist.findInList(mimetype.toCharArray());
	if (i != NULL) {
		return DGCS_NOSCAN;  // match
	}

	String disposition = docheader->disposition();
	String url = requestheader->url();
	String urld = requestheader->decode(url);
	urld.removeWhiteSpace();
	urld.toLower();
	urld.removePTP();
	String domain, tempurl, foundurl, path, extension;
	int fl;
	if (urld.contains("/")) {
		domain = urld.before("/");
		String path = "/";
		path += urld.after("/");
		path.hexDecode();
		path.realPath();
	} else {
		domain = urld;
	}

	// don't scan our web server
	if (domain.startsWith(o.ada.toCharArray())) {
		return DGCS_NOSCAN;
	}

	//exceptionvirusextensionlist
	if (disposition.length() > 2) {
		extension = disposition;
	} else {
		if (!path.contains("?")) {
			extension = path;
		}
		else if (mimetype.contains("application/")) {
			extension = path;
			if (extension.contains("?")) {
				extension = extension.before("?");
			}
		}
	}
	if (extension.contains(".")) {
		i = exceptionvirusextensionlist.findEndsWith(extension.toCharArray());
		if (i != NULL) {
			return DGCS_NOSCAN;  // match
		}
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
	if (tempurl.length() > 1) {	// allows matching of .tld
		tmepurl = "." + tempurl;
		i = exceptionvirussitelist.findInList(tempurl.toCharArray());
		if (i != NULL) {
			return DGCS_NOSCAN;  // exact match
		}
	}

	// exceptionvirusurllist
	tempurl = domain + path;
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
					return DGCS_NOSCAN;  // matches /blah/ or /blah/foo but not /blahfoo
				}
			} else {
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

// CSPluginLoader class

CSPluginLoader::CSPluginLoader()
{
	create_it = NULL;
	destroy_it = NULL;
	is_good = false;
}

// copy constructor
CSPluginLoader::CSPluginLoader(const CSPluginLoader & a)
{
	create_it = a.create_it;  // used to create said plugin
	destroy_it = a.destroy_it;  // to destroy (delete) it
	is_good = a.is_good;
}

CSPluginLoader::~CSPluginLoader()
{
	return;
}

// call the class factory create func of the found CS plugin
CSPlugin *CSPluginLoader::create()
{
	if (create_it) {
		return create_it(cv);
	}
	return NULL;
}

// call the class factory destroy func of the found CS plugin
void CSPluginLoader::destroy(CSPlugin * object)
{
	if (object) {
		if (destroy_it) {
			destroy_it(object);
		}
	}
	return;
}

// take in a configuration file, find the CSPlugin class associated with the plugname variable, and look up its class factory funcs
CSPluginLoader::CSPluginLoader(const char *pluginConfigPath)
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

#ifdef __CLAMD
	if (plugname == "clamdscan") {
#ifdef DGDEBUG
		std::cout << "Enabling ClamDscan CS plugin" << std::endl;
#endif
		create_it = (cscreate_t *) clamdcreate;
		destroy_it = (csdestroy_t *) clamddestroy;
		is_good = true;
		return;
	}
#endif

#ifdef __CLAMAV
	if (plugname == "clamav") {
#ifdef DGDEBUG
		std::cout << "Enabling ClamAV CS plugin" << std::endl;
#endif
		create_it = (cscreate_t *) clamavcreate;
		destroy_it = (csdestroy_t *) clamavdestroy;
		is_good = true;
		return;
	}
#endif

#ifdef __KAVAV
	if (plugname == "kavav") {
#ifdef DGDEBUG
		std::cout << "Enabling KAVClient CS plugin" << std::endl;
#endif
		create_it = (cscreate_t *) kavavcreate;
		destroy_it = (csdestroy_t *) kavavdestroy;
		is_good = true;
		return;
	}
#endif

#ifdef __KAVD
	if (plugname == "kavdscan") {
#ifdef DGDEBUG
		std::cout << "Enabling KAVDscan CS plugin" << std::endl;
#endif
		create_it = (cscreate_t *) kavdcreate;
		destroy_it = (csdestroy_t *) kavddestroy;
		is_good = true;
		return;
	}
#endif

#ifdef __ICAP
	if (plugname == "icapscan") {
#ifdef DGDEBUG
		std::cout << "Enabling ICAPscan CS plugin" << std::endl;
#endif
		create_it = (cscreate_t *) icapcreate;
		destroy_it = (csdestroy_t *) icapdestroy;
		is_good = true;
		return;
	}
#endif

	create_it = NULL;
	destroy_it = NULL;
	if (!is_daemonised) {
		std::cerr << "Unable to load plugin: " << pluginConfigPath << std::endl;
	}
	syslog(LOG_ERR, "Unable to load plugin %s\n", pluginConfigPath);
	return;
}
