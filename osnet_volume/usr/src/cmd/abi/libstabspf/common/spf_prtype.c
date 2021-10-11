/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)spf_prtype.c	1.1	99/05/14 SMI"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stabspf_impl.h"

int
spf_prtype(FILE *stream, char const *type, int levels, void const *value)
{
	hnode_t		*hn;
	type_t		*tp;
	uint64_t	val;
	int		retval = -1;

	/*
	 * It is expected that hash lookups can fail because pointers
	 * do not get a named type in stabs.  So, on fail, we use parse_hints
	 * as the levels of indirection and we do a lookup on the root type
	 */
	if (hash_get_name(type, &hn, HASH_FIND) != STAB_SUCCESS) {
		goto fail;
	}

	if (ttable_td2ptr(hn->hn_td, &tp) != STAB_SUCCESS) {
		goto fail;
	}

	if (levels != 0)
		retval = print_pointer(stream, tp, levels, NULL, value);
	else if (tp->t_code & TS_BASIC) {
		switch (tp->t_tinfo.ti_bt.bt_nbits) {
		case 0:	/* bit width of zero denotes void */
			val = 0;
			break;
		case 8:
			val = *((uint8_t *)value);
			break;
		case 16:
			val = *((uint16_t *)value);
			break;
		case 32:
			val = *((uint32_t *)value);
			break;
		case 64:
			val = *((uint64_t *)value);
			break;
		default:
			/* Cannot happen */
			break;
		}
		retval = print_basic(stream, tp, NULL, 0, val);
	} else 	if (tp->t_code & TS_FLOAT)
		retval = print_float(stream, tp, NULL, value);
	else if (tp->t_code & TS_POINTER)
		retval = print_pointer(stream, tp, levels, NULL, value);
	else if ((tp->t_code & (TS_STRUCT | TS_UNION)))
		retval = print_struct(stream, tp, NULL, 0, value);
	else if (tp->t_code & TS_ENUM)
		retval = print_enum(stream, tp, NULL, (int *)value);
	else if (tp->t_code & TS_ARRAY)
		retval = print_array(stream, tp, NULL, value);

fail:
	return (retval);
}

static void
test_pattern(ulong_t pattern, void *buf_arg, size_t size)
{
	ulong_t *bufend = (ulong_t *)((char *)buf_arg + size);
	ulong_t *buf = buf_arg;

	while (buf < bufend - 3) {
		buf[3] = buf[2] = buf[1] = buf[0] = pattern;
		buf += 4;
	}
	while (buf < bufend)
		*buf++ = pattern;
}

#define	TESTSZ	8192

/*
 * This function has certain knowledge about the string table
 * therefore it should only be used for general cases.
 */
void
print_all_known(void)
{
	char *s;
	hnode_t *hnode;
	stroffset_t offset = 13;
	void *testval;

	testval = malloc(TESTSZ);
	if (testval == NULL) {
		(void) fprintf(stderr,
		    "Failure to allocate test data\n");
		return;
	}

	/*
	 * Fill the test memory with its own pointer value.
	 * This enables the dereferencing of pointers that
	 * are struct/union members will _always_ work.
	 */

	test_pattern((ulong_t)testval, testval, TESTSZ);

	while (string_offset2ptr(offset, &s) == STAB_SUCCESS) {
		if (hash_get_name(s, &hnode, HASH_FIND) == STAB_SUCCESS &&
		    hnode->hn_td != TD_NOTYPE) {
			(void) fprintf(stdout, "===> %s\n", s);
			(void) spf_prtype(stdout, s, 0, testval);
		}
		offset += strlen(s) + 1;
	}
	free(testval);
}

void
memory_report(void)
{
	hash_report();
	keypair_report();
	ttable_report();
	stringt_report();
}
