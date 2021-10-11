#
#ident  "@(#)Makefile.com	1.1	99/04/02 SMI"
#
# Copyright (c) 1999, by Sun Microsystems, Inc.
# All rights reserved.
#
LIBRARY= libslp.a
VERS= .1

CLOBBERFILES += lint.out

OBJECTS= \
SLPFindAttrs.o \
SLPFindSrvTypes.o \
SLPFindSrvs.o \
SLPOpen.o \
SLPReg.o \
SLPUtils.o \
SLPParseSrvURL.o \
SLPGetRefreshInterval.o \
slp_queue.o \
slp_utils.o \
slp_search.o \
slp_ua_common.o \
slp_net.o \
slp_net_utils.o \
slp_ipc.o \
slp_config.o \
slp_utf8.o \
slp_targets.o \
slp_da_cache.o \
slp_jni_support.o \
DAAdvert.o \
SAAdvert.o \
slp_auth.o

# libslp build rules
objs/%.o profs/%.o pics/%.o: ../clib/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile

CPPFLAGS +=	-I../clib -I$(JAVA_ROOT)/include -I$(JAVA_ROOT)/include/solaris -D_REENTRANT
LDLIBS +=	-lc -lnsl -lsocket -lthread -ldl
DYNFLAGS +=	-M $(MAPFILE)

SRCS=	$(OBJECTS:%.o=../clib/%.c)

LIBS = $(DYNLIB) $(LINTLIB)

LINTSRC=        $(LINTLIB:%.ln=%)
ROOTLINTDIR=    $(ROOTLIBDIR)
ROOTLINT=       $(LINTSRC:%=$(ROOTLINTDIR)/%)
LINTFLAGS +=	-I../clib
LINTFLAGS64 +=	-I../clib -Xarch=v9
LINTOUT=        lint.out
CLOBBERFILES += $(MAPFILE)

$(LINTLIB):= SRCS=../common/llib-lslp

.KEEP_STATE:

$(DYNLIB) : $(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

$(ROOTLINTDIR)/%:	../common/%
	$(INS.file)

lint:	$(LINTLIB) lintcheck

lintcheck: $$(SRCS)
	$(LINT.c) $(SRCS) > $(LINTOUT) 2>&1

# include library targets

include ../../Makefile.targ
