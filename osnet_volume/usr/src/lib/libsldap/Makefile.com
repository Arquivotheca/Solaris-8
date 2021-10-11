#
# ident "@(#)Makefile.com 1.1     99/07/07 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libsldap/Makefile.com
#

LIBRARY= libsldap.a
VERS= .1

CLOBBERFILES += lint.out

SLDAPOBJ=	ns_common.o	ns_reads.o	ns_writes.o \
		ns_connect.o	ns_config.o	ns_error.o \
		ns_cache_door.o ns_getalias.o	ns_trace.o \
		ns_init.o	ns_crypt.o

OBJECTS=	$(SLDAPOBJ)

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile

SRCS=		$(SLDAPOBJ:%.o=../common/%.c)

LIBS =		$(DYNLIB) $(LINTLIB)

LOCFLAGS +=	-D_REENTRANT -DSUNW_OPTIONS -DTHREAD_SUNOS5_LWP

# definitions for lint

LINTSRC=        $(LINTLIB:%.ln=%)
ROOTLINTDIR=    $(ROOTLIBDIR)
ROOTLINT=       $(LINTSRC:%=$(ROOTLINTDIR)/%)
LINTFLAGS +=	-I../common $(LOCFLAGS)
LINTFLAGS64 +=	-I../common -Xarch=v9 $(LOCFLAGS)
LINTOUT=        lint.out

CLEANFILES += 	$(LINTOUT) $(LINTLIB)
CLOBBERFILES +=	$(MAPFILE)

$(LINTLIB):= SRCS=../common/llib-lsldap

# Local Libsldap definitions

CFLAGS +=	-v $(LOCFLAGS) 
CFLAGS64 +=	-v $(LOCFLAGS)
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lnsl -lldap -lc -ldoor

.KEEP_STATE:

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile


objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

lint: $(LINTLIB) lintcheck

lintcheck:
	$(LINT.c) $(SRCS) > $(LINTOUT) 2>&1

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)

# include library targets
include ../../Makefile.targ
