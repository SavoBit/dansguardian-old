MAINTAINERCLEANFILES = Makefile.in platform.h.in

SUBDIRS= . downloadmanagers contentscanners 

# A MASSIVE thank you to Lawrence Manning for pointing out the
# -export-dynamic option was needed to solve a long problem
# which stopped DG2.9 from actually running ever.
# Daniel Barron Fri 18th March 2005

AM_CXXFLAGS = @CXXFLAGS@ -fexceptions -g -export-dynamic
AM_CFLAGS = @CFLAGS@

DGCONFFILE = @DGCONFFILE@
DGLOGLOCATION = @DGLOGLOCATION@
DGPIDDIR = @DGPIDDIR@
DGPROXYUSER = @DGPROXYUSER@ 
DGPROXYGROUP = @DGPROXYGROUP@

sbin_PROGRAMS = dansguardian 

dansguardian_SOURCES = String.cpp String.hpp \
                       FDTunnel.cpp FDTunnel.hpp \
                       ConnectionHandler.cpp ConnectionHandler.hpp \
                       DataBuffer.cpp DataBuffer.hpp \
                       HTTPHeader.cpp HTTPHeader.hpp \
                       NaughtyFilter.cpp NaughtyFilter.hpp \
                       RegExp.cpp RegExp.hpp \
                       Socket.cpp Socket.hpp \
                       FatController.cpp FatController.hpp \
                       UDSocket.cpp UDSocket.hpp \
                       SysV.cpp SysV.hpp \
                       ListContainer.cpp ListContainer.hpp \
                       Ident.cpp Ident.hpp \
                       HTMLTemplate.cpp HTMLTemplate.hpp \
                       LanguageContainer.cpp LanguageContainer.hpp \
                       DynamicURLList.cpp DynamicURLList.hpp \
                       ImageContainer.cpp ImageContainer.hpp \
                       OptionContainer.cpp OptionContainer.hpp \
                       FOptionContainer.cpp FOptionContainer.hpp \
                       ListManager.cpp ListManager.hpp \
                       md5.cpp md5.hpp \
                       DownloadManager.cpp DownloadManager.hpp \
                       ConfigVar.cpp ConfigVar.hpp \
                       ContentScanner.cpp ContentScanner.hpp \
                       platform.h \
                       dansguardian.cpp dansguardian.hpp

dansguardian_LDADD = $(ZLIB) -lltdl	
