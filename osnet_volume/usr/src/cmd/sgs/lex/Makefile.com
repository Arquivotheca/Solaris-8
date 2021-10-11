#
#ident	"@(#)Makefile.com	1.7	98/02/06 SMI"
#
# Copyright (c) 1993,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/lex/Makefile.com
#


MACHOBJS=	main.o sub1.o sub2.o sub3.o header.o parser.o
WHATOBJS=	whatdir.o
POBJECTS=	$(MACHOBJS) $(WHATOBJS)
POBJS=		$(POBJECTS:%=objs/%)

LIBRARY=	libl.a
VERS=		.1

LIBOBJS=	allprint.o libmain.o reject.o yyless.o yywrap.o
LIBOBJS_W=	allprint_w.o reject_w.o yyless_w.o
LIBOBJS_E=	reject_e.o yyless_e.o
OBJECTS=	$(LIBOBJS) $(LIBOBJS_W) $(LIBOBJS_E)

FORMS=		nceucform ncform nrform

# 32-bit environment mapfile
MAPFILE=		../common/mapfile-vers

include 	../../../../lib/Makefile.lib

# Override default source file derivation rule (in Makefile.lib)
# from objects
#
SRCS=		$(MACHOBJS:%.o=../common/%.c) \
		$(WHATOBJS:%.o=../../whatdir/common/%.c) \
		$(LIBOBJS:%.o=../common/%.c)

# XXX - Prior to the availability of both 32-bit and 64-bit Solaris,
# libl was delivered as a static library only. We need to maintain
# the availability of the static library in the 32-bit environment and
# hence will continue to deliver this. It continues to live in
# /usr/ccs/lib. Shared objects are also provided, however, because
# the run time linker only searches /usr/lib, the libl shared objects
# reside in /usr/lib rather than /usr/ccs/lib. Becase of how the
# make rules are currently set up, LIBS here refers only to the dynamic
# library and the lint library which allows the installation of these
# to /usr/lib. The static library is addressed with a specific install
# rule later in this makefile.
#
LIBS =          $(DYNLIB) $(LINTLIB)

# Tune ZDEFS to ignore undefined symbols for building the lex shared library
# since these symbols are to be resolved later.
#
$(DYNLIB):= ZDEFS = -z nodefs
$(DYNLIBCCC):= ZDEFS = -z nodefs
LINTSRCS=	../common/llib-l$(LIBNAME)

INCLIST=	$(INCLIST_$(MACH)) -I../../include -I../../include/$(MACH)
DEFLIST=	-DELF

# It is not very clean to base the conditional definitions as below, but
# this will have to do for now.
#
#$(LIBOBJS_W):=	DEFLIST = -DEUC -DJLSLEX  -DWOPTION -D$*=$*_w
objs/%_w.o:=	DEFLIST = -DEUC -DJLSLEX  -DWOPTION -D$*=$*_w
pics/%_w.o:=	DEFLIST = -DEUC -DJLSLEX  -DWOPTION -D$*=$*_w

#$(LIBOBJS_E):=	DEFLIST = -DEUC -DJLSLEX  -DEOPTION -D$*=$*_e
objs/%_e.o:=	DEFLIST = -DEUC -DJLSLEX  -DEOPTION -D$*=$*_e
pics/%_e.o:=	DEFLIST = -DEUC -DJLSLEX  -DEOPTION -D$*=$*_e

CPPFLAGS=	$(INCLIST) $(DEFLIST) $(CPPFLAGS.master)
LDLIBS=		$(LDLIBS.cmd)
BUILD.AR=	$(AR) $(ARFLAGS) $@ `$(LORDER) $(OBJS) | $(TSORT)`
LINTFLAGS=	-ax
LINTPOUT=	lintp.out

$(LINTLIB):=	LINTFLAGS = -nvx
$(ROOTCCSBINPROG):= FILEMODE = 0555

ROOTFORMS=	$(FORMS:%=$(ROOTCCSBIN)/%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRCS:../common/%=$(ROOTLINTDIR)/%)
STATICLIBDIR=	$(ROOTLIBDIR)
STATICLIB=	$(LIBRARY:%=$(STATICLIBDIR)/%)

# XXX - We need to include a link for /usr/ccs/lib/libl.so that
# points to libl.so.1 in /usr/lib. Otherwise, the static library
# in /usr/ccs/lib will always be picked up due to the order of
# search of the libraries. This rule is only necessary for the 32-bit
# environment since a 64-bit static libl is not delivered.
#
DYNLINKLIBDIR=	$(ROOTLIBDIR)
DYNLINKLIB=	$(LIBLINKS:%=$(DYNLINKLIBDIR)/%)

# Need to make sure lib-make's are warning free
$(DYNLIB) $(STATICLIB):=	CCVERBOSE = -v
$(DYNLIB) $(STATICLIB):=	CFLAGS += -v
$(DYNLIB) $(STATICLIB):=	CFLAGS64 += -v

DYNFLAGS += -M $(MAPFILE)
#LDLIBS += -lc

CLEANFILES +=	../common/parser.c $(LINTPOUT) $(LINTOUT)
CLOBBERFILES +=	$(LIBS) $(LIBRARY)
