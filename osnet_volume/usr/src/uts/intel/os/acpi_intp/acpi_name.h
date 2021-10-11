/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_NAME_H
#define	_ACPI_NAME_H

#pragma ident	"@(#)acpi_name.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/* interface for AML names */


/* namesegs */
#define	NAMESEG_BASE(NP) (acpi_nameseg_t *)((char *)NP + sizeof (name_t))
extern acpi_nameseg_t *nameseg_new(unsigned int seg);
extern void nameseg_free(acpi_nameseg_t *segp);


/* actually only the header for names with more than one segment */
typedef struct name {
	unsigned short gens;	/* generation: parent levels */
	unsigned char segs;	/* 4-character segments */
	unsigned char flags;
	/* first segment starts right after */
} name_t;

/* flags */
#define	NAME_ROOT	(0x01)

#define	NAME_PARENTS(NP) (NP)->gens
#define	NAME_NULL(NP) ((NP)->segs == 0)


/* lex functions declared in acpi_act.h */
extern name_t *name_new(int flags, int gens, int segs);
extern void name_free(name_t *namep);
extern name_t *name_get(char *string);
/* returns the number of characters output */
extern int name_sprint(name_t *np, char *buf);
extern char *name_strbuf(name_t *np);
extern int name_strlen(name_t *np);


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_NAME_H */
