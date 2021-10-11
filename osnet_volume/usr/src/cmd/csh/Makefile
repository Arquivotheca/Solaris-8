#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)Makefile	1.28	99/07/16 SMI"
#
# cmd/csh/Makefile
#
#		PROPRIETARY NOTICE (Combined)
#
#This source code is unpublished proprietary information
#constituting, or derived under license from AT&T's UNIX(r) System V.
#In addition, portions of such source code were derived from Berkeley
#4.3 BSD under license from the Regents of the University of
#California.
#
#
#
#		Copyright Notice 
#
#Notice of copyright on this source code product does not indicate 
#publication.
#
#	Copyright (c) 1986-1996, by Sun Microsystems, Inc
#	All rights reserved.
#
#	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
#	          All rights reserved.

#
# Copyright (c) 1980 Regents of the University of California.
# All rights reserved.  The Berkeley Software License Agreement
# specifies the terms and conditions for redistribution.
#
#
# C Shell with process control; VM/UNIX VAX Makefile
# Bill Joy UC Berkeley; Jim Kulp IIASA, Austria
#

include ../Makefile.cmd

ED= ed
#
# For message catalogue files
#
GREP= egrep
SED= sed
POFILE= csh.po
CLOBBERFILES += sh.tconst.h

all :=          TARGET = all
install :=      TARGET = install
clean :=        TARGET = clean
clobber :=      TARGET = clobber
lint :=         TARGET = lint

PFOBJS = sh_policy.o

POBJS = alloc.o bcopy.po bzero.o gethostname.o getpagesize.o\
getrusage.o getwd.o index.o killpg.o printf.o\
rindex.o setpgrp.o sh.o sh.char.o sh.debug.o\
sh.dir.o sh.dol.o sh.err.o sh.exec.o sh.exp.o\
sh.file.o sh.func.o sh.glob.o sh.hist.o\
sh.init.o sh.lex.o sh.misc.o sh.parse.o\
sh.print.o sh.proc.o sh.sem.o sh.set.o\
sh.tchar.o sh.time.o stubs.o wait3.o

CPPFLAGS +=	-D_FILE_OFFSET_BITS=64
CPPFLAGS +=	-I ../sh

.KEEP_STATE:

all install clean clobber lint:	$(MACH)

PFPOFILES= $(PFOBJS:%.o=%.po)

POFILES= alloc.po bcopy.po bzero.po gethostname.po getpagesize.po\
getrusage.po getwd.po index.po killpg.po printf.po\
rindex.po setpgrp.po sh.po sh.char.po sh.debug.po\
sh.dir.po sh.dol.po sh.err.po sh.exec.po sh.exp.po\
sh.file.po sh.func.po sh.glob.po sh.hist.po\
sh.init.po sh.lex.po sh.misc.po sh.parse.po\
sh.print.po sh.proc.po sh.sem.po sh.set.po\
sh.tchar.po sh.time.po stubs.po wait3.po\
$(PFPOFILES)

XGETFLAGS= -a -x csh.xcl


#
# For message catalogue files
#
_msg: $(MSGDOMAIN) sh.tconst.h .WAIT $(POFILE)
	$(RM) $(POFILE)
	cat $(POFILES) > $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)


$(POFILE):	$(POFILES)
	$(RM) $@; cat $(POFILES) > $@

# based on .c.po, BUILD.po from Makefile.master
%.po:	../sh/%.c
	$(COMPILE.cpp) $<  > $(<F).i
	$(XGETTEXT) $(XGETFLAGS) $(<F).i ;\
	$(RM)   $@ ;\
	sed "/^domain/d" < messages.po  > $@ ;\
	$(RM) messages.po $(<F).i

$(PROG):	$$(POBJS)
		$(LINK.c) $(POBJS) -o $@

sh.tconst.h:
	$(RM) $@; $(ED) sh.tconst.c < make.sh.tconst.h.ed

clobber: local_clobber $(MACH)

local_clobber:
	$(RM) $(CLOBBERFILES)

$(MACH): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
