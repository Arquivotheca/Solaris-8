/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_ARGVEC_H
#define	_MDB_ARGVEC_H

#pragma ident	"@(#)mdb_argvec.h	1.1	99/08/11 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct mdb_arg;

typedef struct mdb_argvec {
	struct mdb_arg *a_data;		/* Array of arguments */
	size_t a_nelems;		/* Number of valid elements */
	size_t a_size;			/* Array size */
} mdb_argvec_t;

#define	MDB_OPT_SETBITS	1		/* Boolean option, set opt_bits */
#define	MDB_OPT_CLRBITS	2		/* Boolean option, clear opt_bits */
#define	MDB_OPT_STR	3		/* Option requires a string argument */
#define	MDB_OPT_UINTPTR	4		/* Option requires a value argument */
#define	MDB_OPT_UINT64	5		/* Option requires a value argument */

typedef struct mdb_opt {
	char opt_char;			/* Option name */
	void *opt_valp;			/* Value storage pointer */
	uint_t opt_bits;		/* Bits to set or clear for booleans */
	uint_t opt_type;		/* Option type (see above) */
} mdb_opt_t;

#ifdef _MDB

#ifdef	_BIG_ENDIAN
#ifdef	_LP64
#define	MDB_INIT_CHAR(x)	((const char *)((uintptr_t)(uchar_t)(x) << 56))
#else	/* _LP64 */
#define	MDB_INIT_CHAR(x)	((const char *)((uintptr_t)(uchar_t)(x) << 24))
#endif	/* _LP64 */
#else	/* _BIG_ENDIAN */
#define	MDB_INIT_CHAR(x)	((const char *)(uchar_t)(x))
#endif	/* _BIG_ENDIAN */
#define	MDB_INIT_STRING(x)	((const char *)(x))

void mdb_argvec_create(mdb_argvec_t *);
void mdb_argvec_destroy(mdb_argvec_t *);
void mdb_argvec_append(mdb_argvec_t *, const struct mdb_arg *);
void mdb_argvec_reset(mdb_argvec_t *);
void mdb_argvec_zero(mdb_argvec_t *);
void mdb_argvec_copy(mdb_argvec_t *, const mdb_argvec_t *);

#endif	/* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_ARGVEC_H */
