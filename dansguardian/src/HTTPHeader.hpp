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

#ifndef __HPP_HTTPHeader
#define __HPP_HTTPHeader


// DEFINES

#define __DGHEADER_SENDALL 0
#define __DGHEADER_SENDFIRSTLINE 1
#define __DGHEADER_SENDREST 2


// INCLUDES

#include "platform.h"

#include <deque>

#include "String.hpp"
#include "DataBuffer.hpp"
#include "Socket.hpp"
#include "RegExp.hpp"


// DECLARATIONS

class HTTPHeader
{
public:
	std::deque<String> header;
	DataBuffer postdata;
	unsigned int port;

	// network communication funcs

	void setTimeout(int t);
	void in(Socket *sock, bool allowpersistent = false);
	void out(Socket *sock, int sendflag) throw(exception);
	
	// header value and type checks

	// request type: GET, HEAD, POST etc.
	String requestType();
	int contentLength();
	String getContentType();
	// check received content type against given content type
	bool isContentType(String t);
	// check HTTP message code to see if it's an auth required message
	bool authRequired();
	// Content-Disposition
	String disposition();
	String userAgent();
	// grab contents of X-Forwarded-For
	std::string getXForwardedForIP();
	// check HTTP message code to see if it's a redirect
	bool isRedirection();
	// see if content-type is something other than "identity"
	bool isCompressed();
	String contentEncoding();
	// grab the contents of Proxy-Authorization header
	// returns base64-decoding of the chunk of data after the auth type string
	std::string getAuthData();

	// detailed value/type checks

	bool malformedURL(String url);
	bool isPostUpload();
	String getAuthType();
	String url();

	// header modifications

	void addXForwardedFor(std::string clientip);
	// strip content-encoding, and simultaneously set content-length to newlen
	void removeEncoding(int newlen);
	void setContentLength(int newlen);
	// regexp search and replace
	// urlRegExp Code originally from from Ton Gorter 2004
	bool urlRegExp(int filtergroup);
	// make a connection persistent
	void makePersistent();

	// do URL decoding (%xx) on string
	String decode(String s);

	// Bypass URL & Cookie funcs
	
	// is this a temporary filter bypass URL?
	int isBypassURL(String *url, const char *magic, const char *clientip);
	// is this a scan bypass URL? (download previously scanned file)
	bool isScanBypassURL(String *url, const char *magic, const char *clientip);
	// is this a temporary filter bypass cookie?
	bool isBypassCookie(String *url, const char *magic, const char *clientip);
	// chop GBYPASS/GSPYBASS off URLs (must know it's there to begin with)
	void chopBypass(String url);
	void chopScanBypass(String url);
	// add cookie to outgoing headers with given name & value
	void setCookie(const char *cookie, const char *value);

private:
	// timeout for socket operations
	int timeout;

	// check & fix headers from servers that don't obey standards
	void checkheader(bool allowpersistent);

	// convert %xx back to original character
	String hexToChar(String n);
	// base64 decode an individual char
	int decode1b64(char c);
	// base64 decode a complete string
	std::string decodeb64(String line);

	// modify supplied accept-encoding header, adding "identity" and stripping unsupported compression types
	String modifyEncodings(String e);
	// modifies the URL in all relevant header lines after a regexp search and replace
	// setURL Code originally from from Ton Gorter 2004
	void setURL(String &url);

	// grab cookies from headers
	String getCookie(const char *cookie);
};

#endif
