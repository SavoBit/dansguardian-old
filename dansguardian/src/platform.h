#ifndef __H_PLATFORM
#define __H_PLATFORM 1

#ifdef HAVE_CONFIG_H
	#include "dgconfig.h"
#endif

#ifdef HAVE_SYS_WAIT_H
	#include <sys/wait.h>
#else
	#ifdef HAVE_WAIT_H
		#include <wait.h>
	#else
		#error "You have no sys/wait.h and no wait.h"
	#endif
#endif

#ifdef HAVE_SYS_SELECT_H
	#include <sys/select.h>
#else
	#error "You have no sys/select.h"
#endif

#endif //ifndef __H_PLATFORM
