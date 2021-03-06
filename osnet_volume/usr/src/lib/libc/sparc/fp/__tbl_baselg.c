/*
 * Copyright (c) 1989-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)__tbl_baselg.c	1.6	96/12/06 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include "base_conversion.h"

/*
 *	The following table element i contains
 *
 *	nint(2**12 * log10( 1 + (i+0.5)/__TBL_BASELG_SIZE))
 *
 *	for use in base conversion estimation of log10(1.f) based
 *	on the most significant seven bits of f.
 */

const short unsigned __tbl_baselg[__TBL_BASELG_SIZE] = {
7,  21,  34,  48,  61,  75,  88, 101, 114, 127, 140, 153, 166, 178, 191, 203,
216, 228, 240, 252, 264, 276, 288, 300, 312, 323, 335, 346, 358, 369, 380, 391,
402, 414, 425, 435, 446, 457, 468, 478, 489, 500, 510, 520, 531, 541, 551, 561,
572, 582, 592, 602, 611, 621, 631, 641, 650, 660, 670, 679, 689, 698, 707, 717,
726, 735, 744, 753, 762, 772, 780, 789, 798, 807, 816, 825, 833, 842, 851, 859,
868, 876, 885, 893, 902, 910, 918, 927, 935, 943, 951, 959, 967, 976, 984, 992,
999, 1007, 1015, 1023, 1031, 1039, 1046, 1054, 1062, 1069, 1077, 1085,
1092, 1100, 1107, 1115, 1122, 1129, 1137, 1144, 1151, 1159, 1166, 1173,
1180, 1187, 1194, 1201, 1209, 1216, 1223, 1230
};
