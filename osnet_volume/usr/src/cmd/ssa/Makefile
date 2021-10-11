#
# Copyright (c) 1993-1996,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.12	99/03/30 SMI"
#
# cmd/ssa/Makefile
#
# Makefile for Sparc Storage Array Administrative Program and Library
#

include ../Makefile.cmd

SUBDIRS= lib adm	# Note: do lib first

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber
lint:=          TARGET= lint
browser:=	TARGET= browser

FILEMODE=       644
DIRMODE=        755
OWNER=          root
GROUP=          bin
FIRMWARED=	$(ROOTLIB)/firmware
SSAFIRMWARED=	$(ROOTLIB)/firmware/ssa
FCODED=         $(ROOTLIB)/firmware/fc_s
SSAFIRMWARE=	$(SSAFIRMWARED)/ssafirmware
FCODE=          $(FCODED)/fc_s_fcode

.KEEP_STATE:

all install clean clobber lint browser: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

install:	$(SSAFIRMWARE) $(FCODE)

$(FIRMWARED):
	$(INS.dir)

$(SSAFIRMWARED) $(FCODED):	$(FIRMWARED)
	$(INS.dir)

$(SSAFIRMWARE):	$(SSAFIRMWARED) ssafirmware
	$(INS.file) ssafirmware	

$(FCODE):	$(FCODED) fc_s_fcode
	$(INS.file) fc_s_fcode


FRC:
