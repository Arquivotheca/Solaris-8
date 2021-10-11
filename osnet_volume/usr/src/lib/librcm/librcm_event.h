/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LIBRCM_EVENT_H
#define	_LIBRCM_EVENT_H

#pragma ident	"@(#)librcm_event.h	1.1	99/08/10 SMI"

#include <stdio.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This header file contains data format definitions for packing data
 * and passing between RCM consumers and rcm_daemon. The data format
 * is generally useful for packing typed <name=value> pairs into
 * contiguous buffers to be passed between processes and between kernel
 * and userland.
 *
 * These definitions should eventually be moved to libsysevent
 */

/* Internal data packing format */

#define	SE_DATA_TYPE_BYTE	0
#define	SE_DATA_TYPE_INT	1
#define	SE_DATA_TYPE_STRING	2

typedef struct se_data_header {
	int size;	/* total length of this name-val pair */
	int nitems;	/* number of items in value */
	char type;	/* value type */
	char next;	/* 0 if last name-val pair */
	short val;	/* offset to value, 8-byte aligned  */
	/* name string, followed by value */
} se_data_header_t;

typedef se_data_header_t *se_data_tuple_t;	/* opaque structure */

typedef uint32_t sys_event_id_t;

typedef struct sys_event {
	uint32_t	se_version;
	uint32_t	se_class;
	uint32_t	se_type;
	uint32_t	se_level;
	uint32_t	se_status;	/* status of event delivery */
	sys_event_id_t	se_id;		/* unique id */
	uint32_t	se_datalen;	/* length of event data */
	uint64_t	se_data;	/* pointer to data blocks */
} sys_event_t;

#define	SYS_EVENT_VERSION	0

#define	SE_VERSION(ev)	((ev)->se_version)
#define	SE_CLASS(ev)	((ev)->se_class)
#define	SE_TYPE(ev)	((ev)->se_type)
#define	SE_LEVEL(ev)	((ev)->se_level)
#define	SE_STATUS(ev)	((ev)->se_status)
#define	SE_ID(ev)	((ev)->se_id)
#define	SE_DATALEN(ev)	((ev)->se_datalen)
#define	SE_SIZE(ev)	(sizeof (*(ev)) + SE_DATALEN(ev))

/*
 * Event classes
 *
 * The following is a list of system defined events.
 * Event classes are unique within the event framework.
 */
#define	EC_RCM		1	/* events used by the RCM framework */

/*
 * librcm message passing interfaces
 */
int get_event_service(char *door_name, void *data, size_t datalen,
    void **result, size_t *rlen);
int create_event_service(char *door_name, void (*func)(void **, size_t *));
int revoke_event_service(int door_fd);

/* help functions */
sys_event_t *se_alloc(int, int, int);
int se_append_ints(sys_event_t *, char *, int *, int);
int se_append_bytes(sys_event_t *, char *, uchar_t *, int);
int se_append_strings(sys_event_t *, char *, char *, int);
sys_event_t *se_end_of_data(sys_event_t *);
void se_free(sys_event_t *);

int se_lookup_ints(sys_event_t *, char *, int **);
int se_lookup_bytes(sys_event_t *, char *, uchar_t **);
int se_lookup_strings(sys_event_t *, char *, char **);
se_data_tuple_t se_get_next_tuple(sys_event_t *, se_data_tuple_t);
int se_tuple_type(se_data_tuple_t);
char *se_tuple_name(se_data_tuple_t);
int se_tuple_ints(se_data_tuple_t, int **data);
int se_tuple_bytes(se_data_tuple_t, uchar_t **data);
int se_tuple_strings(se_data_tuple_t, char **data);
void se_print(FILE *fp, sys_event_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBRCM_EVENT_H */
