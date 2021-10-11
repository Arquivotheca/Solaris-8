#
#ident	"@(#)Makefile	1.8	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/osadmin/cmain/Makefile
#

SRCS = chkyn.c spclsize.c
PROG = $(SRCS:%.c=%)


include ../../Makefile.cmd
include ../Makefile.include

LOCALLIBS = ../csub/csub.a
LDLIBS += $(LOCALLIBS)
CPPFLAGS = -I../include $(CPPFLAGS.master)

.KEEP_STATE:

all: $(LOCALLIBS) $(PROG) 

$(LOCALLIBS):
	@cd $(@D); pwd; $(MAKE) all
	@pwd

install: all $(ROOTLIB) $(ROOTLIBPROG)

$(ROOTLIB):
	$(INS.dir)

clean:

lint: lint_SRCS

include ../../Makefile.targ
