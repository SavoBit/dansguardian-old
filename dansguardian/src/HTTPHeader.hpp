//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@//jadeb.com).
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

#define __DGHEADER_SENDALL 0
#define __DGHEADER_SENDFIRSTLINE 1
#define __DGHEADER_SENDREST 2

#ifndef __HPP_HTTPHeader
#define __HPP_HTTPHeader
#include <deque>
#include <string>
#include "String.hpp"

#include "DataBuffer.hpp"
#include "Socket.hpp"

class HTTPHeader {

    public:
        std::deque<String> header;
        DataBuffer postdata;
        unsigned int port;
        void in(Socket *sock);
        void out(Socket *sock, int sendflag) throw(exception);
        int contentlength();
        bool iscontenttype(String t);
        bool malformedURL(String url);
        String url();
        String requesttype();
        String getcontenttype();
        String disposition();
        std::string getauthuser();
        std::string getXForwardedForIP();
        void setTimeout(int t);
        void addXForwardedFor(std::string clientip);
        bool isCompressed();
        String contentEncoding();
        void removeEncoding(int newlen);
        bool isPostUpload();
        bool isRedirection();
        String decode(String s);
        void setContentLength(int newlen);
        bool authRequired();
        int isBypassURL(String *url, const char *magic, const char *clientip);
        bool isScanBypassURL(String *url, const char *magic, const char *clientip);
        bool isBypassCookie(String *url, const char *magic, const char *clientip);
        void chopBypass(String url);
        void chopScanBypass(String url);
        void setCookie(const char *cookie, const char *value);
        String userAgent();


    private:
        void checkheader();
        int timeout;
        String getauth();
        String hexToChar(String n);
        int decode1b64(char c);
        std::string decodeb64(String line);
        String modifyEncodings(String e);
        String getCookie(const char *cookie);

};

#endif
