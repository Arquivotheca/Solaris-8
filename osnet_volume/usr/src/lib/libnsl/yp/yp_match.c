/*
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All rights Reserved.
 */

#pragma	ident	"@(#)yp_match.c	1.28	99/09/20 SMI"

#define	NULL 0
#include "../rpc/rpc_mt.h"
#include <rpc/rpc.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include "yp_b.h"
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sys/time.h>

extern int __yp_dobind_cflookup(char *, struct dom_binding **, int);
extern int __yp_dobind_rsvdport_cflookup(char *, struct dom_binding **, int);

static int domatch(char *, char *, char *, int, struct dom_binding *,
    struct timeval *, char **, int *);
int yp_match_rsvdport();
int yp_match_rsvdport_cflookup();

struct cache {
	struct cache *next;
	unsigned int birth;
	char *domain;
	char *map;
	char *key;
	int  keylen;
	char *val;
	int  vallen;
};

static mutex_t		cache_lock = DEFAULTMUTEX;
static int		generation;	/* Incremented when we add to cache */
static struct cache	*head;

#define	CACHESZ 16
#define	CACHETO 600

static void
freenode(n)
	register struct cache *n;
{
	trace1(TR_freenode, 0);
	if (n->val != 0)
	    free(n->val);
	if (n->key != 0)
	    free(n->key);
	if (n->map != 0)
	    free(n->map);
	if (n->domain != 0)
	    free(n->domain);
	free((char *)n);
	trace1(TR_freenode, 1);
}

static struct cache *
makenode(domain, map, keylen, vallen)
	char *domain, *map;
	int keylen, vallen;
{
	register struct cache *n;

	trace3(TR_makenode, 0, keylen, vallen);
	if ((n = (struct cache *)calloc(1, sizeof (*n))) == 0) {
		trace1(TR_makenode, 1);
		return (0);
	}
	if (((n->domain = strdup(domain)) == 0) ||
	    ((n->map = strdup(map)) == 0) ||
	    ((n->key = malloc(keylen)) == 0) ||
	    ((n->val = malloc(vallen)) == 0)) {
		freenode(n);
		trace1(TR_makenode, 1);
		return (0);
	}
	trace1(TR_makenode, 1);
	return (n);
}

static int
in_cache(domain, map, key, keylen, val, vallen)
	char *domain;
	char *map;
	char *key;
	register int  keylen;
	char **val;		/* returns value array */
	int  *vallen;		/* returns bytes in val */
{
	struct cache *c, **pp;
	int cnt;
	struct timeval now;
	struct timezone tz;

	/*
	 * Assumes that caller (yp_match) has locked the cache
	 */
	for (pp = &head, cnt = 0;  (c = *pp) != 0;  pp = &c->next, cnt++) {
		if ((c->keylen == keylen) &&
		    (memcmp(key, c->key, (size_t)keylen) == 0) &&
		    (strcmp(map, c->map) == 0) &&
		    (strcmp(domain, c->domain) == 0)) {
			/* cache hit */
			(void) gettimeofday(&now, &tz);
			if ((now.tv_sec - c->birth) > CACHETO) {
				/* rats.  it is too old to use */
				*pp = c->next;
				freenode(c);
				break;
			} else {
				*val = c->val;
				*vallen = c->vallen;

				/* Ersatz LRU:  Move this entry to the front */
				*pp = c->next;
				c->next = head;
				head = c;
				return (1);
			}
		}
		if (cnt >= CACHESZ) {
			*pp = c->next;
			freenode(c);
			break;
		}
	}
	return (0);
}

/*
 * Requests the yp server associated with a given domain to attempt to match
 * the passed key datum in the named map, and to return the associated value
 * datum. This part does parameter checking, and implements the "infinite"
 * (until success) sleep loop if 'hardlookup' parameter is set.
 */
int
__yp_match_cflookup(domain, map, key, keylen, val, vallen, hardlookup)
	char *domain;
	char *map;
	char *key;
	register int  keylen;
	char **val;		/* returns value array */
	int  *vallen;		/* returns bytes in val */
	int  hardlookup;	/* retry lookup until we get an answer */
{
	size_t domlen;
	size_t maplen;
	int reason;
	struct dom_binding *pdomb;
	int savesize;
	struct timeval now;
	struct timezone tz;
	char *my_val;
	int  my_vallen;
	int  found_it;
	int  cachegen;

	trace2(TR_yp_match, 0, keylen);
	if ((map == NULL) || (domain == NULL)) {
		trace1(TR_yp_match, 1);
		return (YPERR_BADARGS);
	}

	domlen = strlen(domain);
	maplen = strlen(map);

	if ((domlen == 0) || (domlen > YPMAXDOMAIN) ||
	    (maplen == 0) || (maplen > YPMAXMAP) ||
	    (key == NULL) || (keylen == 0)) {
		trace1(TR_yp_match, 1);
		return (YPERR_BADARGS);
	}

	mutex_lock(&cache_lock);
	found_it = in_cache(domain, map, key, keylen, &my_val, &my_vallen);
	cachegen = generation;

	if (found_it) {
		/* NB: Copy two extra bytes; see below */
		savesize = my_vallen + 2;
		if ((*val = malloc((size_t)savesize)) == 0) {
			trace1(TR_yp_match, 1);
			mutex_unlock(&cache_lock);
			return (YPERR_RESRC);
		}
		(void) memcpy(*val, my_val, (size_t)savesize);
		*vallen = my_vallen;
		trace1(TR_yp_match, 1);
		mutex_unlock(&cache_lock);
		return (0);	/* Success */
	}
	mutex_unlock(&cache_lock);

	for (;;) {

		if (reason = __yp_dobind_cflookup(domain, &pdomb,
						    hardlookup)) {
			trace1(TR_yp_match, 1);
			return (reason);
		}

		if (pdomb->dom_binding->ypbind_hi_vers >= YPVERS) {

			reason = domatch(domain, map, key, keylen, pdomb,
			    &_ypserv_timeout, val, vallen);

			__yp_rel_binding(pdomb);
			if (reason == YPERR_RPC || reason == YPERR_YPSERV ||
			    reason == YPERR_BUSY /* as if */) {
				yp_unbind(domain);
				if (hardlookup)
					(void) _sleep(_ypsleeptime); /* retry */
				else {
					trace1(TR_yp_match, 1);
					return (reason);
				}
			} else
				break;
		} else {
			__yp_rel_binding(pdomb);
			trace1(TR_yp_match, 1);
			return (YPERR_VERS);
		}
	}

	/* add to our cache */
	if (reason == 0) {
		mutex_lock(&cache_lock);
		/*
		 * Check whether some other annoying thread did the same
		 * thing in parallel with us.  I hate it when that happens...
		 */
		if (generation != cachegen &&
		    in_cache(domain, map, key, keylen, &my_val, &my_vallen)) {
			/*
			 * Could get cute and update the birth time, but it's
			 *   not worth the bother.
			 * It looks strange that we return one val[] array
			 *   to the caller and have a different copy of the
			 *   val[] array in the cache (presumably with the
			 *   same contents), but it should work just fine.
			 * So, do absolutely nothing...
			 */
			/* EMPTY */
		} else {
			struct cache	*c;
			/*
			 * NB: allocate and copy extract two bytes of the
			 * value;  these are mandatory CR and NULL bytes.
			 */
			savesize = *vallen + 2;
			c = makenode(domain, map, keylen, savesize);
			if (c != 0) {
				(void) gettimeofday(&now, &tz);
				c->birth = now.tv_sec;
				c->keylen = keylen;
				c->vallen = *vallen;
				(void) memcpy(c->key, key, (size_t)keylen);
				(void) memcpy(c->val, *val, (size_t)savesize);

				c->next = head;
				head = c;
				++generation;
			}
		}
		mutex_unlock(&cache_lock);
	} else if (reason == YPERR_MAP && geteuid() == 0) {
		/*
		 * Lookup could be for a secure map; fail over to retry
		 * from a reserved port. Only useful to try this if we're
		 * the super user.
		 */
		int rsvdreason;
		rsvdreason = yp_match_rsvdport(domain, map, key, keylen, val,
						vallen);
		if (rsvdreason == 0)
			reason = rsvdreason;
	}
	trace1(TR_yp_match, 1);
	return (reason);
}

int
yp_match(
	char *domain,
	char *map,
	char *key,
	int  keylen,
	char **val,		/* returns value array */
	int  *vallen)		/* returns bytes in val */

{
	/* the traditional yp_match loops forever thus hardlookup is set */
	return (__yp_match_cflookup(domain, map, key, keylen, val, vallen, 1));
}

extern void
__empty_yp_cache()
{
	struct cache *p, *n;

	/* Copy the cache pointer and make it ZERO */
	mutex_lock(&cache_lock);
	p = head;
	head = 0;
	mutex_unlock(&cache_lock);

	if (p == 0)
		return;

	/* Empty the cache */
	n = p->next;
	while (p) {
		freenode(p);
		p = n;
		if (p)
			n = p->next;
	}
}

/*
 * Requests the yp server associated with a given domain to attempt to match
 * the passed key datum in the named map, and to return the associated value
 * datum. This part does parameter checking, and implements the "infinite"
 * (until success) sleep loop.
 *
 * XXX special version for handling C2 (passwd.adjunct) lookups when we need
 * a reserved port.
 * Only difference against yp_match is that this function uses
 * __yp_dobind_rsvdport().
 *
 * Only called from NIS switch backend.
 */
int
__yp_match_rsvdport_cflookup(
	char *domain,
	char *map,
	char *key,
	int  keylen,
	char **val,		/* returns value array */
	int  *vallen,		/* returns bytes in val */
	int  hardlookup)	/* retry until we can an answer */
{
	size_t domlen;
	size_t maplen;
	int reason;
	struct dom_binding *pdomb;
	int savesize;
	struct timeval now;
	struct timezone tz;
	char *my_val;
	int  my_vallen;
	int  found_it;
	int  cachegen;

	trace2(TR_yp_match_rsvdport, 0, keylen);
	if ((map == NULL) || (domain == NULL)) {
		trace1(TR_yp_match_rsvdport, 1);
		return (YPERR_BADARGS);
	}

	domlen = strlen(domain);
	maplen = strlen(map);

	if ((domlen == 0) || (domlen > YPMAXDOMAIN) ||
	    (maplen == 0) || (maplen > YPMAXMAP) ||
	    (key == NULL) || (keylen == 0)) {
		trace1(TR_yp_match_rsvdport, 1);
		return (YPERR_BADARGS);
	}

	mutex_lock(&cache_lock);
	found_it = in_cache(domain, map, key, keylen, &my_val, &my_vallen);
	cachegen = generation;
	if (found_it) {
		/* NB: Copy two extra bytes; see below */
		savesize = my_vallen + 2;
		if ((*val = malloc((size_t)savesize)) == 0) {
			trace1(TR_yp_match_rsvdport, 1);
			mutex_unlock(&cache_lock);
			return (YPERR_RESRC);
		}
		(void) memcpy(*val, my_val, (size_t)savesize);
		*vallen = my_vallen;
		trace1(TR_yp_match_rsvdport, 1);
		mutex_unlock(&cache_lock);
		return (0);	/* Success */
	}
	mutex_unlock(&cache_lock);

	for (;;) {

		if (reason = __yp_dobind_rsvdport_cflookup(domain, &pdomb,
							hardlookup)) {
			trace1(TR_yp_match_rsvdport, 1);
			return (reason);
		}

		if (pdomb->dom_binding->ypbind_hi_vers >= YPVERS) {

			reason = domatch(domain, map, key, keylen,
				pdomb, &_ypserv_timeout, val, vallen);

			/*
			 * Have to free the binding since the reserved
			 * port bindings are not cached.
			 */
			__yp_rel_binding(pdomb);
			free_dom_binding(pdomb);

			if (reason == YPERR_RPC || reason == YPERR_YPSERV ||
			    reason == YPERR_BUSY /* as if */) {
				yp_unbind(domain);
				if (hardlookup)
					(void) _sleep(_ypsleeptime); /* retry */
				else {
					trace1(TR_yp_match, 1);
					return (reason);
				}
			} else
				break;
		} else {
			/*
			 * Have to free the binding since the reserved
			 * port bindings are not cached.
			 */
			__yp_rel_binding(pdomb);
			free_dom_binding(pdomb);
			trace1(TR_yp_match_rsvdport, 1);
			return (YPERR_VERS);
		}
	}

	/* add to our cache */
	if (reason == 0) {
		mutex_lock(&cache_lock);
		/*
		 * Check whether some other annoying thread did the same
		 * thing in parallel with us.  I hate it when that happens...
		 */
		if (generation != cachegen &&
		    in_cache(domain, map, key, keylen, &my_val, &my_vallen)) {
			/*
			 * Could get cute and update the birth time, but it's
			 *   not worth the bother.
			 * It looks strange that we return one val[] array
			 *   to the caller and have a different copy of the
			 *   val[] array in the cache (presumably with the
			 *   same contents), but it should work just fine.
			 * So, do absolutely nothing...
			 */
			/* EMPTY */
		} else {
			struct cache	*c;
			/*
			 * NB: allocate and copy extract two bytes of the
			 * value;  these are mandatory CR and NULL bytes.
			 */
			savesize = *vallen + 2;
			c = makenode(domain, map, keylen, savesize);
			if (c != 0) {
				(void) gettimeofday(&now, &tz);
				c->birth = now.tv_sec;
				c->keylen = keylen;
				c->vallen = *vallen;
				(void) memcpy(c->key, key, (size_t)keylen);
				(void) memcpy(c->val, *val, (size_t)savesize);

				c->next = head;
				head = c;
				++generation;
			}
		}
		mutex_unlock(&cache_lock);
	}
	trace1(TR_yp_match_rsvdport, 1);
	return (reason);
}


int
yp_match_rsvdport(
	char *domain,
	char *map,
	char *key,
	int  keylen,
	char **val,		/* returns value array */
	int  *vallen)		/* returns bytes in val */
{
	/* traditional yp_match retries forever so set hardlookup */
	return (__yp_match_rsvdport_cflookup(domain, map, key, keylen, val,
					vallen, 1));
}


/*
 * This talks v3 protocol to ypserv
 */
static int
domatch(char *domain, char *map, char *key, int  keylen,
    struct dom_binding *pdomb, struct timeval *timeoutp, char **val,
    int  *vallen)
{
	struct ypreq_key req;
	struct ypresp_val resp;
	unsigned int retval = 0;

	trace2(TR_domatch, 0, keylen);
	req.domain = domain;
	req.map = map;
	req.keydat.dptr = key;
	req.keydat.dsize = keylen;

	resp.valdat.dptr = NULL;
	resp.valdat.dsize = 0;
	(void) memset((char *)&resp, 0, sizeof (struct ypresp_val));

	/*
	 * Do the match request.  If the rpc call failed, return with status
	 * from this point.
	 */

	switch (clnt_call(pdomb->dom_client, YPPROC_MATCH,
			(xdrproc_t)xdr_ypreq_key, (char *)&req,
			(xdrproc_t)xdr_ypresp_val, (char *)&resp,
			*timeoutp)) {
	case RPC_SUCCESS:
		break;
	case RPC_TIMEDOUT:
		trace1(TR_domatch, 1);
		return (YPERR_YPSERV);
	default:
		trace1(TR_domatch, 1);
		return (YPERR_RPC);
	}

	/* See if the request succeeded */

	if (resp.status != YP_TRUE) {
		retval = ypprot_err(resp.status);
	}

	/* Get some memory which the user can get rid of as he likes */

	if (!retval && ((*val = malloc((size_t)
	    resp.valdat.dsize + 2)) == NULL)) {
		retval = YPERR_RESRC;
	}

	/* Copy the returned value byte string into the new memory */

	if (!retval) {
		*vallen = (int)resp.valdat.dsize;
		(void) memcpy(*val, resp.valdat.dptr,
		    (size_t)resp.valdat.dsize);
		(*val)[resp.valdat.dsize] = '\n';
		(*val)[resp.valdat.dsize + 1] = '\0';
	}

	CLNT_FREERES(pdomb->dom_client,
		(xdrproc_t)xdr_ypresp_val, (char *)&resp);
	trace1(TR_domatch, 1);
	return (retval);

}
