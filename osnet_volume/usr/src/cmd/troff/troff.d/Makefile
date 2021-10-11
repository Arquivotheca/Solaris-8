#
# ident	"@(#)Makefile	1.13	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/troff/troff.d/Makefile
#

include 	../../Makefile.cmd

PROG =		troff ta makedev

SUBDIRS =	tmac.d devaps

TAOBJS =	draw.o ta.o
MAKEDEVOBJS =	makedev.o
TROFFOBJS =	t10.o t6.o
COMMONOBJS =	hytab.o n1.o n2.o n3.o n4.o n5.o		\
		n7.o n8.o n9.o ni.o nii.o suftab.o

SRCS =		$(TAOBJS:%.o=%.c) $(MAKEDEVOBJS:%.o=%.c)	\
		$(TROFFOBJS:%.o=%.c) $(COMMONOBJS:%.o=../%.c)

TXTS =		README maketables troff.sh

CPPFLAGS =	-DUSG -DINCORE -I. -I.. $(CPPFLAGS.master)
#
# For message catalog
#
POFILES= $(TROFFOBJS:%.o=%.po) $(COMMONOBJS:%.o=../%.po)
POFILE= troff.d.po

# conditional assignments

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
strip :=	TARGET= strip

troff:=		POBJS=	$(COMMONOBJS) $(TROFFOBJS)
ta:=		POBJS=	$(TAOBJS)
makedev:=	POBJS=	$(MAKEDEVOBJS)

ta:=		LDLIBS += -lm
troff:=		LDLIBS += -lmapmalloc

# build rule for common source above
%.o: ../%.c
		$(COMPILE.c) $<

.KEEP_STATE:

.PARALLEL: $(COMMONOBJS) $(TROFFOBJS) $(TAOBJS) $(MAKEDEVOBJS)

all :		$(PROG) $(TXTS) $(SUBDIRS)

$(PROG) :	$$(POBJS)
		$(LINK.c) -o $@ $(POBJS) $(LDLIBS)
		$(POST_PROCESS)

install:	$(PROG) $(ROOTPROG) $(SUBDIRS)

clean:		$(SUBDIRS)
		$(RM) $(TAOBJS) $(MAKEDEVOBJS) $(TROFFOBJS) $(COMMONOBJS)

catalog:        $(POFILE)

$(POFILE):      $(POFILES)
	$(RM)	$@
	cat     $(POFILES)      > $@


strip :
		$(STRIP $(PROG)
	
lint :		lint_SRCS

include		../../Makefile.targ

# additional dependency for clobber which is defined in Makefile.targ
clobber:	$(SUBDIRS)

$(SUBDIRS) :	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
