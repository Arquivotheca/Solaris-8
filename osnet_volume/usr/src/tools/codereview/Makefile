#pragma	ident	"@(#)Makefile	1.2	99/06/02 SMI"

PROG= codereview

OBJS= 	lwlp.o
SRCS=	$(OBJS:%.o=%.c)
CLEANFILES += $(OBJS)
TMPDIR= /tmp

include ../Makefile.tools

MAN1FILES= codereview.1

$(ROOTONBLDMAN1FILES) := FILEMODE=      644

COPTS=	-O

.KEEP_STATE:

.PARALLEL: $(OBJS)

all:	$(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

install: all .WAIT  $(ROOTONBLDMACHPROG) $(ROOTONBLDMAN1FILES)

clean:
	$(RM) $(CLEANFILES)

lint: lint_SRCS

include ../Makefile.targ
