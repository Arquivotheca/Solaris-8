/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * prop.h -- public definitions for boot properties routines
 */

#ifndef	_PROP
#define	_PROP

#ident "@(#)prop.h   1.11   98/05/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

void menu_prop(void);
void store_prop(char *fname, char *prop, char *value, int do_menu);
char *read_prop(char *prop, char *where);
void set_boot_control_props(void);
void reset_plat_props(void);
void update_plat_prop(char *root_prop, char *option_prop);
void save_plat_props(void);
void restore_plat_props(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _PROP */
