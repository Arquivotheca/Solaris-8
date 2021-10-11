#
# ident	"@(#)Makefile	1.40	00/09/14 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# build two versions, dynamic and static

PROG= su
STATPROG= su

# install rule for static version
# deliberately ahead of a similar rule in ../Makefile.cmd
#
ROOT= /proto
$(ROOT)/sbin/%: %.static
	$(INS.rename)

include ../Makefile.cmd

DFLTD= $(ROOTETC)/default

ETCDFLTPROG = $(PROG:%=$(DFLTD)/%)

DLLIB = -ldl
LDLIBS_LIST = -lcrypt_i -lcmd -lbsm -lmp \
	-lnss_files -lnss_nis -lnss_nisplus -lnss_compat -lnss_dns \
	-lsocket -lnsl -lstraddr $(DLLIB)
LDLIBS += $(LDLIBS_LIST)
#set /usr/bin/su to 4555; set /sbin/su to 0555
FILEMODE= 04555
STATICFILEMODE= 0555
$(ROOTSBINPROG) := FILEMODE = 0555
OWNER= root
GROUP= sys
$(ROOTSBINPROG) := FILEMODE = $(STATICFILEMODE)

# conditional assignments for the static version
#
# A partially static version is actually created, with just libdl
# linked dynamically.
#

$(STATIC) := DLLIB =

$(STATIC) := LDLIBS = -Bstatic $(LDLIBS_LIST) -lgen -lc -Bdynamic -ldl -Bstatic

LDLIBS += -lpam

$(STATIC) := LDFLAGS = $(LDLIBS.cmd) -dy -R/etc/lib $(LDFLAGS.cmd)

LDFLAGS += -DDYNAMIC_SU

$(ETCDFLTPROG) := FILEMODE = $(LIBFILEMODE)

.KEEP_STATE:

all: $(PROG) $(STATIC) $(PROG).dfl 

# install rule for the default file
$(DFLTD)/%: %.dfl
	$(INS.rename)

# install rule for non-setuid statically linked su
# (exec'd by /sbin/sulogin when booting single user)


install: all $(ROOTSBINPROG) $(ROOTPROG) $(ETCDFLTPROG)

$(ROOTSBINPROG): $(STATIC)
	$(RM) $(ROOTSBINPROG);
	$(RM) $(ROOTSBIN)/su.static;
	$(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) \
		-f $(@D) $(STATIC);
	$(SYMLINK) ../usr/bin/$(PROG) $(ROOTSBINPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
