#ident	"@(#)fds.h	1.5	93/05/17 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#ifndef	_FDS_H
#define	_FDS_H

#include <poll.h>
#include <sys/types.h>

extern struct pollfd *fds;
extern unsigned long nfds;

void add_to_fds(int, short, void (*)());
void delete_from_fds(int);
void change_callback(int, void (*)());
void (*do_callback(int))(int);
boolean_t expected_event(struct pollfd fd);
void log_bad_event(struct pollfd fd);

#endif	_FDS_H
