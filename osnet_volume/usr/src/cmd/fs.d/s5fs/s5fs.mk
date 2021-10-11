#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident "@(#)s5fs.mk	1.1	93/02/16 SMI"
#  /usr/src/cmd/lib/fs/s5fs is the directory of all s5 specific commands
#  whose executable reside in $(INSDIR1) and $(INSDIR2).

ROOT =
TESTDIR = .
INSDIR = $(ROOT)/usr/lib/fs/s5fs
INSDIR2= $(ROOT)/etc/fs/s5fs
INS = install
INC = $(ROOT)/usr/include
CFLAGS = -O -I$(INC)
LDFLAGS = -s



#
#  This is to build all the s5fs commands
#
all:
	@for i in *;\
	do\
	    if [ -d $$i -a -f $$i/$$i.mk ]; \
		then \
		cd  $$i;\
		$(MAKE) -f $$i.mk "INC= $(INC)" "MAKE=$(MAKE)" "AS=$(AS)" "CC=$(CC)" "LD=$(LD)"  "INC=$(INC)" "SHLIBS=$(SHLIBS)" "ROOTLIBS=$(ROOTLIBS)" "ROOT=$(ROOT)" all ; \
		cd .. ; \
	    fi;\
	done

install:
	@for i in *;\
	do\
	    if [ -d $$i -a -f $$i/$$i.mk ]; \
		then \
		cd $$i;\
		$(MAKE) -f $$i.mk "MAKE=$(MAKE)" "AS=$(AS)" "CC=$(CC)" "LD=$(LD)"  "INC=$(INC)" "SHLIBS=$(SHLIBS)" "ROOTLIBS=$(ROOTLIBS)" "ROOT=$(ROOT)" "INS=$(INS)" install ; \
		cd .. ; \
		fi;\
	done

clean:
	@for i in *;\
	do\
	    if [ -d $$i -a -f $$i/$$i.mk ]; \
		then \
			cd $$i;\
			$(MAKE) -f $$i.mk clean; \
			cd .. ; \
		fi;\
	done

clobber:
	@for i in *;\
	do\
	    if [ -d $$i -a -f $$i/$$i.mk ]; \
		then \
			cd $$i;\
			$(MAKE) -f $$i.mk clobber; \
			cd .. ; \
		fi;\
	done

