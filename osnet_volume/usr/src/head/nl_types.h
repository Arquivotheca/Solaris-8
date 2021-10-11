/*
 *  nl_types.h
 *
 * Copyright (c) 1991,1997 Sun Microsystems, Inc.
 * All Rights Reserved
 */

#ifndef	_NL_TYPES_H
#define	_NL_TYPES_H

#pragma ident	"@(#)nl_types.h	1.13	97/07/31 SMI"

#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	NL_SETD 		1    /* XPG3 Conformant Default set number. */
#define	NL_CAT_LOCALE 		(-1) /* XPG4 requirement */

#define	_CAT_MAGIC 		0xFF88FF89
#define	_CAT_HDR_SIZE 		sizeof (struct _cat_hdr)
#define	_CAT_SET_HDR_SIZE 	sizeof (struct _cat_set_hdr)
#define	_CAT_MSG_HDR_SIZE 	sizeof (struct _cat_msg_hdr)

struct _cat_hdr
{
#if	!defined(_LP64)
	long __hdr_magic;		/* must contain CAT_MAGIC */
#else
	int	__hdr_magic;		/* must contain CAT_MAGIC */
#endif
	int __nsets;		/* the number of sets in the catalogue */
	int __mem;		/* the size of the catalogue; the size	   */
				/* does not include the size of the header */
#if	!defined(_LP64)
	long __msg_hdr_offset;	/* the byte offset of the first message */
				/* header */
	long __msg_text_offset;	/* the byte offset of the message text area */
#else
	int __msg_hdr_offset;	/* the byte offset of the first message */
				/* header */
	int __msg_text_offset;	/* the byte offset of the message text area */
#endif
};

struct _cat_set_hdr
{
	int __set_no;	/* the set number; must be greater than 0;   */
			/* should be less than or equal to NL_SETMAX */
	int __nmsgs;	/* the number of msgs in the set */
	int __first_msg_hdr;	/* the index of the first message header in */
				/* the set; the value is not a byte offset, */
				/* it is a 0-based index		    */
};

struct _cat_msg_hdr
{
	int __msg_no;	/* the message number; must be greater than 0; */
			/* should be less than or equal to NL_MSGMAX   */
	int __msg_len;	/* the length of the message; must be greater */
			/* than or equal to zero; should be less than */
			/* or equal to NL_TEXTMAX */
	int __msg_offset; /* the byte offset of the message in the message */
			/* area; the offset is relative to the start of  */
			/* the message area, not to the start of the	 */
			/* catalogue.					 */
};

struct _nl_catd_struct {
	void	*__content;	/* mmaped catalogue contents */
	int	__size;		/* Size of catalogue file */
};

typedef struct _nl_catd_struct *nl_catd;
typedef int nl_item;	/* XPG3 Conformant for nl_langinfo(). */

/* The following is just for the compatibility between OSF and Solaris */
/* Need to be removed later */
typedef	nl_item	__nl_item;

#ifdef	__STDC__
int	catclose(nl_catd);
char	*catgets(nl_catd, int, int, const char *);
nl_catd catopen(const char *, int);
#else
int	catclose();
char	*catgets();
nl_catd catopen();
#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _NL_TYPES_H */
