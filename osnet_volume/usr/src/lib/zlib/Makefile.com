# Makefile for zlib
# Copyright (C) 1995-1998 Jean-loup Gailly.
# For conditions of distribution and use, see copyright notice in zlib.h 

# To compile and test, type:
#   ./configure; make test
# The call of configure is optional if you don't have special requirements
# If you wish to build zlib as a shared library, use: ./configure -s
#
#ident	"@(#)Makefile.com	1.1	99/10/08 SMI"


LIBRARY=libz.a
VERS= .1

OBJECTS = adler32.o compress.o crc32.o gzio.o uncompr.o deflate.o trees.o \
       zutil.o inflate.o infblock.o inftrees.o infcodes.o infutil.o inffast.o

include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
CFLAGS += -DHAVE_UNISTD_H -DUSE_MMAP -I..
LIBS =	$(DYNLIB) $(LINTLIB)
LDLIBS += -lc

# definitions for lint

LINTFLAGS=      -u -I..
LINTFLAGS64=    -u -I..
LINTOUT=        lint.out

LINTSRC=        $(LINTLIB:%.ln=%)
ROOTLINTDIR=    $(ROOTLIBDIR)
ROOTLINT=       $(LINTSRC:%=$(ROOTLINTDIR)/%)
$(LINTLIB):= SRCS=../llib-lz
$(LINTLIB):= LINTFLAGS=-nvx

CLEANFILES +=   $(LINTOUT) $(LINTLIB)
CLOBBERFILES += $(MAPFILE)

DYNFLAGS +=     -M $(MAPFILE)

DISTDIRS= msdos nt amiga os2 contrib contrib/asm386 contrib/asm586 \
    contrib/asm686 contrib/iostream contrib/iostream2 contrib/untgz \
    contrib/minizip contrib/delphi contrib/delphi2
ROOTSRC= $(ROOT)/usr/share/src/zlib
ROOTSRCDIRS=   $(DISTDIRS:%=$(ROOTSRC)/%)
DISTFILES:sh= cat ../distfiles
ROOTDISTFILES= $(DISTFILES:%=$(ROOTSRC)/%)
$(ROOTDISTFILES) := FILEMODE= 0444
$(ROOTSRC)/configure := FILEMODE= 0555

.NO_PARALLEL: $(ROOTSRCDIRS)

OBJA =
# to use the asm code: make OBJA=match.o

TEST_OBJS = example.o minigzip.o

.KEEP_STATE:

lint: $(LINTLIB)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

include ../../Makefile.targ

install_src: $(ROOTSRC) .WAIT $(ROOTSRCDIRS) .WAIT $(ROOTDISTFILES)

$(ROOTSRC) $(ROOTSRCDIRS):
	$(INS.dir)

$(ROOTSRC)/%: ../%
	$(INS.file)

test: all .WAIT minigzip .WAIT example
	@LD_LIBRARY_PATH=.:$(LD_LIBRARY_PATH) ; export LD_LIBRARY_PATH; \
	echo hello world | ./minigzip | ./minigzip -d || \
	  echo '		*** minigzip test FAILED ***' ; \
	if ./example; then \
	  echo '		*** zlib test OK ***'; \
	else \
	  echo '		*** zlib test FAILED ***'; \
	fi

match.o: match.S
	$(CPP) match.S > _match.s
	$(CC) -c _match.s
	mv _match.o match.o
	rm -f _match.s

example: ../example.c $(LIBS)
	$(LINK.c) -o $@ ../example.c $(LDLIBS) -lz

minigzip: ../minigzip.c $(LIBS)
	$(LINK.c) -o $@ ../minigzip.c $(LDLIBS) -lz

# DO NOT DELETE THIS LINE -- make depend depends on it.

adler32.o: zlib.h zconf.h
compress.o: zlib.h zconf.h
crc32.o: zlib.h zconf.h
deflate.o: deflate.h zutil.h zlib.h zconf.h
example.o: zlib.h zconf.h
gzio.o: zutil.h zlib.h zconf.h
infblock.o: infblock.h inftrees.h infcodes.h infutil.h zutil.h zlib.h zconf.h
infcodes.o: zutil.h zlib.h zconf.h
infcodes.o: inftrees.h infblock.h infcodes.h infutil.h inffast.h
inffast.o: zutil.h zlib.h zconf.h inftrees.h
inffast.o: infblock.h infcodes.h infutil.h inffast.h
inflate.o: zutil.h zlib.h zconf.h infblock.h
inftrees.o: zutil.h zlib.h zconf.h inftrees.h
infutil.o: zutil.h zlib.h zconf.h infblock.h inftrees.h infcodes.h infutil.h
minigzip.o:  zlib.h zconf.h
trees.o: deflate.h zutil.h zlib.h zconf.h trees.h
uncompr.o: zlib.h zconf.h
zutil.o: zutil.h zlib.h zconf.h  

pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../%
	$(INS.file)
