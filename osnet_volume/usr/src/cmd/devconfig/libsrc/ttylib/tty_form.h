#ifndef TTY_FORM_H
#define	TTY_FORM_H

/*
 * Copyright (c) 1993 Sun Microsystems, Inc.  All Rights Reserved.
 * Sun considers its source code as an unpublished, proprietary trade secret,
 * and it is available only under strict license provisions.  This copyright
 * notice is placed here only to protect Sun in the event the source is
 * deemed a published work.  Dissassembly, decompilation, or other means of
 * reducing the object code to human readable form is prohibited by the
 * license agreement under which this code is provided to the user or company
 * in possession of this copy.
 *
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the Government
 * is subject to restrictions as set forth in subparagraph (c)(1)(ii) of the
 * Rights in Technical Data and Computer Software clause at DFARS 52.227-7013
 * and in similar clauses in the FAR and NASA FAR Supplement.
 */

#pragma	ident	"@(#)tty_form.h 1.5 94/02/17"

#include "tty_formtypes.h"

typedef struct tty_field Field;
struct tty_field {
	int		f_row;
	int		f_col;
	int		f_label_width;
	int		f_keys;
	Field_desc	*f_desc;
	struct tty_field *f_next;
	struct tty_field *f_prev;
};

typedef struct tty_form Form;
struct tty_form {
	int		err_y;
	Field		*fields;
	Field		*sum;
};

#define	FIELD_V_SPACING	1	/* vertical space between fields */
#define	FIELD_H_SPACING	1	/* space between label and field */

#define	INDENT_LEVEL	2	/* indentation level */

#define	ERR_ROWS	3	/* no. of error msg display rows in a form */

#define	TABSTOP		8		/* spaces per tab */

#define	INTRO_ONE_TIME_ONLY	0
#define	INTRO_ON_DEMAND		1

extern	int	form_common(char *, char *, Field_desc *, int,
						char *(*)(int, int, ...));

extern	Form	*form_create(char *, char *, Field_desc *, int);
extern	int	form_exec(Form *, char *(*)(int, int, ...));

extern	int 	form_intro(char *, char *, Field_help *, int);
extern  void	form_wintro(Field_help *);
extern	void	form_error(char *, char *, Field_help *);

#endif	/* !TTY_FORM_H */
