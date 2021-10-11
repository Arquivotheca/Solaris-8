#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.3	99/09/29 SMI"
#

include		../../../../lib/Makefile.lib

SGSSRC=		../../../sgs
SGSPROTO=	../../proto/$(MACH)

APPTRACELIBNAME= apptrace
APPTRACELIB=	$(APPTRACELIBNAME).so.1
APPTRACESRC=	apptrace.c interceptlib.c private.c

INTCPTLIBNAME=	interceptors
INTCPTLIB=	$(INTCPTLIBNAME).so.1
INTCPTSRC=	interceptors.c
INTCPTMAP=	../common/mapfile-interceptors
INTCPTMFLAG=	$(INTCPTMAP:%=-M %)

APPTRACEPROG=	apptrace
APPTRACEPROGSRC= apptracecmd.c

#
# Used for linting
#
APPTRACEPROG_LINTSRC=	$(APPTRACEPROGSRC:%=../common/%)
APPTRACE_LINTSRC=	$(APPTRACESRC:%=../common/%)
INTCPT_LINTSRC=		$(INTCPTSRC:%=../common/%)
APPTRACE_LINTLIB=	llib-l$(APPTRACELIBNAME).ln
INTCPT_LINTLIB=		llib-l$(INTCPTLIBNAME).ln

ABILIBS=	$(APPTRACELIB) $(INTCPTLIB)

PICDIR=		pics
OBJDIR=		objs

APPTRACEPICS=	$(APPTRACESRC:%.c=$(PICDIR)/%.o) $(PICDIR)/abienv.o
APPTRACEPROGOBJ=$(APPTRACEPROGSRC:%.c=$(OBJDIR)/%.o)
INTCPTPICS=	$(INTCPTSRC:%.c=$(PICDIR)/%.o)

$(APPTRACELIB):=	PICS=$(APPTRACEPICS)
$(INTCPTLIB):=		PICS=$(INTCPTPICS)

sparc_C_PICFLAGS =   -K pic
sparcv9_C_PICFLAGS = -K pic
i386_C_PICFLAGS =    -K pic

#
# Support object for the link editor to cull the stabs sections.
#
sparc_LDCULLSTABS=		-Wl,-S,/usr/lib/abi/ldcullstabs.so.1
i386_LDCULLSTABS=		-Wl,-S,/usr/lib/abi/ldcullstabs.so.1
sparcv9_LDCULLSTABS=		-Wl,-S,/usr/lib/abi/$(MACH64)/ldcullstabs.so.1
ia64_LDCULLSTABS=		-Wl,-S,/usr/lib/abi/$(MACH64)/ldcullstabs.so.1
ABICULLING=			$($(MACH)_LDCULLSTABS)

$(ABILIBS):=	sparc_CFLAGS += -xregs=no%appl $(sparc_C_PICFLAGS)
$(ABILIBS):=	sparcv9_CFLAGS += -xregs=no%appl $(sparcv9_C_PICFLAGS)
$(ABILIBS):=	i386_CFLAGS += $(i386_C_PICFLAGS)

$(APPTRACELIB):=	LDLIBS += -L$(ROOTLIBABI) -R$(REALROOTLIBABI) -lstabspf -lmapmalloc -ldl -lc
$(APPTRACELIB):=	SONAME=	$(APPTRACELIB)

$(APPTRACEPROG):=	LDFLAGS += $(STRIPFLAG) $(LDLIBS.cmd)

$(INTCPTLIB):=		LDLIBS= $(LDLIBS.lib) -ldl -lc
$(INTCPTLIB):=		DYNFLAGS= -h $(SONAME) $(ZTEXT) $(ZDEFS) $(INTCPTMFLAG) $(ABICULLING)
$(INTCPTLIB):=		ZDEFS=
$(INTCPTLIB):=		SONAME= $(INTCPTLIB)
$(INTCPTPICS):=		CFLAGS += -g -xs -xildoff
$(INTCPTPICS):=		CFLAGS64 += -g -xs -xildoff

$(ROOTLIBABI) :=	OWNER =		root
$(ROOTLIBABI) :=	GROUP =		bin
$(ROOTLIBABI) :=	DIRMODE =	775

CPPFLAGS=	-I../common -I$(SGSSRC)/link_audit/common -I. $(CPPFLAGS.master)
LINTFLAGS =	-mxsuF -errtags=yes
LINTLIBS +=	$(LDLIBS)
CLEANFILES +=	$(OBJDIR)/* $(PICDIR)/* $(APPTRACE_LINTLIB) $(INTCPT_LINTLIB)
CLOBBERFILES +=	$(ABILIBS) $(APPTRACEPROG) 

ROOTLIBABI=		$(ROOT)/usr/lib/abi
ROOTLIBABI64=		$(ROOT)/usr/lib/abi/$(MACH64)
ROOTLIBABILIBS=		$(ABILIBS:%=$(ROOTLIBABI)/%)
ROOTLIBABILIBS64=	$(ABILIBS:%=$(ROOTLIBABI64)/%)
REALROOTLIBABI=		/usr/lib/abi
REALROOTLIBABI64=	/usr/lib/abi/sparcv9

ROOTUSRBIN=		$(ROOT)/usr/bin
ROOTUSRBINS=		$(APPTRACEPROG:%=$(ROOTUSRBIN)/%)

FILEMODE=	0755

.PARALLEL:	$(LIBS) $(PROGS) $(SCRIPTS) \
		$(APPTRACEPICS) $(INTCPTPICS)  # More apptrace additions
