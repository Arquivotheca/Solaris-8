#
# ident	"@(#)Makefile	1.14	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/troff/nroff.d/Makefile
#

include 	../../Makefile.cmd

SUBDIRS =	terms.d

PROG =		nroff

OBJS =		n10.o n6.o

COMMONOBJS =	hytab.o n1.o n2.o n3.o n4.o n5.o	\
		n7.o n8.o n9.o ni.o nii.o suftab.o

SRCS =		$(OBJS:%.o=%.c) $(COMMONOBJS:%.o=../%.c)


CPPFLAGS =	-DNROFF -DUSG -DINCORE -DEUC -I. -I.. $(CPPFLAGS.master)

LDLIBS +=	-lmapmalloc
#
# message catalog
#
POFILES= $(OBJS:%.o=%.po)
POFILE= nroff.d.po


all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
strip :=	TARGET= strip

# build rule for common source above
%.o: ../%.c
		$(COMPILE.c) $<

.KEEP_STATE:

.PARALLEL: $(COMMONOBJS) $(OBJS)

all :		$(PROG) $(SUBDIRS)

$(PROG) :	$(OBJS) $(COMMONOBJS)
		$(LINK.c) -o $@ $(OBJS) $(COMMONOBJS) $(LDLIBS)
		$(POST_PROCESS)

install :	$(PROG) $(ROOTPROG) $(SUBDIRS)

catalog:        $(POFILE)
 
$(POFILE):      $(POFILES)
	 $(RM)	$@
	 cat $(POFILES)  >       $@


clean :		$(SUBDIRS)
		$(RM) $(OBJS) $(COMMONOBJS)

strip :
		$(STRIP) $(PROG)

lint :		lint_SRCS

include		../../Makefile.targ

# additional dependency for clobber which is defined in Makefile.targ
clobber :	$(SUBDIRS)

$(SUBDIRS) :	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
