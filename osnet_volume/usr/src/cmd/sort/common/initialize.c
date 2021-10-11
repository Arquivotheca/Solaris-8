/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)initialize.c	1.3	99/11/24 SMI"

#include "initialize.h"

#ifndef TEXT_DOMAIN
/*
 * TEXT_DOMAIN should have been set by build environment.
 */
#define	TEXT_DOMAIN	"SUNW_OST_OSCMD"
#endif /* TEXT_DOMAIN */

/*
 * /dev/zero, output file, stdin, stdout, and stderr
 */
#define	N_FILES_ALREADY_OPEN	5

static const char *filename_stdin = "STDIN";
const char *filename_stdout = "STDOUT";

static const char *default_tmpdir = "/var/tmp";
static const char *default_template = "/stmAAAXXXXXX";
static const char *default_template_count = ".00000000";

static sigjmp_buf signal_jmp_buf;
static volatile sig_atomic_t signal_delivered;

static void
set_signal_jmp(void)
{
	if (sigsetjmp(signal_jmp_buf, 1))
		terminate(SE_CAUGHT_SIGNAL, signal_delivered);
}

static void
sig_handler(int signo)
{
	signal_delivered = signo;
	siglongjmp(signal_jmp_buf, 1);
}

/*
 * Output guard routines.
 */
static int
output_guard_required(sort_t *S)
{
	struct stat output_stat;

	if (S->m_output_to_stdout)
		return (0);

	if (stat(S->m_output_filename, &output_stat) == 0) {
		stream_t *strp = S->m_input_streams;

		while (strp != NULL) {
			/*
			 * We needn't protect an empty file.
			 */
			if (!(strp->s_status & STREAM_NOTFILE) &&
			    strp->s_ino == output_stat.st_ino &&
			    strp->s_filesize > 0) {
				S->m_output_guard = strp;
				set_output_guard(S->m_output_guard);
				return (1);
			}
			strp = strp->s_next;
		}
	}

	return (0);
}

static void
establish_output_guard(sort_t *S)
{
	char *guard_filename;

	ASSERT(S->m_output_guard != NULL && S->m_output_guard->s_filesize > 0);

	if (bump_file_template(S->m_tmpdir_template) < 0)
		terminate(SE_TOO_MANY_TEMPFILES);

	guard_filename = strdup(S->m_tmpdir_template);

	xcp(guard_filename, S->m_output_guard->s_filename,
	    S->m_output_guard->s_filesize);

	set_output_file(S->m_output_guard->s_filename);
	S->m_output_guard->s_filename = guard_filename;
}

void
remove_output_guard(sort_t *S)
{
	if (unlink(S->m_output_guard->s_filename) == -1)
		warning(gettext("unable to unlink %s: %s\n"),
		    S->m_output_guard->s_filename, strerror(errno));
}

void
initialize_pre(sort_t *S)
{
	/*
	 * Initialize sort structure.
	 */
	(void) memset(S, 0, sizeof (sort_t));

	S->m_default_species = ALPHA;

	/*
	 * Simple localization issues.
	 */
	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

#ifndef DEBUG_FORCE_WIDE
	S->m_c_locale = xstreql("C", setlocale(LC_COLLATE, NULL));
	S->m_single_byte_locale = SGN(MB_CUR_MAX == 1);
#else /* DEBUG_FORCE_WIDE */
	S->m_c_locale = 0;
	S->m_single_byte_locale = 0;
#endif /* DEBUG_FORCE_WIDE */

	/*
	 * We use a constant seed so that our sorts on a given file are
	 * reproducible.
	 */
	srand(3459871433);

	/*
	 * Establish signal handlers and sufficient state for clean up.
	 */
	if (signal(SIGTERM, sig_handler) == SIG_ERR)
		terminate(SE_CANT_SET_SIGNAL);
	if (signal(SIGHUP, sig_handler) == SIG_ERR)
		terminate(SE_CANT_SET_SIGNAL);
	if (signal(SIGPIPE, sig_handler) == SIG_ERR)
		terminate(SE_CANT_SET_SIGNAL);

	set_signal_jmp();
}


void
initialize_post(sort_t *S)
{
	char *T;
	field_t	*F;

	/*
	 * Set up temporary filename template.
	 */
	if (S->m_tmpdir_template == NULL)
		S->m_tmpdir_template = getenv("TMPDIR");
	if (S->m_tmpdir_template == NULL)
		S->m_tmpdir_template = (char *)default_tmpdir;

	T = safe_realloc(NULL, strlen(S->m_tmpdir_template)
	    + strlen(default_template) + strlen(default_template_count) + 1);

	(void) strcpy(T, S->m_tmpdir_template);
	(void) strcat(T, default_template);
	(void) mktemp(T);
	(void) strcat(T, default_template_count);

	S->m_tmpdir_template = T;

	/*
	 * Initialize locale-specific ops vectors.
	 */
	field_initialize(S);

	if (S->m_single_byte_locale) {
		S->m_compare_fn = (cmp_fcn_t)strcoll;
		S->m_coll_convert = field_convert;
		F = S->m_fields_head;

		while (F != NULL) {
			switch (F->f_species) {
				case ALPHA:
					F->f_convert = field_convert_alpha;
					break;
				case NUMERIC:
					F->f_convert = field_convert_numeric;
					break;
				case MONTH:
					F->f_convert = field_convert_month;
					break;
				default:
					terminate(SE_BAD_FIELD);
			}
			F = F->f_next;
		}
	} else {
		S->m_compare_fn = (cmp_fcn_t)wcscoll;
		S->m_coll_convert = field_convert_wide;

		F = S->m_fields_head;
		while (F != NULL) {
			switch (F->f_species) {
				case ALPHA:
					F->f_convert = field_convert_alpha_wide;
					break;
				case NUMERIC:
					F->f_convert =
					    field_convert_numeric_wide;
					break;
				case MONTH:
					F->f_convert = field_convert_month_wide;
					break;
				default:
					terminate(SE_BAD_FIELD);
			}
			F = F->f_next;
		}
	}

	/*
	 * Get sizes and inodes for input streams.
	 */
	stream_stat_chain(S->m_input_streams);

	/*
	 * Output guard.
	 */
	if (output_guard_required(S))
		establish_output_guard(S);

	/*
	 * Ready stdin for usage as stream.
	 */
	if (S->m_input_from_stdin) {
		stream_t *str;

		if (S->m_single_byte_locale) {
			str = stream_new(STREAM_SINGLE | STREAM_NOTFILE);
			str->s_element_size = sizeof (char);
		} else {
			str = stream_new(STREAM_WIDE | STREAM_NOTFILE);
			str->s_element_size = sizeof (wchar_t);
		}
		str->s_filename = (char *)filename_stdin;
		stream_push_to_chain(&S->m_input_streams, str);
	}
}
