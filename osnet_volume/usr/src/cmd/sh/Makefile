#
# Copyright (c) 1989,1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.25	99/08/12 SMI"
#
# cmd/sh/Makefile
#

# build two versions, dynamic and static
SH_PROG =	sh
STATPROG =	sh
PROG =		$(SH_PROG)

POBJS=	sh_policy.o
OBJS=	args.o blok.o cmd.o defs.o error.o fault.o hash.o hashserv.o \
	io.o msg.o print.o stak.o string.o word.o xec.o \
	ctype.o echo.o expand.o func.o macro.o pwd.o setbrk.o test.o \
	bltin.o jobs.o ulimit.o
DOBJS=	main.o name.o service.o
SRCS=	$(OBJS:%.o=%.c) $(DOBJS:%.o=%.c) $(POBJS:%.o=%.c)

DYNAMIC_OBJS=	$(DOBJS:%=dobjs/%) $(POBJS:%=dobjs/%)
STATIC_OBJS=	$(DOBJS:%=sobjs/%)

PFSH_PROG =	pfsh
PFSH_ROOTPROG =	$(PFSH_PROG:%=$(ROOTBIN)/%)

# install rule for static version
# deliberately ahead of a similar rule in ../Makefile.cmd
#
ROOT= /proto
$(ROOT)/sbin/%: %.static
	$(INS.rename)

include ../Makefile.cmd

# This flag is being added only for SCO (x86) compatibility  
CFLAGS += $(iBCS2FLAG)

#
# for message cataloge
#
POFILE= sh.po
POFILES= $(SRCS:%.c=%.po)
XGETFLAGS += -a -x sh.xcl
CPPFLAGS += -D_FILE_OFFSET_BITS=64

$(OBJS)		:= CPPFLAGS += -DACCT
$(STATIC_OBJS)	:= CPPFLAGS += -DACCT
$(DYNAMIC_OBJS)	:= CPPFLAGS += -DACCT -DSETLOCALE_FIX -DDYNAMIC

LDLIBS += -lgen
LAZYLIBS = -lsecdb -lnsl
GROUP =	root
CLOBBERFILES += $(STATIC)

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(DYNAMIC_OBJS) $(STATIC_OBJS)

all: $(PROG) .WAIT $(STATIC) 

$(SH_PROG): dobjs .WAIT $(DYNAMIC_OBJS) $(OBJS)
	$(LINK.c) $(DYNAMIC_OBJS) $(OBJS) -o $@ $(LDLIBS) \
	-z lazyload $(LAZYLIBS)
	$(POST_PROCESS)

$(STATIC): sobjs .WAIT $(STATIC_OBJS) $(OBJS)
	-$(RM) -f $@
	$(LINK.c) $(STATIC_OBJS) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(PFSH_ROOTPROG): $(ROOTPROG)
	-$(RM) -f $@
	$(LN) -f $(ROOTPROG) $@

dobjs/%.o sobjs/%.o pobjs/%.o:	%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

dobjs sobjs pobjs:
	-@$(MKDIR) -p $@

$(POFILE): $(POFILES)
	$(RM) $@
	$(CAT)     $(POFILES) > $@

install: all $(ROOTSBINPROG) $(ROOTPROG) $(PFSH_ROOTPROG)
	$(RM) $(ROOTSBIN)/jsh
	$(LN) -f $(ROOTSBIN)/sh $(ROOTSBIN)/jsh
	$(RM) $(ROOTLIB)/rsh
	$(LN) -f $(ROOTBIN)/sh $(ROOTLIB)/rsh
	$(RM) $(ROOTBIN)/jsh
	$(LN) -f $(ROOTBIN)/sh $(ROOTBIN)/jsh

clean:	
	$(RM) $(DYNAMIC_OBJS) $(STATIC_OBJS) $(OBJS)

lint: lint_SRCS

include ../Makefile.targ
