#
# ident	"@(#)Makefile	1.6	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/osadmin/Makefile
#

include ../../Makefile.master

# Not all the subdirs are built anymore, as sysadm has been removed from SunOS5,
# and most of these were only used by sysadm

#SUBDIRS = csub cmain shell
SUBDIRS = shell

all	:=	TARGET = all
install :=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

.KEEP_STATE:

all: $(SUBDIRS)

install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
