#
#ident	"@(#)Makefile	1.9	99/02/13 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		udfs
LIBPROG=	fsck
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

FSCKOBJS=	main.o setup.o utilities.o pass1.o inode.o
FSCKSRCS=	$(FSCKOBJS:%.o=%.c)

UDFSDIR=	../mkfs
UDFSOBJS=	udfslib.o
#UDFSSRCS=	$(UDFSOBJS:%.o=%.c)

OBJS=		$(FSCKOBJS) $(UDFSOBJS)
SRCS=		$(FSCKSRCS) $(UDFSSRCS) ../mkfs/udfslib.c

#CPPFLAGS +=	-D_LARGEFILE64_SOURCE
CPPFLAGS +=	-I../mkfs
LDLIBS += -lc2 -ladm
$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

%.o:	$(UDFSDIR)/%.c
	$(COMPILE.c) $<

# for messaging catalog
#
POFILE= fsck.po

# for messaging catalog
#
catalog:        $(POFILE)

$(POFILE):      $(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS)   > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	sed "/^domain/d" messages.po >  $@
	$(RM) $(POFILE).i messages.po

lint:
	$(LINT.c) -mu $(SRCS)

clean:     
	$(RM) $(FSCKOBJS) $(UDFSOBJS)

putback:
	-cstyle -p inode.c
	-cstyle -p main.c
	-cstyle -p pass1.c
	-cstyle -p setup.c
	-cstyle -p utilities.c
	-keywords inode.c
	-keywords main.c
	-keywords pass1.c
	-keywords setup.c
	-keywords utilities.c
	-keywords Makefile
