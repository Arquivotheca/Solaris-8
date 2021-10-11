/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)streams_array.c	1.3	99/11/24 SMI"

#include "streams_array.h"
#include "streams_common.h"

static int
stream_array_prime(stream_t *str)
{
	ASSERT((str->s_status & STREAM_SOURCE_MASK) == STREAM_ARRAY);

	str->s_type.LA.s_cur_index = MIN(0, str->s_type.LA.s_array_size - 1);
	if (str->s_type.LA.s_cur_index >= 0)
		copy_line_rec(
		    str->s_type.LA.s_array[str->s_type.LA.s_cur_index],
		    &str->s_current);
	else {
		stream_set(str, STREAM_EOS_REACHED);
		return (PRIME_FAILED_EMPTY_FILE);
	}

	return (PRIME_SUCCEEDED);
}

static ssize_t
stream_array_fetch(stream_t *str)
{
	ASSERT(str->s_status & STREAM_OPEN);
	ASSERT(str->s_type.LA.s_cur_index < str->s_type.LA.s_array_size);

	if (++str->s_type.LA.s_cur_index == str->s_type.LA.s_array_size - 1)
		stream_set(str, STREAM_EOS_REACHED);

	copy_line_rec(str->s_type.LA.s_array[str->s_type.LA.s_cur_index],
	    &str->s_current);

	return (NEXT_LINE_COMPLETE);
}

/*ARGSUSED*/
static int
stream_array_is_closable(stream_t *str)
{
	/*
	 * Array streams are not closable.  That is, there is no open file
	 * descriptor directly associated with an array stream.
	 */
	return (0);
}

static int
stream_array_close(stream_t *str)
{
	stream_unset(str, STREAM_OPEN | STREAM_PRIMED);
	return (1);
}

static int
stream_array_free(stream_t *str)
{
	/*
	 * It's now safe for us to close the various streams backing the array
	 * stream's data.
	 */
	stream_unset(str, STREAM_PRIMED | STREAM_NOT_FREEABLE);

	return (1);
}

extern const stream_ops_t stream_array_ops = {
	stream_array_is_closable,
	stream_array_close,
	stream_array_fetch,
	NULL,
	stream_array_free,
	NULL,
	stream_array_prime,
	NULL,
	NULL,
	NULL
};
