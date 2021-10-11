#
# ident	"@(#)Makefile	1.7	94/03/30 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/filter/postscript/postreverse/Makefile
#

include         ../../../Makefile.lp

PROG =           postreverse

SRCS =		postreverse.c

OBJS =		$(SRCS:%.c=%.o)

TXTS=		README 

ENCODING =	2
CPPFLAGS =	-DDFLTENCODING=$(ENCODING)	\
		-I. -I$(COMMONDIR) 		\
		$(CPPFLAGS.master)

.KEEP_STATE :

all :		$(TXTS) $(PROG)

$(PROG) : 	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

install :	all $(ROOTLIBLPPOSTPROG)

clean :
		$(RM) $(OBJS) core *~ *#

strip :
		$(STRIP) $(PROG)

lint :		lint_SRCS

include		../../../../Makefile.targ
