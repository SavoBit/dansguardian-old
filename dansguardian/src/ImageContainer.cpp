//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@jadeb//.com) but heavily based on code
//written by Aecio F. Neto (afn@harvest.com.br).
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

#include "platform.h"
#include "ImageContainer.hpp"
#include <syslog.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <limits.h>

extern bool isDaemonised;

ImageContainer::ImageContainer() {
    image = new char[0];
    imagelength = 0;
}

ImageContainer::~ImageContainer() {
    delete[] image;
}

void ImageContainer::reset() {
    delete[] image;
    mimetype = "";
    imagelength = 0;
}

bool ImageContainer::read(const char* filename) {

    String temp;
    temp = (char*) filename;
    temp.toLower();
    if (temp.endsWith(".jpg") || temp.endsWith(".jpeg")
                              || temp.endsWith(".jpe")) {
        mimetype = "image/jpg";
    }
    else if (temp.endsWith("png")) mimetype = "image/png";
    else {
        mimetype = "image/gif";
    }

    ifstream imagefile;
    imagefile.open(filename, ifstream::binary);
    imagefile.seekg(0, ios::end);
    imagelength = imagefile.tellg();
    imagefile.seekg(0, ios::beg);

    if (imagelength) {
	image = new char[imagelength+1];
	imagefile.read(image, imagelength);
	if (!imagefile.good()) {
	    if (!isDaemonised)
		std::cerr << "Error reading custom image file: " << filename << std::endl;
	    syslog(LOG_ERR, "%s", "Error reading custom image file.");
	    return false;
	}
    }
    else {
	if (!isDaemonised)
	    std::cerr << "Error reading custom image file: " << filename << std::endl;
	syslog(LOG_ERR, "%s", "Error reading custom image file.");
	return false;
    }
    imagefile.close();
//    #ifdef DGDEBUG
//	for (long int i = 0; i < imagelength; i++)
//	    printf("Image byte content: %x\n", image[i]);
//    #endif
    return true;
}

void ImageContainer::display(Socket* s) {
    #ifdef DGDEBUG
        std::cout << "Displaying custom image file" << std::endl;
	std::cout << "mimetype: " << mimetype << std::endl;
    #endif
    (*s).writeString("Content-type: ");
    (*s).writeString(mimetype.toCharArray());
    (*s).writeString("\n\n");
    (*s).writeToSocket(image, imagelength, 0, (*s).getTimeout());
}
