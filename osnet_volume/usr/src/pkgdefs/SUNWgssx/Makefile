#
#pragma ident	"@(#)Makefile	1.9	99/01/28 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#

include	../Makefile.com

DATAFILES += depend i.tiservices

ACTION_SUNWfnsx=grep SUNWfnsx depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWfnsx	fnsx" >> depend; \
	chmod 444 depend );

ACTION_SUNWgss=grep SUNWgss depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWgss		32 bit gss" >> depend; \
	chmod 444 depend );

ACTION_SUNWgssc=grep SUNWgssc depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWgssc	gssapi configuration" >> depend; \
	chmod 444 depend );

.KEEP_STATE:

all: $(FILES) action

install: all pkg

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_SUNWfnsx)
	$(ACTION_SUNWgss)
	$(ACTION_SUNWgssc)
	touch $@

include ../Makefile.targ
