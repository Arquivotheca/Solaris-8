#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)i18n.spec	1.3	99/05/14 SMI"
#
# lib/libc/spec/i18n.spec

function	bindtextdomain 
include		<libintl.h>, <locale.h>
declaration	char *bindtextdomain(const char *domainname, const char *dirname)
version		SUNW_1.1
end

function	dcgettext 
include		<libintl.h>, <locale.h>
declaration	char *dcgettext(const char *domainname, const char *msgid, \
			int category)
version		SUNW_1.1
end

function	dgettext 
include		<libintl.h>, <locale.h>
declaration	char *dgettext(const char *domainname, const char *msgid)
version		SUNW_1.1
end

function	fgetwc 
include		<stdio.h>, <wchar.h>
declaration	wint_t fgetwc(FILE *stream)
version		SUNW_1.1
errno		EAGAIN EBADF EINTR EIO EOVERFLOW ENOMEM ENXIO EILSEQ
exception	$return == WEOF && errno != 0
end

function	finite 
include		<ieeefp.h>
declaration	int finite(double dsrc)
version		SUNW_0.7
exception	$return == 0
end

function	fnmatch 
include		<fnmatch.h>
declaration	int fnmatch(const char *pattern, const char *string, int flags)
version		SUNW_0.8
exception	$return != 0 &&  $return != FNM_NOMATCH
end

function	fpclass 
include		<ieeefp.h>
declaration	fpclass_t fpclass(double dsrc)
version		SUNW_0.7
exception	$return == 0
end

function	fputwc 
include		<stdio.h>, <wchar.h>
declaration	wint_t fputwc(wint_t wc, FILE *stream)
version		SUNW_1.1
errno		EAGAIN EBADF EFBIG EINTR EIO ENOSPC EPIPE ENOMEM ENXIO EILSEQ
exception	$return == WEOF && errno != 0
end

function	fputws 
include		<stdio.h>, <wchar.h>
declaration	int fputws(const wchar_t *s, FILE *stream)
version		SUNW_1.1
exception	$return == -1
end

function	gettext 
include		<libintl.h>, <locale.h>
declaration	char *gettext(const char *msgid)
version		SUNW_1.1 
end

function	getwc 
include		<stdio.h>, <wchar.h>
declaration	wint_t getwc(FILE *stream)
version		SUNW_1.1 
errno		EAGAIN EBADF EINTR EIO EOVERFLOW ENOMEM ENXIO EILSEQ
exception	$return == WEOF && errno != 0
end

function	getwchar 
include		<wchar.h>
declaration	wint_t getwchar(void)
version		SUNW_1.1 
errno		EAGAIN EBADF EINTR EIO EOVERFLOW ENOMEM ENXIO EILSEQ
exception	$return == WEOF && errno != 0
end

function	isalnum 
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int isalnum(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	isalpha 
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int isalpha(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	isascii 
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int isascii(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	_isascii
weak		isascii
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	iscntrl 
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int iscntrl(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	isdigit 
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int isdigit(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	isenglish 
include		<wchar.h>
declaration	int isenglish(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	isgraph 
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int isgraph(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	isideogram 
include		<wchar.h>
declaration	int isideogram(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	islower 
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int islower(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	isnumber 
include		<wchar.h>
declaration	int isnumber(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	isphonogram 
include		<wchar.h>
declaration	int isphonogram(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	isprint 
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int isprint(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	ispunct 
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int ispunct(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	isspace 
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int isspace(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	isspecial 
include		<wchar.h>
declaration	int isspecial(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	isupper 
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int isupper(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	iswalnum 
include		<wchar.h>
declaration	int iswalnum(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	iswalpha 
include		<wchar.h>
declaration	int iswalpha(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	iswcntrl 
include		<wchar.h>
declaration	int iswcntrl(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	iswctype 
include		<wchar.h>
declaration	int iswctype(wint_t wc, wctype_t charclass)
version		SUNW_1.1 
exception	$return == 0
end

function	iswdigit 
include		<wchar.h>
declaration	int iswdigit(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	iswgraph 
include		<wchar.h>
declaration	int iswgraph(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	iswlower 
include		<wchar.h>
declaration	int iswlower(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	iswprint 
include		<wchar.h>
declaration	int iswprint(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	iswpunct 
include		<wchar.h>
declaration	int iswpunct(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	iswspace 
include		<wchar.h>
declaration	int iswspace(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	iswupper 
include		<wchar.h>
declaration	int iswupper(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	iswxdigit 
include		<wchar.h>
declaration	int iswxdigit(wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	isxdigit 
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int isxdigit(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	localeconv 
include		<locale.h>
declaration	struct lconv *localeconv(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	mblen 
include		<stdlib.h>
declaration	int mblen(const char *s, size_t n)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EILSEQ 
exception	$return == 0
end

function	mbstowcs 
include		<stdlib.h>
declaration	size_t mbstowcs(wchar_t *pwcs, const char *s, size_t n)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == -1
end

function	mbtowc 
include		<stdlib.h>
declaration	int mbtowc(wchar_t	*pwc, const char *s, size_t n)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EILSEQ 
exception	$return == -1
end

function	nl_langinfo
include		<langinfo.h>
declaration	char *nl_langinfo(nl_item item)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_nl_langinfo
weak		nl_langinfo
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	putwc 
include		<stdio.h>, <wchar.h>
declaration	wint_t putwc(wint_t wc, FILE *stream)
version		SUNW_1.1 
errno		EAGAIN EBADF EFBIG EINTR EIO ENOSPC EPIPE ENOMEM ENXIO EILSEQ
exception	$return == WEOF && errno != 0
end

function	putwchar 
include		<wchar.h>
declaration	wint_t putwchar(wint_t wc)
version		SUNW_1.1 
errno		EAGAIN EBADF EFBIG EINTR EIO ENOSPC EPIPE ENOMEM ENXIO EILSEQ
exception	$return == WEOF && errno != 0
end

function	regcomp 
include		<sys/types.h>, <regex.h>
declaration	int regcomp(regex_t *preg, const char *pattern, int cflags)
version		SUNW_0.8 
exception	$return != 0
end

function	regerror 
include		<sys/types.h>, <regex.h>
declaration	size_t regerror(int errcode, const regex_t *preg, \
			char *errbuf, size_t errbuf_size)
version		SUNW_0.8 
exception	$return == 0
end

function	regexec 
include		<sys/types.h>, <regex.h>
declaration	int regexec(const regex_t *preg, const char *string, \
			size_t nmatch, regmatch_t *pmatch, int eflags)
version		SUNW_0.8 
exception	$return == REG_NOMATCH || $return == REG_ENOSYS
end

function	regfree 
include		<sys/types.h>, <regex.h>
declaration	void regfree(regex_t *preg)
version		SUNW_0.8 
end

function	setlocale 
include		<locale.h>
declaration	char *setlocale(int category, const char *locale)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	strcoll 
include		<string.h>
declaration	int strcoll(const char *s1, const char *s2)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	strfmon 
include		<monetary.h>
declaration	ssize_t strfmon(char *s, size_t maxsize, \
			const char *format, ...)
version		SUNW_0.8 
errno		ENOSYS 
exception	$return == -1
end

function	strptime 
include		<time.h>
declaration	char *strptime(const char *buf, const char *format, \
			struct tm *tm)
version		SUNW_0.8 
exception	$return == 0
end

function	strxfrm 
include		<string.h>
declaration	size_t strxfrm(char *s1, const char *s2, size_t n)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == (size_t)-1
end

function	textdomain 
include		<libintl.h>, <locale.h>
declaration	char *textdomain(const char *domainname)
version		SUNW_1.1 
end

function	toascii
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int toascii(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_toascii
weak		toascii
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	tolower
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int tolower(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_tolower
weak		tolower
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	toupper
include		<ctype.h>, "i18n_spec.h", <limits.h>
declaration	int toupper(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_toupper
weak		toupper
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	towlower 
include		<wchar.h>
declaration	wint_t towlower(wint_t c)
version		SUNW_1.1 
end

function	towupper 
include		<wchar.h>
declaration	wint_t towupper(wint_t c)
version		SUNW_1.1 
end

function	ungetwc 
include		<stdio.h>, <wchar.h>
declaration	wint_t ungetwc(wint_t wc, FILE *stream)
version		SUNW_1.1 
errno		EILSEQ 
exception	$return == WEOF
end

function	unordered 
include		<ieeefp.h>
declaration	int unordered(double dsrc1, double dsrc2)
version		SUNW_0.7 
exception	$return == 0
end

function	watoll 
#NOTE: long long breaks adl
include		<wchar.h>
declaration	long long watoll(wchar_t *nptr)
version		SUNW_1.1 
errno		EINVAL ERANGE
exception	errno != 0
end

function	wcscat 
include		<wchar.h>
declaration	wchar_t *wcscat(wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
end

function	wcschr 
include		<wchar.h>
declaration	wchar_t *wcschr(const wchar_t *ws,	wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	wcscmp 
include		<wchar.h>
declaration	int wcscmp(const wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
end

function	wcscoll 
include		<wchar.h>
declaration	int wcscoll(const wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
errno		EINVAL ENOSYS
exception	errno != 0
end

function	wcscpy 
include		<wchar.h>
declaration	wchar_t *wcscpy(wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
end

function	wcsftime 
include		<wchar.h>
declaration	size_t wcsftime(wchar_t *wcs, size_t maxsize, \
			const wchar_t *format, const struct tm *timptr)
version		SUNW_1.1 
exception	$return == 0
end

function	wcslen 
include		<wchar.h>
declaration	size_t wcslen(const wchar_t *ws)
version		SUNW_1.1 
end

function	wcsncat 
include		<wchar.h>
declaration	wchar_t *wcsncat(wchar_t *ws1, const wchar_t *ws2, size_t n)
version		SUNW_1.1 
end

function	wcsncmp 
include		<wchar.h>
declaration	int wcsncmp(const wchar_t *ws1, const wchar_t *ws2, size_t n)
version		SUNW_1.1 
end

function	wcsncpy 
include		<wchar.h>
declaration	wchar_t *wcsncpy(wchar_t *ws1, const wchar_t *ws2, size_t n)
version		SUNW_1.1 
end

function	wcspbrk 
include		<wchar.h>
declaration	wchar_t *wcspbrk(const wchar_t *ws1, const	wchar_t	*ws2)
version		SUNW_1.1 
end

function	wcsrchr 
include		<wchar.h>
declaration	wchar_t *wcsrchr(const wchar_t *ws, wchar_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	wcstod 
include		<wchar.h>
declaration	double wcstod(const wchar_t *nptr,	wchar_t	**endptr)
version		SUNW_1.1 
errno		ERANGE EINVAL
exception	errno != 0
end

function	wcstol 
include		<wchar.h>
declaration	long int wcstol(const wchar_t *nptr, wchar_t **endptr, int base)
version		SUNW_1.1 
errno		EINVAL ERANGE
exception	errno != 0
end

function	wcstombs 
include		<stdlib.h>
declaration	size_t wcstombs(char *s, const wchar_t *pwcs, size_t n)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == -1
end

function	wcstoul 
include		<wchar.h>
declaration	unsigned long int wcstoul(const wchar_t *nptr, \
			wchar_t **endptr, int base)
version		SUNW_1.1 
errno		EINVAL ERANGE
exception	errno != 0
end

function	wcswidth 
include		<wchar.h>
declaration	int wcswidth(const wchar_t *pwcs, size_t n)
version		SUNW_1.1 
exception	$return == -1
end

function	wcsxfrm 
include		<wchar.h>
declaration	size_t wcsxfrm(wchar_t *ws1, const wchar_t *ws2, size_t n)
version		SUNW_1.1 
errno		EINVAL ENOSYS
exception	errno != 0
end

function	wctomb 
include		<stdlib.h>
declaration	int wctomb(char *s, wchar_t wchar)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == -1
end

function	wctype 
include		<wchar.h>
declaration	wctype_t wctype(const char *charclass)
version		SUNW_1.1 
exception	$return == 0
end

function	wcwidth 
include		<wchar.h>
declaration	int wcwidth(wint_t	wc)
version		SUNW_1.1 
exception	$return == -1
end

function	wscat 
include		<wchar.h>
declaration	wchar_t *wscat(wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
end

function	wschr 
include		<wchar.h>
declaration	wchar_t *wschr(const wchar_t *ws, wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	wscmp 
include		<wchar.h>
declaration	int wscmp(const wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
end

function	wscoll 
include		<wchar.h>
declaration	int wscoll(const wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
errno		EINVAL ENOSYS
exception	errno != 0
end

function	wscpy 
include		<wchar.h>
declaration	wchar_t *wscpy(wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
end

function	wslen 
include		<wchar.h>
declaration	size_t wslen(const	wchar_t	*ws)
version		SUNW_1.1 
end

function	wsncat 
include		<wchar.h>
declaration	wchar_t *wsncat(wchar_t *ws1, const wchar_t *ws2, size_t n)
version		SUNW_1.1 
end

function	wsncmp 
include		<wchar.h>
declaration	int wsncmp(const wchar_t *ws1, const wchar_t *ws2, size_t n)
version		SUNW_1.1 
end

function	wsncpy 
include		<wchar.h>
declaration	wchar_t *wsncpy(wchar_t *ws1, const wchar_t *ws2, size_t n)
version		SUNW_1.1 
end

function	wspbrk 
include		<wchar.h>
declaration	wchar_t *wspbrk(const wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
end

function	wsrchr 
include		<wchar.h>
declaration	wchar_t *wsrchr(const wchar_t *ws,	wint_t wc)
version		SUNW_1.1 
exception	$return == 0
end

function	wstod 
include		<wchar.h>
declaration	double wstod(const	wchar_t	*nptr, wchar_t **endptr)
version		SUNW_1.1 
errno		ERANGE EINVAL
exception	errno != 0
end

function	wstol 
include		<wchar.h>
declaration	long int wstol(const wchar_t *nptr, wchar_t **endptr, int base)
version		SUNW_1.1 
errno		EINVAL ERANGE
exception	errno != 0
end

function	wsxfrm 
include		<wchar.h>
declaration	size_t wsxfrm(wchar_t *ws1, const wchar_t *ws2, size_t n)
version		SUNW_1.1 
errno		EINVAL ENOSYS
exception	errno != 0
end
