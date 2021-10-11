#
#pragma ident	"@(#)Makefile	1.6	97/03/12 SMI"
#
# Copyright (c) 1993,1997 by Sun Microsystems, Inc.
# All rights reserved.
#

include ../Makefile.com

DATAFILES += depend i.preserve i.renameold
CLASS_ACTION_SCRIPTS = r.preserve
CLOBBERFILES += action

ACTION_SUNWsprot=grep SUNWsprot depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWsprot     SPARCompilers Bundled tools" >> depend; \
	chmod 444 depend );

ACTION_SUNWnisr=grep SUNWnisr depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWnisr      Network Information System, (Root)" >> depend; \
	chmod 444 depend )

ACTION_SUNWnisu=grep SUNWnisu depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWnisu      Network Information System, (Usr)" >> depend; \
	chmod 444 depend )

ACTION_SUNWnskit=grep SUNWnskit depend > /dev/null || \
	( chmod 666 depend; \
	echo "I SUNWnskit     BCP NIS Server for 5.x" >> depend; \
	chmod 444 depend )

ACTION_SUNWnsktr=grep SUNWnsktr depend > /dev/null || \
	( chmod 666 depend; \
	echo "I SUNWnsktr     NIS Server for Solaris (root)" >> depend; \
	chmod 444 depend )

ACTION_SUNWnsktu=grep SUNWnsktu depend > /dev/null || \
	( chmod 666 depend; \
	echo "I SUNWnsktu     NIS Server for Solaris (usr)" >> depend; \
	chmod 444 depend )

.KEEP_STATE:

all: $(FILES) $(CLASS_ACTION_SCRIPTS) action

install: all pkg

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_SUNWsprot)
	$(ACTION_SUNWnisr)
	$(ACTION_SUNWnisu)
	$(ACTION_SUNWnskit)
	$(ACTION_SUNWnsktr)
	$(ACTION_SUNWnsktu)
	touch $@

include ../Makefile.targ
