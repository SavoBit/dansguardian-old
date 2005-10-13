// Kaspersky AV libs content scanning plugin (unfinished)

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

//TODO:	Replace error reporting with detailed entries in syslog(LOG_ERR), short entries in lastmessage.
//		Integrate properly with build system; make plugin work; test.

// INCLUDES

#include "../ContentScanner.hpp"
#include "../OptionContainer.hpp"

#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <kavclient.h>


// GLOBALS

extern OptionContainer o;


// DECLARATIONS

// class name is relevant
class kavavinstance:public CSPlugin
{
public:
	kavavinstance(ConfigVar & definition);

	int scanMemory(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user, int filtergroup,
		const char *ip, const char *object, unsigned int objectsize);
	int scanFile(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user, int filtergroup,
		const char *ip, const char *filename);

	int init();
	int quit(void);

private:
	// KAV daemon connection?
	kav_ctx kavcon;
	
	// convert KAV return value into standard return value
	int doRC(int rc);
};


// IMPLEMENTATION

// class factory code *MUST* be included in every plugin

kavavinstance::kavavinstance(ConfigVar & definition):CSPlugin(definition)
{
	cv = definition;
	return;
};

CSPlugin *kavavcreate(ConfigVar & definition)
{
	return new kavavinstance(definition);
}

void kavavdestroy(CSPlugin * p)
{
	delete p;
}

// end of Class factory

// destroy plugin
int kavavinstance::quit(void)
{
	if (kavcon != NULL) {
		kav_free(kavcon);  // return to kavcon 5 ;-)
	}
	return DGCS_OK;
}

// scan file & memory
int kavavinstance::scanMemory(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user, int filtergroup,
	const char *ip, const char *object, unsigned int objectsize)
{
	lastvirusname = lastmessage = "";
	if (kav_check_mem(kavcon, object, objectsize) != 0) {
		lastmessage = kav_strerror(kav_get_error(kavcon));
		return DGCS_SCANERROR;
	}
	lastvirusname = kav_result_get_report(kavcon);
	return doRC(kav_result_get_status(kavcon));
}

int kavavinstance::scanFile(HTTPHeader * requestheader, HTTPHeader * docheader, const char *user, int filtergroup, const char *ip, const char *filename)
{
	lastvirusname = lastmessage = "";
	if (kav_check_file(kavcon, filename) != 0) {
		lastmessage = kav_strerror(kav_get_error(kavcon));
		return DGCS_SCANERROR;
	}
	lastvirusname = kav_result_get_report(kavcon);
	return doRC(kav_result_get_status(kavcon));
}

// initialise plugin
int kavavinstance::init(int dgversion)
{
	// always include these lists
	if (!readStandardLists()) {
		return DGCS_ERROR;
	}

	kavcon = kav_new();
	if (kavcon == NULL) {
#ifdef DGDEBUG
		std::cout << "kav_new() error" << std::endl;
		syslog(LOG_ERR, "%s", "kav_new() error");
#endif
		return DGCS_ERROR;
	}
	udspath = cv["kavudsfile"];
	if (udspath.length() < 3 || udspath == "default") {
#ifdef DGDEBUG
		std::cout << "kavudsfile - using default option." << std::endl;
#endif
		// it would be far better to do a test connection to the file but
		// could not be arsed for now
		kav_set_socketpath(kavcon, NULL);
	} else {
		kav_set_socketpath(kavcon, udspath.toCharArray());
	}
	kav_set_timeout(kavcon, o.content_scanner_timeout);
	return DGCS_OK;
}

// convert KAV return value into standard return value
int kavavinstance::doRC(int rc)
{
	switch (rc) {
	case KAV_STATUS_CLEAN:
#ifdef DGDEBUG
		std::cerr << "KAV - he say yes (clean)" << std::endl;
#endif
		return DGCS_CLEAN;
	case KAV_STATUS_CURED:
	case KAV_STATUS_INFECTED_DELETED:
	case KAV_STATUS_VIRUSES_FOUND:
	case KAV_STATUS_CORRUPTED_VIRUSES_FOUND:
	case KAV_STATUS_SUSPICIOUS_FOUND:
#ifdef DGDEBUG
		std::cerr << "INFECTED!" << std::endl;
#endif
		return DGCS_INFECTED;
	default:
#ifdef DGDEBUG
		std::cerr << "KAV error" << std::endl;
#endif
	}
	return DGCS_SCANERROR;
}
