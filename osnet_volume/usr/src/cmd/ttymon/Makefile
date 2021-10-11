#
#ident	"@(#)Makefile	1.19	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= stty ttymon ttyadm sttydefs
XPG4PROG= stty

# the 'stty' objects have to be made in a separate directory
# since only they are built with the -DEUC flag (see below).
STTYOBJ=	sttyobjs/stty.o sttyobjs/sttytable.o sttyobjs/sttyparse.o

XPG4STTYOBJ=	sttyobjs.xpg4/stty.o sttyobjs.xpg4/sttytable.o \
		sttyobjs.xpg4/sttyparse.o

TTYMONOBJ=	ttymon.o tmglobal.o tmhandler.o tmpmtab.o tmttydefs.o \
		tmparse.o tmsig.o tmsac.o tmchild.o tmautobaud.o tmterm.o \
		tmutmp.o tmpeek.o tmlog.o tmlock.o tmutil.o tmexpress.o \
		sttytable.o sttyparse.o ulockf.o

TTYADMOBJ=	ttyadm.o tmutil.o admutil.o 

STTYDEFSOBJ=	sttydefs.o admutil.o tmttydefs.o tmparse.o sttytable.o \
		sttyparse.o 

OBJS= $(STTYOBJ) $(XPG4STTYOBJ) $(TTYMONOBJ) $(TTYADMOBJ) $(STTYDEFSOBJ)
STTYSRC= stty.c sttytable.c sttyparse.c
TTYMONSRC= $(TTYMONOBJ:%.o=%.c)
TTYADMSRC= $(TTYADMOBJ:%.o=%.c)
STTYDEFSSRC= $(STTYDEFSOBJ:%.o=%.c)
SRCS= $(STTYSRC) $(TTYMONSRC) $(TTYADMSRC) $(STTYDEFSSRC)

include ../Makefile.cmd

# If machine name and /etc/issue file need to be printed 
# before the service prompt is printed, then:  CPPFLAGS += -DSYS_NAME
# If debug is needed, then:  CPPFLAGS += -DDEBUG
# fix for 1111333 - turn on SYS_NAME so /etc/issue will be read if it exists
CPPFLAGS += -DSYS_NAME
$(XPG4):= CPPFLAGS += -DXPG4
ttymon ttyadm sttydefs := LDLIBS += -lnsl
$(STTYOBJ) := CPPFLAGS += -DEUC
$(XPG4STTYOBJ) := CPPFLAGS += -DEUC
LINTFLAGS = -b -x
CFLAGS += -v
	
#
# Message catalog
#
POFILES= $(STTYOBJ:sttyobjs/%.o=%.po)
POFILE= ttymon.po


LIBSAFD= $(ROOTLIB)/saf
DIRS= $(LIBSAFD)
LIBSAF= ttymon
USRSBINF= sttydefs ttyadm
BINF= stty
ROOTLIBF= $(LIBSAF:%=$(LIBSAFD)/%)
ROOTUSRSBINF= $(USRSBINF:%=$(ROOTUSRSBIN)/%)
ROOTBINF= $(BINF:%=$(ROOTBIN)/%)
$(ROOTLIBF) :=			FILEMODE =	0555
$(ROOTUSRSBINF) :=		FILEMODE =	0755
$(ROOTUSRSBINF) $(ROOTLIBF) :=	OWNER =		root
$(ROOTUSRSBINF) $(ROOTLIBF) :=	GROUP =		sys

$(LIBSAFD)/% : %
	$(INS.file)

.KEEP_STATE:

all: $(PROG) $(XPG4)

stty:	sttyobjs $(STTYOBJ)
	$(LINK.c) $(STTYOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

stty.xpg4: sttyobjs.xpg4 $(XPG4STTYOBJ)
	$(LINK.c) $(XPG4STTYOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

sttyobjs/%.o:	%.c
	$(COMPILE.c) -o $@ $<

sttyobjs.xpg4/%.o:	%.c
	$(COMPILE.c) -o $@ $<

sttyobjs:
	-@mkdir -p $@

sttyobjs.xpg4:
	-@mkdir -p $@

ttymon:	$(TTYMONOBJ)
	$(LINK.c) $(TTYMONOBJ) -o $@ $(LDLIBS) -lpam -ldl -lsocket
	$(POST_PROCESS)

ttyadm:	$(TTYADMOBJ)
	$(LINK.c) $(TTYADMOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

sttydefs: $(STTYDEFSOBJ)
	$(LINK.c) $(STTYDEFSOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(DIRS) $(ROOTLIBF) $(ROOTUSRSBINF) $(ROOTBINF) $(ROOTXPG4PROG)
	-$(RM) $(ROOTETC)/getty
	-$(SYMLINK) ../usr/lib/saf/ttymon $(ROOTETC)/getty

$(POFILE):      $(POFILES)
	$(RM) $@
	cat $(POFILES)  > $@

$(DIRS):
	$(INS.dir)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(STTYSRC)
	$(LINT.c) $(TTYMONSRC)
	$(LINT.c) $(TTYADMSRC)
	$(LINT.c) $(STTYDEFSSRC)

include ../Makefile.targ
