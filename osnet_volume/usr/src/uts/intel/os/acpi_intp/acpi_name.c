/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_name.c	1.1	99/05/21 SMI"

/* AML Names */


#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>
#include <sys/acpi_prv.h>

#ifdef ACPI_BOOT
#include <sys/salib.h>
#endif

#ifdef ACPI_USER
#include <stdlib.h>
#include <strings.h>
#endif

#include "acpi_exc.h"
#include "acpi_bst.h"
#include "acpi_stk.h"
#include "acpi_par.h"

#include "acpi_lex.h"
#include "acpi_elem.h"
#include "acpi_act.h"
#include "acpi_name.h"


#define	NULL_NAME	(0x00)
#define	DUAL_PREFIX	(0x2E)
#define	MULTI_PREFIX	(0x2F)
#define	ROOT_PREFIX	(0x5C)
#define	PARENT_PREFIX	(0x5E)
#define	UNDERSCORE	(0x5F)


#define	LOOK_CHAR(CHAR)		(lex_table[CHAR].flags & CTX_NAME)
#define	LEAD_CHAR(CHAR)		(lex_table[CHAR].flags & CTX_LNAME)
#define	SEG_CHAR(CHAR)		(lex_table[CHAR].flags & CTX_SNAME)

char *name_strbuf(name_t *np);

#define	NAME_BUFSIZ (256)
static char name_buf[NAME_BUFSIZ];
#define	DOT 0x2E

/*
 * nameseg stuff
 */
acpi_nameseg_t *
nameseg_new(unsigned int seg)
{
	acpi_nameseg_t *new;

	if ((new = kmem_alloc(sizeof (acpi_nameseg_t), KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	new->iseg = seg;
	return (new);
}

void
nameseg_free(acpi_nameseg_t *segp)
{
	if (segp)
		kmem_free(segp, sizeof (acpi_nameseg_t));
}

int
nameseg_lex(parse_state_t *pstp)
{
	int byte;
	acpi_nameseg_t *new;
	value_entry_t *vp;

	if ((new = nameseg_new(0)) == NULL)
		return (ACPI_EXC);
	if ((byte = bst_get(pstp->bp)) == ACPI_EXC || ! LEAD_CHAR(byte))
		return (ACPI_EXC);
	new->cseg[0] = byte;
	if ((byte = bst_get(pstp->bp)) == ACPI_EXC || ! SEG_CHAR(byte))
		return (ACPI_EXC);
	new->cseg[1] = byte;
	if ((byte = bst_get(pstp->bp)) == ACPI_EXC || ! SEG_CHAR(byte))
		return (ACPI_EXC);
	new->cseg[2] = byte;
	if ((byte = bst_get(pstp->bp)) == ACPI_EXC || ! SEG_CHAR(byte))
		return (ACPI_EXC);
	new->cseg[3] = byte;
	exc_debug(ACPI_DLEX, "name segment %c%c%c%c",
		new->cseg[0], new->cseg[1], new->cseg[2], new->cseg[3]);
	VALUE_PUSH(vp);
	vp->elem = T_NAME_SEG;
	vp->data = (void *)new;
	return (VALRET);
}

/*
 * names
 */

name_t *
name_new(int flags, int gens, int segs)
{
	name_t *new;
	int size;

	size = sizeof (name_t) + segs * sizeof (acpi_nameseg_t);
	if ((new = kmem_alloc(size, KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	bzero(new, size);
	new->flags = (unsigned char)flags;
	new->gens = (unsigned short)gens;
	new->segs = (unsigned char)segs;
	return (new);
}

void
name_free(name_t *namep)
{
	if (namep)
		kmem_free(namep,
		    sizeof (name_t) + namep->segs * sizeof (acpi_nameseg_t));
}

int
name_string_lex(parse_state_t *pstp)
{
	int byte, gens, segs, flags, i;
	name_t *new;
	value_entry_t *vp;
	acpi_nameseg_t *segp;

	/* eof or bad first character */
	if ((byte = bst_peek(pstp->bp)) == ACPI_EXC || LOOK_CHAR(byte) == 0)
		return (ACPI_EXC);

	flags = 0;
	gens = 0;
	if (byte == ROOT_PREFIX) { /* root */
		(void) bst_get(pstp->bp);
		flags = NAME_ROOT;
	} else if (byte == PARENT_PREFIX) { /* parent(s) */
		for (; ; gens++)
			if (bst_get(pstp->bp) != PARENT_PREFIX)
				break;
		(void) bst_unget(pstp->bp);
		if (bst_peek(pstp->bp) == ACPI_EXC)
			return (ACPI_EXC);
	}

	byte = bst_peek(pstp->bp); /* how many segments */
	if (byte == DUAL_PREFIX) {
		(void) bst_get(pstp->bp);
		segs = 2;
	} else if (byte == MULTI_PREFIX) {
		(void) bst_get(pstp->bp);
		if ((segs = bst_get(pstp->bp)) == ACPI_EXC)
			return (ACPI_EXC);
	} else if (byte == NULL_NAME) {
		(void) bst_get(pstp->bp);
		segs = 0;
	} else
		segs = 1;

	if ((new = name_new(flags, gens, segs)) == NULL)
		return (ACPI_EXC);

	segp = NAMESEG_BASE(new);
	if (bst_buffer(pstp->bp, (char *)segp, segs *
			sizeof (acpi_nameseg_t)) == ACPI_EXC)
		return (ACPI_EXC);
	for (i = 0; i < segs; i++, segp++) {
		if (! LEAD_CHAR(segp->cseg[0]))
			return (exc_code(ACPI_ECHAR));
		if (! SEG_CHAR(segp->cseg[1]) ||
		    ! SEG_CHAR(segp->cseg[2]) ||
		    ! SEG_CHAR(segp->cseg[3]))
			return (exc_code(ACPI_ECHAR));
	}

	exc_debug(ACPI_DLEX, "name string %s", name_strbuf(new));
	VALUE_PUSH(vp);
	vp->elem = V_NAME_STRING;
	vp->data = (void *)new;
	return (VALRET);
}

name_t *
name_get(char *string)
{
	char *save;
	name_t *new;
	acpi_nameseg_t *segp;
	int root, gens, segs, i;

	if (string == NULL)
		return (NULL);

	root = 0;
	gens = 0;
	switch (*string) {
	case ROOT_PREFIX:
		root = 1;
		string++;
		break;
	case PARENT_PREFIX:
		for (; *string == PARENT_PREFIX; string++, gens++)
			;
		break;
	}
	save = string;		/* after all prefixes */

	for (segs = 0, i = 0; *string; string++) /* i == seglen */
		if (*string == DOT) {
			segs++;
			i = 0;
			continue;
		} else if (i == 0 && ! LEAD_CHAR(*string) || /* lead char */
		    i >= 4 || /* seg too long */
		    ! SEG_CHAR(*string))  /* others chars */
			return (exc_null(ACPI_ECHAR));
		else
			i++;
	if (i != 0)
		segs++;
	else
		if (segs != 0) /* can't end on a dot */
			return (exc_null(ACPI_ECHAR));

	if ((new = name_new(root ? NAME_ROOT : 0, gens, segs)) == NULL)
		return (NULL);

	for (i = 0, segp = NAMESEG_BASE(new); i < segs; i++, segp++) {
		segp->iseg = (unsigned int)0x5F5F5F5F;
		segp->cseg[0] = *save++;
		if (*save == NULL)
			break;
		if (*save == DOT) {
			save++;
			continue;
		}
		segp->cseg[1] = *save++;
		if (*save == NULL)
			break;
		if (*save == DOT) {
			save++;
			continue;
		}
		segp->cseg[2] = *save++;
		if (*save == NULL)
			break;
		if (*save == DOT) {
			save++;
			continue;
		}
		segp->cseg[3] = *save++;
		if (*save == NULL)
			break;
		if (*save == DOT)
			save++;
	}
	return (new);
}

/* return length, if printed */
int
name_strlen(name_t *np)
{
	int i, first, seglen, outlen;
	acpi_nameseg_t *segp;

	if (np->flags & NAME_ROOT)
		outlen = 1;
	else if (NAME_PARENTS(np))
		outlen = np->gens;
	else
		outlen = 0;

	segp = NAMESEG_BASE(np);
	for (i = 0, first = 1; i < np->segs; i++, segp++) {
		if (first)
			first = 0;
		else
			outlen++; /* for dot */

		if (segp->cseg[3] != UNDERSCORE)
			seglen = 4;
		else if (segp->cseg[2] != UNDERSCORE)
			seglen = 3;
		else if (segp->cseg[1] != UNDERSCORE)
			seglen = 2;
		else
			seglen = 1;
		outlen += seglen;
	}
	return (outlen);
}


/* returns the number of characters output */
int
name_sprint(name_t *np, char *buf)
{
	int i, j, first, seglen, outlen;
	acpi_nameseg_t *segp;

	if (np->flags & NAME_ROOT) {
		*buf++ = ROOT_PREFIX;
		outlen = 1;
	} else if (NAME_PARENTS(np)) {
		for (i = 0; i < np->gens; i++)
			*buf++ = PARENT_PREFIX;
		outlen = np->gens;
	} else
		outlen = 0;

	segp = NAMESEG_BASE(np);
	for (i = 0, first = 1; i < np->segs; i++, segp++) {
		if (first)
			first = 0;
		else {
			*buf++ = (char)DOT;
			outlen++;
		}

		if (segp->cseg[3] != UNDERSCORE)
			seglen = 4;
		else if (segp->cseg[2] != UNDERSCORE)
			seglen = 3;
		else if (segp->cseg[1] != UNDERSCORE)
			seglen = 2;
		else
			seglen = 1;

		for (j = 0; j < seglen; j++)
			*buf++ = segp->cseg[j];
		outlen += seglen;
	}
	*buf = NULL;		/* tie off end */
	return (outlen);
}

char *
name_strbuf(name_t *np)
{
	if (name_strlen(np) >= NAME_BUFSIZ)
		return (NULL);
	(void) name_sprint(np, &name_buf[0]);
	return (&name_buf[0]);
}


/* eof */
