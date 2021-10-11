# Generated automatically from Makefile.in by configure.
#
# $Id: Makefile.in,v 1.14 1999/03/02 04:16:40 eric Exp $
#
## Makefile for mkisofs.
# 
#ident	"@(#)Makefile	1.2	99/10/17 SMI"
#### Start of system configuration section. ####

srcdir = .
LIBS = 

#### End of system configuration section. ####

PROG=mkisofs
DISTFILES= \
	README \
	README.eltorito \
	configure.in \
	configure \
	config.h.in \
	Makefile.in \
	make.com \
	TODO \
	COPYING \
	ChangeLog \
	mkisofs.8 \
	eltorito.c \
	exclude.c \
	files.c \
	fnmatch.c \
	getopt.c \
	getopt1.c \
	hash.c \
	joliet.c \
	match.c \
	mkisofs.c \
	multi.c \
	name.c \
	rock.c \
	tree.c \
	vms.c \
	write.c \
	mkisofs.spec \
	acconfig.h \
	install-sh \
	defaults.h \
	exclude.h \
	fnmatch.h \
	getopt.h \
	iso9660.h \
	match.h \
	mkisofs.h \
	vms.h \
	README.session \
	diag/Makefile.in \
	diag/README \
	diag/dump.c \
	diag/isodump.c \
	diag/isoinfo.c \
	diag/isovfy.c \
	diag/isoinfo.8 \
	include/fctldefs.h \
	include/mconfig.h \
	include/prototyp.h \
	include/statdefs.h

ROOTSRC=	$(ROOT)/usr/share/src/mkisofs
ROOTSRCDIRS=	$(DISTDIRS:%=$(ROOTSRC)/%)
ROOTDISTFILES=       $(DISTFILES:%=$(ROOTSRC)/%)
$(ROOTDISTFILES) := FILEMODE= 0444
DISTDIRS= diag include

$(ROOTSRC) := GROUP= bin
$(ROOTSRC)/configure := FILEMODE= 0555

MAN1MFILES=	mkisofs.1m

include ../Makefile.cmd

CFLAGS += -I. -Iinclude -DVANILLA_AUTOCONF
LDFLAGS=
OBJS=mkisofs.o tree.o write.o hash.o rock.o multi.o \
		joliet.o match.o name.o fnmatch.o eltorito.o \
		getopt.o getopt1.o

.PARALLEL: $(OBJS)

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTPROG) install_src install_man

install_src: $(ROOTSRC) .WAIT $(ROOTSRCDIRS) .WAIT $(ROOTDISTFILES)

install_man: $(ROOTMAN1MFILES)

$(ROOTSRC) $(ROOTSRCDIRS):
	$(INS.dir)

$(ROOTSRC)/%: %
	$(INS.file)

clean:
	$(RM) $(OBJS)

$(PROG):	$(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LIBS)
	$(POST_PROCESS)

#
# All .h files except for config.h get copied.
#
dist:   Makefile
	(mydir=`basename \`pwd\``;\
	cd .. && tar -cvvf - $$mydir/README $$mydir/README.eltorito \
	$$mydir/configure.in $$mydir/configure $$mydir/config.h.in \
	$$mydir/Makefile.in  $$mydir/make.com $$mydir/TODO \
	$$mydir/COPYING $$mydir/ChangeLog $$mydir/*.8 $$mydir/*.c \
	$$mydir/mkisofs.spec $$mydir/acconfig.h \
	$$mydir/install-sh $$mydir/[d-z]*.h \
	$$mydir/README.session $$mydir/diag/Makefile.in \
	$$mydir/diag/README $$mydir/diag/*.c $$mydir/diag/isoinfo.8 \
	$$mydir/include/*.h | gzip -9 > $${mydir}.tar.gz)


include ../Makefile.targ
