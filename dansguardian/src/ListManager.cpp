//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@/ jadeb.com).
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
#include "ListManager.hpp"
#include "RegExp.hpp"
#include <sys/stat.h>

extern bool isDaemonised;

ListManager::~ListManager() {
   for(unsigned int i = 0; i < l.size(); i++) {
       if (l[i] != NULL) {
           delete l[i];
           l[i] = NULL;
       }
   }
}

int ListManager::newItemList(const char *filename, bool startswith, int filters, bool parent) {
   for(unsigned int i = 0; i < l.size(); i++) {
       if (l[i] == NULL) {
           continue;
       }
       if ((*l[i]).previousUseItem(filename, startswith, filters)) {
           if ((*l[i]).upToDate()) {  // all included files too
               (*l[i]).refcount++;
               #ifdef DGDEBUG
                   std::cout << "Using previous item:" << i << " " << filename << std::endl;
               #endif
               return i;
           }
       }
   }
   int free = findNULL();
   if (free > -1) {
       l[(unsigned)free] = new ListContainer;
   }
   else {
       #ifdef DGDEBUG
           std::cout << "pushing back for new list" << std::endl;
       #endif
       l.push_back(new ListContainer);
       free = l.size() - 1;
   }
   (*l[(unsigned)free]).parent = parent;
   if (!(*l[(unsigned)free]).readItemList(filename, startswith, filters)) {
           delete l[(unsigned)free];
           l[free] = NULL;
           return -1;
   }
   return (unsigned)free;
}

int ListManager::newPhraseList(const char *exception, const char *banned, const char *weighted) {
    int bannedpfiledate = getFileDate(banned);
    int exceptionpfiledate = getFileDate(exception);
    int weightedpfiledate = getFileDate(weighted);
    for(unsigned int i = 0; i < l.size(); i++) {
        if (l[i] == NULL) {
            continue;
        }
        if ((*l[i]).exceptionpfile == String(exception) &&
           (*l[i]).bannedpfile == String(banned) &&
           (*l[i]).weightedpfile == String(weighted) ) {
           if (bannedpfiledate <= (*l[i]).bannedpfiledate &&
               exceptionpfiledate <= (*l[i]).exceptionpfiledate &&
               weightedpfiledate <= (*l[i]).weightedpfiledate) {
           // Known limitation - only weighted, exception, banned phrase
           // list checked for changes - not the included files.
           //
           //need to check all files that were included for phrase list
           //so when phrases read in in list container it needs to store
           //all the file names and if a single one has changed needs a
           //complete regenerate

               #ifdef DGDEBUG
                   std::cout << "Using previous phrase:" << exception << " - " << banned << " - " << weighted << std::endl;
               #endif
               (*l[i]).refcount++;
               return i;
           }
       }
   }
   int free = findNULL();
   if (free > -1) {
       l[(unsigned)free] = new ListContainer;
   }
   else {
       l.push_back(new ListContainer);
       free = l.size() - 1;
   }
   (*l[(unsigned)free]).parent = true; // all phrase lists are parent as
                                       // there are no sub lists
   (*l[(unsigned)free]).bannedpfiledate = bannedpfiledate;
   (*l[(unsigned)free]).exceptionpfiledate = exceptionpfiledate;
   (*l[(unsigned)free]).weightedpfiledate = weightedpfiledate;
   (*l[(unsigned)free]).exceptionpfile = exception;
   (*l[(unsigned)free]).bannedpfile = banned;
   (*l[(unsigned)free]).weightedpfile = weighted;
   return (unsigned)free;
}

void ListManager::deRefList(unsigned int item) {
    if ((item + 1) > l.size()) {
        return;  // should only happen if when a list was generated
        // there was a problem so the list ref is bad
    }
    if (l[item] != NULL) {
        (*l[item]).refcount--;
    }
}

int ListManager::findNULL() {
    for(unsigned int i = 0; i < l.size(); i++) {
        if (l[i] == NULL) {
            #ifdef DGDEBUG
                std::cout << "found free list:" << i << std::endl;
            #endif
            return (signed)i;
        }
    }
    return -1;
}

void ListManager::garbageCollect() {
    for(unsigned int i = 0; i < l.size(); i++) {
        if (l[i] != NULL) {
            if ((*l[i]).refcount < 1) {
                #ifdef DGDEBUG
                   std::cout << "deleting zero ref list:" << i << std::endl;
                #endif
                delete l[i];
                l[i] = NULL;
            }
        }
    }
}

int ListManager::getFileDate(const char* filename) {
    struct stat status;
    int rc = stat(filename, &status);
    if (rc != 0) {
        return -1;
    }
    struct tm *tmnow = localtime(&status.st_mtime);

    int date = (tmnow->tm_year - 100) * 31536000;
    date += tmnow->tm_mon * 2628000;
    date += tmnow->tm_mday * 86400;
    date += tmnow->tm_hour * 3600;
    date += tmnow->tm_min * 60;
    date += tmnow->tm_sec;
    return date;  // a nice int rather than a horrid struct
}

