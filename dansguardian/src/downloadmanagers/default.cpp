// Default download manager, used when no other plugin matches the user agent

//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.

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

#include "../DownloadManager.hpp"
#include "../OptionContainer.hpp"

#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


// GLOBALS

extern OptionContainer o;


// DECLARATIONS

class dminstance:public DMPlugin
{
public:
	dminstance(ConfigVar & definition):DMPlugin(definition) {};
	int in(DataBuffer * d, Socket * sock, Socket * peersock, HTTPHeader * requestheader,
		HTTPHeader * docheader, bool wantall, int *headersent, bool * toobig);
};


// IMPLEMENTATION

// class factory code *MUST* be included in every plugin

DMPlugin *defaultdmcreate(ConfigVar & definition)
{
#ifdef DGDEBUG
	std::cout << "Creating default DM" << std::endl;
#endif
	return new dminstance(definition);
}

// end of Class factory

// download body for this request
int dminstance::in(DataBuffer * d, Socket * sock, Socket * peersock, class HTTPHeader * requestheader,
	class HTTPHeader * docheader, bool wantall, int *headersent, bool * toobig)
{

	//DataBuffer *d = where to stick the data back into
	//Socket *sock = where to read from
	//Socket *peersock = browser to send stuff to for keeping it alive
	//HTTPHeader *requestheader = header client used to request
	//HTTPHeader *docheader = header used for sending first line of reply
	//bool wantall = to determine if just content filter or a full scan
	//int *headersent = to use to send the first line of header if needed
	//                                or to mark the header has already been sent
	//bool *toobig = flag to modify to say if it could not all be downloaded

#ifdef DGDEBUG
	std::cout << "Inside default download manager plugin" << std::endl;
#endif

//  To access settings for the plugin use the following example:
//      std::cout << "cvtest:" << cv["dummy"] << std::endl;

	int rc, newsize;

	char *block;  // buffer for storing a grabbed block from the
	// imput stream
	char *temp;

	bool swappedtodisk = false;

	struct timeval themdays;
	struct timeval nowadays;
	gettimeofday(&themdays, NULL);

	while (true) {
		// send x-header keep-alive here
		if (o.trickle_delay > 0) {
			gettimeofday(&nowadays, NULL);
			if (nowadays.tv_sec - themdays.tv_sec > o.trickle_delay) {
				themdays.tv_sec = nowadays.tv_sec;
				if ((*headersent) < 1) {
#ifdef DGDEBUG
					std::cout << "sending first line of header first" << std::endl;
#endif
					docheader->out(peersock, __DGHEADER_SENDFIRSTLINE);
					(*headersent) = 1;
				}
#ifdef DGDEBUG
				std::cout << "trickle delay - sending X-DGKeepAlive: on" << std::endl;
#endif
				peersock->writeString("X-DGKeepAlive: on\r\n");
			}
		}

		if (wantall) {
			if (!swappedtodisk) {
				if (d->buffer_length > o.max_content_ramcache_scan_size) {
					if (d->buffer_length > o.max_content_filecache_scan_size) {
						(*toobig) = true;
						break;
					} else {
						d->tempfilefd = d->getTempFileFD();
						if (d->tempfilefd < 0) {
#ifdef DGDEBUG
							std::cerr << "error buffering to disk so skipping disk buffering" << std::endl;
#endif
							syslog(LOG_ERR, "%s", "error buffering to disk so skipping disk buffering");

							(*toobig) = true;
							break;
						}
						writeEINTR(d->tempfilefd, d->data, d->buffer_length);
						swappedtodisk = true;
						d->tempfilesize = d->buffer_length;
#ifdef DGDEBUG
						std::cout << "swapping to disk" << std::endl;
#endif
					}

				}
			}
		} else {
			if (d->buffer_length > o.max_content_filter_size) {
				(*toobig) = true;
				break;
			}
		}

		if (d->buffer_length >= 32768) {
			newsize = d->buffer_length;
		} else {
			newsize = 32768;
		}

		if (!swappedtodisk) {
			block = new char[newsize];
			try {
				sock->checkForInput(d->timeout);
			} catch(exception & e) {
				delete[]block;
				break;
			}
			// improved more efficient socket read which uses the buffer better
			rc = d->bufferReadFromSocket(sock, block, newsize, d->timeout);
			// grab a block of input, doubled each time

			if (rc < 1) {
				delete[]block;
				break;  // an error occured so end the while()
				// or none received so pipe is closed
			}
			if (rc > 0) {
				temp = new char[d->buffer_length + rc];  // replacement store
				memcpy(temp, d->data, d->buffer_length);  // copy the current data
				memcpy(temp + d->buffer_length, block, rc);  // copy the new data
				delete[]d->data;  // delete the current data block
				d->data = temp;
				d->buffer_length += rc;  // update data size counter
			}
			delete[]block;
		} else {
			try {
				sock->checkForInput(d->timeout);
			}
			catch(exception & e) {
				break;
			}
			rc = d->bufferReadFromSocket(sock, d->data, d->buffer_length, d->timeout);
			if (rc < 1) {
				break;
			}
			if (rc > 0) {
				lseek(d->tempfilefd, 0, SEEK_END);  // not really needed
				writeEINTR(d->tempfilefd, d->data, rc);
				d->tempfilesize += rc;
#ifdef DGDEBUG
				std::cout << "written to disk:" << rc << " total:" << d->tempfilesize << std::endl;
#endif
			}
		}
	}

	if (!(*toobig) && !swappedtodisk) {	// won't deflate stuff swapped to disk
		if (d->decompress.contains("deflate")) {
#ifdef DGDEBUG
			std::cout << "zlib format" << std::endl;
#endif
			d->zlibinflate(false);  // incoming stream was zlib compressed
		}
		else if (d->decompress.contains("gzip")) {
#ifdef DGDEBUG
			std::cout << "gzip format" << std::endl;
#endif
			d->zlibinflate(true);  // incoming stream was gzip compressed
		}
	}
	d->bytesalreadysent = 0;
	return 0;
}
