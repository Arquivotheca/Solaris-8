#
#ident "@(#)Makefile 1.9 99/04/06"
#
# ident  Derived from template: @(#)Makefile.program	1.5	92/09/24
#

include ../../Makefile.cmd


PROG=	mkmsgs
SRCS=	mkmsgs.c
OBJS=	mkmsgs.o


.KEEP_STATE:

all: $(PROG)

OWNER=root
GROUP=sys
FILEMODE=0755

install:

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../../Makefile.targ
