/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_prop.c	1.12	97/02/12 SMI"

/*
 * Stuff for mucking about with properties
 */

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Create a workaround for PROM bugid 1120271/1123205.
 *
 * In the options node, the return from get-prop and get-proplen
 * may be formatted for display, instead of the actual data.
 *
 * The symptom of the bug is that the returned property length is
 * 27, and the last three characters in the buffer after a getprop
 * are "...", and there is no NULL character terminating the buffer,
 * within the length reported by the prom. In this case, we can
 * use 'interpret' to get the actual len/value of the property.
 */

static dnode_t
get_options_node(void)
{
	static dnode_t options_node;

	if (options_node)
		return (options_node);

	/*
	 * NB: this code is only appropriate for OBP 2.x and later.
	 */
	options_node = prom_finddevice("/options");
	return (options_node);
}

static int
getproplen(dnode_t nodeid, caddr_t name)
{
	int len;

	promif_preprom();
	len = OBP_DEVR_GETPROPLEN(nodeid, name);
	promif_postprom();
	return (len);
}

static int
getprop(dnode_t nodeid, caddr_t name, caddr_t value)
{
	int len;

	promif_preprom();
	len = OBP_DEVR_GETPROP(nodeid, name, value);
	promif_postprom();
	return (len);
}

static char *options_string_names[] = {
	"boot-device",
	"boot-file",
	"diag-device",
	"diag-file",
	"input-device",
	"output-device",
	(char *)0
};

static int
interesting_options_name(char *propname)
{
	char **p;

	for (p = options_string_names;  *p;  ++p)
		if (prom_strcmp(*p, propname) == 0)
			return (1);
	return (0);
}

/*
 * Return non-zero if the workaround is needed for this property.
 */
static int
use_nvram_workaround(dnode_t nodeid, char *name, int proplen, char *buffer)
{

	/*
	 * Don't try 'interpret' if it's OBP 1.x. If it isn't
	 * OBP 1.x, it must be OBP 2.x (romvec version 2 or 3).
	 */
	if (obp_romvec_version == OBP_V0_ROMVEC_VERSION)
		return (0);

	/*
	 * If the possibly flawed length isn't 27, we don't need the
	 * workaround.
	 */
	if (proplen != 27)
		return (0);

	/*
	 * If it isn't the options node, we don't need the workaround.
	 */
	if (nodeid != get_options_node())
		return (0);

	/*
	 * If the property name is not one of the interesting names,
	 * we don't need the workaround.
	 */
	if (!interesting_options_name(name))
		return (0);

	/*
	 * If the last three characters of the buffer are not
	 * "...", we don't need the workaround.
	 */
	(void) getprop(nodeid, name, buffer);
	if (prom_strncmp(buffer + 24, "...", 3) != 0)
		return (0);

	return (1);
}

int
prom_getproplen(dnode_t nodeid, caddr_t name)
{
	int len;
	char ibuf[64];

	len = getproplen(nodeid, name);
	if (use_nvram_workaround(nodeid, name, len, ibuf) == 0)
		return (len);

	/*
	 * Use the workaround, adjusting the length by 1 for the
	 * NULL terminator ... the buf,len on the stack do not
	 * include the NULL terminator.
	 *
	 * Stack diagram for certain prom nvram property words:
	 * 	prop-name ( -- adr,len )
	 *
	 * Not all nvram property words have this stack diagram,
	 * however, those that we're interested in do.
	 */
	(void) prom_sprintf(ibuf, "%s nip h# %x l!", name, &len);
	prom_interpret(ibuf, 0, 0, 0, 0, 0);
	++len;		/* NULL terminate the string */

	return (len);
}

int
prom_getprop(dnode_t nodeid, caddr_t name, caddr_t value)
{
	int len;
	char ibuf[96];
	static char *workaround =
	    "%s dup "			/* ( adr, len, len )	*/
	    "h# %x l! "			/* ( adr, len )		*/
	    "h# %x swap "		/* ( adr, buf, len )	*/
	    "cmove";

	len = getproplen(nodeid, name);
	if (use_nvram_workaround(nodeid, name, len, ibuf) == 0)
		return (getprop(nodeid, name, value));

	/*
	 * Use the workaround which also gets the real property
	 * length into 'len' so we can NULL terminate the buffer.
	 *
	 * Stack diagram for certain prom nvram property words:
	 * 	prop-name ( -- adr, len )
	 *
	 * Not all nvram property words have this stack diagram,
	 * however, those that we're interested in do.
	 */
	(void) prom_sprintf(ibuf, workaround, name, &len, value);
	prom_interpret(ibuf, 0, 0, 0, 0, 0);
	*(value + len) = (char)0;	/* NULL terminate the string */
	++len;

	return (len);
}

/*ARGSUSED*/
caddr_t
prom_nextprop(dnode_t nodeid, caddr_t previous, caddr_t next)
{
	caddr_t prop;

	promif_preprom();
	prop = OBP_DEVR_NEXTPROP(nodeid, previous);
	promif_postprom();
	return (prop);
}

int
prom_setprop(dnode_t nodeid, caddr_t name, caddr_t value, int len)
{
	int i;

	/*
	 * Special prom interface routines are called here to avoid
	 * multiple-demap condition in sun4d. See bug 4011031.
	 */
	promif_setprop_preprom();
	i = OBP_DEVR_SETPROP(nodeid, name, value, len);
	promif_setprop_postprom();
	return (i);
}

/*
 * prom_decode_composite_string:
 *
 * Returns successive strings in a composite string property.
 * A composite string property is a buffer containing one or more
 * NULL terminated strings contained within the length of the buffer.
 *
 * Always call with the base address and length of the property buffer.
 * On the first call, call with prev == 0, call successively
 * with prev == to the last value returned from this function
 * until the routine returns zero which means no more string values.
 */
char *
prom_decode_composite_string(void *buf, size_t buflen, char *prev)
{
	if ((buf == 0) || (buflen == 0) || ((int)buflen == -1))
		return ((char *)0);

	if (prev == 0)
		return ((char *)buf);

	prev += prom_strlen(prev) + 1;
	if (prev >= ((char *)buf + buflen))
		return ((char *)0);
	return (prev);
}
