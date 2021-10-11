#
#ident	"@(#)Makefile	1.14	96/03/18 SMI"	/* SVr4.0 1.12	*/
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		nfs
LIBPROG=	dfmounts
ATTMK=		$(LIBPROG)

include ../../Makefile.fstype

OBJS= 	$(LIBPROG).o
SRCS=	$(OBJS:%.o=%.c)

#
# Message catalog
#
POFILES= $(OBJS:%.o=%.po)
POFILE= dfmounts.po

LDLIBS +=	-lrpcsvc -lnsl

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

#
# message catalog
#
catalog: $(POFILE)
 
$(POFILE): $(SRCS)
	 $(RM) $@
	 $(COMPILE.cpp)   $(SRCS) > $(POFILE).i
	 $(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	 sed "/^domain/d" messages.po > $@
	 $(RM) messages.po $(POFILE).i

lint:	lint_SRCS

clean:     
	$(RM) $(OBJS)
