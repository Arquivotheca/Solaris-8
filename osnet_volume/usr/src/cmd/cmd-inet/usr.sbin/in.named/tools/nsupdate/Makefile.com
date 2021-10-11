#
#pragma ident   "@(#)Makefile.com 1.1     97/12/03 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/in.named/tools/nsupdate/Makefile.com
#
VER= 		LOCAL-`date +%y%m%d.10/14/00M%S`
HOSTNAMECMD= 	hostname || uname -n
DT=		`date`
SY=		solaris 
DIR=		`pwd` 
YACC=		yacc -d
NSUPD=		nsupdate
NSUINC=		../common
PROG=		$(NSUPD) 
OBJS=		nsupdate.o 

SRCS=		$(OBJS:%.o=../%.c)
SUBDIRS=

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

CLEANFILES += $(OBJS)
CFLAGS +=	-I../common $(LOCFLAGS) 
CFLAGS64 +=	-I../common $(LOCFLAGS)

.KEEP_STATE:

# all:: $(PROG) 

# debug install: all $(ROOTUSRSBINPROG) $(SUBDIRS) 

$(NSUPD): $(OBJS) 
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

clean:	$(SUBDIRS)
	$(RM) $(OBJS)

#clobber: $(SUBDIRS)
#	$(RM) $(OBJS) $(PROG)

lint: $(SUBDIRS)
	$(LINT.c) $(SRCS)
