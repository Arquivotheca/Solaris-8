/*LINTLIBRARY*/

#pragma	ident	"@(#)uscsi_cmd.c 1.4     96/02/08 SMI"

/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

/*
 *  This module is part of the Command interface Library
 *  for the Pluto User Interface.
 *
 */

/*
 * Interface to the uscsi ioctl
 */
#include	<stdlib.h>
#include	<stdio.h>
#include	<assert.h>
#include	<sys/types.h>
#include	<memory.h>
#include	<unistd.h>
#include	<errno.h>
#include	<string.h>
#include	<libintl.h>	/* gettext */

/* SVR4 */
#include	<sys/dkio.h>
#include	<sys/dklabel.h>
#include	<sys/vtoc.h>
#include	 <sys/scsi/scsi.h>

#include	"common.h"
#include	"scsi.h"
#include	"error.h"

extern	char	*scsi_find_command_name(int cmd);
extern	char	*p_error_msg_ptr;
extern	char	p_error_msg[];
extern	void	scsi_printerr(struct uscsi_cmd *,
		struct scsi_extended_sense *, int, char *);
extern	void	p_dump(char *, u_char *, int, int);
extern	char	*p_decode_sense(u_char);

/*
 * Execute a command and determine the result.
 * Uses the "uscsi" ioctl interface
 */
int
uscsi_cmd(int file, struct uscsi_cmd *command, int flag)
{
int	status, i;

	/*
	 * Set function flags for driver.
	 *
	 * Set don't retry flags
	 * Set Automatic request sense enable
	 *
	 */
	command->uscsi_flags = USCSI_ISOLATE | USCSI_DIAGNOSE |
		USCSI_RQENABLE;
	command->uscsi_flags |= flag;

	/* print command for debug */
	if (getenv("SSA_S_DEBUG") != NULL) {
		(void) printf("Issuing the following SCSI command: %s\n",
			scsi_find_command_name(command->uscsi_cdb[0]));
		(void) printf("	fd=0x%x cdb=", file);
		for (i = 0; i < (int)command->uscsi_cdblen; i++) {
			(void) printf("%x ", *(command->uscsi_cdb + i));
		}
		(void) printf("\n\tlen=0x%x bufaddr=0x%x buflen=0x%x"
			" flags=0x%x\n",
		command->uscsi_cdblen,
		(int)command->uscsi_bufaddr,
		command->uscsi_buflen, command->uscsi_flags);

		if ((command->uscsi_buflen < 0x40) &&
			(command->uscsi_buflen > 0) &&
			((flag & USCSI_READ) == 0)) {
			(void) p_dump("Buffer data: ",
			(u_char *)command->uscsi_bufaddr,
			command->uscsi_buflen, HEX_ONLY);
		}
	}


	/*
	 * Default command timeout in case command left it 0
	 */
	if (command->uscsi_timeout == 0) {
		command->uscsi_timeout = 60;
	}
	/*	Issue command - finally */
	status = ioctl(file, USCSICMD, command);
	if (status == 0 && command->uscsi_status == 0) {
		if (getenv("SSA_S_DEBUG") != NULL) {
			if ((command->uscsi_buflen < 0x40) &&
				(command->uscsi_buflen > 0) &&
				((flag & USCSI_READ))) {
				(void) p_dump("Data read: ",
				(u_char *)command->uscsi_bufaddr,
				command->uscsi_buflen, HEX_ONLY);
			}
		}
		return (status);
	}
	if ((status != 0) && (command->uscsi_status == 0)) {
		if (getenv("SSA_S_DEBUG") != NULL) {
			(void) printf("Unexpected USCSICMD ioctl error: %s\n",
				strerror(errno));
		}
		return (status);
	}

	/*
	 * Just a SCSI error, create error message
	 */
	if ((command->uscsi_rqbuf != NULL) &&
	    (((char)command->uscsi_rqlen - (char)command->uscsi_rqresid) > 0)) {
		scsi_printerr(command,
			(struct scsi_extended_sense *)command->uscsi_rqbuf,
			(int)(command->uscsi_rqlen - command->uscsi_rqresid),
			p_error_msg);
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
	} else {
		/* Print sense byte information */

		sprintf(p_error_msg, MSG("SCSI Error - Sense Byte: %s\n"),
			p_decode_sense((u_char)command->uscsi_status));
		p_error_msg_ptr = (char *)&p_error_msg; /* set error ptr */
	}
	return (P_SCSI_ERROR | command->uscsi_status);
}
