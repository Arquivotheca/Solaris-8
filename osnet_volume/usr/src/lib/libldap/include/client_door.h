/*
 *
 * Copyright 02/02/98 Sun Microsystems, Inc. 
 * All Rights Reserved
 *
 */

#pragma ident   "@(#)client_door.h 1.1     98/02/02 SMI"

extern int connect_dsserv(int pid, int cid, void ** scid,
													struct in_addr * addr);
extern int disconnect_dsserv(int pid, int cid, void * scid, int reas);
extern int operation_buf_dsserv(int pid, int cid, void * scid,
												 char * buf,int bufsize, char **rbuf, int * rsize);
extern int operation_fd_dsserv(int pid, int cid, void * scid, int fd);
