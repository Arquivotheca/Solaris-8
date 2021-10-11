#
# ident	"@(#)Makefile	1.15	98/08/26 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/dr_daemon/sparc/sun4u/Makefile
#
# Makefile for the dr_daemon command.
#
# PLATFORM is the target for the binary installation.
#

PROG=		dr_daemon
PLATFORM=	SUNW,Ultra-Enterprise-10000

CLASS=		32

SERVER_OBJS=	dr_daemon_svc.o dr_daemon_xdr.o

SVCOBJS=	dr_daemon_proc.o dr_attach.o dr_openprom.o dr_subr.o \
                dr_daemon_main.o dr_daemon_auth.o dr_query.o dr_ap.o\
                dr_kernel.o dr_obp_config.o dr_detach.o dr_config_cost.o \
                dr_info_io.o dr_info_ap.o dr_info_devt.o dr_info_olds.o \
                dr_info_net.o dr_info_rsm.o

DR_MACH_OBJS=	dr_mach_info_devt.o dr_mach_info_io.o dr_mach_obp_config.o

DERIVED_FILES=	dr_daemon.h dr_daemon_svc.c dr_daemon_xdr.c dr_daemon_clnt.c

HDRS=		dr_ap.h dr_subr.h dr_kernel.h dr_info.h dr_openprom.h

RPC_SRC=	dr_daemon.x

LINT_SRCS=	$(SVCOBJS:%.o=%.c) $(DR_MACH_OBJS:%.o=%.c)

LINTFILES=	$(LINT_SRCS:%.c=%.ln)

OBJS=		$(SERVER_OBJS) $(SVCOBJS) $(DR_MACH_OBJS)
SRCS=		$(DERIVED_FILES) $(OBJS:%.o=%.c)

CLOBBERFILES	+= $(DERIVED_FILES)

include ../../../Makefile.cmd
include ../../../../Makefile.psm

FILEMODE= 0544
OWNER= root
GROUP= sys

PSMPROG	= $(ROOT_PSM_LIB_DIR)/$(PROG)

PLAT_CPPFLAGS =	-D_XFIRE -DAP
CPPFLAGS = -I. -I$(USR_PSM_INCL_DIR) $(CPPFLAGS.master) $(PLAT_CPPFLAGS)

LINT_FLAGS = -cux

LDLIBS +=       -lkvm \
                -lelf \
                -ladm \
                -lnsl \
                -lsocket \
                -lsunmath \
                -lm \
                -ldl \
                -lkstat \
                -ldevinfo

.KEEP_STATE:

# Below are rules for the main targets
all: $(PROG)

install: all .WAIT $(PSMPROG)

$(PROG): $(DERIVED_FILES) $(HDRS) $(OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) $(PROG) $(OBJS) $(DERIVED_FILES) $(LINTFILES)

lint:	$(SRCS)
	$(LINT) $(LINT_FLAGS) $(CPPFLAGS) $(LINT_SRCS)

# Below are rules for producing the DERIVED_FILES
dr_daemon.h:	dr_daemon.x
	$(RM) $@
	$(RPCGEN) -h $(PLAT_CPPFLAGS) $(RPC_SRC) > $@

dr_daemon_svc.c: dr_daemon.x
	$(RM) $@
	$(RPCGEN) -m -K -1 $(PLAT_CPPFLAGS) $(RPC_SRC) > $@

dr_daemon_xdr.c: dr_daemon.x
	$(RM) $@
	$(RPCGEN) -c $(PLAT_CPPFLAGS) $(RPC_SRC) > $@

dr_daemon_clnt.c: dr_daemon.x
	$(RM) $@
	$(RPCGEN) -l $(PLAT_CPPFLAGS) $(RPC_SRC) > $@

#
# Below are notes extracted from the Makefiles of the old DR daemon on how to
# construct the various test programs (all of the test code is assumed broken
# by DR 2.next changes to the daemon).
#
# # testclnt is a program to test NODAEMON stubs in dr_daemon_proc.c
# # make testclnt COPTFLAG=-g EXTRA_OPTIONS='-DTEST_CLNT -DNODAEMON \
# # 	-DSTUB_TEST' AP_FLAGS= AP_LIB=
#
# TESTCLNTOBJS=   dr_daemon_proc.o testdr.o dr_daemon_xdr.o
#
# #
# # test* programs are daemon/test driver merge without the RPC mechanism.
# #
# # Testsvc can be run on a Sparc workstation when compiled:
# # make testsvc COPTFLAG=-g EXTRA_OPTIONS='-DTEST_SVC -DNO_DRAGON \
# # 	-DDR_TEST_CONFIG -DNO_SU -DRPC_SVC_FG' AP_FLAGS= AP_LIB=
# #
# # Testcrs can be run on a non-dedicated superdragon (test the IO info code)
# # make testcrs COPTFLAG=-g EXTRA_OPTIONS='-DTEST_SVC -DDR_TEST_CONFIG \
# # 	-DNO_SU -DRPC_SVC_FG -DNODRKERN' AP_FLAGS= AP_LIB=
# #
# # Testdr can be run on the real DR kernel when compiled:
# # make testdr COPTFLAG=-g EXTRA_OPTIONS='-DTEST_SVC -DDR_TEST_CONFIG \
# # 	-DRPC_SVC_FG'
# #
# TESTSVCOBJS=    dr_daemon_proc.o dr_attach.o dr_openprom.o dr_subr.o \
#                 dr_query.o dr_ap.o\
#                 dr_kernel.o dr_obp_config.o dr_detach.o dr_config_cost.o \
#                 testdr.o dr_daemon_xdr.o dr_info_io.o dr_info_ap.o \
#                 dr_info_devt.o dr_info_olds.o dr_info_net.o dr_info_rsm.o
# 
# CFLAGS +=	$(EXTRA_OPTIONS)
# 
# testclnt:       $(TESTCLNTOBJS)
#         $(LINK.c) -o $@ $(TESTCLNTOBJS) -lnsl -lm
# 
# testsvc:        $(TESTSVCOBJS)
#         $(LINK.c) -o $@ $(TESTSVCOBJS) -lnsl -lm
# 
# testcrs:        $(TESTSVCOBJS)
#         $(LINK.c) -o $@ $(TESTSVCOBJS) -lnsl -lsocket -lkvm -lelf -lm
# 
# testdr:         $(TESTSVCOBJS)
#         $(LINK.c) -o $@ $(TESTSVCOBJS) $(LDLIBS)


include ../../../Makefile.targ
include ../../../../Makefile.psm.targ
