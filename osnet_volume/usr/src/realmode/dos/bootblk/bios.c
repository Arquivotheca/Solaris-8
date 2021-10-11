/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)bios.c	1.12	99/01/31 SMI\n"

/*
 * In the original strap sources this file supplied the real BIOS
 * interface.  In the unified bootblk/strap it is just a cache layer
 * built on top of read_sectors (see bootio.c) that provides diskette
 * cacheing.  These routines are used from the DOS FS code.
 */
#include "bootblk.h"
#include "disk.h"

track_cache_t		Cache = {0};

void
init_bios(void)
{
	Cache.c_state = CacheInvalid;
}

/*
 *[]------------------------------------------------------------[]
 * | InitCache_bios -- initialize the track level caching. 	|
 *[]------------------------------------------------------------[]
 */
void
InitCache_bios(void)
{
	/*
	 * Enable the track cache only if there's a small number of
	 * sectors. Anything larger than 36 should be a hard disk and
	 * therefore the speed issue shouldn't be as important. At this
	 * point in time floppies always have less than 36 sectors per track.
	 */
	if (boot_dev.dev_type == DT_FLOPPY && boot_dev.u_secPerTrk <= 32) {
		if (Cache.c_buf == (char *)0) {
			Cache.c_start = -1;
			Cache.c_spt = boot_dev.u_secPerTrk;
			if (!(Cache.c_buf =
			    (char *)malloc_util(Cache.c_spt * SECSIZ))) {
				printf_util("No space for BIOS cache\n");
				Cache.c_state = CacheInvalid;
			}
			else
			  Cache.c_state = CacheValid;
		}
		else if (boot_dev.u_secPerTrk > Cache.c_spt) {
			printf_util("Can't reinit cache, currently too small\n");
			Cache.c_state = CacheInvalid;
		}
	}
	else
		Cache.c_state = CacheInvalid;

}

#define RETRY 2

ReadSect_bios(char far *addr, u_long sect, int count)
{
	u_long s;
	u_short k;

	Dprintf(DBG_READ, ("ReadSect_bios, %d sectors at %ld\n",
		count, sect));

	if (Cache.c_state != CacheInvalid) {
		while (count) {
			if ((Cache.c_state != CachePrimed) ||
			   (sect < Cache.c_start) ||
			   (sect >= (Cache.c_start + Cache.c_spt))) {

				s = (sect / Cache.c_spt) * Cache.c_spt;
	        		if (read_sectors(&boot_dev, s, Cache.c_spt,
	        				Cache.c_buf) != Cache.c_spt) {
				    	Dprintf(DBG_READ, ("Bad read in cache\n"));
					Cache.c_state = CacheInvalid;
					return 1;
				}
				Cache.c_state = CachePrimed;
				Cache.c_start = s;
				Cache.c_fills++;
			}
			k = MIN(count, Cache.c_spt - (sect - Cache.c_start));
			bcopy_util(Cache.c_buf + (sect - Cache.c_start) *
				SECSIZ, addr, k * SECSIZ);
			count -= k;
			sect += k;
			addr += (k * SECSIZ);
		}
		return 0;
	} else
	        return (read_sectors(&boot_dev, sect, count, addr) != count);
}



