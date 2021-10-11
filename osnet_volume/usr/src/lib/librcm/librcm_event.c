/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)librcm_event.c	1.1	99/08/10 SMI"

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <door.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <synch.h>
#include <sys/stat.h>

#include "librcm_event.h"

#define	dprint	if (debug) (void) printf
static int debug = 1;

#define	BUF_THRESHOLD	1024	/* larger bufs require a free */

/*
 * Get event service from a named door.
 *
 * Returns -1 if message not delivered. With errno set to cause of error.
 * Returns 0 for success with the results returned in posting buffer.
 */

int
get_event_service(char *door_name, void *data, size_t datalen,
    void **result, size_t *rlen)
{
	int service_door, error = 0;
	door_arg_t door_arg;

	/*
	 * Open the service door
	 */
	if ((service_door = open(door_name, O_RDONLY, 0)) == -1) {
		errno = ESRCH;
		return (-1);
	}

	door_arg.rbuf = NULL;	/* doorfs will provide return buf */
	door_arg.rsize = 0;
	door_arg.data_ptr = data;
	door_arg.data_size = datalen;
	door_arg.desc_ptr = NULL;
	door_arg.desc_num = 0;

	/*
	 * Make door call
	 */
	error = door_call(service_door, &door_arg);
	if ((error == 0) && result) {
		*result = door_arg.rbuf;
		*rlen = door_arg.rsize;
		/*
		 * If requiring a buf free, make another door call
		 *
		 * NB. This only works if the data conforms to the
		 *	sys_event_t * data structure.
		 */
		if (SE_SIZE((sys_event_t *)*result) > BUF_THRESHOLD) {
			door_arg.rbuf = NULL;
			door_arg.rsize = 0;
			door_arg.data_ptr =
			    (void *)&SE_ID((sys_event_t *)*result);
			door_arg.data_size = sizeof (int);
			door_arg.desc_ptr = NULL;
			door_arg.desc_num = 0;
			if (door_call(service_door, &door_arg)) {
				dprint("fail to free event buf in server\n");
			}
		}
	}

	(void) close(service_door);
	return (error);
}

/*
 * Export an event service door
 */
struct door_result {
	struct door_result *next;
	void 	*data;
	uint_t	seq_num;
};

typedef struct door_cookie {
	int		seq_num;
	mutex_t		door_lock;
	void		(*door_func)(void **, size_t *);
	struct door_result *results;
} door_cookie_t;

/*
 * add result to cookie, this is only invoked if result size > 1K
 */
static void
add_door_result(door_cookie_t *cook, void *data)
{
	struct door_result *result;

	/*
	 * Need a better way to handle memory here
	 */
	result = malloc(sizeof (*result));
	while (result == NULL) {
		(void) sleep(1);
		result = malloc(sizeof (*result));
	}
	result->next = NULL;
	result->data = data;

	/*
	 * Attach current door result to the door cookie
	 */
	(void) mutex_lock(&cook->door_lock);
	SE_ID((sys_event_t *)data) = result->seq_num = cook->seq_num++;
	if (cook->results == NULL) {
		cook->results = result;
	} else {
		struct door_result *tmp = cook->results;
		while (tmp->next) {
			tmp = tmp->next;
		}
		tmp->next = result;
	}
	(void) mutex_unlock(&cook->door_lock);
}

/*
 * free a previous door result as described by number.
 */
static void
free_door_result(door_cookie_t *cook, uint_t num)
{
	struct door_result *prev = NULL, *tmp;

	(void) mutex_lock(&cook->door_lock);
	tmp = cook->results;
	while (tmp && tmp->seq_num != num) {
		prev = tmp;
		tmp = tmp->next;
	}

	if (tmp == NULL) {
		dprint("attempting to free nonexistent buf: %u\n", num);
		(void) mutex_unlock(&cook->door_lock);
		return;
	}

	if (prev) {
		prev->next = tmp->next;
	} else {
		cook->results = tmp->next;
	}
	(void) mutex_unlock(&cook->door_lock);

	free(tmp->data);
	free(tmp);
}

static void
door_service(void *cookie, char *args, size_t alen,
    door_desc_t *ddp, uint_t ndid)
{
	char rbuf[BUF_THRESHOLD];
	door_cookie_t *cook = (door_cookie_t *)cookie;

	/*
	 * Special case for asking to free buffer
	 *
	 * NB. This only works if the data conforms to the
	 *	sys_event_t * data structure.
	 */
	if (alen == sizeof (int)) {
		free_door_result(cookie, *(uint_t *)(void *)args);
		(void) door_return(NULL, 0, NULL, 0);
	}

	/*
	 * door_func update args to point to return results.
	 * memory for results are dynamically allocated.
	 */
	(*cook->door_func)((void **)&args, &alen);

	/*
	 * If no results, just return
	 */
	if (args == NULL) {
		(void) door_return(NULL, 0, NULL, 0);
	}

	if (alen <= BUF_THRESHOLD) {
		bcopy(args, rbuf, alen);
		free(args);
		args = rbuf;
	} else {
		/*
		 * for long data, append results to end of queue in cook
		 * and set ndid, ask client to do another door_call
		 * to free the buffer.
		 */
		add_door_result(cook, args);
	}

	(void) door_return(args, alen, NULL, 0);
}

int
create_event_service(char *door_name,
    void (*func)(void **data, size_t *datalen))
{
	int service_door, fd;
	door_cookie_t *cookie;

	/* create an fs file */
	fd = open(door_name, O_EXCL|O_CREAT, S_IREAD|S_IWRITE);
	if ((fd == -1) && (errno != EEXIST)) {
		return (-1);
	}
	(void) close(fd);

	/* allocate space for door cookie */
	if ((cookie = calloc(1, sizeof (*cookie))) == NULL) {
		return (-1);
	}

	cookie->door_func = func;
	if ((service_door = door_create(door_service,
	    (void *)cookie, 0)) == -1) {
		dprint("door create failed: %s\n", strerror(errno));
		free(cookie);
		return (-1);
	}

	(void) fdetach(door_name);
	if (fattach(service_door, door_name) != 0) {
		dprint("door attaching failed: %s\n", strerror(errno));
		free(cookie);
		(void) close(service_door);
		return (-1);
	}

	return (service_door);
}

int
revoke_event_service(int fd)
{
	struct door_info info;
	door_cookie_t *cookie;

	if (door_info(fd, &info) == -1) {
		return (-1);
	}

	if (door_revoke(fd) != 0) {
		return (-1);
	}

	/* wait for existing door calls to finish */
	(void) sleep(1);

	if ((cookie = (door_cookie_t *)info.di_data) != NULL) {
		struct door_result *tmp = cookie->results;
		while (tmp) {
			cookie->results = tmp->next;
			free(tmp->data);
			free(tmp);
			tmp = cookie->results;
		}
		free(cookie);
	}
	return (0);
}

/*
 * Help functions for data packing and processing kernel events
 */
typedef struct se_data_block {
	struct se_data_block 	*next;
	int			size;
	struct se_data_header	data;
	/* data follows */
} se_data_block_t;

/*
 * Allocate memory for storing a userland event
 */
sys_event_t *
se_alloc(int class, int type, int level)
{
	sys_event_t *ev;

	ev = calloc(1, sizeof (*ev));
	if (ev == NULL) {
		return (NULL);
	}

	SE_CLASS(ev) = class;
	SE_TYPE(ev) = type;
	SE_LEVEL(ev) = level;

	return (ev);
}

/*
 * Free memory associated with event
 */
void
se_free(sys_event_t *ev)
{
	se_data_block_t *block;

	block = (se_data_block_t *)(ev->se_data);
	while (block) {
		se_data_block_t *next = block->next;
		free(block);
		block = next;
	}
	free(ev);
}

/*
 * Copy event to contiguous buffer
 */
sys_event_t *
se_end_of_data(sys_event_t *ev)
{
	sys_event_t *copy;
	se_data_block_t *block;
	caddr_t buf;

	if ((copy = calloc(1, SE_SIZE(ev))) == NULL) {
		se_free(ev);
		return (NULL);
	}

	bcopy(ev, copy, sizeof (*ev));
	copy->se_data = 0;
	buf = (caddr_t)copy + sizeof (*ev);

	/* copy data */
	block = (se_data_block_t *)ev->se_data;
	while (block) {
		bcopy(&block->data, buf, block->data.size);
		buf += block->data.size;
		block = block->next;
	}
	se_free(ev);
	return (copy);
}

char *
se_tuple_name(se_data_tuple_t tuple)
{
	if (tuple == NULL) {
		errno = EINVAL;
		return (NULL);
	}
	return ((char *)((caddr_t)tuple + sizeof (*tuple)));
}

int
se_tuple_type(se_data_tuple_t tuple)
{
	if (tuple == NULL) {
		errno = EINVAL;
		return (-1);
	}
	return (tuple->type);
}

int
se_tuple_ints(se_data_tuple_t tuple, int **data)
{
	if (tuple == NULL) {
		errno = EINVAL;
		return (0);
	}
	*data = (int *)(void *)((caddr_t)tuple + tuple->val);
	return (tuple->nitems);
}

int
se_tuple_bytes(se_data_tuple_t tuple, uchar_t **data)
{
	if (tuple == NULL) {
		errno = EINVAL;
		return (0);
	}
	*data = (uchar_t *)((caddr_t)tuple + tuple->val);
	return (tuple->nitems);
}

int
se_tuple_strings(se_data_tuple_t tuple, char **data)
{
	if (tuple == NULL) {
		errno = EINVAL;
		return (0);
	}
	*data = (char *)((caddr_t)tuple + tuple->val);
	return (tuple->nitems);
}

se_data_tuple_t
se_get_next_tuple(sys_event_t *ev, se_data_tuple_t tuple)
{
	if (tuple == NULL) {
		if (SE_DATALEN(ev) == 0) {
			return (NULL);
		}
		/*LINTED*/
		return ((se_data_tuple_t)((caddr_t)ev + sizeof (*ev)));
	}

	if (tuple->next != 0) {
		/*LINTED*/
		return ((se_data_tuple_t)((caddr_t)tuple + tuple->size));
	}

	return (NULL);
}

int
se_lookup_common(sys_event_t *ev, char *name, char **data, char type)
{
	se_data_tuple_t tuple = NULL;

	while (tuple = se_get_next_tuple(ev, tuple)) {
		if ((se_tuple_type(tuple) == type) &&
		    (strcmp(name, se_tuple_name(tuple)) == 0)) {
			*data = (caddr_t)tuple + tuple->val;
			return (tuple->nitems);
		}
	}
	*data = NULL;
	return (0);
}

int
se_lookup_ints(sys_event_t *ev, char *name, int **data)
{
	return (se_lookup_common(ev, name, (char **)data, SE_DATA_TYPE_INT));
}

int
se_lookup_bytes(sys_event_t *ev, char *name, uchar_t **data)
{
	return (se_lookup_common(ev, name, (char **)data, SE_DATA_TYPE_BYTE));
}

int
se_lookup_strings(sys_event_t *ev, char *name, char **data)
{
	return (se_lookup_common(ev, name, data, SE_DATA_TYPE_STRING));
}

/*
 * Append event data tuple to an event
 */
#define	SE_ALIGN(x)	((((ulong_t)x) + 7ul) & ~7ul)

static se_data_header_t *
se_get_data_buf(sys_event_t *ev, int len)
{
	se_data_block_t *blk, *blk1;
	int size;

	/*
	 * Allocate buffer for data block
	 */
	size = len + offsetof(se_data_block_t, data);
	blk = calloc(1, size);
	if (blk == NULL) {
		return (NULL);
	}
	blk->size = size;

	/*
	 * Put blk at end of data block queue
	 */
	if ((blk1 = (se_data_block_t *)ev->se_data) == NULL) {
		ev->se_data = (uint64_t)blk;
		return (&blk->data);
	}

	while (blk1->next) {
		blk1 = blk1->next;
	}
	blk1->data.next = 1;
	blk1->next = blk;
	return (&blk->data);
}

static int
se_append_common(sys_event_t *ev, char *name, void *data, int nitems,
    char datatype)
{
	int len, datalen = 0;
	caddr_t addr;
	se_data_header_t *hd;

	/* Get data length */
	switch (datatype) {
	case SE_DATA_TYPE_BYTE:
		datalen += nitems;
		break;
	case SE_DATA_TYPE_INT:
		datalen += nitems * sizeof (int);
		break;
	case SE_DATA_TYPE_STRING: {
		int i;
		char *str = (char *)data;
		for (i = 0; i < nitems; i++) {
			datalen += strlen(str) + 1;
			str += strlen(str) + 1;
		}
		break;
	}
	default:
		/* shouldn't reach here */
		return (EFAULT);
	}

	/* add size of block header and name string */
	len = sizeof (*hd) + strlen(name) + 1;
	len = SE_ALIGN(len) + SE_ALIGN(datalen);

	hd = se_get_data_buf(ev, len);
	if (hd == NULL) {
		return (ENOMEM);
	}

	/* fill in various header struct */
	SE_DATALEN(ev) += len;
	hd->size = len;
	hd->nitems = nitems;
	hd->type = datatype;
	hd->next = 0;

	/* copy name and value */
	addr = (caddr_t)hd + sizeof (*hd);
	(void) strcpy(addr, name);
	addr += strlen(name) + 1;
	addr = (caddr_t)SE_ALIGN(addr);
	bcopy(data, addr, datalen);
	hd->val = addr - (caddr_t)hd;

	return (0);
}

int
se_append_bytes(sys_event_t *ev, char *name, uchar_t *data, int nitems)
{
	return (se_append_common(ev, name, (void *)data, nitems,
	    SE_DATA_TYPE_BYTE));
}

int
se_append_ints(sys_event_t *ev, char *name, int *data, int nitems)
{
	return (se_append_common(ev, name, (void *)data, nitems,
	    SE_DATA_TYPE_INT));
}

int
se_append_strings(sys_event_t *ev, char *name, char *data, int nitems)
{
	return (se_append_common(ev, name, (void *)data, nitems,
	    SE_DATA_TYPE_STRING));
}

void
se_print(FILE *fp, sys_event_t *ev)
{
	int i, items, *ints;
	uchar_t *bytes;
	char *strings;
	se_data_tuple_t tuple;

	(void) fprintf(fp, "received message id = 0x%x\n", SE_ID(ev));
	(void) fprintf(fp, "\tclass = %d\n", SE_CLASS(ev));
	(void) fprintf(fp, "\ttype = %d\n", SE_TYPE(ev));

	tuple = se_get_next_tuple(ev, NULL);

	while (tuple) {
		(void) fprintf(fp, "\t%s = ", se_tuple_name(tuple));
		switch (se_tuple_type(tuple)) {
		case SE_DATA_TYPE_INT:
			items = se_tuple_ints(tuple, &ints);
			for (i = 0; i < items; i++) {
				(void) fprintf(fp, "%d ", ints[i]);
			}
			break;
		case SE_DATA_TYPE_BYTE:
			items = se_tuple_bytes(tuple, &bytes);
			(void) fprintf(fp, "0x");
			for (i = 0; i < items; i++) {
				(void) fprintf(fp, "%2.2x ", bytes[i]);
			}
			break;
		case SE_DATA_TYPE_STRING:
			items = se_tuple_strings(tuple, &strings);
			for (i = 0; i < items; i++) {
				(void) fprintf(fp, "%s  ", strings);
				strings += strlen(strings) + 1;
			}
			break;
		default:
			(void) fprintf(fp, "unknown data type");
			break;
		}
		(void) fprintf(fp, "\n");
		tuple = se_get_next_tuple(ev, tuple);
	}
}
