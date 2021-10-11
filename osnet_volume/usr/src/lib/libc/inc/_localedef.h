/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */
#ident	"@(#)_localedef.h	1.2	94/10/04 SMI"

/*
 * Common Header File for Localedef command
 */
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define	 MAX_ID_LENGTH	256
#define	 MAX_LINE_LENGTH	256
#define	 MAX_BYTES	256
#define	 ERROR		-1

#define	_CHARMAP 	1
#define _XSH4_CHRTBL	2
#define _XSH4_COLLATE	3
#define _XSH4_MESSAGE	4
#define _XSH4_MONTBL	5
#define _XSH4_NUMERIC	6
#define _XSH4_TIME	7

/*
 * encoded symbol
 */
typedef struct encoded_val {
	int length;
	unsigned char bytes[MAX_BYTES];
} encoded_val;

/*
 * keyword structure
 */
typedef struct keyword {
	char *name;
	int kval;
	int ktype;
} keyword;
