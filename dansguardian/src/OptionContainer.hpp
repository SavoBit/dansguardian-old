//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@jadeb.com).
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

#ifndef __HPP_OPTIONCONTAINER
#define __HPP_OPTIONCONTAINER


// INCLUDES

#include "platform.h"

#include "DownloadManager.hpp"
#include "ContentScanner.hpp"
#include "String.hpp"
#include "HTMLTemplate.hpp"
#include "ListContainer.hpp"
#include "ListManager.hpp"
#include "FOptionContainer.hpp"
#include "LanguageContainer.hpp"
#include "ImageContainer.hpp"
#include "RegExp.hpp"
#include "Auth.hpp"

#include <deque>


// DECLARATIONS

class OptionContainer
{
public:
	// all our many, many options
	int filter_groups;
	int log_exception_hits;
	int non_standard_delimiter;
	int log_file_format;
	int weighted_phrase_mode;
	int show_weighted_found;
	int forwarded_for;
	int createlistcachefiles;
	int use_custom_banned_image;
	std::string custom_banned_image_file;
	int reverse_lookups;
	int reverse_client_ip_lookups;
	int log_client_hostnames;
	int use_xforwardedfor;
	//int preemptive_banning;
	int logconerror;
	int url_cache_number;
	int url_cache_age;
	int phrase_filter_mode;
	int preserve_case;
	int hex_decode_content;
	int force_quick_search;
	int filter_port;
	int proxy_port;
	std::string proxy_ip;
	std::deque<String> filter_ip;
	int ll;
	int reporting_level;
	int max_children;
	int min_children;
	int maxspare_children;
	int prefork_children;
	int minspare_children;
	int maxage_children;
	std::string daemon_user_name;
	std::string daemon_group_name;
	int proxy_user;
	int proxy_group;
	int root_user;

	int max_ips;
	bool recheck_replaced_urls;
	bool use_filter_groups_list;
	bool use_group_names_list;
	bool no_auth_enabled;

	std::string languagepath;
	std::string filter_groups_list_location;
	std::string html_template_location;
	std::string banned_ip_list_location;
	std::string exceptions_ip_list_location;
	std::string language_list_location;
	std::string access_denied_address;
	std::string log_location;
	std::string stat_location;
	std::string ipc_filename;
	std::string urlipc_filename;
	std::string ipipc_filename;
	std::string pid_filename;
	std::string group_names_list_location;
	int no_daemon;
	int no_logger;
	unsigned int max_logitem_length;
	int anonymise_logs;
	int log_ad_blocks;
	int soft_restart;
	std::string daemon_user;
	std::string daemon_group;
	int max_upload_size;
	unsigned int max_content_filter_size;
	unsigned int max_content_ramcache_scan_size;
	unsigned int max_content_filecache_scan_size;
	int scan_clean_cache;
	int content_scan_exceptions;
	int delete_downloaded_temp_files;
	std::string download_dir;
	int initial_trickle_delay;
	int trickle_delay;
	int content_scanner_timeout;

	HTMLTemplate html_template;
	ListContainer filter_groups_list;
	ListContainer exception_ip_list;
	ListContainer banned_ip_list;
	LanguageContainer language_list;
	ImageContainer banned_image;

	std::deque<Plugin*> dmplugins;
	std::deque<Plugin*> csplugins;
	std::deque<Plugin*> authplugins;
	std::deque<Plugin*>::iterator dmplugins_begin;
	std::deque<Plugin*>::iterator dmplugins_end;
	std::deque<Plugin*>::iterator csplugins_begin;
	std::deque<Plugin*>::iterator csplugins_end;
	std::deque<Plugin*>::iterator authplugins_begin;
	std::deque<Plugin*>::iterator authplugins_end;

	ListManager lm;
	FOptionContainer **fg;
	int numfg;

	// access denied domain (when using the CGI)
	String access_denied_domain;

	bool loadCSPlugins();
	bool loadAuthPlugins();
	void deletePlugins(std::deque<Plugin*> &list);
	void deleteFilterGroups();

	//...and the functions that read them

	OptionContainer();
	~OptionContainer();
	bool read(const char *filename, int type);
	void reset();
	bool inExceptionIPList(const std::string *ip, std::string *&host);
	bool inBannedIPList(const std::string *ip, std::string *&host);
	bool readFilterGroupConf();
	// public so fc_controlit can reload filter group config files
	bool doReadItemList(const char *filename, ListContainer *lc, const char *fname, bool swsort);

private:
	std::deque<std::string> conffile;
	String conffilename;

	bool loadDMPlugins();

	bool precompileregexps();
	int findoptionI(const char *option);
	std::string findoptionS(const char *option);
	bool realitycheck(String s, int minl, int maxl, char *emessage);
	bool readAnotherFilterGroupConf(const char *filename, const char *groupname);
	std::deque<String> findoptionM(const char *option);

	bool inIPList(const std::string *ip, ListContainer& list, std::string *&host);
};

#endif
