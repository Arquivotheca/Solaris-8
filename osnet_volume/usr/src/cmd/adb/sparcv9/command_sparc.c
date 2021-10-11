/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

/*
 * adb - SPARC extended command parser.
 */

#ident "@(#)command_sparc.c	1.3	97/09/04 SMI"

#include <stdio.h>
#include "adb.h"
#include "fio.h"
#include "fpascii.h"

#define	QUOTE	0200
#define	STRIP	0177

extern int datalen;
extern int wp_mask;
const char *bphdr = "breakpoints\ncount%8tbkpt%24ttype%34tlen%40tcommand\n";

/* 
 * This file contains archecture specific extensions to the command parser
 * The following must be defined in this file:
 *
 *-----------------------------------------------------------------------
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
 *-----------------------------------------------------------------------
 * ext_getstruct:
 *   return the address of the ecmd structure for extended commands (ie ::)
 *
 *-----------------------------------------------------------------------
 * ext_ecmd:
 *  extended command parser (ie :: commands)
 *   ON ENTRY:
 *	buf    - pointer to the extended command
 *   ON EXIT:
 *	Will exec a found command and return non-zero
 *
 *-----------------------------------------------------------------------
 * ext_dol:
 *   command extensions for $ .
 *   ON ENTRY:
 *	modif    - is the dollar command (ie $b would yeld b)
 *   ON EXIT:
 *      Will return non-zero if a command was found.
 *
 */


/* This is archecture specific extensions to the slash cmd.
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

/* breakpoints */
struct	bkpt *bkpthead;

/* This is archecture specific extensions to the $ cmd.
 * On return this will return non-zero if there was a cmd!
 */
int
ext_dol(modif)
int	modif;
{
	switch(modif) {
#if defined(KADB) && defined(sun4u)
	case 'l':
		/*
		 * Set watchpoint mask.
		 */
		wp_mask = (hadaddress ? address : 0xff);
		if (wp_mask > 0xff || wp_mask < 0)
			wp_mask = 0xff;
		break;
#endif
	case 'b': {
		register struct bkpt *bkptr;

		printf(bphdr);
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
#if defined(KADB)
				case BPINST:
					printf(":b instr");
					break;
				case BPACCESS:
					printf(":a v,r/w");
					break;
				case BPWRITE:
					printf(":w v,w");
					break;
				case BPPHYS:
					printf(":f p,r/w");
					break;
				case BPDEFR:
					printf(":b defrd");
					break;
#else
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

/*
 * Extended commands.
 */

static struct ecmd sparc_ecmd[];

/*
 * This returns the address of the ext_extended command structure.
 */
struct ecmd *
ext_getstruct()
{
	return (sparc_ecmd);
}

/*
 * Architecture-specific extensions to the extended cmds (::).
 */
int
ext_ecmd(buf)
char *buf;
{
	int i;

	i = extend_scan(buf, sparc_ecmd);
	if (i >= 0) 
	{
		(*sparc_ecmd[i].func)();
		return (1);
	}

	return (0);
}	

/*
 * all aval extended commands should go here
 */
static struct ecmd sparc_ecmd[] = {
{ 0 }
};
