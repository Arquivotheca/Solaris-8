#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	98/06/24 SMI"
#
# cmd/gss/gsscred_clean/Makefile
#

PROG=	gsscred_clean

SRCS=	gsscred_clean.ksh


include ../../Makefile.cmd

#
# Override $ROOTLIB
#
ROOTLIB=	$(ROOT)/usr/lib/gss

DIRS=	$(ROOTLIB)


#
# Override $FILEMODE
#
FILEMODE=744


#
# Set owner to root since it is executed out of crontab by root
#
OWNER=root
GROUP=sys


.KEEP_STATE:

#
# Rule for ksh files
#
.SUFFIXES:	.ksh

.ksh:
	$(RM) $@
	cat $< > $@
	chmod +x $@


all: $(PROG)

$(ROOTLIB):
	$(INS.dir)

$(ROOTLIB)/%:	%
	$(INS.file)

install: all $(DIRS) $(ROOTLIBPROG)

clean:

clobber:

lint:

include ../../Makefile.targ
