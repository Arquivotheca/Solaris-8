/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)streams_wide.c	1.5	99/11/24 SMI"

#include "streams_wide.h"
#include "streams_common.h"

#define	WIDE_VBUF_SIZE	(64 * KILOBYTE)

static int
stream_wide_prime(stream_t *str)
{
	stream_buffered_file_t *BF = &(str->s_type.BF);
	wchar_t *current_position;
	wchar_t *end_of_buffer;
	wchar_t *next_nl;

	ASSERT(!(str->s_status & STREAM_OUTPUT));
	ASSERT(str->s_status & STREAM_OPEN);

	if (str->s_status & STREAM_INSTANT && (str->s_buffer == NULL)) {
		str->s_buffer = xzmap(0, WIDE_VBUF_SIZE, PROT_READ |
		    PROT_WRITE, MAP_PRIVATE, 0);
		if (str->s_buffer == MAP_FAILED)
			terminate(SE_MMAP_FAILED);
		str->s_buffer_size = WIDE_VBUF_SIZE;
	}

	ASSERT(str->s_buffer != NULL);

	if (str->s_status & STREAM_PRIMED) {
		ASSERT(str->s_current.l_data_length >= -1);
		(void) memcpy(str->s_buffer, str->s_current.l_data.wp,
		    (str->s_current.l_data_length + 1) * sizeof (wchar_t));
		str->s_current.l_data.wp = str->s_buffer;

		if (SOP_FETCH(str) == NEXT_LINE_INCOMPLETE)
			terminate(SE_INSUFFICIENT_MEMORY);

		return (PRIME_SUCCEEDED);
	}

	stream_set(str, STREAM_PRIMED);

	current_position = (wchar_t *)str->s_buffer;
	/*LINTED ALIGNMENT*/
	end_of_buffer = (wchar_t *)((char *)str->s_buffer +
	    str->s_buffer_size);

	trip_eof(BF->s_fp);
	if (!feof(BF->s_fp))
		(void) fgetws(current_position, end_of_buffer
		    - current_position, BF->s_fp);
	else {
		stream_set(str, STREAM_EOS_REACHED);
		return (PRIME_FAILED_EMPTY_FILE);
	}

	str->s_current.l_data.wp = current_position;
	next_nl = xmemwchar(current_position, L'\n', end_of_buffer -
	    current_position);
	if (next_nl == NULL)
		str->s_current.l_data_length = MIN(wslen(current_position),
		    end_of_buffer - current_position);
	else
		str->s_current.l_data_length = next_nl - current_position;
	*(str->s_current.l_data.wp + str->s_current.l_data_length) = L'\0';

	str->s_current.l_collate.wp = NULL;
	str->s_current.l_collate_length = 0;

	return (PRIME_SUCCEEDED);
}

#define	SHELF_OCCUPIED  1
#define	SHELF_VACANT    0

static ssize_t
stream_wide_fetch(stream_t *str)
{
	static flag_t shelf;
	ssize_t dist_to_buf_end;
	int ret_val;
	wchar_t *graft_pt;
	wchar_t *next_nl;

	ASSERT(str->s_status & STREAM_OPEN);
	ASSERT((str->s_status & STREAM_EOS_REACHED) == 0);

	graft_pt = str->s_current.l_data.wp + str->s_current.l_data_length + 1;

	if (shelf == SHELF_VACANT)
		str->s_current.l_data.wp = graft_pt;

	dist_to_buf_end = str->s_buffer_size / sizeof (wchar_t) - (graft_pt -
	    (wchar_t *)str->s_buffer);

	if (dist_to_buf_end <= 1) {
		str->s_current.l_data_length = -1;
		return (NEXT_LINE_INCOMPLETE);
	}

	if (fgetws(graft_pt, dist_to_buf_end, str->s_type.BF.s_fp) == NULL) {
		if (feof(str->s_type.BF.s_fp))
			stream_set(str, STREAM_EOS_REACHED);
		else
			terminate(SE_READ_FAILED);
	}

	trip_eof(str->s_type.BF.s_fp);
	/*
	 * Subtract 1 to back off L'\0'.
	 */
	next_nl = xmemwchar(str->s_current.l_data.wp, L'\n', dist_to_buf_end);
	if (next_nl == NULL) {
		str->s_current.l_data_length =
		    MIN(wslen(str->s_current.l_data.wp), dist_to_buf_end);
	} else {
		str->s_current.l_data_length = next_nl -
		    str->s_current.l_data.wp;
	}

	str->s_current.l_collate_length = 0;

	if (*(str->s_current.l_data.wp + str->s_current.l_data_length) ==
	    L'\n') {
		shelf = SHELF_VACANT;
		ret_val = NEXT_LINE_COMPLETE;
		*(str->s_current.l_data.wp +
		    str->s_current.l_data_length) = L'\0';
	} else if (feof(str->s_type.BF.s_fp)) {
		shelf = SHELF_VACANT;
		ret_val = NEXT_LINE_COMPLETE;
		/*
		 * Don't overwrite significant final character.
		 */
		str->s_current.l_data_length++;
		*(str->s_current.l_data.wp +
		    str->s_current.l_data_length) = L'\0';
	} else if (shelf == SHELF_OCCUPIED) {
		terminate(SE_INSUFFICIENT_MEMORY);
	} else {
		shelf = SHELF_OCCUPIED;
		ret_val = NEXT_LINE_INCOMPLETE;
	}

	return (ret_val);
}

ssize_t
stream_wide_fetch_overwrite(stream_t *str)
{
	ssize_t dist_to_buf_end;

	ASSERT(str->s_status & STREAM_OPEN);
	ASSERT((str->s_status & STREAM_EOS_REACHED) == 0);

	str->s_current.l_data.wp = str->s_buffer;
	dist_to_buf_end = str->s_buffer_size / sizeof (wchar_t);

	if (fgetws(str->s_current.l_data.wp, dist_to_buf_end,
	    str->s_type.BF.s_fp) == NULL) {
		if (feof(str->s_type.BF.s_fp))
			stream_set(str, STREAM_EOS_REACHED);
		else
			terminate(SE_READ_FAILED);
	}

	trip_eof(str->s_type.BF.s_fp);
	str->s_current.l_data_length = wslen(str->s_current.l_data.wp) - 1;
	str->s_current.l_collate_length = 0;

	if (*(str->s_current.l_data.wp + str->s_current.l_data_length)
	    != L'\n') {
		if (feof(str->s_type.BF.s_fp))
			str->s_current.l_data_length++;
		else
			terminate(SE_INSUFFICIENT_MEMORY);
	}

	*(str->s_current.l_data.wp + str->s_current.l_data_length) = L'\0';

	return (NEXT_LINE_COMPLETE);
}

static void
stream_wide_send_eol(stream_t *str)
{
	wchar_t w_crlf[2] = { L'\n', L'\0' };

	ASSERT(str->s_status & STREAM_OPEN);
	ASSERT(str->s_status & STREAM_OUTPUT);

	if (wxwrite(str->s_type.SF.s_fd, w_crlf, 1) != 0)
		terminate(SE_WRITE_FAILED, str->s_filename, strerror(errno));
}

static void
stream_wide_put_line(stream_t *str, line_rec_t *line)
{
	ASSERT(str->s_status & STREAM_OPEN);
	ASSERT(str->s_status & STREAM_OUTPUT);

	if (line->l_data_length >= 0) {
		if (wxwrite(str->s_type.SF.s_fd, line->l_data.wp,
		    line->l_data_length) == 0)
			stream_wide_send_eol(str);
		else
			terminate(SE_WRITE_FAILED, str->s_filename,
			    strerror(errno));
	}
}

void
stream_wide_put_line_unique(stream_t *str, line_rec_t *line)
{
	static line_rec_t pvs;

	ASSERT(str->s_status & STREAM_OPEN);
	ASSERT(str->s_status & STREAM_OUTPUT);

	if (collated_wide(&pvs, line, 0, 0) != 0 && line->l_data_length >= 0)
		stream_wide_put_line(str, line);

	copy_line_rec(line, &pvs);
}

extern const stream_ops_t stream_wide_ops = {
	stream_stdio_is_closable,
	stream_stdio_close,
	stream_wide_fetch,
	stream_stdio_flush,
	stream_stdio_free,
	stream_stdio_open_for_write,
	stream_wide_prime,
	stream_wide_put_line,
	stream_wide_send_eol,
	stream_stdio_unlink
};
