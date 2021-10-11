#
#ident	"@(#)Makefile	1.23	97/03/20 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

# build two versions, dynamic and static
PROG= init 
STATPROG= init 

DFLTD= $(ROOTETC)/default

ROOTDFLTPROG = $(PROG:%=$(DFLTD)/%)
$(ROOTDFLTPROG) := FILEMODE = $(LIBFILEMODE)

# install rule for static version
# deliberately ahead of a similar rule in ../Makefile.cmd
#
ROOT= /proto
$(ROOT)/sbin/%: %.static
	$(INS.rename)

include ../Makefile.cmd

OWNER = root
GROUP = sys
LDLIBS_LIST = -lpam -lmp -lcmd -lsocket -lbsm -lnsl -lkstat
LDLIBS += $(LDLIBS_LIST)
CLOBBERFILES= $(STATIC)

# conditional assignments for the static version
#
$(STATIC) := DLLIB =

$(STATIC) := LDLIBS = -Bstatic -lc $(LDLIBS_LIST) -lgen \
			-Bdynamic -ldl -Bstatic

$(STATIC) := LDFLAGS = $(LDLIBS.cmd) -dy -R/etc/lib $(LDFLAGS.cmd)

.KEEP_STATE:

all: $(PROG) $(STATIC) $(PROG).dfl 

$(STATIC): $(PROG).c
		LD_OPTIONS=-I/etc/lib/ld.so.1 $(LINK.c) $(PROG).c -o $@ $(LDLIBS)

install: all $(PROG).dfl $(ROOTDFLTPROG) \
		$(ROOTSBINPROG) $(ROOTUSRSBINPROG)
	$(RM) $(ROOTETCPROG)
	$(RM) $(ROOTETC)/telinit
	$(SYMLINK) ../sbin/$(PROG) $(ROOTETCPROG)
	$(SYMLINK) ../sbin/$(PROG) $(ROOTETC)/telinit
	$(RM) $(ROOTETC)/TIMEZONE
	cd $(ROOTETC); $(SYMLINK) default/init TIMEZONE


$(DFLTD)/% : %.dfl
	$(INS.rename)

test:
	rtest $(PROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
