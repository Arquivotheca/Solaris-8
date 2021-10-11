#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.8	99/03/30 SMI"

PROG= ucblinks
OBJS= $(PROG).o

SHLIBEXFILES= ucblinks.sh
SHLIBFILES= ucblinks.awk

include ../Makefile.ucbcmd

ROOTLIBEXFILES= $(ROOTLIB)/$(SHLIBEXFILES)
ROOTLIBFILES= $(ROOTLIB)/$(SHLIBFILES)
GROUP = sys
$(ROOTLIBEXFILES)	:= FILEMODE = 555
$(ROOTLIBFILES)		:= FILEMODE = 644
$(ROOTLIB)	:= GROUP = bin
$(PROG)	:= GROUP = bin

LDLIBS += -ldevinfo -lelf

CLEANFILES += $(OBJS)


.KEEP_STATE:

all: $(PROG) $(SHLIBEXFILES) $(SHLIBFILES)

install: all $(ROOTPROG) $(ROOTLIB) $(ROOTLIBEXFILES) $(ROOTLIBFILES)

$(ROOTLIB):
	$(INS.dir)

clean:
	$(RM) $(CLEANFILES)

lint:

include ../Makefile.ucbtarg
