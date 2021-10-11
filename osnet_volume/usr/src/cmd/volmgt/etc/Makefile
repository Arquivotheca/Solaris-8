# Copyright 1992, Sun Microsystems
#
# Makefile for volume management files that go in /etc
#
# "@(#)Makefile	1.6	93/04/12 SMI"
#
# cmd/volmgt/etc/Makefile

include ../../Makefile.cmd

ETCFILES=	vold.conf rmmount.conf

ROOTETCFILES=	${ETCFILES:%=${ROOTETC}/%}

#
# For some reason I have to set these modes explicitly otherwise
# they end up executable.
#
${ROOTETC}/vold.conf := FILEMODE = 0444
${ROOTETC}/rmmount.conf := FILEMODE = 0444
${ROOTETC}/vold.conf := OWNER = root
${ROOTETC}/rmmount.conf := OWNER = root

.KEEP_STATE:

all: ${ETCFILES} 

lint:

cstyle:

install: all ${ROOTETCFILES} 

clean:     

clobber: clean
