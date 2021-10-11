#
# Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
#
# @(#)makefile	1.6	95/08/31 SMI

install all: tinyobj\tiny.lib smallobj\small.lib

tinyobj\tiny.lib:
	cd tinyobj
	$(MAKE) $(MAKEFLAGS) all
	cd ..

smallobj\small.lib:
	cd smallobj
	$(MAKE) $(MAKEFLAGS) all
	cd ..

clean:
	cd tinyobj
	$(MAKE) $(MAKEFLAGS) clean
	cd ..
	cd smallobj
	$(MAKE) $(MAKEFLAGS) clean
	cd ..
