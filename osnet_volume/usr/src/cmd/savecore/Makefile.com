#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.1	98/06/03 SMI"
#

PROG= savecore
SRCS= ../savecore.c ../../../uts/common/os/compress.c

include ../../Makefile.cmd

LDLIBS += -lcmd
CFLAGS += -v
CFLAGS64 += -v
CPPFLAGS += -D_LARGEFILE64_SOURCE=1

.KEEP_STATE:

all: $(PROG)

$(PROG): $(SRCS)
	$(LINK.c) -o $(PROG) $(SRCS) $(LDLIBS)
	$(POST_PROCESS)

clean:
	$(RM) $(PROG)

lint:	lint_SRCS

include ../../Makefile.targ
