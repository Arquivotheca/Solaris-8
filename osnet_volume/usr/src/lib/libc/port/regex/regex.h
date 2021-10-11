/*
       Copyright 01/08/96 Sun Microsystems, Inc. All Rights Reserved
			     regex.h 1.1 96/01/08
*/

/*
 * TOKENS THAT MARK SUBEXPRESSIONS IN COMPILED REGULAR EXPRESSIONS
 *
 * regcmp() uses the tokens in compiled regular expressions, and
 * regex() decodes them when checking character strings against
 * compiled regular expressions.
 *
 * NOTE: To maintain backward compatibility with previous versions of regcmp()
 *       and regex(), these values must stay the same.
 */

#ifndef _REGEX_H
#define _REGEX_H


/* TOKENS USED IN COMPILED REGULAR EXPRESSIONS */

#define	ASCII_CHAR                   20
#define	ANY_CHAR                     16
#define	COUNT                        0x3
#define	COUNTED_GROUP                48
#define	END_GROUP                    44
#define	END_REGEX                    52
#define	END_SAVED_GROUP              12
#define	END_OF_STRING_MARK           28
#define	IN_ASCII_CHAR_CLASS          80
#define	IN_MULTIBYTE_CHAR_CLASS      72
#define	IN_OLD_ASCII_CHAR_CLASS      24
#define MAX_SINGLE_BYTE_INT          0xff
#define	MULTIBYTE_CHAR               36
#define	NOT_IN_ASCII_CHAR_CLASS      84
#define	NOT_IN_MULTIBYTE_CHAR_CLASS  76
#define	NOT_IN_OLD_ASCII_CHAR_CLASS   8
#define	ONE_OR_MORE                  0x2
#define	ONE_OR_MORE_GROUP            68
#define	SAVED_GROUP                  60
#define	SIMPLE_GROUP                 40
#define	START_OF_STRING_MARK         32
#define	THRU                         16
#define TIMES_256_SHIFT               8
#ifdef UNLIMITED
#undef UNLIMITED
#endif
#define UNLIMITED                    0xff
#define	ZERO_OR_MORE                 0x1
#define	ZERO_OR_MORE_GROUP           56


/* NUMBER OF SUBSTRING POINTERS THAT MAY BE RETURNED BY regex() */

#define NSUBSTRINGS                  10

#endif /* #ifndef _REGEX_H */
