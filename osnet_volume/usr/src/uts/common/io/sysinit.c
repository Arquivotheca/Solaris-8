/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)sysinit.c	1.3	93/09/09 SMI"


/* sysinit loadable module */

#include <sys/types.h>
#include <sys/modctl.h>

extern struct mod_ops mod_miscops;

#define V1      0x38d4419a
#define V1_K1   0x7a5fd043
#define V1_K2   0x65cb612e

static long t[3] = { V1, V1_K1, V1_K2 };

/*
 * Module linkage information for the kernel.
 */
static struct modlmisc modlmisc = {
	&mod_miscops, "sysinit"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};


extern ulong_t  _bdhs34;
extern char    *_hs1107;

#define A       16807
#define M       2147483647
#define Q       127773
#define R       2836

#define x() if ((s=((A*(s%Q))-(R*(s/Q))))<=0) s+=M

int
_init(void)
{
	char *cp;
	char d[10];
	long s, v;
	int i;

	s = t[1];
	x();
	if (t[2] == s) {
		x();
		s %= 1000000000;
	}
	else
		s = 0;

	for (v = s, i = 0; i < 10; i++) {
		d[i] = v % 10;
		v /= 10;
		if (v == 0)
			break;
	}
	cp = _hs1107;
	for ( ; i >= 0; i--)
		*cp++ = d[i] + '0';
	*cp = 0;
	_bdhs34 = (ulong_t)s + (ulong_t)&_bdhs34;

	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
