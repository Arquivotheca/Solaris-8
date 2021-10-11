
/*
 * Copyright (c) 1996 Sun Microsystems, Inc. All rights reserved.
 */

#ident "@(#)ataconf.h   1.1   97/10/01 SMI\n"

/*
 * blacklist - defined in ataconf.c
 */


typedef struct {
	char	b_model[41];
	char	b_single_sector : 1;
	char	b_bogus_bsy : 1;
	char	b_nec_bad_status : 1;
	char	b_bogus_drq : 1;
	char	b_unused4 : 1;
	char	b_unused5 : 1;
	char	b_unused6 : 1;
	char	b_unused7 : 1;
} bl_t;

extern	bl_t	ata_blacklist[];
