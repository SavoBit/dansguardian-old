// Proxy auth plugin

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

#include "../Auth.hpp"

#include <syslog.h>


// DECLARATIONS

// class name is relevant!
class proxyinstance:public AuthPlugin
{
public:
	proxyinstance(ConfigVar &definition):AuthPlugin(definition) {};
	int identify(Socket& peercon, Socket& proxycon, HTTPHeader &h, int &fg, std::string &string);
};


// IMPLEMENTATION

// class factory code *MUST* be included in every plugin

AuthPlugin *proxycreate(ConfigVar & definition)
{
	return new proxyinstance(definition);
}

// end of Class factory

// proxy auth header username extraction
int proxyinstance::identify(Socket& peercon, Socket& proxycon, HTTPHeader &h, int &fg, std::string &string)
{
	// extract username
	string = h.getAuthData();
	if (string.length() > 0) {
		string = string.substr(string.find_first_of(':')+1);
		fg = determineGroup(string);
		if (fg >= 0)
			return DGAUTH_OK;
		else
			return DGAUTH_NOUSER;
	}
	return DGAUTH_NOMATCH;
}
