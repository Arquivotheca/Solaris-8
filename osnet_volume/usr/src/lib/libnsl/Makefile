#
#pragma ident	"@(#)Makefile	1.80	99/12/06 SMI"
#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libnsl/Makefile
#
PROTOCOL_DIR= $(ROOT)/usr/include/rpcsvc
PROTOCOL_SRCDIR= $(SRC)/head/rpcsvc
PROTOCOL_UTS_SRCDIR= $(SRC)/uts/common/rpc

SUBDIRS = spec .WAIT $(MACH) $(BUILD64) .WAIT $(MACH64)

# objects are listed by source directory

# common utility code used in more than one directory
RPC_DERIVED_FILES= \
	rpc/key_prot.c

GEN_DERIVED_FILES= \
	nis/gen/nis_clnt.h      \
	nis/gen/nis_clnt.c	\
	nis/gen/nis_xdr.c       \
	nis/gen/nis_cback_xdr.c

CACHE_DERIVED_FILES= \
	nis/cache/nis_clnt.h	\
	nis/cache/nis_cache.h	\
	nis/cache/nis_cache_clnt.cc \
	nis/cache/nis_cache_xdr.cc

PROTOCOL_FILES= \
	$(PROTOCOL_DIR)/nis.x		\
	$(PROTOCOL_DIR)/nis.h		\
	$(PROTOCOL_DIR)/nis_object.x	\
	$(PROTOCOL_DIR)/nis_callback.x	\
	$(PROTOCOL_DIR)/nis_callback.h	\
	$(PROTOCOL_DIR)/nis_cache.x	\
	$(PROTOCOL_DIR)/nis_cache.h

PROTOCOL_FILES_UTS= \
	$(PROTOCOL_DIR)/key_prot.x

DERIVED_FILES= $(GEN_DERIVED_FILES) $(CACHE_DERIVED_FILES) $(RPC_DERIVED_FILES)

#
# Make sure they get cleaned when necessary
#
CLEANFILES += $(DERIVED_FILES) $(PROTOCOL_FILES) $(PROTOCOL_FILES_UTS)

# include library definitions
include ../Makefile.lib

TXTS +=		dial/dk.h	dial/uucp.h	dial/parms.h	dial/sysfiles.h

SED=	sed
CP=	cp
GREP=	grep

LIBRARY= libnsl.a
TEXT_DOMAIN= SUNW_OST_NETRPC
POFILE= $(LIBRARY:.a=.po)
POFILES= generic.po _errlst.po

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
_msg :=		TARGET= _msg
package :=	TARGET= package


.KEEP_STATE:

all:		$(PROTOCOL_DIR) $(DERIVED_FILES) txts .WAIT $(SUBDIRS)

headers:	$(PROTOCOL_DIR) .WAIT $(PROTOCOL_FILES) $(PROTOCOL_FILES_UTS) \
		$(DERIVED_FILES)

txts:		$(TXTS)

install:	all $(SUBDIRS)

clean clobber delete lint package:	$(SUBDIRS)

$(PROTOCOL_DIR):
	$(INS.dir)

$(PROTOCOL_DIR)/%.h:	$(PROTOCOL_SRCDIR)/%.h
	$(INS.file)

$(PROTOCOL_DIR)/nis.h:	$(PROTOCOL_SRCDIR)/nis.x $(PROTOCOL_SRCDIR)/nis_object.x
	$(RPCGEN) -C -h $(PROTOCOL_SRCDIR)/nis.x |\
	$(SED) -e '/EDIT_START/,$$ d' > nis.h
	$(RM) $@
	$(INS) -s -m $(FILEMODE) -f $(@D) nis.h
	$(RM) nis.h

$(PROTOCOL_DIR)/nis_callback.h:	$(PROTOCOL_SRCDIR)/nis_callback.x
	$(RPCGEN) -C -h $(PROTOCOL_SRCDIR)/nis_callback.x -o nis_callback.h
	$(RM) $@
	$(INS) -s -m $(FILEMODE) -f $(@D) nis_callback.h
	$(RM) nis_callback.h

$(PROTOCOL_DIR)/nis_cache.h:	$(PROTOCOL_SRCDIR)/nis_cache.x
	$(RPCGEN) -C -h $(PROTOCOL_SRCDIR)/nis_cache.x -o nis_cache.h
	$(RM) $@
	$(INS) -s -m $(FILEMODE) -f $(@D) nis_cache.h
	$(RM) nis_cache.h

$(PROTOCOL_DIR)/%.x:	$(PROTOCOL_SRCDIR)/%.x
	$(INS.file)

$(PROTOCOL_DIR)/%.x:	$(PROTOCOL_UTS_SRCDIR)/%.x
	$(INS.file)

#
# Rules for building the derived files
#
# Derived header files
#
nis/gen/nis_clnt.h: $(PROTOCOL_DIR)/nis.x
	$(RPCGEN) -C -h $(PROTOCOL_DIR)/nis.x |\
	$(SED) -n -e '/EDIT_START/,$$ p' |\
	$(SED) -e 's/_3_svc/_svc/' |\
	$(SED) -e 's/_3/_clnt/' > $@

nis/cache/nis_clnt.h: $(PROTOCOL_DIR)/nis.x
	$(RPCGEN) -C -h $(PROTOCOL_DIR)/nis.x |\
	$(SED) -n -e '/EDIT_START/,$$ p' |\
	$(SED) -e 's/_3_svc/_svc/' |\
	$(SED) -e 's/_3/_clnt/' > $@
#
# Derived source files
#
nis/gen/nis_clnt.c: $(PROTOCOL_DIR)/nis.x
	$(RPCGEN) -C -l $(PROTOCOL_DIR)/nis.x |\
	$(SED) -e 's!\"$(PROTOCOL_DIR)/nis.h\"!\<rpcsvc/nis.h\>!' |\
	$(SED) -e 's/_3/_clnt/' > $@

nis/gen/nis_xdr.c: $(PROTOCOL_DIR)/nis.x
	$(RPCGEN) -C -c $(PROTOCOL_DIR)/nis.x |\
	$(SED) -e 's!\"$(PROTOCOL_DIR)/nis.h\"!\<rpcsvc/nis.h\>!' > $@

nis/gen/nis_cback_xdr.c: $(PROTOCOL_DIR)/nis_callback.x
	$(RPCGEN) -C -c $(PROTOCOL_DIR)/nis_callback.x |\
	$(SED) -e 's!\"$(PROTOCOL_DIR)/nis_callback.h\"!\<rpcsvc/nis_callback.h\>!' > $@

nis/cache/nis_cache_xdr.cc: $(PROTOCOL_DIR)/nis_cache.x
	$(RPCGEN) -C -c $(PROTOCOL_DIR)/nis_cache.x |\
        $(SED) -e 's!\"$(PROTOCOL_DIR)/nis_cache.h\"!"nis_cache.h"!' > $@

nis/cache/nis_cache_clnt.cc: $(PROTOCOL_DIR)/nis_cache.x
	$(RPCGEN) -C -l $(PROTOCOL_DIR)/nis_cache.x |\
        $(SED) -e 's!\"$(PROTOCOL_DIR)/nis_cache.h\"!"nis_cache.h"!' > $@

nis/cache/nis_cache.h: $(PROTOCOL_DIR)/nis_cache.x
	$(RPCGEN) -C -h $(PROTOCOL_DIR)/nis_cache.x |\
        $(SED) -e 's!\"$(PROTOCOL_DIR)/nis_cache.h\"!"nis_cache.h"!' > $@

rpc/key_prot.c: $(PROTOCOL_DIR)/key_prot.x
	$(RPCGEN) -C -c $(PROTOCOL_DIR)/key_prot.x |\
        $(SED) -e 's!"$(PROTOCOL_DIR)/key_prot.h"!<rpc/key_prot.h>!' > $@

# include library targets
include ../Makefile.targ


_msg: $(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

_errlst.po:
	$(RM) messages.po
	$(XGETTEXT) -a nsl/_errlst.c
	$(SED) -e '/^# msg/d' -e '/^domain/d' messages.po > $@
	$(RM) messages.po

generic.po:
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext */*.[ch] nis/*/*.[ch]*`
	$(SED) -e '/^# msg/d' -e '/^domain/d' messages.po > $@
	$(RM) messages.po

$(MSGDOMAIN):
	$(INS.dir)

spec $(MACH) $(MACH64) $(SPEC) $(SPEC64):      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
