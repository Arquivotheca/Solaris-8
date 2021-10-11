#
# Copyright (c) 1993-1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.13	97/04/10 SMI"
#
# cmd/ssa/adm/Makefile
#
#      ssaadm (Administration program for the SPARCstorage Array) Makefile 
#

include	../../Makefile.cmd

PROG		= ssaadm
CPPFLAGS	+= -I../sys -v
LDLIBS		+= ../lib/libssa.a -ldevinfo -ldevice

OBJECTS		= ssaadm.o setboot.o fcupdate.o envsen.o devctl.o

LINT_OBJS	= $(OBJECTS:%.o=%.ln)

.KEEP_STATE:

all debug browser debug_browser: $(PROG)

$(PROG): $(OBJECTS)
	$(LINK.c) -o $@ $(OBJECTS) $(LDLIBS)
	$(POST_PROCESS)

debug	:= CFLAGS += -g
browser	:= CFLAGS += -xsb
debug_browser	:= CFLAGS += -xsb -g

install: all $(ROOTUSRSBINPROG)

lint: $(LINT_OBJS)
	$(LINT.c) $(LINT_OBJS)

clean:
	$(RM) $(OBJECTS)
	$(RM) $(LINT_OBJS)

include	../../Makefile.targ
