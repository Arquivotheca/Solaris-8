#
#ident	"@(#)Makefile	1.10	94/02/01 SMI"	/* SVr4.0 1.12	*/
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		nfs
LIBPROG=	dfshares
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

OBJS= 		$(LIBPROG).o
SRCS=		$(OBJS:%.o=%.c)

LDLIBS +=	-lrpcsvc -lnsl

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

clean:
	$(RM) $(OBJS)
