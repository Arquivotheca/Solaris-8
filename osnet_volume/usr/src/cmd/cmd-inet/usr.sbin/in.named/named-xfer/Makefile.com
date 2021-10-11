#
#pragma ident   "@(#)Makefile.com 1.2     00/09/14 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/in.named/named-xfer/Makefile
#
VER= 		LOCAL-`date +%y%m%d.10/14/00M%S`
HOSTNAMECMD= 	hostname || uname -n
DT=		`date`
SY=		solaris 
DIR=		`pwd` 
NMDXFR=		named-xfer
PROG=		$(NMDXFR) 
NMDDIR=		$(SRC)/cmd/cmd-inet/usr.sbin/in.named/named
NMDOBJS=   	db_glue.o ns_glue.o tmp_version.o 
NMDXFROBJS=	$(NMDOBJS) named-xfer.o
NMDXFRSRCS=   	../common/named-xfer.c

SUBDIRS=
CLOBBERFILES += $(PROG) $(NMDOBJS)

debug :=	CPPFLAGS += $(DEBUG)
debug :=	COPTFLAG = -g
debug :=	CCOPTFLAG = -g

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

SOLCOMPAT = -Dgethostbyname=res_gethostbyname \
	-Dgethostbyaddr=res_gethostbyaddr -Dgetnetbyname=res_getnetbyname \
	-Dgetnetbyaddr=res_getnetbyaddr -Dsethostent=res_sethostent \
	-Dendhostent=res_endhostent -Dgethostent=res_gethostent \
	-Dsetnetent=res_setnetent -Dendnetent=res_endnetent \
	-Dgetnetent=res_getnetent

COMPDIR   = ../../compat
COMPINCL  = $(SRC)/lib/libresolv2/include 
NMDINCL	  = $(NMDDIR)/common

LOCFLAGS +=	-DXSTATS -DSUNW_OPTIONS  -DSVR4 -D_SYS_STREAM_H $(SOLCOMPAT) \
		-I$(COMPINCL) -I$(NMDINCL)

LDLIBS +=	-lresolv -lsocket -lnsl -lc
LDLIBS64 +=	-lresolv -lsocket -lnsl -lc

CLEANFILES += $(NMDOBJS)
CFLAGS +=	-I../common $(LOCFLAGS)
CFLAGS64 +=	-I../common $(LOCFLAGS)

.KEEP_STATE:

# all:: $(PROG) 

# debug install: all $(ROOTUSRSBINPROG) $(SUBDIRS) 

$(NMDXFR): $(NMDXFROBJS) 
	$(LINK.c) $(NMDXFROBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)


%.o: $(NMDDIR)/common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)


$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

clean:	$(SUBDIRS)
	$(RM) $(PROG)
	$(RM) $(NMDOBJS) $(NMDXFROBJS)
	$(RM) version.o

clobber: $(SUBDIRS)
	$(RM) $(NMDOBJS) $(PROG)

lint: $(SUBDIRS)
	$(LINT.c) $(SRCS)
