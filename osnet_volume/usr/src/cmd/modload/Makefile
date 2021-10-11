#
#ident	"@(#)Makefile	1.32	99/11/19 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/modload/Makefile
#
# makefile for loadable module utilities

DRVPROG = add_drv rem_drv
MODPROG = modinfo modunload modload
PROG = $(MODPROG) $(DRVPROG)

include ../Makefile.cmd

# Generic

COMMONSRC = modsubr.c drvsubr.c
COMMONOBJ = $(COMMONSRC:%.c=%.o)
SRCS = $(COMMONSRC) $(PROG:%=%.c)
OBJECTS = $(SRCS:%.c=%.o)

ROOTDRVPROG = $(DRVPROG:%=$(ROOTUSRSBIN)/%)
DRVCOMMONSRC = drvsubr.c
DRVCOMMONOBJ = $(DRVCOMMONSRC:%.c=%.o)

CLOBBERFILES = $(PROG) 

# install specifics

OWNER = root
$(ROOTDRVPROG) := FILEMODE = 0555

add_drv:= LDLIBS += -ldevinfo

.KEEP_STATE:

all: $(PROG) 

$(MODPROG): $(COMMONOBJ)

add_drv:	add_drv.o $(DRVCOMMONOBJ)
	$(LINK.c)  -o $@ $< add_drv.o $(DRVCOMMONOBJ) $(LDLIBS)
	$(POST_PROCESS)
 
rem_drv:	rem_drv.o $(DRVCOMMONOBJ)
	$(LINK.c)  -o $@ $< rem_drv.o $(DRVCOMMONOBJ) $(LDLIBS)
	$(POST_PROCESS)


%: %.c $(COMMONOBJ)
	$(LINK.c) -o $@ $< $(COMMONOBJ) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJECTS) core

lint: lint_SRCS

include ../Makefile.targ
