#include "FDFuncs.hpp"

// wrapper around FD read that restarts on EINTR
int readEINTR(int fd, char *buf, unsigned int count)
{
	int rc;
	errno = 0;
	while (true) {		// using the while as a restart point with continue
		rc = read(fd, buf, count);
		if (rc < 0) {
			if (errno == EINTR) {
				continue;  // was interupted by a signal so restart
			}
		}
		break;  // end the while
	}
	return rc;  // return status
}

// wrapper around FD write that restarts on EINTR
int writeEINTR(int fd, char *buf, unsigned int count)
{
	int rc;
	errno = 0;
	while (true) {		// using the while as a restart point with continue
		rc = write(fd, buf, count);
		if (rc < 0) {
			if (errno == EINTR) {
				continue;  // was interupted by a signal so restart
			}
		}
		break;  // end the while
	}
	return rc;  // return status
}
