#
# Copyright (c) 1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"
#
# cmd/krb5/Makefile
#
# Build everything in parallel; use .WAIT for dependencies
.PARALLEL:

SUBDIRS= \
	kwarn	.WAIT	\
	kadmin	\
	kdestroy	\
	kinit	\
	klist

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package
_msg :=		TARGET= _msg

_msg:	$(SUBDIRS)

.KEEP_STATE:


all clean clobber delete install lint catalog package: $(SUBDIRS)

check: $(CHECKHDRS)
	
$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
