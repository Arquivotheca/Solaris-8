#
#ident  "@(#)Makefile 1.13     99/07/18 SMI"
#
# Copyright (c) 1996,1997, by Sun Microsystems, Inc.
# All rights reserved.
#

include ../../Makefile.cmd

TXTS=
GSSTXTS= mech qop gsscred.conf
KRB5TXTS= warn.conf krb5.conf

IETCFILES=			$(TXTS:%=$(ROOTETC)/%)
IETCGSSFILES=			$(GSSTXTS:%=$(ROOTETC)/gss/%)
IETCKRB5FILES=			$(KRB5TXTS:%=$(ROOTETC)/krb5/%)
GSSDIR=				$(ROOTETC)/gss
KRB5DIR=			$(ROOTETC)/krb5

FILEMODE= 0644
OWNER= root
GROUP= sys


.KEEP_STATE:

all: $(TXTS) $(GSSTXTS) dummy_mech_token.conf

dummy_mech_token.conf:	dummy_mech_token.conf.sh
			$(RM) $@
			sh dummy_mech_token.conf.sh

install: all $(GSSDIR) $(KRB5DIR) $(IETCFILES) $(IETCGSSFILES) $(IETCKRB5FILES)

install_h:

$(GSSDIR):
	$(INS.dir)

$(KRB5DIR):
	$(INS.dir)

$(ROOTETC)/%: %
	$(INS.file)

$(ROOTETC)/gss/%: %
	$(INS.file)

$(ROOTETC)/krb5/%: %
	$(INS.file)

FRC:

include ../../Makefile.targ

CLOBBERFILES += dummy_mech_token.conf

clean lint:
