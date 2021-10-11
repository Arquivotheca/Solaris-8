#
#pragma ident   "@(#)Makefile.com 1.6     98/09/23 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/in.named/tools/nslookup/Makefile.com
#
HELPFILELOC=	../nslookup.help
HELPFILENM=	nslookup.help
YACC=		yacc -d
NSLKUP=		nslookup
NSLINC=		./common
PROG=		$(NSLKUP) 
OBJS=		main.o getinfo.o debug.o send.o skip.o list.o subr.o \
		commands.o

SRCS=		$(OBJS:%.o=./common/%.c)

debug :=	CPPFLAGS += $(DEBUG)
debug :=	COPTFLAG = -g
debug :=	CCOPTFLAG = -g

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber

SOLCOMPAT = -Dgethostbyname=res_gethostbyname \
	-Dgethostbyaddr=res_gethostbyaddr -Dgetnetbyname=res_getnetbyname \
	-Dgetnetbyaddr=res_getnetbyaddr -Dsethostent=res_sethostent \
	-Dendhostent=res_endhostent -Dgethostent=res_gethostent \
	-Dsetnetent=res_setnetent -Dendnetent=res_endnetent \
	-Dgetnetent=res_getnetent

COMPDIR   = ../../compat
COMPINCL  = $(SRC)/lib/libresolv2/include

LOCFLAGS +=	-DXSTATS -D_PATH_HELPFILE=\"/usr/lib/$(HELPFILENM)\" \
		-DSUNW_OPTIONS  -DSVR4 -D_SYS_STREAM_H -I$(COMPINCL)

LDLIBS +=	-ll -lresolv -lsocket -lnsl -lc 
LDLIBS64 +=	-ll -lresolv -lsocket -lnsl -lc

CLEANFILES += $(OBJS) ./common/commands.c
CLOBBERFILES += $(OBJS) ./common/commands.c
CFLAGS +=	-I./common $(LOCFLAGS) 
CFLAGS64 +=	-I./common $(LOCFLAGS)

.KEEP_STATE:

%.o:	./common/%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

ROOTLIBHELP= 	$(ROOTLIB)/$(HELPFILENM)

$(ROOTLIBHELP):=	FILEMODE = 444


clean:	$(SUBDIRS)
	$(RM) $(OBJS)

