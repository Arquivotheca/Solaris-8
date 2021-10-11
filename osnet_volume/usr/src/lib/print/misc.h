/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MISC_H
#define	_MISC_H

#pragma ident	"@(#)misc.h	1.8	99/01/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/* Protocol Defined Requests */
#define PRINT_REQUEST	    1	/* \1printer\n */
#define XFER_REQUEST	    2	/* \2printer\n */
#define     XFER_CLEANUP	1 	/* \1 */
#define     XFER_CONTROL	2	/* \2size name\n */
#define     XFER_DATA		3	/* \3size name\n */

#define SHOW_QUEUE_SHORT_REQUEST  3	/* \3printer [users|jobs ...]\n */
#define SHOW_QUEUE_LONG_REQUEST  4	/* \4printer [users|jobs ...]\n */
#define REMOVE_REQUEST	    5	/* \5printer person [users|jobs ...]\n */

#define ACK_BYTE	0
#define NACK_BYTE	1

#define MASTER_NAME	"printd"
#define MASTER_LOCK	"/var/spool/print/.printd.lock"
#define SPOOL_DIR	"/var/spool/print"
#define TBL_NAME	"printers.conf"


extern char *long_date();
extern char *short_date();
extern int check_client_spool(char *printer);
extern int get_lock(char *name, int write_pid);
extern uid_t get_user_id();
extern char *get_user_name();
extern char *strcdup(char *, char);
extern char *strndup(char *, int);
extern char **strsplit(char *, char *);
extern int  file_size(char *);
extern int  copy_file(char *src, char *dst);
extern int  map_in_file(const char *name, char **buf);
extern int  write_buffer(char *name, char *buf, int len);
extern void start_daemon(int do_fork);
extern int  kill_process(char *file);

#ifdef __cplusplus
}
#endif

#endif /* _MISC_H */  
