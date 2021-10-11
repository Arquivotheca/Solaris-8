#
#pragma ident   "@(#)Makefile.com 1.7     00/09/14 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/in.named/named/Makefile
#
VER= 		"BIND 8.1.2"
HOSTNAMECMD= 	hostname || uname -n
DT=		`date`
SY=		solaris 
DIR=		`pwd` 
YACC=		yacc -d
NMD=		in.named
PROG=		$(NMD) 
NMDHDRS=	db_defs.h db_glob.h ns_defs.h ns_glob.h ns_parser.h named.h pathnames.h
NMDSRCS=   	../common/tmp_version.c ../common/db_dump.c \
		../common/db_load.c ../common/db_lookup.c ../common/db_save.c \
        	../common/db_update.c ../common/db_glue.c \
        	../common/ns_parser.c ../common/ns_lexer.c \
		../common/ns_parseutil.c  ../common/ns_forw.c \
		../common/ns_init.c ../common/ns_main.c ../common/ns_maint.c \
        	../common/ns_req.c ../common/ns_resp.c ../common/ns_stats.c \
		../common/ns_ncache.c \
		../common/ns_xfr.c ../common/ns_glue.c \
        	../common/ns_udp.c ../common/ns_config.c ../common/ns_update.c 
NMDOBJS=   	tmp_version.o db_dump.o db_load.o db_lookup.o db_save.o \
        	db_update.o db_glue.o \
        	ns_parser.o ns_lexer.o ns_parseutil.o \
        	ns_forw.o ns_init.o ns_main.o ns_maint.o \
        	ns_req.o ns_resp.o ns_stats.o ns_ncache.o \
		ns_xfr.o ns_glue.o \
        	ns_udp.o ns_config.o ns_update.o 

SRCS=		$(NMDOBJS:%.o=../common/%.c)
SUBDIRS=
#CLOBBERFILES += $(PROG)

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

LOCFLAGS +=	-DXSTATS -DSUNW_OPTIONS  -DSVR4 -D_SYS_STREAM_H $(SOLCOMPAT) \
		-D_PATH_PIDFILE=\"/etc/named.pid\" -I$(COMPINCL)

LDLIBS +=	-lresolv -lsocket -lnsl -lc
LDLIBS64 +=	-lresolv -lsocket -lnsl -lc

CLEANFILES += $(NMDOBJS)
CFLAGS +=	-I../common $(LOCFLAGS) 
CFLAGS64 +=	-I../common $(LOCFLAGS)

.KEEP_STATE:

# all:: $(PROG) 

# debug install: all $(ROOTUSRSBINPROG) $(SUBDIRS) 

$(NMD): $(NMDOBJS) 
	$(LINK.c) $(NMDOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)


%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

clean:	$(SUBDIRS)
	$(RM) $(NMDOBJS)
	$(RM) ../common/ns_parser.c ../common/ns_parser.h
	$(RM) ns_parser.c tmp_version.c tmp_version.o

#clobber: $(SUBDIRS)
#	$(RM) $(NMDOBJS) $(PROG)

lint: $(SUBDIRS)
	$(LINT.c) $(SRCS)
