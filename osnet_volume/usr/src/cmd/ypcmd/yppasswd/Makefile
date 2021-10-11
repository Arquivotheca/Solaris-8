#
#ident	"@(#)Makefile	1.10	99/03/30 SMI"
#
# Copyright (c) 1989,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ypcmd/yppasswd/Makefile

NETYPPROG =	rpc.yppasswdd # pwconv passmgmt
PROG =		$(NETYPPROG)

include ../../Makefile.cmd

#installed directories
RPCSVC=		$(ROOT)/usr/include/rpcsvc
NETSVC =	$(ROOTLIB)/netsvc
NETYP =		$(NETSVC)/yp
ROOTDIRS =	$(NETSVC) $(NETYP)

# include library definitions
#LDLIBS +=	-lrpcsvc -lnsl -lcrypt -lintl -lgen
LDLIBS +=	-lnsl -ldl

# This file is now in the $(SRC)/head/rpcsvc directory.
#HDRFILE= yppasswd.h
#IHDRFILE= $(HDRFILE:%=$(RPCSVC)/%)

INETYPPROG=		$(NETYPPROG:%=$(NETYP)/%)

COMMONOBJ =		yppasswdxdr.o yplckpwdf.o
RPCYPPASSWDDOBJ =	yppasswdd.o changepasswd.o
#YPPWCONVOBJ = 		pwconv.o
#YPPASSMGMTOBJ =		passmgmt.o

OBJS =			$(RPCYPPASSWDDOBJ) \
			$(COMMONOBJ)
			# $(YPPWCONVOBJ) $(YPPASSMGMTOBJ)

SRCS =			$(OBJS:%.o=%.c)

#conditional assignments
$(INETSVC)   := GROUP=bin
$(INETSVC)   := FILEMODE=555

#install rules

.KEEP_STATE:

all: $(PROG)

rpc.yppasswdd: $(RPCYPPASSWDDOBJ) $(COMMONOBJ) 
	$(LINK.c) -o $@ $(RPCYPPASSWDDOBJ) $(COMMONOBJ) $(LDLIBS)
	$(POST_PROCESS)

#pwconv: $(YPPWCONVOBJ) $(COMMONOBJ) 
#	$(LINK.c) -o $@ $(YPPWCONVOBJ) $(COMMONOBJ) $(LDLIBS)
#	$(POST_PROCESS)

#passmgmt: $(YPPASSMGMTOBJ) $(COMMONOBJ) 
#	$(LINK.c) -o $@ $(YPPASSMGMTOBJ) $(COMMONOBJ) $(LDLIBS)
#	$(POST_PROCESS)

install: all $(ROOTDIRS) $(IBINPROG) $(INETYPPROG)

$(ROOTDIRS):
	$(INS.dir)

$(NETYP)/%: %
	$(INS.file)

clean:
	$(RM) $(OBJS)

lint:
	${LINT.c} ${SRCS}

cstyle:
	${CSTYLE} ${SRCS}

# include library targets
include ../../Makefile.targ
