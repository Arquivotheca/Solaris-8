#
#ident @(#)Makefile	1.46	99/04/09 SMI
#
# Copyright (c) 1996,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#	usr/src/cmd/devconfig
#

PROG=		devconfig

include ../Makefile.cmd

SRCS=		devcfg_msgs.c cui.c
OBJS=		$(SRCS:.c=.o)

MKMSGS =	mkmsgs/mkmsgs
LIBSRC =	libsrc

SUBDIR_MKMSGS=	mkmsgs
SUBDIR_LIBSRC=	libsrc
SUBDIR_SUPPORT=	support

SUBDIRS= $(SUBDIR_MKMSGS) $(SUBDIR_LIBSRC) $(SUBDIR_SUPPORT)

CFLAGS +=	-I${LIBSRC}/cui -I${LIBSRC}/dvc

## passed in from upper level makefile for -g or -O optimization 
OPTFLAG		= 
MCSSTRING       = $(RELEASE_CM)

LD=$(CCC)

INCLUDES        += -I$(INCDESTDIR)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
_msg	:=	TARGET = _msg
lint	:=	TARGET = lint

###
### Instructions:
###
### Fill in HDRS, SRCS, LIBS, and PROGRAM name to build the program.
### LIBS should use the -l<libname> form rather than explicit references
### to .a files, and allow the linker to find library files via the
### -L flags specified above.
###
PUBHDRS		=
PRIVHDRS	= 

HDRS		= ${PRIVHDRS} ${PUBHDRS}


LDLIB +=				\
		-lform			\
		-lmenu			\
		-lpanel			\
		-lcurses		\
		-lkvm			\
		-lgen			\
		-lelf			\
		-ldl			\
		-lc			\
		-Bstatic

LDLIBS +=				\
		-L$(LIBSRC)/dvc		\
		-L$(LIBSRC)/cui		\
		-L$(LIBSRC)/etibugs	\
		-ldvc			\
		-lcui			\
		-letibugs		\
		-lform			\
		-lmenu			\
		-lpanel			\
		-lcurses		\
		-lkvm			\
		-lgen			\
		-lelf			\
		-ldl			\
		-lc			\
		-Bstatic

FILEMODE=0755
OWNER= root
GROUP= bin

.KEEP_STATE:

all:	${SUBDIR_MKMSGS} .WAIT	\
	${SUBDIR_LIBSRC} .WAIT	\
	${SUBDIR_SUPPORT} .WAIT	\
	${PROG}

install: all $(SUBDIRS) $(ROOTUSRSBINPROG)

clean: $(SUBDIRS)
	$(RM) $(OBJS) devcfg_msgs.c


# Degree of comment processing determined by assignments made to
# INTERNAL_RELEASE_BUILD and/or RELEASE_BUILD environment variables.
# See Makefile.master.

$(PROG): $(OBJS)
	$(LINK.cc) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

$(SUBDIRS): FRC
	cd $@ ; pwd; $(MAKE) $(TARGET)

lint: lint_SRCS

include ../Makefile.targ

$(TEXT_DOMAIN).po devcfg_msgs.c:	msg_master
	$(MKMSGS) -d $(TEXT_DOMAIN) < msg_master

## DO NOT DELETE THIS LINE -- make depend depends on it.

FRC:
