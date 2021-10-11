/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)bop2.h   1.1   97/08/13 SMI"
/*
 * Interface file for bop routines
 */
#define	UPDATE_PROP 0
#define	OVERWRITE_PROP 1

void init_bop();
void out_bop(char *buf);
char *in_bop(char *buf, unsigned short len);
char *read_prop(char *prop, char *where);
void write_prop(char *prop, char *where, char *val, char *sep, int flag);
