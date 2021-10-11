#ifndef lint
static char sccsid[] = "@(#)au_open.c 1.11 97/10/29 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <bsm/audit.h>
#include <bsm/libbsm.h>
#include <bsm/audit_record.h>
#include <synch.h>


/*
 * Open an audit record.
 * Find a free descriptor and pass it back
 * For the time being, panic if the descriptor count exceeds a hard
 * maximum, we can change if this becomes a problem.
 */
#define	MAX_AU_DESCRIPTORS	16

extern int _mutex_lock(mutex_t *);
extern int _mutex_unlock(mutex_t *);


static token_t	**au_d;
static mutex_t  mutex_au_d = DEFAULTMUTEX;

int
#ifdef __STDC__
au_open(void)
#else
au_open()
#endif
{
	int d;	/* descriptor */

	_mutex_lock(&mutex_au_d);
	if (au_d == (token_t **)0) {
		au_d = (token_t **)calloc(MAX_AU_DESCRIPTORS,
			sizeof (token_t *));
	}
	for (d = 0; d < MAX_AU_DESCRIPTORS; d++) {
		if (au_d[d] == (token_t *)0) {
			au_d[d] = (token_t *)&au_d;
			_mutex_unlock(&mutex_au_d);
			return (d);
		}
	}
	_mutex_unlock(&mutex_au_d);
	return (-1);
}

/*
 * Write to an audit descriptor.
 * Add the mbuf to the descriptor chain and free the chain passed in.
 */

int
#ifdef __STDC__
au_write(int d, token_t *m)
#else
au_write(d, m)
	int d;
	token_t *m;
#endif
{
	token_t *mp;

	if (d < 0)
		return (-1);
	if (d >= MAX_AU_DESCRIPTORS)
		return (-1);
	if (m == (token_t *)0)
		return (-1);
	_mutex_lock(&mutex_au_d);
	if (au_d[d] == (token_t *)0) {
		return (-1);
	} else if (au_d[d] == (token_t *)&au_d) {
		au_d[d] = m;
		_mutex_unlock(&mutex_au_d);
		return (0);
	}
	for (mp = au_d[d]; mp->tt_next != (token_t *)0; mp = mp->tt_next)
		;
	mp->tt_next = m;
	_mutex_unlock(&mutex_au_d);
	return (0);
}

/*
 * Close an audit descriptor.
 * Use the second parameter to indicate if it should be written or not.
 */
int
#ifdef __STDC__
au_close(int d, int right, short e_type)
#else
au_close(d, right, e_type)
	int d;
	int right;
	short e_type;
#endif
{
	short e_mod;
	struct timeval now;	/* current time */
	adr_t adr;		/* adr header */
	token_t *dchain;	/* mbuf chain which is the tokens */
	token_t *record;	/* mbuf chain which is the record */
	char data_header;	/* token type */
	char version;		/* token version */
	char *buffer;		/* to build record into */
	int  byte_count;	/* bytes in the record */
	int   v;

	if (d < 0 || d >= MAX_AU_DESCRIPTORS)
		return (-1);
	_mutex_lock(&mutex_au_d);
	if ((dchain = au_d[d]) == (token_t *)0) {
		_mutex_unlock(&mutex_au_d);
		return (-1);
	}

	au_d[d] = (token_t *)0;

	if (dchain == (token_t *)&au_d) {
		_mutex_unlock(&mutex_au_d);
		return (0);
	}
	/*
	 * If not to be written toss the record
	 */
	if (!right) {
		while (dchain != (token_t *)0) {
			record = dchain;
			dchain = dchain->tt_next;
			free(record->tt_data);
			free(record);
		}
		_mutex_unlock(&mutex_au_d);
		return (0);
	}

	/*
	 * Count up the bytes used in the record.
	 */
	byte_count = sizeof (char) * 2 + sizeof (short) * 2 +
			sizeof (int32_t) + sizeof (struct timeval);

	for (record = dchain; record != (token_t *)0;
		record = record->tt_next) {
			byte_count += record->tt_size;
	}
	/*
	 * Build the header
	 */
	buffer = malloc((size_t)byte_count);
	gettimeofday(&now, NULL);
#ifdef _LP64
	data_header = AUT_HEADER64;
#else
	data_header = AUT_HEADER32;
#endif
	version = TOKEN_VERSION;
	e_mod = 0;
	adr_start(&adr, buffer);
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, (int32_t *)&byte_count, 1);
	adr_char(&adr, &version, 1);
	adr_short(&adr, &e_type, 1);
	adr_short(&adr, &e_mod, 1);
#ifdef _LP64
	adr_int64(&adr, (int64_t *)&now, 2);
#else
	adr_int32(&adr, (int32_t *)&now, 2);
#endif
	/*
	 * Tack on the data, and free the tokens.
	 * We're not supposed to know how adr works, but ...
	 */
	while (dchain != (token_t *)0) {
		(void) memcpy(adr.adr_now, dchain->tt_data, dchain->tt_size);
		adr.adr_now += dchain->tt_size;
		record = dchain;
		dchain = dchain->tt_next;
		free(record->tt_data);
		free(record);
	}
	/*
	 * Send it down to the system
	 */
	v = audit((caddr_t) buffer, byte_count);
	free(buffer);
	_mutex_unlock(&mutex_au_d);
	return (v);
}
