/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)streams_stdio.c	1.6	99/11/24 SMI"

#include "streams_stdio.h"
#include "streams_common.h"

int
stream_stdio_open_for_write(stream_t *str)
{
	stream_simple_file_t	*SF = &(str->s_type.SF);

	ASSERT(!(str->s_status & STREAM_OPEN));
	ASSERT(!(str->s_status & STREAM_OUTPUT));

	if (str->s_status & STREAM_NOTFILE)
		SF->s_fd = fileno(stdout);
	else
		if ((SF->s_fd = open(str->s_filename, O_CREAT | O_TRUNC |
		    O_WRONLY, OUTPUT_MODE)) < 0) {
			if (errno == EMFILE || errno == ENFILE)
				return (-1);
			else
				terminate(SE_CANT_OPEN_FILE, str->s_filename);
		}

	stream_set(str, STREAM_OPEN | STREAM_OUTPUT);

	return (1);
}

/*
 * In the case of an instantaneous stream, we allocate a small buffer (64k) here
 * for the stream; otherwise, the s_buffer and s_buffer_size members should have
 * been set by stream_set_size() prior to calling stream_prime().
 */
static int
stream_stdio_prime(stream_t *str)
{
	stream_buffered_file_t *BF = &(str->s_type.BF);
	char *current_position;
	char *end_of_buffer;
	char *next_nl;

	ASSERT(!(str->s_status & STREAM_OUTPUT));
	ASSERT(str->s_status & STREAM_OPEN);

	if (str->s_status & STREAM_INSTANT && (str->s_buffer == NULL)) {
		str->s_buffer = xzmap(0, STDIO_VBUF_SIZE, PROT_READ |
		    PROT_WRITE, MAP_PRIVATE, 0);
		if (str->s_buffer == MAP_FAILED)
			terminate(SE_MMAP_FAILED);
		str->s_buffer_size = STDIO_VBUF_SIZE;
	}

	ASSERT(str->s_buffer != NULL);

	if (str->s_status & STREAM_PRIMED) {
		/*
		 * l_data_length is only set to -1 in the case of coincidental
		 * exhaustion of the input butter.  This is thus the only case
		 * which involves no copying on a re-prime.
		 */
		ASSERT(str->s_current.l_data_length >= -1);
		(void) memcpy(str->s_buffer, str->s_current.l_data.sp,
		    str->s_current.l_data_length + 1);
		str->s_current.l_data.sp = str->s_buffer;

		if (SOP_FETCH(str) == NEXT_LINE_INCOMPLETE)
			terminate(SE_INSUFFICIENT_MEMORY);

		return (PRIME_SUCCEEDED);
	}

	stream_set(str, STREAM_PRIMED);

	current_position = (char *)str->s_buffer;
	end_of_buffer = (char *)str->s_buffer + str->s_buffer_size;

	trip_eof(BF->s_fp);
	if (!feof(BF->s_fp))
		(void) fgets(current_position, end_of_buffer - current_position,
		    BF->s_fp);
	else {
		stream_set(str, STREAM_EOS_REACHED);
		return (PRIME_FAILED_EMPTY_FILE);
	}

	str->s_current.l_data.sp = current_position;
	/*
	 * Because one might run sort on a binary file, strlen() is no longer
	 * trustworthy--we must explicitly search for a newline.
	 */
	if ((next_nl = memchr(current_position, '\n',
	    end_of_buffer - current_position)) == NULL)
		str->s_current.l_data_length = end_of_buffer - current_position;
	else
		str->s_current.l_data_length = next_nl - current_position;

	str->s_current.l_collate.sp = NULL;
	str->s_current.l_collate_length = 0;

	return (PRIME_SUCCEEDED);
}

/*
 * stream_stdio_fetch() guarantees the return of a complete line, or a flag
 * indicating that the complete line could not be read.
 */
#define	SHELF_OCCUPIED	1
#define	SHELF_VACANT	0

static ssize_t
stream_stdio_fetch(stream_t *str)
{
	static flag_t shelf;
	ssize_t	dist_to_buf_end;
	int ret_val;
	char *graft_pt, *next_nl;

	ASSERT(str->s_status & STREAM_OPEN);
	ASSERT((str->s_status & STREAM_EOS_REACHED) == 0);

	graft_pt = str->s_current.l_data.sp + str->s_current.l_data_length + 1;

	if (shelf == SHELF_VACANT)
		/*
		 * The graft point is the start of the current line.
		 */
		str->s_current.l_data.sp = graft_pt;

	dist_to_buf_end = str->s_buffer_size - (graft_pt -
	    (char *)str->s_buffer);

	if (dist_to_buf_end <= 1) {
		/*
		 * fgets()'s behaviour in the case of a one-character buffer is
		 * somewhat unhelpful:  it fills the buffer with '\0' and
		 * returns successfully (even if EOF has been reached for the
		 * file in question).  Since we may be in the middle of a
		 * grafting operation, we leave early, maintaining the shelf in
		 * its current state.
		 */
		str->s_current.l_data_length = -1;
		return (NEXT_LINE_INCOMPLETE);
	}

	if (fgets(graft_pt, dist_to_buf_end, str->s_type.BF.s_fp) == NULL) {
		if (feof(str->s_type.BF.s_fp))
			stream_set(str, STREAM_EOS_REACHED);
		else
			terminate(SE_READ_FAILED);
	}

	trip_eof(str->s_type.BF.s_fp);
	/*
	 * Because one might run sort on a binary file, strlen() is no longer
	 * trustworthy--we must explicitly search for a newline.
	 */
	if ((next_nl = memchr(str->s_current.l_data.sp, '\n',
	    dist_to_buf_end)) == NULL)
		str->s_current.l_data_length = dist_to_buf_end - 1;
	else
		str->s_current.l_data_length = next_nl -
		    str->s_current.l_data.sp;

	str->s_current.l_collate_length = 0;

	if (*(str->s_current.l_data.sp + str->s_current.l_data_length) !=
	    '\n' && !feof(str->s_type.BF.s_fp)) {
		/*
		 * We were only able to read part of the line; note that we have
		 * something on the shelf for our next fetch.  If the shelf was
		 * previously occupied, and we still can't get the entire line,
		 * then we need more resources.
		 */
		if (shelf == SHELF_OCCUPIED)
			terminate(SE_INSUFFICIENT_MEMORY);

		shelf = SHELF_OCCUPIED;
		ret_val = NEXT_LINE_INCOMPLETE;
	} else {
		shelf = SHELF_VACANT;
		ret_val = NEXT_LINE_COMPLETE;
	}

	return (ret_val);
}

/*
 * stdio_fetch_overwrite() is used when we are performing an operation where we
 * need the buffer contents only over a single period.  (merge and check are
 * operations of this kind.)  In this case, we read the current line at the head
 * of the stream's defined buffer.  If we cannot read the entire line, we have
 * not allocated sufficient memory.
 */
ssize_t
stream_stdio_fetch_overwrite(stream_t *str)
{
	ssize_t	dist_to_buf_end;

	ASSERT(str->s_status & STREAM_OPEN);
	ASSERT((str->s_status & STREAM_EOS_REACHED) == 0);

	str->s_current.l_data.sp = str->s_buffer;
	dist_to_buf_end = str->s_buffer_size;

	if (fgets(str->s_current.l_data.sp, dist_to_buf_end,
	    str->s_type.BF.s_fp) == NULL) {
		if (feof(str->s_type.BF.s_fp))
			stream_set(str, STREAM_EOS_REACHED);
		else
			terminate(SE_READ_FAILED);
	}

	trip_eof(str->s_type.BF.s_fp);
	str->s_current.l_data_length = strlen(str->s_current.l_data.sp) - 1;
	str->s_current.l_collate_length = 0;

	if (*(str->s_current.l_data.sp + str->s_current.l_data_length) !=
	    '\n' && !feof(str->s_type.BF.s_fp)) {
		/*
		 * In the overwrite case, failure to read the entire line means
		 * our buffer size was insufficient (as we are using all of it).
		 * Exit, requesting more resources.
		 */
		terminate(SE_INSUFFICIENT_MEMORY);
	}

	return (NEXT_LINE_COMPLETE);
}

int
stream_stdio_is_closable(stream_t *str)
{
	if (str->s_status & STREAM_OPEN && !(str->s_status & STREAM_NOTFILE))
		return (1);
	return (0);
}

int
stream_stdio_close(stream_t *str)
{
	ASSERT(str->s_status & STREAM_OPEN);

	if (!(str->s_status & STREAM_OUTPUT)) {
		if (!(str->s_status & STREAM_NOTFILE))
			(void) fclose(str->s_type.BF.s_fp);

		if (str->s_type.BF.s_vbuf != NULL) {
			free(str->s_type.BF.s_vbuf);
			str->s_type.BF.s_vbuf = NULL;
		}
	} else {
		if (cxwrite(str->s_type.SF.s_fd, NULL, 0) == 0)
			(void) close(str->s_type.SF.s_fd);
		else
			terminate(SE_WRITE_FAILED, str->s_filename,
			    strerror(errno));
	}

	stream_unset(str, STREAM_OPEN | STREAM_PRIMED | STREAM_OUTPUT);
	return (1);
}

static void
stream_stdio_send_eol(stream_t *str)
{
	ASSERT(str->s_status & STREAM_OPEN);
	ASSERT(str->s_status & STREAM_OUTPUT);

	if (cxwrite(str->s_type.SF.s_fd, "\n", 1) != 0)
		terminate(SE_WRITE_FAILED, str->s_filename, strerror(errno));
}

void
stream_stdio_flush(stream_t *str)
{
	ASSERT(str->s_status & STREAM_OPEN);
	ASSERT(str->s_status & STREAM_OUTPUT);

	if (cxwrite(str->s_type.SF.s_fd, NULL, 0) != 0)
		terminate(SE_WRITE_FAILED, str->s_filename, strerror(errno));
}

static void
stream_stdio_put_line(stream_t *str, line_rec_t *line)
{
	ASSERT(str->s_status & STREAM_OPEN);
	ASSERT(str->s_status & STREAM_OUTPUT);

	if (line->l_data_length >= 0) {
		if (cxwrite(str->s_type.SF.s_fd, line->l_data.sp,
		    line->l_data_length) == 0)
			stream_stdio_send_eol(str);
		else
			terminate(SE_WRITE_FAILED, str->s_filename,
			    strerror(errno));
	}
	safe_free(line->l_raw_collate.sp);
	line->l_raw_collate.sp = NULL;
}

void
stream_stdio_put_line_unique(stream_t *str, line_rec_t *line)
{
	static line_rec_t pvs;

	ASSERT(str->s_status & STREAM_OPEN);
	ASSERT(str->s_status & STREAM_OUTPUT);

	if ((pvs.l_collate.sp == NULL ||
	    collated(&pvs, line, 0, COLL_UNIQUE) != 0) &&
	    line->l_data_length >= 0)
		stream_stdio_put_line(str, line);

	copy_line_rec(line, &pvs);
}

int
stream_stdio_unlink(stream_t *str)
{
	ASSERT(!(str->s_status & STREAM_OPEN));

	if (!(str->s_status & STREAM_NOTFILE))
		return (unlink(str->s_filename));

	return (0);
}

int
stream_stdio_free(stream_t *str)
{
	/*
	 * Unmap the memory we allocated for input, if it's valid to do so.
	 */
	if (!(str->s_status & STREAM_OPEN) ||
	    (str->s_consumer != NULL &&
	    str->s_consumer->s_status & STREAM_NOT_FREEABLE))
		return (0);

	if (str->s_buffer != NULL) {
		if (munmap(str->s_buffer, str->s_buffer_size) < 0)
			terminate(SE_MUNMAP_FAILED, "/dev/zero");
		else {
			str->s_buffer = NULL;
			str->s_buffer_size = 0;
		}
	}

	stream_unset(str, STREAM_PRIMED | STREAM_INSTANT);

	return (1);
}

extern const stream_ops_t stream_stdio_ops = {
	stream_stdio_is_closable,
	stream_stdio_close,
	stream_stdio_fetch,
	stream_stdio_flush,
	stream_stdio_free,
	stream_stdio_open_for_write,
	stream_stdio_prime,
	stream_stdio_put_line,
	stream_stdio_send_eol,
	stream_stdio_unlink
};
