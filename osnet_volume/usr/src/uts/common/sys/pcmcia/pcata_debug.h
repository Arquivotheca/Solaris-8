/*
 * Copyright (c) 1984-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * debug printf code that does direct to console printf and buffering
 */

#ifndef _PCATA_DEBUG_H
#define	_PCATA_DEBUG_H

#pragma ident	"@(#)pcata_debug.h	1.5	99/03/11 SMI"


#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL)
#include <sys/varargs.h>
#else
#include <varargs.h>
#endif

#undef printf
#undef cmn_err
#ifndef DEBUG_NAME
#define	DEBUG_NAME XXX
#endif
#define	__fmap(x, y)	x##y
#define	__fname(prefix, rest)	__fmap(prefix, rest)

char __fname(DEBUG_NAME, _buff)[4096];
char *__fname(DEBUG_NAME, _buffptr);
void
xxx_prints(char *s)
{
	if (!s)
		return;		/* sanity check for s == 0 */
	while (*s)
		cnputc (*s++, 0);
}

void
__fname(DEBUG_NAME, _printf)(char *fmt, ...)
{
	va_list ap;
	char fbuf[1024];
	va_start(ap, fmt);
	(void) vsprintf(fbuf, fmt, ap);
	if (__fname(DEBUG_NAME, _buffptr) == NULL ||
	    strlen(fbuf) >= (4096 -
	    (__fname(DEBUG_NAME, _buffptr) - __fname(DEBUG_NAME, _buff))))
	    __fname(DEBUG_NAME, _buffptr) = __fname(DEBUG_NAME, _buff);
	(void) strcpy(__fname(DEBUG_NAME, _buffptr), fbuf);
	__fname(DEBUG_NAME, _buffptr) += strlen(fbuf);
	xxx_prints(fbuf);
	cmn_err(CE_CONT, "!%s", fbuf);
	va_end(ap);
}

void
__fname(DEBUG_NAME, _cmn_err)(int tag, char *fmt, ...)
{
	va_list ap;
	char buff[1024];
	va_start(ap, fmt);
	switch (tag) {
	case CE_NOTE:
	case CE_CONT:
		(void) vsprintf(buff, fmt, ap);
		xxx_prints(buff);
		cmn_err(tag, "!%s", buff);
		break;
	}
	va_end(ap);
}

#ifdef	__cplusplus
}
#endif

#endif	/* _PCATA_DEBUG_H */
