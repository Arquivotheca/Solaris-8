/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)conf.h 1.7 95/02/28 SMI"

#ifndef CONF_DOT_H
#define	CONF_DOT_H

/*
 * There is a conf_list node for each device, with name/value pairs
 * pointed to by the alist.  Each alist node holds the name and a pointer
 * to the value list, as each name may have string of values.
 */
typedef struct conf_list {
	struct conf_list *next;
	attr_list_t *alist;
} conf_list_t;

conf_list_t 	*read_conf(FILE *);
void		write_conf(FILE *, conf_list_t *);
conf_list_t	*dup_conf(conf_list_t *cf);
void		free_conf(conf_list_t *cf);

void		make_attr(attr_list_t **alist, char *name, val_t t, ...);
void		free_attr(struct attr_list *alist);
void		remove_attr(attr_list_t **alist, char *name);
char		*get_attr_str(struct attr_list *alist, char *name);
val_list_t	*get_attr_val(struct attr_list *alist, char *name);
char		*find_attr_str(struct attr_list *alist, char *name);
val_list_t	*find_attr_val(struct attr_list *alist, char *name);
attr_list_t	*find_attr_list(struct conf_list *cf, char *name, char *value);
conf_list_t	*find_attr_listc(struct conf_list *cf, char *name, char *value);
int		match_attr(attr_list_t *alist, char *name, char *value);
void		make_val(struct val_list **vlist, val_t t, ...);
int   		find_val_name(char *name, val_list_t *list);
attr_list_t	*dup_alist(attr_list_t *alist);
attr_list_t	*dup_attr(attr_list_t *alist, char *attrval, ...);

FILE		*open_conf_file(char *path, char *name, int wr);
void		write_conf_file(device_info_t *dp);

conf_list_t	*parse_conf(char *fcp);
void		update_mod_conf(void);

char		*get_write_map(char *dir);
char		*get_write_path(char *dir, char *name);

#endif /* CONF_DOT_H */
