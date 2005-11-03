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

class fancydm:public DMPlugin
{
public:
	fancydm(ConfigVar & definition):DMPlugin(definition) {};
	int in(DataBuffer * d, Socket * sock, Socket * peersock, HTTPHeader * requestheader,
		HTTPHeader * docheader, bool wantall, int *headersent, bool * toobig);
};


// IMPLEMENTATION

// class factory code *MUST* be included in every plugin

DMPlugin *fancydmcreate(ConfigVar & definition)
{
#ifdef DGDEBUG
	std::cout << "Creating fancy DM" << std::endl;
#endif
	return new fancydm(definition);
}

// end of Class factory

// download body for this request
int fancydm::in(DataBuffer * d, Socket * sock, Socket * peersock, class HTTPHeader * requestheader, class HTTPHeader * docheader, bool wantall, int *headersent, bool * toobig)
{

	//DataBuffer *d = where to stick the data back into
	//Socket *sock = where to read from
	//Socket *peersock = browser to send stuff to for keeping it alive
	//HTTPHeader *docheader = header used for sending first line of reply
	//HTTPHeader *requestheader = header client used to request
	//bool wantall = to determine if just content filter or a full scan
	//int *headersent = to use to send the first line of header if needed
	//                  or to mark the header has already been sent
	//bool *toobig = flag to modify to say if it could not all be downloaded

#ifdef DGDEBUG
	std::cout << "Inside fancy download manager plugin" << std::endl;
#endif

	int rc;

	unsigned int newsize;
	unsigned int expectedsize = docheader->contentLength();
	unsigned int bytessec = 0;
	unsigned int bytesgot = 0;
	unsigned int percentcomplete = 0;
	unsigned int eta = 0;
	int timeelapsed = 0;

	bool initialsent = false;

	String message, jsmessage;

	char *block;  // buffer for storing a grabbed block from the
	// input stream
	char *temp;

	bool swappedtodisk = false;

	struct timeval starttime;
	struct timeval themdays;
	struct timeval nowadays;
	gettimeofday(&themdays, NULL);
	gettimeofday(&starttime, NULL);

	while (true) {
		// send text header to show status
		if (o.trickle_delay > 0) {
			gettimeofday(&nowadays, NULL);
			timeelapsed = nowadays.tv_sec - starttime.tv_sec;
			if ((!initialsent && timeelapsed > o.initial_trickle_delay) || (initialsent && nowadays.tv_sec - themdays.tv_sec > o.trickle_delay)) {
				initialsent = true;
				if (d->tempfilesize > 0) {
					bytesgot = d->tempfilesize;
				} else {
					bytesgot = d->buffer_length;
				}
				bytessec = bytesgot / timeelapsed;
				themdays.tv_sec = nowadays.tv_sec;
				if ((*headersent) < 1) {
#ifdef DGDEBUG
					std::cout << "sending header for text status" << std::endl;
#endif
					message = "HTTP/1.0 200 OK\nContent-Type: text/html\n\n<HTML><HEAD><TITLE>";
					message += o.language_list.getTranslation(1200);
					message += "</TITLE></HEAD><BODY>\r\n";
					message += o.language_list.getTranslation(1200);
					// "1200","Please wait - downloading to be scanned..."
					if (expectedsize > 0) {
						message +=
							"<table align='center'><tr><td>\n<div style='font-size:8pt;padding:2px;border:solid black 1px'>\n<span id='progress1'>&nbsp; &nbsp;</span>\n<span id='progress2'>&nbsp; &nbsp;</span>\n<span id='progress3'>&nbsp; &nbsp;</span>\n<span id='progress4'>&nbsp; &nbsp;</span>\n<span id='progress5'>&nbsp; &nbsp;</span>\n<span id='progress6'>&nbsp; &nbsp;</span>\n<span id='progress7'>&nbsp; &nbsp;</span>\n<span id='progress8'>&nbsp; &nbsp;</span>\n<span id='progress9'>&nbsp; &nbsp;</span>\n<span id='progress10'>&nbsp; &nbsp;</span>\n<span id='progress11'>&nbsp; &nbsp;</span>\n<span id='progress12'>&nbsp; &nbsp;</span>\n<span id='progress13'>&nbsp; &nbsp;</span>\n<span id='progress14'>&nbsp; &nbsp;</span>\n<span id='progress15'>&nbsp; &nbsp;</span>\n<span id='progress16'>&nbsp; &nbsp;</span>\n</div>\n</td></tr></table>\n<script language='javascript'>for (var i = 1; i <= 16; i++) document.getElementById('progress'+i).style.backgroundColor = 'transparent';</script>\r\n";
					}

					message += "<center><form name='statsform'><input type='text' name='stats' value=''></form></center>\r\n";

					message += "<noscript><PRE></noscript>";
					peersock->writeString(message.toCharArray());
					(*headersent) = 2;
				}
#ifdef DGDEBUG
				std::cout << "trickle delay - sending progress..." << std::endl;
#endif
				message = "Downloading status: ";
				if (expectedsize > 0) {
					percentcomplete = bytesgot / (expectedsize / 100);
					eta = (expectedsize - bytesgot) / bytessec;
					message += String((signed) percentcomplete);
					message += "%, ETA:";
					message += String((signed) eta);
					message += " sec, ";
					jsmessage =
						"<script language='javascript'>for (var i = 1; i <=" + String((signed) percentcomplete / (100 / 16)) + "; i++) document.getElementById('progress'+i).style.backgroundColor = 'blue';</script>";
					peersock->writeString(jsmessage.toCharArray());
				}
				message += String((signed) bytessec);
				message += " bytes/sec, total downloaded ";
				message += String((signed) bytesgot);
				message += " bytes";
				jsmessage = "<script language='javascript'>document.statsform.stats.value='" + message + "';document.statsform.stats.size='" + message.length() + "';</script>";
				peersock->writeString(jsmessage.toCharArray());

				message = "<noscript>" + message + "</noscript>\r\n";
				peersock->writeString(message.toCharArray());
				peersock->writeString("<!-- force flush -->\r\n");
#ifdef DGDEBUG
				std::cout << message << std::endl;
#endif
			}
		}

		if (wantall) {
			if (!swappedtodisk) {
				if (d->buffer_length > o.max_content_ramcache_scan_size) {
					if (d->buffer_length > o.max_content_filecache_scan_size) {
						(*toobig) = true;
						break;
					} else {
#ifdef DGDEBUG
						std::cout << "swapping to disk" << std::endl;
#endif
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
			rc = d->bufferReadFromSocket(sock, block, newsize, d->timeout, o.trickle_delay);
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

	if (initialsent) {

		if (!swappedtodisk) {	// if we sent textual content then we can't
			// stream the file to the user so we must save to disk for them
			// to download by clicking on the magic link
			// You can get to this point by having a large ram cache, or
			// slow internet connection with small initial trickle delay.
			// This should be rare.
#ifdef DGDEBUG
			std::cout << "swapping to disk" << std::endl;
#endif
			d->tempfilefd = d->getTempFileFD();
			if (d->tempfilefd < 0) {
#ifdef DGDEBUG
				std::cerr << "error buffering complete to disk so skipping disk buffering" << std::endl;
#endif
				syslog(LOG_ERR, "%s", "error buffering complete to disk so skipping disk buffering");
			} else {
				writeEINTR(d->tempfilefd, d->data, d->buffer_length);
				swappedtodisk = true;
				d->tempfilesize = d->buffer_length;
			}
		}

		message = o.language_list.getTranslation(1210);
		// "1210","Download Complete.  Starting scan..."
		jsmessage = "<script language='javascript'>document.statsform.stats.value='" + message + "';document.statsform.stats.size='" + message.length() + "';</script>";
		peersock->writeString(jsmessage.toCharArray());
		message = "<NOSCRIPT></PRE>" + message + "</NOSCRIPT>\r\n";
		peersock->writeString(message.toCharArray());

		if (expectedsize > 0) {
			jsmessage = "<script language='javascript'>for (var i = 1; i <=16; i++) document.getElementById('progress'+i).style.backgroundColor = 'blue';</script>\r\n";
			peersock->writeString(jsmessage.toCharArray());
		}
		(*d).preservetemp = true;
		(*d).dontsendbody = true;
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
