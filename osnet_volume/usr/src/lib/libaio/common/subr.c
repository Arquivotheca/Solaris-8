#ident	"@(#)subr.c	1.7	98/12/22 SMI"

#include <sys/types.h>
#include <sys/reg.h>
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include "libaio.h"

static void
_halt(void)
{
	pause();
}

int _halted = 0;

void
_aiopanic(char *s)
{
	char buf[256];

	_halted = 1;
	sprintf(buf, "AIO PANIC (LWP = %d): %s\n", _lwp_self(), s);
	write(1, buf, strlen(buf));
	_halt();
}

assfail(char *a, char *f, int l)
{
	char buf[256];

	sprintf(buf, "assertion failed: %s, file: %s, line:%d", a, f, l);
	_aiopanic(buf);
	/*NOTREACHED*/
	return (0);
}
