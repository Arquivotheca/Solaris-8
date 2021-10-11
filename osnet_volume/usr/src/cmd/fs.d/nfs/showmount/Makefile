#
#ident	"@(#)Makefile	1.15	96/03/18 SMI"	/* */
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/fs.d/nfs/showmount/Makefile

FSTYPE=		nfs
LIBPROG=	showmount
ATTMK=		$(LIBPROG)

OTHERINSTALL=	$(ROOTUSRSBIN)/$(LIBPROG)
LINKVALUE=	../lib/fs/$(FSTYPE)/$(LIBPROG)

include		../../Makefile.fstype

OBJS= 		$(LIBPROG).o
SRCS=		$(OBJS:%.o=%.c)

#
# Message catalog
#
POFILE= showmount.po

LDLIBS +=	-lrpcsvc -lnsl

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

lint:	lint_SRCS

$(ROOTUSRSBIN)/$(LIBPROG):
		$(RM) $@; $(SYMLINK) $(LINKVALUE) $@

#
# message catalog
#

catalog:        $(POFILE)

$(POFILE):      $(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS)   > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	sed "/^domain/d"        messages.po     > $@
	$(RM) messages.po $(POFILE).i

clean:     
	$(RM) $(OBJS)
