/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef __LABEL_H
#define	__LABEL_H

#pragma ident	"@(#)label.h	1.21	95/11/13 SMI"

#include <sys/types.h>
#include <rpc/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Header file for label manipulation
 */

/*
 * structure for handling labels
 */
typedef struct label {
	int 	l_type;		/* label type index (into labsw[]) */
	void	*l_label;	/* pointer to it */
} label;

enum laread_res { L_UNRECOG, L_UNFORMATTED, L_NOTUNIQUE, L_ERROR, L_FOUND };

struct label_loc {
	off_t	ll_off;		/* offset (in bytes) */
	size_t	ll_len;		/* length (in bytes) */
};

/*
 * This is the internal interface between the label modules and the
 * generic labeling code
 */
struct labsw {
	char		*(*l_key)(label *);
	bool_t		(*l_compare)(label *, label *);
	enum laread_res	(*l_read)(int, label *, struct devs *);
	int		(*l_write)(char *, label *);
	void		(*l_setup)(struct vol *);
	void		(*l_xdr)(label *, enum xdr_op, void **);
	size_t		l_size;		/* bytes of just label */
	size_t		l_xdrsize;	/* bytes of xdr'd label */
	char		*l_ident;	/* name of label */
	u_int		l_nll;		/* number of label locations */
	struct label_loc *l_ll;		/* array of label locations */
	char		**l_devlist;	/* devices this label live on */
	int		l_pad[10];	/* room to grow */
};

#define	MAX_LABELS	10

void		label_new(struct labsw *);	/* install a new label type */
bool_t		label_compare(label *, label *); /* TRUE if same */
char		*label_key(label *); 		/* return fairly unique key */
char		*label_ident(int);		/* string for label type */
int		label_type(char *);		/* label type from string */
enum laread_res	label_read(int, label *, struct devs *); /* read the label */
int		label_write(char *, label *); 	/* write the label to char * */
void		label_setup(label *, struct vol *, struct devs *);
void		label_xdr(label *, enum xdr_op, void **);
size_t		label_xdrsize(int);
struct labsw	*label_getlast(void);

/* the init routine to call in each "label_*.so" */
#define	LABEL_SYM	"label_init"

/* unnamed media names */
#define	UNNAMED_PREFIX	"unnamed_"

#ifdef	__cplusplus
}
#endif

#endif /* __LABEL_H */
