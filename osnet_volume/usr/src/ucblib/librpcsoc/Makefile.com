#
# Copyright (c) 1998 BY Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.4	99/09/20 SMI"
#
# ucblib/librpcsoc/Makefile.com
#
LIBRARY= librpcsoc.a
VERS = .1

CLOBBERFILES += lint.out


OBJECTS= clnt_tcp.o clnt_udp.o getrpcport.o rtime.o svc_tcp.o svc_udp.o get_myaddress.o

# include library definitions
include $(SRC)/lib/Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES += $(MAPFILE)
SRCS=		$(OBJECTS:%.o=../%.c)

objs/%.o pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

LIBS = $(DYNLIB)

LDLIBS += -lnsl -lsocket -lc
CFLAGS += -DPORTMAP
CFLAGS64 += -DPORTMAP
DYNFLAGS += -M $(MAPFILE)

ROOTLIBDIR=	$(ROOT)/usr/ucblib
ROOTLIBDIR64=   $(ROOT)/usr/ucblib/$(MACH64)


LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINTDIR64= $(ROOTLIBDIR)/$(MACH64)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%)
ROOTLINT64= $(LINTSRC:%=$(ROOTLINTDIR64)/%)


# install rule for lint library target
$(ROOTLINTDIR)/%: ../%
	$(INS.file)
$(ROOTLINTDIR64)/%: ../%
	$(INS.file)


# definitions for lint

LINTFLAGS=      -u
LINTFLAGS64=    -u -D__sparcv9
LINTOUT=        lint.out
CLEANFILES +=   $(LINTOUT) $(LINTLIB)


CPPFLAGS = -I$(ROOT)/usr/ucbinclude -I../../../lib/libc/inc $(CPPFLAGS.master)

.KEEP_STATE:

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

lint: $(LINTLIB)


$(LINTLIB):= SRCS=../llib-lrpcsoc
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=


# include library targets
include $(SRC)/lib/Makefile.targ
include ../../Makefile.ucbtarg

# install rule for lint library target
$(ROOTLINTDIR)/%: ../%
	$(INS.file)

