#
#ident	"@(#)Makefile	1.8	98/04/19 SMI"
#
# Copyright (c) 1996,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/ufs/roll_log

include		../../Makefile.fstype

INCLUDES=	roll_log.h
SRCS=		roll_log.c
OBJS=		$(SRCS:%.c=$(MACH)/%.o)

CLOBBERFILES=	$(OBJS)

.KEEP_STATE:

all install:	$(MACH) .WAIT $(OBJS)

$(OBJS):	$(INCLUDES)

$(MACH):
	$(MKDIR) -p $@

$(MACH)/%.o:	%.c
	$(COMPILE.c) -o $@ $<

lint:	lint_SRCS

clean clobber:
