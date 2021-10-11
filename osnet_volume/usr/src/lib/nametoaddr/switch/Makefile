#
#ident	"@(#)Makefile	1.11	93/04/13 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# lib/nametoaddr/switch/Makefile
#
# Creates switch.so.1 that is recommended to be the only
# nametoaddr lib for all the inet family transports in the
# /etc/netconfig file. It calls the libnsl routines __switch_gethostbyYY()
# that follow the policy described in the /etc/nsswitch.conf.
# This is to maintains consistency between netdir host/service
# lookup policy and gethostbyYY lookup policy.
#
# WARNING FOR DEVELOPERS:
# This library is built using source under /usr/src/lib/nametoaddr/tcpip.
#

LIBRARY= libswitch.a
VERS= .2

OBJECTS= tcpip.o

# include library definitions
include ../../Makefile.lib

# set exclusively to avoid libswitch.so being built up.
# do not change ordering of includes and DYNLIB
DYNLIB= switch.so$(VERS)
LIBLINKS= switch.so

CPPFLAGS += -I../inc -DSWITCH -D_REENTRANT
LINTFLAGS=
LDLIBS += -lnsl -lsocket -lc

LIBS += $(DYNLIB)

.KEEP_STATE:

objs/%.o profs/%.o pics/%.o: ../tcpip/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

all: $(LIBS)

install: $(ROOTLIBS) $(ROOTLINKS)

# include library targets
include ../../Makefile.targ
