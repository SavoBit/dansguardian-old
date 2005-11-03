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


// INCLUDES

#include "platform.h"

#include "HTTPHeader.hpp"
#include "OptionContainer.hpp"

#include <syslog.h>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <zlib.h>
#include <cerrno>
#include <fstream>
#include <sys/time.h>
#include <queue>

#ifdef __GCCVER3
#include <istream>
#else
#include <istream.h>
#endif


// DEFINES

#define __DGHEADER_SENDALL 0
#define __DGHEADER_SENDFIRSTLINE 1
#define __DGHEADER_SENDREST 2


// GLOBALS

extern OptionContainer o;


// IMPLEMENTATION

DataBuffer::DataBuffer():data(new char[0]), buffer_length(0), compresseddata(NULL), compressed_buffer_length(0),
	timeout(20), tempfilefd(-1), tempfilesize(0), bytesalreadysent(0), preservetemp(false), dontsendbody(false)
{
}

// delete the memory block when the class is destroyed
DataBuffer::~DataBuffer()
{
	delete[]data;
	if (compresseddata != NULL) {
		delete[]compresseddata;
		compresseddata = NULL;
	}
	if (tempfilefd > -1) {
		close(tempfilefd);
		if (!preservetemp) {
			unlink(tempfilepath.toCharArray());
		}
		tempfilefd = -1;
		tempfilesize = 0;
	}
}

// swap back to a compressed version of the data, if one exits
// also delete uncompressed version
// if body was decompressed but not modified, this can save bandwidth
void DataBuffer::swapbacktocompressed()
{
	if (compresseddata != NULL && compressed_buffer_length > 0) {
		delete[]data;
		buffer_length = compressed_buffer_length;
		data = compresseddata;
		compresseddata = NULL;
		compressed_buffer_length = 0;
	}	
}

// standard socket reader func
void DataBuffer::read(Socket * sock, int l) throw(exception)
{
	delete[]data;  // delete the current data store (should be emtpy anyway)
	data = new char[l + 2];  // create a new store large enough
	int rc;

	rc = (*sock).readFromSocketn(data, l, 0, timeout);  // read in the [POST] data

	if (rc < 0) {
		throw exception();  // danger, danger Will Robinson
	}
	// The above should be all that's needed - but wait there's more!
	// Normal data appended to the header by POST is actually 2 bytes longer
	// than the Content-Length header says.  It contains a carrage return and
	// a new line character.  Simple - just add a fudgefactor of 2.
	// No.  Because when uploading a file via a form the POST data is
	// *exactly* as stated and trying to read even 1 more byte will cause the
	// read to hang.  Also Netscape 4.7x it does it differently.
	// So we need to check the status of the connection to see if there really
	// are more bytes to read.

	if ((*sock).checkForInput()) {
		rc = (*sock).readFromSocket(data + l, 2, 0, timeout);
		// if an error occured (rc < 1) we ignore it and try and continue
		if (rc > 0) {
			l += 2;  // adjust the length
		}
	}

	buffer_length = l;  // update data size counter
}

// a much more efficient reader that does not assume the contents of
// the buffer gets filled thus reducing memcpy()ing and new()ing
int DataBuffer::bufferReadFromSocket(Socket * sock, char *buffer, int size, int sockettimeout)
{
	int pos = 0;
	int rc;
	while (pos < size) {
		rc = sock->readFromSocket(&buffer[pos], size - pos, 0, sockettimeout);
		if (rc < 1) {
			// none recieved or an error
			if (pos > 0) {
				return pos;  // some was recieved previous into buffer
			}
			return rc;  // just return with the return code
		}
		pos += rc;
	}
	return size;  // full buffer
}

// a much more efficient reader that does not assume the contents of
// the buffer gets filled thus reducing memcpy()ing and new()ing.
// in addition to the actual socket timeout, used for each individual read, this version
// incorporates a "global" timeout within which all reads must complete.
int DataBuffer::bufferReadFromSocket(Socket * sock, char *buffer, int size, int sockettimeout, int timeout)
{

	int pos = 0;
	int rc;
	struct timeval starttime;
	struct timeval nowadays;
	gettimeofday(&starttime, NULL);
	while (pos < size) {
		rc = sock->readFromSocket(&buffer[pos], size - pos, 0, sockettimeout);
		if (rc < 1) {
			// none recieved or an error
			if (pos > 0) {
				return pos;  // some was recieved previous into buffer
			}
			return rc;  // just return with the return code
		}
		pos += rc;
		gettimeofday(&nowadays, NULL);
		if (nowadays.tv_sec - starttime.tv_sec > timeout) {
#ifdef DGDEBUG
			std::cout << "buffered socket read more than timeout" << std::endl;
#endif
			return pos;  // just return how much got so far then
		}
	}
	return size;  // full buffer
}

// make a temp file and return its FD. only currently used in DM plugins.
int DataBuffer::getTempFileFD()
{
	if (tempfilefd > -1) {
		return tempfilefd;
	}
	tempfilepath = o.download_dir.c_str();
	tempfilepath += "/tfXXXXXX";
	char *tempfilepatharray = new char[tempfilepath.length() + 1];
	strcpy(tempfilepatharray, tempfilepath.toCharArray());
	if ((tempfilefd = mkstemp(tempfilepatharray)) < 0) {
#ifdef DGDEBUG
		std::cerr << "error creating temp: " << tempfilepath << std::endl;
#endif
		syslog(LOG_ERR, "%s", "Could not create temp file to store download for scanning.");
		tempfilefd = -1;
		tempfilepath = "";
	} else {
		tempfilepath = tempfilepatharray;
	}
	delete[]tempfilepatharray;
	return tempfilefd;
}

// check the client's user agent, see if we have a DM plugin compatible with it, and use it to download the body of the given request
bool DataBuffer::in(Socket * sock, Socket * peersock, HTTPHeader * requestheader, HTTPHeader * docheader, bool runav, int *headersent)
{
	//Socket *sock = where to read from
	//Socket *peersock = browser to send stuff to for keeping it alive
	//HTTPHeader *requestheader = header client used to request
	//HTTPHeader *docheader = header used for sending first line of reply
	//bool runav = to determine if limit is av or not
	//int *headersent = to use to send the first line of header if needed
	//				  or to mark that the header has already been sent

	// so we know if we only partially downloaded from
	// squid so later, if allowed, we can send the rest
	bool toobig = false;

	// match request to download manager so browsers potentially can have a prettier version
	// and software updates, stream clients, etc. can have a compatible version.
	int rc = 0;
# ifdef DGDEBUG
	int j = 0;
#endif
	for (std::deque<Plugin *>::iterator i = o.dmplugins_begin; i != o.dmplugins_end; i++) {
		if ((i + 1) == o.dmplugins_end) {
#ifdef DGDEBUG
			std::cerr << "Got to final download manager so defaulting to always match." << std::endl;
#endif
			rc = ((DMPlugin*)(*i))->in(this, sock, peersock, requestheader, docheader, runav, headersent, &toobig);
			break;
		} else {
			if (((DMPlugin*)(*i))->willHandle(requestheader, docheader)) {
#ifdef DGDEBUG
				std::cerr << "Matching download manager number: " << j << std::endl;
#endif
				rc = ((DMPlugin*)(*i))->in(this, sock, peersock, requestheader, docheader, runav, headersent, &toobig);
				break;
			}
		}
#ifdef DGDEBUG
		j++;
#endif
	}
	// we should check rc and log on error/warn
	// note for later - Tue 16th November 2004
	return toobig;
}

// send the request body to the client after having been handled by a DM plugin
void DataBuffer::out(Socket * sock) throw(exception)
{
	if (dontsendbody) {
		return;
	}
	(*sock).readyForOutput(timeout);  // exceptions on timeout or error

	if (tempfilefd > -1) {
		// must have been too big for ram so stream from disk in blocks
#ifdef DGDEBUG
		std::cout << "sending body from temp file" << std::endl;
#endif
		unsigned int sent = bytesalreadysent;
		int rc;
		lseek(tempfilefd, bytesalreadysent, SEEK_SET);
		while (sent < tempfilesize) {
			rc = readEINTR(tempfilefd, data, buffer_length);
#ifdef DGDEBUG
			std::cout << "reading temp file rc:" << rc << std::endl;
#endif
			if (rc < 0) {
#ifdef DGDEBUG
				std::cout << "error reading temp file so throwing exception" << std::endl;
#endif
				throw exception();
			}
			if (rc == 0) {
#ifdef DGDEBUG
				std::cout << "got zero bytes reading temp file" << std::endl;
#endif
				break;  // should never happen
			}
			// as it's cached to disk the buffer must be reasonably big
			if (!(*sock).writeToSocket(data, rc, 0, timeout)) {
				throw exception();
			}
			sent += rc;
#ifdef DGDEBUG
			std::cout << "total sent from temp:" << sent << std::endl;
#endif
		}
	} else {
		// it's in RAM, so just send it, no streaming from disk
		if (!(*sock).writeToSocket(data + bytesalreadysent, buffer_length - bytesalreadysent, 0, timeout)) {
			throw exception();
		}
	}
}

// zlib decompression
void DataBuffer::zlibinflate(bool header)
{
	if (buffer_length < 12) {
		return;  // it can't possibly be zlib'd
	}
#ifdef DGDEBUG
	std::cout << "compressed size:" << buffer_length << std::endl;
#endif

#if ZLIB_VERNUM < 0x1210
#warning ************************************
#warning For gzip support you need zlib 1.2.1
#warning or later to be installed.
#warning You can ignore this warning but
#warning internet bandwidth may be wasted.
#warning ************************************
	if (header) {
		return;
	}
#endif

	int newsize = buffer_length * 5;  // good estimate of deflated HTML

	char *block;
	char *temp;
	block = new char[newsize];
	int err;
	int bout;
	int bytesgot = 0;

	z_stream d_stream;
	d_stream.zalloc = (alloc_func) 0;
	d_stream.zfree = (free_func) 0;
	d_stream.opaque = (voidpf) 0;
	d_stream.next_in = (Bytef *) data;
	d_stream.avail_in = buffer_length;
	d_stream.next_out = (Bytef *) block;
	d_stream.avail_out = newsize;

	if (header) {
		err = inflateInit2(&d_stream, 15 + 32);
	} else {
		err = inflateInit2(&d_stream, -15);
	}

	if (err != Z_OK) {	// was a problem so just return
		delete[]block;  // don't forget to free claimed memory
#ifdef DGDEBUG
		std::cerr << "bad init inflate:" << err << std::endl;
#endif
		return;
	}
	while (true) {
#ifdef DGDEBUG
		std::cerr << "inflate loop" << std::endl;
#endif
		err = inflate(&d_stream, Z_NO_FLUSH);
		bout = newsize - bytesgot - d_stream.avail_out;
		bytesgot += bout;
		if (err == Z_STREAM_END) {
			break;
		}
		if (err != Z_OK) {	// was a problem so just return
			delete[]block;  // don't forget to free claimed memory
#ifdef DGDEBUG
			std::cerr << "bad inflate:" << String(err) << std::endl;

#endif
			return;
		}

		newsize = bytesgot * 2;
		temp = new char[newsize];
		memcpy(temp, block, bytesgot);
		delete[]block;
		block = temp;

		d_stream.next_out = (Bytef *) (block + bytesgot);
		d_stream.avail_out = newsize - bytesgot;
	}
	compresseddata = data;
	compressed_buffer_length = buffer_length;
	data = block;
	buffer_length = bytesgot;
	
	// what exactly is this doing here!?
	// it looks like this is done above anyway, despite what this says.
	
	// I could create a new block the exact size and memcpy
	// it over to save RAM but RAM is cheap and this saves CPU
//	data = new char[bytesgot];
//	memcpy(data, block, bytesgot);
//	delete[] block;

}

// Does a regexp search and replace.
	typedef struct newreplacement
	{
		int match;
		String replacement;
	};
bool DataBuffer::contentRegExp(int filtergroup)
{

#ifdef DGDEBUG
	std::cout << "Starting content reg exp replace" << std::endl;
#endif
	bool contentmodified = false;
	unsigned int i;
	int j, k, m;
	unsigned int s = (*o.fg[filtergroup]).content_regexp_list_comp.size();
	int matches;
	int submatch, submatches;
	RegExp *re;
	String *replacement;
	int replen;
	int sizediff;
	char *newblock;
	char *dstpos;
	unsigned int srcoff;
	unsigned int nextoffset;
	unsigned int matchlen;

	std::queue<newreplacement*> matchqueue;

	for (i = 0; i < s; i++) {
		re = &((*o.fg[filtergroup]).content_regexp_list_comp[i]);
		if (re->match(data)) {
			replacement = &((*o.fg[filtergroup]).content_regexp_list_rep[i]);
			//replen = replacement->length();
			matches = re->numberOfMatches();

			sizediff = 0;
			m = 0;
			for (j = 0; j < matches; j++) {
				srcoff = re->offset(j);
				matchlen = re->length(j);

				// Count matches for ()'s
				for (submatches = 0; j+submatches+1 < matches; submatches++)
					if (re->offset(j+submatches+1) + re->length(j+submatches+1) > srcoff + matchlen)
						break;

				// \1 and $1 replacement
				
				// store match no. and default (empty) replacement string
				newreplacement* newrep = new newreplacement;
				newrep->match = j;
				newrep->replacement = "";
				// iterate over regex's replacement string
				for (k = 0; k < replacement->length(); k++) {
					// look for \1..\9 and $1..$9
					if (((*replacement)[k] == '\\' || (*replacement)[k] == '$') && (*replacement)[k+1] >= '1' && (*replacement)[k+1] <= '9') {
						// determine match number
						submatch = (*replacement)[++k] - '0';
						// add submatch contents to replacement string
						if (submatch <= submatches) {
							newrep->replacement += re->result(j + submatch).c_str();
						}
					} else {
						// unescape \\ and \$, and add other non-backreference characters
						if ((*replacement)[k] == '\\' && ((*replacement)[k+1] == '\\' || (*replacement)[k+1] == '$'))
							k++;
						newrep->replacement += replacement->subString(k, 1);
					}
				}
				matchqueue.push(newrep);

				// update size difference between original and modified content
				sizediff -= re->length(j);
				sizediff += newrep->replacement.length();
				// skip submatches to next top level match
				j += submatches;
				m++;
			}

			// now we know eventual size of content-replaced block, allocate memory for it
			newblock = new char[buffer_length + sizediff];
			srcoff = 0;
			dstpos = newblock;
			matches = m;

#ifdef DGDEBUG
			std::cout << "content matches:" << matches << std::endl;
#endif
			// replace top-level matches using filled-out replacement strings
			newreplacement* newrep;
			for (j = 0; j < matches; j++) {
				newrep = matchqueue.front();
				nextoffset = re->offset(newrep->match);
				if (nextoffset > srcoff) {
					memcpy(dstpos, data + srcoff, nextoffset - srcoff);
					dstpos += nextoffset - srcoff;
					srcoff = nextoffset;
				}
				replen = newrep->replacement.length();
				memcpy(dstpos, newrep->replacement.toCharArray(), replen);
				dstpos += replen;
				srcoff += re->length(newrep->match);
				delete newrep;
				matchqueue.pop();
			}
			if (srcoff < buffer_length) {
				memcpy(dstpos, data + srcoff, buffer_length - srcoff);
			}
			delete[]data;
			data = newblock;
			buffer_length = buffer_length + sizediff;
			contentmodified = true;
		}
	}
	return contentmodified;
}
