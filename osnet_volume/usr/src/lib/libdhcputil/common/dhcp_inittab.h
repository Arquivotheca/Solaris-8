/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DHCP_INITTAB_H
#define	_DHCP_INITTAB_H

#pragma ident	"@(#)dhcp_inittab.h	1.4	99/07/26 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>	/* MIN, MAX */
#include <limits.h>

/*
 * dhcp_inittab.[ch] make up the interface to the inittab file, which
 * is a table of all known DHCP options.  please see `README.inittab'
 * for more background on the inittab api, and dhcp_inittab.c for details
 * on how to use the exported functions.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	ITAB_INITTAB_PATH	"/etc/dhcp/inittab"
#define	ITAB_MAX_LINE_LEN	8192 		/* bytes */
#define	ITAB_MAX_NUMBER_LEN	30		/* digits */
#define	ITAB_COMMENT_CHAR	'#'

#define	ITAB_FAILURE		0
#define	ITAB_SUCCESS		1
#define	ITAB_UNKNOWN		2

/*
 * note: ITAB_CAT_* values must be kept in sync with ife_categories[]
 * in dhcp_inittab.c.
 */

#define	ITAB_CAT_STANDARD	0x01
#define	ITAB_CAT_FIELD		0x02
#define	ITAB_CAT_INTERNAL	0x04
#define	ITAB_CAT_VENDOR		0x08
#define	ITAB_CAT_SITE		0x10
#define	ITAB_CAT_COUNT		5

#define	ITAB_CONS_INFO		'i'
#define	ITAB_CONS_SERVER	'd'
#define	ITAB_CONS_SNOOP		's'
#define	ITAB_CONS_MANAGER	'm'
#define	ITAB_CONS_STRING	"idsm"
#define	ITAB_CONS_COUNT		(sizeof (ITAB_CONS_STRING) - 1)

#define	ITAB_CODE_MAX		UCHAR_MAX	/* for now */
#define	ITAB_GRAN_MAX		UCHAR_MAX
#define	ITAB_MAX_MAX		UCHAR_MAX

/*
 * XXX DHCP_SYMBOL_SIZE and CDTYPE should come from <netinet/dhcp.h>
 */

#define	DHCP_SYMBOL_SIZE	8
typedef enum {

	ASCII, OCTET, IP, NUMBER, BOOL, INCLUDE, UNUMBER8, UNUMBER16,
	UNUMBER32, UNUMBER64, SNUMBER8, SNUMBER16, SNUMBER32, SNUMBER64

} CDTYPE;

/*
 * note: inittab_verify() depends on the order of these fields;  new
 * fields should be appended, not inserted arbitrarily.
 */

typedef struct inittab_entry {

	uchar_t		ie_category;			/* category */
	uint16_t	ie_code;			/* option code */
	char		ie_name[DHCP_SYMBOL_SIZE + 1];	/* option name */
	CDTYPE		ie_type;			/* type of parameter */
	uchar_t		ie_gran;			/* granularity */
	uchar_t		ie_max;				/* maximum number */
	uint16_t	ie_max_size;			/* maximum total size */

} inittab_entry_t;

extern inittab_entry_t	*inittab_load(uchar_t, char, size_t *);
extern inittab_entry_t	*inittab_getbyname(uchar_t, char, const char *);
extern inittab_entry_t	*inittab_getbycode(uchar_t, char, uint16_t);
extern int		 inittab_verify(inittab_entry_t *, inittab_entry_t *);
extern uchar_t		*inittab_encode(inittab_entry_t *, const char *,
			    uint16_t *, boolean_t);
extern char		*inittab_decode(inittab_entry_t *, uchar_t *, uint16_t,
			    boolean_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _DHCP_INITTAB_H */
