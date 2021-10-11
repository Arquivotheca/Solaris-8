#ident	"@(#)fifo.h	1.3	99/10/26 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#ifndef	_FIFO_H
#define	_FIFO_H

#define	FIFO_PATH "/tmp/.asppp.fifo"

enum fifo_msg_codes {
	FIFO_UNAME = 0x200,
	FIFO_RESTART,
	FIFO_DEBUG
};

typedef struct {
	ulong_t	msg;
	char	uname[16];
} uname_t;

typedef struct {
	ulong_t	msg;
} restart_t;

typedef struct {
	ulong_t	msg;
	int	debug_level;
} debug_t;

union fifo_msgs {
	ulong_t		msg;
	uname_t		uname;
	restart_t	restart;
	debug_t		debug;
};

void	create_fifo(void);

#endif	_FIFO_H
