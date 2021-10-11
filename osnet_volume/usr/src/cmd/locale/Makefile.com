#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.2	97/07/28 SMI"
#

PROG= locale

OBJS= locale.o
SRCS= $(OBJS:%.o=../%.c)

include ../../Makefile.cmd

POFILE= locale.po

CLEANFILES += $(OBJS)

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

lint: lint_SRCS

%.o:	../%.c
	$(COMPILE.c) $<

%.po:	../%.c
	$(COMPILE.cpp) $< > `basename $<`.i
	$(XGETTEXT) $(XGETFLAGS) `basename $<`.i
	$(RM)	$@
	sed "/^domain/d" < messages.po > $@
	$(RM) messages.po `basename $<`.i

clean:
	$(RM) $(CLEANFILES)

include ../../Makefile.targ
