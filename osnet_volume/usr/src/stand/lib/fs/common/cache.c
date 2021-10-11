/*
 *  Copyright (c) 1991-1996, Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  This is mostly new code.  Major revisions were made to allow multiple
 *  file systems to share a common cache.  While this consisted primarily
 *  of including a "devid_t" pointer in the hash functions, I also re-
 *  organized everything to eliminate much of the duplicated code that
 *  had existed previously.
 */

#pragma	ident	"@(#)cache.c	1.15	96/09/18 SMI"

#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/sysmacros.h>
#include <sys/filep.h>
#include <sys/salib.h>

#ifndef	ICACHE_SIZE
/*
 *  These should probably be defined in an architecture-specific header
 *  file.  The values below are analogous to those used in eariler versions
 *  of this module.
 */

#define	ICACHE_SIZE 50	    /* Max number of I-node in file cache	*/
#define	DCACHE_SIZE 110	    /* Max number of cached directories		*/
#define	BCACHE_SIZE 250	    /* Max number of cached disk blocks		*/
#define	STR_CACHE_SIZE 10   /* Max number of cached strings 		*/
#define	DNLC_CACHE_SIZE 20  /* Max number of cached dnlc entries	*/
#define	NEGFN_CACHE_SIZE 70 /* Max number of cached negative file paths */
#define	CFSB_CACHE_SIZE 20  /* Max number of cached cachefs data blocks */
#endif

#define	Next 0		    /* Next pointer in Fwd/Bak link		*/
#define	Prev 1		    /* Previous pointer in Fwd/Back links	*/

#define	Frst 0		    /* Ptr to first element of a chain		*/
#define	Last 1		    /* Ptr to last element of a chain		*/

#define	Hash 2		    /* Offset of hash chain ptrs.		*/

typedef struct cache {	    /* Generic cache element:			*/
    struct cache *link[4];  /* .. Fwd/Bak links for hash chain & LRU	*/
    struct cache **chn;	    /* .. Hash chain link			*/
    int		   dev;	    /* .. Device file handle			*/
    void	 *data;	    /* .. Ptr to associated data		*/
    int		  size;	    /* .. Size of cached data			*/
} cache_t;

typedef struct head {	    /* Generic cache header:			*/
	cache_t	   *aged[2];	/* .. LRU list				*/
	int (*cmp)(cache_t *);	/* .. Ptr to comparison function	*/
	int	  size;		/* .. Size of "cache" objects		*/
	int	  maxblks;	/* .. Max number of cached elements	*/
	int	  count;	/* .. Current number of cached elements	*/
	int	  hits;		/* .. Total cache hits			*/
	int	  searches;	/* .. Total searches			*/
	int	  purges;	/* .. Total purges			*/
} head_t;

/* Constructor for cache headers:					*/
#define	cache_head(h, f, t, n) \
	{{(cache_t *)&h, (cache_t *)&h}, f, sizeof (t), n}

int read_opt;		/* Number of times cache was bypassed	*/
static int x_dev;	/* Target device ID saved here!		*/
static int x_len;		/* length of object			*/
static char *x_str;		/* string names				*/

extern caddr_t bkmem_alloc();
extern void bkmem_free();
extern void prom_panic(char *);

#define	LOG2(x) \
	(((x) <= 16)  ?	 4 : /* Yeah, it's ugly.  But it works! */ \
	(((x) <= 32)  ?  5 : /* .. Binary log should be part of */ \
	(((x) <= 64)  ?  6 : /* .. the language!		*/ \
	(((x) <= 128) ?	 7 : 8))))

static cache_t *
get_cache(cache_t *cap, head_t *chp)
{
	/*
	 *  Search cache:
	 *
	 *  The caller pass a pointer to the first "cache" object in the current
	 *  hash chain ["cap"] and a pointer to the corresponding cache header
	 *  ["chp"].  This routine follows the cache chain until it finds an
	 *  entry that matches both the current device [as noted in "x_dev"]
	 *  and the cache-specific comparison ["chp->cmp"].
	 *
	 *  Returns the address of the matching cache object or null if there
	 *  is none.
	 */

	while (cap) {
		/*
		 * Check all entries on the cache chain.  We expect
		 * chains to be relatively short, so we use a simple
		 * linear search.
		 */
		if ((x_dev == cap->dev) && (*chp->cmp)(cap)) {
			/*
			 * Found the entry we're looking for! Move it
			 * to the front of the cache header's LRU list
			 * before returing its addres to the caller.
			 */
			cap->link[Next]->link[Prev] = cap->link[Prev];
			cap->link[Prev]->link[Next] = cap->link[Next];

			cap->link[Prev] = (cache_t *)chp->aged;
			cap->link[Next] = chp->aged[Frst];
			chp->aged[Frst]->link[Prev] = cap;
			chp->aged[Frst] = cap;
			chp->hits += 1;
			break;
		}

		cap = cap->link[Hash+Next];
	}

	chp->searches += 1;
	return (cap);
}

static cache_t *
reclaim_cache(head_t *chp, int dev)
{
	/*
	 * Reclaim a cache element:
	 *
	 * This routine is used to: [a] free the oldest element from
	 * the cache headed at "chp" and return the address of the
	 * corresponding "cache_t" struct (iff dev == -1), or [b] free all
	 * elements on the cache headed at "chp" that belong to the
	 * indicated "dev"ice.
	 */
	cache_t *cap, *cxp;
	cache_t *cpp = (cache_t *)chp;

	while ((cap = cpp->link[Prev]) != (cache_t *)chp) {
		/*
		 * We follow the cache's LRU chain from oldest to
		 * newest member.  This ensures that we remove only
		 * the oldest element when we're called with a
		 * negative "dev" argument.
		 */
		if ((dev == -1) || (dev == cap->dev)) {
			/*
			 * This is one of the (perhaps the only)
			 * elements we're supposed to free.  Remove it
			 * from both the LRU list and its associated
			 * hash chain.  Then free the data bound the
			 * the cache_t element and, if "dev" is
			 * not -1, the element itself!
			 */
			cap->link[Prev]->link[Next] = cap->link[Next];
			cap->link[Next]->link[Prev] = cap->link[Prev];

			if ((cxp = cap->link[Hash+Prev]) != 0)
				cxp->link[Hash+Next] = cap->link[Hash+Next];
			else
				*(cap->chn) = cap->link[Hash+Next];

			if ((cxp = cap->link[Hash+Next]) != 0)
				cxp->link[Hash+Prev] = cap->link[Hash+Prev];

			bkmem_free((caddr_t)cap->data, cap->size);
			if (dev == -1)
				return (cap);

			bkmem_free((caddr_t)cap, chp->size);
			chp->count -= 1;

		} else {
			/*
			 * Skip this element, it's not one of the
			 * ones we want to free up.
			 */
			cpp = cap;
		}
	};

	return (0);
}

static cache_t *
set_cache(cache_t **ccp, head_t *chp)
{
	/*
	 *  Install a cache element:
	 *
	 *  The caller passes the address of cache descriptor ["chp"] and the
	 *  hash chain into which the new element is to be linked ["ccp"].  This
	 *  routine allocates a new cache_t structure (or, if the maximum number
	 *  of elements has already been allocated, reclaims the oldest element
	 *  from the cache), links it into the indicated hash chain, and returns
	 *  its address to the caller.
	 */
	cache_t *cap;

	if ((chp->count < chp->maxblks) &&
	    (cap = (cache_t *)bkmem_alloc(chp->size))) {
		/*
		 * We haven't reached the maximum cache size yet.
		 * Allocate a new "cache_t" struct to be added to the
		 * cache.
		 */
		chp->count += 1;

	} else {
		/*
		 * Cache is full.  Use the "reclaim_cache" routine to
		 * remove the oldest element from the cache.  This
		 * will become the cache_t struct associated with the
		 * new element.
		 */
		cap = reclaim_cache(chp, -1);
		chp->purges += 1;
	}

	bzero((char *)cap, chp->size);

	cap->chn = ccp;
	cap->link[Prev] = (cache_t *)chp;
	cap->link[Next] = chp->aged[Frst];
	cap->link[Prev]->link[Next] = cap->link[Next]->link[Prev] = cap;

	if ((cap->link[Hash+Next] = *ccp) != 0)
		(*ccp)->link[Hash+Prev] = cap;
	return (*ccp = cap);
}

/*
 *  The File Cache:
 *
 *  This cache (also known as the inode cache) is used to keep track of all
 *  files open on a given device.  The only special data required to locate
 *  a cache entry is the file reference number which is file-system dependent
 *  (for UNIX file systems, it's an inode number).
 */

typedef struct icache {		/* Inode cache element:		*/
	cache_t ic_hdr;		/* .. Standard header		*/
	int	ic_num;		/* .. I-node number		*/
} ic_t;

#define	IC_MAX_HDRS (1 << LOG2(ICACHE_SIZE/6))
#define	IC_HASH(d, i) (((d) + (i)) & (IC_MAX_HDRS - 1))

static int x_inode;

static int		    /* Cache search predicate:			    */
cmp_icache(cache_t *p)
{
	/* Just check the file number ("x_inode") ...	*/
	return (((ic_t *)p)->ic_num == x_inode);
}

static head_t	ic_head = cache_head(ic_head, cmp_icache, ic_t, ICACHE_SIZE);
static cache_t *ic_hash[IC_MAX_HDRS];

void *
get_icache(int dev, int inum)
{
	/*
	 *  Search File Cache:
	 *
	 *  This routine searches the file cache looking for the entry bound to
	 *  the given "dev"ice and file number ["inum"].  If said entry exists,
	 *  it returns the address of the associated file structure.  Otherwise
	 *  it returns null.
	 */
	cache_t *icp;

	x_dev = dev;
	x_inode = inum;
	icp = get_cache(ic_hash[IC_HASH(dev, inum)], &ic_head);

	return (icp ? (caddr_t)icp->data : 0);
}

void
set_icache(int dev, int inum, void *ip, int size)
{
	/*
	 *  Build a File Cache Entry:
	 *
	 * This routne installs the "size"-byte file structure at
	 * "*ip" in the inode cache where it may be retrieved by
	 * subsequent call to get_icache.
	 */
	ic_t *icp = (ic_t *)set_cache(&ic_hash[IC_HASH(dev, inum)], &ic_head);

	icp->ic_num = inum;
	icp->ic_hdr.data = ip;
	icp->ic_hdr.dev = dev;
	icp->ic_hdr.size = size;
}

/*
 *  The Directory Cache:
 *
 *  This cache is designed to speed directory searches.	 Each entry cor-
 *  responds to a directory entry that was used in a pathname resolution.
 *  The idea is that most files used by the boot wil be contained in a hand-
 *  full of directories, so we can speed searches if we know ahead of time
 *  just where these directories are.
 */

typedef struct dcache {		/* Directory cache objects:	*/
	cache_t dc_hdr;		/* .. Standard header		*/
	int	dc_inum;	/* .. File number		*/
	int	dc_pnum;	/* .. Parent diretory's file number */
} dc_t;

#define	DC_MAX_HDRS (1 << LOG2(DCACHE_SIZE/6))
#define	DC_HASH(d, n, l) (((d) + (n)[0] + (n)[(l)-1] + (l)) & (DC_MAX_HDRS-1))

static char *x_name;
static int x_pnum;

static int
cmp_dcache(cache_t *p) /* Cache Search predicate:	*/
{
	/* Check name, length, and parent's file number	*/
	return ((x_len == p->size) && (x_pnum == ((dc_t *)p)->dc_pnum) &&
	    (strcmp((char *)p->data, x_name) == 0));
}

static head_t	dc_head = cache_head(dc_head, cmp_dcache, dc_t, DCACHE_SIZE);
static cache_t *dc_hash[DC_MAX_HDRS];

int
get_dcache(int dev, char *name, int pnum)
{
	/*
	 *  Search Directory Cache:
	 *
	 *  This routine searches the directory cache for an entry
	 *  associated with directory number "pnum" from the given
	 *  file system that de-scribes a file of the given "name".
	 *  If we find such an entry, we return the corresponding file
	 *  number, 0 otherwise.
	 */
	dc_t *dcp;

	x_dev = dev;
	x_len = strlen(name)+1;
	x_pnum = pnum;
	x_name = name;
	dcp = (dc_t *)get_cache(dc_hash[DC_HASH(dev, name, x_len)], &dc_head);

	return (dcp ? dcp->dc_inum : 0);
}

void
set_dcache(int dev, char *name, int pnum, int inum)
{
	/*
	 *  Build Directory Cache Entry:
	 *
	 *  This routine creates directory cache entries to be retrieved later
	 *  via "get_dcache".  The cache key is composed of three parts: The
	 *  device specifier, the file name ("name"), and the file number of
	 *  the directory containing that name ("pnum").  The data portion of
	 *  the entry consists of the file number ("inum").
	 */

	int len = strlen(name)+1;
	dc_t *dcp =
	    (dc_t *)set_cache(&dc_hash[DC_HASH(dev, name, len)], &dc_head);

	if (dcp->dc_hdr.data = (void *)bkmem_alloc(len)) {
		/*
		 * Allocate a buffer for the pathname component, and
		 * make this the "data" portion of the generalize
		 * "cache_t" struct. Also fill in the cache-specific
		 * fields (pnum, inum).
		 */
		dcp->dc_pnum = pnum;
		dcp->dc_inum = inum;
		dcp->dc_hdr.dev = dev;
		dcp->dc_hdr.size = len;
		bcopy(name, (char *)dcp->dc_hdr.data, len);

	} else {
		/*
		 * Not enough memory to make a copy of the name!
		 * There's probably not enough to do much else either!
		 */
		prom_panic("no memory for directory cache");
	}
}

/*
 *  Disk Block Cache:
 */

typedef struct bcache {	    /* Disk block cache objects:		*/
	cache_t		bc_hdr;	/* .. Standard header			*/
	unsigned long	bc_blk;	/* .. The block number			*/
} bc_t;

#define	BC_MAX_HDRS (1 << LOG2(BCACHE_SIZE/6))
#define	BC_HASH(d, b, l) (((d) + (b) + ((l) >> 8)) & (BC_MAX_HDRS-1))

static unsigned long x_blkno;

static int
cmp_bcache(cache_t *p) /* Cache Search predicate:		*/
{
	/* Check block number, buffer size	*/
	return ((x_len == p->size) && (x_blkno == ((bc_t *)p)->bc_blk));
}

static head_t	bc_head = cache_head(bc_head, cmp_bcache, bc_t, BCACHE_SIZE);
static cache_t *bc_hash[BC_MAX_HDRS];

caddr_t
get_bcache(fileid_t *fp)
{
	/*
	 *  Search Disk Block Cache:
	 *
	 *  This should be getting pretty monotonous by now.  Aren't generalized
	 *  subroutines ("objects", if you prefer) great?
	 */
	cache_t *bcp;

	x_len = fp->fi_count;
	x_blkno = fp->fi_blocknum;
	x_dev = fp->fi_devp->di_dcookie;
	bcp = get_cache(bc_hash[BC_HASH(x_dev, x_blkno, x_len)], &bc_head);

	return (bcp ? (caddr_t)bcp->data : 0);
}

int
set_bcache(fileid_t *fp)
{
	/*
	 *  Insert Disk Block Cache Entry:
	 *
	 *  In this case, we actually read the requested block into a
	 *  dynamically allocated buffer before inserting it into the
	 *  cache.  If the read fails, we return a non-zero value.
	 *
	 *  The search keys for disk blocks are the block number and
	 *  buffer size.  The data associated with each entry is the
	 *  corresponding data buffer.
	 */
	bc_t *bcp;

	if (fp->fi_memp = bkmem_alloc(x_len = fp->fi_count)) {
		/*
		 *  We were able to succesffully allocate an input
		 *  buffer, now read the data into it.
		 */
		extern int diskread(fileid_t *);

		if (diskread(fp) != 0) {
			/*
			 * I/O error on read. Free the input buffer,
			 * print an error message, and bail out.
			 */
			bkmem_free(fp->fi_memp, x_len);
			printf("disk read error\n");
			return (-1);
		}

		x_blkno = fp->fi_blocknum;
		x_dev = fp->fi_devp->di_dcookie;
		bcp = (bc_t *)
		    set_cache(&bc_hash[BC_HASH(x_dev, x_blkno, x_len)],
			&bc_head);
		bcp->bc_blk = x_blkno;
		bcp->bc_hdr.dev = x_dev;
		bcp->bc_hdr.size = x_len;
		bcp->bc_hdr.data = (void *)fp->fi_memp;

	} else {
		/*
		 * We could be a bit more convervative here by
		 * calling "set_cache" before we try to allocate a
		 * buffer (thereby giving us a chance to re-use a
		 * previously allocated buffer) but the error recovery
		 * is a bit trickier, and if we're that short on memory
		 * we'll have trouble elsewhere anyway!
		 */
		prom_panic("can't read - no memory");
	}

	return (0);
}

/*
 *  The DNLC Cache:
 *
 *  This cache is designed to speed directory searches.  Each entry cor-
 *  responds to a directory pathname that has been resolved to the inode.
 *  The idea is that most files used by the boot wil be contained in a hand-
 *  full of directories, so we can speed searches if we know ahead of time
 *  just where these directories are.
 */

typedef struct dnlc {		/* Directory cache objects	*/
	cache_t dnlc_hdr;	/* Standard header		*/
	ino64_t dnlc_inum;	/* File number			*/
} dnlc_t;

#define	DNLC_MAX_HDRS (1 << LOG2(DNLC_CACHE_SIZE/6))

#define	DNLC_HASH(d, n, l) \
	(((d) + (n)[0] + (n)[(l)-1] + (l)) & (DNLC_MAX_HDRS-1))

/*
 * compare items on hash by length & string
 */
static int
cmpdnlc(cache_t *p)
{
	return ((x_len == p->size) &&
	    (strcmp((char *)p->data, x_str) == 0));
}

static head_t dnlc_head =
	cache_head(dnlc_head, cmpdnlc, dnlc_t, DNLC_CACHE_SIZE);

static cache_t *dnlc_hash[DNLC_MAX_HDRS];

ino64_t
get_dnlc(int dev, char *name)
{
	/*
	 * Search Directory Cache:
	 * This routine searches the dnlc cache for an entry with the
	 * pathname "name".  Returns the cached inode.
	 */
	dnlc_t *dnlcp;

	x_dev = dev;
	x_len = strlen(name)+1;
	x_str = name;

	dnlcp = (dnlc_t *)get_cache(dnlc_hash[DNLC_HASH(x_dev, x_str, x_len)],
	    &dnlc_head);
	return (dnlcp ? dnlcp->dnlc_inum : (ino64_t)0);
}

void
set_dnlc(int dev, char *name, ino64_t inum)
{
	/*
	 *  Build DNLC Entry:
	 *
	 *  This routine creates DNLC entries to be retrieved later
	 *  via "get_dnlc".  The cache key is composed of three parts: The
	 *  device specifier, the pathname ("name"), and the length of
	 *  pathname ("name") The data portion of the entry consists of
	 *  the file number ("inum").
	 */

	dnlc_t *dnlcp;

	x_len = strlen(name)+1;
	x_dev = dev;
	x_str = name;

	dnlcp = (dnlc_t *)set_cache(&dnlc_hash[DNLC_HASH(x_dev, x_str, x_len)],
	    &dnlc_head);

	if (dnlcp->dnlc_hdr.data = (void *)bkmem_alloc(x_len)) {
		/*
		 * Allocate a buffer for the pathname component, and
		 * make this the "data" portion of the generalize "cache_t"
		 * structure.
		 * Fill in the cache-specific field inum.
		 */
		dnlcp->dnlc_inum = inum;
		dnlcp->dnlc_hdr.dev = x_dev;
		dnlcp->dnlc_hdr.size = x_len;
		bcopy(name, (char *)dnlcp->dnlc_hdr.data, x_len);

	} else {
		prom_panic("no memory for DNLC cache");
	}
}

/*
 *  The String Cache
 *
 *  This cache is used by cachefs routines to store lookups of the
 *  symbolic links.
 */
typedef struct string_cache {
    cache_t sc_hdr;
} sc_t;

#define	STR_MAX_HDRS (1 << LOG2(STR_CACHE_SIZE/6))
#define	STR_HASH(d, n, l) \
	(((d) + (n)[0] + (n)[(l)-1] + (l)) & (STR_MAX_HDRS-1))

static int
cmpstr(cache_t *p) {
	return ((x_len == p->size) &&
		(strcmp((char *)p->data, x_str) == 0));
}

static head_t str_head = cache_head(str_head, cmpstr, sc_t, STR_CACHE_SIZE);
static cache_t *str_hash[STR_MAX_HDRS];

char *
set_string_cache(int dev, char *str)
{
	sc_t *strcp;

	x_len = strlen(str)+1;
	x_dev = dev;
	x_str = str;

	strcp = (sc_t *)set_cache(&str_hash[STR_HASH(x_dev, x_str, x_len)],
	    &str_head);

	if (strcp->sc_hdr.data = (void *)bkmem_alloc(x_len)) {
		/*
		 * Allocate a buffer for the string and make this the
		 * "data" portion of the generalized "cache_t" struct.
		 */
		strcp->sc_hdr.dev = x_dev;
		strcp->sc_hdr.size = x_len;
		bcopy(str, (char *)strcp->sc_hdr.data, x_len);
	} else {
		prom_panic("no memory for cachefs string cache");
	}

	return ((char *)(strcp->sc_hdr.data));
}

char *
get_string_cache(int dev, char *str)
{

	sc_t *strcp;

	x_len = strlen(str)+1;
	x_str = str;
	x_dev = dev;

	strcp = (sc_t *)get_cache(str_hash[STR_HASH(x_dev, x_str, x_len)],
	    &str_head);

	return ((char *)(strcp ? strcp->sc_hdr.data :
	    set_string_cache(dev, str)));
}

/*
 *  The Negative Filename Cache
 *
 *  This cache is used by cachefs to track files it knows not to exist.
 *  Entries are made when cachefs fails to find a given file on both
 *  the frontfs and backfs.
 */
typedef struct negfn_cache {
    cache_t negfn_hdr;
} negfn_t;

#define	NEGFN_MAX_HDRS (1 << LOG2(NEGFN_CACHE_SIZE/6))
#define	NEGFN_HASH(d, n, l) \
	(((d) + (n)[0] + (n)[(l)-1] + (l)) & (NEGFN_MAX_HDRS-1))

static head_t negfn_head =
	cache_head(negfn_head, cmpstr, negfn_t, NEGFN_CACHE_SIZE);
static cache_t *negfn_hash[NEGFN_MAX_HDRS];

char *
get_negative_filename(int dev, char *str)
{
	negfn_t *negfnp;

	x_len = strlen(str)+1;
	x_str = str;
	x_dev = dev;

	negfnp =
	    (negfn_t *)get_cache(negfn_hash[NEGFN_HASH(x_dev, x_str, x_len)],
	    &negfn_head);

	return ((char *)(negfnp != NULL ? negfnp->negfn_hdr.data : 0));
}

void
set_negative_filename(int dev, char *str)
{
	negfn_t *negfnp;

	x_len = strlen(str) + 1;
	x_dev = dev;
	x_str = str;

	negfnp =
	    (negfn_t *)set_cache(&negfn_hash[NEGFN_HASH(x_dev, x_str, x_len)],
	    &negfn_head);

	if (negfnp->negfn_hdr.data = (void *)bkmem_alloc(x_len)) {
		/*
		 * Allocate a buffer for the filename and make it the
		 * "data" portion of the generalized "cache_t" struct.
		 */
		negfnp->negfn_hdr.dev = dev;
		negfnp->negfn_hdr.size = x_len;
		bcopy(str, (char *)negfnp->negfn_hdr.data, x_len);
	} else {
		prom_panic("no memory for negative filename cache");
	}
}

/*
 *  CacheFS Block Cache:
 */
typedef struct cfsbcache {
	cache_t cfsb_hdr;
	ino64_t cfsb_blkno;
} cfsb_t;

static ino64_t x_cfsblk;		/* used for cache lookup */

#define	CFSB_MAX_HDRS (1 << LOG2(CFSB_CACHE_SIZE/6))
#define	CFSB_HASH(d, b, l) (((d) + (b) + ((l) >> 8)) & (CFSB_MAX_HDRS-1))

static int
cmpcfsb(cache_t *p)
{
	return ((x_len == p->size) && (x_cfsblk == ((cfsb_t *)p)->cfsb_blkno));
}

static head_t cfsb_head =
	cache_head(cfsb_head, cmpcfsb, cfsb_t, CFSB_CACHE_SIZE);
static cache_t *cfsb_hash[CFSB_MAX_HDRS];

char *
get_cfsb(int dev, ino64_t blkno, u_int size)
{
	/*
	 *  Search CFS Block Cache:
	 */
	cfsb_t *cfsbp;

	x_len = size;
	x_cfsblk = blkno;
	x_dev = dev;
	cfsbp = (cfsb_t *)get_cache(
	    cfsb_hash[CFSB_HASH(x_dev, (uint_t)(blkno), x_len)],
	    &cfsb_head);

	return (cfsbp != NULL ? (char *)(cfsbp->cfsb_hdr.data) : NULL);
}

void
set_cfsb(int dev, ino64_t blkno, void *buf, u_int size)
{
	/*
	 *  Insert CFS Block Entry:
	 *
	 * The search keys for cachefs blocks are the block
	 * number and buffer size.
	 * The data associated with each entry is the
	 * corresponding data buffer.
	 */

	cfsb_t *cfsbp;

	x_cfsblk = blkno;
	x_len = size;
	x_dev = dev;

	cfsbp = (cfsb_t *)set_cache(
	    &cfsb_hash[CFSB_HASH(x_dev, (uint_t)blkno, x_len)], &cfsb_head);
	if (cfsbp->cfsb_hdr.data = bkmem_alloc(x_len)) {
		cfsbp->cfsb_blkno = blkno;
		cfsbp->cfsb_hdr.dev = x_dev;
		cfsbp->cfsb_hdr.size = x_len;
		bcopy((char *)buf, (char *)cfsbp->cfsb_hdr.data, x_len);
	} else {
		prom_panic("no memory for cachefs buffer");
	}
}

void
release_cache(int dev)
{
	/*
	 *  Reclaim all cache entries:
	 *
	 *  This routine is called by the file-system's "closeall" method.  It
	 *  removes all cache entries associated with that file system from the
	 *  global cache and release any resources bound to said entrires.
	 */

	reclaim_cache(&ic_head, dev);
	reclaim_cache(&dc_head, dev);
	reclaim_cache(&bc_head, dev);
	reclaim_cache(&str_head, dev);
	reclaim_cache(&dnlc_head, dev);
	reclaim_cache(&negfn_head, dev);
	reclaim_cache(&cfsb_head, dev);
}

void
print_cache_data()
{
	/*
	 *  Print some cacheing statistics ...
	 */
	static char	*tag[] = { "inode", "directory", "disk block",
	    "string", "dnlc", "negative file", "cachefs", 0};
	static head_t	*hdp[] = { &ic_head, &dc_head, &bc_head,
	    &str_head, &dnlc_head, &negfn_head, &cfsb_head, 0 };

	int j;

	for (j = 0; tag[j]; j++) {
		/*
		 * Print statistics maintained in the header
		 * ("head_t" struct) of each of the above caches.
		 */
		head_t *hp = hdp[j];

		if (j)
			printf("\n");
		printf("%s cache:\n", tag[j]);
		printf("   max size %d\n", hp->maxblks);
		printf("   actual size %d\n", hp->count);
		printf("   total searches %d\n", hp->searches);
		printf("   cache hits %d\n", hp->hits);
		printf("   cache purges %d\n", hp->purges);
	}

	printf("\nread opts %d\n", read_opt);
}
