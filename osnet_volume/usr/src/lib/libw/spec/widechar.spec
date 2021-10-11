#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)widechar.spec	1.1	99/01/25 SMI"
#
# lib/libw/spec/widechar.spec

function	getws extends libc/spec/widec.spec getws
include		<stdio.h>, <widec.h>, "widec_spec.h"
version		SUNW_0.7
end		

function	fgetws extends libc/spec/widec.spec fgetws
include		<stdio.h>, <widec.h>, "widec_spec.h"
version		SUNW_0.7
end		

function	fgetwc extends libc/spec/i18n.spec fgetwc
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	getwc extends libc/spec/i18n.spec getwc
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	getwchar extends libc/spec/i18n.spec getwchar
include		<wchar.h>
version		SUNW_0.7
end		

function	ungetwc extends libc/spec/i18n.spec ungetwc
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	iswalpha extends libc/spec/i18n.spec iswalpha
version		SUNW_0.7
end		

function	iswupper extends libc/spec/i18n.spec iswupper
version		SUNW_0.7
end		

function	iswlower extends libc/spec/i18n.spec iswlower
version		SUNW_0.7
end		

function	iswdigit extends libc/spec/i18n.spec iswdigit
version		SUNW_0.7
end		

function	iswxdigit extends libc/spec/i18n.spec iswxdigit
version		SUNW_0.7
end		

function	iswalnum extends libc/spec/i18n.spec iswalnum
version		SUNW_0.7
end		

function	iswspace extends libc/spec/i18n.spec iswspace
version		SUNW_0.7
end		

function	iswpunct extends libc/spec/i18n.spec iswpunct
version		SUNW_0.7
end		

function	iswprint extends libc/spec/i18n.spec iswprint
version		SUNW_0.7
end		

function	iswgraph extends libc/spec/i18n.spec iswgraph
version		SUNW_0.7
end		

function	iswcntrl extends libc/spec/i18n.spec iswcntrl
version		SUNW_0.7
end		

function	isphonogram extends libc/spec/i18n.spec isphonogram
version		SUNW_0.7
end		

function	isideogram extends libc/spec/i18n.spec isideogram
version		SUNW_0.7
end		

function	isenglish extends libc/spec/i18n.spec isenglish
version		SUNW_0.7
end		

function	isnumber extends libc/spec/i18n.spec isnumber
version		SUNW_0.7
end		

function	isspecial extends libc/spec/i18n.spec isspecial
version		SUNW_0.7
end		

function	putws extends libc/spec/widec.spec putws
include		<stdio.h>, <widec.h>, "widec_spec.h"
version		SUNW_0.7
end		

function	fputwc  extends libc/spec/i18n.spec
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	fputws  extends libc/spec/i18n.spec
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	putwc  extends libc/spec/i18n.spec
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	putwchar extends libc/spec/i18n.spec putwchar
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	iswctype extends libc/spec/i18n.spec iswctype
include		<wchar.h>
version		SUNW_0.8
end		

function	towlower extends libc/spec/i18n.spec towlower
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	towupper extends libc/spec/i18n.spec towupper
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcscoll extends libc/spec/i18n.spec wcscoll
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wscoll extends libc/spec/i18n.spec wscoll
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcsftime extends libc/spec/i18n.spec wcsftime
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wcstod extends libc/spec/i18n.spec wcstod
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wstod extends libc/spec/i18n.spec wstod
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcstol extends libc/spec/i18n.spec wcstol
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wstol extends libc/spec/i18n.spec wstol
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcstoul extends libc/spec/i18n.spec wcstoul
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wcscat extends libc/spec/i18n.spec wcscat
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wscat extends libc/spec/i18n.spec wscat
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcsncat extends libc/spec/i18n.spec wcsncat
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wsncat extends libc/spec/i18n.spec wsncat
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcscmp extends libc/spec/i18n.spec wcscmp
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wscmp extends libc/spec/i18n.spec wscmp
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcsncmp extends libc/spec/i18n.spec wcsncmp
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wsncmp extends libc/spec/i18n.spec wsncmp
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcscpy extends libc/spec/i18n.spec wcscpy
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wscpy extends libc/spec/i18n.spec wscpy
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcsncpy extends libc/spec/i18n.spec wcsncpy
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wsncpy extends libc/spec/i18n.spec wsncpy
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcslen extends libc/spec/i18n.spec wcslen
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wslen extends libc/spec/i18n.spec wslen
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcwidth extends libc/spec/i18n.spec wcwidth
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wcswidth extends libc/spec/i18n.spec wcswidth
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wcschr extends libc/spec/i18n.spec wcschr
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wschr extends libc/spec/i18n.spec wschr
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcsrchr extends libc/spec/i18n.spec wcsrchr
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wsrchr extends libc/spec/i18n.spec wsrchr
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcspbrk extends libc/spec/i18n.spec wcspbrk
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wspbrk extends libc/spec/i18n.spec wspbrk
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wcsxfrm extends libc/spec/i18n.spec wcsxfrm
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wsxfrm extends libc/spec/i18n.spec wsxfrm
include		<stdio.h>, <wchar.h>
version		SUNW_0.7
end		

function	wctype extends libc/spec/i18n.spec wctype
include		<stdio.h>, <wchar.h>
version		SUNW_0.8
end		

function	wsprintf extends libc/spec/widec.spec wsprintf
version		SUNW_0.7
end		

function	wsscanf extends libc/spec/widec.spec wsscanf
version		SUNW_0.7
end		

function	wscasecmp extends libc/spec/widec.spec wscasecmp
include		<widec.h>, "widec_spec.h"
version		SUNW_0.7
end		

function	wsncasecmp extends libc/spec/widec.spec wsncasecmp
include		<widec.h>, "widec_spec.h"
version		SUNW_0.7
end		

function	wsdup extends libc/spec/widec.spec wsdup
include		<widec.h>, "widec_spec.h"
version		SUNW_0.7
end		

function	wscol extends libc/spec/widec.spec wscol
include		<widec.h>, "widec_spec.h"
version		SUNW_0.7
end		

function	strtows extends libc/spec/sys.spec
version		SUNW_0.8
end		

function	watoll extends libc/spec/i18n.spec
version		SUNW_0.7
end		

function	wcscspn extends libc/spec/missing.spec
version		SUNW_0.8
end		

function	wcsspn extends libc/spec/missing.spec
version		SUNW_0.8
end		

function	wcstok extends libc/spec/missing.spec
version		SUNW_0.8
end		

function	wcswcs extends libc/spec/missing.spec
version		SUNW_0.8
end		

function	wscspn extends libc/spec/missing.spec
version		SUNW_0.7
end		

function	wsspn extends libc/spec/missing.spec
version		SUNW_0.7
end		

function	wstok extends libc/spec/missing.spec
version		SUNW_0.7
end		

function	wstoll extends libc/spec/sys.spec
version		SUNW_0.7
end		

function	wstostr extends libc/spec/sys.spec
version		SUNW_0.8
end		

