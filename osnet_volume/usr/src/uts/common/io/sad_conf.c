
#ident	"@(#)sad_conf.c	1.9	93/06/11 SMI"

/*
 * Config dependent data structures for the Streams Administrative Driver
 * (or "Ballad of the SAD Cafe").
 */
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/stream.h>
#include <sys/sad.h>
#include <sys/kmem.h>

#ifndef	NSAD
#define	NSAD	16
#endif

struct saddev *saddev;			/* sad device array */
int	sadcnt = NSAD;			/* number of sad devices */

/*
 * Auto-push structures.
 */
#ifndef NAUTOPUSH
#define	NAUTOPUSH	32
#endif
struct autopush *autopush;
int	nautopush = NAUTOPUSH;

/*
 * Auto-push cache for streams administrative driver.
 */
#ifndef NSTRPHASH
#define	NSTRPHASH	64
#endif
extern struct autopush **strpcache;
extern int strpmask;

void
sad_initspace()
{
	saddev = (struct saddev *)
			kmem_zalloc(sadcnt * sizeof (struct saddev),
				    KM_SLEEP);

	autopush = (struct autopush *)
			kmem_zalloc(nautopush * sizeof (struct autopush),
				    KM_SLEEP);

	strpcache = (struct autopush **)
			kmem_zalloc((strpmask + 1) * sizeof (struct autopush),
				    KM_SLEEP);
}
