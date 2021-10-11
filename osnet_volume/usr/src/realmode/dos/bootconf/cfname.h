/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * cfname.h -- public definitions for cfname module
 */

#ifndef	_CFNAME_H
#define	_CFNAME_H

#ident "@(#)cfname.h   1.10   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

void init_cfname(void);
void save_cfname(void);
void delete_cfname(void);
void auto_boot_cfname(void);
int copy_file_cfname(char *new_file, char *old_file);

/*
 * Globals
 */
extern int No_cfname; /* set if new configuration */
extern char *Machenv_name;

#ifdef	__cplusplus
}
#endif

#endif	/* _CFNAME_H */
