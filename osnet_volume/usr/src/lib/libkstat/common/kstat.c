/*
 * Copyright (c) 1992, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kstat.c	1.5	97/12/08 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include "kstat.h"

/*LINTLIBRARY*/

static void
kstat_zalloc(void **ptr, size_t size, int free_first)
{
	if (free_first)
		free(*ptr);
	*ptr = calloc(size, 1);
}

static void
kstat_chain_free(kstat_ctl_t *kc)
{
	kstat_t *ksp, *nksp;

	ksp = kc->kc_chain;
	while (ksp) {
		nksp = ksp->ks_next;
		free(ksp->ks_data);
		free(ksp);
		ksp = nksp;
	}
	kc->kc_chain = NULL;
	kc->kc_chain_id = 0;
}

kstat_ctl_t *
kstat_open(void)
{
	kstat_ctl_t *kc;
	int kd;

	kd = open("/dev/kstat", O_RDONLY);
	if (kd == -1)
		return (NULL);
	kstat_zalloc((void **)&kc, sizeof (kstat_ctl_t), 0);
	if (kc == NULL)
		return (NULL);
	kc->kc_kd = kd;
	kc->kc_chain = NULL;
	kc->kc_chain_id = 0;
	if (kstat_chain_update(kc) == -1) {
		(void) kstat_close(kc);
		return (NULL);
	}
	return (kc);
}

int
kstat_close(kstat_ctl_t *kc)
{
	int rc;

	kstat_chain_free(kc);
	rc = close(kc->kc_kd);
	free(kc);
	return (rc);
}

kid_t
kstat_read(kstat_ctl_t *kc, kstat_t *ksp, void *data)
{
	kid_t kcid;

	if (ksp->ks_data == NULL && ksp->ks_data_size > 0) {
		kstat_zalloc(&ksp->ks_data, ksp->ks_data_size, 0);
		if (ksp->ks_data == NULL)
			return (-1);
	}
	while ((kcid = (kid_t)ioctl(kc->kc_kd, KSTAT_IOC_READ, ksp)) == -1) {
		if (errno == EAGAIN) {
			(void) poll(NULL, 0, 100);	/* back off a moment */
			continue;			/* and try again */
		}
		/*
		 * Mating dance for variable-size kstats.
		 * You start with a buffer of a certain size,
		 * which you hope will hold all the data.
		 * If your buffer is too small, the kstat driver
		 * returns ENOMEM and sets ksp->ks_data_size to
		 * the current size of the kstat's data.  You then
		 * resize your buffer and try again.  In practice,
		 * this almost always converges in two passes.
		 */
		if (errno == ENOMEM && (ksp->ks_flags & KSTAT_FLAG_VAR_SIZE)) {
			kstat_zalloc(&ksp->ks_data, ksp->ks_data_size, 1);
			if (ksp->ks_data != NULL)
				continue;
		}
		return (-1);
	}
	if (data != NULL)
		(void) memcpy(data, ksp->ks_data, ksp->ks_data_size);
	return (kcid);
}

kid_t
kstat_write(kstat_ctl_t *kc, kstat_t *ksp, void *data)
{
	kid_t kcid;

	if (ksp->ks_data == NULL && ksp->ks_data_size > 0) {
		kstat_zalloc(&ksp->ks_data, ksp->ks_data_size, 0);
		if (ksp->ks_data == NULL)
			return (-1);
	}
	if (data != NULL)
		(void) memcpy(ksp->ks_data, data, ksp->ks_data_size);
	while ((kcid = (kid_t)ioctl(kc->kc_kd, KSTAT_IOC_WRITE, ksp)) == -1) {
		if (errno == EAGAIN) {
			(void) poll(NULL, 0, 100);	/* back off a moment */
			continue;			/* and try again */
		}
		break;
	}
	return (kcid);
}

/*
 * If the current KCID is the same as kc->kc_chain_id, return 0;
 * if different, update the chain and return the new KCID.
 * This operation is non-destructive for unchanged kstats.
 */
kid_t
kstat_chain_update(kstat_ctl_t *kc)
{
	kstat_t k0, *headers, *oksp, *nksp, **okspp, *next;
	int i;
	kid_t kcid;

	kcid = (kid_t)ioctl(kc->kc_kd, KSTAT_IOC_CHAIN_ID, NULL);
	if (kcid == -1)
		return (-1);
	if (kcid == kc->kc_chain_id)
		return (0);

	/*
	 * kstat 0's data is the kstat chain, so we can get the chain
	 * by doing a kstat_read() of this kstat.  The only fields the
	 * kstat driver needs are ks_kid (this identifies the kstat),
	 * ks_data (the pointer to our buffer), and ks_data_size (the
	 * size of our buffer).  We set ks_data = NULL and ks_data_size = 0,
	 * so that kstat_read() will automatically determine the size
	 * and allocate space for us.  We also fill in the name, so that
	 * truss can print something meaningful.
	 */
	k0.ks_kid = 0;
	k0.ks_data = NULL;
	k0.ks_data_size = 0;
	(void) strcpy(k0.ks_name, "kstat_headers");

	kcid = kstat_read(kc, &k0, NULL);
	if (kcid == -1) {
		free(k0.ks_data);
		return (-1);
	}
	headers = k0.ks_data;

	/*
	 * Chain the new headers together
	 */
	for (i = 1; i < k0.ks_ndata; i++)
		headers[i - 1].ks_next = &headers[i];

	headers[k0.ks_ndata - 1].ks_next = NULL;

	/*
	 * Remove all deleted kstats from the chain.
	 */
	nksp = headers;
	okspp = &kc->kc_chain;
	oksp = kc->kc_chain;
	while (oksp != NULL) {
		next = oksp->ks_next;
		if (nksp != NULL && oksp->ks_kid == nksp->ks_kid) {
			okspp = &oksp->ks_next;
			nksp = nksp->ks_next;
		} else {
			*okspp = oksp->ks_next;
			free(oksp->ks_data);
			free(oksp);
		}
		oksp = next;
	}

	/*
	 * Add all new kstats to the chain.
	 */
	while (nksp != NULL) {
		kstat_zalloc((void **)okspp, sizeof (kstat_t), 0);
		if ((oksp = *okspp) == NULL) {
			free(headers);
			return (-1);
		}
		*oksp = *nksp;
		okspp = &oksp->ks_next;
		oksp->ks_next = NULL;
		oksp->ks_data = NULL;
		nksp = nksp->ks_next;
	}

	free(headers);
	kc->kc_chain_id = kcid;
	return (kcid);
}

kstat_t *
kstat_lookup(kstat_ctl_t *kc, char *ks_module, int ks_instance, char *ks_name)
{
	kstat_t *ksp;

	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		if ((ks_module == NULL ||
		    strcmp(ksp->ks_module, ks_module) == 0) &&
		    (ks_instance == -1 || ksp->ks_instance == ks_instance) &&
		    (ks_name == NULL || strcmp(ksp->ks_name, ks_name) == 0))
			return (ksp);
	}
	return (NULL);
}

void *
kstat_data_lookup(kstat_t *ksp, char *name)
{
	int i, size;
	char *namep, *datap;

	switch (ksp->ks_type) {

	case KSTAT_TYPE_NAMED:
		size = sizeof (kstat_named_t);
		namep = KSTAT_NAMED_PTR(ksp)->name;
		break;

	case KSTAT_TYPE_TIMER:
		size = sizeof (kstat_timer_t);
		namep = KSTAT_TIMER_PTR(ksp)->name;
		break;

	default:
		return (NULL);
	}

	datap = ksp->ks_data;
	for (i = 0; i < ksp->ks_ndata; i++) {
		if (strcmp(name, namep) == 0)
			return (datap);
		namep += size;
		datap += size;
	}
	return (NULL);
}
