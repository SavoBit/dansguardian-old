// AuthPlugin class - interface for plugins for retrieving client usernames
// and filter group membership

//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@//jadeb.com).
//For support go to http://groups.yahoo.com/group/dansguardian

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

#include "Auth.hpp"
#include "OptionContainer.hpp"

#include <iostream>
#include <syslog.h>

// GLOBALS

extern OptionContainer o;
extern bool is_daemonised;

extern authcreate_t proxycreate;
extern authcreate_t identcreate;
extern authcreate_t ipcreate;

#ifdef __NTLM
extern authcreate_t ntlmcreate;
#endif

// IMPLEMENTATION

AuthPlugin::AuthPlugin(ConfigVar &definition)
{
	cv = definition;
}

int AuthPlugin::init(void *args)
{
	return 0;
}

int AuthPlugin::quit()
{
	return 0;
}

// determine what filter group the given username is in
// return -1 when user not found
int AuthPlugin::determineGroup(std::string &user)
{
	if (user.length() < 1 || user == "-") {
		return -1;
	}
	String u(user);
	String ue(u);
	ue += "=";

	char *i = o.filter_groups_list.findStartsWithPartial(ue.toCharArray());

	if (i == NULL) {
#ifdef DGDEBUG
		std::cout << "User not in filter groups list: " << ue << std::endl;
#endif
		return -1;
	}
#ifdef DGDEBUG
	std::cout << "User found: " << i << std::endl;
#endif
	ue = i;
	if (ue.before("=") == u) {
		ue = ue.after("=filter");
		int l = ue.length();
		if (l < 1 || l > 2) {
			return -1;
		}
		int g = ue.toInteger();
		if (g > o.numfg) {
			return -1;
		}
		if (g > 0) {
			g--;
		}
		return g;
	}
	return -1;
}

// take in a configuration file, find the AuthPlugin class associated with the plugname variable, and return an instance
AuthPlugin* auth_plugin_load(const char *pluginConfigPath)
{
	ConfigVar cv;

	if (cv.readVar(pluginConfigPath, "=") > 0) {
		if (!is_daemonised) {
			std::cerr << "Unable to load plugin config: " << pluginConfigPath << std::endl;
		}
		syslog(LOG_ERR, "Unable to load plugin config %s", pluginConfigPath);
		return NULL;
	}

	String plugname = cv["plugname"];
	if (plugname.length() < 1) {
		if (!is_daemonised) {
			std::cerr << "Unable read plugin config plugname variable: " << pluginConfigPath << std::endl;
		}
		syslog(LOG_ERR, "Unable read plugin config plugname variable %s", pluginConfigPath);
		return NULL;
	}

	if (plugname == "proxy") {
#ifdef DGDEBUG
		std::cout << "Enabling proxy auth plugin" << std::endl;
#endif
		return proxycreate(cv);
	}

	if (plugname == "ident") {
#ifdef DGDEBUG
		std::cout << "Enabling ident server auth plugin" << std::endl;
#endif
		return identcreate(cv);
	}

	if (plugname == "ip") {
#ifdef DGDEBUG
		std::cout << "Enabling IP-based auth plugin" << std::endl;
#endif
		return ipcreate(cv);
	}

#ifdef __NTLM
	if (plugname == "ntlm") {
#ifdef DGDEBUG
		std::cout << "Enabling NTLM auth plugin" << std::endl;
#endif
		return ntlmcreate(cv);
	}
#endif

	if (!is_daemonised) {
		std::cerr << "Unable to load plugin: " << pluginConfigPath << std::endl;
	}
	syslog(LOG_ERR, "Unable to load plugin %s", pluginConfigPath);
	return NULL;
}
