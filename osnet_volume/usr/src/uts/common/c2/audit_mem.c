/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)audit_mem.c	1.21	99/08/31 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/t_lock.h>
#include <sys/thread.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <c2/audit.h>
#include <c2/audit_kernel.h>
#include <c2/audit_record.h>

/*
 * extern kmutex_t au_queue_update_lock;
 * extern kmutex_t au_queue_active_lock;
 */


au_buff_t *au_free_queue;

/*
 * free audit data structure pool
 */
struct p_audit_data *au_plist = NULL;
struct t_audit_data *au_tlist = NULL;
struct f_audit_data *au_flist = NULL;
/*
 * memory alloc statistics
 */
struct au_list_stat au_plist_stat = {0, 0, 0, 0, 0};
struct au_list_stat au_tlist_stat = {0, 0, 0, 0, 0};
struct au_list_stat au_flist_stat = {0, 0, 0, 0, 0};
/*
 * free queue statistics
 */
struct au_list_stat au_mem_stat = {0, 0, 0, 0, 0};

int tlist_size = 30;
int plist_size = 30;
int flist_size = 30;

kmutex_t au_free_queue_lock;
kmutex_t  au_tlist_lock; /* thread pool lock */
kmutex_t  au_flist_lock; /* file pool lock */
kmutex_t  au_plist_lock; /* process pool lock */

extern kmutex_t au_stat_lock;

/*
 * Function: au_get_chunk
 * args:
 *	int number;		Number of pages to allocate
*/
int
au_get_chunk(int number)
{
	int i, j;
	au_buff_t *heads, *free_tail;
	caddr_t buffers;

	if (number <= 0)
		return (-1);

	/* Find the end of the free queue */

	free_tail = au_free_queue;
	while ((free_tail != NULL) && (free_tail->next_buf != NULL))
		free_tail = free_tail->next_buf;

	for (i = 0; i < number; i++) {

		buffers = kmem_zalloc(AU_PAGE, KM_SLEEP);
		AS_INC(as_memused, AU_PAGE);

		heads = kmem_zalloc(AU_CNTL, KM_SLEEP);

		AS_INC(as_memused, AU_CNTL);

		for (j = 0; j < ONPAGE; j++) {
			(heads+j)->buf = buffers + j * AU_BUFSIZE;
			(heads+j)->next_buf = heads + j + 1;
		}
		(heads + ONPAGE - 1)->next_buf = NULL;
		if (au_free_queue == NULL) {
			au_free_queue = heads;
			free_tail = heads + ONPAGE - 1;
		} else {
			free_tail->next_buf = heads;
			free_tail = heads+ONPAGE - 1;
		}
		au_mem_stat.size += ONPAGE;
	}
	return (0);
}

/*
 * Function: au_get_buf
 * args:
 *	int wait;	can we sleep
 */
struct au_buff *
au_get_buff(int wait)
{
	au_buff_t *buffer = NULL;

	mutex_enter(&au_free_queue_lock);

	if (au_free_queue == NULL) {
		QUEUE_MISS;
		if (!wait || au_get_chunk(1)) {
			mutex_exit(&au_free_queue_lock);
			return (NULL);
		}
	} else {
		QUEUE_HIT;
	}

	buffer = au_free_queue;
	au_free_queue = au_free_queue->next_buf;
	mutex_exit(&au_free_queue_lock);
	buffer->next_buf = NULL;
	return (buffer);
}

/*
 * Function: au_free_buf
 * args:
 *	au_buff_t *buf;		first buffer in the chain
 */
au_buff_t *
au_free_buf(au_buff_t *buf)
{
	au_buff_t *next = NULL;

	next = buf->next_buf;
	bzero(buf->buf, AU_BUFSIZE);
	buf->next_rec = NULL;
	buf->rec_len = 0;
	buf->len = 0;
	buf->flag = 0;

	mutex_enter(&au_free_queue_lock);
	buf->next_buf = au_free_queue;
	au_free_queue = buf;
	QUEUE_FREE;
	mutex_exit(&au_free_queue_lock);

	return (next);
}

/*
 * Function: au_free_rec
 * args:
 *	au_buff_t *buf;		start of the record chain
 */
void
au_free_rec(au_buff_t *buf)
{

	while (buf != NULL) {
		buf = au_free_buf(buf);
	}
}

/*
 * Function: au_append_rec
 * args:
 *	au_buff_t *rec;		start of the record chain
 *	au_buff_t *buf;		buffer to append
 */
au_append_rec(au_buff_t *rec, au_buff_t *buf)
{
	au_buff_t *head;

	if (!rec)
		return (-1);

	head = rec;
	while (rec->next_buf)
		rec = rec->next_buf;
	if ((int)(rec->len + buf->len) <= AU_BUFSIZE) {
		bcopy(buf->buf, (char *)(rec->buf + rec->len),
		    (uint_t)buf->len);
		rec->len += buf->len;
		rec->next_buf = buf->next_buf;
		(void) au_free_buf(buf);
	} else {
		rec->next_buf = buf;
		head->rec_len++;
	}
	return (0);
}

/*
 * Function: au_append_buf
 * args:
 *	char *data;		data buffer to append
 *	int len;		size of data to append
 *	au_buff_t *buf;		buffer to append to
 */
au_append_buf(const char *data, int len, au_buff_t *buf)
{
	au_buff_t *new_buf;
	int	new_len;

	while (buf->next_buf != NULL)
		buf = buf->next_buf;

	new_len = (uint_t)(buf->len + len) > AU_BUFSIZE ?
		AU_BUFSIZE - buf->len : len;
	bcopy(data, (buf->buf + buf->len), (uint_t)new_len);
	buf->len += (uchar_t)new_len;
	len -= new_len;

	while (len > NULL) {
		data += new_len;
		if ((new_buf = au_get_buff(WAIT)) == NULL) {
			return (-1);
		}
		buf->next_buf = new_buf;
		buf = new_buf;
		new_len = len > AU_BUFSIZE ? AU_BUFSIZE : len;
		bcopy(data, buf->buf, (uint_t)new_len);
		buf->len = (uchar_t)new_len;
		len -= new_len;
	}

	return (0);
}

void
au_mem_init()
{

	mutex_init(&au_tlist_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&au_flist_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&au_plist_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&au_free_queue_lock, NULL, MUTEX_DEFAULT, NULL);

	/*
	 * we are single threaded so we don't need to lock
	 * the free-queue allocate 2 pages for the initial free queue
	 */
	if (au_get_chunk(2) == -1) {
		panic("unable to allocate memory for auditing\n");
	}
}
