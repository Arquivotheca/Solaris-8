#
#ident	"@(#)Makefile	1.7	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= _sactab _pmtab _sysconfig iu.ap ttydefs

SRCS= $(PROG:%=%.sh)

include ../Makefile.cmd

DEFPM=   zsmon

ETCSAFD= $(ROOTETC)/saf
ETCPMD=  $(ETCSAFD)/$(DEFPM)
VARPMD=  $(ROOT)/var/saf/$(DEFPM)

# Don't re-install /etc/saf which is installed by Targetdirs
#DIRS= $(ETCSAFD) $(ETCPMD) $(VARPMD)
DIRS= $(ETCPMD) $(VARPMD)

ETCSAF= _sactab _sysconfig
ETCF=   ttydefs iu.ap
ETCPM=  _pmtab
PMLOG=  log

ROOTETCSAF= $(ETCSAF:%=$(ETCSAFD)/%)
ROOTETCF= $(ETCF:%=$(ROOTETC)/%)
ROOTETCPM = $(ETCPM:%=$(ETCPMD)/%) 
ROOTVARPM = $(PMLOG:%=$(VARPMD)/%) 

FILEMODE= 0644
OWNER= root
GROUP= sys

$(ETCSAFD)/% : %
	$(INS.file)

$(VARPMD)/% : %
	$(INS.file)

$(ETCPMD)/% : %
	$(INS.file)

.KEEP_STATE:

all: $(PROG) $(PMLOG) 

_sysconfig: _sysconf.sh
	$(SH) _sysconf.sh

_sactab _pmtab iu.ap ttydefs:
	$(SH) $@.sh

$(PMLOG):
	$(TOUCH) $@

install: all $(DIRS) $(ROOTETCSAF) $(ROOTETCF) $(ROOTETCPM) $(ROOTVARPM)

$(DIRS):
	$(INS.dir)

clean:

lint:

include ../Makefile.targ
