#
# ident	"@(#)Makefile	1.7	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/filter/postscript/dpost/Makefile
#

include         ../../../Makefile.lp

PROG=           dpost

SRCS =		dpost.c draw.c color.c pictures.c ps_include.c

OBJS =		$(SRCS:%.c=%.o)

COMMONDIR=      ../common

COMMONOBJS =	$(COMMONDIR)/glob.o \
		$(COMMONDIR)/misc.o \
		$(COMMONDIR)/request.o \
		$(COMMONDIR)/tempnam.o

TXTS=		README 

LDLIBS +=	-lm

# dpost uses some floating point arithmetic, so if you're running on a system
# without floating point hardware add the -f option to the definition of CFLAGS.

ENCODING=	2
CPPFLAGS =	-DDFLTENCODING=$(ENCODING)	\
		-I. -I$(COMMONDIR)		\
		$(CPPFLAGS.master)

.KEEP_STATE:

all:		$(TXTS) $(PROG)

install:	all $(ROOTLIBLPPOSTPROG)

$(PROG):	$(OBJS) $(COMMONOBJS)
		$(LINK.c) -o $@ $(OBJS) $(COMMONOBJS) $(LDLIBS)
		$(POST_PROCESS)

$(COMMONOBJS):	$$(@:%.o=%.c)
		cd $(@D); $(MAKE) $(@F)

ps_include.o:	ps_include.h
ps_include.h:	ps_include.ps ps_include.awk
		$(RM) $@; awk -f ps_include.awk ps_include.ps >$@

clean:
		$(RM) $(OBJS) ps_include.h

strip:
		$(STRIP) $(PROG)

lint:		lint_SRCS

include		../../../../Makefile.targ
