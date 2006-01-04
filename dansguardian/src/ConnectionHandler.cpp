//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@jadeb//.com).
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

#include "ConnectionHandler.hpp"
#include "DataBuffer.hpp"
#include "UDSocket.hpp"
#include "Auth.hpp"
#include "FDTunnel.hpp"
#include "ImageContainer.hpp"
#include "FDFuncs.hpp"

#include <syslog.h>
#include <cerrno>
#include <cstdio>
#include <algorithm>
#include <netdb.h>
#include <cstdlib>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef __GCCVER3
#include <istream>
#else
#include <istream.h>
#endif


// GLOBALS
extern OptionContainer o;
extern bool is_daemonised;
extern bool reloadconfig;


// IMPLEMENTATION

// strip the URL down to just the IP/hostname, then do an isIPHostname on the result
bool ConnectionHandler::isIPHostnameStrip(String url)
{
	url.removePTP();  // chop off the ht(f)tp(s)://
	if (url.contains("/")) {
		url = url.before("/");  // chop off any path after the domain
	}
	return (*o.fg[0]).isIPHostname(url);
}

// perform URL encoding on a string
std::string ConnectionHandler::miniURLEncode(const char *s)
{
	std::string encoded;
	//char *buf = new char[16];  // way longer than needed
	char *buf = new char[2];
	unsigned char c;
	for (int i = 0; i < (signed) strlen(s); i++) {
		c = s[i];
		// allowed characters in a url that have non special meaning
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
			encoded += c;
			continue;
		}
		// all other characters get encoded
		sprintf(buf, "%02x", c);
		encoded += "%";
		encoded += buf;
	}
	delete[]buf;
	return encoded;
}

// create a temporary bypass URL for the banned page
String ConnectionHandler::hashedURL(String *url, int filtergroup, std::string *clientip, bool infectionbypass)
{
	// filter/virus bypass hashes last for a certain time only
	String timecode(time(NULL) + (infectionbypass ? (*o.fg[filtergroup]).infection_bypass_mode : (*o.fg[filtergroup]).bypass_mode));
	// use the standard key in normal bypass mode, and the infection key in infection bypass mode
	String magic = infectionbypass ? (*o.fg[filtergroup]).imagic.c_str() : (*o.fg[filtergroup]).magic.c_str();
	magic += (*clientip).c_str();
	magic += timecode;
	String res = infectionbypass ? "GIBYPASS=" : "GBYPASS=";
	if (!(*url).after("://").contains("/")) {
		String newurl = (*url);
		newurl += "/";
		res += newurl.md5(magic.toCharArray());
	} else {
		res += (*url).md5(magic.toCharArray());
	}
	res += timecode;
	return res;
}

// create temporary bypass cookie
String ConnectionHandler::hashedCookie(String * url, int filtergroup, std::string * clientip, int bypasstimestamp)
{
	String timecode(bypasstimestamp);
	String magic = (*o.fg[filtergroup]).cookie_magic.c_str();
	magic += (*clientip).c_str();
	magic += timecode;
	String res = (*url).md5(magic.toCharArray());
	res += timecode;

#ifdef DGDEBUG
	std::cout << "hashedCookie=" << res << std::endl;
#endif
	return res;
}

// when using IP address counting - have we got any remaining free IPs?
bool ConnectionHandler::gotIPs(char *ipstr) {
	if (reloadconfig)
		return false;
	UDSocket ipcsock;
	if (ipcsock.getFD() < 0) {
		syslog(LOG_ERR, "%s","Error creating ipc socket to IP cache");
		return false;
	}
	// TODO: put in proper file name check
	if (ipcsock.connect((char*) o.ipipc_filename.c_str()) < 0) {  // connect to dedicated ip list proc
		syslog(LOG_ERR, "%s","Error connecting via ipc to IP cache");
		return false;
	}
	char reply;
	ipstr[strlen(ipstr)] = '\n';
	try {
		ipcsock.writeToSockete(ipstr, strlen(ipstr)+1, 0, 6);
		ipcsock.readFromSocket(&reply, 1, 0, 6);  // throws on err
	}
	catch (exception& e) {
#ifdef DGDEBUG
		std::cerr << "Exception with IP cache" << std::endl;
		std::cerr << e.what() << std::endl;
#endif
		syslog(LOG_ERR, "Exception with IP cache");
		syslog(LOG_ERR, e.what());
	}
	ipstr[strlen(ipstr)] = '\0';
	ipcsock.close();
	return reply == 'Y';
}

// check the URL cache to see if we've already flagged an address as clean
bool ConnectionHandler::wasClean(String &url)
{
	if (reloadconfig)
		return false;
	String myurl = url.after("://");
	UDSocket ipcsock;
	if (ipcsock.getFD() < 0) {
		syslog(LOG_ERR, "%s", "Error creating ipc socket to url cache");
		return false;
	}
	if (ipcsock.connect((char *) o.urlipc_filename.c_str()) < 0) {	// conn to dedicated url cach proc
		syslog(LOG_ERR, "%s", "Error connecting via ipc to url cache");
		ipcsock.close();
		return false;
	}
	char reply;
#ifdef DGDEBUG
	std::cout << "sending clean request:" << myurl.toCharArray() << std::endl;
#endif
	myurl += "\n";
	try {
		ipcsock.writeString(myurl.toCharArray());  // throws on err
	}
	catch(exception & e) {
#ifdef DGDEBUG
		std::cerr << "Exception writing to url cache" << std::endl;
		std::cerr << e.what() << std::endl;
#endif
		syslog(LOG_ERR, "Exception writing to url cache");
		syslog(LOG_ERR, e.what());
	}
	try {
		ipcsock.readFromSocket(&reply, 1, 0, 6);  // throws on err
	}
	catch(exception & e) {
#ifdef DGDEBUG
		std::cerr << "Exception reading from url cache" << std::endl;
		std::cerr << e.what() << std::endl;
#endif
		syslog(LOG_ERR, "Exception reading from url cache");
		syslog(LOG_ERR, e.what());
	}
	ipcsock.close();
	return reply == 'Y';
}

// add a known clean URL to the cache
void ConnectionHandler::addToClean(String &url)
{
	if (reloadconfig)
		return;
	String myurl = url.after("://");
	UDSocket ipcsock;
	if (ipcsock.getFD() < 0) {
		syslog(LOG_ERR, "%s", "Error creating ipc socket to url cache");
		return;
	}
	if (ipcsock.connect((char *) o.urlipc_filename.c_str()) < 0) {	// conn to dedicated url cach proc
		syslog(LOG_ERR, "Error connecting via ipc to url cache");
#ifdef DGDEBUG
		std::cout << "Error connecting via ipc to url cache" << std::endl;
#endif
		return;
	}
	myurl = "A" + myurl;
	try {
		ipcsock.writeString(myurl.toCharArray());  // throws on err
	}
	catch(exception & e) {
#ifdef DGDEBUG
		std::cerr << "Exception adding to url cache" << std::endl;
		std::cerr << e.what() << std::endl;
#endif
		syslog(LOG_ERR, "Exception adding to url cache");
		syslog(LOG_ERR, e.what());
	}
	ipcsock.close();
}

// send a file to the client - used during bypass of blocked downloads
unsigned int ConnectionHandler::sendFile(Socket * peerconn, String & filename, String & filemime, String & filedis)
{
	int fd = open(filename.toCharArray(), O_RDONLY);
	if (fd < 0) {		// file access error
		syslog(LOG_ERR, "Error reading file to send");
#ifdef DGDEBUG
		std::cout << "Error reading file to send:" << filename << std::endl;
#endif
		String fnf = o.language_list.getTranslation(1230);
		String message = "HTTP/1.0 404 " + fnf + "\nContent-Type: text/html\n\n<HTML><HEAD><TITLE>" + fnf + "</TITLE></HEAD><BODY><H1>" + fnf + "</H1></BODY></HTML>\n";
		peerconn->writeString(message.toCharArray());
		return 0;
	}

	unsigned int filesize = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	String message = "HTTP/1.0 200 OK\nContent-Type: " + filemime + "\nContent-Length: " + String(filesize);
	if (filedis.length() > 0) {
		message += "\nContent-disposition: attachement; filename=" + filedis;
	}
	message += "\n\n";
	try {
		peerconn->writeString(message.toCharArray());
	}
	catch(exception & e) {
		close(fd);
		return 0;
	}

	// perform the actual sending
	unsigned int sent = 0;
	int rc;
	char *buffer = new char[250000];
	while (sent < filesize) {
		rc = readEINTR(fd, buffer, 250000);
#ifdef DGDEBUG
		std::cout << "reading send file rc:" << rc << std::endl;
#endif
		if (rc < 0) {
#ifdef DGDEBUG
			std::cout << "error reading send file so throwing exception" << std::endl;
#endif
			delete[]buffer;
			throw exception();
		}
		if (rc == 0) {
#ifdef DGDEBUG
			std::cout << "got zero bytes reading send file" << std::endl;
#endif
			break;  // should never happen
		}
		// as it's cached to disk the buffer must be reasonably big
		if (!peerconn->writeToSocket(buffer, rc, 0, 100)) {
			delete[]buffer;
			throw exception();
		}
		sent += rc;
#ifdef DGDEBUG
		std::cout << "total sent from temp:" << sent << std::endl;
#endif
	}
	delete[]buffer;
	close(fd);
	return sent;
}

// pass data between proxy and client, filtering as we go.
// this is the only public function of ConnectionHandler - all content blocking/filtering is triggered from calls that live here.
void ConnectionHandler::handleConnection(Socket &peerconn, String &ip, int port)
{
	struct timeval thestart;
	gettimeofday(&thestart, NULL);

	peerconn.setTimeout(10);

	HTTPHeader header;  // to hold the incoming client request header

	header.setTimeout(120);  // set a timeout as we don't want blocking 4 eva
	// this also sets how long a pconn will wait for other requests
	// squid apparently defaults to 1 minute (persistent_request_timeout),
	// so wait slightly less than this to avoid duff pconns.
	// checkme: have this value configurable, and/or reconnect to proxy
	// when the onward connection has timed out instead of failing?

	HTTPHeader docheader;  // to hold the returned page header from proxy

	docheader.setTimeout(20);

	DataBuffer docbody;  // to hold the returned page

	docbody.setTimeout(120);

	bool waschecked = false;  // flags
	bool wasrequested = false;
	bool isexception = false;
	bool isourwebserver = false;
	bool wasclean = false;
	bool cachehit = false;
	bool isbypass = false;
	bool iscookiebypass = false;
	bool isvirusbypass = false;
	int bypasstimestamp = 0;
	bool isscanbypass = false;
	bool ispostblock = false;
	bool pausedtoobig = false;
	bool wasinfected = false;
	bool wasscanned = false;
	bool contentmodified = false;
	bool urlmodified = false;
	bool isconnect;
	bool ishead;
	bool scanerror;

	bool runav = false;  // not just AV but any content scanner
	int headersent = 0;  // 0=none,1=first line,2=all
	std::deque<bool > sendtoscanner;

	std::string mimetype = "-";

	String url;
	String urld;
	String urldomain;

	std::string exceptionreason;  // to hold the reason for not blocking

	int docsize = 0;  // to store the size of the returned document for logging

	std::string clientip(ip.toCharArray());  // hold the clients ip
	delete clienthost;
	clienthost = NULL;  // and the hostname, if available
	matchedip = false;

#ifdef DGDEBUG			// debug stuff surprisingly enough
	std::cout << "got connection" << std::endl;
	std::cout << clientip << std::endl;
#endif

	Socket proxysock;  // to hold connection to proxy

	try {

		// connect to proxy
		int rc = proxysock.connect(o.proxy_ip, o.proxy_port);

		if (rc) {
#ifdef DGDEBUG
			std::cerr << "Error connecting to proxy" << std::endl;
#endif
			syslog(LOG_ERR, "%s", "Error connecting to proxy");
			return;  // if we can't connect to the proxy, there is no point
			// in continuing
		}

		header.in(&peerconn, true, true);  // get header from client, allowing persistency and breaking on reloadconfig
		bool persist = header.isPersistent();
		bool firsttime = true;
#ifdef DGDEBUG
		int pcount = 0;
#endif

		// assume all requests over the one persistent connection are from
		// the same user. means we only need to query the auth plugin until
		// we get credentials, then assume they are valid for all reqs. on
		// the persistent connection.
		std::string clientuser;
		std::string oldclientuser;
		int filtergroup, oldfg;
		bool authed = false;
		bool isbanneduser = false;
		bool persistent_authed = false;
		
		FDTunnel fdt;
		NaughtyFilter checkme;
		AuthPlugin* auth_plugin;

		// maintain a persistent connection
		while ((firsttime || persist) && !reloadconfig) {

			if (!firsttime) {
#ifdef DGDEBUG
				std::cout << "persisting (count " << ++pcount << ")" << std::endl;
				syslog(LOG_ERR, "Served %d requests on this connection so far", pcount);
				std::cout << clientip << std::endl;
#endif
				try {
					header.in(&peerconn, true, true);  // get header from client, allowing persistency and breaking on reloadconfig
				} catch (exception &e) {
#ifdef DGDEBUG
					std::cout << "Persistent connection closed" << std::endl;
#endif
					break;
				}
				// don't let the connection persist if the client doesn't want it to.
				persist = header.isPersistent();

				// we will actually need to do *lots* of resetting of flags etc. here for pconns to work
				gettimeofday(&thestart, NULL);
				waschecked = false;  // flags
				wasrequested = false;
				isexception = false;
				isourwebserver = false;
				wasclean = false;
				cachehit = false;
				isbypass = false;
				iscookiebypass = false;
				isvirusbypass = false;
				bypasstimestamp = 0;
				isscanbypass = false;
				ispostblock = false;
				pausedtoobig = false;
				wasinfected = false;
				wasscanned = false;
				contentmodified = false;
				urlmodified = false;

				authed = false;
				isbanneduser = false;

				//bool runav = false;  // not just AV but any content scanner
				headersent = 0;  // 0=none,1=first line,2=all
				delete clienthost;
				clienthost = NULL;  // and the hostname, if available
				matchedip = false;
				docsize = 0;  // to store the size of the returned document for logging
				mimetype = "-";
				exceptionreason = "";

				sendtoscanner.clear();
				
				// reset header, docheader & docbody
				// headers *should* take care of themselves on the next in()
				docbody.reset();
				// more?
			} else {
				// reset flags & objects next time round the loop
				firsttime = false;
			}

			// don't have credentials for this connection yet? get some!
			if (!persistent_authed) {
#ifdef DGDEBUG
				std::cout << "Not got persistent credentials for this connection - querying auth plugins" << std::endl;
#endif
				for (std::deque<Plugin*>::iterator i = o.authplugins_begin; i != o.authplugins_end; i++) {
#ifdef DGDEBUG
					std::cout << "Querying next auth plugin..." << std::endl;
#endif
					// try to get the username & parse the return value
					auth_plugin = (AuthPlugin*)(*i);
					rc = auth_plugin->identify(peerconn, proxysock, header, /*filtergroup,*/ clientuser);
					if (rc == DGAUTH_NOMATCH) {
#ifdef DGDEBUG
						std::cout<<"Auth plugin did not find a match; querying remaining plugins"<<std::endl;
#endif
						continue;
					}
					else if (rc == DGAUTH_REDIRECT) {
#ifdef DGDEBUG
						std::cout<<"Auth plugin told us to redirect client to \"" << clientuser << "\"; not querying remaining plugins"<<std::endl;
#endif
						// ident plugin told us to redirect to a login page
						proxysock.close();
						String writestring = "HTTP/1.0 302 Redirect\nLocation: ";
						writestring += clientuser;
						writestring += "\n\n";
						peerconn.writeString(writestring.toCharArray());
						return;
					}
					else if (rc < 0) {
						if (!is_daemonised)
							std::cerr<<"Auth plugin returned error code: "<<rc<<std::endl;
						syslog(LOG_ERR,"Auth plugin returned error code: %d", rc);
						proxysock.close();
						return;
					}
#ifdef DGDEBUG
					std::cout << "Auth plugin found username " << clientuser << " (" << oldclientuser << "), now determining group" << std::endl;
#endif
					if (clientuser == oldclientuser) {
#ifdef DGDEBUG
						std::cout << "Same user as last time, re-using old group no." << std::endl;
#endif
						authed = true;
						filtergroup = oldfg;
						break;
					}
					// try to get the filter group & parse the return value
					rc = auth_plugin->determineGroup(clientuser, filtergroup);
					if (rc == DGAUTH_OK) {
#ifdef DGDEBUG
						std::cout<<"Auth plugin found username & group; not querying remaining plugins"<<std::endl;
#endif
						authed = true;
						break;
					}
					else if (rc == DGAUTH_NOMATCH) {
#ifdef DGDEBUG
						std::cout<<"Auth plugin did not find a match; querying remaining plugins"<<std::endl;
#endif
						continue;
					}
					else if (rc == DGAUTH_NOUSER) {
#ifdef DGDEBUG
						std::cout<<"Auth plugin found username \"" << clientuser << "\" but no associated group; not querying remaining plugins"<<std::endl;
#endif
						filtergroup = 0;  //default group - one day configurable?
						authed = true;
						break;
					}
					else if (rc < 0) {
						if (!is_daemonised)
							std::cerr<<"Auth plugin returned error code: "<<rc<<std::endl;
						syslog(LOG_ERR,"Auth plugin returned error code: %d", rc);
						proxysock.close();
						return;
					}
				}
				if (!authed) {
#ifdef DGDEBUG
					std::cout << "No identity found; using defaults" << std::endl;
#endif
					clientuser = "-";
					filtergroup = 0;  //default group - one day configurable?
				} else {
#ifdef DGDEBUG
					std::cout << "Identity found; caching username & group" << std::endl;
#endif
					if (auth_plugin->is_connection_based) {
#ifdef DGDEBUG
						std::cout<<"Auth plugin is for a connection-based auth method - keeping credentials for entire connection"<<std::endl;
#endif
						persistent_authed = true;
					}
					oldclientuser = clientuser;
					oldfg = filtergroup;
				}
			}
#ifdef DGDEBUG
			else
				std::cout << "Already got credentials for this connection - not querying auth plugins" << std::endl;
#endif

#ifdef DGDEBUG
			std::cout << "username: " << clientuser << std::endl;
			std::cout << "filtergroup: " << filtergroup << std::endl;
			std::cout << "groupmode: " << o.fg[filtergroup]->group_mode << std::endl;
#endif

			// filter group modes are: 0 = banned, 1 = filtered, 2 = exception.
			// is this user banned?
			isbanneduser = (o.fg[filtergroup]->group_mode == 0);

			url = header.url();
			urld = header.decode(url);
			if (url.after("://").contains("/")) {
				urldomain = url.after("//").before("/");
			} else {
				urldomain = url.after("//");
			}

			if (header.malformedURL(url)) {
				// checks for bad URLs to prevent security hole
				try {	// writestring throws exception on error/timeout
					peerconn.writeString("HTTP/1.0 400 Bad Request\nContent-Type: text/html\n\n<HTML><HEAD><TITLE>DansGuardian - 400 Bad Request</TITLE></HEAD><BODY><H1>DansGuardian - 400 Bad Request</H1> ");
					peerconn.writeString(o.language_list.getTranslation(200));
					// The requested URL is malformed.
					peerconn.writeString("</BODY></HTML>\n");
				}
				catch(exception & e) {
				}
				proxysock.close();  // close connection to proxy
				return;
			}

			if (o.use_xforwardedfor == 1) {
				std::string xforwardip = header.getXForwardedForIP();
				if (xforwardip.length() > 6) {
					clientip = xforwardip;
				}
#ifdef DGDEBUG
				std::cout << "using x-forwardedfor:" << clientip << std::endl;
#endif
			}

			// is this machine banned?
			bool isbannedip = o.inBannedIPList(&clientip, clienthost);
			if (isbannedip)
				matchedip = clienthost == NULL;

			if (o.forwarded_for == 1) {
				header.addXForwardedFor(clientip);  // add squid-like entry
			}

			if (header.isScanBypassURL(&url, (*o.fg[filtergroup]).magic.c_str(), clientip.c_str())) {
#ifdef DGDEBUG
				std::cout << "Scan Bypass URL match" << std::endl;
#endif
				isscanbypass = true;
				isbypass = true;
				exceptionreason = o.language_list.getTranslation(608);
			}
			else if (((*o.fg[filtergroup]).bypass_mode != 0) || ((*o.fg[filtergroup]).infection_bypass_mode != 0)) {
#ifdef DGDEBUG
				std::cout << "About to check for bypass..." << std::endl;
#endif
				if ((*o.fg[filtergroup]).bypass_mode != 0)
					bypasstimestamp = header.isBypassURL(&url, (*o.fg[filtergroup]).magic.c_str(), clientip.c_str(), NULL);
				if ((bypasstimestamp == 0) && ((*o.fg[filtergroup]).infection_bypass_mode != 0))
					bypasstimestamp = header.isBypassURL(&url, (*o.fg[filtergroup]).imagic.c_str(), clientip.c_str(), &isvirusbypass);
				if (bypasstimestamp > 0) {
#ifdef DGDEBUG
					if (isvirusbypass)
						std::cout << "Infection bypass URL match" << std::endl;
					else
						std::cout << "Filter bypass URL match" << std::endl;
#endif
					header.chopBypass(url, isvirusbypass);
					if (bypasstimestamp > 1) {	// not expired
						isbypass = true;
						// checkme: need a TR string for virus bypass
						exceptionreason = o.language_list.getTranslation(606);
					}
				}
				else if (((*o.fg[filtergroup]).bypass_mode != 0)
					&& header.isBypassCookie(&urldomain, (*o.fg[filtergroup]).cookie_magic.c_str(), clientip.c_str()))
				{
#ifdef DGDEBUG
					std::cout << "Bypass cookie match" << std::endl;
#endif
					iscookiebypass = true;
					isbypass = true;
					exceptionreason = o.language_list.getTranslation(607);
				}
#ifdef DGDEBUG
				std::cout << "Finished bypass checks." << std::endl;
#endif
			}

#ifdef DGDEBUG
			if (isbypass) {
				std::cout << "Bypass activated!" << std::endl;
			}
#endif

			if (isscanbypass) {
				//we need to decode the URL and send the temp file with the
				//correct header to the client then delete the temp file
				String tempfilename = url.after("GSBYPASS=").after("&N=");
				String tempfilemime = tempfilename.after("&M=");
				String tempfiledis = tempfilemime.after("&D=");
				String rtype = header.requestType();
				tempfilemime = tempfilemime.before("&D=");
				tempfilename = o.download_dir + "/tf" + tempfilename.before("&M=");
				try {
					docsize = sendFile(&peerconn, tempfilename, tempfilemime, tempfiledis);
					header.chopScanBypass(url);
					url = header.url();
					//urld = header.decode(url);  // unneeded really

					doLog(clientuser, clientip, url, header.port, exceptionreason,
						rtype, docsize, NULL, o.ll, false, isexception, o.log_exception_hits, false, &thestart,
						cachehit, 200, mimetype, wasinfected, wasscanned, 0, filtergroup);

					if (o.delete_downloaded_temp_files == 1) {
						unlink(tempfilename.toCharArray());
					}
				}
				catch(exception & e) {
				}
				proxysock.close();  // close connection to proxy
				return;  // connection dealt with so exit
			}

			// being a banned user/IP overrides the fact that a site may be in the exception lists
			// needn't check these lists in bypass modes
			if (!(isbanneduser || isbannedip || isbypass)) {
				if ((o.fg[filtergroup]->group_mode == 2)) {	// admin user
					isexception = true;
					exceptionreason = o.language_list.getTranslation(601);
					// Exception client user match.
				}
				else if (o.inExceptionIPList(&clientip, clienthost)) {	// admin pc
					matchedip = clienthost == NULL;
					isexception = true;
					exceptionreason = o.language_list.getTranslation(600);
					// Exception client IP match.
				}
				else if ((*o.fg[filtergroup]).inExceptionSiteList(urld)) {	// allowed site
					if ((*o.fg[0]).isOurWebserver(url)) {
						isourwebserver = true;
					} else {
						isexception = true;
						exceptionreason = o.language_list.getTranslation(602);
						// Exception site match.
					}
				}
				else if ((*o.fg[filtergroup]).inExceptionURLList(urld)) {	// allowed url
					isexception = true;
					exceptionreason = o.language_list.getTranslation(603);
					// Exception url match.
				}
				else if ((rc = (*o.fg[filtergroup]).inExceptionRegExpURLList(urld)) > -1) {
					isexception = true;
					// exception regular expression url match:
					exceptionreason = o.language_list.getTranslation(609);
					exceptionreason += (*o.fg[filtergroup]).exception_regexpurl_list_source[rc].toCharArray();
				}
			}


#ifdef DGDEBUG
			std::cout << "extracted url:" << urld << std::endl;
#endif

			// don't run scanTest if content scanning is disabled, or on exceptions if contentscanexceptions is off,
			// or on SSL (CONNECT) requests, or on HEAD requests, or if in AV bypass mode
			String reqtype = header.requestType();
			isconnect = reqtype[0] == 'C';
			ishead = reqtype[0] == 'H';
			runav = ((*o.fg[filtergroup]).disable_content_scan != 1) && !(isexception && !o.content_scan_exceptions)
				&& !isconnect && !ishead && !isvirusbypass;
#ifdef DGDEBUG
			std::cerr << "runav = " << runav << std::endl;
#endif

			if (((isexception || iscookiebypass || isvirusbypass)
				// don't filter exception and local web server
				// Cookie bypass so don't need to add cookie so just CONNECT (unless should virus scan)
				&& !isbannedip	 // bad users pc
				&& !isbanneduser	 // bad user
				&& !runav)  // needs virus scanning
				// bad people still need to be able to access the banned page
				|| isourwebserver)
			{
				proxysock.readyForOutput(10);  // exception on timeout or error
				header.out(&proxysock, __DGHEADER_SENDALL, true);  // send proxy the request
				// if this is not a CONNECT request, check the returned headers for persistency flags
				if (!isconnect && persist) {
					docheader.in(&proxysock, persist);
					persist = docheader.isPersistent();
					docheader.out(&peerconn, __DGHEADER_SENDALL);
				}
				try {
					fdt.reset();  // make a tunnel object
					// tunnel from client to proxy and back
					// two-way if SSL
					fdt.tunnel(proxysock, peerconn, isconnect, docheader.contentLength());  // not expected to exception
					docsize = fdt.throughput;
					if (!isourwebserver) {	// don't log requests to the web server
						String rtype = header.requestType();
						doLog(clientuser, clientip, url, header.port, exceptionreason, rtype, docsize, NULL, o.ll, false, isexception,
							o.log_exception_hits, false, &thestart, cachehit, ((!isconnect && persist) ? docheader.returnCode() : 200),
							mimetype, wasinfected, wasscanned, 0, filtergroup);
					}
				}
				catch(exception & e) {
				}
				if (persist)
					continue;
				proxysock.close();  // close connection to proxy
				return;  // connection dealt with so exit
			}

			checkme.reset();  // our filter object
			checkme.filtergroup = filtergroup;

			if ((o.max_ips > 0) && (!gotIPs((char*)clientip.c_str()))) {
#ifdef DGDEBUG
				std::cout << "no client IP slots left" << std::endl;
#endif
				checkme.isItNaughty = true;
				checkme.whatIsNaughty = "IP limit exceeded.  There is a ";
				checkme.whatIsNaughty += String(o.max_ips).toCharArray();
				checkme.whatIsNaughty += " IP limit set.";
				checkme.whatIsNaughtyLog = checkme.whatIsNaughty;
				checkme.whatIsNaughtyCategories = "IP Limit";
			}
			
			// URL regexp search and replace
			urlmodified = header.urlRegExp(filtergroup);
			if (urlmodified) {
				url = header.url();
				urld = header.decode(url);
				if (url.after("://").contains("/")) {
					urldomain = url.after("//").before("/");
				} else {
					urldomain = url.after("//");
				}
				// if the user wants, re-check the exception site, URL and regex lists after modification.
				// this allows you to, for example, force safe search on Google URLs, then flag the
				// request as an exception, to prevent questionable language in returned site summaries
				// from blocking the entire request.
				// this could be achieved with exception phrases (which are, of course, always checked
				// after the URL) too, but there are cases for both, and flexibility is good.
				if ((o.recheck_replaced_urls == 1) && !(isbanneduser || isbannedip)) {
					if ((*o.fg[filtergroup]).inExceptionSiteList(urld)) {	// allowed site
						if ((*o.fg[0]).isOurWebserver(url)) {
							isourwebserver = true;
						} else {
							isexception = true;
							exceptionreason = o.language_list.getTranslation(602);
							// Exception site match.
						}
					}
					else if ((*o.fg[filtergroup]).inExceptionURLList(urld)) {	// allowed url
						isexception = true;
						exceptionreason = o.language_list.getTranslation(603);
						// Exception url match.
					}
					else if ((rc = (*o.fg[filtergroup]).inExceptionRegExpURLList(urld)) > -1) {
						isexception = true;
						// exception regular expression url match:
						exceptionreason = o.language_list.getTranslation(609);
						exceptionreason += (*o.fg[filtergroup]).exception_regexpurl_list_source[rc].toCharArray();
					}
					// don't filter exception and local web server
					if ((isexception
						// even after regex URL replacement, we still don't want banned IPs/users viewing exception sites
						&& !isbannedip	 // bad users pc
						&& !isbanneduser	 // bad user
						&& !runav)
						|| isourwebserver)
					{
						proxysock.readyForOutput(10);  // exception on timeout or error
						header.out(&proxysock, __DGHEADER_SENDALL, true);  // send proxy the request
						// if this is not a CONNECT request, check the returned headers for persistency flags
						if (!isconnect && persist) {
							docheader.in(&proxysock, persist);
							persist = docheader.isPersistent();
							docheader.out(&peerconn, __DGHEADER_SENDALL);
						}
						try {
							fdt.reset();  // make a tunnel object
							// tunnel from client to proxy and back
							// two-way if SSL
							fdt.tunnel(proxysock, peerconn, isconnect, docheader.contentLength());  // not expected to exception
							docsize = fdt.throughput;
							if (!isourwebserver) {	// don't log requests to the web server
								String rtype = header.requestType();
								doLog(clientuser, clientip, url, header.port, exceptionreason, rtype, docsize, NULL, o.ll, false, isexception,
									o.log_exception_hits, false, &thestart, cachehit, ((!isconnect && persist) ? docheader.returnCode() : 200),
									mimetype, wasinfected, wasscanned, checkme.naughtiness, filtergroup,
									// content wasn't modified, but URL was
									false, true);
							}
						}
						catch(exception & e) {
						}
						if (persist)
							continue;
						proxysock.close();  // close connection to proxy
						return;  // connection dealt with so exit
					}
				}
			}

			// if o.content_scan_exceptions is on then exceptions have to
			// pass on until later for AV scanning too.
			// Bloody annoying feature that adds mess and complexity to the code
			if (isexception) {
				checkme.isException = true;
				checkme.whatIsNaughtyLog = exceptionreason;
			}

			// Improved IF structure as suggested by AFN

			if (isconnect && !isbypass && !isexception) {
				// send header to proxy
				proxysock.readyForOutput(10);
				header.out(&proxysock, __DGHEADER_SENDALL, true);
				// get header from proxy
				proxysock.checkForInput(120);
				docheader.in(&proxysock, persist);
				persist = docheader.isPersistent();

				wasrequested = true;
				
				if (docheader.authRequired()) {
#ifdef DGDEBUG
					std::cout << "CONNECT request requires proxy authentication - doing standard filtering on auth required response" << std::endl;
#endif
					isconnect = false;
				} else {
#ifdef DGDEBUG
					std::cout << "CONNECT request does not require proxy authentication - attempting pre-emptive ban" << std::endl;
#endif
					// if its a connect and we don't do filtering on it now then
					// it will get tunneled and not filtered.  We can't tunnel later
					// as its ssl so we can't see the return header etc
					// So preemptive banning is forced on with ssl unfortunately.
					// It is unlikely to cause many problems though.
					requestChecks(&header, &checkme, &urld, &clientip, &clientuser, filtergroup, &ispostblock, isbanneduser, isbannedip);
#ifdef DGDEBUG
					std::cout << "done checking" << std::endl;
#endif
				}
			}

			if (!checkme.isItNaughty && isconnect) {
				// can't filter content of CONNECT
				if (!wasrequested) {
					proxysock.readyForOutput(10);  // exception on timeout or error
					header.out(&proxysock, __DGHEADER_SENDALL, true);  // send proxy the request
				} else {
					docheader.out(&peerconn, __DGHEADER_SENDALL);
				}
				try {
#ifdef DGDEBUG
					std::cout << "Opening tunnel for CONNECT" << std::endl;
#endif
					fdt.reset();  // make a tunnel object
					// tunnel from client to proxy and back - *true* two-way tunnel
					fdt.tunnel(proxysock, peerconn, true);  // not expected to exception
					docsize = fdt.throughput;
					String rtype = header.requestType();
					doLog(clientuser, clientip, url, header.port, exceptionreason, rtype, docsize, NULL, o.ll, false,
						isexception, o.log_exception_hits, false, &thestart,
						cachehit, (wasrequested ? docheader.returnCode() : 200), mimetype, wasinfected, wasscanned, checkme.naughtiness, filtergroup, false, urlmodified);
				}
				catch(exception & e) {
				}
				if (persist)
					continue;
				proxysock.close();  // close connection to proxy
				return;  // connection dealt with so exit
			}

			if (!checkme.isItNaughty) {
				// the request is ok, so we can	now pass it to the proxy, and check the returned header

				// temp char used in various places here
				char *i;

				// send header to proxy
				if (!wasrequested) {
					proxysock.readyForOutput(10);
					header.out(&proxysock, __DGHEADER_SENDALL, true);
					// get header from proxy
					proxysock.checkForInput(120);
					docheader.in(&proxysock, persist);
					persist = docheader.isPersistent();
				}

#ifdef DGDEBUG
				std::cout << "got header from proxy" << std::endl;
				if (!persist)
					std::cout << "header says close, so not persisting" << std::endl;
#endif
				wasrequested = true;  // so we know where we are later

				// if we're not careful, we can end up accidentally setting the bypass cookie twice.
				// because of the code flow, this second cookie ends up with timestamp 0, and is always disallowed.
				if (isbypass && !isvirusbypass && !iscookiebypass) {
#ifdef DGDEBUG
					std::cout<<"Setting GBYPASS cookie; bypasstimestamp = "<<bypasstimestamp<<std::endl;
#endif
					docheader.setCookie("GBYPASS", hashedCookie(&urldomain, filtergroup, &clientip, bypasstimestamp).toCharArray());
				}

				// now that we have the proxy's header too, we can make a better informed decision on whether or not to scan.
				// this used to be done before we'd grabbed the proxy's header, rendering exceptionvirusmimetypelist useless,
				// and exceptionvirusextensionlist less effective, because we didn't have a Content-Disposition header.
				// checkme: split scanTest into two parts? we can check the site & URL lists long before we get here, then
				// do the rest of the checks later.
				if (runav) {
					runav = false;
#ifdef DGDEBUG
					std::cerr << "cs plugins:" << o.csplugins.size() << std::endl;
#endif
					//send header to plugin here needed
					//also send user and group
					int csrc = 0;
#ifdef DGDEBUG
					int j = 0;
#endif
					for (std::deque<Plugin *>::iterator i = o.csplugins_begin; i != o.csplugins_end; i++) {
#ifdef DGDEBUG
						std::cerr << "running scanTest " << j << std::endl;
#endif
						csrc = ((CSPlugin*)(*i))->scanTest(&header, &docheader, clientuser.c_str(), filtergroup, clientip.c_str());
#ifdef DGDEBUG
						std::cerr << "scanTest " << j << " returned: " << csrc << std::endl;
#endif
						if (csrc > 0) {
							sendtoscanner.push_back(true);
							runav = true;
						} else {
							if (csrc < 0)
								syslog(LOG_ERR, "scanTest returned error: %d", csrc);
							sendtoscanner.push_back(false);
						}
#ifdef DGDEBUG
						j++;
#endif
					}
				}
#ifdef DGDEBUG
				std::cerr << "runav = " << runav << std::endl;
#endif

				// no need to check bypass mode, exception mode, auth required headers, or banned ip/user (the latter get caught by requestChecks later)
				// checkme: surely redirections don't have a MIME type either? why do *any* checking on them - we just force them to be un-naughty later!
				if (!isexception && !isbypass && !(isbannedip || isbanneduser) && !docheader.authRequired())
				{

					mimetype = docheader.getContentType().toCharArray();

					unsigned int p = (*o.fg[filtergroup]).banned_mimetype_list;
					if ((i = (*o.lm.l[p]).findInList((char *) mimetype.c_str())) != NULL) {
						checkme.whatIsNaughty = o.language_list.getTranslation(800);
						// Banned MIME Type:
						checkme.whatIsNaughty += i;
						checkme.whatIsNaughtyLog = checkme.whatIsNaughty;
						checkme.isItNaughty = true;
						checkme.whatIsNaughtyCategories = "Banned MIME Type.";
					}
#ifdef DGDEBUG
					std::cout << mimetype.length() << std::endl;
					std::cout << ":" << mimetype;
					std::cout << ":" << std::endl;
#endif

					if (!checkme.isItNaughty && !docheader.isRedirection()) {
						// Can't ban file extensions of URLs that just redirect
						String tempurl = urld;
						String tempdispos = docheader.disposition();
						if (tempdispos.length() > 1) {
							// dispos filename must take presidense
#ifdef DGDEBUG
							std::cout << "Disposition filename:" << tempdispos << ":" << std::endl;
#endif
							// The function expects a url so we have to
							// generate a psudo one.
							tempdispos = "http://foo.bar/" + tempdispos;
							if ((i = (*o.fg[filtergroup]).inBannedExtensionList(tempdispos)) != NULL) {
								checkme.whatIsNaughty = o.language_list.getTranslation(900);
								// Banned extension:
								checkme.whatIsNaughty += i;
								checkme.whatIsNaughtyLog = checkme.whatIsNaughty;
								checkme.isItNaughty = true;
								checkme.whatIsNaughtyCategories = "Banned extension.";
							}
						} else {
							if (!tempurl.contains("?")) {
								if ((i = (*o.fg[filtergroup]).inBannedExtensionList(tempurl)) != NULL) {
									checkme.whatIsNaughty = o.language_list.getTranslation(900);
									// Banned extension:
									checkme.whatIsNaughty += i;
									checkme.whatIsNaughtyLog = checkme.whatIsNaughty;
									checkme.isItNaughty = true;
									checkme.whatIsNaughtyCategories = "Banned extension.";
								}
							}
							else if (String(mimetype.c_str()).contains("application/")) {
								while (tempurl.endsWith("?")) {
									tempurl.chop();
								}
								while (tempurl.contains("/")) {	// no slash no url
									if ((i = (*o.fg[filtergroup]).inBannedExtensionList(tempurl)) != NULL) {
										checkme.whatIsNaughty = o.language_list.getTranslation(900);
										// Banned extension:
										checkme.whatIsNaughty += i;
										checkme.whatIsNaughtyLog = checkme.whatIsNaughty;
										checkme.isItNaughty = true;
										checkme.whatIsNaughtyCategories = "Banned extension.";
										break;
									}
									while (tempurl.contains("/") && !tempurl.endsWith("?")) {
										tempurl.chop();
									}
									tempurl.chop();  // get rid of the ?
								}
							}
						}
					}
				}

				// check header sent to proxy - this could be done before the send, but we
				// want to wait until after the MIME type & extension checks, because they may
				// act as a quicker rejection.
				if (!isbypass && !isexception && !checkme.isItNaughty && !docheader.authRequired()) {
					requestChecks(&header, &checkme, &urld, &clientip, &clientuser, filtergroup,
						&ispostblock, isbanneduser, isbannedip);
				}

				// check body from proxy
				// can't do content filtering on HEAD
				if (!checkme.isItNaughty && !docheader.isRedirection() && !ishead) {
					if (docheader.isContentType("text") || runav) {
						if (o.url_cache_number > 0 && !(!o.scan_clean_cache && runav)) {
							if (wasClean(urld)) {
								wasclean = true;
								cachehit = true;
								runav = false;
#ifdef DGDEBUG
								std::cout << "url was clean skipping content and AV checking" << std::endl;
#endif
							}
						}
						waschecked = true;
						contentFilter(&docheader, &header, &docbody, &proxysock, &peerconn, &headersent, &pausedtoobig,
							&docsize, &checkme, runav, wasclean, cachehit, filtergroup, &sendtoscanner, &clientuser, &clientip,
							&wasinfected, &wasscanned, isbypass, urld, urldomain, &scanerror, contentmodified);
					}
				}
			}

			if (!isexception && checkme.isException) {
				isexception = true;
				exceptionreason = checkme.whatIsNaughtyLog;
			}

			if (docheader.isRedirection()) {
				checkme.isItNaughty = false;
			}

			if (o.url_cache_number > 0) {
				if (!wasclean && !checkme.isItNaughty
					&& (docheader.isContentType("text") || (runav && o.scan_clean_cache))
					&& header.requestType() == "GET" && !docheader.authRequired())
				{
					addToClean(urld);
				}
			}

			// then we deny. previously, this checked the isbypass flag too; now, since bypass requests only undergo the same checking
			// as exceptions, it needn't. and in fact it mustn't, if bypass requests are to be virus scanned/blocked in the same manner as exceptions.
			if (checkme.isItNaughty) {
				String rtype = header.requestType();
				std::cout<<"Category: "<<checkme.whatIsNaughtyCategories<<std::endl;
				doLog(clientuser, clientip, url, header.port, checkme.whatIsNaughtyLog,
					rtype, docsize, &checkme.whatIsNaughtyCategories, o.ll, true, false, 0, false, &thestart,
					cachehit, 403, mimetype, wasinfected, wasscanned, checkme.naughtiness, filtergroup, contentmodified, urlmodified);
				if (denyAccess(&peerconn, &proxysock, &header, &docheader, &url, &checkme, &clientuser,&clientip, filtergroup, ispostblock, headersent, wasinfected, scanerror))
				{
					return;  // not stealth mode
				}
				// if get here in stealth mode
			}

			if (wasrequested == false) {
				proxysock.readyForOutput(10);  // exceptions on error/timeout
				header.out(&proxysock, __DGHEADER_SENDALL, true);  // exceptions on error/timeout
				proxysock.checkForInput(120);  // exceptions on error/timeout
				docheader.in(&proxysock, persist);  // get reply header from proxy
			}
#ifdef DGDEBUG
			std::cout << "sending header to client" << std::endl;
#endif
			peerconn.readyForOutput(10);  // exceptions on error/timeout
			if (headersent == 1) {
				docheader.out(&peerconn, __DGHEADER_SENDREST);  // send rest of header to client
#ifdef DGDEBUG
				std::cout << "sent rest header to client" << std::endl;
#endif
			}
			else if (headersent == 0) {
				docheader.out(&peerconn, __DGHEADER_SENDALL);  // send header to client
#ifdef DGDEBUG
				std::cout << "sent all header to client" << std::endl;
#endif
			}

			if (waschecked) {
				if (!docheader.authRequired() && !pausedtoobig) {
					String rtype = header.requestType();
					doLog(clientuser, clientip, url, header.port, exceptionreason,
						rtype, docsize, NULL, o.ll, false, isexception, o.log_exception_hits,
						docheader.isContentType("text"), &thestart, cachehit, docheader.returnCode(), mimetype,
						wasinfected, wasscanned, checkme.naughtiness, filtergroup, contentmodified, urlmodified);
				}

				peerconn.readyForOutput(10);  // check for error/timeout needed

				// it must be clean if we got here

				if (docbody.dontsendbody && docbody.tempfilefd > -1) {
					// must have been a 'fancy'
					// download manager so we need to send a special link which
					// will get recognised and cause DG to send the temp file to
					// the browser.  The link will be the original URL with some
					// magic appended to it like the bypass system.

					// format is:
					// GSBYPASS=hash(ip+url+tempfilename+mime+disposition+secret)
					// &N=tempfilename&M=mimetype&D=dispos

#ifdef DGDEBUG
					std::cout << "sending magic link to client" << std::endl;
#endif

					String ip = clientip;
					String tempfilename = docbody.tempfilepath.after("/tf");
					String tempfilemime = docheader.getContentType();
					String tempfiledis = docheader.disposition();
					String secret = (*o.fg[filtergroup]).magic.c_str();
					String magic = ip + url + tempfilename + tempfilemime + tempfiledis + secret;
					String hashed = magic.md5();
					String sendurl = url;
					if (!sendurl.after("://").contains("/")) {
						sendurl += "/";
					}
					if (sendurl.contains("?")) {
						sendurl = sendurl + "&GSBYPASS=" + hashed + "&N=";
					} else {
						sendurl = sendurl + "?GSBYPASS=" + hashed + "&N=";
					}
					sendurl += tempfilename + "&M=" + tempfilemime + "&D=" + tempfiledis;
					docbody.dm_plugin->sendLink(peerconn, sendurl, url);
				} else {
#ifdef DGDEBUG
					std::cout << "sending body to client" << std::endl;
#endif
					docbody.out(&peerconn);  // send doc body to client
				}
#ifdef DGDEBUG
				if (pausedtoobig) {
					std::cout << "sent PARTIAL body to client" << std::endl;
				} else {
					std::cout << "sent body to client" << std::endl;
				}
#endif
				if (pausedtoobig && !docbody.dontsendbody) {
#ifdef DGDEBUG
					std::cout << "about to start tunnel to send the rest" << std::endl;
#endif

					fdt.reset();
#ifdef DGDEBUG
					std::cout << "tunnel activated" << std::endl;
#endif
					fdt.tunnel(proxysock, peerconn);
					docsize += fdt.throughput;
					String rtype = header.requestType();
					doLog(clientuser, clientip, url, header.port, exceptionreason,
						rtype, docsize, NULL, o.ll, false, isexception, o.log_exception_hits,
						docheader.isContentType("text"), &thestart, cachehit, docheader.returnCode(), mimetype,
						wasinfected, wasscanned, checkme.naughtiness, filtergroup, contentmodified, urlmodified);
				}
			} else {	// was not supposed to be checked
				fdt.reset();
#ifdef DGDEBUG
				std::cout << "tunnel activated" << std::endl;
#endif
				fdt.tunnel(proxysock, peerconn, false, docheader.contentLength());
				docsize = fdt.throughput;
				String rtype = header.requestType();
				doLog(clientuser, clientip, url, header.port, exceptionreason,
					rtype, docsize, NULL, o.ll, false, isexception, o.log_exception_hits,
					docheader.isContentType("text"), &thestart, cachehit, docheader.returnCode(), mimetype,
					wasinfected, wasscanned, checkme.naughtiness, filtergroup, contentmodified, urlmodified);
			}
		} // while persist
	}
	catch(exception & e) {
#ifdef DGDEBUG
		std::cerr << "connection handler caught an exception: " << e.what() << std::endl;
#endif
		proxysock.close();  // close connection to proxy
		return;
	}

	proxysock.close();  // close conection to squid

	try {
		peerconn.readyForOutput(10);
	}
	catch(exception & e) {
		return;
	}
	return;
}

// decide whether or not to perform logging, categorise the log entry, and write it.
void ConnectionHandler::doLog(std::string &who, std::string &from, String &where, unsigned int &port,
		std::string &what, String &how, int &size, std::string *cat, int &loglevel, bool isnaughty,
		bool isexception, int logexceptions, bool istext, struct timeval *thestart, bool cachehit,
		int code, std::string &mimetype, bool wasinfected, bool wasscanned, int naughtiness, int filtergroup,
		bool contentmodified, bool urlmodified)
{
	// don't log if logging disabled entirely, or if it's an ad block and ad logging is disabled
	if ((loglevel == 0) || ((cat != NULL) && (o.log_ad_blocks == 0) && (strstr(cat->c_str(),"ADs") != NULL))) {
#ifdef DGDEBUG
		if ((loglevel != 0) && (cat != NULL))
			std::cout << "Not logging AD blocks" << std::endl;
#endif
		return;
	}
	else if ((isexception && logexceptions == 1)
		|| isnaughty || loglevel == 3 || (loglevel == 2 && istext))
	{
		if (port != 0 && port != 80) {
			// put port numbers of non-standard HTTP requests into the logged URL
			String newwhere = where.toCharArray();
			if (newwhere.after("://").contains("/")) {
				String proto, host, path;
				proto = newwhere.before("://");
				host = newwhere.after("://");
				path = host.after("/");
				host = host.before("/");
				newwhere = proto;
				newwhere += "://";
				newwhere += host;
				newwhere += ":";
				newwhere += String((int) port);
				newwhere += "/";
				newwhere += path;
				where = newwhere.toCharArray();
			} else {
				where += ":";
				where += String((int) port).toCharArray();
			}
		}
		
		// stamp log entries so they stand out/can be searched
		if (isnaughty) {
			what = "*DENIED* " + what;
		}
		else if (isexception) {
			if (logexceptions == 1) {
				what = "*EXCEPTION* " + what;
			} else {
				what = "";
			}
		}
		if (wasscanned) {
			if (wasinfected) {
				what = "*INFECTED* " + what;
			} else {
				what = "*SCANNED* " + what;
			}
		}
		if (contentmodified) {
			what = "*CONTENTMOD* " + what;
		}
		if (urlmodified) {
			what = "*URLMOD* " + what;
		}

		// start making the log entry proper
		std::string logline, year, month, day, hour, min, sec, when, ssize, sweight;

		// "when" not used in format 3
		if (o.log_file_format != 3) {
			String temp;
			time_t tnow;  // to hold the result from time()
			struct tm *tmnow;  // to hold the result from localtime()
			time(&tnow);  // get the time after the lock so all entries in order
			tmnow = localtime(&tnow);  // convert to local time (BST, etc)
			year = String(tmnow->tm_year + 1900).toCharArray();
			month = String(tmnow->tm_mon + 1).toCharArray();
			day = String(tmnow->tm_mday).toCharArray();
			hour = String(tmnow->tm_hour).toCharArray();
			temp = String(tmnow->tm_min);
			if (temp.length() == 1) {
				temp = "0" + temp;
			}
			min = temp.toCharArray();
			temp = String(tmnow->tm_sec);
			if (temp.length() == 1) {
				temp = "0" + temp;
			}
			sec = temp.toCharArray();
			when = year + "." + month + "." + day + " " + hour + ":" + min + ":" + sec;
			// truncate long log items
			/*if ((o.max_logitem_length > 0) && (when.length() > o.max_logitem_length))
				when = when.substr(0, o.max_logitem_length);*/
		}
		
		sweight = String(naughtiness).toCharArray();
		ssize = String(size).toCharArray();

		// truncate long log items
		if (o.max_logitem_length > 0) {
			where.limitLength(o.max_logitem_length);
			if ((cat != NULL) && (cat->length() > o.max_logitem_length)) {
				(*cat) = cat->substr(0, o.max_logitem_length);
			}
			if (what.length() > o.max_logitem_length) {
				what = what.substr(0, o.max_logitem_length);
			}
			/*if (who.length() > o.max_logitem_length)
				who = who.substr(0, o.max_logitem_length);
			if (from.length() > o.max_logitem_length)
				from = from.substr(0, o.max_logitem_length);
			how.limitLength(o.max_logitem_length);
			if (ssize.length() > o.max_logitem_length)
				ssize = ssize.substr(0, o.max_logitem_length);*/
		}
		
		// put client hostname in log if enabled.
		// for banned & exception IP/hostname matches, we want to output exactly what was matched against,
		// be it hostname or IP - therefore only do lookups here when we don't already have a cached hostname,
		// and we don't have a straight IP match agaisnt the banned or exception IP lists.
		if ((o.log_client_hostnames == 1) && (clienthost == NULL) && !matchedip && (o.anonymise_logs != 1)) {
#ifdef DGDEBUG
			std::cout<<"logclienthostnames enabled but reverseclientiplookups disabled; lookup forced."<<std::endl;
#endif
			std::deque<String> names = o.fg[0]->ipToHostname(from.c_str());
			if (names.size() > 0)
				clienthost = new std::string(names.front().toCharArray());
		}

		// blank out IP, hostname and username if desired
		if (o.anonymise_logs == 1) {
			who = "";
			from = "0.0.0.0";
			delete clienthost;
			clienthost = NULL;
		}
		
		std::string stringcode = String(code).toCharArray();
		std::string stringgroup = String(filtergroup+1).toCharArray();
		stringgroup = "filter" + stringgroup;
		
		switch (o.log_file_format) {
		case 4:
			logline = when + "\t" + who + "\t" + stringgroup + "\t" + (clienthost ? *clienthost : from) + "\t" + where.toCharArray() + "\t" + what
				+ "\t" + how.toCharArray() + "\t" + stringcode + "\t" + ssize + "\t" + mimetype + "\t" + sweight + "\t" + (cat ? (*cat) : "N/A") + "\n";
			break;
		case 3:
			{
				// as utime and duration are only logged in format 3, their creation is best done here, not in all cases.
				std::string duration, utime, hier, hitmiss;
				struct timeval theend;
				gettimeofday(&theend, NULL);
				long durationsecs, durationusecs;
				durationsecs = theend.tv_sec - (*thestart).tv_sec;
				durationusecs = theend.tv_usec - (*thestart).tv_usec;
				durationusecs = (durationusecs / 1000) + durationsecs * 1000;
				String temp = String((int) durationusecs);
				while (temp.length() < 6) {
					temp = " " + temp;
				}
				duration = temp.toCharArray();
				temp = String((int) (theend.tv_usec / 1000));
				while (temp.length() < 3) {
					temp = "0" + temp;
				}
				if (temp.length() > 3) {
					temp = "999";
				}
				utime = temp.toCharArray();
				utime = "." + utime;
				utime = String((int) theend.tv_sec).toCharArray() + utime;

				if (code == 403) {
					hitmiss = "TCP_DENIED/403";
				} else {
					if (cachehit) {
						hitmiss = "TCP_HIT/";
						hitmiss += stringcode;
					} else {
						hitmiss = "TCP_MISS/";
						hitmiss += stringcode;
					}
				}
				hier = "DEFAULT_PARENT/";
				hier += o.proxy_ip;

				/*if (o.max_logitem_length > 0) {
					if (utime.length() > o.max_logitem_length)
						utime = utime.substr(0, o.max_logitem_length);
					if (duration.length() > o.max_logitem_length)
						duration = duration.substr(0, o.max_logitem_length);
					if (hier.length() > o.max_logitem_length)
						hier = hier.substr(0, o.max_logitem_length);
					if (hitmiss.length() > o.max_logitem_length)
						hitmiss = hitmiss.substr(0, o.max_logitem_length);
				}*/

				logline = utime + " " + duration + " " + (clienthost ? *clienthost : from) + " " + hitmiss + " " + ssize + " "
					+ how.toCharArray() + " " + where.toCharArray() + " " + who + " " + hier + " " + mimetype + "\n";
				break;
			}
		case 2:
			logline = "\"" + when + "\",\"" + who + "\",\"" + stringgroup + "\",\"" + (clienthost ? *clienthost : from) + "\",\"" + where.toCharArray()
				+ "\",\"" + what + "\",\"" + how.toCharArray() + "\",\"" + stringcode + "\",\"" + ssize + "\",\"" + mimetype + "\",\""
				+ sweight + "\",\"" + (cat ? (*cat) : "N/A") + "\"\n";
			break;
		default:
			logline = when + " " + who + " " + stringgroup + " " + (clienthost ? *clienthost : from) + " " + where.toCharArray() + " " + what + " "
				+ how.toCharArray() + " " + stringcode + " " + ssize + " " + mimetype + " " + sweight + " " + (cat ? (*cat) : "N/A") + "\n";
		}

		// connect to dedicated logging proc
		UDSocket ipcsock;
		if (ipcsock.getFD() < 0) {
			syslog(LOG_ERR, "%s", "Error creating ipc socket to log");
			return;
		}
		if (ipcsock.connect((char *) o.ipc_filename.c_str()) < 0) {
			syslog(LOG_ERR, "%s", "Error connecting via ipc to log");
			ipcsock.close();
			return;
		}
		ipcsock.writeString(logline.c_str());
		ipcsock.close();
	}
}

// check the request header is OK (client host/user/IP allowed to browse, site not banned, upload not too big)
void ConnectionHandler::requestChecks(HTTPHeader *header, NaughtyFilter *checkme, String *urld,
	std::string *clientip, std::string *clientuser, int filtergroup, bool *ispostblock,
	bool &isbanneduser, bool &isbannedip)
{
	if (isbannedip) {
		(*checkme).isItNaughty = true;
		(*checkme).whatIsNaughtyLog = o.language_list.getTranslation(100);
		// Your IP address is not allowed to web browse:
		(*checkme).whatIsNaughtyLog += clienthost ? *clienthost : *clientip;
		(*checkme).whatIsNaughty = o.language_list.getTranslation(101);
		// Your IP address is not allowed to web browse.
		(*checkme).whatIsNaughtyCategories = "Banned Client IP";
		return;
	}
	else if (isbanneduser) {
		(*checkme).isItNaughty = true;
		(*checkme).whatIsNaughtyLog = o.language_list.getTranslation(102);
		// Your username is not allowed to web browse:
		(*checkme).whatIsNaughtyLog += (*clientuser);
		(*checkme).whatIsNaughty = (*checkme).whatIsNaughtyLog;
		(*checkme).whatIsNaughtyCategories = "Banned User";
		return;
	}

	char *i;
	int j;
	String temp;
	temp = (*urld);
	bool igsl = (*o.fg[filtergroup]).inGreySiteList(temp);
	bool igul = (*o.fg[filtergroup]).inGreyURLList(temp);

	if (!(*checkme).isItNaughty && (*o.fg[filtergroup]).blanketblock == 1 && !igsl && !igul) {
		(*checkme).isItNaughty = true;
		(*checkme).whatIsNaughty = o.language_list.getTranslation(502);
		// Blanket Block is active and that site is not on the white list.
		(*checkme).whatIsNaughtyLog = (*checkme).whatIsNaughty;
		(*checkme).whatIsNaughtyCategories = "Blanket Block";
	}

	if (!(*checkme).isItNaughty && (*o.fg[filtergroup]).blanketblock == 0) {
		if ((*o.fg[filtergroup]).blanket_ip_block == 1 && isIPHostnameStrip(temp)) {
			(*checkme).isItNaughty = true;
			(*checkme).whatIsNaughty = o.language_list.getTranslation(502);
			// Blanket IP Block is active and that address is an IP only address.
			(*checkme).whatIsNaughtyLog = (*checkme).whatIsNaughty;
			(*checkme).whatIsNaughtyCategories = "Blanket IP Block";
		}
		else if (!igsl && !igul && ((i = (*o.fg[filtergroup]).inBannedSiteList(temp)) != NULL)) {
			(*checkme).whatIsNaughty = o.language_list.getTranslation(500);  // banned site
			(*checkme).whatIsNaughty += i;
			(*checkme).whatIsNaughtyLog = (*checkme).whatIsNaughty;
			(*checkme).isItNaughty = true;
			(*checkme).whatIsNaughtyCategories = (*o.lm.l[(*o.fg[filtergroup]).banned_site_list]).lastcategory.toCharArray();
		}
	}

	if (!(*checkme).isItNaughty) {
		if (!igsl && !igul && ((i = (*o.fg[filtergroup]).inBannedURLList(temp)) != NULL)) {
			(*checkme).whatIsNaughty = o.language_list.getTranslation(501);
			// Banned URL:
			(*checkme).whatIsNaughty += i;
			(*checkme).whatIsNaughtyLog = (*checkme).whatIsNaughty;
			(*checkme).isItNaughty = true;
			(*checkme).whatIsNaughtyCategories = (*o.lm.l[(*o.fg[filtergroup]).banned_url_list]).lastcategory.toCharArray();
		}
		else if (!igsl && !igul && ((j = (*o.fg[filtergroup]).inBannedRegExpURLList(temp)) >= 0)) {
			(*checkme).isItNaughty = true;
			(*checkme).whatIsNaughtyLog = o.language_list.getTranslation(503);
			// Banned Regular Expression URL:
			(*checkme).whatIsNaughtyLog += (*o.fg[filtergroup]).banned_regexpurl_list_source[j].toCharArray();
			(*checkme).whatIsNaughty = o.language_list.getTranslation(504);
			// Banned Regular Expression URL found.
			(*checkme).whatIsNaughtyCategories = (*o.lm.l[(*o.fg[filtergroup]).banned_regexpurl_list_ref[j]]).category.toCharArray();
		}
		else if ((o.max_upload_size > -1) && (*header).isPostUpload())
		{
#ifdef DGDEBUG
			std::cout << "is post upload" << std::endl;
#endif
			if (o.max_upload_size == 0) {
				(*checkme).whatIsNaughty = o.language_list.getTranslation(700);
				// Web upload is banned.
				(*checkme).whatIsNaughtyLog = (*checkme).whatIsNaughty;
				(*checkme).whatIsNaughtyCategories = "Web upload.";
				(*checkme).isItNaughty = true;
				(*ispostblock) = true;
			}
			else if ((*header).contentLength() > o.max_upload_size) {
				(*checkme).whatIsNaughty = o.language_list.getTranslation(701);
				// Web upload limit exceeded.
				(*checkme).whatIsNaughtyLog = (*checkme).whatIsNaughty;
				(*checkme).whatIsNaughtyCategories = "Web upload.";
				(*checkme).isItNaughty = true;
				(*ispostblock) = true;
			}
		}
	}
	// look for URLs within URLs - ban, for example, images originating from banned sites during a Google image search.
	if (!(*checkme).isItNaughty && (*o.fg[filtergroup]).deep_url_analysis == 1) {
#ifdef DGDEBUG
		std::cout << "starting deep analysis" << std::endl;
#endif
		String deepurl = temp.after("p://");
		while (deepurl.contains(":")) {
			deepurl = deepurl.after(":");
			while (deepurl.startsWith(":") || deepurl.startsWith("/")) {
				deepurl.lop();
			}
#ifdef DGDEBUG
			std::cout << "deep analysing: " << deepurl << std::endl;
#endif
			if (!igsl && !igul && ((i = (*o.fg[filtergroup]).inBannedSiteList(deepurl)) != NULL)) {
				(*checkme).whatIsNaughty = o.language_list.getTranslation(500); // banned site
				(*checkme).whatIsNaughty += i;
				(*checkme).whatIsNaughtyLog = (*checkme).whatIsNaughty;
				(*checkme).isItNaughty = true;
				(*checkme).whatIsNaughtyCategories = (*o.lm.l[(*o.fg[filtergroup]).banned_site_list]).lastcategory.toCharArray();
#ifdef DGDEBUG
				std::cout << "deep site: " << deepurl << std::endl;
#endif
			}
			else if (!igsl && !igul && ((i = (*o.fg[filtergroup]).inBannedURLList(deepurl)) != NULL)) {
				(*checkme).whatIsNaughty = o.language_list.getTranslation(501);
				 // Banned URL:
				(*checkme).whatIsNaughty += i;
				(*checkme).whatIsNaughtyLog = (*checkme).whatIsNaughty;
				(*checkme).isItNaughty = true;
				(*checkme).whatIsNaughtyCategories = (*o.lm.l[(*o.fg[filtergroup]).banned_url_list]).lastcategory.toCharArray();
#ifdef DGDEBUG
				std::cout << "deep url: " << deepurl << std::endl;
#endif
			}
		}
#ifdef DGDEBUG
		std::cout << "done deep analysis" << std::endl;
#endif
	}
}

// based on patch by Aecio F. Neto (afn@harvest.com.br) - Harvest Consultoria (http://www.harvest.com.br)
// show the relevant banned page/image/CGI based on report level setting, request type etc.
bool ConnectionHandler::denyAccess(Socket * peerconn, Socket * proxysock, HTTPHeader * header, HTTPHeader * docheader,
	String * url, NaughtyFilter * checkme, std::string * clientuser, std::string * clientip, int filtergroup,
	bool ispostblock, int headersent, bool wasinfected, bool scanerror)
{
	try {  // writestring throws exception on error/timeout

		// flags to enable filter/infection bypass hash generation
		bool filterhash = false;
		bool virushash = false;
		// flag to enable internal generation of hashes (i.e. obey the "-1" setting; to allow the modes but disable hash generation)
		// (if disabled, just output '1' or '2' to show that the CGI should generate a filter/virus bypass hash;
		// otherwise, real hashes get put into substitution variables/appended to the ban CGI redirect URL)
		bool dohash = false;
		if (o.reporting_level > 0) {
			// generate a filter bypass hash
			if (!wasinfected && ((*o.fg[filtergroup]).bypass_mode != 0) && !ispostblock) {
#ifdef DGDEBUG
				std::cout << "Enabling filter bypass hash generation" << std::endl;
#endif
				filterhash = true;
				if (o.fg[filtergroup]->bypass_mode > 0)
					dohash = true;
			}
			// generate an infection bypass hash
			else if (wasinfected && (*o.fg[filtergroup]).infection_bypass_mode != 0) {
				// only generate if scanerror (if option to only bypass scan errors is enabled)
				if ((*o.fg[filtergroup]).infection_bypass_errors_only ? scanerror : true) {
#ifdef DGDEBUG
					std::cout << "Enabling infection bypass hash generation" << std::endl;
#endif
					virushash = true;
					if (o.fg[filtergroup]->infection_bypass_mode > 0)
						dohash = true;
				}
			}
		}

		// the user is using the full whack of custom banned images and/or HTML templates
		if (o.reporting_level == 3 || (headersent > 0 && o.reporting_level > 0)) {
			// if reporting_level = 1 or 2 and headersent then we can't
			// send a redirect so we have to display the template instead

			(*proxysock).close();  // finished with proxy
			(*peerconn).readyForOutput(10);
			if ((*header).requestType().startsWith("CONNECT")) {

				// if it's a CONNECT then headersent can't be set
				// so we don't need to worry about it

				// if preemptive banning is not in place then a redirect
				// is not guaranteed to ban the site so we have to write
				// an access denied page.  Unfortunately IE does not
				// work with access denied pages on SSL more than a few
				// hundred bytes so we have to use a crap boring one
				// instead.  Nothing can be done about it - blame
				// mickysoft.

				String writestring = "HTTP/1.0 403 ";
				writestring += o.language_list.getTranslation(500);  // banned site
				writestring += "\nContent-Type: text/html\n\n<HTML><HEAD><TITLE>DansGuardian - ";
				writestring += o.language_list.getTranslation(500);  // banned site
				writestring += "</TITLE></HEAD><BODY><H1>DansGuardian - ";
				writestring += o.language_list.getTranslation(500);  // banned site
				writestring += "</H1>";
				writestring += (*url);
				writestring += "</BODY></HTML>\n";

				try {	// writestring throws exception on error/timeout
					(*peerconn).writeString(writestring.toCharArray());
				}
				catch(exception & e) {
				}
			} else {
				// we're dealing with a non-SSL'ed request, and have the option of using the custom banned image/page directly
				bool replaceimage = false;
				if (o.use_custom_banned_image == 1) {

					// It would be much nicer to do a mime comparison
					// and see if the type is image/* but the header
					// never (almost) gets back from squid because
					// it gets denied before then.
					// This method is prone to over image replacement
					// but will work most of the time.

					String lurl = (*url);
					lurl.toLower();
					if (lurl.endsWith(".gif") || lurl.endsWith(".jpg") || lurl.endsWith(".jpeg") || lurl.endsWith(".jpe")
						|| lurl.endsWith(".png") || lurl.endsWith(".bmp") || (*docheader).isContentType("image/"))
					{
						replaceimage = true;
					}
				}
				
				// if we're denying an image request, show the image; otherwise, show the HTML page.
				// (or advanced ad block page, or HTML page with bypass URLs)
				if (replaceimage) {
					if (headersent == 0) {
						(*peerconn).writeString("HTTP/1.0 200 OK\n");
					}
					o.banned_image.display(peerconn);
				} else {
					// advanced ad blocking - if category contains ADs, wrap ad up in an "ad blocked" message,
					// which provides a link to the original URL if you really want it. primarily
					// for IFRAMEs, which will end up containing this link instead of the ad (standard non-IFRAMEd
					// ad images still get image-replaced.)
					if (strstr(checkme->whatIsNaughtyCategories.c_str(), "ADs") != NULL) {
						String writestring = "HTTP/1.0 200 ";
						writestring += o.language_list.getTranslation(1101); // advert blocked
						writestring += "\nContent-Type: text/html\n\n<HTML><HEAD><TITLE>Guardian - ";
						writestring += o.language_list.getTranslation(1101); // advert blocked
						writestring += "</TITLE></HEAD><BODY><CENTER><FONT SIZE=\"-1\"><A HREF=\"";
						writestring += (*url);
						writestring += "\" TARGET=\"_BLANK\">";
						writestring += o.language_list.getTranslation(1101); // advert blocked
						writestring += "</A></FONT></CENTER></BODY></HTML>\n";
						try { // writestring throws exception on error/timeout
							(*peerconn).writeString(writestring.toCharArray());
						} catch (exception& e) {}
					}
					
					// Mod by Ernest W Lessenger Mon 2nd February 2004
					// Other bypass code mostly written by Ernest also
					// create temporary bypass URL to show on denied page
					else {
						String hashed;
						// generate valid hash locally if enabled
						if (dohash) {
							hashed = hashedURL(url, filtergroup, clientip, virushash);
						}
						// otherwise, just generate flags showing what to generate
						else if (filterhash) {
							hashed = "HASH=1";
						}
						else if (virushash) {
							hashed = "HASH=2";
						}

						if (headersent == 0) {
							(*peerconn).writeString("HTTP/1.0 200 OK\n");
						}
						if (headersent < 2) {
							(*peerconn).writeString("Content-type: text/html\n\n");
						}
						// if the header has been sent then likely displaying the
						// template will break the download, however as this is
						// only going to be happening if the unsafe trickle
						// buffer method is used and we want the download to be
						// broken we don't mind too much
						o.fg[filtergroup]->getHTMLTemplate()->display(peerconn,
							url, (*checkme).whatIsNaughty, (*checkme).whatIsNaughtyLog,
							// grab either the full category list or the thresholded list
							(checkme->usedisplaycats ? checkme->whatIsNaughtyDisplayCategories : checkme->whatIsNaughtyCategories),
							clientuser, clientip, clienthost, filtergroup + 1, hashed);
					}
				}
			}
		}
		
		// the user is using the CGI rather than the HTML template - so issue a redirect with parameters filled in on GET string
		else if (o.reporting_level > 0) {
			// grab either the full category list or the thresholded list
			std::string cats;
			cats = checkme->usedisplaycats ? checkme->whatIsNaughtyDisplayCategories : checkme->whatIsNaughtyCategories;

			String hashed;
			// generate valid hash locally if enabled
			if (dohash) {
				hashed = hashedURL(url, filtergroup, clientip, virushash);
			}
			// otherwise, just generate flags showing what to generate
			else if (filterhash) {
				hashed = "1";
			}
			else if (virushash) {
				hashed = "2";
			}

			(*proxysock).close();  // finshed with proxy
			(*peerconn).readyForOutput(10);
			if ((*checkme).whatIsNaughty.length() > 2048) {
				(*checkme).whatIsNaughty = String((*checkme).whatIsNaughty.c_str()).subString(0, 2048).toCharArray();
			}
			if ((*checkme).whatIsNaughtyLog.length() > 2048) {
				(*checkme).whatIsNaughtyLog = String((*checkme).whatIsNaughtyLog.c_str()).subString(0, 2048).toCharArray();
			}
			String writestring = "HTTP/1.0 302 Redirect\n";
			writestring += "Location: ";
			writestring += o.fg[filtergroup]->access_denied_address;

			if (o.non_standard_delimiter == 1) {
				writestring += "?DENIEDURL==";
				writestring += miniURLEncode((*url).toCharArray()).c_str();
				writestring += "::IP==";
				writestring += (*clientip).c_str();
				writestring += "::USER==";
				writestring += (*clientuser).c_str();
				if (clienthost != NULL) {
					writestring += "::HOST==";
					writestring += clienthost->c_str();
				}
				writestring += "::CATEGORIES==";
				writestring += miniURLEncode(cats.c_str()).c_str();
				if (virushash || filterhash) {
					// output either a genuine hash, or just flags
					if (dohash) {
						writestring += "::";
						writestring += hashed.before("=").toCharArray();
						writestring += "==";
						writestring += hashed.after("=").toCharArray();
					} else {
						writestring += "::HASH==";
						writestring += hashed.toCharArray();
					}
				}
				writestring += "::REASON==";
			} else {
				writestring += "?DENIEDURL=";
				writestring += miniURLEncode((*url).toCharArray()).c_str();
				writestring += "&IP=";
				writestring += (*clientip).c_str();
				writestring += "&USER=";
				writestring += (*clientuser).c_str();
				if (clienthost != NULL) {
					writestring += "&HOST=";
					writestring += clienthost->c_str();
				}
				writestring += "&CATEGORIES=";
				writestring += miniURLEncode(cats.c_str()).c_str();
				if (virushash || filterhash) {
					// output either a genuine hash, or just flags
					if (dohash) {
						writestring += "::";
						writestring += hashed.before("=").toCharArray();
						writestring += "==";
						writestring += hashed.after("=").toCharArray();
					} else {
						writestring += "::HASH==";
						writestring += hashed.toCharArray();
					}
				}
				writestring += "&REASON=";
			}
			if (o.reporting_level == 1) {
				writestring += miniURLEncode((*checkme).whatIsNaughty.c_str()).c_str();
			} else {
				writestring += miniURLEncode((*checkme).whatIsNaughtyLog.c_str()).c_str();
			}
			writestring += "\n\n";
			(*peerconn).writeString(writestring.toCharArray());
#ifdef DGDEBUG			// debug stuff surprisingly enough
			std::cout << "******* redirecting to:" << std::endl;
			std::cout << writestring << std::endl;
			std::cout << "*******" << std::endl;
#endif
		}
		
		// the user is using the barebones banned page
		else if (o.reporting_level == 0) {
			(*proxysock).close();  // finshed with proxy
			String writestring = "HTTP/1.0 200 OK\n";
			writestring += "Content-type: text/html\n\n";
			writestring += "<HTML><HEAD><TITLE>DansGuardian - ";
			writestring += o.language_list.getTranslation(1);  // access denied
			writestring += "</TITLE></HEAD><BODY><CENTER><H1>DansGuardian - ";
			writestring += o.language_list.getTranslation(1);  // access denied
			writestring += "</H1></CENTER></BODY></HTML>";
			(*peerconn).readyForOutput(10);
			(*peerconn).writeString(writestring.toCharArray());
#ifdef DGDEBUG			// debug stuff surprisingly enough
			std::cout << "******* displaying:" << std::endl;
			std::cout << writestring << std::endl;
			std::cout << "*******" << std::endl;
#endif
		}
		
		// stealth mode
		else if (o.reporting_level == -1) {
			(*checkme).isItNaughty = false;  // dont block
		}
	}
	catch(exception & e) {
	}
	
	// we blocked the request, so flush the client connection & close the proxy connection.
	if ((*checkme).isItNaughty) {
		try {
			(*peerconn).readyForOutput(10);  //as best a flush as I can
		}
		catch(exception & e) {
		}
		(*proxysock).close();  // close connection to proxy
		// we said no to the request, so return true, indicating exit the connhandler
		return true;  
	}
	return false;
}

// do content scanning (AV filtering) and naughty filtering
void ConnectionHandler::contentFilter(HTTPHeader *docheader, HTTPHeader *header, DataBuffer *docbody,
	Socket *proxysock, Socket *peerconn, int *headersent, bool *pausedtoobig, int *docsize, NaughtyFilter *checkme,
	bool runav, bool wasclean, bool cachehit, int filtergroup, std::deque<bool> *sendtoscanner,
	std::string *clientuser, std::string *clientip, bool *wasinfected, bool *wasscanned, bool isbypass,
	String &url, String &domain, bool *scanerror, bool &contentmodified)
{
	proxysock->checkForInput(120);
	bool compressed = docheader->isCompressed();
	if (compressed) {
#ifdef DGDEBUG
		std::cout << "Decompressing as we go....." << std::endl;
#endif
		docbody->setDecompress(docheader->contentEncoding());
	}
#ifdef DGDEBUG
	std::cout << docheader->contentEncoding() << std::endl;
	std::cout << "about to get body from proxy" << std::endl;
#endif
	(*pausedtoobig) = docbody->in(proxysock, peerconn, header, docheader, runav, headersent);  // get body from proxy

#ifdef DGDEBUG
	if ((*pausedtoobig)) {
		std::cout << "got PARTIAL body from proxy" << std::endl;
	} else {
		std::cout << "got body from proxy" << std::endl;
	}
#endif
	unsigned int dblen;
	bool isfile = false;
	if (docbody->tempfilesize > 0) {
		dblen = docbody->tempfilesize;
		isfile = true;
	} else {
		dblen = docbody->buffer_length;
	}
	// don't scan zero-length buffers (waste of AV resources, especially with external scanners (ICAP)).
	// these were encountered browsing opengroup.org, caused by a stats script. (PRA 21/09/2005)
	// if we wanted to honour a hypothetical min_content_scan_size, we'd do it here.
	if (((*docsize) = (signed) dblen) == 0) {
#ifdef DGDEBUG
		std::cout << "Not scanning zero-length body" << std::endl;
#endif
		// it's not inconceivable that we received zlib or gzip encoded content
		// that is, after decompression, zero length. we need to cater for this.
		// seen on SW's internal MediaWiki.
		if (compressed)
			docheader->removeEncoding(0);
		return;
	}

	if (!wasclean) {	// was not clean or no urlcache

		// fixed to obey maxcontentramcachescansize
		if (runav && (isfile ? dblen <= o.max_content_filecache_scan_size : dblen <= o.max_content_ramcache_scan_size)) {
			(*wasscanned) = true;
			int csrc = 0;
			std::deque<bool>::iterator j = sendtoscanner->begin();
#ifdef DGDEBUG
			int k = 0;
#endif
			for (std::deque<Plugin *>::iterator i = o.csplugins_begin; i != o.csplugins_end; i++) {
				if (*j) {
					if (isfile) {
#ifdef DGDEBUG
						std::cout << "Running scanFile" << std::endl;
#endif
						csrc = ((CSPlugin*)(*i))->scanFile(header, docheader, clientuser->c_str(), filtergroup, clientip->c_str(), docbody->tempfilepath.toCharArray());
						if (csrc != DGCS_OK) {
							unlink(docbody->tempfilepath.toCharArray());
							// delete infected (or unscanned due to error) file straight away
						}
					} else {
#ifdef DGDEBUG
						std::cout << "Running scanMemory" << std::endl;
#endif
						csrc = ((CSPlugin*)(*i))->scanMemory(header, docheader, clientuser->c_str(), filtergroup, clientip->c_str(), docbody->data, docbody->buffer_length);
					}
#ifdef DGDEBUG
					std::cerr << "AV scan " << k << " returned:" << csrc << std::endl;
#endif
					if (csrc < 0) {
						syslog(LOG_ERR, "scanFile/Memory returned error: %d", csrc);
						//TODO: have proper error checking/reporting here?
						//at the very least, integrate with the translation system.
						checkme->whatIsNaughty = "WARNING: Could not perform virus scan!";
						checkme->whatIsNaughtyLog = ((CSPlugin*)(*i))->getLastMessage().toCharArray();
						checkme->whatIsNaughtyCategories = "Content scanning";
						checkme->isItNaughty = true;
						checkme->isException = false;
						(*wasinfected) = true;
						(*scanerror) = true;
						break;
					}
					else if (csrc > 0) {
						checkme->whatIsNaughty = o.language_list.getTranslation(1100);
						String virname = ((CSPlugin*)(*i))->getLastVirusName();

						if (virname.length() > 0) {
							checkme->whatIsNaughty += " ";
							checkme->whatIsNaughty += virname.toCharArray();
						}
						checkme->whatIsNaughtyLog = checkme->whatIsNaughty;
						checkme->whatIsNaughtyCategories = "Content scanning";
						checkme->isItNaughty = true;
						checkme->isException = false;
						(*wasinfected) = true;
						(*scanerror) = false;
						break;
					}
				}
				j++;
#ifdef DGDEBUG
				k++;
#endif
			}

#ifdef DGDEBUG
			std::cout << "finished running AV" << std::endl;
			system("date");
#endif
		}
#ifdef DGDEBUG
		else if (runav) {
			std::cout << "content length large so skipping content scanning (virus) filtering" << std::endl;
		}
		system("date");
#endif
		if (!checkme->isItNaughty && !checkme->isException && !isbypass && !docheader->authRequired()) {
			if (dblen <= o.max_content_filter_size) {
				checkme->checkme(docbody, url, domain);  // content filtering
			}
#ifdef DGDEBUG
			else {
				std::cout << "content length large so skipping content filtering" << std::endl;
			}
			system("date");
#endif
		}
#ifdef DGDEBUG
		else {
			std::cout << "exception or isnaughty (av infected) so skipping content filtering" << std::endl;
		}
#endif
	}

	// don't do phrase filtering or content replacement on exception/bypass accesses
	if (checkme->isException || isbypass) {
		// don't forget to swap back to compressed!
		docbody->swapbacktocompressed();
		return;
	}

	if (dblen <= o.max_content_filter_size && docheader->isContentType("text") && !checkme->isItNaughty) {
		contentmodified = docbody->contentRegExp(filtergroup);
		// content modifying uses global variable
	}
#ifdef DGDEBUG
	else {
		std::cout << "Skipping content modification: ";
		if (dblen > o.max_content_filter_size)
			std::cout << "Content too large";
		else if (!docheader->isContentType("text"))
			std::cout << "Not text";
		else if (checkme->isItNaughty)
			std::cout << "Already flagged as naughty";
		std::cout << std::endl;
	}
	system("date");
#endif

	if (contentmodified) {	// this would not include infected/cured files
		// if the content was modified then it must have fit in ram so no
		// need to worry about swapped to disk stuff
#ifdef DGDEBUG
		std::cout << "content modification made" << std::endl;
#endif
		if (compressed) {
			docheader->removeEncoding(docbody->buffer_length);
			// need to modify header to mark as not compressed
			// it also modifies Content-Length as well
		} else {
			docheader->setContentLength(docbody->buffer_length);
		}
	} else {
		docbody->swapbacktocompressed();
		// if we've not modified it might as well go back to
		// the original compressed version (if there) and send
		// that to the browser
	}
}
