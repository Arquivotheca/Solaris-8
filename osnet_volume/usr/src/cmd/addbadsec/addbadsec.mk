#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	Portions Copyright (c) 1988, Sun Microsystems, Inc.
# 	All Rights Reserved.

#ident	"@(#)addbadsec.mk	1.2	89/11/21 SMI"	/* SVr4.0 1.3	*/

#     Makefile for addbadsec 

ROOT =

DIR = $(ROOT)/usr/bin

INC = $(ROOT)/usr/include

INS = install

LDFLAGS = -s $(SHLIBS) 

CFLAGS = -O -I$(INC)

STRIP = strip

SIZE = size

#top#

MAKEFILE = addbadsec.mk

MAINS = addbadsec 

OBJECTS =  addbadsec.o ix_altsctr.o

SOURCES =  addbadsec.c ix_altsctr.c

ALL:          $(MAINS)

$(MAINS):	addbadsec.o ix_altsctr.o
	$(CC) $(CFLAGS) -o addbadsec addbadsec.o ix_altsctr.o $(LDFLAGS)
	
clean:
	rm -f $(OBJECTS)

clobber:
	rm -f $(OBJECTS) $(MAINS)

newmakefile:
	makefile -m -f $(MAKEFILE)  -s INC $(INC)
#bottom#

all :	ALL

install:	ALL
	$(INS) -f $(DIR) -m 00555 -u bin -g bin addbadsec

size: ALL
	$(SIZE) $(MAINS)

strip: ALL
	$(STRIP) $(MAINS)

#     These targets are useful but optional

partslist:
	@echo $(MAKEFILE) $(SOURCES) $(LOCALINCS)  |  tr ' ' '\012'  |  sort

productdir:
	@echo $(DIR) | tr ' ' '\012' | sort

product:
	@echo $(MAINS)  |  tr ' ' '\012'  | \
	sed 's;^;$(DIR)/;'

srcaudit:
	@fileaudit $(MAKEFILE) $(LOCALINCS) $(SOURCES) -o $(OBJECTS) $(MAINS)

