/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)check.c	1.2	99/04/26 SMI"

#include "check.h"

#define	MSG_DISORDER		gettext("sort: disorder: ")
#define	MSG_NONUNIQUE		gettext("sort: non-unique: ")
#define	CHECK_FAILURE_DISORDER	0x1
#define	CHECK_FAILURE_NONUNIQUE	0x2
#define	CHECK_WIDE		0x4

static void
fail_check(line_rec_t *L, int flags)
{
	char *line;
	ssize_t length;

	if (flags & CHECK_WIDE) {
		if ((length = (ssize_t)wcstombs(NULL, L->l_data.wp, 0)) < 0)
			terminate(SE_ILLEGAL_CHARACTER);

		/*
		 * +1 for null character
		 */
		line = alloca(length + 1);
		(void) wcstombs(line, L->l_data.wp, L->l_data_length);
		line[length] = '\0';
	} else {
		line = L->l_data.sp;
		length = L->l_data_length;
	}

	if (flags & CHECK_FAILURE_DISORDER) {
		(void) fprintf(stderr, MSG_DISORDER);
		(void) write(fileno(stderr), line, length);
		(void) fprintf(stderr, "\n");
		return;
	}

	(void) fprintf(stderr, MSG_NONUNIQUE);
	(void) write(fileno(stderr), line, length);
	(void) fprintf(stderr, "\n");
}

static void
swap_coll_bufs(line_rec_t *A, line_rec_t *B)
{
	char *coll_buffer = B->l_collate.sp;
	ssize_t coll_bufsize = B->l_collate_bufsize;

	copy_line_rec(A, B);

	A->l_collate.sp = coll_buffer;
	A->l_collate_bufsize = coll_bufsize;
}

/*
 * check_if_sorted() interacts with a stream in a slightly different way than a
 * simple sort or a merge operation:  the check involves looking at two adjacent
 * lines of the file and verifying that they are collated according to the key
 * specifiers given.  For files accessed via mmap(), this is simply done as the
 * entirety of the file is present in the address space.  For files accessed via
 * stdio, regardless of locale, we must be able to guarantee that two lines are
 * present in memory at once.  The basic buffer code for stdio does not make
 * such a guarantee, so we use stream_swap_buffer() to alternate between two
 * input buffers.
 */
void
check_if_sorted(sort_t *S)
{
	size_t av_mem = available_memory(S->m_memory_limit);
	size_t input_mem;
	int numerator, denominator;

	char *data_buffer = NULL;
	size_t data_bufsize = 0;
	line_rec_t last_line;
	int r;
	int swap_required;
	flag_t coll_flags;
	stream_t *cur_streamp = S->m_input_streams;

	ssize_t (*conversion_fcn)(field_t *, line_rec_t *, flag_t, vchar_t) =
	    field_convert;
	int (*collation_fcn)(line_rec_t *, line_rec_t *, ssize_t, flag_t) =
	    collated;

	set_memory_ratio(S, &numerator, &denominator);

	if (stream_open_for_read(S, cur_streamp) > 1)
		terminate(SE_CHECK_ERROR);

	if (stream_eos(cur_streamp))
		terminate(SE_CHECK_SUCCEED);

	(void) memset(&last_line, 0, sizeof (line_rec_t));

	/*
	 * We need to swap data buffers for the stream with each fetch, except
	 * on STREAM_MMAP (which are implicitly STREAM_SUSTAIN).
	 */
	swap_required = !(cur_streamp->s_status & STREAM_MMAP);
	if (swap_required) {
		stream_set(cur_streamp, STREAM_INSTANT);
		/*
		 * We use one half of the available memory for input, half for
		 * each buffer.  (The other half is left unreserved, in case
		 * conversions to collatable form require it.)
		 */
		input_mem = numerator * av_mem / denominator / 4;

		stream_set_size(cur_streamp, input_mem);
		stream_swap_buffer(cur_streamp, &data_buffer, &data_bufsize);
		stream_set_size(cur_streamp, input_mem);

		if (cur_streamp->s_status & STREAM_WIDE) {
			conversion_fcn = field_convert_wide;
			collation_fcn = collated_wide;
		}
	}

	if (SOP_PRIME(cur_streamp) > 1)
		terminate(SE_CHECK_ERROR);

	if (S->m_field_options & FIELD_REVERSE_COMPARISONS)
		coll_flags = COLL_REVERSE;
	else
		coll_flags = 0;
	if (S->m_unique_lines)
		coll_flags |= COLL_UNIQUE;

	cur_streamp->s_current.l_collate_bufsize = INITIAL_COLLATION_SIZE
	    * cur_streamp->s_element_size;
	cur_streamp->s_current.l_collate.sp = safe_realloc(NULL,
	    cur_streamp->s_current.l_collate_bufsize);
	last_line.l_collate_bufsize = INITIAL_COLLATION_SIZE *
	    cur_streamp->s_element_size;
	last_line.l_collate.sp = safe_realloc(NULL,
	    last_line.l_collate_bufsize);

	(void) conversion_fcn(S->m_fields_head, &cur_streamp->s_current,
	    FCV_REALLOC, S->m_field_separator);

	swap_coll_bufs(&cur_streamp->s_current, &last_line);
	if (swap_required)
		stream_swap_buffer(cur_streamp, &data_buffer, &data_bufsize);

	while (!stream_eos(cur_streamp)) {
		(void) SOP_FETCH(cur_streamp);

		(void) conversion_fcn(S->m_fields_head, &cur_streamp->s_current,
		    FCV_REALLOC, S->m_field_separator);

		r = collation_fcn(&last_line, &cur_streamp->s_current, 0,
		    coll_flags);

		if (r < 0 || (r == 0 && S->m_unique_lines == 0)) {
			swap_coll_bufs(&cur_streamp->s_current, &last_line);
			if (swap_required)
				stream_swap_buffer(cur_streamp, &data_buffer,
				    &data_bufsize);
			continue;
		}

		if (r > 0) {
#ifndef	XPG4
			fail_check(&cur_streamp->s_current,
			    CHECK_FAILURE_DISORDER |
			    (S->m_single_byte_locale ? 0 : CHECK_WIDE));
#endif /* XPG4 */
			terminate(SE_CHECK_FAILED);
		}

		if (r == 0 && S->m_unique_lines != 0) {
#ifndef	XPG4
			fail_check(&cur_streamp->s_current,
			    CHECK_FAILURE_NONUNIQUE |
			    (S->m_single_byte_locale ? 0 : CHECK_WIDE));
#endif /* XPG4 */
			terminate(SE_CHECK_FAILED);
		}
	}

	terminate(SE_CHECK_SUCCEED);
	/* NOTREACHED */
}
