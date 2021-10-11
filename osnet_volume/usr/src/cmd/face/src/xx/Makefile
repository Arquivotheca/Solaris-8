#
#ident	"@(#)Makefile	1.7	95/01/02 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# /usr/src/cmd/face/src/xx/Makefile

include		../../../Makefile.cmd

# include common face definitions and rules
include		../Makefile.com

PROG=		slash facesuspend
SHFILES=	chksys unix
SRCs=		slash.c suspend.c

TEXTS=		initial.txt cmdfile.txt

SUBDIRS=	../proc ../../HELP ../../OBJECTS

ROOTFILES=	$(PROG) $(SHFILES)

ROOTCMDS=	$(ROOTFILES:%=$(ROOTVMBIN)/%)
ROOTTEXTS=	$(TEXTS:%.txt=$(ROOTVMBIN)/%)

# local libraries
LOCALLIB = ../proc/libproc.a
facesuspend := LDLIBS += $(LOCALLIB)

CPPFLAGS = 	-I../filecab/inc $(CPPFLAGS.master)

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint


$(ROOTCMDS):=		FILEMODE=	755
$(ROOTTEXTCMDS):=	FILEMODE=	644

.KEEP_STATE:

all: $(SUBDIRS) .WAIT $(PROG) $(SHFILES) $(TEXTS)

facesuspend: suspend.c $(LOCALLIB)
	$(LINK.c) -o $@ suspend.c $(LDLIBS)

install: all .WAIT $(ROOTCMDS) $(ROOTTEXTS) $(SUBDIRS)


$(ROOTVMBIN)/% : %.txt
	$(INS.rename)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

clobber clean: $(SUBDIRS)

lint: lint_SRCS

FRC:

include ../../../Makefile.targ
