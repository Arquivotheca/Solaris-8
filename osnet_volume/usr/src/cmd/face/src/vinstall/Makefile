#
#ident	"@(#)Makefile	1.5	94/08/12 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/face/src/vinstall/Makefile

SCRIPTS=	vsetup vmodify vdelete addmenu delmenu


# common command definitions
include		../../../Makefile.cmd

# common face definitions
include		../Makefile.com

ROOTSCRIPTS=	$(SCRIPTS:%=$(ROOTVMBIN)/%)

FILEMODE=	755

.KEEP_STATE:

all: $(SCRIPTS) 

install: all $(ROOTSCRIPTS)

$(ROOTSCRIPTS): $(ROOTVMBIN)

clean lint:

clobber:
	$(RM) $(SCRIPTS)
