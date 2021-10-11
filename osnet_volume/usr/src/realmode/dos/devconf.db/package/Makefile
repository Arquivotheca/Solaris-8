#
#ident  "@(#)Makefile	1.4    98/03/26 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# Solaris boot/configuration assistant package makefile
#
%: %.ksh
	$(RM) $@
	cp $< $@
	chmod +x $@

all: database bld_awk_pkginfo pkgkit doit

clean clobber:
	$(RM) awk_pkginfo bld_awk_pkginfo pkgkit

doit:
	./pkgkit -u
	echo 'Run pkgkit -p to build package'
