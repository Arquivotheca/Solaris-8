/*
 * Copyright (c) 1991, 1995-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)obpsym_obp.c	1.9	99/10/04 SMI"

/*
 * This file contains the OBP specific parts of the symbol table
 * lookup support callbacks for SMCC SPARC OBP machines.  This file
 * contains the glue that gets control from the prom and knows how
 * to transfer the data into and out of the prom.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/kobj.h>
#include <sys/promif.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/reboot.h>
#include <rpc/types.h>
#include <rpc/xdr.h>

#define	DPRINTF(str)		if (obpsym_debug) prom_printf(str)
#define	DPRINTF1(str, a)	if (obpsym_debug) prom_printf(str, a);
#define	DPRINTF2(str, a, b)	if (obpsym_debug) prom_printf(str, a, b);
#define	DXPRINTF		if (obpsym_debug > 1) prom_printf

#define	MAX_NAME	128
#define	XDR_BUF_SIZE	256
XDR xdrm;
char xdrbuf[XDR_BUF_SIZE];

/*
 * Pre-1275 SPARC OpenBoot wrapper; encodes result in xdrbuf and
 * returns xdr-like encoded result to the firmware. The wrapper
 * is called directly from the firmware.
 */

static caddr_t
obp_name_to_value(char *fullname)
{
	int	retval;
	u_long	value;
	extern	int name_to_value(char *name, u_long *value);

	retval = name_to_value(fullname, &value);

	xdr_destroy(&xdrm);
	xdrmem_create(&xdrm, (caddr_t)xdrbuf, XDR_BUF_SIZE, XDR_ENCODE);
	xdr_int(&xdrm, &retval);
	retval = (int)value;
	xdr_int(&xdrm, &retval);
	return ((caddr_t)xdrbuf);
}

static char symbol[MAX_NAME];

static caddr_t
obp_value_to_name(u_long value)
{
	u_int	offset;
	char	*name = symbol;
	extern u_long value_to_name(u_long value, char *symbol);

	offset = value_to_name(value, name);

	xdr_destroy(&xdrm);
	xdrmem_create(&xdrm, (caddr_t)xdrbuf, XDR_BUF_SIZE, XDR_ENCODE);
	xdr_int(&xdrm, (int *)&offset);
	xdr_opaque(&xdrm, (caddr_t)name, strlen(name) + 1);
	return ((caddr_t)xdrbuf);
}

/*
 * This routine is used only on pre-1275 OpenBoot SPARC systems ...
 */
static int
defined_word(char *name)
{
	int is_defined;
	(void) sprintf(xdrbuf, "p\" %s\" find nip swap ! ", name);
	prom_interpret(xdrbuf, (int)(&is_defined), 0, 0, 0, 0);
	return (is_defined);
}

static int
no_can_do(void)
{
	int s;
	volatile caddr_t dup_vaddr;
	unsigned char c;

	if (prom_is_openprom() == 0)
		return (1);

	/*
	 * If the PROM already defines these words, (or we already have)
	 * then we know we can install the handlers, so we are done here.
	 */
	if (defined_word("set-symbol-lookup") != 0)
		return (0);

	/*
	 * Figure out if "ramforth" was done...
	 * If ' dup is writable, and a write to the location really
	 * writes something other than what was there, then we assume
	 * we will be able to patch things if we have to.
	 */
	prom_interpret("' dup swap !", (int)(&dup_vaddr), 0, 0, 0, 0);
	s = splhi();
	if (ddi_peekc(NULL, dup_vaddr, (char *)&c))
		goto romforth;
	if (ddi_pokec(NULL, dup_vaddr, ~c))
		goto romforth;
	if (*dup_vaddr == c)
		goto romforth;
	*dup_vaddr = c;
	splx(s);
	return (0);

romforth:
	printf("obpsym: You must use ramforth to use this module!\n");
	return (1);
}

static void
define_set_symbol_lookup(void)
{
	/*
	 * Get these definitions in the main dictionary and define
	 * the basic things we need.
	 */
	char *prologue =
		"only forth definitions "
		"0 value name-to-value "
		"0 value value-to-name "
		" "
		": set-symbol-lookup ( n2v v2n -- )  "
		"    is value-to-name is name-to-value "
		"; ";


	/*
	 * Rudimentary cstring support only for 1.x proms.
	 * Each cstr overwrites the last one!  We only do
	 * this if we can't find the definition of cstr.
	 */
	char *cstrdef =
		"d# 128 constant /cstrbuf "
		"/cstrbuf buffer: cstrbuf "
		": cstr ( pstr -- cstr ) "
		"    cstrbuf /cstrbuf 0 fill "
		"    count cstrbuf swap move  "
		"    cstrbuf "
		";";

	char *cstrlendef =
		": cstrlen ( cstr -- n ) "
		"    dup begin "
		"        dup  c@ "
		"    while "
		"        ca1+ "
		"    repeat swap - "
		";";

	/*
	 * New definitions of words that we patch into the prom,
	 * so the symbol table handlers get used if they are installed.
	 */
	char *definitions =
		": .adr2 ( addr -- ) "
		"  value-to-name  ?dup  if "		/* ( addr v2n ) */
		"    call "				/* ( addr retval ) */
		"    dup @ -1 = if "			/* ( addr retval ) */
		"      drop .h exit "
		"    then "				/* ( addr retval ) */
		"    swap .h space "			/* ( retval ) */
		"    dup @ swap 4 + dup cstrlen "    /* ( offset name,len ) */
		"    type ?dup  if .\" +\" .h then "
		"  else "
		"    .h "
		"  then "
		";"
		" "
		": literal2? "		/* ( pstr -- n true | pstr false ) */
		"   (literal?  ?dup if exit then "
		" "
		"   name-to-value  if "			/*  ( pstr ) */
		"      dup >r cstr "			/* ( cstr ) */
		"      name-to-value call  nip "	/* ( retval ) */
		"      dup @  if  drop r> false exit then " /*  ( retval ) */
		"      4 + @ r> drop true exit "	/* ( n true ) */
		"   then " 				/* ( pstr ) */
		"   false "				/* ( pstr false ) */
		";";

	char *patch =
		"  ' ctrace >body ' %pc origin - "
		" p\" tshift\" find if execute else drop 1 then >> "
		"  swap 40 bounds do i "
		"     w@ over =  if  i /token + leave  then "
		"  /token +loop "
		"  nip token@ ' .adr swap (is "
		" "
		"  ' .adr2 ' .adr >body token!  "
		"  ' exit ' .adr >body /token + token! "
		"  ' literal2? is literal? ";

	prom_interpret(prologue, 0, 0, 0, 0, 0);
	if (defined_word("cstr") == 0)
		prom_interpret(cstrdef, 0, 0, 0, 0, 0);
	if (defined_word("cstrlen") == 0)
		prom_interpret(cstrlendef, 0, 0, 0, 0, 0);
	prom_interpret(definitions, 0, 0, 0, 0, 0);
	prom_interpret(patch, 0, 0, 0, 0, 0);
}

void
set_sym_callbacks()
{
	prom_interpret("set-symbol-lookup",
	    (int)obp_value_to_name, (int)obp_name_to_value, 0, 0, 0);
}

int
install_callbacks(void)
{
	if (no_can_do() != 0)
		return (-1);

	if (defined_word("set-symbol-lookup") == 0)
		define_set_symbol_lookup();

	set_sym_callbacks();
	xdrmem_create(&xdrm, (caddr_t)xdrbuf, XDR_BUF_SIZE, XDR_ENCODE);
	return (0);
}

void
remove_callbacks(void)
{
	prom_interpret("set-symbol-lookup", 0, 0, 0, 0, 0);
	xdr_destroy(&xdrm);
}
