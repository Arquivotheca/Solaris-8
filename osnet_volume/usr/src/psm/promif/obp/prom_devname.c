/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_devname.c	1.8	95/08/15 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

int
prom_devname_from_pathname(register char *pathname, register char *buffer)
{
	register char *p;

	if ((pathname == (char *)0) || (*pathname == (char)0))
		return (-1);

	p = prom_strrchr(pathname, '/');
	if (p == 0)
		return (-1);

	p++;
	while (*p != 0)  {
		*buffer++ = *p++;
		if ((*p == '@') || (*p == ':'))
			break;
	}
	*buffer = (char)0;

	return (0);
}

/*
 * Get base device name of stdin/stdout device into callers buffer.
 * Return 0 if successful; -1 otherwise.
 */

int
prom_stdin_devname(char *buffer)
{
	return (prom_devname_from_pathname(prom_stdinpath(), buffer));
}

int
prom_stdout_devname(char *buffer)
{
	return (prom_devname_from_pathname(prom_stdoutpath(), buffer));
}

/*
 * Return 1 if stdin/stdout are on the same device and subdevice.
 * Return 0, otherwise.
 */

int
prom_stdin_stdout_equivalence(void)
{
	register char *s, *p;

	/*
	 * Really, all the cases should be handled, here, the
	 * hard way, but this does encompass the default case in the
	 * switch statement, below.  prom_stdinpath() and prom_stdoutpath()
	 * should always be returning a pathname, regardless of PROM type or
	 * level, so the rest of the code is probably unnecessary.
	 */

	s = prom_stdinpath();
	p = prom_stdoutpath();

	/*
	 * XXX: The assumption here, is that if the pathnames came
	 * XXX: from the OBP PROM, that one pathname has not been
	 * XXX: abbreviated differently from the other.  I should probably
	 * XXX: do some more intelligent parsing of the pathnames in order
	 * XXX: to avoid this.  Really, the PROM should only give us fully
	 * XXX: qualified pathnames.
	 */

	if ((s != (char *)0) && (p != (char *)0))  {
		return (prom_strcmp(s, p) == 0 ? 1:0);
	}

	switch (obp_romvec_version)  {
	case OBP_V0_ROMVEC_VERSION:
	case OBP_V2_ROMVEC_VERSION:
		if ((OBP_V0_OUTSINK != OUTSCREEN) &&
		    (OBP_V0_INSOURCE == OBP_V0_OUTSINK))
			return (1);

	}
	return (0);
}

/*
 *	This just returns a pointer to the option's part of the
 *	last part of the string.  Useful for determining which is
 *	the boot partition, tape file or channel of the DUART.
 */
char *
prom_path_options(register char *path)
{
	register char *p, *s;

	s = prom_strrchr(path, '/');
	if (s == (char *)0)
		return ((char *)0);
	p = prom_strrchr(s, ':');
	if (p == (char *)0)
		return ((char *)0);
	return (p+1);
}
