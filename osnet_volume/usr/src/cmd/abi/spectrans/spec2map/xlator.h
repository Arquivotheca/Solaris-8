/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_XLATOR_H
#define	_XLATOR_H

#pragma ident	"@(#)xlator.h	1.1	99/01/25 SMI"

#include "parser.h"
#include "errlog.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define	ARCHBUFLEN	80

/* Architecture Bitmap */
#define	XLATOR_SPARC	0x01
#define	XLATOR_SPARCV9	0x02
#define	XLATOR_I386	0x04
#define	XLATOR_IA64	0x08
#define	XLATOR_ALLARCH	0xFF

/* *_sanity() return codes */
#define	VS_OK	0
#define	VS_INVARCH	1
#define	VS_INVVERS	2
#define	VS_INVALID	3

typedef enum {
	NOTYPE,			/* A type has not yet been assigned */
				/* to the interface */
	FUNCTION = XLATOR_KW_FUNC,	/* Functional Interface */
	DATA = XLATOR_KW_DATA		/* Data Interface */
}    Iftype;

typedef struct Interface {
	char const *IF_name;		/* Name of interface */
	Iftype IF_type;		/* type: FUNCTION or DATA */
	char *IF_version;	/* Version information */
	char *IF_class;		/* public or private or some color */
}   						Interface;

extern char *TargetArchStr;

extern int check_version(const int, const int, const char);
extern int parse_setfile(const char *);
extern int parse_versions(const char *);

extern int valid_version(const char *);
extern int valid_arch(const char *);

#ifdef	__cplusplus
}
#endif

#endif	/* _XLATOR_H */
