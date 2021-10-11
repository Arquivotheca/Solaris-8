/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_termio.c	1.2	99/11/19 SMI"

/*
 * Terminal I/O Backend
 *
 * Terminal editing backend for standard input.  The terminal i/o backend is
 * actually built on top of two other i/o backends: one for raw input and
 * another for raw output (presumably stdin and stdout).  When IOP_READ is
 * invoked, the terminal backend enters a read-loop in which it can perform
 * command-line editing and access a history buffer.  Once a newline is read,
 * the entire buffered command-line is returned to the caller.
 */

#include <setjmp.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>

#include <mdb/mdb_types.h>
#include <mdb/mdb_cmdbuf.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_io_impl.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_signal.h>
#include <mdb/mdb_stdlib.h>
#include <mdb/mdb_string.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_frame.h>
#include <mdb/mdb.h>

#ifdef ERR
#undef ERR
#endif

#include <curses.h>

#define	KEY_ESC	(0x01b)			/* Escape key code */
#define	META(c)	((c) | 0x080)		/* Convert 'x' to 'M-x' */
#define	KPAD(c) ((c) | 0x100)		/* Convert 'x' to 'ESC-[-x' */

#define	TIO_DEFAULT_ROWS	24	/* Default number of rows */
#define	TIO_DEFAULT_COLS	80	/* Default number of columns */

typedef union termio_attr_val {
	const char *at_str;		/* String value */
	int at_val;			/* Integer or boolean value */
} termio_attr_val_t;

typedef struct termio_info {
	termio_attr_val_t ti_dch1;	/* Delete character */
	termio_attr_val_t ti_cub1;	/* Move back one space */
	termio_attr_val_t ti_cuf1;	/* Move forward one space */
	termio_attr_val_t ti_cuu1;	/* Move up one line */
	termio_attr_val_t ti_cud1;	/* Move down one line */
	termio_attr_val_t ti_el;	/* Clear to end-of-line */
	termio_attr_val_t ti_am;	/* Automatic right margin? */
	termio_attr_val_t ti_bw;	/* Backward motion at left edge? */
	termio_attr_val_t ti_xenl;	/* Newline ignored after 80 cols? */
	termio_attr_val_t ti_cols;	/* # of columns */
	termio_attr_val_t ti_lines;	/* # of rows */
	termio_attr_val_t ti_smso;	/* Set standout mode */
	termio_attr_val_t ti_rmso;	/* Remove standout mode */
	termio_attr_val_t ti_smul;	/* Set underline mode */
	termio_attr_val_t ti_rmul;	/* Remove underline mode */
	termio_attr_val_t ti_smacs;	/* Set alternate character set */
	termio_attr_val_t ti_rmacs;	/* Remove alternate character set */
	termio_attr_val_t ti_smcup;	/* Set mode where cup is active */
	termio_attr_val_t ti_rmcup;	/* Remove mode where cup is active */
	termio_attr_val_t ti_rev;	/* Set reverse video mode */
	termio_attr_val_t ti_bold;	/* Set bold text mode */
	termio_attr_val_t ti_dim;	/* Set dim text mode */
	termio_attr_val_t ti_sgr0;	/* Remove all video attributes */
	termio_attr_val_t ti_ich1;	/* Insert character */
	termio_attr_val_t ti_clear;	/* Clear screen and home cursor */
	termio_attr_val_t ti_cnorm;	/* Make cursor appear normal */
} termio_info_t;

typedef enum {
	TIO_ATTR_STR,			/* String attribute */
	TIO_ATTR_BOOL,			/* Boolean attribute */
	TIO_ATTR_INT			/* Integer attribute */
} termio_attr_type_t;

typedef struct termio_attr {
	const char *ta_name;		/* Capability name */
	termio_attr_type_t ta_type;	/* Capability type */
	termio_attr_val_t *ta_valp;	/* String pointer location */
} termio_attr_t;

struct termio_data;
typedef const char *(*keycb_t)(struct termio_data *, int);

#define	TIO_FINDHIST	0x01		/* Find-history-mode */
#define	TIO_AUTOWRAP	0x02		/* Terminal has autowrap */
#define	TIO_BACKLEFT	0x04		/* Terminal can go back at left edge */
#define	TIO_INSERT	0x08		/* Terminal has insert mode */
#define	TIO_USECUP	0x10		/* Use smcup/rmcup sequences */

typedef struct termio_data {
	mdb_io_t *tio_io;		/* Pointer back to containing i/o */
	mdb_io_t *tio_out_io;		/* Terminal output backend */
	mdb_io_t *tio_in_io;		/* Terminal input backend */
	mdb_iob_t *tio_out;		/* I/o buffer for terminal output */
	mdb_iob_t *tio_in;		/* I/o buffer for terminal input */
	mdb_iob_t *tio_link;		/* I/o buffer to resize on WINCH */
	keycb_t tio_keymap[KEY_MAX];	/* Keymap (callback functions) */
	mdb_cmdbuf_t tio_cmdbuf;	/* Editable command-line buffer */
	struct termios tio_otios;	/* Saved terminal settings */
	struct termios tio_ntios;	/* Modified terminal settings */
	jmp_buf tio_env;		/* Read loop setjmp(3c) environment */
	termio_info_t tio_info;		/* Terminal attribute strings */
	char *tio_attrs;		/* Attribute string buffer */
	size_t tio_attrslen;		/* Length in bytes of tio_attrs */
	const char *tio_prompt;		/* Prompt string for this read */
	size_t tio_promptlen;		/* Length of prompt string */
	size_t tio_rows;		/* Terminal height */
	size_t tio_cols;		/* Terminal width */
	size_t tio_x;			/* Cursor x coordinate */
	size_t tio_y;			/* Cursor y coordinate */
	size_t tio_max_x;		/* Previous maximum x coordinate */
	size_t tio_max_y;		/* Previous maximum y coordinate */
	int tio_intr;			/* Interrupt char */
	int tio_quit;			/* Quit char */
	int tio_erase;			/* Erase char */
	int tio_kill;			/* Kill char */
	int tio_eof;			/* End-of-file char */
	int tio_susp;			/* Suspend char */
	uint_t tio_flags;		/* Miscellaneous flags */
	volatile mdb_bool_t tio_active;	/* Flag denoting read loop active */
} termio_data_t;

static ssize_t termio_read(mdb_io_t *, void *, size_t);
static ssize_t termio_write(mdb_io_t *, const void *, size_t);
static off64_t termio_seek(mdb_io_t *, off64_t, int);
static int termio_ctl(mdb_io_t *, int, void *);
static void termio_close(mdb_io_t *);
static const char *termio_name(mdb_io_t *);
static void termio_link(mdb_io_t *, mdb_iob_t *);
static void termio_unlink(mdb_io_t *, mdb_iob_t *);
static ssize_t termio_attrstr(mdb_io_t *, int, uint_t, char *);

static void termio_addch(termio_data_t *, char, size_t);
static void termio_mvcur(termio_data_t *);
static void termio_bspch(termio_data_t *);
static void termio_delch(termio_data_t *);
static void termio_clear(termio_data_t *);
static void termio_redraw(termio_data_t *);
static void termio_prompt(termio_data_t *);

static const char *termio_insert(termio_data_t *, int);
static const char *termio_accept(termio_data_t *, int);
static const char *termio_backspace(termio_data_t *, int);
static const char *termio_delchar(termio_data_t *, int);
static const char *termio_fwdchar(termio_data_t *, int);
static const char *termio_backchar(termio_data_t *, int);
static const char *termio_transpose(termio_data_t *, int);
static const char *termio_home(termio_data_t *, int);
static const char *termio_end(termio_data_t *, int);
static const char *termio_fwdword(termio_data_t *, int);
static const char *termio_backword(termio_data_t *, int);
static const char *termio_kill(termio_data_t *, int);
static const char *termio_reset(termio_data_t *, int);
static const char *termio_prevhist(termio_data_t *, int);
static const char *termio_nexthist(termio_data_t *, int);
static const char *termio_findhist(termio_data_t *, int);
static const char *termio_refresh(termio_data_t *, int);
static const char *termio_intr(termio_data_t *, int);
static const char *termio_quit(termio_data_t *, int);
static const char *termio_susp(termio_data_t *, int);

static void termio_winch(int, siginfo_t *, ucontext_t *, void *);
static void termio_tstp(int, siginfo_t *, ucontext_t *, void *);

extern const char *tigetstr(const char *);
extern int tigetflag(const char *);
extern int tigetnum(const char *);

static const mdb_io_ops_t termio_ops = {
	termio_read,
	termio_write,
	termio_seek,
	termio_ctl,
	termio_close,
	termio_name,
	termio_link,
	termio_unlink,
	termio_attrstr
};

static termio_info_t termio_info;

static termio_attr_t termio_attrs[] = {
	{ "dch1", TIO_ATTR_STR, &termio_info.ti_dch1 },
	{ "cub1", TIO_ATTR_STR, &termio_info.ti_cub1 },
	{ "cuf1", TIO_ATTR_STR, &termio_info.ti_cuf1 },
	{ "cuu1", TIO_ATTR_STR, &termio_info.ti_cuu1 },
	{ "cud1", TIO_ATTR_STR, &termio_info.ti_cud1 },
	{ "el", TIO_ATTR_STR, &termio_info.ti_el },
	{ "am", TIO_ATTR_BOOL, &termio_info.ti_am },
	{ "bw", TIO_ATTR_BOOL, &termio_info.ti_bw },
	{ "xenl", TIO_ATTR_BOOL, &termio_info.ti_xenl },
	{ "cols", TIO_ATTR_INT, &termio_info.ti_cols },
	{ "lines", TIO_ATTR_INT, &termio_info.ti_lines },
	{ "smso", TIO_ATTR_STR, &termio_info.ti_smso },
	{ "rmso", TIO_ATTR_STR, &termio_info.ti_rmso },
	{ "smul", TIO_ATTR_STR, &termio_info.ti_smul },
	{ "rmul", TIO_ATTR_STR, &termio_info.ti_rmul },
	{ "smacs", TIO_ATTR_STR, &termio_info.ti_smacs },
	{ "rmacs", TIO_ATTR_STR, &termio_info.ti_rmacs },
	{ "smcup", TIO_ATTR_STR, &termio_info.ti_smcup },
	{ "rmcup", TIO_ATTR_STR, &termio_info.ti_rmcup },
	{ "rev", TIO_ATTR_STR, &termio_info.ti_rev },
	{ "bold", TIO_ATTR_STR, &termio_info.ti_bold },
	{ "dim", TIO_ATTR_STR, &termio_info.ti_dim },
	{ "sgr0", TIO_ATTR_STR, &termio_info.ti_sgr0 },
	{ "ich1", TIO_ATTR_STR, &termio_info.ti_ich1 },
	{ "clear", TIO_ATTR_STR, &termio_info.ti_clear },
	{ "cnorm", TIO_ATTR_STR, &termio_info.ti_cnorm },
	{ NULL, NULL, NULL }
};

static void
termio_enter(termio_data_t *td)
{
	if ((td->tio_flags & TIO_USECUP) && td->tio_info.ti_smcup.at_str) {
		mdb_iob_puts(td->tio_out, td->tio_info.ti_smcup.at_str);
		mdb_iob_flush(td->tio_out);

		if (td->tio_info.ti_clear.at_str) {
			mdb_iob_puts(td->tio_out, td->tio_info.ti_clear.at_str);
			mdb_iob_flush(td->tio_out);
		}
	}

	if (td->tio_info.ti_cnorm.at_str) {
		mdb_iob_puts(td->tio_out, td->tio_info.ti_cnorm.at_str);
		mdb_iob_flush(td->tio_out);
	}
}

static void
termio_exit(termio_data_t *td)
{
	/*
	 * Temporary hack for baud rate delays; see comments about strstr("$<")
	 * in the termio_attrstr() code below.
	 */
	if (td->tio_info.ti_sgr0.at_str &&
	    strstr(td->tio_info.ti_sgr0.at_str, "$<") == NULL) {
		mdb_iob_puts(td->tio_out, td->tio_info.ti_sgr0.at_str);
		mdb_iob_flush(td->tio_out);
	}

	if ((td->tio_flags & TIO_USECUP) && td->tio_info.ti_rmcup.at_str) {
		mdb_iob_puts(td->tio_out, td->tio_info.ti_rmcup.at_str);
		mdb_iob_flush(td->tio_out);
	}
}

static ssize_t
termio_read(mdb_io_t *io, void *buf, size_t nbytes)
{
	termio_data_t *td = io->io_data;

	mdb_bool_t esc = FALSE, pad = FALSE;
	ssize_t rlen = 0;
	int c, sig;

	const char *s;
	size_t len;

	if (io->io_next != NULL)
		return (IOP_READ(io->io_next, buf, nbytes));

	if (termio_ctl(td->tio_io, TCSETS, &td->tio_ntios) == -1)
		warn("failed to set terminal attributes");

	if (nbytes == 1) {
		if ((c = mdb_iob_getc(td->tio_in)) == EOF)
			goto out;

		*((uchar_t *)buf) = (uchar_t)c;

		rlen = 1;
		goto out;
	}

	td->tio_prompt = mdb.m_prompt;
	td->tio_promptlen = mdb.m_promptlen;

	termio_prompt(td);

	/*
	 * We need to redraw the entire command-line and restart our read loop
	 * in the event of a SIGWINCH or resume following SIGTSTP (SIGCONT).
	 * If sig == SIGCONT, we also need to restore our terminal settings
	 * and re-install our signal handlers.
	 */
	if ((sig = setjmp(td->tio_env)) != 0) {
		if (sig == SIGCONT) {
			struct winsize winsz;

			if (termio_ctl(td->tio_io, TCGETS, &td->tio_otios) < 0)
				warn("failed to get terminal attributes");

			if (termio_ctl(td->tio_io, TCSETS, &td->tio_ntios) < 0)
				warn("failed to reset terminal attributes");

			if (termio_ctl(td->tio_io, TIOCGWINSZ, &winsz) == 0) {
				if (winsz.ws_row != 0)
					td->tio_rows = (size_t)winsz.ws_row;
				if (winsz.ws_col != 0)
					td->tio_cols = (size_t)winsz.ws_col;
			}
		}

		td->tio_active = FALSE;
		td->tio_x = td->tio_y = 0;

		len = td->tio_cmdbuf.cmd_buflen + td->tio_promptlen;
		td->tio_max_x = len % td->tio_cols;
		td->tio_max_y = len / td->tio_cols;

		esc = pad = FALSE;

		mdb_iob_putc(td->tio_out, '\r');
		mdb_iob_flush(td->tio_out);
		termio_redraw(td);

		if (sig == SIGCONT) {
			(void) mdb_signal_sethandler(SIGWINCH,
			    termio_winch, td);
			(void) mdb_signal_sethandler(SIGTSTP,
			    termio_tstp, td);
		}
	}

	/*
	 * Since we're about to start the read loop, we know our linked iob
	 * is quiescent. We can now safely resize it to the latest term size.
	 */
	if (td->tio_link != NULL)
		mdb_iob_resize(td->tio_link, td->tio_rows, td->tio_cols);

	td->tio_active = TRUE;

	do {
char_loop:
		if ((c = mdb_iob_getc(td->tio_in)) == EOF) {
			td->tio_active = FALSE;
			goto out;
		}

		if (c == KEY_ESC && esc == FALSE) {
			esc = TRUE;
			goto char_loop;
		}

		if (esc) {
			esc = FALSE;

			if (c == '[') {
				pad++;
				goto char_loop;
			}

			c = META(c);
		}

		if (pad) {
			c = KPAD(CTRL(c));
			pad = FALSE;
		}

		len = td->tio_cmdbuf.cmd_buflen + td->tio_promptlen;

		td->tio_max_x = len % td->tio_cols;
		td->tio_max_y = len / td->tio_cols;

	} while ((s = (*td->tio_keymap[c])(td, c)) == NULL);

	td->tio_active = FALSE;
	mdb_iob_nl(td->tio_out);

	if ((rlen = strlen(s)) >= nbytes - 1)
		rlen = nbytes - 1;

	(void) strncpy(buf, s, rlen);
	((char *)buf)[rlen++] = '\n';

out:
	if (termio_ctl(td->tio_io, TCSETS, &td->tio_otios) == -1)
		warn("failed to restore terminal attributes");

	return (rlen);
} 

static ssize_t
termio_write(mdb_io_t *io, const void *buf, size_t nbytes)
{
	termio_data_t *td = io->io_data;

	if (io->io_next != NULL)
		return (IOP_WRITE(io->io_next, buf, nbytes));

	return (IOP_WRITE(td->tio_out_io, buf, nbytes));
}

/*ARGSUSED*/
static off64_t
termio_seek(mdb_io_t *io, off64_t offset, int whence)
{
	return (set_errno(ENOTSUP));
}

static int
termio_ctl(mdb_io_t *io, int req, void *arg)
{
	termio_data_t *td = io->io_data;

	if (io->io_next != NULL)
		return (IOP_CTL(io->io_next, req, arg));

	return (IOP_CTL(td->tio_in_io, req, arg));
}

static void
termio_close(mdb_io_t *io)
{
	termio_data_t *td = io->io_data;

	(void) mdb_signal_sethandler(SIGWINCH, SIG_DFL, NULL);
	(void) mdb_signal_sethandler(SIGTSTP, SIG_DFL, NULL);

	termio_exit(td);

	if (termio_ctl(io, TCSETS, &td->tio_otios) == -1)
		warn("failed to restore terminal attributes");

	if (td->tio_attrs)
		mdb_free(td->tio_attrs, td->tio_attrslen);

	mdb_cmdbuf_destroy(&td->tio_cmdbuf);

	mdb_iob_destroy(td->tio_out);
	mdb_iob_destroy(td->tio_in);

	mdb_free(td, sizeof (termio_data_t));
}

static const char *
termio_name(mdb_io_t *io)
{
	termio_data_t *td = io->io_data;

	if (io->io_next != NULL)
		return (IOP_NAME(io->io_next));

	return (IOP_NAME(td->tio_in_io));
}

static void
termio_link(mdb_io_t *io, mdb_iob_t *iob)
{
	termio_data_t *td = io->io_data;

	if (io->io_next == NULL) {
		mdb_iob_resize(iob, td->tio_rows, td->tio_cols);
		td->tio_link = iob;
	} else
		IOP_LINK(io->io_next, iob);
}

static void
termio_unlink(mdb_io_t *io, mdb_iob_t *iob)
{
	termio_data_t *td = io->io_data;

	if (io->io_next == NULL) {
		if (td->tio_link == iob)
			td->tio_link = NULL;
	} else
		IOP_UNLINK(io->io_next, iob);
}

static ssize_t
termio_attrstr(mdb_io_t *io, int req, uint_t attrs, char *buf)
{
	termio_data_t *td = io->io_data;

	if (io->io_next != NULL)
		return (IOP_ATTRSTR(io->io_next, req, attrs, buf));

	if ((req != ATT_ON && req != ATT_OFF) || (attrs & ~ATT_ALL) != 0)
		return (-1);

	buf[0] = 0;

	/*
	 * Temporary hack for baud rate delays: if the attribute string has an
	 * embedded delay ("$<n>"), pretend this attribute is not supported.
	 * We need to fix this in the future to actually support delays.
	 */
#define	APPEND(at, f) if ((attrs & (at)) && td->tio_info.f.at_str != NULL && \
	strstr(td->tio_info.f.at_str, "$<") == NULL) \
	    (void) strcat(buf, td->tio_info.f.at_str)

	if (req == ATT_ON) {
		APPEND(ATT_STANDOUT, ti_smso);
		APPEND(ATT_UNDERLINE, ti_smul);
		APPEND(ATT_REVERSE, ti_rev);
		APPEND(ATT_BOLD, ti_bold);
		APPEND(ATT_DIM, ti_dim);
		APPEND(ATT_ALTCHARSET, ti_smacs);

	} else {
		APPEND(ATT_STANDOUT, ti_rmso);
		APPEND(ATT_UNDERLINE, ti_rmul);
		APPEND(ATT_ALTCHARSET, ti_rmacs);

		if ((attrs & (ATT_REVERSE | ATT_BOLD | ATT_DIM)) &&
		    (td->tio_info.ti_sgr0.at_str != NULL))
			(void) strcat(buf, td->tio_info.ti_sgr0.at_str);
	}

#undef APPEND

	return (strlen(buf));
}

static void
termio_addch(termio_data_t *td, char c, size_t width)
{
	if (width == 1) {
		mdb_iob_putc(td->tio_out, c);
		td->tio_x++;

		if (td->tio_x >= td->tio_cols) {
			mdb_iob_putc(td->tio_out, '\r');
			mdb_iob_nl(td->tio_out);
			td->tio_x = 0;
			td->tio_y++;
		}

		mdb_iob_flush(td->tio_out);
	} else
		termio_redraw(td);
}

static void
termio_mvcur(termio_data_t *td)
{
	size_t tipos = td->tio_cmdbuf.cmd_bufidx + td->tio_promptlen;
	size_t dst_x = tipos % td->tio_cols;
	size_t dst_y = tipos / td->tio_cols;

	const char *str;
	size_t cnt, i;

	if (td->tio_y != dst_y) {
		if (td->tio_y < dst_y) {
			str = td->tio_info.ti_cud1.at_str;
			cnt = dst_y - td->tio_y;
			td->tio_x = 0; /* Note: cud1 moves cursor to column 0 */
		} else {
			str = td->tio_info.ti_cuu1.at_str;
			cnt = td->tio_y - dst_y;
		}

		for (i = 0; i < cnt; i++)
			mdb_iob_puts(td->tio_out, str);

		mdb_iob_flush(td->tio_out);
		td->tio_y = dst_y;
	}

	if (td->tio_x != dst_x) {
		if (td->tio_x < dst_x) {
			str = td->tio_info.ti_cuf1.at_str;
			cnt = dst_x - td->tio_x;
		} else {
			str = td->tio_info.ti_cub1.at_str;
			cnt = td->tio_x - dst_x;
		}

		for (i = 0; i < cnt; i++)
			mdb_iob_puts(td->tio_out, str);

		mdb_iob_flush(td->tio_out);
		td->tio_x = dst_x;
	}
}

static void
termio_bspch(termio_data_t *td)
{
	size_t i;

	if (td->tio_x == 0) {
		if (td->tio_flags & TIO_BACKLEFT)
			mdb_iob_putc(td->tio_out, '\b');
		else {
			mdb_iob_puts(td->tio_out, td->tio_info.ti_cuu1.at_str);

			for (i = 0; i < td->tio_cols - 1; i++) {
				mdb_iob_puts(td->tio_out,
				    td->tio_info.ti_cuf1.at_str);
			}
		}

		td->tio_x = td->tio_cols - 1;
		td->tio_y--;
	} else {
		mdb_iob_putc(td->tio_out, '\b');
		td->tio_x--;
	}

	termio_delch(td);
}

static void
termio_delch(termio_data_t *td)
{
	mdb_iob_putc(td->tio_out, ' ');
	mdb_iob_puts(td->tio_out, td->tio_info.ti_cub1.at_str);
	mdb_iob_flush(td->tio_out);
}

static void
termio_clear(termio_data_t *td)
{
	while (td->tio_x-- != 0)
		mdb_iob_puts(td->tio_out, td->tio_info.ti_cub1.at_str);

	while (td->tio_y < td->tio_max_y) {
		mdb_iob_puts(td->tio_out, td->tio_info.ti_cud1.at_str);
		td->tio_y++;
	}

	while (td->tio_y-- != 0) {
		mdb_iob_puts(td->tio_out, td->tio_info.ti_el.at_str);
		mdb_iob_puts(td->tio_out, td->tio_info.ti_cuu1.at_str);
	}

	mdb_iob_puts(td->tio_out, td->tio_info.ti_el.at_str);
	mdb_iob_flush(td->tio_out);

	termio_prompt(td);
}

static void
termio_redraw(termio_data_t *td)
{
	size_t tipos;

	termio_clear(td);

	if (td->tio_cmdbuf.cmd_buflen != 0) {
		mdb_iob_nputs(td->tio_out, td->tio_cmdbuf.cmd_buf,
		    td->tio_cmdbuf.cmd_buflen);

		tipos = td->tio_cmdbuf.cmd_buflen + td->tio_promptlen;
		td->tio_x = tipos % td->tio_cols;
		td->tio_y = tipos / td->tio_cols;

		if (td->tio_x == 0 && td->tio_y > td->tio_max_y) {
			mdb_iob_putc(td->tio_out, '\r');
			mdb_iob_nl(td->tio_out);
		}

		mdb_iob_flush(td->tio_out);
		termio_mvcur(td);
	}
}

static void
termio_prompt(termio_data_t *td)
{
	mdb_iob_puts(td->tio_out, td->tio_prompt);
	mdb_iob_flush(td->tio_out);

	td->tio_x = td->tio_promptlen;
	td->tio_y = 0;
}

mdb_io_t *
mdb_termio_create(const char *name, int rfd, int wfd)
{
	termio_data_t *td;

	struct winsize winsz;
	termio_attr_t *ta;

	const char *str;
	size_t nbytes;
	char *bufp;

	int err, i;

	td = mdb_alloc(sizeof (termio_data_t), UM_SLEEP);
	td->tio_io = mdb_alloc(sizeof (mdb_io_t), UM_SLEEP);

	if (setupterm((char *)name, rfd, &err) == ERR) {
		if (err == 0) {
			die("Cannot find terminal info for %s\n",
			    name ? name : "<unknown>");
		}
		if (err == -1)
			die("failed to locate terminfo database\n");
		else
			die("failed to initialize terminal\n");
	}

	/*
	 * Initialize i/o structures and command-line buffer:
	 */
	td->tio_io->io_ops = &termio_ops;
	td->tio_io->io_data = td;
	td->tio_io->io_next = NULL;
	td->tio_io->io_refcnt = 0;

	td->tio_in_io = mdb_fdio_create(rfd);
	td->tio_in = mdb_iob_create(td->tio_in_io, MDB_IOB_RDONLY);

	td->tio_out_io = mdb_fdio_create(wfd);
	td->tio_out = mdb_iob_create(td->tio_out_io, MDB_IOB_WRONLY);

	td->tio_link = NULL;

	mdb_cmdbuf_create(&td->tio_cmdbuf);

	/*
	 * Load terminal attributes:
	 */
	if (termio_ctl(td->tio_io, TCGETS, &td->tio_otios) == -1)
		die("failed to get terminal attributes");

	for (nbytes = 0, ta = &termio_attrs[0]; ta->ta_name != NULL; ta++) {
		switch (ta->ta_type) {
		case TIO_ATTR_STR:
			str = tigetstr(ta->ta_name);

			if (str == (const char *)-1) {
				die("termio_create: bad terminfo str attr %s\n",
				    ta->ta_name);
			}

			if (str != NULL)
				nbytes += strlen(str) + 1;
			break;

		case TIO_ATTR_BOOL:
			if (tigetflag(ta->ta_name) == -1) {
				die("termio_create: bad terminfo bool %s\n",
				    ta->ta_name);
			}
			break;

		case TIO_ATTR_INT:
			if (tigetnum(ta->ta_name) == -2) {
				die("termio_create: bad terminfo num %s\n",
				    ta->ta_name);
			}
			break;

		default:
			die("termio_create: bad terminfo type %s/%d\n",
				ta->ta_name, ta->ta_type);
		}
	}

	if (nbytes != 0)
		td->tio_attrs = mdb_alloc(nbytes, UM_SLEEP);
	else
		td->tio_attrs = NULL;

	td->tio_attrslen = nbytes;
	bufp = td->tio_attrs;

	for (ta = &termio_attrs[0]; ta->ta_name != NULL; ta++) {
		switch (ta->ta_type) {
		case TIO_ATTR_STR:
			if ((str = tigetstr(ta->ta_name)) != NULL) {
				(void) strcpy(bufp, str);
				ta->ta_valp->at_str = bufp;
				bufp += strlen(str) + 1;
			} else {
				ta->ta_valp->at_str = NULL;
			}
			break;

		case TIO_ATTR_BOOL:
			ta->ta_valp->at_val = tigetflag(ta->ta_name);
			break;

		case TIO_ATTR_INT:
			ta->ta_valp->at_val = tigetnum(ta->ta_name);
			break;
		}
	}

	/*
	 * Copy attribute pointers from temporary struct into td->tio_info
	 */
	bcopy(&termio_info, &td->tio_info, sizeof (termio_info_t));

	/*
	 * Fill in all the keymap entries with the insert function
	 */
	for (i = 0; i < KEY_MAX; i++)
		td->tio_keymap[i] = termio_insert;

	/*
	 * Now override selected entries with editing functions:
	 */
	td->tio_keymap['\n'] = termio_accept;
	td->tio_keymap['\r'] = termio_accept;
	td->tio_keymap['\b'] = termio_backspace;

	td->tio_keymap[CTRL('f')] = termio_fwdchar;
	td->tio_keymap[CTRL('b')] = termio_backchar;
	td->tio_keymap[CTRL('t')] = termio_transpose;
	td->tio_keymap[CTRL('a')] = termio_home;
	td->tio_keymap[CTRL('e')] = termio_end;
	td->tio_keymap[META('f')] = termio_fwdword;
	td->tio_keymap[META('b')] = termio_backword;
	td->tio_keymap[CTRL('k')] = termio_kill;
	td->tio_keymap[CTRL('p')] = termio_prevhist;
	td->tio_keymap[CTRL('n')] = termio_nexthist;
	td->tio_keymap[CTRL('r')] = termio_findhist;
	td->tio_keymap[CTRL('l')] = termio_refresh;
	td->tio_keymap[CTRL('d')] = termio_delchar;

	td->tio_keymap[KPAD(CTRL('A'))] = termio_prevhist;
	td->tio_keymap[KPAD(CTRL('B'))] = termio_nexthist;
	td->tio_keymap[KPAD(CTRL('C'))] = termio_fwdchar;
	td->tio_keymap[KPAD(CTRL('D'))] = termio_backchar;

	/*
	 * Initialize the rest of the termio_data_t
	 */
	if (termio_ctl(td->tio_io, TIOCGWINSZ, &winsz) == 0) {
		td->tio_rows = (size_t)winsz.ws_row;
		td->tio_cols = (size_t)winsz.ws_col;
	} else {
		td->tio_rows = td->tio_info.ti_lines.at_val;
		td->tio_cols = td->tio_info.ti_cols.at_val;
	}

	if (td->tio_rows == 0) {
		if ((str = getenv("LINES")) != NULL && strisnum(str) != 0 &&
		    (i = strtoi(str)) > 0)
			td->tio_rows = i;
		else
			td->tio_rows = TIO_DEFAULT_ROWS;
	}

	if (td->tio_cols == 0) {
		if ((str = getenv("COLUMNS")) != NULL && strisnum(str) != 0 &&
		    (i = strtoi(str)) > 0)
			td->tio_cols = i;
		else
			td->tio_cols = TIO_DEFAULT_COLS;
	}

	td->tio_x = 0;
	td->tio_y = 0;
	td->tio_max_x = 0;
	td->tio_max_y = 0;

	td->tio_intr = td->tio_otios.c_cc[VINTR];
	td->tio_quit = td->tio_otios.c_cc[VQUIT];
	td->tio_erase = td->tio_otios.c_cc[VERASE];
	td->tio_kill = td->tio_otios.c_cc[VKILL];
	td->tio_eof = td->tio_otios.c_cc[VEOF];
	td->tio_susp = td->tio_otios.c_cc[VSUSP];

	td->tio_flags = 0;
	td->tio_active = FALSE;

	if (td->tio_info.ti_am.at_val && td->tio_info.ti_xenl.at_val)
		td->tio_flags |= TIO_AUTOWRAP;
	else
		td->tio_cols--;

	if (td->tio_info.ti_bw.at_val)
		td->tio_flags |= TIO_BACKLEFT;

	if (td->tio_info.ti_ich1.at_str)
		td->tio_flags |= TIO_INSERT;

	if (mdb.m_flags & MDB_FL_USECUP)
		td->tio_flags |= TIO_USECUP;

	/*
	 * Adjust keymap according to terminal settings
	 */
	td->tio_keymap[td->tio_intr] = termio_intr;
	td->tio_keymap[td->tio_quit] = termio_quit;
	td->tio_keymap[td->tio_erase] = termio_backspace;
	td->tio_keymap[td->tio_kill] = termio_reset;
	td->tio_keymap[td->tio_susp] = termio_susp;

	/*
	 * Copy terminal settings into temporary struct for modifications.
	 * We retain the original settings in td->tio_otios so we can
	 * restore them when this termio is destroyed.
	 */
	bcopy(&td->tio_otios, &td->tio_ntios, sizeof (struct termios));

	td->tio_ntios.c_lflag &=
	    ~(ICANON | ISTRIP | INPCK | ICRNL | INLCR | ISIG | ECHO);

	td->tio_ntios.c_cflag |= CS8;
	td->tio_ntios.c_cc[VMIN] = 1;

	(void) mdb_signal_sethandler(SIGWINCH, termio_winch, td);
	(void) mdb_signal_sethandler(SIGTSTP, termio_tstp, td);

	termio_enter(td);
	return (td->tio_io);
}

int
mdb_iob_isatty(mdb_iob_t *iob)
{
	mdb_io_t *io;

	for (io = iob->iob_iop; io != NULL; io = io->io_next) {
		if (io->io_ops == &termio_ops)
			return (1);
	}

	return (0);
}

static const char *
termio_insert(termio_data_t *td, int c)
{
	size_t olen = td->tio_cmdbuf.cmd_buflen;

	if (mdb_cmdbuf_insert(&td->tio_cmdbuf, c) == 0) {
		if (mdb_cmdbuf_atend(&td->tio_cmdbuf))
			termio_addch(td, c, td->tio_cmdbuf.cmd_buflen - olen);
		else
			termio_redraw(td);
	}

	return (NULL);
}

static const char *
termio_accept(termio_data_t *td, int c)
{
	if (td->tio_flags & TIO_FINDHIST) {
		(void) mdb_cmdbuf_findhist(&td->tio_cmdbuf, c);

		td->tio_prompt = mdb.m_prompt;
		td->tio_promptlen = mdb.m_promptlen;
		td->tio_flags &= ~TIO_FINDHIST;

		termio_redraw(td);
		return (NULL);
	}

	return (mdb_cmdbuf_accept(&td->tio_cmdbuf));
}

static const char *
termio_backspace(termio_data_t *td, int c)
{
	if (mdb_cmdbuf_backspace(&td->tio_cmdbuf, c) == 0) {
		if (mdb_cmdbuf_atend(&td->tio_cmdbuf))
			termio_bspch(td);
		else
			termio_redraw(td);
	}

	return (NULL);
}

static const char *
termio_delchar(termio_data_t *td, int c)
{
	if (!(mdb.m_flags & MDB_FL_IGNEOF) &&
	    mdb_cmdbuf_atend(&td->tio_cmdbuf) &&
	    mdb_cmdbuf_atstart(&td->tio_cmdbuf))
		return (termio_quit(td, c));

	if (mdb_cmdbuf_delchar(&td->tio_cmdbuf, c) == 0) {
		if (mdb_cmdbuf_atend(&td->tio_cmdbuf))
			termio_delch(td);
		else
			termio_redraw(td);
	}

	return (NULL);
}

static const char *
termio_fwdchar(termio_data_t *td, int c)
{
	if (mdb_cmdbuf_fwdchar(&td->tio_cmdbuf, c) == 0)
		termio_mvcur(td);

	return (NULL);
}

static const char *
termio_backchar(termio_data_t *td, int c)
{
	if (mdb_cmdbuf_backchar(&td->tio_cmdbuf, c) == 0)
		termio_mvcur(td);

	return (NULL);
}

static const char *
termio_transpose(termio_data_t *td, int c)
{
	if (mdb_cmdbuf_transpose(&td->tio_cmdbuf, c) == 0)
		termio_redraw(td);

	return (NULL);
}

static const char *
termio_home(termio_data_t *td, int c)
{
	if (mdb_cmdbuf_home(&td->tio_cmdbuf, c) == 0)
		termio_mvcur(td);

	return (NULL);
}

static const char *
termio_end(termio_data_t *td, int c)
{
	if (mdb_cmdbuf_end(&td->tio_cmdbuf, c) == 0)
		termio_mvcur(td);

	return (NULL);
}

static const char *
termio_fwdword(termio_data_t *td, int c)
{
	if (mdb_cmdbuf_fwdword(&td->tio_cmdbuf, c) == 0)
		termio_mvcur(td);

	return (NULL);
}

static const char *
termio_backword(termio_data_t *td, int c)
{
	if (mdb_cmdbuf_backword(&td->tio_cmdbuf, c) == 0)
		termio_mvcur(td);

	return (NULL);
}

static const char *
termio_kill(termio_data_t *td, int c)
{
	if (mdb_cmdbuf_kill(&td->tio_cmdbuf, c) == 0)
		termio_redraw(td);

	return (NULL);
}

static const char *
termio_reset(termio_data_t *td, int c)
{
	if (mdb_cmdbuf_reset(&td->tio_cmdbuf, c) == 0)
		termio_clear(td);

	return (NULL);
}

static const char *
termio_prevhist(termio_data_t *td, int c)
{
	if (mdb_cmdbuf_prevhist(&td->tio_cmdbuf, c) == 0)
		termio_redraw(td);

	return (NULL);
}

static const char *
termio_nexthist(termio_data_t *td, int c)
{
	if (mdb_cmdbuf_nexthist(&td->tio_cmdbuf, c) == 0)
		termio_redraw(td);

	return (NULL);
}

static const char *
termio_findhist(termio_data_t *td, int c)
{
	if (mdb_cmdbuf_reset(&td->tio_cmdbuf, c) == 0) {
		td->tio_prompt = "Search: ";
		td->tio_promptlen = strlen(td->tio_prompt);
		td->tio_flags |= TIO_FINDHIST;
		termio_redraw(td);
	}

	return (NULL);
}

/*ARGSUSED*/
static const char *
termio_refresh(termio_data_t *td, int c)
{
	termio_redraw(td);
	return (NULL);
}

/*ARGSUSED*/
static const char *
termio_intr(termio_data_t *td, int c)
{
	(void) mdb_cmdbuf_reset(&td->tio_cmdbuf, c);
	longjmp(mdb.m_frame->f_pcb, MDB_ERR_SIGINT);
	/*NOTREACHED*/
	return (NULL);
}

/*ARGSUSED*/
static const char *
termio_susp(termio_data_t *td, int c)
{
	(void) mdb_signal_sethandler(SIGWINCH, SIG_IGN, NULL);
	(void) mdb_signal_sethandler(SIGTSTP, SIG_IGN, NULL);

	termio_exit(td);
	mdb_iob_nl(td->tio_out);

	if (termio_ctl(td->tio_io, TCSETS, &td->tio_otios) == -1)
		warn("failed to restore terminal attributes");

	(void) mdb_signal_sethandler(SIGTSTP, SIG_DFL, NULL);
	(void) mdb_signal_pgrp(SIGTSTP);
	longjmp(td->tio_env, SIGCONT);
	/*NOTREACHED*/
	return (NULL);
}

/*ARGSUSED*/
static const char *
termio_quit(termio_data_t *td, int c)
{
	(void) mdb_cmdbuf_reset(&td->tio_cmdbuf, c);
	longjmp(mdb.m_frame->f_pcb, MDB_ERR_QUIT);
	/*NOTREACHED*/
	return (NULL);
}

/*ARGSUSED*/
static void
termio_winch(int sig, siginfo_t *sip, ucontext_t *ucp, void *data)
{
	termio_data_t *td = (termio_data_t *)data;
	mdb_bool_t change = FALSE;
	struct winsize winsz;

	if (termio_ctl(td->tio_io, TIOCGWINSZ, &winsz) == -1)
		goto done;

	if (td->tio_rows != (size_t)winsz.ws_row ||
	    td->tio_cols != (size_t)winsz.ws_col) {

		if (td->tio_active)
			termio_clear(td);

		if (winsz.ws_row != 0)
			td->tio_rows = (size_t)winsz.ws_row;

		if (winsz.ws_col != 0)
			td->tio_cols = (size_t)winsz.ws_col;

		if (td->tio_active)
			termio_clear(td);

		change = TRUE;
	}

done:
	if (change && td->tio_active)
		longjmp(td->tio_env, sig);

	(void) mdb_signal_unblock(sig);
}

/*ARGSUSED*/
static void
termio_tstp(int sig, siginfo_t *sip, ucontext_t *ucp, void *data)
{
	(void) termio_susp(data, CTRL('Z'));
}
