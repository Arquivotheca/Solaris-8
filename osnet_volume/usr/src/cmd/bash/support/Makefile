#ident	"@(#)Makefile	1.1	99/10/08 SMI"
#
# Generated automatically from Makefile.in by configure.
#
# Simple Makefile for the support programs.
#
# documentation support: man2html
# testing support: printenv recho zecho
#
# bashbug lives here but is created by the top-level makefile
#
# Currently only man2html is built
#

#
# Boilerplate
#
topdir = ..
srcdir = .
VPATH = .:.
BUILD_DIR = ..

include ../../Makefile.cmd

RM	= rm -f
SHELL	= /bin/sh

#
# Compiler options:
#
PROFILE_FLAGS = 
LOCAL_CFLAGS = 
CFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
DEFS = -DHAVE_CONFIG_H
LOCAL_DEFS = -DSHELL
LDFLAGS =   $(LOCAL_LDFLAGS) $(CFLAGS)
LOCAL_LDFLAGS = 
LDLIBS += -lsocket -lnsl -ldl  

INCLUDES = -I${BUILD_DIR} -I${topdir}

#
CFLAGS += ${PROFILE_FLAGS} $(DEFS) $(LOCAL_DEFS) $(SYSTEM_FLAGS) \
	  ${INCLUDES} $(LOCAL_CFLAGS)

SRC1 =	man2html.c
OBJ1 =	man2html.o

all: man2html

man2html: $(OBJ1)
	$(LINK.c) $(CFLAGS) $(OBJ1) -o $@ ${LDLIBS}

clean:
	rm man2html

man2html.o: man2html.c

include ../../Makefile.targ
