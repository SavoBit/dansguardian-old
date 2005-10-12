// Ident class - implements various methods of retrieving client usernames

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

//This file contains modifications suggested and mostly provided by
//Daniel Robbins 13/4/01 drobbins@gento.org
//Modifications include, but not limited to, getcontenttype(), << , >>


// INCLUDES
#include "Ident.hpp"


// GLOBALS

extern OptionContainer o;


// IMPLEMENTATION

Ident::Ident()
:	username("")
{
}

// Get username from proxy auth headers
bool Ident::getUsernameProxyAuth(HTTPHeader * h)
{
	username = (*h).getAuthUser();  // extract username
	if (username.length() > 0) {
		return true;
	}
	return false;
}

// get username via NTLM - unimplemented
bool Ident::getUsernameNTLM(HTTPHeader * h)
{
	return false;
}

// get client's username from one of various sources depending on enabled options
std::string Ident::getUsername(HTTPHeader * h, string * s, int port)
{
#ifdef DGDEBUG
	std::cout << "getting username..." << std::endl;
#endif
	if (username.length() > 0) {
		return username;
	}
	if (o.uim_proxyauth == 1) {
		getUsernameProxyAuth(h);
	}
	if (username.length() < 1 && o.uim_ntlm == 1) {
		getUsernameNTLM(h);
	}
	if (username.length() < 1 && o.uim_ident == 1) {
		getUsernameIdent(s, port, o.filter_port, 5);
	}
	if (username.length() < 1) {
		username = "-";
	}
	String u = username.c_str();
	u.toLower();
	username = u.toCharArray();
	return username;
}

// get username from ident server
bool Ident::getUsernameIdent(string *s, int port, int serverport, int timeout)
{
#ifdef DGDEBUG
	std::cout << "Connecting to:" << (*s) << std::endl;
	std::cout << "to ask about:" << port << std::endl;
#endif
	Socket iq;
	iq.setTimeout(timeout);
	int rc = iq.connect((*s).c_str(), 113);  // ident port
	if (rc) {
#ifdef DGDEBUG
		std::cerr << "Error connecting to obtain ident from: " << (*s) << std::endl;
#endif
		return false;
	}
#ifdef DGDEBUG
	std::cout << "Connected to:" << (*s) << std::endl;
#endif
	std::string request;
	request = String(port).toCharArray();
	request += ", ";
	request += String(serverport).toCharArray();
	request += "\r\n";
#ifdef DGDEBUG
	std::cout << "About to send:" << request << std::endl;
#endif
	if (!iq.writeToSocket((char *) request.c_str(), request.length(), 0, timeout)) {
#ifdef DGDEBUG
		std::cerr << "Error writing to ident connection to: " << (*s) << std::endl;
#endif
		try {
			iq.close();  // close conection to client
		} catch(exception & e) {
		}
		return false;
	}
#ifdef DGDEBUG
	std::cout << "wrote ident request to:" << (*s) << std::endl;
#endif
	char buff[8192];
	try {
		iq.getLine(buff, 8192, timeout);
	} catch(exception & e) {
		return false;
	}
	String temp;
	temp = buff;  // convert to String
#ifdef DGDEBUG
	std::cout << "got ident reply:" << temp << " from:" << (*s) << std::endl;
#endif
	try {
		iq.close();  // close conection to client
	}
	catch(exception & e) {
	}
	temp = temp.after(":");
	if (!temp.before(":").contains("USERID")) {
		return false;
	}
	temp = temp.after(":");
	temp = temp.after(":");
	temp.removeWhiteSpace();
	username = temp.toCharArray();
	if (username.length() > 0) {
		return true;
	}
	return false;
}
