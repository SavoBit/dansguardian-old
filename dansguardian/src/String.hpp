//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@ jadeb//.com).
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

//A cut-down String class based on the Java String class interface

#include "platform.h"
#ifndef __HPP_STRING
#define __HPP_STRING
#include <iostream>
#include <string>

#ifdef __GCCVER3
using namespace std;
#endif

class String {

public:
    String();
    ~String();
    String(const char bs[]);
    String(const String& s);
    String(const int num);
    String(const long num);
    String(const char bs[], int len);
    String(const char bs[], int start, int len);
    String(const std::string &s);
    friend ostream &  operator << (ostream & out, const String& s);
    friend String operator+ (const String & lhs, const String & s);
    String& operator = (const String & s);
    bool operator != (const String & s);
    bool operator == (const String & s);
    String& operator += (const String & s);
    String operator+ (const String & s);
    char operator [] (int i) const;
    String after(const char* bs);
    String before(const char* bs);
    bool startsWith(const String s);
    bool endsWith(const String s);
    String subString(int start, int l);
    int toInteger();
    long int toLong();
    int length();
    char* toCharArray();
    int indexOf(const char *s);
    void chop();
    void lop();
    bool contains(const char *s);
    void toLower();
    void toUpper();
    void removeWhiteSpace();
    unsigned char charAt(int index);
    void removePTP();
    int limitLength(unsigned int l);
    void removeMultiChar(unsigned char c);
    void hexDecode();
    String md5();
    String md5(const char *salt);
    void replace(const char *what, const char *with);
    void realPath();

private:
    char* data;
    int sl;

};

#endif
