#pragma ident	"@(#)des.h	1.1	99/07/18 SMI"
/*
 * include/des.h
 *
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Include file for the Data Encryption Standard library.
 */

/* only do the whole thing once	 */
#ifndef DES_DEFS
#define DES_DEFS

#include "k5-int.h"

#ifndef KRB_INT32
#if (SIZEOF_LONG == 4)
#define KRB_INT32 long
#elif (SIZEOF_INT == 4)
#define KRB_INT32 int
#elif (SIZEOF_SHORT == 4)
#define KRB_INT32 short
#else
  ?== No 32 bit type available
#endif
#endif /* !KRB_INT32 */
#ifndef KRB_UINT32
#define KRB_UINT32 unsigned KRB_INT32
#endif

#ifndef NCOMPAT
#define C_Block des_cblock
#define Key_schedule des_key_schedule
#define ENCRYPT DES_ENCRYPT
#define DECRYPT DES_DECRYPT
#define KEY_SZ DES_KEY_SZ
#define string_to_key des_string_to_key
#define read_pw_string des_read_pw_string
#define random_key des_random_key
#define pcbc_encrypt des_pcbc_encrypt
#define key_sched des_key_sched
#define cbc_encrypt des_cbc_encrypt
#define cbc_cksum des_cbc_cksum
#define C_Block_print des_cblock_print
#define quad_cksum des_quad_cksum
typedef struct des_ks_struct bit_64;
#endif

#define des_cblock_print(x) des_cblock_print_file(x, stdout)

#endif /* DES_DEFS */
