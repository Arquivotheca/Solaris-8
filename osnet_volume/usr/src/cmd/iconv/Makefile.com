#
#ident	"@(#)Makefile.com	1.5	97/08/02 SMI"        
#
# Copyright (c) 1990, 1994, 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/iconv/Makefile.com

PROG=iconv
KPROG=kbdcomp 


IOBJS= iconv.o gettab.o process.o
KOBJS= main.o gram.o lexan.o output.o reach.o sort.o sym.o tree.o
ISRCS= $(IOBJS:%.o=../%.c)
KSRCS= $(KOBJS:%.o=../%.c)
ROOTKPROG= $(KPROG:%=$(ROOTBIN)/%)
ROOTKPROG32= $(KPROG:%=$(ROOTBIN32)/%)
ROOTKPROG64= $(KPROG:%=$(ROOTBIN64)/%)

CLOBBERFILES= $(KPROG)
CLEANFILES= $(IOBJS) $(KOBJS) gram.c y.tab.h $(GENCODESETS) \
	$(POFILES) $(POFILE)


include ../../Makefile.cmd

#
# Message catalog
#
POFILES= $(IOBJS:%.o=%.po)
POFILE= iconv_.po

YFLAGS += -d
CPPFLAGS= -I. $(CPPFLAGS.master)

ROOTDIRS32=	$(ROOTLIB)/iconv
ROOTDIRS64=	$(ROOTLIB)/iconv/$(MACH64)

GENCODESETS=\
646da.8859.t 646de.8859.t 646en.8859.t \
646es.8859.t 646fr.8859.t 646it.8859.t \
646sv.8859.t 8859.646.t   8859.646da.t \
8859.646de.t 8859.646en.t 8859.646es.t \
8859.646fr.t 8859.646it.t 8859.646sv.t

SRCCODESETS=	$(GENCODESETS:%.t=../%.p)

CODESETS= $(GENCODESETS) iconv_data

ROOTCODESETS32=	$(CODESETS:%=$(ROOTDIRS32)/%)
ROOTCODESETS64=	$(CODESETS:%=$(ROOTDIRS64)/%)

# conditional assignments
#
$(ROOTCODESETS32) := FILEMODE = 444
$(ROOTCODESETS64) := FILEMODE = 444

.SUFFIXES: .p .t

# build rule
#

%.t:	../%.p
	./$(KPROG) -o $@ $<

# install rule
# 

$(ROOTDIRS64)/%: ../%
	$(INS.file)

$(ROOTDIRS64)/%: %
	$(INS.file)

$(ROOTDIRS32)/%: ../%
	$(INS.file)

$(ROOTDIRS32)/%: %
	$(INS.file)


.KEEP_STATE:

.PARALLEL: $(IOBJS) $(KOBJS)

$(CODESETS): $(KPROG) $(SRCCODESETS)

$(PROG): $(IOBJS)
	$(LINK.c) $(IOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(KPROG): $(KOBJS)
	$(LINK.c) $(KOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)
#
# Message catalog
#
$(POFILE):  $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

gram.c + y.tab.h : ../gram.y 
	$(RM) gram.c y.tab.h
	$(YACC.y) ../gram.y
	mv y.tab.c gram.c

lexan.o:	y.tab.h

$(ROOTDIRS32):
	$(INS.dir)

$(ROOTDIRS64):	$(ROOTDIRS32)
	$(INS.dir)

$(ROOTCODESETS64):	$(ROOTDIRS64)

$(ROOTCODESETS32):	$(ROOTDIRS32)

lint: lint_SRCS

%.o:	%.c
	$(COMPILE.c) $<

%.o:	../%.c
	$(COMPILE.c) $<

%.po:	../%.c
	$(COMPILE.cpp) $< > `basename $<`.i
	$(XGETTEXT) $(XGETFLAGS) `basename $<`.i
	$(RM)	$@
	sed "/^domain/d" < messages.po > $@
	$(RM) messages.po `basename $<`.i

clean:
	$(RM) $(CLEANFILES)

include ../../Makefile.targ
