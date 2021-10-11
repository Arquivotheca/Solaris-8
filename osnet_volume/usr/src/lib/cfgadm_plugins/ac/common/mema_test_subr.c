/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mema_test_subr.c	1.2	98/04/20 SMI"

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/param.h>
#include <memory.h>
#include <config_admin.h>
#include "mema_test.h"

void *
mtest_allocate_buf(
	mtest_handle_t handle,
	size_t size)
{
	struct mtest_alloc_ent *new_ent;

	new_ent =
	    (struct mtest_alloc_ent *)malloc(sizeof (struct mtest_alloc_ent));
	if (new_ent == NULL)
		return (NULL);

	new_ent->buf = malloc(size);
	if (new_ent->buf == NULL) {
		free((void *)new_ent);
		return (NULL);
	}
	/* TODO: probably not thread safe? */
	new_ent->next = handle->alloc_list;
	handle->alloc_list = new_ent;

	return (new_ent->buf);
}

/* This routine dedicated to George Cameron */
void
mtest_deallocate_buf(
	mtest_handle_t handle,
	void *buf)
{
	struct mtest_alloc_ent **p, *p1;

	p = &handle->alloc_list;
	while ((*p) != NULL && (*p)->buf != buf)
		p = &(*p)->next;
	assert((*p) != NULL);
	p1 = *p;
	*p = (*p)->next;
	free(p1->buf);
	free((void *)p1);
}

void
mtest_deallocate_buf_all(mtest_handle_t handle)
{
	struct mtest_alloc_ent *p1;

	while ((p1 = handle->alloc_list) != NULL) {
		handle->alloc_list = p1->next;
		free(p1->buf);
		free((void *)p1);
	}
}

void
mtest_message(mtest_handle_t handle, const char *msg)
{
	if (handle->msgp != NULL && handle->msgp->message_routine != NULL) {
		(*handle->msgp->message_routine)(handle->msgp->appdata_ptr,
		    msg);
	}
}
