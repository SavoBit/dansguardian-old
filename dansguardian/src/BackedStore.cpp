//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
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

#ifdef HAVE_CONFIG_H
	#include "dgconfig.h"
#endif

#include <cstddef>
#include <vector>
#include <string>
#include <exception>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sstream>

#include <unistd.h>

#include <sys/mman.h>

#include "BackedStore.hpp"

#ifdef DGDEBUG
#include <iostream>
#endif


// IMPLEMENTATION

BackedStore::BackedStore(size_t _ramsize, size_t _disksize, const char *_tempdir)
	: fd(-1), length(0), filename(NULL), ramsize(_ramsize),
	disksize(_disksize), tempdir(_tempdir), map(MAP_FAILED)
{
}

BackedStore::~BackedStore()
{
	if (map != MAP_FAILED)
		munmap(map, length);

	if (fd >= 0)
	{
#ifdef DGDEBUG
		std::cout << "BackedStore: closing & deleting temp file " << filename << " BADGERS!" << std::endl;
#endif
		int rc = 0;
		do
		{
			rc = close(fd);
		}
		while (rc < 0 && errno == EINTR);
#ifdef DGDEBUG
		if (rc < 0)
			std::cout << "BackedStore: cannot close temp file fd: " << strerror(errno) << std::endl;
#endif
		rc = unlink(filename);
#ifdef DGDEBUG
		if (rc < 0)
			std::cout << "BackedStore: cannot delete temp file: " << strerror(errno) << std::endl;
#endif
		delete filename;
	}
}

bool BackedStore::append(const char *data, size_t len)
{
	if (fd < 0)
	{
#ifdef DGDEBUG
		std::cout << "BackedStore: appending to RAM" << std::endl;
#endif
		// Temp file not yet opened - try to write to RAM
		if (rambuf.size() + len > ramsize)
		{
			// Would exceed RAM threshold
			if (rambuf.size() + len > disksize)
			{
				// Would also exceed disk threshold
				// - give up
#ifdef DGDEBUG
				std::cout << "BackedStore: data would exceed both RAM and disk thresholds" << std::endl;
#endif
				return false;
			}

#ifdef DGDEBUG
			std::cout << "BackedStore: data would exceed RAM threshold; dumping RAM to disk" << std::endl;
#endif

			// Open temp file, dump current data in there,
			// leave code below this if{} to write current
			// data to the file as well
			filename = new char[tempdir.length() + 14];
			strncpy(filename, tempdir.c_str(), tempdir.length());
			strncpy(filename + tempdir.length(), "/__dgbsXXXXXX", 13);
			filename[tempdir.length() + 13] = '\0';
#ifdef DGDEBUG
			std::cout << "BackedStore: filename template: " << filename << std::endl;
#endif
			if ((fd = mkstemp(filename)) < 0)
			{
				std::ostringstream ss;
				ss << "BackedStore could not create temp file: " << strerror(errno);
				delete filename;
				throw std::runtime_error(ss.str().c_str());
			}
#ifdef DGDEBUG
			std::cout << "BackedStore: filename: " << filename << std::endl;
#endif
			size_t bytes_written = 0;
			ssize_t rc = 0;
			do
			{
				rc = write(fd, &(rambuf.front()) + bytes_written, rambuf.size() - bytes_written);
				if (rc > 0)
					bytes_written += rc;
			}
			while (bytes_written < rambuf.size() && (rc > 0 || errno == EINTR));
			if (rc < 0 && errno != EINTR)
			{
				std::ostringstream ss;
				ss << "BackedStore could not dump RAM buffer to temp file: " << strerror(errno);
				throw std::runtime_error(ss.str().c_str());
			}
			length = rambuf.size();
			rambuf.clear();
		}
		else
			rambuf.insert(rambuf.end(), data, data + len);
	}

	if (fd >= 0)
	{
#ifdef DGDEBUG
		std::cout << "BackedStore: appending to disk" << std::endl;
#endif
		// Temp file opened - try to write to disk
		if (map != MAP_FAILED)
			throw std::runtime_error("BackedStore could not append to temp file: store already finalised");
		if (len + length > disksize)
		{
#ifdef DGDEBUG
			std::cout << "BackedStore: data would exceed disk threshold" << std::endl;
#endif
			return false;
		}
		size_t bytes_written = 0;
		ssize_t rc = 0;
		do
		{
			rc = write(fd, data + bytes_written, len - bytes_written);
			if (rc > 0)
				bytes_written += rc;
		}
		while (bytes_written < len && (rc > 0 || errno == EINTR));
		if (rc < 0 && errno != EINTR)
		{
			std::ostringstream ss;
			ss << "BackedStore could not dump RAM buffer to temp file: " << strerror(errno);
			throw std::runtime_error(ss.str().c_str());
		}
		length += len;
	}

#ifdef DGDEBUG
	std::cout << "BackedStore: finished appending" << std::endl;
#endif

	return true;
}

size_t BackedStore::getLength() const
{
	if (fd >= 0)
		return length;
	else
		return rambuf.size();
}

void BackedStore::finalise()
{
	if (fd < 0)
		// No temp file - nothing to finalise
		return;

	lseek(fd, 0, SEEK_SET);
	map = mmap(0, length, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED)
	{
		std::ostringstream ss;
		ss << "BackedStore could not mmap() temp file: " << strerror(errno);
		throw std::runtime_error(ss.str().c_str());
	}
}

const char *BackedStore::getData() const
{
	if (fd < 0)
	{
#ifdef DGDEBUG
		std::cout << "BackedStore: returning pointer to RAM" << std::endl;
#endif
		return &(rambuf.front());
	}
	else
	{
#ifdef DGDEBUG
		std::cout << "BackedStore: returning pointer to mmap-ed file" << std::endl;
#endif
		if (map == MAP_FAILED)
			throw std::runtime_error("BackedStore could not return data pointer: store not finalised");
		return (const char*) map;
	}
}