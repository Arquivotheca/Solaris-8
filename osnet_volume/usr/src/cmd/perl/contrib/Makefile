#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/16 SMI"
#
# cmd/perl5/contrib/Makefile
#
#
include ../../Makefile.cmd
include ../Makefile.locns

ENV_LLPATH=	LD_LIBRARY_PATH=`pwd`/../distrib
ENV_PLAT=	PERL_PLAT=$(PERL_PLAT)

CP= /usr/bin/cp
GREP=/usr/bin/grep
INSTALL= /usr/sbin/install
MKDIR= /usr/bin/mkdir
MKFILE= /usr/sbin/mkfile
SED= /usr/bin/sed
SH= /usr/bin/sh

BUILD_COSMETICS= > /dev/null 2>&1
DYNAMIC_EXT_DIRS= \
	Sun/Solaris/Utils \
	Sun/Solaris/Kstat 

NON_XS_EXTS= 

TARGET = all
all :=		TARGET = all
install :=	TARGET = install
clobber :=	TARGET = clobber
clean :=	TARGET = clean
test :=		TARGET = test

all: extensions

#
# Installation performed by modified installperl script in ../distrib.
#
install: all

#
# Phase I.  extensions with binary components
#
extensions: fake_cflags .WAIT $(DYNAMIC_EXT_DIRS)

$(DYNAMIC_EXT_DIRS): FRC
	@TOP=`pwd`/../distrib export TOP; \
	$(ENV_LLPATH) export LD_LIBRARY_PATH; \
	$(ENV_PLAT) export PERL_PLAT; \
	PERL5LIB=$$TOP/ext/DynaLoader export PERL5LIB; \
	. $$TOP/config.sh; \
	cd $@; pwd; \
	$$TOP/miniperl -I$$TOP/lib Makefile.PL INST_ARCHLIB=$$TOP/lib \
	    INST_LIB=$$TOP/lib PERL_SRC=$$TOP $(BUILD_COSMETICS); \
	unset KEEP_STATE VERSION; \
	$(MAKE) LINKTYPE=dynamic all $(BUILD_COSMETICS)

#
# fake_cflags quiets our compilations of the various core extensions, which are
# otherwise upset by our slightly non-standard build of Perl.
#
fake_cflags:
	$(MKFILE) 1b cflags

FRC:

clobber_extensions:
	@TOP=`pwd` export TOP; \
	unset KEEP_STATE VERSION; \
	for dir in $(DYNAMIC_EXT_DIRS); do \
        	cd $$TOP/$$dir; pwd; \
		if [ -f Makefile ]; then \
		    $(MAKE) realclean $(BUILD_COSMETICS); \
		fi; \
	done

clean:

clobber: clean clobber_extensions 

include ../../Makefile.targ
