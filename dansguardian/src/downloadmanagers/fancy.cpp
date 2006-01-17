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
#include "../HTMLTemplate.hpp"

#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>


// GLOBALS

extern OptionContainer o;
extern bool is_daemonised;


// DECLARATIONS

class fancydm:public DMPlugin
{
public:
	fancydm(ConfigVar & definition):DMPlugin(definition) {};
	int in(DataBuffer * d, Socket * sock, Socket * peersock, HTTPHeader * requestheader,
		HTTPHeader * docheader, bool wantall, int *headersent, bool * toobig);

	int init(void* args);

	void sendLink(Socket &peersock, String &linkurl, String &prettyurl);

private:
	HTMLTemplate progresspage;
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

// initialisation - load the template file
int fancydm::init(void* args)
{
	// call inherited init
	DMPlugin::init(args);

	String fname = cv["template"];
	if (fname.length() > 0) {
		// read the template file, and return OK on success, error on failure.
		fname = o.languagepath + fname;
		return progresspage.readTemplateFile(fname.toCharArray(), "-FILENAME-|-FILESIZE-") ? 0 : -1;
	} else {
		// eek! there's no template option in our config.
		if (!is_daemonised)
			std::cerr << "Template not specified for fancy download manager" << std::endl;
		syslog(LOG_ERR, "Template not specified for fancy download manager");
		return -2;
	}
}

// call template's downloadlink JavaScript function
void fancydm::sendLink(Socket &peersock, String &linkurl, String &prettyurl)
{
	String mess = "<script language='javascript'>\n<!--\ndownloadlink(\""+linkurl+"\",\""+prettyurl+"\");\n//-->\n</script>\n";
	peersock.writeString(mess.toCharArray());
	// send text-only version for non-JS-enabled browsers
	// 1220 "Scan complete.</p><p>Click here to download: "
	mess = "<noscript><p>";
	mess += o.language_list.getTranslation(1220);
	mess += "<a href=\"" + linkurl + "\">" + prettyurl + "</a></p></noscript></body></html>\n";
	peersock.writeString(mess.toCharArray());
}

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
	int percentcomplete = 0;
	unsigned int eta = 0;
	int timeelapsed = 0;

	// if using non-persistent connections, some servers will not report
	// a content-length. in these situations, just download everything.
	bool geteverything = false;
	if ((expectedsize < 0) && !(docheader->isPersistent()))
		geteverything = true;

	bool initialsent = false;

	String message, jsmessage;

	char *block;  // buffer for storing a grabbed block from the input stream
	char *temp;

	bool swappedtodisk = false;

	struct timeval starttime;
	struct timeval themdays;
	struct timeval nowadays;
	gettimeofday(&themdays, NULL);
	gettimeofday(&starttime, NULL);
	
	// buffer size for streaming downloads
	unsigned int blocksize = 32768;
	// set to a sensible minimum
	if (!wantall && (blocksize > o.max_content_filter_size))
		blocksize = o.max_content_filter_size;
	else if (wantall && (blocksize > o.max_content_ramcache_scan_size))
		blocksize = o.max_content_ramcache_scan_size;
#ifdef DGDEBUG
	std::cout << "blocksize: " << blocksize << std::endl;
#endif

	// determine downloaded filename
	String filename = requestheader->disposition();
	if (filename.length() == 0) {
		filename = requestheader->url();
		filename = requestheader->decode(filename);
		if (filename.contains("?"))
			filename = filename.before("?");
		while (filename.contains("/"))
			filename = filename.after("/");
	}

	while ((bytesgot < expectedsize) || geteverything) {
		// send text header to show status
		if (o.trickle_delay > 0) {
			gettimeofday(&nowadays, NULL);
			timeelapsed = nowadays.tv_sec - starttime.tv_sec;
			if ((!initialsent && timeelapsed > o.initial_trickle_delay) || (initialsent && nowadays.tv_sec - themdays.tv_sec > o.trickle_delay)) {
				initialsent = true;
				bytessec = bytesgot / timeelapsed;
				themdays.tv_sec = nowadays.tv_sec;
				if ((*headersent) < 1) {
#ifdef DGDEBUG
					std::cout << "sending header for text status" << std::endl;
#endif
					message = "HTTP/1.0 200 OK\nContent-Type: text/html\n\n";
					// Output initial template
					std::deque<String>::iterator i = progresspage.html.begin();
					std::deque<String>::iterator penultimate = progresspage.html.end()-1;
					bool newline;
					while (i != progresspage.html.end()) {
						newline = false;
						message = *i;
						if (message == "-FILENAME-") {
							message = filename;
						}
						else if (message == "-FILESIZE-") {
							message = String(expectedsize);
						}
						else if ((i == penultimate) || ((*(i+1))[0] != '-')) {
								newline = true;
						}
						peersock->writeString(message.toCharArray());
						// preserve line breaks from the original template file
						if (newline)
							peersock->writeString("\n");
						i++;
					}
					// send please wait message for non-JS-enabled browsers
					// 1200 "Please wait - downloading to be scanned..."
					message = "<noscript><p>";
					message += o.language_list.getTranslation(1200);
					message += "</p></noscript>\n";
					peersock->writeString(message.toCharArray());
					(*headersent) = 2;
				}
#ifdef DGDEBUG
				std::cout << "trickle delay - sending progress..." << std::endl;
#endif
				message = "Downloading status: ";
				if (expectedsize > 0) {
					// Output a call to template's JavaScript progressupdate function
					jsmessage = "<script language='javascript'>\n<!--\nprogressupdate(" + String(bytesgot) + "," + String(bytessec) + ");\n//-->\n</script>";
					peersock->writeString(jsmessage.toCharArray());
					// send text only version for non-JS-enabled browsers.
					percentcomplete = bytesgot/(expectedsize/100);
					eta = (expectedsize-bytesgot)/bytessec;
					// checkme: translation?
					message = "<noscript><p>" + String(percentcomplete) + "%, ETA " + String(eta) + " sec, "
						+ String(bytessec) + " bytes/sec, total downloaded " + String(bytesgot) + "</p></noscript>\n";
					peersock->writeString(message.toCharArray());
				}
				peersock->writeString("<!-- force flush -->\r\n");
			}
		}

		if (wantall) {
			if (!swappedtodisk) {
				// if not swapped to disk and file is too large for RAM, then swap to disk
				if (bytesgot > o.max_content_ramcache_scan_size) {
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
			} else if (bytesgot > o.max_content_filecache_scan_size) {
				// if swapped to disk and file too large for that too, then give up
#ifdef DGDEBUG
				std::cout << "fancydm: file too big to be scanned, halting download" << std::endl;
#endif
				(*toobig) = true;
				break;
			}
		} else {
			if (bytesgot > o.max_content_filter_size) {
				// if we aren't downloading for virus scanning, and file too large for filtering, give up
#ifdef DGDEBUG
				std::cout << "fancydm: file too big to be filtered, halting download" << std::endl;
#endif
				(*toobig) = true;
				break;
			}
		}

		if (!swappedtodisk) {
			if (d->buffer_length >= blocksize) {
				newsize = d->buffer_length;
			} else {
				newsize = blocksize;
			}
#ifdef DGDEBUG
			std::cout << "newsize: " << newsize << std::endl;
#endif
			// if not getting everything until connection close, grab only what is left
			if (!geteverything && (newsize > (expectedsize - bytesgot)))
				newsize = expectedsize - bytesgot;
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

			if (rc <= 0) {
				delete[]block;
				break;  // an error occured so end the while()
				// or none received so pipe is closed
			}
			else {
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
			rc = d->bufferReadFromSocket(sock, d->data,
				// if not getting everything until connection close, grab only what is left
				(!geteverything && ((expectedsize - bytesgot) < d->buffer_length) ? (expectedsize - bytesgot) : d->buffer_length), d->timeout);
			if (rc <= 0) {
				break;
			}
			else {
				lseek(d->tempfilefd, 0, SEEK_END);  // not really needed
				writeEINTR(d->tempfilefd, d->data, rc);
				d->tempfilesize += rc;
#ifdef DGDEBUG
				std::cout << "written to disk: " << rc << " total: " << d->tempfilesize << std::endl;
#endif
			}
		}
		if (d->tempfilesize > 0) {
			bytesgot = d->tempfilesize;
		} else {
			bytesgot = d->buffer_length;
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

		// Output a call to template's JavaScript nowscanning function
		peersock->writeString("<script language='javascript'>\n<!--\nnowscanning();\n//-->\n</script>\n");
		// send text-only version
		// 1210 "Download Complete.  Starting scan..."
		message = "<noscript><p>";
		message += o.language_list.getTranslation(1210);
		message += "</p></noscript>\n";
		peersock->writeString(message.toCharArray());
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
