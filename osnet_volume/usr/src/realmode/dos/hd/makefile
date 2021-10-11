#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)makefile	1.6	99/02/10 SMI

install: all doinst

doinst:
	cd mboot
	$(MAKE) $(MAKEFLAGS) install
	cd ..
	cd pboot
	$(MAKE) $(MAKEFLAGS) install
	cd ..

all: 
	cd mboot
	$(MAKE) $(MAKEFLAGS) all
	cd ..
	cd pboot
	$(MAKE) $(MAKEFLAGS) all
	cd ..

clean: 
	cd mboot
	$(MAKE) $(MAKEFLAGS) clean
	cd ..
	cd pboot
	$(MAKE) $(MAKEFLAGS) clean
	cd ..
