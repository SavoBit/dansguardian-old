//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@// jadeb.com).
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

#ifndef __HPP_REGEXP
#define __HPP_REGEXP
#include "platform.h"
#include <sys/types.h>  // needed for size_t used in regex.h
#include <regex.h>
#include <string>
#include <deque>

#ifdef __GCCVER3
using namespace std;
#endif

class RegExp {

public:
    RegExp();
    ~RegExp();
    RegExp(const RegExp& r);
    bool comp(const char* exp);
    bool match(const char* text);
    int numberOfMatches();
    bool matched();
    std::string result(int i);
    unsigned int offset(int i);
    unsigned int length(int i);
    char* search(char* file, char* fileend, char* phrase, char* phraseend);

private:
    std::deque<std::string> results;
    std::deque<unsigned int> offsets;
    std::deque<unsigned int> lengths;
    bool imatched;
    regex_t reg;
    bool wascompiled;
    std::string searchstring;
};

#endif
