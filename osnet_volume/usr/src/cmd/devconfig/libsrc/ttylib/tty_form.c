/*LINTLIBRARY*/
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

#pragma	ident	"@(#)tty_form.c 1.13 94/02/17"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libintl.h>
#include <ctype.h>
#include <netdb.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/bitmap.h>
#include "tty_form.h"
#include "tty_utils.h"
#include "tty_help.h"

#include "ttylib_msgs.h"
#define	YES			TTYLIB_MSGS(TTYLIB_YES)
#define	NO			TTYLIB_MSGS(TTYLIB_NO)
#define	CONTINUE_TO_ADVANCE	TTYLIB_MSGS(TTYLIB_CONTINUE)

#define	ERR_SHOW_TEXT	0
#define	ERR_STATUS_ONLY	1

static Field	*next_field(Field *);
static Field	*prev_field(Field *);

static int	field_text(Field *);
static int	field_radio(Field *, int);
static int	field_confirm(Field *, int);

static void	field_label(int, int, char *, int, int, int *, int *);

static Callback_proc	field_help;
static int	field_instructions(Field *);
static int	field_validate(
			Field_desc *, int, int, int, char *(*)(int, int, ...));

static char	*menu_title(const char *string);

extern void	*xmalloc(size_t);

void
form_wintro( Field_help *help)
{
	wintro(field_help, help);
}

int
form_intro(char *title, char *text, Field_help *help, int force)
{
	int ch;
	static Field_desc intro[] = {
	    { FIELD_NONE, NULL, NULL, NULL, NULL, -1, -1, -1, -1, 
          FF_BYPASS, NULL }
	};
	static int	first_prompt = 1;
	Form		*form;

	intro[0].help = help;

	form = form_create(title, text, intro, 1);
	ch = form_exec(form, (char *(*)(int, int, ...))0);

	return (ch);
}

void
form_error(char *title, char *text, Field_help *help)
{
	static Field_desc err[] = {
	    { FIELD_NONE, NULL, NULL, NULL, NULL,
		-1, -1, -1, -1, FF_POPUP, NULL }
	};
	Form		*form;

	err[0].help = help;

	form = form_create(title, text, err, 1);
	(void) form_exec(form, (char *(*)(int, int, ...))0);
}

int
form_common(
	char		*title,
	char		*text,
	Field_desc	*fields,
	int		nfields,
	char		*(*err_to_string)(int, int, ...))
{
	Form	*form;
	int	ch;

	form = form_create(title, text, fields, nfields);
	ch = form_exec(form, err_to_string);

	return (ch);
}

Form *
form_create(char *title, char *text, Field_desc *fields, int nfields)
{
	Form	*form;
	Field	*fp;
	int	i, n, y;
	int	max;

	(void) start_curses();
	(void) clear();

	wheader(stdscr, title);

	y = 2;
	if (text) {
		n = mvwfmtw(stdscr,
			y, INDENT0, COLS - (2 * INDENT0),
			text);
		y += (n + 1);
	}
	form = (Form *)xmalloc(sizeof (Form));
	form->fields = (Field *)xmalloc(nfields * sizeof (Field));
	/*
	 * Pass 1:
	 *
	 *	1)  Determine the maximum label width of
	 *	    the aligned fields.  All labels for
	 *	    aligned fields will be padded to this
	 *	    width and aligned on their right margains.
	 *
	 *	2) Determine whether a "summary field" is
	 *	   present.  A summary field is an insensitive
	 *	   field that gets updated whenever any of the
	 *	   form's sensitive fields are modified.  The
	 *	   summary field's Validate_proc is used for
	 *	   this purpose.
	 *
	 *	3) Determine were errors will be displayed.
	 */
	form->sum = (Field *)0;
	max = 0;
	fp = form->fields;
	for (i = 0; i < nfields; i++) {
		Field_desc *f = &fields[i];
		int	label_len;

		if (i > 0)
			fp->f_prev = fp - 1;
		if (i < (nfields - 1))
			fp->f_next = fp + 1;
		fp->f_desc = f;
		fp->f_row = y;
		fp->f_col = INDENT0;
		if (f->flags & FF_POPUP)
			fp->f_keys = F_HELP | F_OKEYDOKEY;
		else
			fp->f_keys = F_HELP | F_CONTINUE;

		if (f->flags & FF_CANCEL)
			fp->f_keys |= F_CANCEL;

		if (f->flags & FF_CHANGE)
			fp->f_keys |= F_CHANGE;

		if (f->flags & FF_GOBACK)
			fp->f_keys |= F_GOBACK;

		if (f->flags & FF_BYPASS)
			fp->f_keys |= F_BYPASS;

		if (f->label) {
			label_len = strlen(f->label);
			if ((f->flags & FF_LAB_ALIGN) && label_len > max)
				max = label_len;
		}
		if ((f->flags & FF_RDONLY) && (f->flags & FF_SUMMARY)) {
			if (f->validate != (Validate_proc *)0)
				form->sum = fp;
			y++;		/* double-space */
		}
		if (f->flags & FF_ENDGROUP)
			y++;
		y++;
		fp++;
	}
	form->err_y = y + 2;
	/*
	 * Finish linked list linkage
	 */
	form->fields[0].f_prev = &form->fields[nfields - 1];
	form->fields[nfields - 1].f_next = &form->fields[0];
	/*
	 * Pass 2:
	 *
	 *	Print the form's labels and initial values
	 *	if there is more than one field.
	 */
	if (nfields > 1) {
		fp = form->fields;
		do {
			Field_desc *f = fp->f_desc;
			int	oflags;

			fp->f_label_width = max;

			oflags = f->flags;
			f->flags |= FF_RDONLY;

			switch (f->type) {
			case FIELD_TEXT:
				(void) field_text(fp);
				break;
			case FIELD_EXCLUSIVE_CHOICE:
				/* XXX always last and handled in form_exec */
				break;
			case FIELD_CONFIRM:
				(void) field_confirm(fp,
						form->err_y - fp->f_row);
				break;
			}
			f->flags = oflags;
			fp = fp->f_next;
		} while (fp != form->fields);
	} else
		form->fields->f_label_width = max;

	return (form);
}

int
form_exec(Form *form, char *(*err_to_string)(int, int, ...))
{
	Field	*fp;
	int	last_y = LINES - 1;
	int	errcode;
	int	ch;

	fp = form->fields;
	for (;;) {
		Field_desc *f = fp->f_desc;

		if ((f->flags & FF_RDONLY) == 0)
			last_y = field_instructions(fp);

		switch (f->type) {
		case FIELD_TEXT:
			ch = field_text(fp);
			break;
		case FIELD_EXCLUSIVE_CHOICE:
			ch = field_radio(fp, last_y - fp->f_row);
			break;
		case FIELD_CONFIRM:
			ch = field_confirm(fp, last_y - fp->f_row);
			break;
		default:
			wcursor_hide(stdscr);
			ch = wzgetch(stdscr, fp->f_keys);
			break;
		}
		/*
		 * Update summary field, if present.
		 */
		if ((f->flags & FF_RDONLY) == 0 &&
		    fp != form->sum && form->sum != (Field *)0) {
			(void) form->sum->f_desc->validate(form->sum->f_desc);
			field_text(form->sum);
		}
		/*
		 * Use output of field processing routine
		 * to determine to which field we are to
		 * navigate next.
		 */
		if (sel_cmd(ch)) {
			/*
			 * Get a handle on the next
			 * *traversable* field
			 */
			Field	*next = next_field(fp);

			/*
			 * Check field with focus
			 */
			errcode = field_validate(f,
					form->err_y, last_y,
					ERR_SHOW_TEXT,
					err_to_string);

			if (errcode == 0) {	/* focus field OK */
				/*
				 * Put up "Continue" message when
				 * user successfully completes the
				 * entire form (i.e., the last field).
				 */
				if (f->type == FIELD_TEXT &&
				    fp->f_next == form->fields) {
					Field	*fp2;
					/*
					 * Check to see whether all [other
					 * traversable] fields are OK.
					 */
					for (fp2 = next;
					    fp2 != fp && errcode == 0;
					    fp2 = next_field(fp2)) {
						errcode = field_validate(
							fp2->f_desc,
							form->err_y, last_y,
							ERR_STATUS_ONLY,
							err_to_string);
					}
					if (errcode == 0)	/* all OK! */
						(void) mvwfmtw(stdscr,
						    form->err_y, INDENT0,
						    COLS - (2 * INDENT0),
						    CONTINUE_TO_ADVANCE);
				}
				/*
				 * Move focus forward since field with focus
				 * was entered successfully -- other fields
				 * may still have errors.
				 */
				if (next == fp) {
					beep();
				} else
					fp = next;
			}
		} else if (fwd_cmd(ch)) {
			Field	*next = next_field(fp);

			if (next == fp) {
				beep();
			} else
				fp = next;
		} else if (bkw_cmd(ch)) {
			Field	*prev = prev_field(fp);

			if (prev == fp) {
				beep();
			} else
				fp = prev;
		} else if (is_continue(ch)) {
			Field	*fp2 = fp;
			/*
			 * Allow user to leave form only when
			 * all fields have been validated.
			 */
			do {
				errcode = field_validate(
						fp2->f_desc,
						form->err_y, last_y,
						ERR_SHOW_TEXT, err_to_string);
				if (errcode != 0)
					break;
				fp2 = fp2->f_next;
			} while (fp2 != fp);
			if (errcode == 0)
				break;
			fp = fp2;	/* set focus on "bad" field */
		} else if ((fp->f_keys & F_CANCEL) && is_cancel(ch)) {
			break;
		} else if ((fp->f_keys & F_CHANGE) && is_change(ch)) {
			break;
		} else if ((fp->f_keys & F_GOBACK) && is_goback(ch)) {
			break;
		} else if ((fp->f_keys & F_BYPASS) && is_bypass(ch)) {
			break;
		} else if (is_help(ch)) {
			(void) field_help((void *)f->help, (void *)0);
		} else if (!esc_cmd(ch))
			beep();
	}
	free(form->fields);	/* NB: array with linked list semantics */
	free(form);

	return (ch);
}

static Field *
next_field(Field *fp)
{
	Field	*next;

	for (next = fp->f_next; next != fp &&
	    (next->f_desc->flags & FF_RDONLY);
	    next = next->f_next)
		;
	return (next);
}

static Field *
prev_field(Field *fp)
{
	Field	*prev;

	for (prev = fp->f_prev; prev != fp &&
	    (prev->f_desc->flags & FF_RDONLY);
	    prev = prev->f_prev)
		;
	return (prev);
}

static int
field_text(Field *fp)
{
	Field_desc	*f = fp->f_desc;
	int	value_x, value_y;
	int	value_length;
	int	field_length;
	int	ch;

	field_label(fp->f_row, fp->f_col,
		f->label, fp->f_label_width, f->flags,
		&value_y, &value_x);

	value_x += FIELD_H_SPACING;

	field_length = COLS - (INDENT0 + value_x);

	if (f->field_length != -1 && f->field_length < field_length)
		field_length = f->field_length;

	if ((f->flags & FF_RDONLY) == 0) {
		ch = mvwgets(stdscr, value_y, value_x, field_length,
		    field_help, (void *)f->help,
		    (char *)f->value, f->value_length,
		    (f->flags & FF_TYPE_TO_WIPE) ? TRUE : FALSE,
		    fp->f_keys);
	} else {
		value_length = strlen(f->value);
		if (value_length > field_length) {
			value_length = field_length - 1;
			(void) mvwprintw(stdscr, value_y, value_x,
				"%-*.*s>",
				value_length, value_length, f->value);
		} else
			(void) mvwprintw(stdscr, value_y, value_x,
				"%s", f->value);
		ch = RETURN;
	}
	return (ch);
}

static int
field_radio(Field *fp, int height)
{
	Field_desc *f = fp->f_desc;
	Menu	*menu = (Menu *)f->value;
	int	flags;
	int	ch;

	flags = M_RADIO | M_RADIO_ALWAYS_ONE | M_CHOICE_REQUIRED;
	if (f->flags & FF_RDONLY)
		flags |= M_READ_ONLY;

	ch = wmenu(stdscr, fp->f_row, fp->f_col,
		height, COLS - (fp->f_col + INDENT0),
		field_help, (void *)f->help,		/* help proc and arg */
		(Callback_proc *)0, (void *)0,		/* select proc */
		(Callback_proc *)0, (void *)0,		/* deselect proc */
		menu_title(f->label),
		menu->labels, (size_t)menu->nitems, (void *)&menu->selected,
		flags, fp->f_keys);

	return (ch);
}

static int
field_confirm(Field *fp, int height)
{
	Field_desc *f = fp->f_desc;
	static	char *yesorno[2];
	int	answer;
	int	flags;
	int	ch;

	if (yesorno[0] == (char *)0) {
		yesorno[0] = YES;
		yesorno[1] = NO;
	}

	switch ((int)f->value) {
	case TRUE:
		answer = 0;
		break;
	case FALSE:
	default:
		answer = 1;
		break;
	}

	flags = M_RADIO | M_RADIO_ALWAYS_ONE;
	if (f->flags & FF_RDONLY)
		flags |= M_READ_ONLY;

	ch = wmenu(stdscr, fp->f_row, fp->f_col,
		height, COLS - fp->f_col - INDENT0,
		field_help, (void *)f->help,		/* help proc and arg */
		(Callback_proc *)0, (void *)0,		/* select proc */
		(Callback_proc *)0, (void *)0,		/* deselect proc */
		menu_title(f->label),
		yesorno, 2, (void *)&answer,
		flags, fp->f_keys);

	if (answer == 0)
		f->value = (void *)TRUE;
	else
		f->value = (void *)FALSE;

	return (ch);
}

static void
field_label(int	y,
	int	x,	/* base column */
	char	*text,	/* l10n'ed label text */
	int	width,	/* width of allocated space for label */
	int	flags,	/* left-, right-justification, etc. */
	int	*v_y,	/* RETURN:  y where value should start */
	int	*v_x)	/* RETURN:  x where value should start */
{
	int	label_len;
	int	label_x;

	if (text == (char *)0) {
		if (flags & FF_LAB_ALIGN && x != -1)
			*v_x = x + INDENT_LEVEL;
		else
			*v_x = x;
		*v_y = y;
		return;
	}

	label_len = strlen(text);

	if (flags & FF_LAB_ALIGN)
		label_x = x + INDENT_LEVEL;
	else {
		label_x = x;
		width = label_len;
	}

	if (width > (COLS - (label_x + INDENT0))) {
		/*
		 * The following code is mainly for l10n'ed
		 * questions that overflow a line.  We
		 * essentially ignore any formatting info
		 * (i.e., justification and centering).
		 */
		char	**text_lines, **cpp;
		int	n = 0;

		text_lines = format_text(text, COLS - (label_x + INDENT0));
		for (cpp = text_lines; *cpp != (char *)0; cpp++) {
			(void) mvwprintw(stdscr, y + n, label_x, "%s", *cpp);
			(void) wclrtoeol(stdscr);
			n++;
		}
		if (cpp > text_lines)
			cpp--;			/* go back to last line */
		if (*cpp == (char *)0)
			width = 0;
		else
			width = strlen(*cpp);
		if (n > 0)
			*v_y = y + (n - 1);	/* no "auto-newline" */
		else
			*v_y = y;
		free(text_lines[0]);
		free(text_lines);
	} else {
		/*
		 * This is the code used most of the
		 * time to print labels.
		 */
		if (flags & FF_LAB_RJUST)
			(void) mvwprintw(stdscr, y, label_x,
				"%*.*s", width, width, text);
		else if (flags & FF_LAB_CENTER)
			(void) mvwprintw(stdscr,
				y, label_x + ((width - label_len) / 2),
				"%s", text);
		else	/* left-justify by default */
			(void) mvwprintw(stdscr, y, label_x,
				"%-*.*s", width, width, text);
		wclrtoeol(stdscr);
		*v_y = y;
	}
	*v_x = label_x + width;
}

/*ARGSUSED*/
static int
field_help(void *client_data, void *call_data)
{
	Field_help *help = (Field_help *)client_data;

	if (help != (Field_help *)0)
		do_help_index(stdscr, HELP_TOPIC, help->topics);
	else
		do_help_index(stdscr, HELP_NONE, (char *)0);

	return (1);
}


static int
field_instructions(Field *fp)
{
	wfooter(stdscr, fp->f_keys);

	return (LINES - 3);
}

static int
field_validate(Field_desc *f,
	int	err_y,
	int	last_y,
	int	mode,
	char	*(*err_to_string)(int, int, ...))
{
	int	errcode;
	char	*errstr;
	int	n, y;

	if (f->validate != (Validate_proc *)0)
		errcode = f->validate(f);
	else
		errcode = 0;
	if (errcode != 0 && mode == ERR_SHOW_TEXT) {
		errstr = err_to_string(errcode, 1, f->value);
		n =  mvwfmtw(stdscr,
			err_y, INDENT0,
			COLS - (2 * INDENT0),
			errstr);
		beep();
	} else
		n = 0;

	for (y = err_y + n; y < last_y; y++) {
		(void) wmove(stdscr, y, INDENT0);
		(void) wclrtoeol(stdscr);
	}
	wrefresh(stdscr);
	return (errcode);
}

/*
 * Menu titles can't end in colons
 *
 * NB:  if you need the returned value to be
 * persistent, you'd better strdup it!
 */
static char *
menu_title(const char *string)
{
	static char buf[256];
	char	*cp;

	(void) strncpy(buf, string, sizeof (buf));
	buf[sizeof (buf) - 1] = '\0';

	cp = strrchr(buf, ':');
	if (cp != (char *)0)
		*cp = '\0';

	return (buf);
}
