//Please refer to http://dansguardian.org/?page=copyright
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
#include "LanguageContainer.hpp"
#include "RegExp.hpp"
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include "String.hpp"
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <sys/time.h>

extern bool isDaemonised;



void LanguageContainer::reset() {
    keys.clear();
    values.clear();
}

bool LanguageContainer::readLanguageList(const char* filename) {
    std::string linebuffer;  // a string line buffer ;)
    String v;
    String line;
    unsigned int k;
    ifstream Languagefile(filename, ios::in);  // open the file for reading
    if (!Languagefile.good()) {
        if (!isDaemonised) {
            std::cerr << "Error opening messages file (does it exist?): " << filename << std::endl;
        }
        syslog(LOG_ERR, "%s","Error opening messages file (does it exist?): ");
        syslog(LOG_ERR, "%s",filename);
        return false;
    }
    while (!Languagefile.eof()) {  // keep going until end of file
        getline(Languagefile, linebuffer);  // grab a line
        if (linebuffer.length() == 0) {
            continue;
        }
        line = linebuffer.c_str();
        k = line.after("\"").before("\",\"").toInteger();
        v = line.after("\",\"").before("\"");
        if (k > 0 && v.length() > 0) {
            keys.push_back(k);
            values.push_back(v);
        }
    }
    Languagefile.close();
    return true;  // sucessful read
}

const char* LanguageContainer::getTranslation(const unsigned int index) {
    int i;
    int s = keys.size();
    for (i = 0; i < s; i++) {
        if (keys[i] == index) {
            return values[i].toCharArray();
        }
    }
    return "MISSING TRANSLATION KEY";
}
