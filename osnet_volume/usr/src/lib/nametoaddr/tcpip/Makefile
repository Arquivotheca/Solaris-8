#
#ident	"@(#)Makefile	1.16	93/04/06 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/nametoaddr/tcpip/Makefile
#
# WARNING FOR DEVELOPERS:
# The source in here is also used by /usr/src/lib/nametoaddr/switch
# build switch.so.2. Any changes done here should be followed by a build
# of /usr/src/lib/nametoaddr/switch/Makefile and test of switch.so.2.
#
LIBRARY= libtcpip.a
VERS= .2

OBJECTS= tcpip.o file_db.o

# include library definitions
include ../../Makefile.lib

# set exclusively to avoid libtcpip.so being built up.
# do not change ordering of includes and DYNLIB
DYNLIB= tcpip.so$(VERS)
LIBLINKS= tcpip.so

# See below.
PSLIB= $(DYNLIB).ps
ROOTETCLIB = $(ROOT)/etc/lib
ROOTPSLIB =  $(ROOTETCLIB)/$(DYNLIB)
ROOTPSLINK = $(ROOTETCLIB)/$(LIBLINKS)

$(ROOTPSLIB) := FILEMODE= 755

$(ROOTETCLIB)/%: %.ps
	$(INS.rename)

LINTFLAGS=
CPPFLAGS += -I../inc -D_REENTRANT
LDLIBS += -lsocket -lnsl -lc

LIBS += $(DYNLIB)

.KEEP_STATE:

all: $(LIBS) $(PSLIB)

# Generate a version that does not have
# a declared dependency on libc. This version
# is installed on the usr file system, and
# installed on the root directory /usr/lib
# in the case of diskless clients and hosts
# that mount /usr over NFS. When installed
# on the root file system it is renamed tcpip.so.
# Part of the diskless booting changes.
$(PSLIB): pics .WAIT $$(PICS)
	$(LD) -o $@ -dy -G $(DYNFLAGS) $(PICS)
	$(POST_PROCESS_SO)

# note conditional assignment below
install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTPSLIB) $(ROOTPSLINK)

$(ROOTPSLINK): $(ROOTPSLIB)
	$(INS.liblink)

# include library targets
include ../../Makefile.targ

# to avoid make warnings, this conditional assignment must
# follow the include of Makefile.targ
$(ROOTPSLIB) $(ROOTPSLINK)	:= ROOTLIBDIR=$(ROOTETCLIB)
