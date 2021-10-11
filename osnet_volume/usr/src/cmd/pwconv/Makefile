#
#ident	"@(#)Makefile	1.10	96/03/19 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/pwconv/Makefile
#

PROG=           pwconv

include ../Makefile.cmd
TEXT_DOMAIN= SUNW_OST_ADMIN

FILEMODE=       0555
OWNER=          root
GROUP=          sys
RELSBIN=        ../sbin

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTUSRSBINPROG) $(ROOTPROG)

$(ROOTPROG):
	-$(RM) $@; $(SYMLINK) ../sbin/$(PROG) $@

clean:

lint:   lint_PROG
 
include ../Makefile.targ
