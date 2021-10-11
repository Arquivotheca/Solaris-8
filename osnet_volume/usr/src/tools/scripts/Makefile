#
#ident	"@(#)Makefile	1.3	99/11/01 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

SHFILES= \
	Install \
	bfu \
	bldenv \
	build_cscope \
	check_rpaths \
	checkproto \
	cstyle \
	elfcmp \
	hdrchk \
	jstyle \
	keywords \
	makebfu \
	mkbfu \
	nightly \
	protocmp.terse \
	sccscp \
	sccshist \
	sccsmv \
	sccsrm \
	ws \
	wx

MAN1FILES= \
	Install.1 \
	nightly.1 \
	sccscp.1 \
	sccsmv.1 \
	sccsrm.1 \
	ws.1

include ../Makefile.tools

OWNER=		root
GROUP=		bin
$(ROOTONBLDMAN1FILES) := FILEMODE=	644

.KEEP_STATE:

all:	$(SHFILES) $(MAN1FILES)

install: all .WAIT $(ROOTONBLDSHFILES) $(ROOTONBLDMAN1FILES)

clean:
	$(RM) $(CLEANFILES)

include ../Makefile.targ

