MAINTAINERCLEANFILES = Makefile.in

lib_LTLIBRARIES = clamdscan.la icapscan.la kavdscan.la

#if HAVE_KAVCLIENT
#lib_LTLIBRARIES+=kavav.la 
#endif

if HAVE_LIBCLAMAV
lib_LTLIBRARIES+=clamav.la 
endif

clamav_la_SOURCES = clamav.cpp
clamav_la_CFLAGS = $(LIBCLAMAV_CFLAGS)
clamav_la_LDFLAGS = -module $(LIBCLAMAV_LIBS) -lclamav

#kavav_la_SOURCES = kavav.cpp
#kavav_la_CFLAGS = -I$(KAV_INCLUDES)
#kavav_la_LDFLAGS = -module $(KAV_LIBS)

clamdscan_la_SOURCES = clamdscan.cpp
clamdscan_la_LDFLAGS = -module

icapscan_la_SOURCES = icapscan.cpp
icapscan_la_LDFLAGS = -module

kavdscan_la_SOURCES = kavdscan.cpp
kavdscan_la_LDFLAGS = -module

#The kavav is unfinished - Wed 09th February 2005 DB
