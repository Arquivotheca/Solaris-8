/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)merge.c	1.4	99/04/26 SMI"

#include "merge.h"

static int pq_N;
static stream_t	**pq_queue;
static int (*pq_coll_fcn)(line_rec_t *, line_rec_t *, ssize_t, flag_t);

static ssize_t (*mg_coll_convert)(field_t *, line_rec_t *, flag_t, vchar_t);

static int
prepare_output_stream(stream_t *ostrp, sort_t *S)
{
	stream_clear(ostrp);
	stream_unset(ostrp, STREAM_OPEN);

	stream_set(ostrp,
	    (S->m_single_byte_locale ? STREAM_SINGLE : STREAM_WIDE) |
	    (S->m_unique_lines ? STREAM_UNIQUE : 0));

	if (S->m_output_to_stdout) {
		stream_set(ostrp, STREAM_NOTFILE);
		ostrp->s_filename = (char *)filename_stdout;
	} else
		ostrp->s_filename = S->m_output_filename;

	return (SOP_OPEN_FOR_WRITE(ostrp));
}

static void
merge_one_stream(stream_t *strp, stream_t *outstrp)
{
	if (strp->s_status & STREAM_SINGLE || strp->s_status & STREAM_WIDE)
		stream_set(strp, STREAM_INSTANT);

	(void) SOP_PRIME(strp);

	/*
	 * We needn't clean up the l_raw_collate fields, as we're about to
	 * finish our sorting, and can leave those for exit().
	 */
	stream_dump(strp, outstrp, 0);

	SOP_FLUSH(outstrp);
}

static void
merge_two_streams(field_t *fields_chain, stream_t *str_a, stream_t *str_b,
    stream_t *outstr, vchar_t field_separator, flag_t coll_flags)
{
	int (*collate_fcn)(line_rec_t *, line_rec_t *, ssize_t, flag_t);

	ASSERT(str_a->s_element_size == str_b->s_element_size);

	if (str_a->s_element_size == sizeof (char))
		collate_fcn = collated;
	else
		collate_fcn = collated_wide;

	if (str_a->s_status & STREAM_SINGLE || str_a->s_status & STREAM_WIDE)
		stream_set(str_a, STREAM_INSTANT);
	if (str_b->s_status & STREAM_SINGLE || str_b->s_status & STREAM_WIDE)
		stream_set(str_b, STREAM_INSTANT);

	(void) SOP_PRIME(str_a);
	(void) SOP_PRIME(str_b);

	str_a->s_current.l_collate.sp = safe_realloc(NULL,
	    str_a->s_current.l_collate_bufsize = INITIAL_COLLATION_SIZE *
	    str_a->s_element_size);
	str_b->s_current.l_collate.sp = safe_realloc(NULL,
	    str_b->s_current.l_collate_bufsize = INITIAL_COLLATION_SIZE *
	    str_b->s_element_size);

	(void) mg_coll_convert(fields_chain, &str_a->s_current, FCV_REALLOC,
	    field_separator);
	(void) mg_coll_convert(fields_chain, &str_b->s_current, FCV_REALLOC,
	    field_separator);

	for (;;)
		if (collate_fcn(&str_a->s_current, &str_b->s_current, 0,
		    coll_flags) < 0) {
			SOP_PUT_LINE(outstr, &str_a->s_current);
			if (stream_eos(str_a)) {
				(void) SOP_CLOSE(str_a);
				str_a = str_b;
				break;
			}
			SOP_FETCH(str_a);
			if (str_a->s_current.l_collate_length != 0)
				continue;
			(void) mg_coll_convert(fields_chain, &str_a->s_current,
			    FCV_REALLOC, field_separator);
		} else {
			SOP_PUT_LINE(outstr, &str_b->s_current);
			if (stream_eos(str_b)) {
				SOP_CLOSE(str_b);
				break;
			}
			SOP_FETCH(str_b);
			if (str_b->s_current.l_collate_length != 0)
				continue;
			(void) mg_coll_convert(fields_chain, &str_b->s_current,
			    FCV_REALLOC, field_separator);
		}

	/*
	 * Again, leave l_raw_collate cleanup for exit().
	 */
	stream_dump(str_a, outstr, 0);
	SOP_FLUSH(outstr);
}

/*
 * priority queue routines
 *   used for merges involving more than two sources
 */
static void
heap_up(stream_t **A, int k, flag_t coll_flags)
{
	while (k > 1 &&
	    pq_coll_fcn(&A[k / 2]->s_current, &A[k]->s_current, 0,
		coll_flags) > 0) {
		swap((void **)&pq_queue[k], (void **)&pq_queue[k / 2]);
		k /= 2;
	}
}

static void
heap_down(stream_t **A, int k, int N, flag_t coll_flags)
{
	int	j;

	while (2 * k <= N) {
		j = 2 * k;
		if (j < N && pq_coll_fcn(&A[j]->s_current,
		    &A[j + 1]->s_current, 0, coll_flags) > 0)
			j++;
		if (pq_coll_fcn(&A[k]->s_current, &A[j]->s_current, 0,
		    coll_flags) <= 0)
			break;
		swap((void **)&pq_queue[k], (void **)&pq_queue[j]);
		k = j;
	}
}

static int
pqueue_empty()
{
	return (pq_N == 0);
}

static void
pqueue_init(size_t max_size,
    int (*coll_fcn)(line_rec_t *, line_rec_t *, ssize_t, flag_t))
{
	pq_queue = safe_realloc(NULL, sizeof (stream_t *) * (max_size + 1));
	pq_N = 0;
	pq_coll_fcn = coll_fcn;
}

static void
pqueue_insert(stream_t *source, flag_t coll_flags)
{
	pq_queue[++pq_N] = source;
	heap_up(pq_queue, pq_N, coll_flags);
}

static stream_t *
pqueue_head(flag_t coll_flags)
{
	swap((void **)&pq_queue[1], (void **)&pq_queue[pq_N]);
	heap_down(pq_queue, 1, pq_N - 1, coll_flags);
	return (pq_queue[pq_N--]);
}

static void
merge_n_streams(sort_t *S, stream_t *head_streamp, int n_streams,
    stream_t *out_streamp, flag_t coll_flags)
{
	stream_t *top_streamp;
	stream_t *cur_streamp;
	stream_t *bot_streamp;
	stream_t *loop_out_streamp;
	flag_t is_single_byte = S->m_single_byte_locale;

	pqueue_init(n_streams, is_single_byte ? collated : collated_wide);

	top_streamp = bot_streamp = head_streamp;

	for (;;) {
		hold_file_descriptor();
		while (bot_streamp != NULL) {
			if (stream_open_for_read(S, bot_streamp) == -1) {
				/*
				 * Remainder of file descriptors exhausted; back
				 * off to the last valid, primed stream.
				 */
				bot_streamp = bot_streamp->s_previous;
				break;
			}

			if (bot_streamp->s_status & STREAM_SINGLE ||
			    bot_streamp->s_status & STREAM_WIDE)
				stream_set(bot_streamp, STREAM_INSTANT);

			(void) SOP_PRIME(bot_streamp);

			bot_streamp = bot_streamp->s_next;
		}
		release_file_descriptor();

		if (bot_streamp == NULL) {
			if (prepare_output_stream(out_streamp, S) != -1)
				loop_out_streamp = out_streamp;
			else
				terminate(SE_INSUFFICIENT_DESCRIPTORS);
		} else {
			loop_out_streamp = stream_push_to_temporary(
			    S->m_tmpdir_template, &head_streamp, NULL, ST_OPEN |
			    ST_NOCACHE | (is_single_byte ? 0 : ST_WIDE));

			if (loop_out_streamp == NULL ||
			    top_streamp == bot_streamp)
				/*
				 * We need three file descriptors to make
				 * progress; if top_streamp == bot_streamp, then
				 * we have only two.
				 */
				terminate(SE_INSUFFICIENT_DESCRIPTORS);
		}

		cur_streamp = top_streamp;
		while (cur_streamp != bot_streamp) {
			if (stream_eos(cur_streamp)) {
				cur_streamp = cur_streamp->s_next;
				continue;
			}

			cur_streamp->s_current.l_collate.sp =
			    safe_realloc(NULL,
				cur_streamp->s_current.l_collate_bufsize =
				INITIAL_COLLATION_SIZE);
			(void) mg_coll_convert(S->m_fields_head,
			    &cur_streamp->s_current, FCV_REALLOC,
			    S->m_field_separator);

			pqueue_insert(cur_streamp, coll_flags);

			cur_streamp = cur_streamp->s_next;
		}

		while (!pqueue_empty()) {
			cur_streamp = pqueue_head(coll_flags);

			SOP_PUT_LINE(loop_out_streamp, &cur_streamp->s_current);

			if (!stream_eos(cur_streamp)) {
				SOP_FETCH(cur_streamp);
				(void) mg_coll_convert(S->m_fields_head,
				    &cur_streamp->s_current, FCV_REALLOC,
				    S->m_field_separator);
				pqueue_insert(cur_streamp, coll_flags);
			}
		}

		cur_streamp = top_streamp;
		while (cur_streamp != bot_streamp) {
			if (!(cur_streamp->s_status & STREAM_ARRAY))
				safe_free(cur_streamp->s_current.l_collate.sp);
			cur_streamp->s_current.l_collate.sp = NULL;

			if (!(cur_streamp->s_status & STREAM_NOT_FREEABLE)) {
				(void) SOP_FREE(cur_streamp);
				(void) SOP_CLOSE(cur_streamp);
			}
			cur_streamp = cur_streamp->s_next;
		}

		(void) SOP_FLUSH(loop_out_streamp);

		if (bot_streamp == NULL)
			break;

		if (!(loop_out_streamp->s_status & STREAM_NOTFILE)) {
			(void) SOP_CLOSE(loop_out_streamp);
			/*
			 * Get file size so that we may treat intermediate files
			 * with our stream_mmap facilities.
			 */
			stream_stat_chain(loop_out_streamp);
		}

		top_streamp = bot_streamp;
		bot_streamp = bot_streamp->s_next;
	}
}

void
merge(sort_t *S)
{
	stream_t *merge_chain;
	stream_t *cur_streamp;
	stream_t out_stream;
	uint_t n_merges;
	flag_t coll_flags;

	if (S->m_merge_only)
		merge_chain = S->m_input_streams;
	else {
		merge_chain = S->m_temporary_streams;
		stream_stat_chain(merge_chain);
	}

	if (S->m_field_options & FIELD_REVERSE_COMPARISONS)
		coll_flags = COLL_REVERSE;
	else
		coll_flags = 0;
	if (S->m_entire_line)
		coll_flags |= COLL_UNIQUE;

	set_cleanup_chain(merge_chain);

	n_merges = stream_count_chain(merge_chain);

	mg_coll_convert = S->m_coll_convert;
	cur_streamp = merge_chain;

	switch (n_merges) {
		case 0:
			/*
			 * no files for merge
			 */
			warning(gettext("no files available to merge\n"));
			break;
		case 1:
			/*
			 * fast path: only one file for merge
			 */
			(void) stream_open_for_read(S, cur_streamp);
			(void) prepare_output_stream(&out_stream, S);
			merge_one_stream(cur_streamp, &out_stream);
			break;
		case 2:
			/*
			 * fast path: only two files for merge
			 */
			(void) stream_open_for_read(S, cur_streamp);
			(void) stream_open_for_read(S, cur_streamp->s_next);
			if (prepare_output_stream(&out_stream, S) == -1)
				terminate(SE_INSUFFICIENT_DESCRIPTORS);
			merge_two_streams(S->m_fields_head, cur_streamp,
			    cur_streamp->s_next, &out_stream,
			    S->m_field_separator, coll_flags);
			break;
		default:
			/*
			 * full merge
			 */
			merge_n_streams(S, cur_streamp, n_merges, &out_stream,
			    coll_flags);
			break;
	}

	if (S->m_output_guard)
		remove_output_guard(S);

	stream_unlink_temporaries(merge_chain);
}
