// Ident class - provides various methods for retrieving client usernames

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

#ifndef __HPP_IDENT
#define __HPP_IDENT


// INCLUDES
#include "platform.h"

#include "Socket.hpp"
#include "HTTPHeader.hpp"
#include "OptionContainer.hpp"
#include "String.hpp"

class Ident
{
public:
	Ident();
	
	// get client's username from one of various sources, depending on configuration
	std::string getUsername(HTTPHeader * h, string * s, int port);

private:
	std::string username;
	
	// get username from proxy auth headers
	bool getUsernameProxyAuth(HTTPHeader * h);
	// get username via NTLM (unimplemented)
	bool getUsernameNTLM(HTTPHeader * h);
	// get username from ident server
	bool getUsernameIdent(string * s, int port, int serverport, int timeout);
};

#endif
