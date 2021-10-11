#!/bin/sh -x
#
# Copyright (c) 1997, by Sun Microsystems, Inc
# All Rights Reserved.
#
# @(#)filter.sh	1.3	97/03/25 SMI
# filter out certain lint errors that we can't fix so that we can make
# some since out of the output.
#

Dir=`pwd`/
grep -v '/b/ws0/mcneal/dos.inc/malloc.h' $1 | \
	grep -v /b/ws0/mcneal/dos.inc/ctype.h | \
	grep -v /b/ws0/mcneal/dos.inc/stdio.h | \
	grep -v /b/ws0/mcneal/dos.inc/stdlib.h | \
	grep -v /b/ws0/mcneal/dos.inc/string.h | \
	grep -v /b/ws0/mcneal/dos.inc/dos.h | \
	grep -v /b/ws0/mcneal/dos.inc/io.h | \
	grep -v /b/ws0/mcneal/dos.inc/conio.h | \
	grep -v /b/ws0/mcneal/dos.inc/time.h | \
	grep -v /b/ws0/mcneal/dos.inc/direct.h | \
	grep -v 'never used or defined: rel_res' | \
	grep -v 'never used or defined: set_res' | \
	grep -v 'never used or defined: mem_adj' | \
	grep -v 'never used or defined: get_res' | \
	grep -v 'never used or defined: get_prop' | \
	grep -v 'never used or defined: set_prop' | \
	grep -v 'never used or defined: node_op' | \
	grep -v 'warning: constant promoted to unsigned long' | \
	grep -v 'returns value which is always ignored: printf' | \
	grep -v 'returns value which is always ignored: printf_tty' | \
	grep -v 'returns value which is always ignored: iprintf_tty' | \
	grep -v 'warning: pointer cast may result in improper alignment' | \
	grep -v 'putc in /b/ws0/mcneal/dos.inc/stdio.h' | \
	sed -e 's;"'$Dir'\(.*\)'$Dir'\(.*\);\1\2;'
