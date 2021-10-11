#
#ident	"@(#)Makefile.com	1.7	98/03/23 SMI"
#
# Copyright (c) 1995-1997 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG=		rdb

# DEMO DELETE START
include 	../../../../Makefile.cmd
# DEMO DELETE END

MACH:sh=	uname -p

CFLAGS +=	$(DEMOCFLAGS)

COMSRC= bpt.c dis.c main.c ps.c gram.c lex.c globals.c help.c \
	utils.c maps.c syms.c callstack.c disasm.c
M_SRC=	regs.c m_utils.c

BLTSRC= gram.c lex.c
BLTHDR= gram.h

# DEMO DELETE START
ONLDLIBDIR=	/opt/SUNWonld/lib

# DEMO DELETE END
OBJDIR=		objs
OBJS =		$(COMSRC:%.c=$(OBJDIR)/%.o) $(M_SRC:%.c=$(OBJDIR)/%.o) \
		$(BLTSRC:%.c=$(OBJDIR)/%.o)

SRCS =		$(COMSRC:%=../common/%) $(M_SRC) $(BLTSRC)

.PARALLEL:	$(OBJS)

CPPFLAGS=	-I../common -I. $(CPPFLAGS.master)
LDLIBS +=	-lrtld_db -lelf -ll -ly

CLEANFILES +=	$(BLTSRC) $(BLTHDR) simp libsub.so.1

# DEMO DELETE START
LINTFLAGS +=	$(LDLIBS) -L../../$(MACH)
CLEANFILES +=	$(LINTOUT)
# DEMO DELETE END

test-sparc=	test-sparc-regs
test-i386=	
TESTS= test-maps test-breaks test-steps test-plt_skip test-object-padding \
	$(test-$(MACH))

# DEMO DELETE START
ROOTONLDBIN=		$(ROOT)/opt/SUNWonld/bin
ROOTONLDBINPROG=	$(PROG:%=$(ROOTONLDBIN)/%)
ROOTONLDBINPROG64=	$(PROG:%=$(ROOTONLDBIN)/sparcv9/%)

FILEMODE=	0755
# DEMO DELETE END
