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
#include <algorithm>
#include "DynamicURLList.hpp"
#include "OptionContainer.hpp"
#include <sys/stat.h>
#include <sys/time.h>

extern OptionContainer o;
extern bool isDaemonised;

DynamicURLList::DynamicURLList()
:index(new unsigned int[0]),urlreftime(new unsigned long int[0]),urls(new char[0]),size(0),agepos(0),timeout(0) {}



// delete the memory block when the class is destryed
DynamicURLList::~DynamicURLList() {
  delete[] index;
  delete[] urlreftime;
  delete[] urls;
}


bool DynamicURLList::setListSize(unsigned int s, unsigned int t) {
    if (s < 2) {
        return false;
    }
    if (t < 2) {
        return false;
    }
    size = s;
    timeout = t;
    agepos = 0;
    items = 0;
    delete[] index;
    delete[] urlreftime;
    delete[] urls;

    index = new unsigned int[size];
    urlreftime = new unsigned long int[size];
    urls = new char[size * 1000]; // allows url up to 999 in length
    return true;
}

void DynamicURLList::flush() {  // not really a flush more of a set all old
    for(int i = 0; i < items; i++) {
        urlreftime[index[i]] = 0; // make all entried old so not used
    }
}


bool DynamicURLList::inURLList(const char* url) {
    #ifdef DGDEBUG
        std::cout << "inURLList(" << url << ")" << std::endl;
    #endif
    if (items == 0) {
        return false;
    }

    #ifdef DGDEBUG
        std::cout << "****** url cache table ******" << std::endl;
        std::cout << "items: " << items << std::endl;
        int i;
        char* u;
        for(i = 0; i < items; i++) {
            u = index[i] * 1000 + urls;
            std::cout << u << std::endl;
        }
        std::cout << "****** url cache table ******" << std::endl;
    #endif


    int pos;
    if (strlen(url) > 999) {
        String r = String(url, 999);
        pos = posInList(r.toCharArray());
    }
    else {
        pos = posInList(url);
    }

    #ifdef DGDEBUG
        std::cout << "pos:" << pos << std::endl;
    #endif

    if (pos > -1) {
        unsigned long int timenow = time(NULL);
        if ((timenow - urlreftime[index[pos]]) > timeout) {
             #ifdef DGDEBUG
                 std::cout << "found but url ttl exceeded:" << (timenow - urlreftime[index[pos]]) << std::endl;
             #endif
            return false;
        }
        return true;
    }
    return false;
}


int DynamicURLList::search(int a, int s, const char* url) {
    if (a > s) return (-1 - a);
    int m = (a + s) / 2;
    char* i = index[m] * 1000 + urls;
    int c = compare(i, url);
    if (c == 0) return m;
    if (c == -1) return search(m + 1, s, url);
    if (a == s) return (-1 - a);
    return search(a, m - 1, url);
}


int DynamicURLList::compare(const char* a, const char* b) {
    // 0 = ==
    // -1 = a < b
    // 1 = a > b
        int alen = strlen(a);
    int blen = strlen(b);
    int maxlen = alen < blen ? alen : blen;
    char* apos = (char*)a;
    char* bpos = (char*)b;
    int i = 0;
    unsigned char achar;
    unsigned char bchar;
    while(i < maxlen) {
        achar = apos[0];
        bchar = bpos[0];
        if (achar > bchar) {
            return 1;
        }
        if (achar < bchar) {
            return -1;
        }
        i ++;
        apos ++;
        bpos ++;
    }
    if (alen > blen) {
        return 1;
    }
    if (alen < blen) {
        return -1;
    }
    return 0;
}


void DynamicURLList::addEntry(const char* url) {
    #ifdef DGDEBUG
        std::cout << "addEntry:" << url << std::endl;
        std::cout << "itemsbeforeadd:" << items << std::endl;
    #endif
    int len = strlen(url);
    bool resized = false;
    char* u;
    if (len > 999) {
        u = new char[1000];
        u[999] = '\0';
        resized = true;
        len = 999;
    }
    else {
        u = (char*)url;
    }
    int pos = posInList(u);
    if (pos >= 0) { // found
        if (resized) {
            delete[] u;
        }
        #ifdef DGDEBUG
            std::cout << "Entry found at pos:" << pos << std::endl;
        #endif
        urlreftime[index[pos]] = time(NULL);  // reset refresh counter
        return; // not going to add entry thats there already
    }

    pos = 0 - pos - 1;  // now contains the insertion point

    #ifdef DGDEBUG
        std::cout << "insertion pos:" << pos << std::endl;
        std::cout << "size:" << size << std::endl;
    #endif

    if (items < size) {
        #ifdef DGDEBUG
            std::cout << "itens<size:" << items << "<" << size << std::endl;
        #endif
        char* urlref;
        urlref = items * 1000 + urls;
        memcpy(urlref, u, len);
        urlref[len] = '\0';
        urlreftime[items] = time(NULL);
        int i;
        for (i = items; i > pos; i--) {
            index[i] = index[i - 1];
        }
        index[pos] = items;
        items++;
        if (resized) {
            delete[] u;
        }
        return;
    }

    // now replace the oldest entry but first need to find it in
    // the index to remove from there

    char* oldestref = urls + agepos * 1000;

    int delpos = posInList(oldestref);  // now contains pos in index to del

    memcpy(oldestref, u, len);
    oldestref[len] = '\0';
    urlreftime[agepos] = time(NULL);

    // now both del the delpos and add into pos the agepos thus maintaining
    // the sorted list

    if (delpos == pos) {
        index[pos] = agepos;
    }
    else if (delpos < pos) {
        int endpos = pos - 1;
        for (int i = delpos; i < endpos; i++) {
            index[i] = index[i + 1];
        }
        index[pos - 1] = agepos;
    }
    else if (delpos > pos) {
        for (int i = delpos; i > pos; i--) {
            index[i] = index[i - 1];
        }
        index[pos] = agepos;
    }
    agepos++;
    if (agepos == size) {
        agepos = 0;
    }

    if (resized) {
        delete[] u;
    }
}


// -ve if not found 0-(pos + 1) is where it would go
// 0 to size if found
int DynamicURLList::posInList(const char* url) {
    if (items == 0) {
        return -1;
    }
    return search(0, items - 1, url);
}
