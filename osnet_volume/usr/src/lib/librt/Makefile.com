#
# Copyright (c) 1990,1993,1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.6	99/01/25 SMI"
#
# lib/librt/Makefile.com

LIBRARY=	librt.a
VERS=		.1

OBJECTS=	\
	aio.o		\
	clock_timer.o	\
	fdatasync.o	\
	mqueue.o	\
	pos4.o		\
	pos4obj.o	\
	sched.o		\
	sem.o		\
	shm.o		\
	sigrt.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
SRCS=		$(OBJECTS:%.o=../common/%.c)

# Override LIBS so that only a dynamic library is built.

LIBS =		$(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS=	-u
LINTFLAGS64=	-u -D__sparcv9
LINTOUT=	lint.out

LINTSRC=        $(LINTLIB:%.ln=%)
ROOTLINTDIR=    $(ROOTLIBDIR)
ROOTLINT=       $(LINTSRC:%=$(ROOTLINTDIR)/%)
ROOTLINTDIR64=  $(ROOTLIBDIR64)
ROOTLINT64=     $(LINTSRC:%=$(ROOTLINTDIR64)/%)
ROOTLINKS64=    $(ROOTLIBDIR64)/$(LIBLINKS)

CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS	+=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	 -laio -lc

CPPFLAGS += -I../inc -I../../libc/inc $(CPPFLAGS.master) -D_REENTRANT
CPPFLAGS64 += -I../inc -I../../libc/inc $(CPPFLAGS.master) -D_REENTRANT


#
# If and when somebody gets around to messaging this, CLOBBERFILE should not
# be cleared (so that any .po file will be clobbered.
#
CLOBBERFILES=	test $(MAPFILE)

.KEEP_STATE:

all: $(LIBS)

lint: $(LINTLIB)

$(DYNLIB): 	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# install rule for 64 bit lint library target
$(ROOTLINTDIR64)/%.ln:	%.ln
	$(INS.file)
	cd $(ROOTLINTDIR64); \
		$(RM) llib-lposix4.ln ; \
		$(SYMLINK) ./llib-lrt.ln llib-lposix4.ln ;

# Include library targets
#
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%:	../common/% ../common/llib-lrt
	$(INS.file)
	cd $(ROOTLINTDIR); \
		$(RM) llib-lposix4 ; \
		$(SYMLINK) ./llib-lrt llib-lposix4 ; \
		$(RM) llib-lposix4.ln ; \
		$(SYMLINK) ./llib-lrt.ln llib-lposix4.ln ;


# install rules for 32-bit librt.so in /usr/lib
$(ROOTLINKS) := INS.liblink= \
	$(RM) $@; $(SYMLINK) $(LIBLINKPATH)$(LIBLINKS)$(VERS) $@; \
		cd $(ROOTLIBDIR); \
		$(RM)  libposix4.so$(VERS) libposix4.so; \
		$(SYMLINK) librt.so$(VERS) libposix4.so$(VERS); \
		$(SYMLINK) libposix4.so$(VERS) libposix4.so;

# install rules for 64-bit librt.so in /usr/lib/sparcv9
$(ROOTLIBDIR64)/$(LIBLINKS) := INS.liblink64 = \
	-$(RM) $@; \
	cd $(ROOTLIBDIR64); \
	$(RM) libposix4.so$(VERS) libposix4.so ; \
	$(SYMLINK) $(LIBLINKS)$(VERS) $(LIBLINKS); \
	$(SYMLINK) librt.so$(VERS) libposix4.so$(VERS); \
	$(SYMLINK) libposix4.so$(VERS) libposix4.so

