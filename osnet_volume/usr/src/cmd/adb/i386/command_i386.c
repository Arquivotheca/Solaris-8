/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * adb - i386 command_i386 parser
 */

#ident "@(#)command_i386.c	1.8	98/02/09 SMI"

#include <stdio.h>
#include "adb.h"
#include "fio.h"
#include "fpascii.h"

#define	QUOTE	0200
#define	STRIP	0177

/*
 * This file contains archecture specific extensions to the command parser
 * The following must be defined in this file:
 *
 * -----------------------------------------------------------------------
 * ext_slash:
 *   command extensions for /,?,*,= .
 *   ON ENTRY:
 *	cmd    - is the char format (ie /v would yeld v)
 *	buf    - pointer to the raw command buffer
 *	defcom - is the char of the default command (ie last command)
 *	eqcom  - true if command was =
 *	atcom  - true if command was @
 *	itype  - address space type (see adb.h)
 *	ptype  - symbol space type (see adb.h)
 *   ON EXIT:
 *      Will return non-zero if a command was found.
 *
 * -----------------------------------------------------------------------
 * ext_getstruct:
 *   return the address of the ecmd structure for extended commands (ie ::)
 *
 * -----------------------------------------------------------------------
 * ext_ecmd:
 *  extended command parser (ie :: commands)
 *   ON ENTRY:
 *	buf    - pointer to the extended command
 *   ON EXIT:
 *	Will exec a found command and return non-zero
 *
 * -----------------------------------------------------------------------
 * ext_dol:
 *   command extensions for $ .
 *   ON ENTRY:
 *	modif    - is the dollar command (ie $b would yeld b)
 *   ON EXIT:
 *	Will return non-zero if a command was found.
 *
 */


/*
 * This is archecture specific extensions to the slash cmd.
 * On return this will return non-zero if there was a cmd!
 */
int
ext_slash(cmd, buf, defcom, eqcom, atcom, itype, ptype)
char	cmd;
char	*buf, defcom;
int	eqcom, atcom, itype, ptype;
{
	return (0);
}

int datalen;

/* breakpoints */
struct	bkpt *bkpthead;


/*
 * This is archecture specific extensions to the $ cmd.
 * On return this will return non-zero if there was a cmd!
 */
int
ext_dol(modif)
int	modif;
{
	switch (modif) {
#ifdef KADB
	case 'l':
		if (hadaddress)
			if (address == 1 || address == 2 || address == 4)
				datalen = address;
			else
				error("bad data length");
		else
			datalen = 1;
		break;
#endif
	case 'b': {
		register struct bkpt *bkptr;

		printf("breakpoints\ncount%8tbkpt%24ttype%34tlen%40tcommand\n");
		for (bkptr = bkpthead; bkptr; bkptr = bkptr->nxtbkpt)
			if (bkptr->flag) {
				printf("%-8.8d", bkptr->count);
#if defined(KADB)
				if (bkptr->type == BPDEFR) {
					printf("%s#", bkptr->mod_name);
					printf("%s+", bkptr->sym_name);
				} else if (bkptr->mod_name[0] != '\0') {
					printf("%s#", bkptr->mod_name);
				}
#endif
				psymoff(bkptr->loc, ISYM, "%24t");
				switch (bkptr->type) {
				case BPINST:
					printf(":b instr");
					break;
				case BPDBINS:
					printf(":p exec");
					break;
				case BPACCESS:
					printf(":a rd/wr");
					break;
				case BPWRITE:
					printf(":w write");
					break;
#if defined(KADB)
				case BPDEFR:
					printf(":b defrd");
					break;
				case BPX86IO:
					printf(":P I/O");
					break;
#endif
				}
				printf("%34t%-5D %s", bkptr->len, bkptr->comm);
			}
		}
		break;
	default:
		return (0);
	}
	return (1);
}

static struct ecmd i386_ecmd[];

/*
 * This returns the address of the ext_extended command structure
 */
struct ecmd *
ext_getstruct()
{
	return (i386_ecmd);
}

/*
 * This is archecture specific extensions to extended cmds.
 * On return this will return non-zero if there was a cmd!
 */
int
ext_ecmd(buf)
char *buf;
{
	int i;

	i = extend_scan(buf, i386_ecmd);
	if (i >= 0)
	{
		(*i386_ecmd[i].func)();
		return (1);
	}

	return (0);
}	

/*
 * all avail extended commands should go here
 */
static struct ecmd i386_ecmd[] = {
{ 0 }
};
