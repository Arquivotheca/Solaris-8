#
#pragma ident "@(#)Makefile	1.25	99/10/21 SMI"
#
# Copyright (c) 1992-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#
# Makefile for the nis+ daemon
#
#	cmd/rpcsvc/nis/rpc.nisd/Makefile
#

# XXX - Temporary until CC can pass the -s flag through.
STRIP = strip
SED = sed

INIT_OBJS= nisinit.o 

LOG_OBJS= nislog.o nis_log_common.o

SRV_OBJS= \
  nis_main.o     nis_ns_proc.o     nis_ib_proc.o     nis_xx_proc.o   \
  nis_db.o       nis_subr_proc.o   nis_log_common.o  yp_ns_proc.o    \
  nis_log_svc.o  nis_service.o     resolv_common.o   resolv.o	     \
  ypserv1.o      nis_multival.o	   nis_opacc.o

SRCS= \
  nis_main.c      nis_ns_proc.c     nis_ib_proc.c   nis_xx_proc.c   nis_db.c  \
  nis_subr_proc.c nis_log_common.c  yp_ns_proc.c    nisinit.c \
  nis_log_svc.c   nis_service.c     resolv_common.c resolv.c	  ypserv1.c \
  nis_multival.c  nis_opacc.c

OBJS= $(SRV_OBJS) $(INIT_OBJS)
PROG= nisinit rpc.nisd nislog
DERIVED_FILES= nis_svc.h
CLEANFILES += $(DERIVED_FILES)

include $(SRC)/cmd/Makefile.cmd

PROTOCOL_DIR= $(ROOT)/usr/include/rpcsvc
CPPFLAGS += -I$(SRC)/lib/libnsl/include
LDLIBS += -lnsl

SUBDIR= resolv_server

.KEEP_STATE:

all := TARGET= all
install := TARGET= install
clean := TARGET= clean
clobber := TARGET= clobber
lint := TARGET= lint

all: $(DERIVED_FILES) $(PROG) $(SUBDIR)

install: all $(ROOTUSRSBINPROG) $(SUBDIR)

clean:
	$(RM) $(LOG_OBJS) $(OBJS) $(CLEANFILES)

lint: lint_SRCS

#
# Explicit link rules for the actual programs to get around the C++
# situation.
#
nislog: $(LOG_OBJS)
	$(LINK.c) -o $@ $(LOG_OBJS) $(LDLIBS)
	$(POST_PROCESS)

nisinit: $(INIT_OBJS)
	$(LINK.c) -o $@ $(INIT_OBJS) $(LDLIBS)
	$(POST_PROCESS)

# XXX - Strip is temporary until CC can pass the -s flag through.
rpc.nisd: $(SRV_OBJS)
	$(CCC) -compat=4 -norunpath -nolib -o $@ $(SRV_OBJS) \
		-lm $(LDLIBS) -lnisdb -ldl -lC -lthread -lc
	$(CH)$(STRIP) $@
	$(POST_PROCESS)

$(SUBDIR): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

SRC_DER=nis_main.o nis_service.o nis_subr_proc.o nis_xx_proc.o nisinit.o
$(SRC_DER): nis_svc.h

FRC:

#
# Rules for building the derived files :
#
nis_svc.h: $(PROTOCOL_DIR)/nis.x $(PROTOCOL_DIR)/nis_object.x
	$(RPCGEN) -C -h $(PROTOCOL_DIR)/nis.x |\
	$(SED) -n -e '/EDIT_START/,$$ p' |\
	$(SED) -e 's/_2_svc/_svc/' |\
	$(SED) -e 's/_3_svc/_svc/' |\
	$(SED) -e 's/_3/_clnt/' |\
	$(SED) -e 's/_2/_clnt/' >nis_svc.h

include $(SRC)/cmd/Makefile.targ
