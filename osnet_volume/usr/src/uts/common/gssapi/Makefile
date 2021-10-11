#
# Copyright (c) 1989,1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.21	99/12/06 SMI"
#
# uts/common/gssd/Makefile
#
# include global definitions
include ../../../Makefile.master


INSTALLED_HDRS=	gssapi.h gssapi_ext.h
PRIVATE_HDRS=	gssd.x gssd_prot.h
HDRS=	$(INSTALLED_HDRS) $(PRIVATE_HDRS)

DERIVED_FILES=	gssd_prot.h gssd_prot.c gssd_xdr.c

GSSDDIRS= $(ROOT)/usr/include/gssapi

GSSDHDRS= $(INSTALLED_HDRS:%=$(GSSDDIRS)/%)

CHECKHDRS= $(INSTALLED_HDRS:%.h=%.check)

# gssd_prot.h is rpcgen'ed and can never be made to pass
# cstyle so it is unchecked
UNCHECKED_HDRS=	gss_prot.h

# install rule
$(GSSDDIRS)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: all_h $(GSSDDIRS) $(GSSDHDRS)

all_h: $(DERIVED_FILES)

$(GSSDDIRS):
	$(INS.dir)

gssd_prot.h:	gssd.x
	$(RM) $@
	$(RPCGEN) -M -h gssd.x > $@

gssd_prot.c:   gssd.x
	$(RM) $@

#	Over ticotsord we do zero retries. Over ticlts we do 5
#	retries. Hence, a default of 25 seconds for ticotsord is
#	too little. 125 = 25 + 6 * MAXTIMO (from clnt_clts.c).
#
	$(RPCGEN) -M -l gssd.x | sed -e \
		's;#include..gssd.h.;#include "gssd_prot.h";' \
		| sed 's/TIMEOUT/gssd_timeout/' \
		| sed 's/{ 25, 0 }/{ 125, 0 }/' \
		| grep -v stdlib.h | grep -v stdio.h > $@

gssd_xdr.c:    gssd.x
	$(RM) $@
	$(RPCGEN) -M -c gssd.x | sed -e \
		's;#include..gssd.h.;#include "gssd_prot.h";' > $@

check:	$(CHECKHDRS)

clean:
	$(RM) $(DERIVED_FILES)

