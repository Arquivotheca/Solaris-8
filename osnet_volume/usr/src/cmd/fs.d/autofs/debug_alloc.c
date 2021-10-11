/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)debug_alloc.c	1.4	98/02/10 SMI"

#ifdef	MALLOC_DEBUG
#include <stdlib.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>

/*
 * To use debugging facility, compile with * -DMALLOC_DEBUG.
 * You can do this by setting the environment variable
 * MALLOC_DEBUG to "-DMALLOC_DEBUG"
 *
 * To make automountd dump trace records (i.e. make it call check_leaks),
 * run:
 * 	make malloc_dump
 */

struct alloc_list
{
	char alloc_type[20];
	void *addr;
	int size;
	char file[80];
	int line;
	struct alloc_list *next;
};

static struct alloc_list *halist = NULL;
static mutex_t alloc_list_lock = DEFAULTMUTEX;

void *
my_malloc(size_t size, const char *file, int line)
{
	struct alloc_list *alist = NULL;

	/* allocate the list item */
	alist = (struct alloc_list *) malloc(sizeof (*alist));
	if (alist == NULL) {
		syslog(LOG_ERR, "my_malloc: out of memory\n");
		return (NULL);
	}
	strcpy(alist->alloc_type, "MALLOC");
	alist->addr = (void *)malloc(size);
	alist->size = size;
	strcpy(alist->file, file);
	alist->line = line;

	/* add it to the head of the list */
	mutex_lock(&alloc_list_lock);
	if (halist == NULL)
		alist->next = NULL;
	else
		alist->next = halist;
	halist = alist;
	mutex_unlock(&alloc_list_lock);

	return (alist->addr);
}

void *
my_realloc(void *addr, size_t size, const char *file, int line)
{
	struct alloc_list *alist, *alist_prev = NULL;
	void *ptr;
	char alloctype[80];

	mutex_lock(&alloc_list_lock);
	alist = halist;
	while (alist != NULL) {
		if (addr == alist->addr) {
			if (halist == alist)
				halist = halist->next;
			else {
				alist_prev->next = alist->next;
			}
			mutex_unlock(&alloc_list_lock);
			break;
		}
		alist_prev = alist;
		alist = alist->next;
	}

	if (alist == NULL) {
	    syslog(LOG_ERR, "*** POSSIBLE CORRUPTION ****\n");
	    syslog(LOG_ERR, "\t realloc: unallocated bytes at %p in %s/%d\n",
		addr, file, line);

	    /* allocate the list item */

	    alist = (struct alloc_list *) malloc(sizeof (*alist));
	    if (alist == NULL) {
		    mutex_unlock(&alloc_list_lock);
		    syslog(LOG_ERR, "my_malloc: out of memory\n");
		    return (NULL);
	    }
	}

	ptr = (void *)realloc(addr, size);
	if (ptr == NULL) {
		    mutex_unlock(&alloc_list_lock);
		    return (NULL);
	}


	strcpy(alist->alloc_type, "REALLOC");
	alist->addr = (void *)ptr;
	alist->size = size;
	strcpy(alist->file, file);
	alist->line = line;

	/* add it to the head of the list */
	if (halist == NULL)
		alist->next = NULL;
	else
		alist->next = halist;
	halist = alist;
	mutex_unlock(&alloc_list_lock);

	return (ptr);
}

void
my_free(void *addr, const char *file, int line)
{
	struct alloc_list *alist, *alist_prev = NULL;
	char alloctype[80];

	mutex_lock(&alloc_list_lock);
	alist = halist;
	while (alist != NULL) {
		if (addr == alist->addr) {
			if (halist == alist)
				halist = halist->next;
			else {
				alist_prev->next = alist->next;
			}
			free(alist);
			alist = NULL;
			free(addr);
			mutex_unlock(&alloc_list_lock);
			return;
		}
		alist_prev = alist;
		alist = alist->next;
	}

	syslog(LOG_ERR, "*** POSSIBLE CORRUPTION ****\n");
	syslog(LOG_ERR, "\t free: unallocated bytes at %p in %s/%d\n",
		addr, file, line);
	free(addr);
	mutex_unlock(&alloc_list_lock);
}

char *
my_strdup(const char *straddr, const char *file, int line)
{
	struct alloc_list *alist = NULL;

	/* allocate the list item */
	alist = (struct alloc_list *) malloc(sizeof (*alist));
	if (alist == NULL) {
		syslog(LOG_ERR, "my_strdup: out of memory\n");
		return (NULL);
	}
	strcpy(alist->alloc_type, "STRDUP");
	alist->addr = strdup(straddr);
	alist->size = strlen(straddr);
	strcpy(alist->file, file);
	alist->line = line;

	/* put it onto the head of the list - don't bother about reallocs */
	mutex_lock(&alloc_list_lock);
	if (halist == NULL)
		alist->next = NULL;
	else
		alist->next = halist;
	halist = alist;
	mutex_unlock(&alloc_list_lock);

	return ((char *)alist->addr);
}

void
check_leaks(char *filename)
{
	struct alloc_list *alist;

	FILE *fp;
	fp = fopen(filename, "a");
	if (fp == NULL) {
		syslog(LOG_ERR, "check_leaks, could not open file: %s",
			filename);
		return;
	}

	fprintf(fp, "*** POSSIBLE LEAKS ****\n");
	mutex_lock(&alloc_list_lock);
	alist = halist;
	while (alist != NULL) {
		fprintf(fp, "\t%s: %d bytes at %p in %s/%d\n",
			alist->alloc_type, alist->size, alist->addr,
			alist->file, alist->line);
		alist = alist->next;
	}
	mutex_unlock(&alloc_list_lock);

	(void) fclose(fp);
}
#else
/*
 * To prevent a compiler warning.
 */
static char filler;
#endif
