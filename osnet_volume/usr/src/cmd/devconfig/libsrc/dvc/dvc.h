/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)dvc.h 1.18 95/05/24 SMI"

#ifndef DVC_DOT_H
#define	DVC_DOT_H

/*
 * Device categories.
 */
void	fetch_cat_info(void);
void	set_cat_idx(int idx);
char	*next_cat_title(void);
char	*next_cat_dev_title(void);

/*
 * Device attributes.
 */
typedef enum {
	VAL_UNUMERIC,		/* unsigned numeric	*/
	VAL_NUMERIC,
	VAL_STRING,
	VAL_ERROR
} val_t;

typedef union val_store {
	char			*string;
	int			integer;
	unsigned int		uinteger;
} val_store_t;

typedef struct val_list {
	struct val_list		*next;
	val_t			val_type;
	val_store_t		val;
} val_list_t;

typedef struct attr_list {
	struct attr_list	*next;
	char			*name;
	val_list_t		*vlist;
} attr_list_t;

#define	NAME_ATTR	"name"
#define	CLASS_ATTR	"class"
#define	WNAME_ATTR	"__wname__"
#define	WCLASS_ATTR	"__wclass__"
#define	TITLE_ATTR	"__title__"
#define	CAT_ATTR	"__category__"
#define	AUTO_ATTR	"__auto__"
#define	DRIVER_ATTR	"__driver__"
#define	COMMON_ATTR	"__common__"
#define	INSTANCE_ATTR	"__instance__"
#define	GROUP_ATTR	"__group__"
#define	MEMBERS_ATTR	"__members__"
#define	DEFAULT_ATTR	"__default__"
#define	INTERNAL_ATTR	"^__.*__$"
#define	XINSIDE_ATTR	"__xinside__"
#define	THIRDP_ATTR	"__3party__"
#define	DEPTH_ATTR	"__depth__"
#define	RES_ATTR	"__resolution__"
#define	KRN_ATTR	"__krndrv__"
#define	PLTFRM_ATTR	"__pltfrm__"
#define	USR_ATTR	"__usr__"
#define	DRVR_ATTR	"drv_name"

#define	NUMERIC_STRING	"numeric,"
#define	STRING_STRING	"string,"
#define	VAR_STRING	"var,"

int		count_numeric(char *fmt);
int		count_string(char *fmt);
void		next_numeric(char **var_fmt, int *pn, int *usint);
char		*next_string(char **var_fmt);
char		*expand_abbr(char *text);
val_list_t	*find_attr_val(struct attr_list *alist, char *name);
attr_list_t	*find_typ_info(char *name);

/*
 * Configured devices.
 */
typedef struct device_info {
	struct device_info	*next;
	char			*name;
	int			unit;
	char			*title;
	attr_list_t		*dev_alist;
	attr_list_t		*typ_alist;
	int			modified;
	void			*ui;
} device_info_t;

char		*next_dev_title(void);
device_info_t	*make_dev_node(int idx);
void		free_dev_node(device_info_t *dp);
int		add_dev_node(device_info_t *dp);
int		remove_dev_node(int idx);
device_info_t	*get_dev_node(int idx);
char		*get_dev_cat(device_info_t *dp);
char		*get_depth(device_info_t *dp, char *bd);
void		get_resdepth(device_info_t *dp, char *depth,char **resp, char **deskp);

/*
 * Configuration update.
 */
int	modified_conf(void);
void	update_conf(void);

/*
 * User interface.
 */
void	ui_notice(char *text);
void	ui_error_exit(char *text);

/*
 * Memory allocation.
 */
#include <stddef.h> /* picks up size_t */
void	*xmalloc(size_t n);
void	*xzmalloc(size_t n);
void	*xrealloc(void *p, size_t n);
char	*xstrdup(char *str);
void	xfree(void *p);
char	*strcats(char *s, ...);

/*
 * Misc.
 */
int	streq(char *a, char *b);
int	match(const char *a, const char *b);
void	print_num(char *bfrm, int val, int usint);
char	*dvc_home(void);

extern int dvc_fake_data;
extern int dvc_tmp_root;
extern int dvc_verbose;

#ifndef FALSE
#define	FALSE	0
#define	TRUE	1
#endif

#ifndef NULL
#define	NULL	0
#endif

/*
 * Devconfig specific interface routines
 */
char		*get_cat_name(int idx);
void		*get_cat_ui_info(int idx);
void		set_cat_ui_info(void *ui, int idx);

int		chk_num(char *, int);

attr_list_t	*get_dev_info(int idx);
char		*get_dev_name(int idx);
char		*get_dev_title(int idx);
int		get_dev_unit(int idx);
void		set_dev_modified(int idx);
attr_list_t	*get_typ_info(int idx);
void		*get_ui_info(int idx);
void		set_ui_info(void *ui, int idx);

int		valid_win_conf(void);
int 		is_xinside_attr(attr_list_t *alist);

#endif	/* DVC_DOT_H */
