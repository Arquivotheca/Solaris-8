/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ifndef	NETWORK_MGMT_H
#define	NETWORK_MGMT_H
/*==================================================================*/
/*
*/
#ident	"@(#)networkMgmt.h	1.5	93/11/18 SMI"	/* SVr4.0 1.4	*/

#include	"_networkMgmt.h"
#include	<sys/stat.h>
#include	"xdrMsgs.h"
#include	"lists.h"
#include	"boolean.h"

typedef	struct
{
	int	size;
	void	*data_p;

}  dataPacket;

/*------------------------------------------------------------------*/
/*
**	Interface definition.
*/

int		SendJob(connectionInfo *, list *, list *, list *, uid_t, gid_t);
int		NegotiateJobClearance (connectionInfo *);
int		ReceiveJob (connectionInfo *, list **, list **);
int		rstat(char *, char *, struct stat *, uid_t, gid_t);
int		LengthOfList(list *lp);
char		*ReceiveFile (connectionInfo *, fileFragmentMsg *);
void		SetJobPriority (int);
void		FreeNetworkMsg(void **);
void		FreeDataPacket (dataPacket **);
boolean		JobPending (connectionInfo *);
boolean		SendData (connectionInfo *, boolean, void *, int);
boolean		EncodeNetworkMsgTag (connectionInfo *, networkMsgType);
boolean		SendJobControlMsg (connectionInfo *, jobControlCode);
boolean		SendSystemIdMsg (connectionInfo *, void *, int);
boolean		SendFileFragmentMsg (connectionInfo *, boolean,
			fileFragmentMsg *);
dataPacket	*NewDataPacket (int);
networkMsgTag	*ReceiveNetworkMsg (connectionInfo *, void **);
networkMsgTag	*DecodeNetworkMsg (connectionInfo *, void **);

/*==================================================================*/
#endif
