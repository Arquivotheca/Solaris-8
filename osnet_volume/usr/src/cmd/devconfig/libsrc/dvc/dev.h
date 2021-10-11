/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)dev.h 1.4 94/07/13 SMI"

#ifndef DEV_DOT_H
#define DEV_DOT_H

void		fetch_dev_info(void);
device_info_t*	make_dev_node1(attr_list_t* typ);
attr_list_t*	make_dev_alist(attr_list_t *dev_alist, attr_list_t *typ_alist);
char* 		make_title(char *name, int unit, attr_list_t *typ);
extern device_info_t *dev_head;

void		fetch_devi_info(void);
void		add_devi_node(conf_list_t* alist);
extern conf_list_t *devi_head;

void		fetch_typ_info(void);
char*		get_typ_group(char* name);
attr_list_t*	make_typ_attr(attr_list_t* val_string, attr_list_t* typ);
attr_list_t*	make_typ_alist(attr_list_t* val_string, attr_list_t* typ);
extern conf_list_t *typ_head;

void		fetch_pmi_info(void);
char*		make_pmi_fname(char* type, char* pmi_base, char* isxin);
char*		make_vda_fname(char* type, char* vda_base);
char*		get_pmi_name(char* pmi_file);
char*		get_pmi_type(char* name);
char*		pmi_path();
char*		extract_keyword(char *path, char *active);
void 		strip_crap_from_tail(char* str, char* chars);
void		each_pmi_title(char* n, char* t, void (*a)(char* p, char* t));
void		each_vda_title(char* n, char* t, void (*a)(char* p, char* t));

conf_list_t*	cvt_typ_in(attr_list_t *typ);
void		cvt_typ_out(attr_list_t *typ);
void		cvt_dev_in(device_info_t *dp);
device_info_t*	cvt_dev_out(device_info_t *dp);

#endif /* DEV_DOT_H */
