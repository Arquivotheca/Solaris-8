# Generated automatically from Makefile.in by configure.
# Makefile for gzip (GNU zip)    -*- Indented-Text -*-
# Copyright (C) 1992-1993 Jean-loup Gailly and the Free Software Foundation

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# $Id: Makefile.in,v 0.23 1993/06/24 11:51:47 jloup Exp $

#
#ident	"@(#)Makefile	1.2	99/10/17 SMI"
#### Start of system configuration section. ####


srcdir = .
VPATH = .

ASCPP = /lib/cpp -DNO_UNDERLINE

# Things you might add to DEFS (configure usually figures out what to do):
# -DGNU_STANDARD        Behave as gzip even if invoked as gunzip (GNU standard)
# -DDIRENT              Use <dirent.h>  for recursion (-r)
# -DSYSDIR              Use <sys/dir.h> for recursion (-r)
# -DSYSNDIR             Use <sys/ndir.h> for recursion (-r)
# -DNDIR                Use <ndir.h> for recursion (-r)
# -DSTDC_HEADERS        Use <stdlib.h>
# -DHAVE_UNISTD_H	Use <unistd.h>
# -DNO_FCNTL_H          Don't use <fcntl.h>
# -DNO_UTIME_H		Don't use <utime.h>
# -DHAVE_SYSUTIME_H	Use <sys/utime.h>
# -DNO_MEMORY_H         Don't use <memory.h>. Not needed if STDC_HEADERS.
# -DNO_STRING_H         Use strings.h, not string.h. Not needed if STDC_HEADERS
# -DRETSIGTYPE=int      Define this if signal handlers must return an int.
# -DNO_SYMLINK          OS defines S_IFLNK but does not support symbolic links
# -DNO_MULTIPLE_DOTS    System does not allow file names with multiple dots
# -DNO_UTIME		System does not support setting file modification time
# -DNO_CHOWN		System does not support setting file owner
# -DNO_DIR		System does not support readdir()
# -DPROTO		Force function prototypes even if __STDC__ not defined
# -DASMV		Use asm version match.S
# -DMSDOS		MSDOS specific
# -DOS2			OS/2 specific
# -DVAXC		Vax/VMS with Vax C compiler
# -DVMS			Vax/VMS with gcc
# -DDEBUG		Debug code
# -DDYN_ALLOC		Use dynamic allocation of large data structures
# -DMAXSEG_64K		Maximum array size is 64K (for 16 bit system)
# -DRECORD_IO           read() and write() are rounded to record sizes.
# -DNO_STDIN_FSTAT      fstat() is not available on stdin
# -DNO_FSTAT            fstat() is not available
# -DNO_SIZE_CHECK       stat() does not give a reliable file size

DEFS =  -DSTDC_HEADERS=1 -DHAVE_UNISTD_H=1 -DDIRENT=1
LIBS = 

# additional assembly sources for particular systems may be required.
OBJA = 

SEDCMD = 1d

#CFLAGS = -O
# If you want debug on by default, use: CFLAGS="-g" ./configure
LDFLAGS = $(CFLAGS)

G=g
# To install znew, zmore, etc... as gznew, gzmore... use: G=g

ZCAT=gzcat
# To install zcat executable and man page as gzcat, use: ZCAT=gzcat

O=.o
# For OS/2 or MSDOS, use: O=.obj

SRCS = gzip.c zip.c deflate.c trees.c bits.c unzip.c inflate.c util.c crypt.c \
       lzw.c unlzw.c unpack.c unlzh.c getopt.c match.S

OBJS = gzip$O zip$O deflate$O trees$O bits$O unzip$O inflate$O util$O \
       lzw$O unlzw$O unpack$O unlzh$O getopt$O $(OBJA)

HDRS = gzip.h lzw.h tailor.h revision.h crypt.h getopt.h

GENFILES =  README NEWS INSTALL Makefile.in configure.in configure COPYING \
  TODO THANKS ChangeLog $(SRCS) $(HDRS) zmore.in znew.in zdiff.in zgrep.in \
  zforce.in gzexe.in gzip.1 zdiff.1 zgrep.1 zmore.1 znew.1 gzexe.1 zforce.1 \
  gzip.doc algorithm.doc gzip.texi texinfo.tex gpl.texinfo gzip.info

sampleFILES =  sample/makecrc.c sample/zread.c sample/add.c sample/sub.c \
       sample/ztouch sample/zfile

msdosFILES = msdos/tailor.c msdos/match.asm msdos/gzip.prj msdos/doturboc.bat \
  msdos/Makefile.msc msdos/Makefile.bor msdos/Makefile.djg

os2FILES = os2/Makefile.os2 os2/gzip.def os2/gzip16.def

ntFILES = nt/Makefile.nt

vmsFILES = vms/Readme.vms vms/Makefile.vms vms/Makefile.gcc vms/makegzip.com \
   vms/Makefile.mms vms/vms.c vms/gzip.hlp

amigaFILES = amiga/Makefile.sasc amiga/Makefile.gcc amiga/tailor.c \
  amiga/utime.h amiga/match.a

atariFILES = atari/Makefile.st

primosFILES = primos/readme primos/primos.c primos/ci.opts \
   primos/build.cpl primos/include/errno.h primos/include/fcntl.h \
   primos/include/stdlib.h primos/include/sysStat.h primos/include/sysTypes.h

DISTFILES = $(GENFILES) $(sampleFILES) $(msdosFILES) $(os2FILES) $(ntFILES)\
   $(vmsFILES) $(amigaFILES) $(atariFILES) $(primosFILES)
DISTDIRS= sample msdos os2 nt vms amiga atari primos primos/include

PROG=gzip
ROOTLINKS= $(ROOTBIN)/gunzip $(ROOTBIN)/$(ZCAT)
SCRIPTS = $(G)zdiff $(G)zgrep $(G)zmore $(G)znew $(G)zforce gzexe $(G)zcmp
ROOTSCRIPTS=  $(SCRIPTS:%=$(ROOTBIN)/%)
ROOTSRC= $(ROOT)/usr/share/src/gzip
ROOTSRCDIRS=	$(DISTDIRS:%=$(ROOTSRC)/%)
ROOTDISTFILES=       $(DISTFILES:%=$(ROOTSRC)/%)
$(ROOTDISTFILES) := FILEMODE= 0444
$(ROOTSRC)/configure := FILEMODE= 0555
MAN1FILES=	gzdiff.1 gzcmp.1 gzexe.1 gzforce.1 gzgrep.1 gzmore.1 \
		gznew.1 gzip.1 gunzip.1 gzcat.1

include ../Makefile.cmd

CFLAGS += $(DEFS) -errtags=yes -erroff=E_NON_CONST_INIT 
FILEMODE=	555
CLOBBERFILES += $(SCRIPTS)
BINDIR=		/usr/bin

.PARALLEL: $(OBJS)

.NO_PARALLEL: $(ROOTSRCDIRS)

.KEEP_STATE:

all:	gzip $(SCRIPTS)

install: all $(ROOTPROG) $(ROOTLINKS) $(ROOTSCRIPTS) install_src install_man

install_src: $(ROOTSRC) .WAIT $(ROOTSRCDIRS) .WAIT $(ROOTDISTFILES)

install_man: $(ROOTMAN1FILES)

$(ROOTSRC) $(ROOTSRCDIRS):
	$(INS.dir)

$(ROOTSRC)/%: %
	$(INS.file)

# Actual build-related targets

$(PROG):	$(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LIBS)
	$(POST_PROCESS)
	rm -f gunzip $(ZCAT)
	ln $(PROG) gunzip
	ln $(PROG) $(ZCAT)

$(ROOTLINKS):	$(ROOTPROG)
	$(RM) $@
	$(LN) $(ROOTPROG) $@

gzip$O zip$O deflate$O trees$O bits$O unzip$O inflate$O: gzip.h tailor.h
util$O lzw$O unlzw$O unpack$O unlzh$O crypt$O: gzip.h tailor.h

gzip$O unlzw$O: revision.h lzw.h

bits$O unzip$O util$O zip$O: crypt.h

gzip$O getopt$O: getopt.h

match$O: match.S
	$(ASCPP) $(srcdir)/match.S > _match.s
	$(CC) -c _match.s
	mv _match$O match$O
	rm -f _match.s

$(G)zdiff: zdiff.in
	sed -e "$(SEDCMD)" -e "s|BINDIR|$(BINDIR)|" $(srcdir)/zdiff.in > $@
	chmod 755 $@

$(G)zgrep: zgrep.in
	sed -e "$(SEDCMD)" -e "s|BINDIR|$(BINDIR)|" $(srcdir)/zgrep.in > $@
	chmod 755 $@

$(G)zmore: zmore.in
	sed -e "$(SEDCMD)" -e "s|BINDIR|$(BINDIR)|" $(srcdir)/zmore.in > $@
	chmod 755 $@

$(G)znew: znew.in
	sed -e "$(SEDCMD)" -e "s|BINDIR|$(BINDIR)|" $(srcdir)/znew.in > $@
	chmod 755 $@

$(G)zforce: zforce.in
	sed -e "$(SEDCMD)" -e "s|BINDIR|$(BINDIR)|" $(srcdir)/zforce.in > $@
	chmod 755 $@

gzexe: gzexe.in
	sed -e "$(SEDCMD)" -e "s|BINDIR|$(BINDIR)|" $(srcdir)/gzexe.in > $@
	chmod 755 $@

$(G)zcmp: zdiff.in
	sed -e "$(SEDCMD)" -e "s|BINDIR|$(BINDIR)|" $(srcdir)/zdiff.in > $@
	chmod 755 $@

# end of file

clean:
	$(RM) $(OBJS)

include ../Makefile.targ
