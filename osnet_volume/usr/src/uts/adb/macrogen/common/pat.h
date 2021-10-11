/*
 * Copyright (c) 1995,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Interface to pattern-matching routines.
 */

#ifndef	_PAT_H
#define	_PAT_H

#pragma ident	"@(#)pat.h	1.2	98/04/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* Fast globbing-style pattern-matching */

/* Error codes (0 = success). */
#define	PAT_ERR_MEMORY    1	/* out of memory */
#define	PAT_ERR_PAREN	  2	/* unpaired brace */
#define	PAT_ERR_BACKSLASH 3	/* backslash at end of pattern */
#define	PAT_ERR_MAX	  0xf	/* highest err # this package will "ever" use */

/* A pat_handle_t is really a pointer to one of these things. */
typedef int pat_cell_t;
typedef struct pat_handle {
	pat_cell_t *cells_end;
	pat_cell_t  cells[1];
} *pat_handle_t;

/* A type used internally. */
typedef struct pat_rest pat_rest_t;

/* Structure for providing configuration information. */
typedef struct {
	char left_paren;
	char right_paren;
	char match_any;
	char match_one;
	char not;
	char or;
	char backslash;
	char reserved[9];  /* should be 0; for future growth */
} pat_config_t;
extern const pat_config_t pat_default_config;

int	/* error code */
pat_compile(
	const char *,		/* pattern string */
	const pat_config_t *,	/* config info */
	pat_handle_t *		/* returned handle */
);
char *	/* ptr to new null terminator */
pat_decompile(
	pat_handle_t,
	const pat_config_t *,	/* config info */
	char *			/* buffer to hold decompiled pattern */
);
#define	PAT_MATCH(pat, start, end) \
	pat_match_((pat)->cells, (pat)->cells_end, start, end, \
			(pat_rest_t *)0)
#define	PAT_STRMATCH(pat, s) PAT_MATCH(pat, s, s + strlen(s))
int	/* 1 if they match, else 0 */
pat_match_(/* internal interface - use PAT_MATCH() or PAT_STRMATCH() */
	const pat_cell_t *,
	const pat_cell_t *,
	const char *,		/* first of chars to try to match */
	const char *,		/* end = last char + 1 */
	const pat_rest_t *
);
void
pat_dump(pat_handle_t);  /* for debug */
void
pat_destroy(pat_handle_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _PAT_H */
