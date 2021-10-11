#
#ident	"@(#)Makefile	1.3	94/12/10 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/ufs/i386/Makefile
#

BASEDIR = ../..

include $(BASEDIR)/ufs/Makefile.ufs

.KEEP_STATE:

all: $(INSTALLBOOT)

install: $(USR_SBIN_INSTALLBOOT)

lint clean: FRC

clobber: 
	-$(RM) $(INSTALLBOOT)

#
# Pattern matching rules for source in this directory
#
%: %.sh
	$(RM) $@
	cat $< > $@
	chmod +x $@

FRC:

