#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident "@(#)fsck.mk	1.1	93/02/16 SMI"

TESTDIR = .
INSDIR1 = $(ROOT)/usr/lib/fs/s5fs
INSDIR2 = $(ROOT)/etc/fs/s5fs
INS = install
CFLAGS = -O -I$(INC)
LDFLAGS =
INC = $(ROOT)/usr/include
OBJS=   fsck.o fsckinit.o l3.o

all: fsck

fsck: $(OBJS) $(S5OBJS)
	$(CC) -I$(INCSYS) $(CFLAGS) $(LDFLAGS) -o fsck $(OBJS) $(ROOTLIBS)

l3.o: l3.c
	$(CC) -I$(INCSYS) $(CFLAGS) $(LDFLAGS) -c l3.c 

$(OBJS): 

%.o:	%.c
	$(CC) -I$(INCSYS) $(CFLAGS) $(LDFLAGS) -c $*.c
	
install: fsck
	@if [ ! -d $(ROOT)/usr/lib/fs ]; \
		then \
		mkdir $(ROOT)/usr/lib/fs; \
		fi;
	@if [ ! -d $(INSDIR1) ]; \
		then \
		mkdir $(INSDIR1); \
		fi;
	@if [ ! -d $(ROOT)/etc/fs ]; \
		then \
		mkdir $(ROOT)/etc/fs; \
		fi;
	@if [ ! -d $(INSDIR2) ]; \
		then \
		mkdir $(INSDIR2); \
		fi;
	$(INS) -f $(INSDIR1) -m 0555 -u bin -g bin $(TESTDIR)/fsck
	$(INS) -f $(INSDIR2) -m 0555 -u bin -g bin $(TESTDIR)/fsck


clean:     
	-rm -f $(OBJS) $(UFSOBJS) main.o
	
clobber: clean
	rm -f fsck

