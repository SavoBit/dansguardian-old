// String - guess what: it's a string class!

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


// INCLUDES

#include "String.hpp"
#include "md5.hpp"

#include <algorithm>

#ifdef __GCCVER3
#include <sstream>
#else
#include <strstream>
#endif

#include <cstdlib>
#include <cstring>
#include <syslog.h>
#include <ctype.h>


// IMPLEMENTATION

// default contructor - i.e. an empty String
String::String():data(new char[1]), sl(0)
{
	data[0] = '\0';
}

// destructor - called when String is destroyed
String::~String()
{
	delete[]data;
}

// constructor from a c-string
String::String(const char *bs)
{
	int l = strlen(bs);
	data = new char[l + 1];
	memcpy(data, bs, l);
	sl = l;
	data[sl] = '\0';
}

// copy constructor
String::String(const String & s)
{
	int l = s.sl;
	data = new char[l + 1];
	memcpy(data, s.data, l + 1);
	sl = l;
}

// constructor from C++ string
String::String(const std::string & s)
{
	int l = s.length();
	data = new char[l + 1];
	memcpy(data, s.c_str(), l);
	sl = l;
	data[sl] = '\0';
}

// substring constructor (takes c-style string & amount to copy)
String::String(const char *bs, int len)
{
	data = new char[len + 1];
	memcpy(data, bs, len);
	sl = len;
	data[sl] = '\0';
}

// substring constructor with offset option
String::String(const char *bs, int start, int len)
{
	data = new char[len + 1];
	memcpy(data, bs + start, len);
	sl = len;
	data[sl] = '\0';
}

// construct string representations of ints/longs

// Bodge to handle GCC3.x - improvements very welcome!!!
// Sun 1st December 2002 - daniel@ //jadeb.com
#ifdef __GCCVER3
String::String(const int num)
{
	std::ostringstream buf;
	buf << num << std::ends;
	std::string s = buf.str();
	char *bs = (char *) s.c_str();
	int l = strlen(bs);
	data = new char[l + 1];
	memcpy(data, bs, l);
	sl = l;
	data[sl] = '\0';
}
#else
String::String(const int num)
{
	std::ostrstream buf;
	buf << num << std::ends;
	data = buf.str();  // with side effect: it calls buf.freeze()
	sl = buf.pcount() - 1;
}
#endif

// Bodge to handle GCC3.x - improvements very welcome!!!
// Sun 1st December 2002 - daniel@ //jadeb.com
#ifdef __GCCVER3
String::String(const long num)
{
	std::ostringstream buf;
	buf << num << std::ends;
	std::string s = buf.str();
	char *bs = (char *) s.c_str();
	int l = strlen(bs);
	data = new char[l + 1];
	memcpy(data, bs, l);
	sl = l;
	data[sl] = '\0';
}
#else
String::String(const long num)
{
	std::ostrstream buf;
	buf << num << std::ends;
	data = buf.str();  // with side effect: it calls buf.freeze()
	sl = buf.pcount() - 1;
}
#endif

// Bodge to handle GCC3.x - improvements very welcome!!!
// Sun 1st December 2002 - daniel@ //jadeb.com
#ifdef __GCCVER3
String::String(const unsigned int num)
{
	std::ostringstream buf;
	buf << num << std::ends;
	std::string s = buf.str();
	char *bs = (char *) s.c_str();
	int l = strlen(bs);
	data = new char[l + 1];
	memcpy(data, bs, l);
	sl = l;
	data[sl] = '\0';
}
#else
String::String(const unsigned int num)
{
	std::ostrstream buf;
	buf << num << std::ends;
	data = buf.str();  // with side effect: it calls buf.freeze()
	sl = buf.pcount() - 1;
}
#endif

// *
// *
// * Operators
// *
// *

// stream output operator
ostream & operator <<(ostream & out, const String & s)
{
	out.write(s.data, s.sl);
	return out;
}

// assignment operator
String & String::operator =(const String & s)
{
	if (&s == this) {
		return *this;
	}
	delete[]data;
	data = new char[s.sl + 1];
	memcpy(data, s.data, s.sl + 1);
	sl = s.sl;
	return *this;
}

// inequality operator
bool String::operator !=(const String & s)
{
	if (sl != s.sl) {
		return true;
	}
	int result = memcmp(data, s.data, sl);
	if (result == 0) {
		return false;
	}
	return true;
}

// equality operator
bool String::operator ==(const String & s)
{
	if (sl != s.sl) {
		return false;
	}
	int result = memcmp(data, s.data, sl);
	if (result == 0) {
		return true;
	}
	return false;
}

// concatenate & assign operator
String & String::operator +=(const String & s)
{
	if (&s == this) {
		return *this;
	}
	char *temp = new char[sl + s.sl + 1];
	memcpy(temp, data, sl);
	memcpy(temp + sl, s.data, s.sl + 1);
	delete[]data;
	sl += s.sl;
	data = temp;
	return *this;
}

// concatenation operator
String String::operator +(const String & s)
{
	String t(*this);
	t += s;
	return (t);
}

// ...and again? (checkme: when is this called instead of the above?)
String operator+(const String & lhs, const String & s)
{
	String t(lhs);
	t += s;
	return (t);
}

// index operator
char String::operator [] (int i) const
{
	return (data[i]);
}

// *
// *
// * Other funcs
// *
// *

// return string length
int String::length()
{
	return sl;
}

// *
// *
// * Conversions
// *
// *

// return c-style string
char *String::toCharArray()
{
	return data;
}

// string-to-integer conversion
int String::toInteger()
{
	if (sl == 0) {
		return 0;
	}
	return (atoi(data));
}

// string-to-long-int conversion
long int String::toLong()
{
	if (sl == 0) {
		return 0;
	}
	return (atol(data));
}

// case conversions
void String::toLower()
{
	for (int i = 0; i < sl; i++) {
		data[i] = tolower(data[i]);
	}
}

void String::toUpper()
{
	for (int i = 0; i < sl; i++) {
		data[i] = toupper(data[i]);
	}
}

// return a substring of length l, beginning at start
String String::subString(int start, int l)
{
	if ((start + l) > sl) {
		return (String(""));
	}
	return (String(data + start, l));
}

// decode %xx to individual characters (checkme: i'm sure this is duplicated elsewhere...)
void String::hexDecode()
{
	char *temp = new char[sl + 1];
	unsigned char c;
	unsigned char c1;
	unsigned char c2;
	unsigned char c3;
	char hexval[5] = "0x";  // Initializes a "hexadecimal string"
	hexval[4] = '\0';
	char *ptr;  // pointer required by strtol
	int j = 0;
	int end = sl - 2;
	int i;
	for (i = 0; i < end;) {
		c1 = data[i];
		c2 = data[i + 1];
		c3 = data[i + 2];
		if (c1 == '%' && (((c2 >= '0') && (c2 <= '9')) || ((c2 >= 'a') && (c2 <= 'f')) || ((c2 >= 'A') && (c2 <= 'F'))) && (((c3 >= '0') && (c3 <= '9')) || ((c3 >= 'a') && (c3 <= 'f')) || ((c3 >= 'A') && (c3 <= 'F')))) {
			hexval[2] = c2;
			hexval[3] = c3;
			c = (unsigned char) strtol(hexval, &ptr, 0);
			i += 3;
		} else {
			c = c1;
			i++;
		}
		temp[j++] = c;
	}
	for (; i < sl; i++) {
		temp[j++] = data[i];  // copy last 2 bytes if any//
	}
	delete[]data;
	sl = j;
	data = new char[sl + 1];
	memcpy(data, temp, sl);
	data[sl] = '\0';
	delete[]temp;
}

// *
// *
// * Searches
// *
// *

// does this string start with the given text?
bool String::startsWith(const String s)
{
	if (!strncmp(data, s.data, s.sl)) {
		return true;
	}
	return false;
}

// does this string start with the given text after conversion to lowercase?
bool String::startsWithLower(const String s)
{
	for (int i = 0; i < s.sl; i++) {
		if (tolower(data[i]) != tolower(s.data[i]))
			return false;
	}
	return true;
}

// does this string end with the given text?
bool String::endsWith(const String s)
{
	if (s.sl > sl) {
		return false;
	}
	if (!strncmp((data + sl - s.sl), s.data, s.sl)) {
		return true;
	}
	return false;
}

// does this string contain given substring?
bool String::contains(const char *s)
{
	if (indexOf(s) != -1) {
		return true;
	}
	return false;
}

// index operator mark 2
unsigned char String::charAt(int index)
{
	if (index >= sl) {
		return 0;
	}
	if (index < 0) {
		return 0;
	}
	unsigned char c = data[index];
	return c;
}

// grab the part of the string that follows the first occurrence of given text
String String::after(const char *bs)
{
	int l = strlen(bs);
	if (l >= sl) {
		return (String(""));
	}
	char *result = NULL;
	result = strstr(data, bs);
	if (result == NULL) {
		return (String(""));
	}
	return (String(result + l));
}

// grab the part of the string that precedes the first occurrence of given text
String String::before(const char *bs)
{
	int l = strlen(bs);
	if (l >= sl) {
		return (String(""));
	}
	char *result = NULL;
	result = strstr(data, bs);
	if (result == NULL) {
		return (String(""));
	}
	return (String(data, (int) (result - data)));
}

// find the position of the given substring within the string
int String::indexOf(const char *s)
{
	if (sl == 0) {
		return -1;
	}
	if ((signed) strlen(s) > sl) {
		return -1;
	}
	char *pos;
	pos = strstr(data, s);
	if (pos == NULL) {
		return -1;
	}
	return ((int) (pos - data));
}

// search & replace "what" with "with"
void String::replace(const char *what, const char *with)
{
	if (sl == 0) {
		return;
	}
	unsigned int whatlen = strlen(what);
	if (whatlen < (unsigned) 1 || whatlen > (unsigned) sl) {
		return;
	}
	unsigned int withlen = strlen(with);
	char *pos;
	unsigned int offset;
	unsigned int i;
	unsigned int j;
	unsigned int newlen;
	while ((pos = strstr(data, what)) != NULL) {
		offset = (unsigned int) (pos - data);
		newlen = sl + withlen - whatlen;
		char *temp = new char[newlen + 1];
		for (i = 0; i < offset; i++) {
			temp[i] = data[i];
		}
		for (i = 0; i < withlen; i++) {
			temp[i + offset] = with[i];
		}
		j = offset + withlen;
		for (i = offset + whatlen; i < (unsigned) sl; i++) {
			temp[j++] = data[i];
		}
		temp[newlen] = '\0';
		delete[]data;
		data = temp;
		sl = newlen;
	}
}

// *
// *
// * Tidy-up functions
// *
// *

// removes a char from the end
void String::chop()
{
	if (sl < 1)
		return;  // can't have -ve String length
	char *temp = new char[sl];
	memcpy(temp, data, sl - 1);
	delete[]data;
	data = temp;
	sl -= 1;
	data[sl] = '\0';
}

// removes a char from the begining
void String::lop()
{
	if (sl < 1)
		return;  // can't have -ve String length
	char *temp = new char[sl];
	memcpy(temp, data + 1, sl - 1);
	delete[]data;
	data = temp;
	sl -= 1;
	data[sl] = '\0';
}

// remove whitespace from beginning and end
void String::removeWhiteSpace()
{
	if (sl < 1)
		return;  // nothing to remove
	while ((unsigned char) data[sl - 1] < 33 && sl > 0) {
		chop();
	}
	while ((unsigned char) data[0] < 33 && sl > 0) {
		lop();
	}
}

// remove protocol prefix
void String::removePTP()
{
	if (startsWith("http://") || startsWith("ftp://")
	    || startsWith("https://")) {
		int pos = strstr(data, "://") - data + 3;
		char *temp = new char[sl - pos + 1];
		memcpy(temp, data + pos, sl - pos);
		delete[]data;
		data = temp;
		sl -= pos;
		data[sl] = '\0';
	}
}

// truncate string to given length
int String::limitLength(unsigned int l)
{
	if (l >= (unsigned) sl || l < 1) {
		return sl;
	}
	char *temp = new char[l + 1];
	memcpy(temp, data, l);
	delete[]data;
	data = temp;
	sl = l;
	data[sl] = '\0';
	return sl;
}

// remove repeated occurrences of given character
void String::removeMultiChar(unsigned char c)
{
	char *temp = new char[sl + 1];
	int j = 0;
	unsigned char t;
	bool wasslash = false;
	for (int i = 0; i < sl; i++) {
		t = data[i];
		if (t != c) {
			// we didn't find the character - copy what we did find,
			// and clear repetition flag
			temp[j++] = t;
			wasslash = false;
			continue;
		}
		// we found the character
		if (wasslash) {
			// we found it repeated - don't copy it again
			continue;
		}
		// we found the character, without repetition flag set
		// - copy only first occurrence & set repetition flag
		wasslash = true;
		temp[j++] = t;
	}
	// copy new, cleaned string into permanent storage
	char *temp2 = new char[j + 1];
	memcpy(temp2, temp, j);
	temp2[j] = '\0';
	delete[]temp;
	delete[]data;
	data = temp2;
	sl = j;
}

// tidy up slashes, trailing dots, etc. in file paths
void String::realPath()
{
	if (sl < 3) {
		return;
	}
	char *temp = new char[sl + 1];
	unsigned char b, c, d;
	unsigned int offset = 0;
	for (int i = 0; i < sl; i++) {
		b = data[i];
		if (b == '/') {
			if (data[i + 1] == '/') {	// ignore multiple slashes
				continue;
			}
		}
		if (b == '.') {
			c = data[i + 1];
			if (c == '\0' || c == '/') {
				continue;  // ignore just dot
			}
			if (c == '.') {
				d = data[i + 2];
				if (d == '\0' || d == '/' || d == '\\') {
					if (offset > 0) {
						offset--;
					}
					while (offset > 0) {
						if (temp[--offset] == '/') {
							break;
						}
						if (temp[offset] == '\\') {
							break;
						}
					}
					i++;
					continue;
				}
			}
		}
		temp[offset++] = b;
	}
	char *temp2 = new char[offset + 1];
	memcpy(temp2, temp, offset);
	temp2[offset] = '\0';
	delete[]data;
	delete[]temp;
	data = temp2;
	sl = offset;
}

// *
// *
// * Hashing functions
// *
// *

String String::md5(const char *salt)
{
	String newValue(*this);
	newValue += salt;
	return newValue.md5();
}

String String::md5()
{
	char *md5array = new char[16];
	char *buf = new char[16];
	int i;

	String ret;

	md5_buffer(data, (size_t) sl, md5array);

	for (i = 0; i < 16; i++) {
		sprintf(buf, "%02X", (unsigned char) (md5array[i]));
		ret += buf;
	}

	delete[]md5array;
	delete[]buf;

	return ret;
}
