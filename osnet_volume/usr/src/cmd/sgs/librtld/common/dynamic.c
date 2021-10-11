/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 * Update any dynamic entry offsets.  One issue with dynamic entries is that
 * you only know whether they refer to a value or an offset if you know each
 * type.  Thus we check for all types we know about, it a type is found that
 * we don't know about then return and error as we have no idea what to do.
 */
#pragma ident	"@(#)dynamic.c	1.14	99/09/14 SMI"

#include	<libelf.h>
#include	<link.h>
#include	"libld.h"
#include	"msg.h"
#include	"rtld.h"
#include	"_rtld.h"

int
update_dynamic(Cache * cache, Cache * _cache, Rt_map * lmp, int flags,
    Addr addr, Off off, const char * file, Xword null, Xword data, Xword func,
    Xword entsize, Xword checksum)
{
	Dyn *		dyn = (Dyn *)_cache->c_data->d_buf, * posdyn = 0;
	const char *	strs;
	Cache *		__cache;

	/*
	 * If we're dealing with an object that might have bound to an external
	 * dependency establish our string table for possible NEEDED processing.
	 */
	if (flags & RTLD_REL_DEPENDS) {
		__cache = &cache[_cache->c_shdr->sh_link];
		strs = (const char *)__cache->c_data->d_buf;
	}

	/*
	 * Loop through the dynamic table updating all offsets.
	 */
	while (dyn->d_tag != DT_NULL) {
		switch ((Xword)dyn->d_tag) {
		case DT_NEEDED:
			if (posdyn) {
				Rt_map *	dlmp, * elmp;
				Listnode *	lnp;

				/*
				 * Determine whether this dependency has been
				 * loaded (this is the most generic way to check
				 * any alias names), and if it has been bound
				 * to, undo any lazy-loading position flag.
				 */
				if (dlmp = is_so_loaded(LIST(lmp),
				    (strs + dyn->d_un.d_val), 0)) {
					for (LIST_TRAVERSE(&EDEPENDS(lmp),
					    lnp, elmp)) {
						if (dlmp == elmp) {
						    posdyn->d_un.d_val &=
							~DF_P1_LAZYLOAD;
						    break;
						}
					}
				}
			}
			break;

		case DT_RELAENT:
		case DT_STRSZ:
		case DT_SYMENT:
		case DT_SONAME:
		case DT_RPATH:
		case DT_SYMBOLIC:
		case DT_RELENT:
		case DT_PLTREL:
		case DT_TEXTREL:
		case DT_VERDEFNUM:
		case DT_VERNEEDNUM:
		case DT_AUXILIARY:
		case DT_USED:
		case DT_FILTER:
		case DT_DEPRECATED_SPARC_REGISTER:
		case M_DT_REGISTER:
			break;

		case DT_PLTGOT:
		case DT_HASH:
		case DT_STRTAB:
		case DT_SYMTAB:
		case DT_INIT:
		case DT_FINI:
		case DT_VERDEF:
		case DT_VERNEED:
			dyn->d_un.d_ptr += addr;
			break;

		/*
		 * If the memory image is being used, this element would have
		 * been initialized to the runtime linkers internal link-map
		 * list.  Clear it.
		 */
		case DT_DEBUG:
			dyn->d_un.d_val = 0;
			break;

		/*
		 * The number of relocations may have been reduced if
		 * relocations have been saved in the new image.  Thus we
		 * compute the new relocation size and start.
		 */
		case DT_RELASZ:
		case DT_RELSZ:
			dyn->d_un.d_val = ((data + func) * entsize);
			break;

		case DT_RELA:
		case DT_REL:
			dyn->d_un.d_ptr = (addr + off + (null * entsize));
			break;

		/*
		 * If relative relocations have been processed clear the count.
		 */
		case DT_RELACOUNT:
		case DT_RELCOUNT:
			if (flags & RTLD_REL_RELATIVE)
				dyn->d_un.d_val = 0;
			break;

		case DT_PLTRELSZ:
			dyn->d_un.d_val = (func * entsize);
			break;

		case DT_JMPREL:
			dyn->d_un.d_ptr = (addr + off +
				((null + data) * entsize));
			break;

		/*
		 * Recompute the images elf checksum.
		 */
		case DT_CHECKSUM:
			dyn->d_un.d_val = checksum;
			break;

		/*
		 * If a flag entry is available indicate if this image has
		 * been generated via the configuration process (crle(1)).
		 * Because we only started depositing DT_FLAGS_1 entries in all
		 * objects starting with Solaris 8, set a feature flag if it
		 * is present (these got added in Solaris 7).
		 * The runtime linker may use this flag to search for a local
		 * configuration file - this is only meaningful in executables
		 * but the flag has value for identifying images regardless.
		 */
		case DT_FLAGS_1:
			if (flags & RTLD_CONFSET)
				dyn->d_un.d_val |= DF_1_CONFALT;
			break;

		case DT_FEATURE_1:
			if (flags & RTLD_CONFSET)
				dyn->d_un.d_val |= DTF_1_CONFEXP;
			break;

		/*
		 * If a position flag is available save it for possible update
		 * when processing the next NEEDED tag.
		 */
		case DT_POSFLAG_1:
			if (flags & RTLD_REL_DEPENDS) {
				posdyn = dyn++;
				continue;
			}
			break;

		/*
		 * Collect the defaults.
		 */
		default:
			/*
			 * If d_val is used, don't touch
			 */
			if ((dyn->d_tag >= DT_VALRNGLO) &&
			    (dyn->d_tag <= DT_VALRNGHI))
				break;

			/*
			 * If d_ptr is used, adjust
			 */
			if ((dyn->d_tag >= DT_ADDRRNGLO) &&
			    (dyn->d_tag <= DT_ADDRRNGHI)) {
				dyn->d_un.d_ptr += addr;
				break;
			}

			eprintf(ERR_FATAL, MSG_INTL(MSG_DT_UNKNOWN), file,
			    EC_XWORD(dyn->d_tag));
			return (1);
		}
		posdyn = 0;
		dyn++;
	}
	return (0);
}
