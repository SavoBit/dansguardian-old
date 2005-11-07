// String - guess what: it's a string class! Cut down version of Java string
// class interface

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

#ifndef __HPP_STRING
#define __HPP_STRING


// INCLUDES

#include "platform.h"

#include <iostream>
#include <string>

#ifdef __GCCVER3
using namespace std;
#endif


// DECLARATIONS

class String
{
public:
	String();
	~String();
	
	// constructor from c-string
	String(const char* bs);
	// copy constructor
	String(const String &s);
	// construct string represenations of numbers
	String(const int num);
	String(const long num);
	String(const unsigned int num);
	// substring constructors
	String(const char *bs, int len);
	String(const char *bs, int start, int len);
	// construct from c++ string
	String(const std::string & s);
	
	// stream output operator
	friend ostream & operator <<(ostream & out, const String & s);
	// concatenation operator(s) ("checkme:" on the second one)
	friend String operator+(const String & lhs, const String & s);
	String operator+(const String & s);
	// concatenate & assign
	String & operator +=(const String & s);
	// assignment operator
	String & operator =(const String & s);
	// boolean operators
	bool operator !=(const String & s);
	bool operator ==(const String & s);
	// index operator
	char operator [] (int i) const;

	// return string length
	int length();

	// return c-string
	char* toCharArray();
	// convert to integer/long integer
	int toInteger();
	long int toLong();
	// case conversions
	void toLower();
	void toUpper();
	// return substring of length l from start
	String subString(int start, int l);

	// decode %xx to characters (checkme: duplicate code?)
	void hexDecode();

	// does the string start/end with this text?
	bool startsWith(const String s);
	bool endsWith(const String s);
	// does this string start with the given text after conversion to lowercase?
	bool startsWithLower(const String s);
	// does it contain this text?
	bool contains(const char *s);
	// index operator mark 2
	unsigned char charAt(int index);

	// return string following first occurrence of bs
	String after(const char *bs);
	// return string preceding first occurrence of bs
	String before(const char *bs);
	// return offset of substring s within the string
	int indexOf(const char *s);
	// search & replace
	void replace(const char *what, const char *with);

	// remove character from end/beginning
	void chop();
	void lop();
	// remove leading & trailing whitespace
	void removeWhiteSpace();
	// remove protocol prefix (e.g. http://)
	void removePTP();
	// truncate to given length
	int limitLength(unsigned int l);
	// remove repeated occurrences of this character
	void removeMultiChar(unsigned char c);
	// clean up slashes, trailing dots, etc. in file paths
	void realPath();

	// generate MD5 hash of string (using given salt)
	String md5();
	String md5(const char *salt);

private:
	// the actual string data & its length
	char *data;
	int sl;
};

#endif
