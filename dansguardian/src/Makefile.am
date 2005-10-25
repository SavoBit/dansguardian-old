# Please don't delete platform.h.in. it's not very nice.
MAINTAINERCLEANFILES = Makefile.in #platform.h.in

clean_SUBDIRS= . downloadmanagers contentscanners 

# A MASSIVE thank you to Lawrence Manning for pointing out the
# -export-dynamic option was needed to solve a long problem
# which stopped DG2.9 from actually running ever.
# Daniel Barron Fri 18th March 2005

# Unfortunately, since it was never made to work on anything
# other than Linux, libtool support has been removed in favour
# of configure-time options (at DB's request).

#AM_CXXFLAGS = @CXXFLAGS@ -fexceptions
AM_CXXFLAGS = -fexceptions $(LIBCLAMAV_CFLAGS) $(LIBPCRE_CFLAGS)
#AM_CFLAGS = @CFLAGS@

DGCONFFILE = @DGCONFFILE@
DGLOGLOCATION = @DGLOGLOCATION@
DGPIDDIR = @DGPIDDIR@
DGPROXYUSER = @DGPROXYUSER@ 
DGPROXYGROUP = @DGPROXYGROUP@

if HAVE_PCRE
PCRE_LDFLAGS = $(LIBPCRE_LIBS) -lpcreposix
else
PCRE_LDFLAGS =
endif

if HAVE_CLAMAV
CLAMAV_LDFLAGS = $(LIBCLAMAV_LIBS)
CLAMAV_SOURCE = contentscanners/clamav.cpp
else
CLAMAV_LDFLAGS =
CLAMAV_SOURCE =
endif

#if HAVE_KAVCLIENT
#KAVAV_SOURCES = contentscanners/kavav.cpp
#KAVAV_CFLAGS = -I$(KAV_INCLUDES)
#KAVAV_LDFLAGS = $(KAV_LIBS)
#else
#KAVAV_SOURCES =
#KAVAV_CFLAGS =
#KAVAV_LDFLAGS =
#endif

if ENABLE_CLAMD
CLAMDSCAN_SOURCE = contentscanners/clamdscan.cpp
else
CLAMDSCAN_SOURCE =
endif

if ENABLE_ICAP
ICAPSCAN_SOURCE = contentscanners/icapscan.cpp
else
ICAPSCAN_SOURCE =
endif

if ENABLE_KAVD
KAVDSCAN_SOURCE = contentscanners/kavdscan.cpp
else
KAVDSCAN_SOURCE =
endif

DEFAULTDM_SOURCE = downloadmanagers/default.cpp

if ENABLE_FANCYDM
FANCYDM_SOURCE = downloadmanagers/fancy.cpp
else
FANCYDM_SOURCE =
endif

sbin_PROGRAMS = dansguardian 

dansguardian_SOURCES = String.cpp String.hpp \
                       FDTunnel.cpp FDTunnel.hpp \
                       ConnectionHandler.cpp ConnectionHandler.hpp \
                       DataBuffer.cpp DataBuffer.hpp \
                       HTTPHeader.cpp HTTPHeader.hpp \
                       NaughtyFilter.cpp NaughtyFilter.hpp \
                       RegExp.cpp RegExp.hpp \
		       FDFuncs.cpp FDFuncs.hpp \
		       BaseSocket.cpp BaseSocket.hpp \
                       Socket.cpp Socket.hpp \
                       FatController.cpp FatController.hpp \
                       UDSocket.cpp UDSocket.hpp \
                       SysV.cpp SysV.hpp \
                       ListContainer.cpp ListContainer.hpp \
                       Ident.cpp Ident.hpp \
                       HTMLTemplate.cpp HTMLTemplate.hpp \
                       LanguageContainer.cpp LanguageContainer.hpp \
                       DynamicURLList.cpp DynamicURLList.hpp \
		       DynamicIPList.cpp DynamicIPList.hpp \
                       ImageContainer.cpp ImageContainer.hpp \
                       OptionContainer.cpp OptionContainer.hpp \
                       FOptionContainer.cpp FOptionContainer.hpp \
                       ListManager.cpp ListManager.hpp \
                       md5.cpp md5.hpp \
                       DownloadManager.cpp DownloadManager.hpp \
                       ConfigVar.cpp ConfigVar.hpp \
                       ContentScanner.cpp ContentScanner.hpp \
		       SocketArray.cpp SocketArray.hpp \
                       platform.h \
                       dansguardian.cpp \
		       $(CLAMAV_SOURCE) $(ICAPSCAN_SOURCE) \
		       $(KAVDSCAN_SOURCE) $(CLAMDSCAN_SOURCE) \
		       $(DEFAULTDM_SOURCE) $(FANCYDM_SOURCE) \
		       $(KAVAV_SOURCES)

dansguardian_LDADD = $(PCRE_LDFLAGS) $(ZLIB) $(CLAMAV_LDFLAGS) $(KAVAV_LDFLAGS)

#dansguardian_CFLAGS = $(CLAMAV_CFLAGS) $(KAVAV_CFLAGS)
