#pragma ident "@(#)rplin.c 1.5	92/11/09 SMI"

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/stropts.h>
#include <poll.h>
#include <sys/stat.h>
#include "rpld.h"

extern	struct  strbuf	ctl, data;
extern	char	ctlbuf[];
extern	char	databuf[];
extern	struct	pollfd	llc_fd;
extern	int	debugLevel;
extern	long	delayGran;
extern  long	startDelay;
extern	char	debugmsg[];
extern	client_t *clientlookup();

int
incoming(fd)
int	fd;
{
int	flags;
long	in_seqnum;
client_t *cp;
bootfile_t *tmpfp, *tmpfp1;
struct stat statbuf;
char	b[20];

	ctl.maxlen = 100;
	ctl.len = 0;
	ctl.buf = ctlbuf;
	data.maxlen = 2000;
	data.len = 0;
	data.buf = databuf;
	flags = 0;
	if (getmsg(fd, &ctl, &data, &flags) < 0) {
		perror("getmsg in incoming\n");
		return(-1);
	}
	/*
	dumpctl(&ctl);
	dumpdata(&data);
	*/

	switch (databuf[3]) {
	case CMD_FIND:
		if (debugLevel >= MSG_INFO_1) {
			sprintf(debugmsg, "FIND frame received\n");
			senddebug(MSG_INFO_1);
		}
		if (clientvalidate() < 0) {
			if (debugLevel >= MSG_WARN_2) {
				sprintf(debugmsg, "Client failed validation\n");
				senddebug(MSG_WARN_2);
			}
			return;
		}
		break;
	case CMD_SEND_FILE:
		if (debugLevel >= MSG_INFO_1) {
			sprintf(debugmsg, "SEND FILE REQUEST received\n");
			senddebug(MSG_INFO_1);
		}
		if ((cp = clientlookup(&databuf[32])) == NULL) {
			if (debugLevel >= MSG_WARN_2) {
				sprintf(debugmsg, "Unknown client request to SEND FILE\n");
				senddebug(MSG_WARN_2);
			}
			return;
		}

		switch (cp->status) {
		case ST_FOUND_SENT:
			cp->status = ST_DATA_XFER;
			break;
		case ST_DATA_XFER:
		case ST_FINISH:
			/*
			 * Need to adjust the delay timing by looking at
			 * the sequence number requested.  If the sequence
			 * number is smaller than the current sequence
			 * number, it is a retransmit request.  The delay
			 * would be adjusted upwards.  However, if the
			 * sequence number is the same or 1 larger than
			 * the current sequence number, the delay is too
			 * long and we adjust it downwards.
			 */
			if (cp->status == ST_FINISH) {
				cp->status = ST_DATA_XFER;
				cp->timeo = 10*cp->maxdelay;
			}

			in_seqnum = (((long)databuf[8] & 0xff) << 24) |
				    (((long)databuf[9] & 0xff) << 16) |
				    (((long)databuf[10] & 0xff) << 8) |
				    (((long)databuf[11] & 0xff));
			if (in_seqnum < cp->seqnum) {
				if (debugLevel >= MSG_WARN_2) {
					sprintf(debugmsg, "retransmit %ld requested, current is %ld\n", in_seqnum, cp->seqnum);
					senddebug(MSG_WARN_2);
				}

				/*
				 * Adjust the currfp and seekfp to point
				 * to this requested sequence number's
				 * frame so that when we next send a frame
				 * out, it would pick up from there.
				 */
				tmpfp = cp->bootfp;
				tmpfp1 = tmpfp->next;
				while (tmpfp) {
					if ((tmpfp1 == NULL) ||
					(tmpfp1->seqnum == -1) ||
					((in_seqnum >= tmpfp->seqnum) &&
					 (in_seqnum < tmpfp1->seqnum)))
						break;
					else {
						tmpfp = tmpfp1;
						tmpfp1 = tmpfp->next;
					}
				}
				if (tmpfp) {
					if (tmpfp != cp->currfp) {
						fclose(cp->fstr);
						if ((cp->fstr = fopen(tmpfp->filename, "r")) == NULL) {
							if (debugLevel >= MSG_ERROR_1) {
								sprintf(debugmsg, "Cannot open %s in retransmit processing\n", tmpfp->filename);
								senddebug(MSG_ERROR_1);
							}
							return;
						}
						cp->currfp = tmpfp;
						stat(tmpfp->filename, &statbuf);
						tmpfp->size = (long)statbuf.st_size;
					}
					/*
					 * If it was the last file in the list
					 * of boot files, when we have sent out
					 * the last frame and waiting for time
					 * out, the file has been closed, and
					 * cp->fstr will be reset to NULL.
					 * Here we must check for this and
					 * reopen this file.
					 */
					if (cp->fstr == NULL) {
						if ((cp->fstr = fopen(tmpfp->filename, "r")) == NULL) {
							if (debugLevel >= MSG_ERROR_1) {
								sprintf(debugmsg, "Cannot open %s in retransmit, last file closed.\n");
								senddebug(MSG_ERROR_1);
							}
							return;
						}
						cp->currfp = tmpfp;
						stat(tmpfp->filename, &statbuf);
						tmpfp->size = (long)statbuf.st_size;
					}
					cp->seekp = (in_seqnum - tmpfp->seqnum)*
						(cp->framesz - 29);
					cp->seqnum = in_seqnum;
				}

				/* Adjust delay factors */
				cp->delay += delayGran;
				cp->resetdflt += delayGran;
				if (cp->delay > cp->maxdelay)
					cp->maxdelay = cp->delay;

				if (debugLevel >= MSG_INFO_1) {
					sprintf(debugmsg, "New delay value = %ld\n",
						cp->resetdflt);
					senddebug(MSG_INFO_1);
				}

			} else if (in_seqnum == (cp->seqnum + 1)) {

				if ((cp->delay - delayGran) > 0)
					cp->delay -= delayGran;
				else
					cp->delay = (long)1;

				if ((cp->resetdflt - delayGran) > 0)
					cp->resetdflt -= delayGran;
				else
					cp->resetdflt = (long)1;

				if (debugLevel >= MSG_INFO_1) {
					sprintf(debugmsg, "New delay value = %ld\n",
						cp->resetdflt);
					senddebug(MSG_INFO_1);
				}
			}
			break;
		default:
			if (debugLevel >= MSG_WARN_2) {
				sprintf(debugmsg, "Invalid status %d when SEND FILE comes in\n", cp->status);
				senddebug(MSG_WARN_2);
			}
		}
		break;
	case CMD_PROGRAM_ALERT:
		if (debugLevel >= MSG_WARN_2) {
			sprintf(debugmsg, "PROGRAM ALERT received\n");
			senddebug(MSG_WARN_2);
		}
		break;
	}
	return(0);
}

