#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.10	99/03/21 SMI"
#
# lib/libresolv2/Makefile.com

LIBRARY= libresolv.a
VERS= .2

OBJECTS=   	daemon.o    putenv.o       strcasecmp.o   \
		strsep.o    ftruncate.o    readv.o       \
		strdup.o    strtoul.o      gettimeofday.o  \
		setenv.o    strerror.o     utimes.o  \
		mktemp.o    setitimer.o    strpbrk.o   \
		writev.o  \
		hostnamelen.o    inet_net_pton.o  inet_ntop.o  \
		inet_addr.o      inet_neta.o      inet_pton.o  \
		inet_lnaof.o     inet_netof.o     nsap_addr.o  \
		inet_makeaddr.o  inet_network.o   inet_net_ntop.o \
		inet_ntoa.o  \
		dns.o       gen_ho.o       getnetgrent.o    \
		lcl_ng.o    nis_nw.o       dns_gr.o   \
		gen_ng.o    getprotoent.o  lcl_nw.o   \
		nis_pr.o    dns_ho.o       gen_nw.o    \
		getpwent.o  lcl_pr.o 	   nis_pw.o   \
		dns_nw.o    gen_pr.o       getservent.o     \
		lcl_pw.o    nis_sv.o 	   dns_pr.o   \
		gen_pw.o    hesiod.o       lcl_sv.o   \
		nul_ng.o    dns_pw.o       gen_sv.o    \
		irs_data.o  nis.o          util.o   \
		dns_sv.o    getgrent.o     lcl.o  \
		nis_gr.o    gen.o    	   gethostent.o   \
		lcl_gr.o    nis_ho.o       gen_gr.o   \
		getnetent.o lcl_ho.o       nis_ng.o    \
		base64.o    ev_files.o     ev_waits.o  \
		logging.o   bitncmp.o      ev_streams.o \
		eventlib.o  tree.o	   ev_connects.o \
		ev_timers.o heap.o  assertions.o memcluster.o \
		ns_name.o   ns_netint.o    ns_parse.o \
		ns_print.o  ns_ttl.o  \
		herror.o    res_debug.o    res_data.o \
		res_comp.o  res_init.o     res_mkquery.o \
		res_mkupdate.o  res_query.o    res_send.o \
		res_update.o sunw_mtctxres.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS +=		$(DYNLIB) $(LINTLIB)

# definitions for lint

#LINTLIB:=	SRCS=../common/llib-lresolv
#LINTFLAGS=	-u -I..
#LINTFLAGS64=	-u -I..
#LINTOUT=	lint.out
##
#LINTSRC=	$(LINTLIB:%.ln=../common/llib-lresolv)
#ROOTLINTDIR=	$(ROOTLIBDIR)
#ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)
$(LINTLIB):= 	SRCS=../common/llib-lresolv
$(LINTLIB):= 	LINTFLAGS=-nvx $(LOCFLAGS)
$(LINTLIB):= 	TARGET_ARCH=

LINTFLAGS=	-u  -I$(COMPINCL)
LINTFLAGS64=	-u  -I$(COMPINCL)
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)


CLEANFILES += 	$(LINTOUT) $(LINTLIB)
CLOBBERFILES +=	$(MAPFILE)
# Local Libresolv definitions
SOLCOMPAT =	-Dgethostbyname=res_gethostbyname \
	-Dgethostbyaddr=res_gethostbyaddr -Dgetnetbyname=res_getnetbyname \
	-Dgethostbyname2=res_gethostbyname2\
	-Dgetnetbyaddr=res_getnetbyaddr -Dsethostent=res_sethostent \
	-Dendhostent=res_endhostent -Dgethostent=res_gethostent \
	-Dsetnetent=res_setnetent -Dendnetent=res_endnetent \
	-Dgetnetent=res_getnetent -Dsocket=_socket

LOCFLAGS +=	-D_SYS_STREAM_H -D_REENTRANT -DSVR4 -DSUNW_OPTIONS \
		$(SOLCOMPAT) -I../include

CFLAGS +=	-v $(LOCFLAGS) 
CFLAGS64 +=	-v -DSUNW_LP64 $(LOCFLAGS)
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lsocket -lnsl -lc

.KEEP_STATE:

lint: $(LINTLIB)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

#objs/nlsdata.o objs/nlsenv objs/nlsrequest pics/nlsdata.o pics/nlsenv.o \
#	pics/nlsrequest.o:

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)
