#
#ident	"@(#)Makefile	1.7	98/10/16 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#
# cmd/sgs/size/sparc/Makefile
#

PROG=		size

include 	../../../Makefile.cmd
include 	../../Makefile.com

OBJS=		main.o process.o fcns.o

SRCS=		$(OBJS:%.o=../common/%.c)

LDFLAGS +=	'-R$$ORIGIN/../../lib'
INCLIST=	-I../common -I../../include -I../../include/sparc \
		-I$(SRCBASE)/uts/$(ARCH)/sys
DEFLIST=	-DSPARC -DELF
CPPFLAGS=	$(INCLIST) $(DEFLIST) $(CPPFLAGS.master)
LDLIBS +=	-L../../libconv/sparc -lconv -lelf
LINTFLAGS=	-mx $(LDLIBS)

CLEANFILES +=	$(LINTOUT)


# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	LDLIBS += /usr/lib/libintl.so.1


%.o:		../common/%.c
		$(COMPILE.c) $<

.KEEP_STATE:

all:		$(PROG)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

package \
install:	all $(ROOTCCSBINPROG)

clean:
		$(RM) $(OBJS) $(CLEANFILES)

lint:		$(LINTOUT)

$(LINTOUT):	$(SRCS)
		$(LINT.c) $(SRCS) > $(LINTOUT) 2>&1

include		../../../Makefile.targ
