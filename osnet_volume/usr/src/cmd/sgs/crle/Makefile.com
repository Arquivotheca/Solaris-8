#
#ident	"@(#)Makefile.com	1.1	99/08/13 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.

PROG=		crle

include		$(SRC)/cmd/Makefile.cmd
include		$(SRC)/cmd/sgs/Makefile.com

COMOBJ=		config.o	crle.o 		depend.o	dump.o \
		inspect.o	hash.o		print.o		util.o
BLTOBJ=		msg.o

OBJS=		$(BLTOBJ) $(COMOBJ)

MAPFILE=	../common/mapfile-vers

CPPFLAGS +=	-I. -I../../include -I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master)
LDFLAGS +=	-Yl,$(SGSPROTO) -M $(MAPFILE) '-R$$ORIGIN/../lib' -zlazyload
LDLIBS +=	-lelf -ldl $(INTLLIB) -L ../../libconv/$(MACH) -lconv
LINTFLAGS=	-mx

BLTDEFS=        msg.h
BLTDATA=        msg.c
BLTMESG=        $(SGSMSGDIR)/crle

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGCOM=	../common/crle.msg
SGSMSGTARG=	$(SGSMSGCOM)
SGSMSGALL=	$(SGSMSGCOM)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n crle_msg

SRCS=		$(COMOBJ:%.o=../common/%.c) $(BLTDATA)
LINTSRCS=	$(SRCS) ../common/lintsup.c

CLEANFILES +=	$(SGSLINTOUT) $(BLTFILES)


# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	INTLLIB += /usr/lib/libintl.so.1
