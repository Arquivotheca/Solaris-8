#
#ident	"@(#)Makefile	1.7	92/11/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG=	profile .login 
PROGSKEL= local.login local.profile local.cshrc

include ../Makefile.cmd

ROOTETCSKEL= $(ROOTETC)/skel
ROOTETCSKELPROG= $(PROGSKEL:%=$(ROOTETCSKEL)/%)
FILEMODE= $(LIBFILEMODE)
OWNER= root
GROUP= sys

$(ROOTETC)/profile :=	FILEMODE =	0644
$(ROOTETC)/.login  :=	FILEMODE =	0644
$(ROOTETCSKEL)/local.login  :=	FILEMODE =	0644
$(ROOTETCSKEL)/local.cshrc  :=	FILEMODE =	0644
$(ROOTETCSKEL)/local.profile := FILEMODE =	0644

.login: login.csh
	$(RM) .login; cp login.csh .login

.KEEP_STATE:

all: $(PROG) $(PROGSKEL) 

$(ROOTETCSKEL)/%: %
	$(INS.file)

$(ROOTETCSKEL):
	$(INS.dir)

install: all $(ROOTETCSKEL) $(ROOTETCPROG) $(ROOTETCSKELPROG)

clean:

clobber:
	$(RM) .login

lint:

