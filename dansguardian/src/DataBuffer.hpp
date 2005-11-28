//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@/jadeb//.com).
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

#ifndef __HPP_DATABUFFER
#define __HPP_DATABUFFER

#include <exception>
#include "Socket.hpp"
#include "String.hpp"
#include "FDFuncs.hpp"

class DMPlugin;

class DataBuffer
{
public:
	char *buffer[1024];
	char *data;
	unsigned int buffer_length;
	char *compresseddata;
	unsigned int compressed_buffer_length;
	unsigned int tempfilesize;
	String tempfilepath;
	bool dontsendbody;  // used for fancy download manager for example
	int tempfilefd;
	
	// the download manager we used during the last "in"
	DMPlugin *dm_plugin;

	DataBuffer();
	~DataBuffer();

	void read(Socket * sock, int length) throw(exception);

	int length() { return buffer_length; };

	void copyToMemory(char *location) { memcpy(location, data, buffer_length); };
	
	// read body in from proxy
	// gives true if it pauses due to too much data
	bool in(Socket * sock, Socket * peersock, class HTTPHeader * requestheader, class HTTPHeader * docheader, bool runav, int *headersent);
	// send body to client
	void out(Socket * sock) throw(exception);

	void setTimeout(int t) { timeout = t; };
	void setDecompress(String d) { decompress = d; };
	
	// swap back to compressed version of body data (if data was decompressed but not modified; saves bandwidth)
	void swapbacktocompressed();

	// content regexp search and replace
	bool contentRegExp(int filtergroup);

	// create a temp file and return its FD	- NOT a simple accessor function
	int getTempFileFD();

	void reset();
private:
	// DM plugins do horrible things to our innards - this is acceptable pending a proper cleanup
	friend class DMPlugin;
	friend class dminstance;
#ifdef __FANCYDM
	friend class fancydm;
#endif

	int timeout;
	unsigned int bytesalreadysent;
	bool preservetemp;

	String decompress;

	void zlibinflate(bool header);

	// buffered socket reads - one with an extra "global" timeout within which all individual reads must complete
	int bufferReadFromSocket(Socket * sock, char *buffer, int size, int sockettimeout);
	int bufferReadFromSocket(Socket * sock, char *buffer, int size, int sockettimeout, int timeout);

};

#endif
