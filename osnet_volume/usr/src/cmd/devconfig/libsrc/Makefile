# Copyright 1995 Sun Microsystems, Inc.
# All Rights Reserved.
#
#ident "@(#)Makefile	1.21	95/03/23 SMI"
#
#

include $(SRC)/Makefile.master

# passed in from upper level makefile for -g or -O optimization 
LEVEL="=="
OPTFLAG		= -O
CPOPTFLAG	= -O
MCSSTRING       = $(RELEASE_CM)\(\#\)Solaris\ 2.1\ FCS\ November\ 1992

CFLAGS		= ${OPTFLAG}

###
### Instructions:
###
### Fill in SUBDIRS for the subdirectories to descend into for the build.
###
SUBDIRS		= cui etibugs dvc ttylib
DSUBDIRS	= cui .WAIT etibugs .WAIT dvc .WAIT ttylib

all:=		TARGET = all
install:=       TARGET = install
clean:=		TARGET = clean
clobber:=       TARGET = clobber
cstyle:=	TARGET = cstyle
depend:=	TARGET = depend
lint:=		TARGET = lint
lintlib:=	TARGET = lintlib
msgs:=		TARGET = msgs
sccs:=		TARGET = sccs
sclean:=	TARGET = sclean

.KEEP_STATE:

all install clean cstyle depend lintlib lint sccs: ${DSUBDIRS}
	@true

${SUBDIRS}: FRC
	@(cd $@; pwd; $(MAKE) 'LEVEL=${LEVEL}==' 'OPTFLAG=${OPTFLAG}' \
		'CPOPTFLAG=${CPOPTFLAG}' ${TARGET})

clobber: ${DSUBDIRS} rm_msgs
	    $(RM) -r $(LIBDESTDIR) $(INCDESTDIR)

sclean: clean ${SUBDIRS}
	@true

msgs: rm_msgs .WAIT messages .WAIT ${SUBDIRS}
	@mv messages/* ../../messages
	@-rm -rf messages

rm_msgs: FRC
	@-rm -rf messages

messages: FRC
	@[ -d messages ] || (rm -f messages ;mkdir -p messages)

FRC:

.WAIT:
