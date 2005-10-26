//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@//jadeb/.com).
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

#include "OptionContainer.hpp"
#include "RegExp.hpp"
#include "ConfigVar.hpp"

#include <iostream>
#include <fstream>
#include <netdb.h>		// for gethostby
#include <netinet/in.h>		// for address structures
#include <arpa/inet.h>		// for inet_aton()
#include <sys/socket.h>
#include <syslog.h>

#include <unistd.h>		// checkme: remove?


// GLOBALS

extern bool is_daemonised;


// IMPLEMENTATION

OptionContainer::OptionContainer():numfg(0)
{
}

OptionContainer::~OptionContainer()
{
	deleteFilterGroups();
	deleteDMPlugins();
	deleteCSPlugins();
}

void OptionContainer::reset()
{
	deleteFilterGroups();
	deleteDMPlugins();
	deleteCSPlugins();
	exception_user_list.reset();
	exception_ip_list.reset();
	banned_ip_list.reset();
	banned_user_list.reset();
	html_template.reset();
	language_list.reset();
	conffile.clear();
	filter_groups_list.reset();
	filter_ip.clear();
}

void OptionContainer::deleteFilterGroups()
{
	for (int i = 0; i < numfg; i++) {
		if (fg[i] != NULL) {
			delete fg[i];  // delete extra FOptionContainer objects
			fg[i] = NULL;
		}
	}
	if (numfg > 0) {
		delete[]fg;
		numfg = 0;
	}
}

void OptionContainer::deleteCSPlugins()
{
	for (unsigned int i = 0; i < csplugins.size(); i++) {
		if (csplugins[i] != NULL && cspluginloaders.size() < i) {
			csplugins[i]->quit();
			cspluginloaders[i].destroy(csplugins[i]);
			csplugins[i] = NULL;
		}
	}
	csplugins.clear();
	cspluginloaders.clear();
}


void OptionContainer::deleteDMPlugins()
{
	for (unsigned int i = 0; i < dmplugins.size(); i++) {
		if (dmplugins[i] != NULL && dmpluginloaders.size() < i) {
			dmplugins[i]->quit();
			dmpluginloaders[i].destroy(dmplugins[i]);
			dmplugins[i] = NULL;
		}
	}
	dmplugins.clear();
	dmpluginloaders.clear();
	dmplugins_regexp.clear();
}


bool OptionContainer::read(const char *filename, int type)
{
	conffilename = filename;
	try {			// all sorts of exceptions could occur reading conf files
		std::string linebuffer;
		String temp;  // for tempory conversion and storage
		int j;  // counter
		ifstream conffiles(filename, ios::in);  // dansguardian.conf
		if (!conffiles.good()) {
			if (!is_daemonised) {
				std::cerr << "error reading: " << filename << std::endl;
			}
			syslog(LOG_ERR, "%s", "error reading dansguardian.conf");
			return false;
		}
		while (!conffiles.eof()) {
			getline(conffiles, linebuffer);
			if (!conffiles.eof() && linebuffer.length() != 0) {
				if (linebuffer[0] != '#') {	// i.e. not commented out
					for (j = 0; j < (signed) linebuffer.length(); j++) {
						linebuffer[j] = tolower(linebuffer[j]);
					}
					temp = (char *) linebuffer.c_str();
					if (temp.contains("#")) {
						temp = temp.before("#");
					}
					temp.removeWhiteSpace();  // get rid of spaces at end of line
					linebuffer = temp.toCharArray();
					conffile.push_back(linebuffer);  // stick option in deque
				}
			}
		}
		conffiles.close();

		if (type == 0 || type == 2) {

			if ((ipc_filename = findoptionS("ipcfilename")) == "")
				ipc_filename = "/tmp/.dguardianipc";

			if ((urlipc_filename = findoptionS("urlipcfilename")) == "")
				urlipc_filename = "/tmp/.dguardianurlipc";

			if ((ipipc_filename = findoptionS("ipipcfilename")) == "")
				ipipc_filename = "/tmp/.dguardianipipc";

			if ((pid_filename = findoptionS("pidfilename")) == "") {
				pid_filename = __PIDDIR;
				pid_filename += "dansguardian.pid";
			}

			if ((log_location = findoptionS("loglocation")) == "") {
				log_location = __LOGLOCATION;
				log_location += "access.log";
			}

			if (type == 0) {
				return true;
			}
		}

		if ((daemon_user_name = findoptionS("daemonuser")) == "") {
			daemon_user_name = __PROXYUSER;
		}

		if ((daemon_group_name = findoptionS("daemongroup")) == "") {
			daemon_group_name = __PROXYGROUP;
		}

		if (findoptionS("nodaemon") == "on") {
			no_daemon = 1;
		} else {
			no_daemon = 0;
		}

		if (findoptionS("nologger") == "on") {
			no_logger = 1;
		} else {
			no_logger = 0;
		}

		if (findoptionS("softrestart") == "on") {
			soft_restart = 1;
		} else {
			soft_restart = 0;
		}


		// the dansguardian.conf and pics files get amalgamated into one
		// deque.  They are only seperate files for clarity.

		max_logitem_length = findoptionI("maxlogitemlength");
		if (!realitycheck(String(max_logitem_length), 1, 4, "maxlogitemlength")) {
			return false;
		}
		max_children = findoptionI("maxchildren");
		if (!realitycheck(String(max_children), 1, 3, "maxchildren")) {
			return false;
		}		// check its a reasonable value
		min_children = findoptionI("minchildren");
		if (!realitycheck(String(min_children), 1, 2, "children")) {
			return false;
		}		// check its a reasonable value
		maxspare_children = findoptionI("maxsparechildren");
		if (!realitycheck(String(maxspare_children), 1, 2, "children")) {
			return false;
		}		// check its a reasonable value
		if (maxspare_children < min_children) {
			maxspare_children = min_children;
		}
		prefork_children = findoptionI("preforkchildren");
		if (!realitycheck(String(prefork_children), 1, 2, "children")) {
			return false;
		}		// check its a reasonable value
		minspare_children = findoptionI("minsparechildren");
		if (!realitycheck(String(minspare_children), 1, 2, "children")) {
			return false;
		}		// check its a reasonable value
		maxage_children = findoptionI("maxagechildren");
		if (!realitycheck(String(maxage_children), 1, 6, "children")) {
			return false;
		}		// check its a reasonable value

		max_ips = findoptionI("maxips");
		if (!realitycheck(String(max_ips), 1, 5, "maxips")) {
			return false;
		}

		max_upload_size = findoptionI("maxuploadsize") * 1024;
		if (!realitycheck(String(max_upload_size), 1, 8, "maxuploadsize")) {
			return false;
		}		// check its a reasonable value
		max_content_filter_size = findoptionI("maxcontentfiltersize") * 1024;
		if (!realitycheck(String((signed) max_content_filter_size), 1, 8, "maxcontentfiltersize")) {
			return false;
		}		// check its a reasonable value
		max_content_ramcache_scan_size = findoptionI("maxcontentramcachescansize") * 1024;
		if (!realitycheck(String((signed) max_content_ramcache_scan_size), 1, 8, "maxcontentramcachescansize")) {
			return false;
		}

		max_content_filecache_scan_size = findoptionI("maxcontentfilecachescansize") * 1024;
		if (!realitycheck(String((signed) max_content_filecache_scan_size), 1, 8, "maxcontentfilecachescansize")) {
			return false;
		}
		if (max_content_ramcache_scan_size == 0) {
			max_content_ramcache_scan_size = max_content_filecache_scan_size;
		}
		if (max_content_filter_size == 0) {
			max_content_filter_size = max_content_ramcache_scan_size;
		}
		if (max_content_filter_size > max_content_ramcache_scan_size) {
			if (!is_daemonised) {
				std::cerr << "maxcontentfiltersize can not be greater than maxcontentramcachescansize" << std::endl;
			}
			syslog(LOG_ERR, "%s", "maxcontentfiltersize can not be greater than maxcontentramcachescansize");
			return false;
		}
		if (max_content_ramcache_scan_size > max_content_filecache_scan_size) {
			if (!is_daemonised) {
				std::cerr << "maxcontentramcachescansize can not be greater than max_content_filecache_scan_size" << std::endl;
			}
			syslog(LOG_ERR, "%s", "maxcontentramcachescansize can not be greater than max_content_filecache_scan_size");
			return false;
		}

		trickle_delay = findoptionI("trickledelay");
		if (!realitycheck(String(trickle_delay), 1, 6, "trickledelay")) {
			return false;
		}
		initial_trickle_delay = findoptionI("initialtrickledelay");
		if (!realitycheck(String(initial_trickle_delay), 1, 6, "initialtrickledelay")) {
			return false;
		}

		content_scanner_timeout = findoptionI("contentscannertimeout");
		if (!realitycheck(String(content_scanner_timeout), 1, 3, "contentscannertimeout")) {
			return false;
		}

		url_cache_number = findoptionI("urlcachenumber");
		if (!realitycheck(String(url_cache_number), 1, 5, "urlcachenumber")) {
			return false;
		}		// check its a reasonable value

		url_cache_age = findoptionI("urlcacheage");
		if (!realitycheck(String(url_cache_age), 1, 5, "urlcacheage")) {
			return false;
		}		// check its a reasonable value

		if (findoptionS("scancleancache") == "off") {
			scan_clean_cache = 0;
		} else {
			scan_clean_cache = 1;
		}

		if (findoptionS("contentscanexceptions") == "on") {
			content_scan_exceptions = 1;
		} else {
			content_scan_exceptions = 0;
		}

		if (findoptionS("deletedownloadedtempfiles") == "off") {
			delete_downloaded_temp_files = 0;
		} else {
			delete_downloaded_temp_files = 1;
		}

		phrase_filter_mode = findoptionI("phrasefiltermode");
		if (!realitycheck(String(phrase_filter_mode), 1, 1, "phrasefiltermode")) {
			return false;
		}
		preserve_case = findoptionI("preservecase");
		if (!realitycheck(String(preserve_case), 1, 1, "preservecase")) {
			return false;
		}
		hex_decode_content = findoptionI("hexdecodecontent");
		if (!realitycheck(String(hex_decode_content), 1, 1, "hex_decode_content")) {
			return false;
		}
		force_quick_search = findoptionI("forcequicksearch");
		if (!realitycheck(String(force_quick_search), 1, 1, "forcequicksearch")) {
			return false;
		}		// check its a reasonable value

		use_custom_banned_image = findoptionI("usecustombannedimage");
		if (!realitycheck(String(use_custom_banned_image), 1, 1, "usecustombannedimage")) {
			return false;
		}
		custom_banned_image_file = findoptionS("custombannedimagefile");
		banned_image.read(custom_banned_image_file.c_str());

		filter_port = findoptionI("filterport");
		if (!realitycheck(String(filter_port), 2, 6, "filterport")) {
			return false;
		}		// check its a reasonable value
		proxy_port = findoptionI("proxyport");
		if (!realitycheck(String(proxy_port), 2, 6, "proxyport")) {
			return false;
		}		// etc
		proxy_ip = findoptionS("proxyip");
		if (!realitycheck(String(proxy_ip.c_str()), 7, 15, "proxyip")) {
			return false;
		}

		// multiple listen IP support
		filter_ip = findoptionM("filterip");
		if (filter_ip.size() > 127) {
			if (!is_daemonised) {
				std::cerr << "Can not listen on more than 127 IPs" << std::endl;
			}
			syslog(LOG_ERR, "%s", "Can not listen on more than 127 IPs");
			return false;
		}

		ll = findoptionI("loglevel");
		if (!realitycheck(ll, 1, 1, "loglevel")) {
			return false;
		}		// etc
		log_file_format = findoptionI("logfileformat");
		if (!realitycheck(log_file_format, 1, 1, "logfileformat")) {
			return false;
		}		// etc
		if (log_file_format < 1 || log_file_format > 4) {
			log_file_format = 1;
		}
		if (findoptionS("anonymizelogs") == "on") {
			anonymise_logs = 1;
		} else {
			anonymise_logs = 0;
		}
		if (findoptionS("logadblocks") == "on") {
			log_ad_blocks = 1;
		} else {
			log_ad_blocks = 0;
		}

		if (findoptionS("showweightedfound") == "on") {
			show_weighted_found = 1;
		} else {
			show_weighted_found = 0;
		}
		weighted_phrase_mode = findoptionI("weightedphrasemode");
		if (!realitycheck(String(weighted_phrase_mode), 1, 1, "weightedphrasemode")) {
			return false;
		}
		reporting_level = findoptionI("reportinglevel");
		if (!realitycheck(String(reporting_level), 1, 2, "reportinglevel")) {
			return false;
		}
		html_template_location = findoptionS("languagedir") + "/" + findoptionS("language") + "/template.html";

		if (findoptionS("forwardedfor") == "on") {
			forwarded_for = 1;
		} else {
			forwarded_for = 0;
		}
		if (findoptionS("logexceptionhits") == "on") {
			log_exception_hits = 1;
		} else {
			log_exception_hits = 0;
		}
		if (findoptionS("nonstandarddelimiter") == "off") {
			non_standard_delimiter = 0;
		} else {
			non_standard_delimiter = 1;
		}
		if (findoptionS("createlistcachefiles") == "off") {
			createlistcachefiles = 0;
		} else {
			createlistcachefiles = 1;
		}
		if (findoptionS("logconnectionhandlingerrors") == "on") {
			logconerror = 1;
		} else {
			logconerror = 0;
		}
		if (findoptionS("usernameidmethodproxyauth") == "on") {
			uim_proxyauth = 1;
		} else {
			uim_proxyauth = 0;
		}
		if (findoptionS("usernameidmethodntlm") == "on") {
			uim_ntlm = 1;
		} else {
			uim_ntlm = 0;
		}
		if (findoptionS("usernameidmethodident") == "on") {
			uim_ident = 1;
		} else {
			uim_ident = 0;
		}

		if (findoptionS("preemptivebanning") == "off") {
			preemptive_banning = 0;
		} else {
			preemptive_banning = 1;
		}

		if (findoptionS("reverseaddresslookups") == "on") {
			reverse_lookups = 1;
		} else {
			reverse_lookups = 0;
		}
		if (findoptionS("reverseclientiplookups") == "on") {
			reverse_client_ip_lookups = 1;
		} else {
			reverse_client_ip_lookups = 0;
		}
		if (findoptionS("logclienthostnames") == "on") {
			log_client_hostnames = 1;
		} else {
			log_client_hostnames = 0;
		}

		if (findoptionS("recheckreplacedurls") == "on") {
			recheck_replaced_urls = 1;
		} else {
			recheck_replaced_urls = 0;
		}

		if (findoptionS("usexforwardedfor") == "on") {
			use_xforwardedfor = 1;
		} else {
			use_xforwardedfor = 0;
		}

		filter_groups = findoptionI("filtergroups");

		if (!realitycheck(String(filter_groups), 1, 2, "filtergroups")) {
			return false;
		}
		if (filter_groups < 1) {
			if (!is_daemonised) {
				std::cerr << "filtergroups too small" << std::endl;
			}
			syslog(LOG_ERR, "%s", "filtergroups too small");
			return false;
		}

		if (!loadDMPlugins()) {
			if (!is_daemonised) {
				std::cerr << "Error loading DM plugins" << std::endl;
			}
			syslog(LOG_ERR, "%s", "Error loading DM plugins");
			return false;
		}

		if (!loadCSPlugins()) {
			if (!is_daemonised) {
				std::cerr << "Error loading CS plugins" << std::endl;
			}
			syslog(LOG_ERR, "%s", "Error loading CS plugins");
			return false;
		}

		download_dir = findoptionS("filecachedir");

		filter_groups_list_location = findoptionS("filtergroupslist");
		banned_ip_list_location = findoptionS("bannediplist");
		banned_user_list_location = findoptionS("banneduserlist");
		exceptions_user_list_location = findoptionS("exceptionuserlist");
		exceptions_ip_list_location = findoptionS("exceptioniplist");
		language_list_location = findoptionS("languagedir") + "/" + findoptionS("language") + "/messages";
		access_denied_address = findoptionS("accessdeniedaddress");
		ada = access_denied_address.c_str();
		ada = ada.after("://");
		ada.removeWhiteSpace();
		if (ada.contains("/")) {
			ada = ada.before("/");  // ada now contains the FQ host nom of the
			// server that serves the accessdenied.html
			// file
		}
		if (ada.contains(":")) {
			ada = ada.before(":");  // chop off the port number if any
		}
		if (reporting_level == 1 || reporting_level == 2) {
			if (ada.length() < 4) {
				if (!is_daemonised) {
					cerr << "accessdeniedaddress setting appears to be wrong." << endl;
				}
				syslog(LOG_ERR, "%s", "accessdeniedaddress setting appears to be wrong.");
				return false;
			}
		}

		if (!doReadItemList(filter_groups_list_location.c_str(),&filter_groups_list,"filtergroupslist",true)) {
			return false;
		}

		if (!doReadItemList(banned_ip_list_location.c_str(),&banned_ip_list,"bannediplist",true)) {
			return false;
		}		// read banned ip list
		if (!doReadItemList(banned_user_list_location.c_str(),&banned_user_list,"banneduserlist",true)) {
			return false;
		}		// read banned user list
		if (!doReadItemList(exceptions_ip_list_location.c_str(),&exception_ip_list,"exceptioniplist",false)) {
			return false;
		}		// ip exceptions
		if (!doReadItemList(exceptions_user_list_location.c_str(),&exception_user_list,"exceptionuserlist",false)) {
			return false;
		}		// site exceptions

		if (!language_list.readLanguageList(language_list_location.c_str())) {
			return false;
		}		// messages language file

		if (reporting_level == 3) {	// only if reporting set to HTML templ
			if (!html_template.readTemplateFile(html_template_location.c_str())) {
				if (!is_daemonised) {
					std::cerr << "Error reading HTML Template file:" << html_template_location << std::endl;
				}
				syslog(LOG_ERR, "%s", "Error reading HTML Template file.");
				return false;
				// HTML template file
			}
		}

		if (!readFilterGroupConf()) {
			if (!is_daemonised) {
				std::cerr << "Error reading filter group conf file(s)." << std::endl;
			}
			syslog(LOG_ERR, "%s", "Error reading filter group conf file(s).");
			return false;

		}

	}
	catch(exception & e) {
		if (!is_daemonised) {
			std::cerr << e.what() << std::endl;  // when called the daemon has not
			// detached so we can do this
		}
		return false;
	}
	return true;
}

bool OptionContainer::doReadItemList(const char* filename, ListContainer* lc, const char* fname, bool swsort) {
	bool result = lc->readItemList(filename, false, 0);
	if (!result) {
		if (!is_daemonised) {
			std::cerr << "Error opening " << fname << std::endl;
		}
		syslog(LOG_ERR, "Error opening %s", fname);
		return false;
	}
	if (swsort)
		lc->startsWithSort();
	else
		lc->endsWithSort();
	return true;
}

bool OptionContainer::inExceptionUserList(const std::string *user)
{
	if ((*user).length() < 1) {
		return false;
	}
	return exception_user_list.inList((char *) (*user).c_str());
}

bool OptionContainer::inBannedUserList(const std::string *user)
{
	if ((*user).length() < 1) {
		return false;
	}
	return banned_user_list.inList((char *) (*user).c_str());
}

bool OptionContainer::inIPList(const std::string *ip, ListContainer& list, std::string *&host)
{
	if ((*ip).length() < 1) {
		return false;
	}
	if (reverse_client_ip_lookups != 1) {
		return list.inList((char *) (*ip).c_str());
	}
	if (list.inList((char *) (*ip).c_str())) {
		return true;
	}
	std::deque<String > hostnames = (*fg[0]).ipToHostname((*ip).c_str());
	bool result;
	for (unsigned int i = 0; i < hostnames.size(); i++) {
		result = list.inList(hostnames[i].toCharArray());
		if (result) {
			// return the matched host name for logging purposes
			// hey.. since the lookup is the hard part, not the logging,
			// why not always return a hostname if it wasn't a straight IP
			// that triggered the match?
			//if (log_client_hostnames == 1) {
				delete host;
				host = new std::string(hostnames[i].toCharArray());
#ifdef DGDEBUG
				std::cout<<"Found hostname: "<<(*host)<<std::endl;
#endif
			//}
			return true;
		}
	}
	return false;
}

// checkme: remove these and make inIPList public?

bool OptionContainer::inExceptionIPList(const std::string *ip, std::string *&host)
{
	return inIPList(ip, exception_ip_list, host);
}

bool OptionContainer::inBannedIPList(const std::string *ip, std::string *&host)
{
	return inIPList(ip, banned_ip_list, host);
}


int OptionContainer::findoptionI(const char *option)
{
	int res = String(findoptionS(option).c_str()).toInteger();
	return res;
}


std::string OptionContainer::findoptionS(const char *option)
{
	// findoptionS returns a found option stored in the deque
	String temp;
	String temp2;
	String o = option;
	for (int i = 0; i < (signed) conffile.size(); i++) {
		temp = conffile[i].c_str();
		temp2 = temp.before("=");
		while (temp2.endsWith(" ")) {	// get rid of tailing spaces before =
			temp2.chop();
		}
		if (o == temp2) {
			temp = temp.after("=");
			while (temp.startsWith(" ")) {	// get rid of heading spaces
				temp.lop();
			}
			if (temp.startsWith("'")) {	// inverted commas
				temp.lop();
			}
			while (temp.endsWith(" ")) {	// get rid of tailing spaces
				temp.chop();
			}
			if (temp.endsWith("'")) {	// inverted commas
				temp.chop();
			}
			return temp.toCharArray();
		}
	}
	return "";
}


std::deque<String > OptionContainer::findoptionM(const char *option)
{
	// findoptionS returns all the matching options
	String temp;
	String temp2;
	String o = option;
	std::deque<String > results;
	for (int i = 0; i < (signed) conffile.size(); i++) {
		temp = conffile[i].c_str();
		temp2 = temp.before("=");
		while (temp2.endsWith(" ")) {	// get rid of tailing spaces before =
			temp2.chop();
		}
		if (o == temp2) {
			temp = temp.after("=");
			while (temp.startsWith(" ")) {	// get rid of heading spaces
				temp.lop();
			}
			if (temp.startsWith("'")) {	// inverted commas
				temp.lop();
			}
			while (temp.endsWith(" ")) {	// get rid of tailing spaces
				temp.chop();
			}
			if (temp.endsWith("'")) {	// inverted commas
				temp.chop();
			}
			results.push_back(temp);
		}
	}
	return results;
}


bool OptionContainer::realitycheck(String s, int minl, int maxl, char *emessage)
{
	// realitycheck checks a String for certain expected criteria
	// so we can spot problems in the conf files easier
	if ((signed) s.length() < minl) {
		if (!is_daemonised) {
			std::cerr << emessage << std::endl;
			// when called we have not detached from
			// the console so we can write back an
			// error

			std::cerr << "Too short or missing." << std::endl;
		}
		syslog(LOG_ERR, "%s", emessage);
		syslog(LOG_ERR, "%s", "Too short or missing.");

		return false;
	}
	if ((signed) s.length() > maxl && maxl > 0) {
		if (!is_daemonised) {
			std::cerr << emessage << std::endl;
			std::cerr << "Too long or broken." << std::endl;
		}
		syslog(LOG_ERR, "%s", emessage);
		syslog(LOG_ERR, "%s", "Too long or broken.");
		return false;
	}
	return true;
}



bool OptionContainer::readFilterGroupConf()
{
	String prefix = conffilename;
	prefix = prefix.before(".conf");
	prefix += "f";
	String file;
	for (int i = 1; i <= filter_groups; i++) {
		file = prefix + String(i);
		file += ".conf";
		if (!readAnotherFilterGroupConf(file.toCharArray())) {
			if (!is_daemonised) {
				std::cerr << "Error opening filter list:" << file << std::endl;
			}
			syslog(LOG_ERR, "%s", "Error opening filter list:");
			syslog(LOG_ERR, "%s", file.toCharArray());
			return false;
		}
	}
	return true;
}

bool OptionContainer::readAnotherFilterGroupConf(const char *filename)
{
#ifdef DGDEBUG
	std::cout << "adding filter group" << numfg << " " << filename << std::endl;
#endif

	// array of pointers to FOptionContainer
	typedef FOptionContainer *PFOptionContainer;
	FOptionContainer **temp = new PFOptionContainer[numfg + 1];
	for (int i = 0; i < numfg; i++) {
		temp[i] = fg[i];
	}
	if (numfg > 0) {
		delete[]fg;
	}
	fg = temp;
	fg[numfg] = new FOptionContainer;

#ifdef DGDEBUG
	std::cout << "added filter group" << numfg << " " << filename << std::endl;
#endif

	// pass all the vars from OptionContainer needed
	(*fg[numfg]).weighted_phrase_mode = weighted_phrase_mode;
	(*fg[numfg]).force_quick_search = force_quick_search;
	(*fg[numfg]).createlistcachefiles = createlistcachefiles;
	(*fg[numfg]).reverse_lookups = reverse_lookups;
	(*fg[numfg]).ada = ada;

#ifdef DGDEBUG
	std::cout << "passed variables to filter group" << numfg << " " << filename << std::endl;
#endif

	bool rc = (*fg[numfg]).read(filename);
#ifdef DGDEBUG
	std::cout << "read filter group" << numfg << " " << filename << std::endl;
#endif

	numfg++;

	if (!rc) {
		return false;
	}
	return true;
}

bool OptionContainer::loadDMPlugins()
{
	std::deque<String > dq = findoptionM("downloadmanager");
	unsigned int numplugins = dq.size();
	if (numplugins < 1) {
		if (!is_daemonised) {
			std::cerr << "There must be at least one download manager option" << std::endl;
		}
		syslog(LOG_ERR, "%s", "There must be at least one download manager option");
		return false;
	}
	String match, config;
	RegExp reefer;
	for (unsigned int i = 0; i < numplugins; i++) {
		if (i == (numplugins - 1)) {
			match = ".*";
		} else {
			match = dq[i].before("','");
		}

#ifdef DGDEBUG
		std::cout << "loading download manager config for match:" << match << std::endl;
#endif

		config = dq[i].after("','");
		// worth adding some input checking on config and match

		DMPluginLoader dmpl(config.toCharArray());
		DMPlugin *dmpp;

		if (!dmpl.is_good) {
			if (!is_daemonised) {
				std::cerr << "Error starting download manager plugin loader with config file:" << config << std::endl;
			}
			syslog(LOG_ERR, "%s", "Error starting download manager plugin loader with config file:");
			syslog(LOG_ERR, "%s", config.toCharArray());
			return false;
		}
		dmpp = dmpl.create();

		if (dmpp == NULL) {
			if (!is_daemonised) {
				std::cerr << "dmpl.create() returned NULL pointer with config file:" << config << std::endl;
			}
			syslog(LOG_ERR, "%s", "dmpl.create() returned NULL pointer with config file:");
			syslog(LOG_ERR, "%s", config.toCharArray());
			return false;
		}

		int rc = dmpp->init();

		if (rc < 0) {
			if (!is_daemonised) {
				std::cerr << "Download manager plugin init returned error value:" << rc << std::endl;
			}
			syslog(LOG_ERR, "%s", "Download manager plugin init returned error value.");
			return false;
		}
		else if (rc > 0) {
			if (!is_daemonised) {
				std::cerr << "Download manager plugin init returned warning value:" << rc << std::endl;
			}
			syslog(LOG_ERR, "%s", "Download manager plugin init returned warning value.");
		}

		dmpluginloaders.push_back(dmpl);
		dmplugins.push_back(dmpp);

		dmplugins_regexp.push_back(reefer);
		if (!dmplugins_regexp[dmplugins_regexp.size() - 1].comp(match.toCharArray())) {
			if (!is_daemonised) {
				std::cerr << "Error compiling download manager RegExp:" << match << std::endl;
			}
			syslog(LOG_ERR, "%s", "Error compiling download manager RegExp:");
			syslog(LOG_ERR, "%s", match.toCharArray());
			return false;
		}
	}
	return true;
}


bool OptionContainer::loadCSPlugins()
{
	std::deque<String > dq = findoptionM("contentscanner");
	unsigned int numplugins = dq.size();
	if (numplugins < 1) {
		return true;  // to have one is optional
	}
	String match, config;
	RegExp reefer;
	for (unsigned int i = 0; i < numplugins; i++) {

		config = dq[i];
		// worth adding some input checking on config
#ifdef DGDEBUG
		std::cout << "loading config for:" << config << std::endl;
#endif

		CSPluginLoader cspl(config.toCharArray());

#ifdef DGDEBUG
		std::cout << "CSPluginLoader created" << std::endl;
#endif

		CSPlugin *cspp;

		if (!cspl.is_good) {
			if (!is_daemonised) {
				std::cerr << "Error starting content scanner plugin loader with config file:" << config << std::endl;
			}
			syslog(LOG_ERR, "%s", "Error starting content scanner plugin loader with config file:");
			syslog(LOG_ERR, "%s", config.toCharArray());
			return false;
		}
#ifdef DGDEBUG
		std::cout << "Content scanner plugin loader is good" << std::endl;
#endif
		cspp = cspl.create();
#ifdef DGDEBUG
		std::cout << "Content scanner plugin loader created plugin" << std::endl;
#endif

		if (cspp == NULL) {
			if (!is_daemonised) {
				std::cerr << "cspl.create() returned NULL pointer with config file:" << config << std::endl;
			}
			syslog(LOG_ERR, "%s", "cspl.create() returned NULL pointer with config file:");
			syslog(LOG_ERR, "%s", config.toCharArray());
			return false;
		}
#ifdef DGDEBUG
		std::cout << "Content scanner plugin is good" << std::endl;
#endif

		int rc = cspp->init();

#ifdef DGDEBUG
		std::cout << "Content scanner plugin init called" << std::endl;
#endif

		if (rc < 0) {
			if (!is_daemonised) {
				std::cerr << "Content scanner plugin init returned error value:" << rc << std::endl;
			}
			syslog(LOG_ERR, "%s", "Content scanner plugin init returned error value.");
			return false;
		}
		else if (rc > 0) {
			if (!is_daemonised) {
				std::cerr << "Content scanner plugin init returned warning value:" << rc << std::endl;
			}
			syslog(LOG_ERR, "%s", "Content scanner plugin init returned warning value.");
		}

		cspluginloaders.push_back(cspl);
		csplugins.push_back(cspp);
	}
	return true;
}
