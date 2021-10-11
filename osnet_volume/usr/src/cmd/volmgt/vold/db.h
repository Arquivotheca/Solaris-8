/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ifndef	__DB_H
#define	__DB_H

#pragma ident	"@(#)db.h	1.9	95/10/20 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct dbops {
	void	(*dop_lookup)(vvnode_t *);
	void	(*dop_root)();
	bool_t	(*dop_update)(obj_t *);
	bool_t	(*dop_add)(obj_t *);
	bool_t	(*dop_remove)(obj_t *);
	vol_t	*(*dop_findlabel)(char *, label *);
	bool_t	(*dop_testkey)(char *, char *, char *);
	char	*dop_name;
	int	dop_pad[10];
};

/*
 * Database working operations.
 */
void 	db_lookup(vvnode_t *);
void	db_root();
bool_t 	db_update(obj_t *);
bool_t 	db_add(obj_t *);
bool_t 	db_remove(obj_t *);
vol_t	*db_findlabel(char *, label *);
bool_t	db_testkey(char *, char *, char *);
int	db_configured_cnt(void);

/*
 * Database initialization operations.
 */
int	db_load(char *);		/* load a new database object */
void	db_new(struct dbops *);		/* link in a dbops structure */

#define	DB_SYM	"db_init"

#ifdef	__cplusplus
}
#endif

#endif /* __DB_H */
