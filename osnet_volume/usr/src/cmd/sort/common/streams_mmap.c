/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)streams_mmap.c	1.2	99/01/11 SMI"

#include "streams_mmap.h"
#include "streams_common.h"

static int
stream_mmap_prime(stream_t *str)
{
	if (str->s_status & STREAM_PRIMED)
		return (PRIME_SUCCEEDED);

	stream_set(str, STREAM_PRIMED);

	str->s_current.l_data.sp = str->s_buffer;
	str->s_current.l_data_length = (char *)memchr(str->s_buffer, '\n',
	    str->s_buffer_size) - (char *)str->s_buffer;

	/*
	 * + 1 for new line.
	 */
	if (str->s_current.l_data.sp + str->s_current.l_data_length + 1 >=
	    (char *)str->s_buffer + str->s_buffer_size)
		stream_set(str, STREAM_EOS_REACHED);

	str->s_current.l_collate.sp = NULL;
	str->s_current.l_collate_length = 0;

	return (PRIME_SUCCEEDED);
}

/*
 * stream_mmap_fetch() sets the fields of str->s_current to delimit the next
 * line of the field.
 */
static ssize_t
stream_mmap_fetch(stream_t *str)
{
	ssize_t dist_to_buf_end;
	char *next_nl;

	ASSERT(str->s_status & STREAM_PRIMED);
	ASSERT((str->s_status & STREAM_EOS_REACHED) == 0);

	/*
	 * adding one for newline
	 */
	str->s_current.l_data.sp = str->s_current.l_data.sp +
	    str->s_current.l_data_length + 1;

	dist_to_buf_end = str->s_buffer_size - (str->s_current.l_data.sp
	    - (char *)str->s_buffer);
	ASSERT(dist_to_buf_end >= 0 && dist_to_buf_end <= str->s_buffer_size);

	next_nl = memchr(str->s_current.l_data.sp, '\n', dist_to_buf_end);

	if (next_nl)
		str->s_current.l_data_length = next_nl
		    - str->s_current.l_data.sp;
	else {
		warning(
		    gettext("missing NEWLINE added at end of input file %s"),
		    str->s_filename);
		str->s_current.l_data_length = dist_to_buf_end;
	}

	/*
	 * adding one for newline
	 */
	if (str->s_current.l_data.sp + str->s_current.l_data_length + 1 >=
	    (char *)str->s_buffer + str->s_buffer_size)
		stream_set(str, STREAM_EOS_REACHED);

	str->s_current.l_collate_length = 0;

	return (NEXT_LINE_COMPLETE);
}

static int
stream_mmap_is_closable(stream_t *str)
{
	if (str->s_status & STREAM_OPEN)
		return (1);
	return (0);
}

static int
stream_mmap_close(stream_t *str)
{
	if (str->s_type.SF.s_fd > -1) {
		(void) close(str->s_type.SF.s_fd);
		stream_unset(str, STREAM_OPEN);
		return (1);
	}

	return (0);
}

static int
stream_mmap_free(stream_t *str)
{
	if (!(str->s_status & STREAM_OPEN) ||
	    (str->s_consumer != NULL &&
	    str->s_consumer->s_status & STREAM_NOT_FREEABLE))
		return (0);

	if (str->s_buffer == NULL)
		return (1);

	if (munmap(str->s_buffer, str->s_buffer_size) < 0)
		terminate(SE_MUNMAP_FAILED, str->s_filename);

	str->s_buffer = NULL;
	str->s_buffer_size = 0;

	stream_unset(str, STREAM_PRIMED);

	return (1);
}

extern const stream_ops_t stream_mmap_ops = {
	stream_mmap_is_closable,
	stream_mmap_close,
	stream_mmap_fetch,
	NULL,
	stream_mmap_free,
	NULL,
	stream_mmap_prime,
	NULL,
	NULL,
	stream_stdio_unlink
};
