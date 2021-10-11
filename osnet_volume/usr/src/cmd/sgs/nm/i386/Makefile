#
#ident	"@(#)Makefile	1.11	98/10/16 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/sgs/nm/i386/Makefile
#

PROG=		nm
XPG4PROG=	nm

ARFORMAT=	PORTAR

include 	../../../Makefile.cmd
include 	../../Makefile.com

COMOBJS=	nm.o

OBJS=		$(COMOBJS)
XPG4OBJS=	objs.xpg4/nm.o

SRCS=		$(COMOBJS:%.o=../common/%.c)

LLDFLAGS =	'-R$$ORIGIN/../../lib'
INCLIST=	-I../../include -I../../include/i386 \
		-I$(SRCBASE)/uts/$(ARCH)/sys
DEFLIST=	-DTARGET=I386 -DI386=1 -D$(ARFORMAT) -DELF
CPPFLAGS=	$(INCLIST) $(DEFLIST) $(CPPFLAGS.master)
LDLIBS +=	-L ../../sgsdemangler/i386 -ldemangle \
		-L ../../libconv/i386 -lconv -lelf -ldl
LINTFLAGS +=	$(LDLIBS)
CLEANFILES +=	$(LINTOUT)
objs.xpg4/%.o := CPPFLAGS += -DXPG4

$(DEMOBJS):=	DEFLIST = -DELF_OBJ

# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	LDLIBS += /usr/lib/libintl.so.1


objs.xpg4/%.o:	../common/%.c
		$(COMPILE.c) -o $@ $<

%.o:		../common/%.c
		$(COMPILE.c) $<

.KEEP_STATE:

all:		$(PROG) $(XPG4)

$(PROG):	$(OBJS) ../../sgsdemangler/i386/libdemangle.a
		$(LINK.c) $(OBJS) -o $@ $(LLDFLAGS) $(LDLIBS)
		$(POST_PROCESS)

$(XPG4):	$(XPG4OBJS) ../../sgsdemangler/i386/libdemangle.a
		$(LINK.c) $(XPG4OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

$(XPG4OBJS):	objs.xpg4

objs.xpg4:
	-@mkdir -p $@

package \
install:	all $(ROOTCCSBINPROG) $(ROOTXPG4PROG)

clean:
		$(RM) $(OBJS) $(XPG4OBJS) $(CLEANFILES)

lint:		$(LINTOUT)

$(LINTOUT):	$(SRCS)
		$(LINT.c) $(SRCS) > $(LINTOUT) 2>&1

include		../../../Makefile.targ
