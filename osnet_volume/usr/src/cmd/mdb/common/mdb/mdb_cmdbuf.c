/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_cmdbuf.c	1.1	99/08/11 SMI"

/*
 * The MDB command buffer is a simple structure that keeps track of the
 * command history list, and provides operations to manipulate the current
 * buffer according to the various emacs editing options.  The terminal
 * code uses this code to keep track of the actual contents of the command
 * line, and then uses this content to perform redraw operations.
 */

#include <strings.h>
#include <stdio.h>
#include <ctype.h>

#include <mdb/mdb_modapi.h>
#include <mdb/mdb_cmdbuf.h>
#include <mdb/mdb.h>

#define	CMDBUF_LINELEN	BUFSIZ		/* Length of each buffer line */
#define	CMDBUF_TABLEN	8		/* Length of a tab in spaces */

static void
cmdbuf_shiftr(mdb_cmdbuf_t *cmd, size_t nbytes)
{
	bcopy(&cmd->cmd_buf[cmd->cmd_bufidx],
	    &cmd->cmd_buf[cmd->cmd_bufidx + nbytes],
	    cmd->cmd_buflen - cmd->cmd_bufidx);
}

void
mdb_cmdbuf_create(mdb_cmdbuf_t *cmd)
{
	size_t i;

	/*
	 * This is pretty weak, but good enough for the moment: just allocate
	 * BUFSIZ-sized chunks in advance for every history element.  Later
	 * it would be nice to replace this with either code that allocates
	 * space for the history list on-the-fly so as not to waste so much
	 * memory, or that keeps a mapped history file like the shell.
	 */
	cmd->cmd_history = mdb_alloc(mdb.m_histlen * sizeof (char *), UM_SLEEP);
	cmd->cmd_linebuf = mdb_alloc(CMDBUF_LINELEN, UM_SLEEP);

	for (i = 0; i < mdb.m_histlen; i++)
		cmd->cmd_history[i] = mdb_alloc(CMDBUF_LINELEN, UM_SLEEP);

	cmd->cmd_buf = cmd->cmd_history[0];
	cmd->cmd_linelen = CMDBUF_LINELEN;
	cmd->cmd_histlen = mdb.m_histlen;
	cmd->cmd_buflen = 0;
	cmd->cmd_bufidx = 0;
	cmd->cmd_hold = 0;
	cmd->cmd_hnew = 0;
	cmd->cmd_hcur = 0;
	cmd->cmd_hlen = 0;
}

void
mdb_cmdbuf_destroy(mdb_cmdbuf_t *cmd)
{
	size_t i;

	for (i = 0; i < cmd->cmd_histlen; i++)
		mdb_free(cmd->cmd_history[i], CMDBUF_LINELEN);

	mdb_free(cmd->cmd_linebuf, CMDBUF_LINELEN);
	mdb_free(cmd->cmd_history, cmd->cmd_histlen * sizeof (char *));
}

int
mdb_cmdbuf_caninsert(mdb_cmdbuf_t *cmd, size_t nbytes)
{
	return (cmd->cmd_buflen + nbytes < cmd->cmd_linelen);
}

int
mdb_cmdbuf_atstart(mdb_cmdbuf_t *cmd)
{
	return (cmd->cmd_bufidx == 0);
}

int
mdb_cmdbuf_atend(mdb_cmdbuf_t *cmd)
{
	return (cmd->cmd_bufidx == cmd->cmd_buflen);
}

int
mdb_cmdbuf_insert(mdb_cmdbuf_t *cmd, int c)
{
	if (c == '\t') {
		if (cmd->cmd_buflen + CMDBUF_TABLEN < cmd->cmd_linelen) {
			int i;

			if (cmd->cmd_buflen != cmd->cmd_bufidx)
				cmdbuf_shiftr(cmd, CMDBUF_TABLEN);

			for (i = 0; i < CMDBUF_TABLEN; i++)
				cmd->cmd_buf[cmd->cmd_bufidx++] = ' ';

			cmd->cmd_buflen += CMDBUF_TABLEN;
			return (0);
		}

		return (-1);
	}

	if (c < ' ' || c > '~')
		return (-1);

	if (cmd->cmd_buflen < cmd->cmd_linelen) {
		if (cmd->cmd_buflen != cmd->cmd_bufidx)
			cmdbuf_shiftr(cmd, 1);

		cmd->cmd_buf[cmd->cmd_bufidx++] = c;
		cmd->cmd_buflen++;

		return (0);
	}

	return (-1);
}

const char *
mdb_cmdbuf_accept(mdb_cmdbuf_t *cmd)
{
	if (cmd->cmd_bufidx < cmd->cmd_linelen) {
		cmd->cmd_buf[cmd->cmd_buflen++] = '\0';
		(void) strcpy(cmd->cmd_linebuf, cmd->cmd_buf);

		/*
		 * Don't bother inserting empty buffers into the history ring.
		 */
		if (cmd->cmd_buflen > 1) {
			cmd->cmd_hnew = (cmd->cmd_hnew + 1) % cmd->cmd_histlen;
			cmd->cmd_buf = cmd->cmd_history[cmd->cmd_hnew];
			cmd->cmd_hcur = cmd->cmd_hnew;

			if (cmd->cmd_hlen + 1 == cmd->cmd_histlen)
				cmd->cmd_hold =
				    (cmd->cmd_hold + 1) % cmd->cmd_histlen;
			else
				cmd->cmd_hlen++;
		}

		cmd->cmd_bufidx = 0;
		cmd->cmd_buflen = 0;

		return ((const char *)cmd->cmd_linebuf);
	}

	return (NULL);
}

/*ARGSUSED*/
int
mdb_cmdbuf_backspace(mdb_cmdbuf_t *cmd, int c)
{
	if (cmd->cmd_bufidx > 0) {
		if (cmd->cmd_buflen != cmd->cmd_bufidx) {
			bcopy(&cmd->cmd_buf[cmd->cmd_bufidx],
			    &cmd->cmd_buf[cmd->cmd_bufidx - 1],
			    cmd->cmd_buflen - cmd->cmd_bufidx);
		}

		cmd->cmd_bufidx--;
		cmd->cmd_buflen--;

		return (0);
	}

	return (-1);
}

/*ARGSUSED*/
int
mdb_cmdbuf_delchar(mdb_cmdbuf_t *cmd, int c)
{
	if (cmd->cmd_bufidx < cmd->cmd_buflen) {
		if (cmd->cmd_bufidx < --cmd->cmd_buflen) {
			bcopy(&cmd->cmd_buf[cmd->cmd_bufidx + 1],
			    &cmd->cmd_buf[cmd->cmd_bufidx],
			    cmd->cmd_buflen - cmd->cmd_bufidx);
		}

		return (0);
	}

	return (-1);
}

/*ARGSUSED*/
int
mdb_cmdbuf_fwdchar(mdb_cmdbuf_t *cmd, int c)
{
	if (cmd->cmd_bufidx < cmd->cmd_buflen) {
		cmd->cmd_bufidx++;
		return (0);
	}

	return (-1);
}

/*ARGSUSED*/
int
mdb_cmdbuf_backchar(mdb_cmdbuf_t *cmd, int c)
{
	if (cmd->cmd_bufidx > 0) {
		cmd->cmd_bufidx--;
		return (0);
	}

	return (-1);
}

int
mdb_cmdbuf_transpose(mdb_cmdbuf_t *cmd, int c)
{
	if (cmd->cmd_bufidx > 0 && cmd->cmd_buflen > 1) {
		c = cmd->cmd_buf[cmd->cmd_bufidx - 1];

		if (cmd->cmd_bufidx == cmd->cmd_buflen) {
			cmd->cmd_buf[cmd->cmd_bufidx - 1] =
			    cmd->cmd_buf[cmd->cmd_bufidx - 2];
			cmd->cmd_buf[cmd->cmd_bufidx - 2] = c;
		} else {
			cmd->cmd_buf[cmd->cmd_bufidx - 1] =
			    cmd->cmd_buf[cmd->cmd_bufidx];
			cmd->cmd_buf[cmd->cmd_bufidx++] = c;
		}

		return (0);
	}

	return (-1);
}

/*ARGSUSED*/
int
mdb_cmdbuf_home(mdb_cmdbuf_t *cmd, int c)
{
	cmd->cmd_bufidx = 0;
	return (0);
}

/*ARGSUSED*/
int
mdb_cmdbuf_end(mdb_cmdbuf_t *cmd, int c)
{
	cmd->cmd_bufidx = cmd->cmd_buflen;
	return (0);
}

/*ARGSUSED*/
int
mdb_cmdbuf_fwdword(mdb_cmdbuf_t *cmd, int c)
{
	size_t i = cmd->cmd_bufidx + 1;

	if (cmd->cmd_bufidx == cmd->cmd_buflen)
		return (-1);

	while (i < cmd->cmd_buflen && isspace(cmd->cmd_buf[i]))
		i++;

	while (i < cmd->cmd_buflen &&
	    (isalnum(cmd->cmd_buf[i]) || cmd->cmd_buf[i] == '_'))
		i++;

	if (i > cmd->cmd_buflen)
		return (-1);

	cmd->cmd_bufidx = i;
	return (0);
}

/*ARGSUSED*/
int
mdb_cmdbuf_backword(mdb_cmdbuf_t *cmd, int c)
{
	size_t i = cmd->cmd_bufidx - 1;

	if (cmd->cmd_bufidx == 0)
		return (-1);

	while (i != 0 && isspace(cmd->cmd_buf[i]))
		i--;

	while (i != 0 &&
	    (isalnum(cmd->cmd_buf[i]) || cmd->cmd_buf[i] == '_'))
		i--;

	cmd->cmd_bufidx = i;
	return (0);
}

/*ARGSUSED*/
int
mdb_cmdbuf_kill(mdb_cmdbuf_t *cmd, int c)
{
	cmd->cmd_buflen = cmd->cmd_bufidx;
	return (0);
}

/*ARGSUSED*/
int
mdb_cmdbuf_reset(mdb_cmdbuf_t *cmd, int c)
{
	cmd->cmd_buflen = 0;
	cmd->cmd_bufidx = 0;
	return (0);
}

/*ARGSUSED*/
int
mdb_cmdbuf_prevhist(mdb_cmdbuf_t *cmd, int c)
{
	if (cmd->cmd_hcur != cmd->cmd_hold) {
		if (cmd->cmd_hcur-- == cmd->cmd_hnew) {
			cmd->cmd_buf[cmd->cmd_buflen] = 0;
			(void) strcpy(cmd->cmd_linebuf, cmd->cmd_buf);
		}

		if (cmd->cmd_hcur < 0)
			cmd->cmd_hcur = cmd->cmd_histlen - 1;

		(void) strcpy(cmd->cmd_buf, cmd->cmd_history[cmd->cmd_hcur]);
		cmd->cmd_bufidx = strlen(cmd->cmd_buf);
		cmd->cmd_buflen = cmd->cmd_bufidx;

		return (0);
	}

	return (-1);
}

/*ARGSUSED*/
int
mdb_cmdbuf_nexthist(mdb_cmdbuf_t *cmd, int c)
{
	if (cmd->cmd_hcur != cmd->cmd_hnew) {
		cmd->cmd_hcur = (cmd->cmd_hcur + 1) % cmd->cmd_histlen;

		if (cmd->cmd_hcur == cmd->cmd_hnew) {
			(void) strcpy(cmd->cmd_buf, cmd->cmd_linebuf);
		} else {
			(void) strcpy(cmd->cmd_buf,
			    cmd->cmd_history[cmd->cmd_hcur]);
		}

		cmd->cmd_bufidx = strlen(cmd->cmd_buf);
		cmd->cmd_buflen = cmd->cmd_bufidx;

		return (0);
	}

	return (-1);
}

/*ARGSUSED*/
int
mdb_cmdbuf_findhist(mdb_cmdbuf_t *cmd, int c)
{
	ssize_t i, n;

	if (cmd->cmd_buflen != 0) {
		cmd->cmd_hcur = cmd->cmd_hnew;
		cmd->cmd_buf[cmd->cmd_buflen] = 0;
		(void) strcpy(cmd->cmd_linebuf, cmd->cmd_buf);
	}

	for (i = cmd->cmd_hcur, n = 0; n < cmd->cmd_hlen; n++) {
		if (--i < 0)
			i = cmd->cmd_histlen - 1;

		if (strstr(cmd->cmd_history[i], cmd->cmd_linebuf) != NULL) {
			(void) strcpy(cmd->cmd_buf, cmd->cmd_history[i]);
			cmd->cmd_bufidx = strlen(cmd->cmd_buf);
			cmd->cmd_buflen = cmd->cmd_bufidx;
			cmd->cmd_hcur = i;

			return (0);
		}
	}

	cmd->cmd_hcur = cmd->cmd_hnew;

	cmd->cmd_bufidx = 0;
	cmd->cmd_buflen = 0;

	return (-1);
}
