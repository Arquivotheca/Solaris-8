#
#ident	"@(#)Makefile	1.20	98/11/10 SMI"	/* SVr4.0 1.	*/
#
# Copyright (c) 1995-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/nfs/umount/Makefile

FSTYPE=		nfs
LIBPROG=	umount

include		../../Makefile.fstype

COMMON=		fslib.o replica.o
OBJS=		$(LIBPROG).o $(COMMON)
SRCS=		$(LIBPROG).c ../../fslib.c ../lib/replica.c

CPPFLAGS += -I../.. -I../lib
CFLAGS += -v

LDLIBS +=	-lrpcsvc -lnsl

#
# Message catalog
#
POFILE= umount.po

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


$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

lint:	lint_SRCS

clean:     
	$(RM) $(OBJS)

fslib.o: ../../fslib.c 
	$(COMPILE.c) ../../fslib.c

replica.o: ../lib/replica.c 
	$(COMPILE.c) ../lib/replica.c
