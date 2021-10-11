#
#ident	"@(#)Makefile	1.9	96/05/23 SMI"
#
# Copyright (c) 1994-1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/pcmciad/Makefile
#

include ../Makefile.cmd

PROG= 	pcmciad

# srcs
SRCS = $(PROG:%=%.c)
DEVS=	

SOFILES= ${DEVS}

# objects
OBJECTS = $(SRCS:%.c=%.o)

LDLIBS += -lthread -lvolmgt -ldl
CFLAGS += -v
$(SOFILES) :=	LDLIBS += -lintl -lc
# to get the correct DSO flags used for compilation/linking
$(SOFILES) :=	CFLAGS += -K pic
$(SOFILES) :=	LDFLAGS += -ztext

ROOTLIBPROG=	$(PROG:%=$(ROOTLIB)/%)

OWNER= root
GROUP= bin
FILEMODE= 555

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

.KEEP_STATE:

all: $(PROG) $(DEVLINK) $(SOFILES)

$(PROG): $(COMMONOBJ) $(LIBS)

# build rule
%: %.c $(COMMONOBJ)
	$(LINK.c) -o $@ $< $(COMMONOBJ) $(LIBS) $(LDLIBS)
	$(POST_PROCESS)

.SUFFIXES: .so.1 ${SUFFIXES}

.c.so.1:
	${CC} ${CPPFLAGS} ${CFLAGS} -G -o $@ $< $(LDLIBS) $(LDFLAGS)
	$(POST_PROCESS_SO)

install: all $(ROOTLIBPROG) $(ROOTDEVLINK)

$(ROOTLIB)/%:	%
		$(INS.file)

clean:	$(LIBSUBDIRS)
	$(RM) $(OBJECTS) $(DEVLINK)

lint:	lint_SRCS 

FRC:

include ../Makefile.targ

# additional dependency for clobber which is defined in ../Makefile.targ
clobber:
