#
#ident	"@(#)Makefile.com	1.6	98/02/06 SMI"
#
# Copyright (c) 1993,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/yacc/Makefile.com
#

COMOBJS=	y1.o y2.o y3.o y4.o
WHATOBJS=	whatdir.o
POBJECTS=	$(COMOBJS) $(WHATOBJS)
POBJS=		$(POBJECTS:%=objs/%)

OBJECTS=	libmai.o libzer.o

LIBRARY=	liby.a
VERS=		.1
YACCPAR=	yaccpar

# 32-bit environment mapfile
MAPFILE=		../common/mapfile-vers

include ../../../../lib/Makefile.lib

# Override default source file derivation rule (in Makefile.lib)
# from objects
#
SRCS=		$(COMOBJS:%.o=../common/%.c) \
		$(WHATOBJS:%.o=../../whatdir/common/%.c) \
		$(OBJECTS:%.o=../common/%.c)


# XXX - Prior to the availability of both 32-bit and 64-bit Solaris,
# liby was delivered as a static library only. We need to maintain
# the availability of the static library in the 32-bit environment and
# hence will continue to deliver this. It continues to live in
# /usr/ccs/lib. Shared objects are also provided, however, because
# the run time linker only searches /usr/lib, the liby shared objects
# reside in /usr/lib rather than /usr/ccs/lib. Becase of how the
# make rules are currently set up, LIBS here refers only to the dynamic
# library and the lint library which allows the installation of these
# to /usr/lib. The static library is addressed with a specific install
# rule later in this makefile.
#
LIBS =          $(DYNLIB) $(LINTLIB)

# Tune ZDEFS to ignore undefined symbols for building the yacc shared library
# since these symbols (mainly yyparse) are to be resolved elsewhere.
#
$(DYNLIB):= ZDEFS = -z nodefs
$(DYNLIBCCC):= ZDEFS = -z nodefs
LINTSRCS=	../common/llib-l$(LIBNAME)

INCLIST=	-I../../include -I../../include/$(MACH)
CPPFLAGS=	$(INCLIST) $(DEFLIST) $(CPPFLAGS.master)
LDLIBS=		$(LDLIBS.cmd)
BUILD.AR=	$(AR) $(ARFLAGS) $@ `$(LORDER) $(OBJS) | $(TSORT)`
LINTFLAGS=	-ax
LINTPOUT=	lintp.out

$(LINTLIB):=	LINTFLAGS = -nvx
$(ROOTCCSBINPROG):= FILEMODE = 0555

ROOTYACCPAR=	$(YACCPAR:%=$(ROOTCCSBIN)/%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRCS:../common/%=$(ROOTLINTDIR)/%)
STATICLIBDIR=	$(ROOTLIBDIR)
STATICLIB=	$(LIBRARY:%=$(STATICLIBDIR)/%)

# XXX - We need to include a link for /usr/ccs/lib/liby.so that
# points to liby.so.1 in /usr/lib. Otherwise, the static library
# in /usr/ccs/lib will always be picked up due to the order of
# search of the libraries. This rule is only necessary for the 32-bit
# environment since a 64-bit static liby is not delivered.
#
DYNLINKLIBDIR=	$(ROOTLIBDIR)
DYNLINKLIB=	$(LIBLINKS:%=$(DYNLINKLIBDIR)/%)

# Need to make sure lib-make's are warning free
$(DYNLIB) $(STATICLIB):=	CCVERBOSE = -v
$(DYNLIB) $(STATICLIB):=	CFLAGS += -v
$(DYNLIB) $(STATICLIB):=	CFLAGS64 += -v

DYNFLAGS += -M $(MAPFILE)

CLEANFILES +=	$(LINTPOUT) $(LINTOUT)
CLOBBERFILES +=	$(LIBS) $(LIBRARY)
