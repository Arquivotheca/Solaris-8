#
# Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
#
# @(#)makefile	1.6	96/04/17 SMI

install:
	cd loader
	$(MAKE) $(MAKEFLAGS) install
	cd ..
	cd gluecode
	$(MAKE) $(MAKEFLAGS) install
	cd ..

all:
	cd loader
	$(MAKE) $(MAKEFLAGS) all
	cd ..
	cd gluecode
	$(MAKE) $(MAKEFLAGS) all
	cd ..

clean:
	cd loader
	$(MAKE) $(MAKEFLAGS) clean
	cd ..
	cd gluecode
	$(MAKE) $(MAKEFLAGS) clean
	cd ..
