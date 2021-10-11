#
#ident	"@(#)Makefile	1.15	97/11/12 SMI"
#
# Copyright (c) 1989, 1995 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/sx/sparc/sun4m/sxconfig
#

#
#	Create default so empty rules don't
#	confuse make
#
CLASS		= 32

PROG=		sxconfig
SHFILES=	$(PROG)
PLATFORM=	sun4m

include ../../../../Makefile.cmd
include ../../../../../Makefile.psm

FILEMODE= 0555
OWNER= root
GROUP= sys

ROOTPSMPROG	= $(USR_PSM_SBIN_DIR)/$(PROG)

CPPFLAGS = -I$(USR_PSM_INCL_DIR) $(CPPFLAGS.master)

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPSMPROG)

clean:
	$(RM) $(PROG)

include ../../../../Makefile.targ
include ../../../../../Makefile.psm.targ
