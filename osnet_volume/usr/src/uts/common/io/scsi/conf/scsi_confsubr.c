/*
 * Copyright (c) 1988-1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)scsi_confsubr.c	1.54	99/05/04 SMI"

/*
 * Utility SCSI configuration routines
 */
/*
 * Many routines in this file have built in parallel bus assumption
 * which might need to change as other interconnect evolve.
 */


#include <sys/scsi/scsi.h>
#include <sys/modctl.h>

/*
 * macro for filling in lun value for scsi-1 support
 */

#define	FILL_SCSI1_LUN(devp, pkt) \
	if ((devp->sd_address.a_lun > 0) && \
	    (devp->sd_inq->inq_ansi == 0x1)) { \
		((union scsi_cdb *)(pkt)->pkt_cdbp)->scc_lun = \
		    devp->sd_address.a_lun; \
	}

extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc = {
	&mod_miscops,	/* Type of module */
	"SCSI Bus Utility Routines"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};


_init()
{
	scsi_initialize_hba_interface();
	scsi_watch_init();
	return (mod_install(&modlinkage));
}

/*
 * there is no _fini() routine because this module is never unloaded
 */

_info(modinfop)
struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}


/*
 *
 * SCSI slave probe routine - provided as a service to target drivers
 *
 * Mostly attempts to allocate and fill devp inquiry data..
 */

#define	ROUTE	(&devp->sd_address)

int
scsi_slave(register struct scsi_device *devp, int (*callback)())
{
	register struct scsi_pkt *pkt;
	register struct scsi_pkt *rq_pkt = NULL;
	register struct buf *rq_bp = NULL;
	register int rval = SCSIPROBE_EXISTS;

	/*
	 * the first test unit ready will tell us whether a target
	 * responded and if there was one, it will clear the unit attention
	 * condition
	 */
	pkt = scsi_init_pkt(ROUTE, (struct scsi_pkt *)NULL, NULL,
	    CDB_GROUP0, sizeof (struct scsi_arq_status), 0, 0, callback, NULL);

	if (pkt == NULL) {
		rval = SCSIPROBE_NOMEM_CB;
		goto out;
	}

	(void) scsi_setup_cdb((union scsi_cdb *)pkt->pkt_cdbp,
	    SCMD_TEST_UNIT_READY, 0, 0, 0);
	FILL_SCSI1_LUN(devp, pkt);
	pkt->pkt_flags = FLAG_NOINTR|FLAG_NOPARITY;

	if (scsi_poll(pkt) < 0) {
		if (pkt->pkt_reason == CMD_INCOMPLETE)
			rval = SCSIPROBE_NORESP;
		else
			rval = SCSIPROBE_FAILURE;
		goto out;
	}

	/*
	 * the second test unit ready, allows the host adapter to negotiate
	 * synchronous transfer period and offset
	 */
	if (scsi_poll(pkt) < 0) {
		if (pkt->pkt_reason == CMD_INCOMPLETE)
			rval = SCSIPROBE_NORESP;
		else
			rval = SCSIPROBE_FAILURE;
	}

	/*
	 * do a rqsense if there was a check condition and ARQ was not done
	 */
	if ((pkt->pkt_state & STATE_ARQ_DONE) == 0) {
		if (((struct scsi_status *)pkt->pkt_scbp)->sts_chk) {
			/*
			 * prepare rqsense packet
			 */
			rq_bp = scsi_alloc_consistent_buf(ROUTE,
			    (struct buf *)NULL,
			    (uint_t)SENSE_LENGTH, B_READ, callback, NULL);
			if (rq_bp == NULL) {
				rval = SCSIPROBE_NOMEM;
				goto out;
			}

			rq_pkt = scsi_init_pkt(ROUTE, (struct scsi_pkt *)NULL,
			    rq_bp, CDB_GROUP0, 1, 0, PKT_CONSISTENT,
			    callback, NULL);

			if (rq_pkt == NULL) {
				if (rq_bp->b_error == 0)
					rval = SCSIPROBE_NOMEM_CB;
				else
					rval = SCSIPROBE_NOMEM;
				goto out;
			}
			ASSERT(rq_bp->b_error == 0);

			(void) scsi_setup_cdb((union scsi_cdb *)rq_pkt->
			    pkt_cdbp, SCMD_REQUEST_SENSE, 0, 0, 0);
			FILL_SCSI1_LUN(devp, rq_pkt);
			rq_pkt->pkt_flags = FLAG_NOINTR|FLAG_NOPARITY;

			/*
			 * The controller type is as yet unknown, so we
			 * have to do a throwaway non-extended request sense,
			 * and hope that that clears the check condition
			 * for that unit until we can find out what kind
			 * of drive it is. A non-extended request sense
			 * is specified by stating that the sense block
			 * has 0 length, which is taken to mean that it
			 * is four bytes in length.
			 */
			if (scsi_poll(rq_pkt) < 0) {
				rval = SCSIPROBE_FAILURE;
				goto out;
			}
		}
	}

	/*
	 * call scsi_probe to do the inquiry
	 * XXX there is minor difference with the old scsi_slave implementation:
	 * busy conditions are not handled in scsi_probe.
	 */
out:
	if (pkt) {
		scsi_destroy_pkt(pkt);
	}
	if (rq_pkt) {
		scsi_destroy_pkt(rq_pkt);
	}
	if (rq_bp) {
		scsi_free_consistent_buf(rq_bp);
	}
	if (rval == SCSIPROBE_EXISTS) {
		return (scsi_probe(devp, callback));
	} else {
		return (rval);
	}
}


/*
 * Undo scsi_slave - older interface, but still supported
 */
void
scsi_unslave(register struct scsi_device *devp)
{
	if (devp->sd_inq) {
		kmem_free((caddr_t)devp->sd_inq, SUN_INQSIZE);
		devp->sd_inq = (struct scsi_inquiry *)NULL;
	}
}


/*
 * Undo scsi_probe
 */
void
scsi_unprobe(register struct scsi_device *devp)
{
	if (devp->sd_inq) {
		kmem_free((caddr_t)devp->sd_inq, SUN_INQSIZE);
		devp->sd_inq = (struct scsi_inquiry *)NULL;
	}
}


static int
scsi_test(struct scsi_pkt *pkt)
{
	register int rval = -1;

	pkt->pkt_flags |= FLAG_NOINTR;
	pkt->pkt_time = SCSI_POLL_TIMEOUT;

	if (scsi_ifgetcap(&pkt->pkt_address, "tagged-qing", 1) == 1) {
		pkt->pkt_flags |= FLAG_STAG;
	}

	if (scsi_transport(pkt) != TRAN_ACCEPT) {
		goto exit;

	} else if (pkt->pkt_reason == CMD_INCOMPLETE && pkt->pkt_state == 0) {
		goto exit;

	} else if (pkt->pkt_reason != CMD_CMPLT) {
		goto exit;

	} else if (((*pkt->pkt_scbp) & STATUS_MASK) == STATUS_BUSY) {
		rval = 0;

	} else {
		rval = 0;
	}

exit:
	return (rval);
}


/*
 * The implementation of scsi_probe now allows a particular
 * HBA to intercept the call, for any post- or pre-processing
 * it may need.  The default, if the HBA does not override it,
 * is to call scsi_hba_probe(), which retains the old functionality
 * intact.
 */
int
scsi_probe(struct scsi_device *devp, int (*callback)())
{
	scsi_hba_tran_t		*hba_tran = devp->sd_address.a_hba_tran;

	if (hba_tran->tran_tgt_probe != NULL) {
		return ((*hba_tran->tran_tgt_probe)(devp, callback));
	} else {
		return (scsi_hba_probe(devp, callback));
	}
}


/*
 * scsi_hba_probe does not do any test unit ready's which access the medium
 * and could cause busy or not ready conditions.
 * scsi_hba_probe does 2 inquiries and a rqsense to clear unit attention
 * and to allow sync negotiation to take place
 * finally, scsi_hba_probe does one more inquiry which should
 * reliably tell us what kind of target we have.
 * A scsi-2 compliant target should be able to	return inquiry with 250ms
 * and we actually wait more than a second after reset.
 */
int
scsi_hba_probe(register struct scsi_device *devp, int (*callback)())
{
	register struct scsi_pkt *inq_pkt = NULL;
	register struct scsi_pkt *rq_pkt = NULL;
	register int rval = SCSIPROBE_NOMEM;
	struct buf *inq_bp = NULL;
	struct buf *rq_bp = NULL;
	int (*cb_flag)();

	if (devp->sd_inq == NULL) {
		devp->sd_inq = (struct scsi_inquiry *)
			kmem_alloc(SUN_INQSIZE, ((callback == SLEEP_FUNC) ?
				KM_SLEEP : KM_NOSLEEP));
		if (devp->sd_inq == NULL) {
			goto out;
		}
	}

	if (callback != SLEEP_FUNC && callback != NULL_FUNC) {
		cb_flag = NULL_FUNC;
	} else {
		cb_flag = callback;
	}
	inq_bp = scsi_alloc_consistent_buf(ROUTE,
	    (struct buf *)NULL, SUN_INQSIZE, B_READ, cb_flag, NULL);
	if (inq_bp == NULL) {
		goto out;
	}

	inq_pkt = scsi_init_pkt(ROUTE, (struct scsi_pkt *)NULL,
	    inq_bp, CDB_GROUP0, sizeof (struct scsi_arq_status),
	    0, PKT_CONSISTENT, callback, NULL);
	if (inq_pkt == NULL) {
		if (inq_bp->b_error == 0)
			rval = SCSIPROBE_NOMEM_CB;
		goto out;
	}
	ASSERT(inq_bp->b_error == 0);

	bzero((caddr_t)devp->sd_inq, SUN_INQSIZE);
	(void) scsi_setup_cdb((union scsi_cdb *)inq_pkt->pkt_cdbp,
	    SCMD_INQUIRY, 0, SUN_INQSIZE, 0);
	inq_pkt->pkt_flags = FLAG_NOINTR|FLAG_NOPARITY;

	/*
	 * if lun > 0, probe for lun=0 first and figure
	 * out if it is a scsi-1 device
	 */
	if (devp->sd_address.a_lun > 0) {
		struct scsi_inquiry *inq;
		if (scsi_test(inq_pkt) < 0) {
			if (inq_pkt->pkt_reason == CMD_INCOMPLETE) {
				rval = SCSIPROBE_NORESP;
				goto out;
			} else {
				/*
				 * retry one more time
				 */
				if (scsi_test(inq_pkt) < 0) {
					rval = SCSIPROBE_FAILURE;
					goto out;
				}
			}
		}
		if ((inq_pkt->pkt_reason == CMD_CMPLT) &&
		    (((*inq_pkt->pkt_scbp) & STATUS_MASK) == 0)) {
			inq = (struct scsi_inquiry *)inq_bp->b_un.b_addr;
			if (inq->inq_ansi == 0x1) {
				if (devp->sd_address.a_lun > 7) {
					/*
					 * invalid lun for scsi-1,
					 * return probe failure.
					 */
					rval = SCSIPROBE_FAILURE;
					goto out;
				}
				/*
				 *  scsi-1 device, stuff the lun in cdb.
				 */
				((union scsi_cdb *)(inq_pkt)->pkt_cdbp)->
				    scc_lun = devp->sd_address.a_lun;
			}
		} else {
			/*
			 * couldn't get the inquiry data for lun=0, so
			 * return probe failure
			 */
			rval = SCSIPROBE_FAILURE;
			goto out;
		}
	}
	/*
	 * the first inquiry will tell us whether a target
	 * responded
	 */
	if (scsi_test(inq_pkt) < 0) {
		if (inq_pkt->pkt_reason == CMD_INCOMPLETE) {
			rval = SCSIPROBE_NORESP;
			goto out;
		} else {
			/*
			 * retry one more time
			 */
			if (scsi_test(inq_pkt) < 0) {
				rval = SCSIPROBE_FAILURE;
				goto out;
			}
		}
	}

	/*
	 * if we are lucky, this inquiry succeeded
	 */
	if ((inq_pkt->pkt_reason == CMD_CMPLT) &&
	    (((*inq_pkt->pkt_scbp) & STATUS_MASK) == 0)) {
		goto done;
	}

	/*
	 * the second inquiry, allows the host adapter to negotiate
	 * synchronous transfer period and offset
	 */
	if (scsi_test(inq_pkt) < 0) {
		if (inq_pkt->pkt_reason == CMD_INCOMPLETE)
			rval = SCSIPROBE_NORESP;
		else
			rval = SCSIPROBE_FAILURE;
		goto out;
	}

	/*
	 * if target is still busy, give up now
	 */
	if (((struct scsi_status *)inq_pkt->pkt_scbp)->sts_busy) {
		rval = SCSIPROBE_BUSY;
		goto out;
	}

	/*
	 * do a rqsense if there was a check condition and ARQ was not done
	 */
	if ((inq_pkt->pkt_state & STATE_ARQ_DONE) == 0) {
		if (((struct scsi_status *)inq_pkt->pkt_scbp)->sts_chk) {

			/*
			 * prepare rqsense packet
			 * there is no real need for this because the
			 * check condition should have been cleared by now.
			 */
			rq_bp = scsi_alloc_consistent_buf(ROUTE,
			    (struct buf *)NULL,
			    (uint_t)SENSE_LENGTH, B_READ, cb_flag, NULL);
			if (rq_bp == NULL) {
				goto out;
			}

			rq_pkt = scsi_init_pkt(ROUTE, (struct scsi_pkt *)NULL,
			    rq_bp, CDB_GROUP0, 1, 0, PKT_CONSISTENT, callback,
			    NULL);

			if (rq_pkt == NULL) {
				if (rq_bp->b_error == 0)
					rval = SCSIPROBE_NOMEM_CB;
				goto out;
			}
			ASSERT(rq_bp->b_error == 0);

			(void) scsi_setup_cdb((union scsi_cdb *)rq_pkt->
			    pkt_cdbp, SCMD_REQUEST_SENSE, 0, SENSE_LENGTH, 0);
			FILL_SCSI1_LUN(devp, rq_pkt);
			rq_pkt->pkt_flags = FLAG_NOINTR|FLAG_NOPARITY;

			/*
			 * The controller type is as yet unknown, so we
			 * have to do a throwaway non-extended request sense,
			 * and hope that that clears the check condition for
			 * that unit until we can find out what kind of drive
			 * it is. A non-extended request sense is specified
			 * by stating that the sense block has 0 length,
			 * which is taken to mean that it is four bytes in
			 * length.
			 */
			if (scsi_test(rq_pkt) < 0) {
				rval = SCSIPROBE_FAILURE;
				goto out;
			}
		}
	}

	/*
	 * At this point, we are guaranteed that something responded
	 * to this scsi bus target id. We don't know yet what
	 * kind of device it is, or even whether there really is
	 * a logical unit attached (as some SCSI target controllers
	 * lie about a unit being ready, e.g., the Emulex MD21).
	 */

	if (scsi_test(inq_pkt) < 0) {
		rval = SCSIPROBE_FAILURE;
		goto out;
	}

	if (((struct scsi_status *)inq_pkt->pkt_scbp)->sts_busy) {
		rval = SCSIPROBE_BUSY;
		goto out;
	}

	/*
	 * Okay we sent the INQUIRY command.
	 *
	 * If enough data was transferred, we count that the
	 * Inquiry command succeeded, else we have to assume
	 * that this is a non-CCS scsi target (or a nonexistent
	 * target/lun).
	 */

	if (((struct scsi_status *)inq_pkt->pkt_scbp)->sts_chk) {
		/*
		 * try a request sense if we have a pkt, otherwise
		 * just retry the inquiry one more time
		 */
		if (rq_pkt) {
			(void) scsi_test(rq_pkt);
		}

		/*
		 * retry inquiry
		 */
		if (scsi_test(inq_pkt) < 0) {
			rval = SCSIPROBE_FAILURE;
			goto out;
		}
		if (((struct scsi_status *)inq_pkt->pkt_scbp)->sts_chk) {
			rval = SCSIPROBE_FAILURE;
			goto out;
		}
	}

done:
	/*
	 * If we got a parity error on receive of inquiry data,
	 * we're just plain out of luck because we told the host
	 * adapter to not watch for parity errors.
	 */
	if ((inq_pkt->pkt_state & STATE_XFERRED_DATA) == 0 ||
	    ((SUN_INQSIZE - inq_pkt->pkt_resid) < SUN_MIN_INQLEN)) {
		rval = SCSIPROBE_NONCCS;
	} else {
		bcopy((caddr_t)inq_bp->b_un.b_addr,
		    (caddr_t)devp->sd_inq, SUN_INQSIZE);
		rval = SCSIPROBE_EXISTS;
	}

out:
	if (rq_pkt) {
		scsi_destroy_pkt(rq_pkt);
	}
	if (inq_pkt) {
		scsi_destroy_pkt(inq_pkt);
	}
	if (rq_bp) {
		scsi_free_consistent_buf(rq_bp);
	}
	if (inq_bp) {
		scsi_free_consistent_buf(inq_bp);
	}
	return (rval);
}


#define	A_TO_TRAN(ap)	(ap->a_hba_tran)

/*
 * Function to get target and lun identifiers from HBA driver.
 */
int
scsi_get_bus_addr(struct scsi_device *devp, char *name, int len)
{
	struct scsi_address *ap = &devp->sd_address;

	if ((A_TO_TRAN(ap)->tran_get_bus_addr) == NULL) {
		(void) sprintf(name, "%x,%x", ap->a_target, ap->a_lun);
		return (1);
	}
	return (*A_TO_TRAN(ap)->tran_get_bus_addr)(devp, name, len);
}

/*
 * Function to get name from HBA driver.
 */
int
scsi_get_name(struct scsi_device *devp, char *name, int len)
{
	struct scsi_address *ap = &devp->sd_address;

	if ((A_TO_TRAN(ap)->tran_get_name) == NULL) {
		(void) sprintf(name, "%x,%x", ap->a_target, ap->a_lun);
		return (1);
	}
	return (*A_TO_TRAN(ap)->tran_get_name)(devp, name, len);
}
