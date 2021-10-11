/*
 *	replica.h
 *
 *	Copyright (c) 1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ifndef _REPLICA_H
#define	_REPLICA_H

#pragma ident	"@(#)replica.h	1.3	96/06/14 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Used to hold results of parsing replica lists for failover
 */
struct replica {
	char *host;
	char *path;
};

struct replica	*parse_replica(char *, int *);
void		free_replica(struct replica *, int);

#ifdef __cplusplus
}
#endif

#endif	/* _REPLICA_H */
