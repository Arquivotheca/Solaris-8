/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)variant.h 1.1	96/01/17 SMI"

/*
 * Copyright 1993 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 * 
 * $Header: /rd/h/rcs/variant.h 1.2 1994/01/24 23:12:41 mark Exp $
 */

 /*
 * For EBCDIC support:
 *    the variant structure that contains the 13 POSIX.2 portable characters 
 *    that are variant in EBCDIC based code pages.
 */

#ifndef __M_VARIANT_H__
#define	__M_VARIANT_H__

struct variant {
	char	*codeset;
	char	backslash;
	char	right_bracket;
	char	left_bracket;
	char	right_brace;
	char	left_brace;
	char	circumflex;
	char	tilde;
	char	exclamation_mark;
	char	number_sign;
	char	vertical_line;
	char	dollar_sign;
	char	commercial_at;
	char	grave_accent;
};

struct variant *getsyntx(void);

#endif	/* __M_VARIANT_H__ */
