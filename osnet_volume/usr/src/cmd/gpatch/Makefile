# Generated automatically from Makefile.in by configure.
# Makefile for GNU patch.
# Copyright 1993, 1997-1998, 1999 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.
# If not, write to the Free Software Foundation,
# 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#
#ident	"@(#)Makefile	1.2	99/10/17 SMI"
#### Start of system configuration section. ####

srcdir = .



ed_PROGRAM = /usr/bin/ed

DEFS = -DHAVE_CONFIG_H
LIBOBJS =  getopt1.o getopt.o
PATCH_VERSION = 2.5.4

#### End of system configuration section. ####

SHELL = /bin/sh

LIBSRCS = getopt.c getopt1.c malloc.c memchr.c realloc.c rename.c
SRCS = addext.c argmatch.c backupfile.c basename.c error.c inp.c maketime.c \
	mkdir.c partime.c patch.c pch.c quotearg.c quotesys.c \
	rmdir.c util.c version.c xmalloc.c $(LIBSRCS)
OBJS = addext.o argmatch.o backupfile.o basename.o error.o inp.o \
	maketime.o partime.o patch.o pch.o quotearg.o quotesys.o \
	util.o version.o xmalloc.o $(LIBOBJS)
HDRS = argmatch.h backupfile.h basename.h common.h error.h getopt.h \
	inp.h maketime.h partime.h patchlevel.h pch.h quotearg.h quotesys.h \
	util.h version.h xalloc.h
MISC = AUTHORS COPYING ChangeLog INSTALL Makefile.in NEWS README \
	aclocal.m4 ansi2knr.1 ansi2knr.c \
	config.guess config.hin config.sub configure configure.in \
	install-sh mkinstalldirs patch.man
M4DIR = $(srcdir)/m4
ACINCLUDE_INPUTS = $(M4DIR)/ccstdc.m4 $(M4DIR)/d-ino.m4 \
  $(M4DIR)/inttypes_h.m4 $(M4DIR)/largefile.m4 $(M4DIR)/malloc.m4 \
  $(M4DIR)/protos.m4 $(M4DIR)/realloc.m4 $(M4DIR)/utimbuf.m4

DISTFILES_M4 = $(ACINCLUDE_INPUTS)
DISTFILES_PC = pc/chdirsaf.c
DISTFILES_PC_DJGPP = pc/djgpp/README pc/djgpp/config.sed \
	pc/djgpp/configure.bat pc/djgpp/configure.sed
DISTDIRS= m4 pc pc/djgpp
DISTFILES = $(MISC) $(SRCS) $(HDRS) $(DISTFILES_PC) $(DISTFILES_PC_DJGPP) \
    $(DISTFILES_M4)

PROG=	gpatch
ROOTSRC= $(ROOT)/usr/share/src/gpatch
ROOTSRCDIRS=    $(DISTDIRS:%=$(ROOTSRC)/%)
ROOTDISTFILES=       $(DISTFILES:%=$(ROOTSRC)/%)
$(ROOTDISTFILES) := FILEMODE= 0444
$(ROOTSRC)/configure := FILEMODE= 0555

MAN1FILES=	gpatch.1

include ../Makefile.cmd

CFLAGS += $(DEFS) -errtags=yes -erroff=E_NON_CONST_INIT \
	  -Ded_PROGRAM=\"$(ed_PROGRAM)\" -I. -I$(srcdir) 
FILEMODE=	555

.PARALLEL: $(OBJS)

all:	$(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG) install_src install_man

install_src: $(ROOTSRC) .WAIT $(ROOTSRCDIRS) .WAIT $(ROOTDISTFILES)

install_man: $(ROOTMAN1FILES)

$(ROOTSRC) $(ROOTSRCDIRS):
	$(INS.dir)

$(ROOTSRC)/%: %
	$(INS.file)

patchlevel.h: configure.in
	@echo '#define PATCH_VERSION "$(PATCH_VERSION)"' >patchlevel.h

clean:
	rm -f $(OBJS)

include ../Makefile.targ

$(OBJS): config.h
addext.o: backupfile.h
argmatch.o: argmatch.h error.h quotearg.h
backupfile.o: argmatch.h backupfile.h
error.o: error.h

getopt.o getopt1.o: getopt.h
inp.o: backupfile.h common.h inp.h pch.h quotearg.h util.h xalloc.h
maketime.o: maketime.h partime.h
partime.o: partime.h
patch.o: argmatch.h backupfile.h common.h getopt.h inp.h \
	pch.h quotearg.h util.h version.h xalloc.h
pch.o: backupfile.h basename.h common.h inp.h pch.h quotearg.h util.h
quotearg.o: quotearg.h xalloc.h
quotesys.o: quotesys.h
util.o: backupfile.h basename.h common.h maketime.h \
	partime.h quotearg.h quotesys.h util.h version.h xalloc.h
version.o: common.h patchlevel.h util.h version.h
xmalloc.o: xalloc.h
