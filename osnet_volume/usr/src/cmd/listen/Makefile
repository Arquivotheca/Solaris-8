#
#ident	"@(#)Makefile	1.7	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
PROG= listen nlps_server

DBGOBJ	= doprnt.o
DBGSRC	= doprnt.c
 
LSOBJS  =	listen.o lslog.o lsdbf.o lsdata.o nstoa.o nlsaddr.o
 
NLPSOBJS =	nlps_serv.o lsdbf.o lssmb.o nstoa.o lslog.o lsdata.o

OBJS= $(LSOBJS) $(NLPSOBJS)
LSSRC= $(LSOBJS:%.o=%.c)
NLPSSRC= $(NLPSOBJS:%.o=%.c)
SRCS= $(LSSRC) $(NLPSSRC)

include ../Makefile.cmd

LIBSAFD = $(ROOTLIB)/saf
NETD = $(ROOT)/usr/net
NETNLSD = $(NETD)/nls

# Don"t re-install /usr/lib/saf/ and /usr/net, Targetdirs installs them
#DIRS= $(LIBSAFD) $(NETD) $(NETNLSD)
DIRS=  $(NETNLSD)
ROOTSAFPROG= $(PROG:%=$(LIBSAFD)/%) 

# If machine name and /etc/issue file need to be printed # before the service prompt is printed, then:  CPPFLAGS += -DSYS_NAME
# If debug is needed, then:  CPPFLAGS += -DDEBUG

LDLIBS += -lnsl
LINTFLAGS = -b -x
OWNER= root
GROUP= sys
FILEMODE= 0755

$(LIBSAFD)/% : %
	$(INS.file)

.KEEP_STATE:

all: $(PROG) 

listen:	$(LSOBJS)
	$(LINK.c) $(LSOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

nlps_server:	$(NLPSOBJS)
	$(LINK.c) $(NLPSOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(DIRS) $(ROOTSAFPROG)
	$(RM) $(ROOT)/usr/net/nls/listen
	-$(SYMLINK) ../../lib/saf/listen $(ROOT)/usr/net/nls/listen
	$(RM) $(ROOT)/usr/net/nls/nlps_server
	-$(SYMLINK) ../../lib/saf/nlps_server $(ROOT)/usr/net/nls/nlps_server

$(DIRS):
	$(INS.dir)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(LSSRC)
	$(LINT.c) $(NLPSSRC)

include ../Makefile.targ
