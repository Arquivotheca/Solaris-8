#
# Copyright (c) 1994-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libthread_db/sparc/Makefile
#
#pragma ident	"@(#)Makefile	1.7	99/01/25 SMI"
#

.SUFFIXES:
.SUFFIXES: 	.extdcl.h .pubdcl.h .stadcl.h .i $(SUFFIXES)

MACH64=sparcv9
LIBTHREAD_DBDIR=$(SRC)/lib/libthread_db
LIBTHREAD_DIR=$(SRC)/lib/libthread

COMMONDIR=$(LIBTHREAD_DBDIR)/common
SPARCDIR=$(LIBTHREAD_DBDIR)/sparc
ARCHDIR=$(LIBTHREAD_DBDIR)/$(MACH64)
ROOTSYS_DIR=$(ROOT)/usr/include/sys

PROD=	td
VERS= .1
SCCS=sccs

LIBRARY= libthread_db.a

COBJECTS= td.o		\
	td_po.o		\
	td_to.o		\
	td_so.o		\
	td_error.o	\
	td_event.o	

XOBJECTS= xtd_po.o

XV9OBJECTS= xtd_to.o

OBJECTS= $(COBJECTS) $(XV9OBJECTS) $(XOBJECTS)

SRCS=	$(OBJECTS:%.o=%.c)

MAPDIR=		../spec/sparcv9
MAPFILE=	$(MAPDIR)/mapfile.1
MAPOPTS=	$(MAPFILE:%=-M %)
CLOBBERFILES += $(MAPFILE)

CSRCS=	$(COBJECTS:%.o=$(COMMONDIR)/%.c)
CHDRS=	$(COBJECTS:%.o=$(COMMONDIR)/%.h)
CLOCHDRS=	$(COMMONDIR)/td_impl.h        $(COMMONDIR)/td_po_impl.h     \
		$(COMMONDIR)/td_to_impl.h

XV9SRCS=	$(XV9OBJECTS:%.o=$(ARCHDIR)/%.c)
XSRCS=	$(XOBJECTS:%.o=$(SPARCDIR)/%.c)
XHDRS=	$(XOBJECTS:%.o=$(SPARCDIR)/%.h)

LOCHDRS=	$(CLOCHDRS) $(XLOCHDRS)

LIBTHREADHDRS=	$(LIBTHREAD_DIR)/common/libthread.h 		\
		$(LIBTHREAD_DIR)/common/debug.h			\
		$(ROOTSYS_DIR)/synch32.h			\
		$(LIBTHREAD_DIR)/inc/utrace.h			\
		$(LIBTHREAD_DIR)/$(MACH)/ml/machlibthread.h

HDRS=	$(CHDRS) $(XHDRS) $(LOCHDRS) $(LIBTHREADHDRS)

CEXTERNS=	$(COMMONDIR)/$(PROD).extdcl.h
CPUBLICS=	$(COMMONDIR)/$(PROD).pubdcl.h
CDCLHDRS=	$(CEXTERNS) $(CPUBLICS) $(CSTATICS)

XEXTERNS=	$(SPARCDIR)/x$(PROD).extdcl.h
XPUBLICS=	$(SPARCDIR)/x$(PROD).pubdcl.h
XDCLHDRS=	$(XEXTERNS) $(XPUBLICS) $(XSTATICS)

DCLHDRS=	$(CDCLHDRS) $(XDCLHDRS)

objs/%.o pics/%.o:	${ARCHDIR}/%.c  $(HDRS) $(DCLHDRS)
	$(COMPILE.c) $(LCFLAGS) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o:	${COMMONDIR}/%.c  $(HDRS) $(DCLHDRS)
	$(COMPILE.c) $(LCFLAGS) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o:	${SPARCDIR}/%.c  $(HDRS) $(DCLHDRS)
	$(COMPILE.c) $(LCFLAGS) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o:	%.c $(HDRS) $(DCLHDRS)
	$(COMPILE.c) $(LCFLAGS) -o $@ $<
	$(POST_PROCESS_O)

# include library definitions
include ../../Makefile.lib
include ../../Makefile.lib.64

LCFLAGS += -D__sparcv9 -Dsparcv9 -D_SYSCALL32 -D_SYSCALL32_IMPL 
CPPFLAGS += -I$(LIBTHREAD_DIR)/inc -I$(LIBTHREAD_DIR)/common 		\
	-I$(LIBTHREAD_DIR)/$(MACH64) -I$(LIBTHREAD_DIR)/$(MACH64)/ml 	\
	-I$(LIBTHREAD_DIR)/$(MACH)/ml -I$(LIBTHREAD_DIR)/sys/inc	\
	-I$(COMMONDIR) -I$(SPARCDIR) -I.

#	-DTEST_PS_CALLS

DYNFLAGS +=	$(MAPOPTS)
LIBS += $(DYNLIB)
LDLIBS += -ldl -lc
ZDEFS=

$(DYNLIB): $(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) `basename $@`

.KEEP_STATE:

all:  $(DYNLIB) 

install: all $(ROOTLIBDIR64)/${DYNLIB} $(ROOTLINKS64)

# include library targets
include ../../Makefile.targ

lint: 		
	$(LINT.c) -o llib-l$(LIBNAME).ln $(CFLAGS) $(LCFLAGS) \
		$(CSRCS) $(XSRCS) $(XV9SRCS) > $(LINTOUT) 2>&1

llib-l%.ln:	%.c
	lint -o $@ $<

.c.i	:
	$(COMPILE.cpp) $<  > $@

clean_all:	clean
	rm -f $(XSTATICS) $(XEXTERNS) $(XEXTERNS) 

