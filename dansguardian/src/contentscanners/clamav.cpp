// LibClamAV content scanning plugin

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

#include "../ContentScanner.hpp"
#include "../OptionContainer.hpp"

#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <clamav.h>


// GLOBALS

extern OptionContainer o;


// DECLARATIONS

// class name is relevant!
class clamavinstance:public CSPlugin
{
public:
	clamavinstance(ConfigVar & definition);

	// we are replacing the inherited scanMemory as it has support for it
	int scanMemory(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user, int filtergroup,
		const char *ip, const char *object, unsigned int objectsize);
	int scanFile(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user, int filtergroup,
		const char *ip, const char *filename);

	// could be useful, but doesn't yet do anything - see comments on implementation
	//int scanTest(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user, int filtergroup, const char *ip);

	int init();
	int quit();

private:
	// virus database root node
	struct cl_node *root;
	// archive limit options
	struct cl_limits limits;

	// convert clamav return value to standard return value
	int doRC(int rc, const char *vn);
};


// IMPLEMENTATION

// class factory code *MUST* be included in every plugin

clamavinstance::clamavinstance(ConfigVar & definition):CSPlugin(definition)
{
	cv = definition;
	return;
};

CSPlugin *clamavcreate(ConfigVar & definition)
{
	return new clamavinstance(definition);
}

void clamavdestroy(CSPlugin * p)
{
	delete p;
}

// end of Class factory

// destroy plugin
int clamavinstance::quit()
{
	cl_free(root);
	return DGCS_OK;
}

// does the given request need virus scanning?
// this could do clever things to return true only when something matches the
// options we pass to libclamav - but for now, it just calls the default,
// and so is unnecessary. PRA 13-10-2005
/*int clamavinstance::scanTest(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user, int filtergroup, const char *ip)
{
	return CSPlugin::scanTest(requestheader, docheader, user, filtergroup, ip);
}*/

// override the inherited scanMemory, since libclamav can actually scan memory areas directly
int clamavinstance::scanMemory(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user,
	int filtergroup, const char *ip, const char *object, unsigned int objectsize)
{
	lastmessage = lastvirusname = "";
	const char *vn = "";
	int rc = cl_scanbuff(object, objectsize - 1, &vn, root);
	return doRC(rc, vn);
}

// scan given filename
int clamavinstance::scanFile(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user, int filtergroup, const char *ip, const char *filename)
{
	lastmessage = lastvirusname = "";
	const char *vn = "";
	int rc = cl_scanfile(filename, &vn, NULL, root, &limits, CL_SCAN_STDOPT
		/*CL_ARCHIVE | CL_OLE2 | CL_MAIL | CL_OLE2 | CL_SCAN_PE | CL_SCAN_BLOCKBROKEN | CL_SCAN_HTML */ );
	return doRC(rc, vn);
}

// convert libclamav return values into our own standard return values
int clamavinstance::doRC(int rc, const char *vn)
{
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

// initialise libclamav
int clamavinstance::init()
{
	// always include these lists
	if (!readStandardLists()) {
		return DGCS_ERROR;
	}

	// set file, recursion and compression ratio limits for scanning archives
	root = NULL;
	limits.maxfiles = cv["maxfiles"].toInteger();
	if (limits.maxfiles < 1) {
		limits.maxfiles = 1000;
	}
	limits.maxfilesize = o.max_content_filecache_scan_size + 1024 * 1024;
	limits.maxreclevel = cv["maxreclevel"].toInteger();
	if (limits.maxreclevel < 1) {
		limits.maxreclevel = 5;
	}
	limits.maxratio = cv["maxratio"].toInteger();
	if (limits.maxratio < 1) {
		limits.maxratio = 200;
	}
#ifdef DGDEBUG
	std::cerr << "maxfiles:" << limits.maxfiles << " maxfilesize:" << limits.maxfilesize
		<< " maxreclevel:" << limits.maxreclevel << " maxratio:" << limits.maxratio << std::endl;
#endif

	// load virus database
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
