#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.29	99/01/25 SMI"
#
# lib/librpcsvc/Makefile
#
include  ../../Makefile.master

PROTOCOL_DIR= $(ROOT)/usr/include/rpcsvc

SUBDIRS=	spec .WAIT $(MACH) $(BUILD64) $(MACH64) $(SPEC64)

DERIVED_FILES=  \
	common/bootparam_prot_xdr.c	\
	common/rstat_xdr.c	\
	common/rusers_xdr.c	\
	common/spray_xdr.c	\
	common/nlm_prot.c	\
	common/sm_inter_xdr.c	\
	common/mount_xdr.c	\
	common/nsm_addr_xdr.c	\
	common/rpc_sztypes.c

CLEANFILES += $(DERIVED_FILES)

# include library definitions
include ../Makefile.lib

# definitions for install_h target
HDRS= 		$(TXTS)
ROOTHDRDIR=	$(ROOT)/usr/include/rpcsvc
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install

.KEEP_STATE:

all:	$(DERIVED_FILES) $(TXTS) .WAIT $(SUBDIRS)

install: all .WAIT $(SUBDIRS)

install_h: $(DERIVED_FILES) $(ROOTHDRS)

check: $(CHECKHDRS)

clean clobber : $(SUBDIRS)

MAPFILE=	mapfile-vers

#
# Rules for building the derived files
#
# Derived files
#
common/bootparam_prot_xdr.c: $(PROTOCOL_DIR)/bootparam_prot.x
	$(RPCGEN) -c $(PROTOCOL_DIR)/bootparam_prot.x > $@

common/rstat_xdr.c: $(PROTOCOL_DIR)/rstat.x
	$(RPCGEN) -c $(PROTOCOL_DIR)/rstat.x > $@

common/rusers_xdr.c: $(PROTOCOL_DIR)/rusers.x
	$(RPCGEN) -c $(PROTOCOL_DIR)/rusers.x > $@

common/sm_inter_xdr.c: $(PROTOCOL_DIR)/sm_inter.x
	$(RPCGEN) -c $(PROTOCOL_DIR)/sm_inter.x > $@

common/spray_xdr.c: $(PROTOCOL_DIR)/spray.x
	$(RPCGEN) -c $(PROTOCOL_DIR)/spray.x > $@

common/nlm_prot.c: $(PROTOCOL_DIR)/nlm_prot.x
	$(RPCGEN) -c $(PROTOCOL_DIR)/nlm_prot.x > $@

common/mount_xdr.c: $(PROTOCOL_DIR)/mount.x
	$(RPCGEN) -c $(PROTOCOL_DIR)/mount.x > $@

common/nsm_addr_xdr.c: $(PROTOCOL_DIR)/nsm_addr.x
	$(RPCGEN) -c $(PROTOCOL_DIR)/nsm_addr.x > $@

common/rpc_sztypes.c: $(PROTOCOL_DIR)/rpc_sztypes.x
	$(RPCGEN) -c $(PROTOCOL_DIR)/rpc_sztypes.x > $@

# include library targets
include ../Makefile.targ

$(MACH) $(MACH64) spec: FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
