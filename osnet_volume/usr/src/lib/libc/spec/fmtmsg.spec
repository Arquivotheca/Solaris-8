#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)fmtmsg.spec	1.2	99/05/04 SMI"
#
# lib/libc/spec/fmtmsg.spec

function	addsev
declaration	int addsev(int int_val, const char *string)
version		SUNW_0.8
exception	$return == -1
end

function	addseverity
declaration	int addseverity(int severity, const char *string)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == MM_NOTOK
end

function	_addseverity
weak		addseverity
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fmtmsg
include		<fmtmsg.h>
declaration	int fmtmsg(long classification, const char *label, \
			int severity, const char *text, const char *action, \
			const char *tag)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return== MM_NOTOK || $return == MM_NOMSG || $return == MM_NOCON
end

function	_fmtmsg
weak		fmtmsg 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end
