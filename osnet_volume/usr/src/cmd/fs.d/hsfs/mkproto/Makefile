#
#ident	"@(#)Makefile	1.6	94/05/17 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/fs.d/hsfs/mkproto/Makefile

FSTYPE=		hsfs

all:		mkproto

include		../../Makefile.fstype

OBJS=		main.o dlist.o dlist_path.o dlist_proto.o \
		iso.o util.o
SRCS=		$(OBJS:%.o=%.c)

mkproto:	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

lint:	lint_SRCS

clean:     
	-$(RM) $(OBJS)
