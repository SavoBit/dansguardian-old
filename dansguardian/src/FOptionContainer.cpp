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

#include "platform.h"
#include <syslog.h>
#include "FOptionContainer.hpp"
#include "OptionContainer.hpp"
#include "RegExp.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <netdb.h>  // for gethostby
#include <netinet/in.h>  // for address structures
#include <arpa/inet.h>  // for inet_aton()
#include <sys/socket.h>

//#include <unistd.h>  // remove

extern bool isDaemonised;
extern OptionContainer o;

FOptionContainer::~FOptionContainer() {
    o.lm.deRefList(banned_phrase_list);
    o.lm.deRefList(exception_site_list);
    o.lm.deRefList(exception_url_list);
    o.lm.deRefList(banned_extension_list);
    o.lm.deRefList(banned_mimetype_list);
    o.lm.deRefList(banned_site_list);
    o.lm.deRefList(banned_url_list);
    o.lm.deRefList(grey_site_list);
    o.lm.deRefList(grey_url_list);
    o.lm.deRefList(banned_regexpurl_list);
    o.lm.deRefList(content_regexp_list);
}

void FOptionContainer::reset() {
    o.lm.deRefList(banned_phrase_list);
    o.lm.deRefList(exception_site_list);
    o.lm.deRefList(exception_url_list);
    o.lm.deRefList(banned_extension_list);
    o.lm.deRefList(banned_mimetype_list);
    o.lm.deRefList(banned_site_list);
    o.lm.deRefList(banned_url_list);
    o.lm.deRefList(grey_site_list);
    o.lm.deRefList(grey_url_list);
    o.lm.deRefList(banned_regexpurl_list);
    o.lm.deRefList(content_regexp_list);
    banned_phrase_list_index.clear();
    conffile.clear();
    banned_regexpurl_list_comp.clear();
    content_regexp_list_comp.clear();
    content_regexp_list_rep.clear();
    banned_regexpurl_list_source.clear();
    banned_regexpurl_list_ref.clear();
}

bool FOptionContainer::read(std::string filename) {
    try { // all sorts of exceptions could occur reading conf files
        std::string linebuffer;
        String temp;  // for tempory conversion and storage
        int j;  // counter
        ifstream conffiles(filename.c_str(), ios::in);  // dansguardianfN.conf
        if (!conffiles.good()) {
            if (!isDaemonised) {
                std::cerr << "error reading: " << filename << std::endl;
            }
            syslog(LOG_ERR, "%s","error reading dansguardian.conf");
            return false;
        }
        while (!conffiles.eof()) {
            getline(conffiles, linebuffer);
            if (!conffiles.eof() && linebuffer.length() != 0) {
                if (linebuffer[0] != '#') {  // i.e. not commented out
                    for(j = 0; j < (signed)linebuffer.length(); j++) {
                        linebuffer[j] = tolower(linebuffer[j]);
                    }
                    temp = (char*)linebuffer.c_str();
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


        #ifdef DGDEBUG
            std::cout << "read conf into memory" << filename << std::endl;
        #endif

        // the dansguardian.conf and pics files get amalgamated into one
        // deque.  They are only seperate files for clarity.

        linebuffer = findoptionS("picsfile");
        ifstream picsfiles(linebuffer.c_str(), ios::in); // pics file
        if (!picsfiles.good()) {
            if (!isDaemonised) {
                std::cerr << "error reading: " << linebuffer << std::endl;
            }
            syslog(LOG_ERR, "%s","error reading pics file");
            return false;
        }
        while (!picsfiles.eof()) {
            getline(picsfiles, linebuffer);
            if (!picsfiles.eof() && linebuffer.length() != 0) {
                if (linebuffer[0] != '#') {  // i.e. not commented out
                    temp = (char*)linebuffer.c_str();
                    if (temp.contains("#")) {
                        temp = temp.before("#");
                    }
                    while (temp.endsWith(" ")) {
                        temp.chop();  // get rid of spaces at end of line
                    }
                    linebuffer = temp.toCharArray();
                    conffile.push_back(linebuffer);  // stick option in deque
                }
            }
        }
        picsfiles.close();

        #ifdef DGDEBUG
            std::cout << "read pics into memory" << filename << std::endl;
        #endif

        if (findoptionS("enablePICS") == "off") {
            enable_PICS = 0;
        }
        else {
            enable_PICS = 1;
        }

        if (findoptionS("disablecontentscan") == "on") {
            disable_content_scan = 1;
        }
        else {
            disable_content_scan = 0;
        }

        naughtyness_limit = findoptionI("naughtynesslimit");
        if (!realitycheck(String(naughtyness_limit), 1, 4, "naughtynesslimit"))
            { return false; }
        exception_phrase_list_location = findoptionS("exceptionphraselist");
        weighted_phrase_list_location = findoptionS("weightedphraselist");
        banned_phrase_list_location = findoptionS("bannedphraselist");
        banned_extension_list_location = findoptionS("bannedextensionlist");
        banned_mimetype_list_location = findoptionS("bannedmimetypelist");
        banned_site_list_location = findoptionS("bannedsitelist");
        banned_url_list_location = findoptionS("bannedurllist");
        grey_site_list_location = findoptionS("greysitelist");
        grey_url_list_location = findoptionS("greyurllist");
        banned_regexpurl_list_location = findoptionS("bannedregexpurllist");
        content_regexp_list_location = findoptionS("contentregexplist");
        exceptions_site_list_location = findoptionS("exceptionsitelist");
        exceptions_url_list_location = findoptionS("exceptionurllist");

        pics_rsac_nudity = findoptionI("RSACnudity");
        pics_rsac_language = findoptionI("RSAClanguage");
        pics_rsac_sex = findoptionI("RSACsex");
        pics_rsac_violence = findoptionI("RSACviolence");
        pics_evaluweb_rating = findoptionI("evaluWEBrating");
        pics_cybernot_sex = findoptionI("CyberNOTsex");
        pics_cybernot_other = findoptionI("CyberNOTother");
        pics_safesurf_agerange = findoptionI("SafeSurfagerange");
        pics_safesurf_profanity = findoptionI("SafeSurfprofanity");
        pics_safesurf_heterosexualthemes = findoptionI("SafeSurfheterosexualthemes");
        pics_safesurf_homosexualthemes = findoptionI("SafeSurfhomosexualthemes");
        pics_safesurf_nudity = findoptionI("SafeSurfnudity");
        pics_safesurf_violence = findoptionI("SafeSurfviolence");
        pics_safesurf_sexviolenceandprofanity = findoptionI("SafeSurfsexviolenceandprofanity");
        pics_safesurf_intolerance = findoptionI("SafeSurfintolerance");
        pics_safesurf_druguse = findoptionI("SafeSurfdruguse");
        pics_safesurf_otheradultthemes = findoptionI("SafeSurfotheradultthemes");
        pics_safesurf_gambling = findoptionI("SafeSurfgambling");
        pics_icra_chat = findoptionI("ICRAchat");
        pics_icra_moderatedchat = findoptionI("ICRAmoderatedchat");
        pics_icra_languagesexual = findoptionI("ICRAlanguagesexual");
        pics_icra_languageprofanity = findoptionI("ICRAlanguageprofanity");
        pics_icra_languagemildexpletives = findoptionI("ICRAlanguagemildexpletives");
        pics_icra_nuditygraphic = findoptionI("ICRAnuditygraphic");
        pics_icra_nuditymalegraphic = findoptionI("ICRAnuditymalegraphic");
        pics_icra_nudityfemalegraphic = findoptionI("ICRAnudityfemalegraphic");
        pics_icra_nuditytopless = findoptionI("ICRAnuditytopless");
        pics_icra_nuditybottoms = findoptionI("ICRAnuditybottoms");
        pics_icra_nuditysexualacts = findoptionI("ICRAnuditysexualacts");
        pics_icra_nudityobscuredsexualacts = findoptionI("ICRAnudityobscuredsexualacts");
        pics_icra_nuditysexualtouching = findoptionI("ICRAnuditysexualtouching");
        pics_icra_nuditykissing = findoptionI("ICRAnuditykissing");
        pics_icra_nudityartistic = findoptionI("ICRAnudityartistic");
        pics_icra_nudityeducational = findoptionI("ICRAnudityeducational");
        pics_icra_nuditymedical = findoptionI("ICRAnuditymedical");
        pics_icra_drugstobacco = findoptionI("ICRAdrugstobacco");
        pics_icra_drugsalcohol = findoptionI("ICRAdrugsalcohol");
        pics_icra_drugsuse = findoptionI("ICRAdrugsuse");
        pics_icra_gambling = findoptionI("ICRAgambling");
        pics_icra_weaponuse = findoptionI("ICRAweaponuse");
        pics_icra_intolerance = findoptionI("ICRAintolerance");
        pics_icra_badexample = findoptionI("ICRAbadexample");
        pics_icra_pgmaterial = findoptionI("ICRApgmaterial");
        pics_icra_violenceobjects = findoptionI("ICRAviolenceobjects");
        pics_icra_violencerape = findoptionI("ICRAviolencerape");
        pics_icra_violencetohumans = findoptionI("ICRAviolencetohumans");
        pics_icra_violencetoanimals = findoptionI("ICRAviolencetoanimals");
        pics_icra_violencetofantasy = findoptionI("ICRAviolencetofantasy");
        pics_icra_violencekillinghumans = findoptionI("ICRAviolencekillinghumans");
        pics_icra_violencekillinganimals = findoptionI("ICRAviolencekillinganimals");
        pics_icra_violencekillingfantasy = findoptionI("ICRAviolencekillingfantasy");
        pics_icra_violenceinjuryhumans = findoptionI("ICRAviolenceinjuryhumans");
        pics_icra_violenceinjuryanimals = findoptionI("ICRAviolenceinjuryanimals");
        pics_icra_violenceinjuryfantasy = findoptionI("ICRAviolenceinjuryfantasy");
        pics_icra_violenceartisitic = findoptionI("ICRAviolenceartisitic");
        pics_icra_violenceeducational = findoptionI("ICRAviolenceeducational");
        pics_icra_violencemedical = findoptionI("ICRAviolencemedical");
        pics_icra_violencesports = findoptionI("ICRAviolencesports");
        pics_weburbia_rating = findoptionI("Weburbiarating");
        pics_vancouver_multiculturalism = findoptionI("Vancouvermulticulturalism");
        pics_vancouver_educationalcontent = findoptionI("Vancouvereducationalcontent");
        pics_vancouver_environmentalawareness = findoptionI("Vancouverenvironmentalawareness");
        pics_vancouver_tolerance = findoptionI("Vancouvertolerance");
        pics_vancouver_violence = findoptionI("Vancouverviolence");
        pics_vancouver_sex = findoptionI("Vancouversex");
        pics_vancouver_profanity = findoptionI("Vancouverprofanity");
        pics_vancouver_safety = findoptionI("Vancouversafety");
        pics_vancouver_canadiancontent = findoptionI("Vancouvercanadiancontent");
        pics_vancouver_commercialcontent = findoptionI("Vancouvercommercialcontent");
        pics_vancouver_gambling = findoptionI("Vancouvergambling");

        bypass_mode = findoptionI("bypass");
        if (!realitycheck(String(bypass_mode), 1, 4, "bypass"))
            { return false; }
        if (bypass_mode > 0) {
            magic = findoptionS("bypasskey");
            if (magic.length() < 9) {
                std::string s(16u, ' ');
                for (int i = 0; i < 16; i++) {
                    s[i] = (rand() % 26) + 'A';
                }
                magic = s;
            }
            #ifdef DGDEBUG
                std::cout << "Setting magic key to '" << magic << "'" << std::endl;
            #endif
            // Create the Bypass Cookie magic key
            cookie_magic = std::string(16u, ' ');
            for (int i = 0; i < 16; i++) {
                cookie_magic[i] = (rand() % 26) + 'A';
            }
        }

        // Most of the readfoofiles could be amalgamated into one fuction
        // and will be one day.  So it's a bit messy at the moment.

        #ifdef DGDEBUG
            std::cout << "settings into memory" << filename << std::endl;
        #endif

        if (!readbplfile(banned_phrase_list_location.c_str(), exception_phrase_list_location.c_str(), weighted_phrase_list_location.c_str())) {
            return false;
        }  // read banned, exception, weighted phrase list
        #ifdef DGDEBUG
            std::cout << "read phrase lists into memory" << filename << std::endl;
        #endif
        if (!readeslfile(exceptions_site_list_location.c_str())) {
            return false;
        }  // site exceptions

        if (!readeurllfile(exceptions_url_list_location.c_str())) {
            return false;
        }  // url exceptions
        if (!readbelfile(banned_extension_list_location.c_str())) {
            return false;
        }  // file extensions
        if (!readbmlfile(banned_mimetype_list_location.c_str())) {
            return false;
        }  // mime types
        if (!readbslfile(banned_site_list_location.c_str())) {
            return false;
        }  // banned domains
        if (!readbulfile(banned_url_list_location.c_str())) {
            return false;
        }  // banned urls
        if (!readgslfile(grey_site_list_location.c_str())) {
            return false;
        }  // grey domains
        if (!readgulfile(grey_url_list_location.c_str())) {
            return false;
        }  // grey urls

        if (!readbreulfile(banned_regexpurl_list_location.c_str())) {
            return false;
        }  // banned reg exp urls

        if (!readcrelfile(content_regexp_list_location.c_str())) {
            return false;
        }  // content replacement regular expressions
        #ifdef DGDEBUG
            std::cout << "lists into memory" << filename << std::endl;
        #endif
        if (!precompileregexps()) {
            return false;
        }  // precompiled reg exps for speed

        if ((*o.lm.l[banned_site_list]).inList("**")) {
            blanketblock = 1;
        }
        else {
            blanketblock = 0;
        }
        if ((*o.lm.l[banned_site_list]).inList("*ip")) {
            blanket_ip_block = 1;
        }
        else {
            blanket_ip_block = 0;
        }
    } catch (exception& e) {
        if (!isDaemonised) {
            std::cerr << e.what() << std::endl;  // when called the daemon has not
                                   // detached so we can do this
        }
	return false;
    }
    return true;
}

bool FOptionContainer::readbplfile(const char* banned, const char* exception, const char* weighted) {

    int res = o.lm.newPhraseList(exception, banned, weighted);
    if (res < 0) {
        if (!isDaemonised) {
            std::cerr << "Error opening phraselists" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening phraselists");
        return false;
    }
    banned_phrase_list = res;
    if (!(*o.lm.l[banned_phrase_list]).used) {
        #ifdef DGDEBUG
            std::cout << "Reading new phrase lists" << std::endl;
        #endif
        bool result = (*o.lm.l[banned_phrase_list]).readPhraseList(exception, true);
        if (!result) {
            if (!isDaemonised) {
                std::cerr << "Error opening exceptionphraselist" << std::endl;
            }
            syslog(LOG_ERR, "%s","Error opening exceptionphraselist");
            return false;
        }

        result = (*o.lm.l[banned_phrase_list]).readPhraseList(banned, false);
        if (!result) {
            if (!isDaemonised) {
                std::cerr << "Error opening bannedphraselist" << std::endl;
            }
            syslog(LOG_ERR, "%s","Error opening bannedphraselist");
            return false;
        }
        if (weighted_phrase_mode > 0) {  // if zero wpl is deactivated
            #ifdef DGDEBUG
                std::cout << "Reading weighted phrase list" << std::endl;
            #endif
            result = (*o.lm.l[banned_phrase_list]).readPhraseList(weighted, false);
            if (!result) {
                if (!isDaemonised) {
                    std::cerr << "Error opening weightedphraselist" << std::endl;
                }
                syslog(LOG_ERR, "%s","Error opening weightedphraselist");
                return false;
            }
        }
        (*o.lm.l[banned_phrase_list]).makeGraph(force_quick_search);

        (*o.lm.l[banned_phrase_list]).used = true;
    }
    return true;
}

bool FOptionContainer::readeslfile(const char* filename) {
    int result = o.lm.newItemList(filename, false, 0, true);
    if (result < 0) {
        if (!isDaemonised) {
            std::cerr << "Error opening exceptionsitelist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening exceptionsitelist");
        return false;
    }
    exception_site_list = (unsigned)result;
    if (!(*o.lm.l[exception_site_list]).used) {
        (*o.lm.l[exception_site_list]).endsWithSort();
        if (createlistcachefiles == 1) {
            if (!(*o.lm.l[exception_site_list]).createCacheFile()) {
                return false;
            }
        }
        (*o.lm.l[exception_site_list]).used = true;
    }  // idea is that if the list has already been used it is already
    // compiled, sorted, etc so no point doing it again
    return true;
}

bool FOptionContainer::readeurllfile(const char* filename) {
    int result = o.lm.newItemList(filename, false, 0, true);
    if (result < 0) {
        if (!isDaemonised) {
            std::cerr << "Error opening exceptionurllist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening exceptionurllist");
        return false;
    }
    exception_url_list = (unsigned)result;
    if (!(*o.lm.l[exception_url_list]).used) {
        (*o.lm.l[exception_url_list]).startsWithSort();
        if (createlistcachefiles == 1) {
            if (!(*o.lm.l[exception_url_list]).createCacheFile()) {
                return false;
            }
        }
        (*o.lm.l[exception_url_list]).used = true;
    }
    return true;
}




bool FOptionContainer::readbelfile(const char* filename) {
    int result = o.lm.newItemList(filename, false, 0, true);
    if (result < 0) {
        if (!isDaemonised) {
            std::cerr << "Error opening bannedextensionlist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening bannedextensionlist");
        return false;
    }
    banned_extension_list = (unsigned)result;
    if (!(*o.lm.l[banned_extension_list]).used) {
        (*o.lm.l[banned_extension_list]).endsWithSort();
        (*o.lm.l[banned_extension_list]).used = true;
    }
    return true;
}


bool FOptionContainer::readbslfile(const char* filename) {
    int res = o.lm.newItemList(filename, false, 0, true);
    if (res < 0) {
        if (!isDaemonised) {
            std::cerr << "Error opening bannedsitelist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening bannedsitelist");
        return false;
    }
    banned_site_list = (unsigned)res;
    if (!(*o.lm.l[banned_site_list]).used) {
        (*o.lm.l[banned_site_list]).endsWithSort();
        if (createlistcachefiles == 1) {
            if (!(*o.lm.l[banned_site_list]).createCacheFile()) {
                return false;
            }
        }
        (*o.lm.l[banned_site_list]).used = true;
    }
    return true;
}



bool FOptionContainer::readgslfile(const char* filename) {
    int res = o.lm.newItemList(filename, false, 0, true);
    if (res < 0) {
        if (!isDaemonised) {
            std::cerr << "Error opening greysitelist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening greysitelist");
        return false;
    }
    grey_site_list = (unsigned)res;
    if (!(*o.lm.l[grey_site_list]).used) {
        (*o.lm.l[grey_site_list]).endsWithSort();
        if (createlistcachefiles == 1) {
            if (!(*o.lm.l[grey_site_list]).createCacheFile()) {
                return false;
            }
        }
        (*o.lm.l[grey_site_list]).used = true;
    }
    return true;
}

bool FOptionContainer::readbulfile(const char* filename) {
    int res = o.lm.newItemList(filename, true, 1, true);
    if (res < 0) {
        if (!isDaemonised) {
            std::cerr << "Error opening bannedurllist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening bannedurllist");
        return false;
    }
    banned_url_list = (unsigned)res;
    if (!(*o.lm.l[banned_url_list]).used) {
    (*o.lm.l[banned_url_list]).startsWithSort();
        if (createlistcachefiles == 1) {
            if (!(*o.lm.l[banned_url_list]).createCacheFile()) {
                return false;
            }
        }
        (*o.lm.l[banned_url_list]).used = true;
    }
    return true;
}


bool FOptionContainer::readgulfile(const char* filename) {
    int res = o.lm.newItemList(filename, true, 1, true);
    if (res < 0) {
        if (!isDaemonised) {
            std::cerr << "Error opening greyurllist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening greyurllist");
        return false;
    }
    grey_url_list = (unsigned)res;
    if (!(*o.lm.l[grey_url_list]).used) {
        (*o.lm.l[grey_url_list]).startsWithSort();
        if (createlistcachefiles == 1) {
            if (!(*o.lm.l[grey_url_list]).createCacheFile()) {
                return false;
            }
        }
        (*o.lm.l[grey_url_list]).used = true;
    }
    return true;
}

bool FOptionContainer::readbreulfile(const char* filename) {
    int result = o.lm.newItemList(filename, true, 0, true);
    if (result < 0) {
        if (!isDaemonised) {
            std::cerr << "Error opening bannedregexpurllist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening bannedregexpurllist");
        return false;
    }
    banned_regexpurl_list = (unsigned)result;
    return compilebreulfile(banned_regexpurl_list);
}
// NOTE TO SELF - MOVE TO LISTCONTAINER TO SOLVE FUDGE
bool FOptionContainer::compilebreulfile(unsigned int list) {
    for(unsigned int i = 0; i < (*o.lm.l[list]).morelists.size(); i++) {
        if (!compilebreulfile((*o.lm.l[list]).morelists[i])) {
            return false;
        }
    }
    RegExp r;
    bool rv = true;
    int len = (*o.lm.l[list]).getListLength();
    String source;
    for(int i = 0; i < len; i++) {
        source = (*o.lm.l[list]).getItemAtInt(i).c_str();
        rv = r.comp(source.toCharArray());
        if (rv == false) {
            if (!isDaemonised) {
                std::cerr << "Error compiling regexp:" << source << std::endl;
            }
            syslog(LOG_ERR, "%s", "Error compiling regexp:");
            syslog(LOG_ERR, "%s", source.toCharArray());
            return false;
        }
        banned_regexpurl_list_comp.push_back(r);
        banned_regexpurl_list_source.push_back(source);
    }
    (*o.lm.l[list]).used = true;
    return true;
}


bool FOptionContainer::readcrelfile(const char* filename) {
    int result = o.lm.newItemList(filename, true, 0, true);
    if (result < 0) {
        if (!isDaemonised) {
            std::cerr << "Error opening contentregexplist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening contentregexplist");
        return false;
    }
    content_regexp_list = (unsigned)result;
    if (!(*o.lm.l[content_regexp_list]).used) {
        (*o.lm.l[content_regexp_list]).startsWithSort();
        (*o.lm.l[content_regexp_list]).used = true;
    }
    RegExp r;
    bool rv = true;
    String regexp;
    String replacement;
    for(int i = 0; i < (*o.lm.l[content_regexp_list]).getListLength(); i++) {
        regexp = (*o.lm.l[content_regexp_list]).getItemAtInt(i).c_str();
        replacement = regexp.after("\"->\"");
        while(!replacement.endsWith("\"")) {
            if (replacement.length() < 2) {
                break;
            }
            replacement.chop();
        }
        replacement.chop();
        regexp = regexp.after("\"").before("\"->\"");
        if (replacement.length() < 1 || regexp.length() < 1) {
            continue;
        }
        rv = r.comp(regexp.toCharArray());
        if (rv == false) {
            if (!isDaemonised) {
                std::cerr << "Error compiling regexp:" << (*o.lm.l[content_regexp_list]).getItemAtInt(i) << std::endl;
            }
            syslog(LOG_ERR, "%s", "Error compiling regexp:");
            syslog(LOG_ERR, "%s", (*o.lm.l[content_regexp_list]).getItemAtInt(i).c_str());
        return false;
        }
        content_regexp_list_comp.push_back(r);
        content_regexp_list_rep.push_back(replacement);
    }
    return true;
}

bool FOptionContainer::readbmlfile(const char* filename) {
    int result = o.lm.newItemList(filename, false, 0, true);
    if (result < 0) {
        if (!isDaemonised) {
            std::cerr << "Error opening bannedmimetypelist" << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening bannedmimetypelist");
        return false;
    }
    banned_mimetype_list = (unsigned)result;
    if (!(*o.lm.l[banned_mimetype_list]).used) {
        (*o.lm.l[banned_mimetype_list]).endsWithSort();
        (*o.lm.l[banned_mimetype_list]).used = true;
    }
    return true;
}


bool FOptionContainer::inexceptions(String url) {
    if (iswebserver(url)) {  // don't filter our web server
        return true;
    }
    url.removeWhiteSpace();  // just in case of weird browser crap
    url.toLower();
    url.removePTP();  // chop off the ht(f)tp(s)://
    if (url.contains("/")) {
        url = url.before("/");  // chop off any path after the domain
    }
    char *i;
    while (url.contains(".")) {
        i = (*o.lm.l[exception_site_list]).findInList(url.toCharArray());
        if (i != NULL) {
            return true;  // exact match
        }
        url = url.after(".");  // check for being in higher level domains
    }
    if (url.length() > 1) {  // allows matching of .tld
        url = "." + url;
        i = (*o.lm.l[exception_site_list]).findInList(url.toCharArray());
        if (i != NULL) {
            return i;  // exact match
        }
    }
    return false;  // and our survey said "UUHH UURRGHH"
}


bool FOptionContainer::inurlexceptions(String url) {
    int fl;
    char *i;
    String foundurl;
    url.removeWhiteSpace();  // just in case of weird browser crap
    url.toLower();
    url.removePTP();  // chop off the ht(f)tp(s)://
    if (url.contains("/")) {
        String tpath = "/";
        tpath += url.after("/");
        url = url.before("/");
        tpath.hexDecode();
        tpath.realPath();
        url += tpath;  // will resolve ../ and %2e2e/ and // etc
    }
    if (url.endsWith("/")) {
        url.chop();  // chop off trailing / if any
    }
    while (url.before("/").contains(".")) {
        i = (*o.lm.l[exception_url_list]).findStartsWith(url.toCharArray());
        if (i != NULL) {
            foundurl = i;
            fl = foundurl.length();
            if (url.length() > fl) {
                unsigned char c = url[fl];
                if (c == '/' || c == '?' || c == '&' || c == '=') {
                    return true; // matches /blah/ or /blah/foo but not /blahfoo
                }
            }
            else {
                return true;  // exact match
            }
        }
        url = url.after(".");  // check for being in higher level domains
    }
    return false;
}

char* FOptionContainer::inBannedSiteList(String url) {
    url.removeWhiteSpace();  // just in case of weird browser crap
    url.toLower();
    url.removePTP();  // chop off the ht(f)tp(s)://
    if (url.contains("/")) {
        url = url.before("/");  // chop off any path after the domain
    }
    char *i;
    bool isipurl = isIPHostname(url);
    if (reverse_lookups == 1 && isipurl) {  // change that ip into hostname
        std::deque<String> url2s = ipToHostname(url);
        String url2;
        unsigned int j;
        for (j = 0; j < url2s.size(); j++) {
            url2 = url2s[j];
            while (url2.contains(".")) {
                i = (*o.lm.l[banned_site_list]).findInList(url2.toCharArray());
                if (i != NULL) {
                    return i;  // exact match
                }
                url2 = url2.after(".");  // check for being in hld
            }
        }
    }
    while (url.contains(".")) {
        i = (*o.lm.l[banned_site_list]).findInList(url.toCharArray());
        if (i != NULL) {
            return i;  // exact match
        }
        url = url.after(".");  // check for being in higher level domains
    }
    if (url.length() > 1) {  // allows matching of .tld
        url = "." + url;
        i = (*o.lm.l[banned_site_list]).findInList(url.toCharArray());
        if (i != NULL) {
            return i;  // exact match
        }
    }
    return NULL;  // and our survey said "UUHH UURRGHH"
}


bool FOptionContainer::inGreySiteList(String url) {
    url.removeWhiteSpace();  // just in case of weird browser crap
    url.toLower();
    url.removePTP();  // chop off the ht(f)tp(s)://
    if (url.contains("/")) {
        url = url.before("/");  // chop off any path after the domain
    }
    char *i;
    bool isipurl = isIPHostname(url);
    if (reverse_lookups == 1 && isipurl) {  // change that ip into hostname
        std::deque<String> url2s = ipToHostname(url);
        String url2;
        unsigned int j;
        for (j = 0; j < url2s.size(); j++) {
            url2 = url2s[j];
            while (url2.contains(".")) {
                i = (*o.lm.l[grey_site_list]).findInList(url2.toCharArray());
                if (i != NULL) {
                    return true;  // exact match
                }
                url2 = url2.after(".");  // check for being in hld
            }
        }
    }
    while (url.contains(".")) {
        i = (*o.lm.l[grey_site_list]).findInList(url.toCharArray());
        if (i != NULL) {
            return true;  // exact match
        }
        url = url.after(".");  // check for being in higher level domains
    }
    if (url.length() > 1) {  // allows matching of .tld
        url = "." + url;
        i = (*o.lm.l[grey_site_list]).findInList(url.toCharArray());
        if (i != NULL) {
            return true;  // exact match
        }
    }
    return false;
}


char* FOptionContainer::inBannedExtensionList(String url) {
    url.removeWhiteSpace();  // just in case of weird browser crap
    url.toLower();
    url.hexDecode();
    url.removePTP();  // chop off the ht(f)tp(s)://
    url = url.after("/");  // chop off any domain before the path
    if (url.length() < 2) {  // will never match
        return NULL;
    }
    return (*o.lm.l[banned_extension_list]).findEndsWith(url.toCharArray());
}


char* FOptionContainer::inBannedURLList(String url) {
    int fl;
    char *i;
    String foundurl;
    #ifdef DGDEBUG
        std::cout << "inBannedURLList:" << url << std::endl;
    #endif
    url.removeWhiteSpace();  // just in case of weird browser crap
    url.toLower();
    url.removePTP();  // chop off the ht(f)tp(s)://
    if (url.contains("/")) {
        String tpath = "/";
        tpath += url.after("/");
        url = url.before("/");
        tpath.hexDecode();
        tpath.realPath();
        url += tpath;  // will resolve ../ and %2e2e/ and // etc
    }
    #ifdef DGDEBUG
        std::cout << "inBannedURLList(processed):" << url << std::endl;
    #endif
    if (url.endsWith("/")) {
        url.chop();  // chop off trailing / if any
    }
    if (reverse_lookups == 1 && url.after("/").length() > 0) {
        String hostname = url.before("/");
        if (isIPHostname(hostname)) {
            std::deque<String> url2s = ipToHostname(hostname);
            String url2;
            unsigned int j;
            for (j = 0; j < url2s.size(); j++) {
                url2 = url2s[j];
                url2 += "/";
                url2 += url.after("/");
                while (url2.before("/").contains(".")) {
                    i = (*o.lm.l[banned_url_list]).findStartsWith(url2.toCharArray());
                    if (i != NULL) {
                        foundurl = i;
                        fl = foundurl.length();
                        if (url2.length() > fl) {
                            unsigned char c = url[fl];
                            if (c == '/' || c == '?' || c == '&' || c == '=') {
                                return i; // matches /blah/ or /blah/foo
                                          // but not /blahfoo
                            }
                        }
                        else {
                            return i;  // exact match
                        }
                    }
                    url2 = url2.after(".");  // check for being in hld
                }
            }
        }
    }
    while (url.before("/").contains(".")) {
        i = (*o.lm.l[banned_url_list]).findStartsWith(url.toCharArray());
        if (i != NULL) {
            foundurl = i;
            fl = foundurl.length();
            #ifdef DGDEBUG
                std::cout << "foundurl:" << foundurl << foundurl.length() << std::endl;
                std::cout << "url:" << url << fl << std::endl;
            #endif
            if (url.length() > fl) {
                if (url[fl] == '/') {
                    return i; // matches /blah/ or /blah/foo but not /blahfoo
                }
            }
            else {
                return i;  // exact match
            }
        }
        url = url.after(".");  // check for being in higher level domains
    }
    return NULL;
}


bool FOptionContainer::inGreyURLList(String url) {
    int fl;
    char *i;
    String foundurl;
    url.removeWhiteSpace();  // just in case of weird browser crap
    url.toLower();
    url.removePTP();  // chop off the ht(f)tp(s)://
    if (url.contains("/")) {
        String tpath = "/";
        tpath += url.after("/");
        url = url.before("/");
        tpath.hexDecode();
        tpath.realPath();
        url += tpath;  // will resolve ../ and %2e2e/ and // etc
    }
    if (url.endsWith("/")) {
        url.chop();  // chop off trailing / if any
    }
    if (reverse_lookups == 1 && url.after("/").length() > 0) {
        String hostname = url.before("/");
        if (isIPHostname(hostname)) {
            std::deque<String> url2s = ipToHostname(hostname);
            String url2;
            unsigned int j;
            for (j = 0; j < url2s.size(); j++) {
                url2 = url2s[j];
                url2 += "/";
                url2 += url.after("/");
                while (url2.before("/").contains(".")) {
                    i = (*o.lm.l[grey_url_list]).findStartsWith(url2.toCharArray());
                    if (i != NULL) {
                        foundurl = i;
                        fl = foundurl.length();
                        if (url2.length() > fl) {
                            unsigned char c = url[fl];
                            if (c == '/' || c == '?' || c == '&' || c == '=') {
                                return true; // matches /blah/ or /blah/foo
                                          // but not /blahfoo
                            }
                        }
                        else {
                            return true;  // exact match
                        }
                    }
                    url2 = url2.after(".");  // check for being in hld
                }
            }
        }
    }
    while (url.before("/").contains(".")) {
        i = (*o.lm.l[grey_url_list]).findStartsWith(url.toCharArray());
        if (i != NULL) {
            foundurl = i;
            fl = foundurl.length();
            #ifdef DGDEBUG
                std::cout << "foundurl:" << foundurl << foundurl.length() << std::endl;
                std::cout << "url:" << url << fl << std::endl;
            #endif
            if (url.length() > fl) {
                if (url[fl] == '/') {
                    return true; // matches /blah/ or /blah/foo but not /blahfoo
                }
            }
            else {
                return true;  // exact match
            }
        }
        url = url.after(".");  // check for being in higher level domains
    }
    return false;
}



// NOTE TO SELF - MOVE TO LISTCONTAINER TO SOLVE FUDGE
int FOptionContainer::inBannedRegExpURLList(String url) {
    url.removeWhiteSpace();  // just in case of weird browser crap
    url.toLower();
    url.removePTP();  // chop off the ht(f)tp(s)://
    if (url.contains("/")) {
        String tpath = "/";
        tpath += url.after("/");
        url = url.before("/");
        tpath.hexDecode();
        tpath.realPath();
        url += tpath;  // will resolve ../ and %2e2e/ and // etc
    }
    if (url.endsWith("/")) {
        url.chop();  // chop off trailing / if any
    }
    unsigned int i;
    for (i = 0; i < banned_regexpurl_list_comp.size(); i++) {
        banned_regexpurl_list_comp[i].match(url.toCharArray());
        if (banned_regexpurl_list_comp[i].matched()) {
            return i;
        }
    }
    return -1;
}



std::deque<String> FOptionContainer::ipToHostname(String ip) {
    std::deque<String> result;
    struct in_addr address, ** addrptr;
    if (inet_aton(ip.toCharArray(), &address))  {  // convert to in_addr
        struct hostent* answer;
        answer = gethostbyaddr((char*) &address, sizeof(address), AF_INET);
        if (answer) {  // sucess in reverse dns
            result.push_back(String(answer->h_name));
            for (addrptr = (struct in_addr **) answer->h_addr_list;
                 *addrptr; addrptr++) {
                result.push_back(String(inet_ntoa(**addrptr)));
            }
        }
    }
    return result;
}

bool FOptionContainer::isIPHostname(String url) {
    if (!isiphost.match(url.toCharArray())) {
        return true;
    }
    return false;
}


int FOptionContainer::findoptionI(const char* option) {
    int res = String(findoptionS(option).c_str()).toInteger();
    return res;
}


std::string FOptionContainer::findoptionS(const char* option) {
      // findoptionS returns a found option stored in the deque
    String temp;
    String temp2;
    String o = option;
    for (int i = 0; i < (signed)conffile.size(); i++) {
        temp = conffile[i].c_str();
        temp2 = temp.before("=");
        while(temp2.endsWith(" ")) { // get rid of tailing spaces before =
            temp2.chop();
        }
        if (o == temp2) {
            temp = temp.after("=");
            while(temp.startsWith(" ")) { // get rid of heading spaces
                temp.lop();
            }
            if(temp.startsWith("'")) { // inverted commas
                temp.lop();
            }
            while(temp.endsWith(" ")) { // get rid of tailing spaces
                temp.chop();
            }
            if(temp.endsWith("'")) { // inverted commas
                temp.chop();
            }
            return temp.toCharArray();
        }
    }
    return "";
}

bool FOptionContainer::realitycheck(String s, int minl, int maxl, char* emessage) {
      // realitycheck checks a String for certain expected criteria
      // so we can spot problems in the conf files easier
    if ((signed)s.length() < minl) {
        if (!isDaemonised) {
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
    if ((signed)s.length() > maxl && maxl > 0) {
        if (!isDaemonised) {
            std::cerr << emessage << std::endl;
            std::cerr << "Too long or broken." << std::endl;
        }
        syslog(LOG_ERR, "%s", emessage);
        syslog(LOG_ERR, "%s", "Too long or broken.");
        return false;
    }
    return true;
}


bool FOptionContainer::precompileregexps() {
    if (!pics1.comp("pics-label\"[ \t]*content=[\'\"]([^>]*)[\'\"]")) {
        if (!isDaemonised) {
            std::cerr << "Error compiling RegExp pics1." << std::endl;
        }
        syslog(LOG_ERR, "%s", "Error compiling RegExp pics1.");
        return false;
    }
    if (!pics2.comp("[r|{ratings}] *\\(([^\\)]*)\\)")) {
        if (!isDaemonised) {
            std::cerr << "Error compiling RegExp pics2." << std::endl;
        }
        syslog(LOG_ERR, "%s", "Error compiling RegExp pics2.");
        return false;
    }
    if (!isiphost.comp(".*[a-z|A-Z].*")) {
        if (!isDaemonised) {
            std::cerr << "Error compiling RegExp isiphost." << std::endl;
        }
        syslog(LOG_ERR, "%s", "Error compiling RegExp isiphost.");
        return false;
    }

    return true;
}


bool FOptionContainer::iswebserver(String url) {
    url.removeWhiteSpace();  // just in case of weird browser crap
    url.toLower();
    url.removePTP();  // chop off the ht(f)tp(s)://
    if (url.contains("/")) {
        url = url.before("/");  // chop off any path after the domain
    }
    if (url.startsWith(ada.toCharArray())) { // don't filter our web
                                             // server
        return true;
    }
    return false;
}

