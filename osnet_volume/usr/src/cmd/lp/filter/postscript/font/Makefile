#
# ident	"@(#)Makefile	1.10	95/02/01 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/filter/postscript/font/Makefile
#

include			../../../Makefile.lp

PROG =			makedev

SRCS =			makedev.c

OBJS =			$(SRCS:%.c=%.o)

TXTS =			README Omakedev.c

SUBDIRS =		devpost

COMMONDIR =		../common
ROOTLIBFONT =		$(ROOTLIB)/font
ROOTLIBFONTPROG =	$(PROG:%=$(ROOTLIBFONT)/%)

CPPFLAGS =		-I$(COMMONDIR) $(CPPFLAGS.master)

NATIVEDIR =		native
NATIVEPROG =		$(NATIVEDIR)/$(PROG)
$(NATIVEPROG) :=	CC=$(NATIVECC)
$(NATIVEPROG) :=	LDLIBS=
$(NATIVEPROG) :=	CPPFLAGS.master=

.KEEP_STATE:

all :			$(TXTS) $(PROG) $(NATIVEPROG) $(SUBDIRS)

install :		$(PROG) $(ROOTLIBFONTPROG) $(SUBDIRS)

clean :			$(SUBDIRS)
			$(RM) $(OBJS)

clobber :		$(SUBDIRS) clean
			$(RM) $(PROG) $(NATIVEPROG)

strip:
			$(STRIP) $(PROG)

lint:
			$(LINT.c) $(SRCS)

$(NATIVEPROG) :		$(NATIVEDIR) $(SRCS)
			$(LINK.c) -o $@ $(SRCS) $(LDLIBS)

$(NATIVEDIR) :
			-@mkdir -p $@

$(ROOTLIBFONT)/%:	%
			$(INS.file)

$(PROG) :		$(SRCS)
			$(LINK.c) -o $@ $(SRCS) $(LDLIBS)
			$(POST_PROCESS)

$(SUBDIRS):		FRC
			@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
