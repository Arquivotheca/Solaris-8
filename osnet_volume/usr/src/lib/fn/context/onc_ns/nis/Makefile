#
#ident	"@(#)Makefile	1.14	97/11/25 SMI"
#
# Copyright (c) 1989 - 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/context/onc_ns/nis/Makefile
#
include         ../../../../../Makefile.master

SUBDIRS=        $(MACH) $(BUILD64) $(MACH64)

RPCFILES=	common/fnsypprot.h common/fnsypprot_client.c common/fnsypprot_clnt.c common/fnsypprot_xdr.c

# include library definitions
include ../../../Makefile.libfn

all :=          TARGET= all
clean :=        TARGET= clean
clobber :=      TARGET= clobber
install :=      TARGET= install
install_h :=	TARGET = install_h
debug := 	TARGET = debug
tcov := 	TARGET = tcov
analyse := 	TARGET = analyse

.KEEP_STATE:

clean clobber : $(SUBDIRS)
all debug install: install_h $(RPCFILES) $(SUBDIRS)

install_h:

# For generating the RPC files in the libxfn directory
common/fnsypprot.h: common/fnsypprot.x
	cd common; $(RPCGEN) -C -A -h -o fnsypprot.h fnsypprot.x ; cd ..

common/fnsypprot_client.c: common/fnsypprot.h

common/fnsypprot_clnt.c: common/fnsypprot.x
	cd common; $(RPCGEN) -C -A -l -o fnsypprot_clnt.c fnsypprot.x; cd ..

common/fnsypprot_xdr.c: common/fnsypprot.x
	cd common; $(RPCGEN) -C -A -c -o fnsypprot_xdr.c fnsypprot.x; cd ..


# include library targets
include ../../../Makefile.targ

$(MACH) $(MACH64):      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:


