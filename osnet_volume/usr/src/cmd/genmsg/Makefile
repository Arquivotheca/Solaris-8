#
#ident  "@(#)Makefile 1.1     96/06/20 SMI"
#
# Copyright (c) 1995, by Sun Microsystems, Inc.
# All Rights Reserved.
#
# cmd/genmsg/Makefile

include ../Makefile.cmd

PROG=		genmsg
PSRC_Y=		genmsg.y
C_OBJS=         main.o util.o lex.yy.o
 
LEX=		lex
LEXARGS=
LEXSRCS=	genmsg.l
LEXINTSRCS=	lex.yy.c
LEXOBJS=	lex.yy.o

LDLIBS += -ll -lgen -lintl
YFLAGS = -d
CPPFLAGS = -I../head $(CPPFLAGS.master)
 
OBJS =          $(C_OBJS) $(PSRC_Y:%.y=%.o)
PSRC_C =        $(PSRC_Y:%.y=%.c)
SRCS =          $(C_OBJS:%.o=%.c) $(PSRC_C)
 
SED= sed
 
.KEEP_STATE:
 
all:    $(PROG)
 
install: all $(ROOTPROG)
 
$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)

$(LEXINTSRCS): $(LEXSRCS)
	$(LEX) $(LEXARGS) $(LEXSRCS)
 
$(PSRC_C) + y.tab.h:    $(PSRC_Y)
	$(YACC) $(YFLAGS) $(PSRC_Y)
	$(MV) y.tab.c $(PSRC_C)
 
$(OBJS): y.tab.h lex.yy.c
 
catalog:        $(POFILE)
 
$(POFILE):      $(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS)  > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	$(SED) "/^domain/d" messages.po > $@
	$(RM) $(POFILE).i messages.po
 
lint: $(SRCS) .WAIT lint_SRCS
 
clean:
	$(RM) $(OBJS) $(PSRC_C) y.tab.h lex.yy.c $(PSRC_C) *.po
 
include ../Makefile.targ
