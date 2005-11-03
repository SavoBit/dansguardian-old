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

#ifndef __HPP_AUTH
#define __HPP_AUTH


// INCLUDES

#include "platform.h"

#include "Plugin.hpp"
#include "ConfigVar.hpp"
#include "HTTPHeader.hpp"


// DEFINES

// success
#define DGAUTH_OK 0

// auth info required for this method not found
#define DGAUTH_NOMATCH 1

// redirect the user to a login page
#define DGAUTH_REDIRECT 2

// permit these headers to be transmitted & continue - for multi-step auth
// like NTLM
#define DGAUTH_CONTINUE 3

// any < 0 return code signifies error


// DECLARATIONS

class AuthPlugin:public Plugin
{
public:
	AuthPlugin(ConfigVar &definition);
	
	virtual int init(void* args);
	virtual int quit();

	// return one of the codes defined above.
	// OK - put group no. in filtergroup & username in string
	// NOMATCH or CONTINUE - leave inputs alone
	// REDIRECT - leave group no. alone, put redirect URL in string
	virtual int identify(const int &clientport, std::string &clientip, HTTPHeader &h, int &fg, std::string &string) = 0;

private:
	ConfigVar cv;

protected:
	// determine what filter group the given username is in
	int determineGroup(std::string &user);
};

// class factory functions for Auth plugins
typedef AuthPlugin* authcreate_t(ConfigVar &);

// Return an instance of the plugin defined in the given configuration file
AuthPlugin* auth_plugin_load(const char *pluginConfigPath);

#endif
