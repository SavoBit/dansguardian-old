//Implements the HTMLTemplate class, for displaying template-based banned pages to clients

//Please refer to http://dansguardian.org/?page=copyright
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

#include "platform.h"

#include "HTMLTemplate.hpp"
#include "RegExp.hpp"
#include "String.hpp"

#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <syslog.h>
//#include <istream.h>
#include <istream>
#include <iostream>
#include <fstream>


// GLOBALS

extern bool is_daemonised;


// IMPLEMENTATION

// wipe the loaded template
void HTMLTemplate::reset()
{
	html.clear();
}

// push a line onto our string list
void HTMLTemplate::push(String s)
{
	if (s.length() > 0) {
		html.push_back(s);
	}
}

// read in HTML template and find URL, readon, category etc. placeholders
bool HTMLTemplate::readTemplateFile(const char *filename)
{
	std::string linebuffer;
	RegExp re;
	// compile regexp for matching supported placeholders
	re.comp("-URL-|-REASONGIVEN-|-REASONLOGGED-|-USER-|-IP-|-FILTERGROUP-|-BYPASS-|-CATEGORIES-");
	unsigned int offset;
	String result;
	String line;
	std::ifstream templatefile(filename, ios::in);  // dansguardian.conf
	if (!templatefile.good()) {
		if (!is_daemonised) {
			std::cerr << "error reading: " << filename << std::endl;
		}
		syslog(LOG_ERR, "%s", "error reading HTML template file.");
		return false;
	}
	while (!templatefile.eof()) {
		std::getline(templatefile, linebuffer);
//        #ifdef DGDEBUG
//            std::cout << linebuffer << std::endl;
//        #endif
		line = linebuffer.c_str();
		// look for placeholders
		re.match(line.toCharArray());
		while (re.numberOfMatches() > 0) {
			// whenever we find one, push the text before it onto the list, then the placeholder, then the text after it
			offset = re.offset(0);
			result = re.result(0).c_str();
			if (offset > 0) {
				push(line.subString(0, offset));
				push(result);
				line = line.subString(offset + result.length(), line.length() - offset - result.length());
			} else {
				push(result);
				line = line.subString(result.length(), line.length() - result.length());
			}
			re.match(line.toCharArray());
		}
		// if any text remains, or we didn't find a placeholder, push the remainder of the line
		if (line.length() > 0) {
			push(line);
		}
	}
	templatefile.close();
//    #ifdef DGDEBUG
//        for(unsigned int j = 0; j < html.size(); j++) {
	//           std::cout << html[j] << std::endl;
	//       }
//    #endif
	return true;
}

// fill in placeholders with the given information and send the resulting page to the client
void HTMLTemplate::display(Socket *s, String url, String reason, String logreason, String categories, String user, String ip, String filtergroup, String hashed)
{
#ifdef DGDEBUG
	std::cout << "Displaying TEMPLATE" << std::endl;
#endif
	String line;
	bool newline;
	unsigned int sz = html.size() - 1;  // the last line can have no thingy. erm... carriage return?
	for (unsigned int i = 0; i < sz; i++) {
		newline = false;
		line = html[i];
		// look for placeholders (split onto their own line by readTemplateFile) and replace them
		if (line == "-URL-") {
			line = url;
		}
		else if (line == "-REASONGIVEN-") {
			line = reason;
		}
		else if (line == "-REASONLOGGED-") {
			line = logreason;
		}
		else if (line == "-USER-") {
			line = user;
		}
		else if (line == "-IP-") {
			line = ip;
		}
		else if (line == "-FILTERGROUP-") {
			line = filtergroup;
		}
		else if (line == "-CATEGORIES-") {
			if (categories.length() > 0) {
				line = categories;
			} else {
				line = "N/A";
			}
		}
		else if (line == "-BYPASS-") {
			if (hashed.length() > 0) {
				if (!url.after("://").contains("/")) {
					url += "/";
				}
				if (url.contains("?")) {
					line = url + "&" + hashed;
				} else {
					line = url + "?" + hashed;
				}
			} else {
				line = "";
			}
		} else {
			if (html[i + 1][0] != '-') {
				newline = true;
			}
		}
		if (line.length() > 0) {
			(*s).writeString(line.toCharArray());
		}
		if (newline) {
			(*s).writeString("\n");
		}
	}
	(*s).writeString(html[sz].toCharArray());
	(*s).writeString("\n");
}
