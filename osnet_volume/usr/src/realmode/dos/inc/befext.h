/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * befext.h -- public definitions for befext routines
 */

#ifndef	_BEFEXT_H
#define	_BEFEXT_H

#ident "@(#)befext.h	1.38	98/10/26 SMI"

#include <dostypes.h>

/*
 *	The "old" entry point to the driver is at a fixed offset and did
 *	not change -- by jumping to the old offset you still get the "probe
 *	and install" function of the driver.  The extended interface is also
 *	based on a fixed offset from the beginning of the driver.  To see if
 *	a real-mode driver supports the extended interface, check the 4-byte
 *	integer at offset BEF_EXTMAGOFF in the driver and see if it is equal
 *	to BEF_EXTMAGIC.  If so, the extended functions are available by
 *	jumping to the location at offset BEF_EXTENTRY.
 */

#define	BEF_SIGNATURE	0x5A4D
#define	BEF_EXTMAGOFF	0x1C
#define	BEF_EXTMAGIC	0x2E626566
#define	BEF_EXTENTRY	(BEF_EXTMAGOFF+4)

/*
 *  The desired function is specified in register AX when the caller
 *  jumps to BEF_EXTENTRY.  The following are the currently defined
 *  functions...
 */

#define	BEF_PROBEINSTAL	0	/* probe & install (old versions) 	*/
#define	BEF_LEGACYPROBE	1	/* probe for legacy devices		*/
#define	BEF_INSTALLONLY	2	/* install the given configurations	*/

/*
 *  The driver returns a value in AX to specify any error conditions...
 */

#define	BEF_OK		0	/* function was executed		*/
#define	BEF_RESFAIL	1	/* not enough resources for devices	*/
#define	BEF_FAIL	(-1)	/* unknown failure			*/
#define	BEF_CALLBACK	(-3)	/* attempted callback from PROBEINSTAL	*/
#define	BEF_BADFUNC	(-4)	/* invalid dispatch function		*/
#define	BEF_LOADFAIL	(-255)	/* couldn't load .bef file!		*/

/*
 *  The following struct is used for the commands:
 *
 *	BEF_LEGACYPROBE
 *	BEF_INSTALLONLY
 *
 *  A far pointer to this struct is in ES:DI on entry to the driver.  The
 *  two routine pointers provide the callback interface described below.
 */

struct bef_interface {
	int version;
	int (far *resource)(int, char far *, DWORD far *, DWORD far *);
	int (far *node)(int);
	int (far *prop)(int, char far *, char far * far *, int far *, int);
	int (far *putc)(int);
	int (far *mem_adjust)(unsigned short, unsigned short);
	unsigned short mem_base;
	unsigned short mem_size;
};

/*
 * All known version numbers should be defined here so that code can
 * do appropriate compatibility testing.  Items should only ever be
 * appended to the structure definition, so the names here are chosen
 * according to what was the last member at the time the number was
 * used.  BEF_IF_VERS should always be equal to the newest version.
 */
#define	BEF_IF_VERS_PUTC	0x0100
#define	BEF_IF_VERS_MEM_SIZE	0x0200
#define	BEF_IF_VERS		0x0200

				/* Functions for (*resource) callback:	*/
#define	RES_SET		1	/* .. set given resource		*/
#define	RES_GET		2	/* .. get given resource		*/
#define	RES_REL		3	/* .. release the given resource	*/

/*
 * The following options may be ORed into the "RES_SET" function code:
 */
#define	RES_SHARE	0x0100	/* .. request is for shared reservation	*/
#define	RES_WEAK	0x0200	/* .. request is for weak resource binding */
#define	RES_SILENT	0x0400	/* .. don't complain about conflicts to user */
#define	RES_USURP	0x0800	/* .. request can usurp weak bindings */

				/* Return codes from (*resource) callback:  */
#define	RES_OK		0	/* .. op functioned normally		*/
#define	RES_CONFLICT	1	/* .. set_res: resource conflict	*/
#define	RES_FAIL	(-1)	/* .. serious error			*/

				/* Functions for (*node) callback:	*/
#define	NODE_START	1	/* .. start a new node			*/
#define	NODE_FREE	2	/* .. error occur, free up node space	*/
#define	NODE_DONE	3	/* .. node complete			*/
#define	NODE_INCOMPLETE	4	/* couldn't get all needed resources */

/*
 * The following options may be ORed into the "NODE_DONE" function code:
 */
#define	NODE_UNIQ	0x0100	/* .. request is for unique node	*/

				/* Return codes from (*node) callback:	*/
#define	NODE_OK		0	/* .. op functioned normally		*/
#define	NODE_FAIL	(-1)	/* .. generic failure			*/

				/* Functions for (*prop) callback:	*/
#define	PROP_GET	1	/* .. Get a device property		*/
#define	PROP_SET	2	/* .. Set a device property		*/
#define	PROP_SET_ROOT	3	/* .. Set a property in root node	*/
#define	PROP_GET_ROOT	4	/* .. Get a property from root node	*/

				/* Return codes from (*prop) callback:	*/
#define	PROP_OK		0	/* .. op functioned normally		*/
#define	PROP_FAIL	(-1)	/* .. generic failure			*/

				/* Property Types */
#define	PROP_CHAR	0	/* property is a character string */
#define	PROP_BIN	1	/* property is a set of binary bytes */

				/* Return codes from (*mem_adjust) callback: */
#define	MEM_OK		0	/* .. op functioned normally */
#define	MEM_FAIL	(-1)	/* .. couldn't change to desired size */

/*
 * Bus Types
 */
#define	RES_BUS_ISA	0x01		/* .. ISA (not self identifying) */
#define	RES_BUS_EISA	0x02		/* .. EISA			*/
#define	RES_BUS_PCI	0x04		/* .. PCI			*/
#define	RES_BUS_PCMCIA	0x08		/* .. PCMCIA			*/
#define	RES_BUS_PNPISA	0x10		/* .. Plug-n-Play ISA		*/
#define	RES_BUS_MCA	0x20		/* .. IBM Microchannel		*/
#define	RES_BUS_I8042	0x40		/* .. i8042 (keyboard / mouse)	*/

#define	RES_BUS_NO	7		/* Number of busses		*/

#define	RES_BUS_PCICLASS 0x03		/* PCI class			*/

/*
 *  Interface routines, client (driver) side:
 *
 *	int node_op(int func)
 *
 *	    Perform the indicated "func"tion on the current device node.
 *	    Valid function codes are:
 *
 *		NODE_START:  Establish a new device node for this driver
 *		NODE_FREE:   Cancel the current node (e.g, due to conflicts)
 *		NODE_DONE:   Put the current node in the ESCD (or device tree).
 *
 *	int set_res(char far *name, DWORD far *val, DWORD far *len, int flag)
 *
 *	    Makes a reservation for the "name"d resource on behalf of the
 *	    current node.  The resource(s) to be reserved are identified in
 *	    the "len"-word "val"ue buffer, the layout of which is type specific:
 *
 *		name	 len	value buffer
 *		----	 ---	------------
 *
 *		"slot"	   1	EISA/MCA slot number in high/low half of
 *				the word.
 *
 *		"name"	   2	EISA device name (compressed form) in the
 *				first word, bus type in the second word.
 *				If the bus type is PCI, the compressed name
 *				consists of the concatenation of the 16-bit
 *				vendor and device names.  If bus type is MCA,
 *				 it consists of the 16-bit MCA device id.
 *
 *		"port"	  3N	3-word tuple for each resource; Port address
 *				in first word, size (in bytes) in 2nd, and
 *				flags (see "RES_PORT_*", above) in third.
 *
 *		"irq"	  2N	2-word tuple for each resource; IRQ number in
 *				first word, flags (see "RES_IRQ_*", above)
 *				in second.
 *
 *		"dma"	  2N	2-word tuple for each resource; DMA channel
 *				number in 1st word, flags (see "RES_DMA_*",
 *				above) in second.
 *
 *		"mem"	  3N	3-word tuple for each resource; memory addr
 *				in first word, size (in bytes) in 2nd, and
 *				flags (see "RES_MEM_*", above) in third.
 *
 *		"addr"	  1	 For pci devices only. This is the pci address
 *				 in the form:-
 *				 <8 bit bus no> <5 bit dev> <3 bit func>
 *
 *	The global "flag" bits are:
 *
 *		RES_SHARE: Reservation may be shared with other devices
 *		RES_WEAK:  Weak binding resource. If another device needs
 *			   this resource, then the whole device is stolen.
 *			   This works for com port irqs and parallel port
 *			   resources, and allows parallel port devices
 *			   (eg Xircom PE, or trantor) to take over the device
 *			   or other devices to use the com port irqs (eg
 *			   smc card at irq3).
 *
 *	    Drivers must obey the following rules when using the set_res
 *	    callback:
 *
 *		1. No more than one "slot" or "name" resource can be set
 *		   per device.
 *
 *		2. Reservations cannot be "interrupted".  Once you start
 *		   reserving resources of a given type, you must reserve all
 *		   resources of that type before going on to the next type.
 *
 *	    Set_res returns 0 if the reservation is made successfully,
 *	    RES_CONFLICT if the reservations conflicts with another device,
 *	    or RES_FAIL if the interface protocol is violated.
 *
 *	int rel_res(char far *name, DWORD far *val, DWORD far *len)
 *
 *	    Releases a reservation made by "set_res".  Arguments are the same
 *	    as for "set_res", except that no flags are specified.  Returns 0
 *	    if it works, RES_FAIL if the specified resource is not currently
 *	    reserved. You cannot release a "slot" or "name" reservation; use
 *	    NODE_FREE instead.
 *
 *	int get_res(char far *name, DWORD far *val, DWORD far *len)
 *
 *	    Returns the current device's assignments for the "name"d resource
 *	    (as established by "set_res" or by the PnP BIOS/CM) in the "len"-
 *	    word "val"ue buffer.  The value buffer is type-specific as de-
 *	    scribed above.  The "Len"gth word specifies the maximum value
 *	    buffer size (in 32-bit words) upon entry, the number of words
 *	    actually used upon return.
 *
 *	    Returns 0 if the resource assignments are successfully returned,
 *	    even if there are no resources of the indicated type assigned to
 *	    the current device ("len" is set to zero in this case).  Returns
 *	    RES_FAIL if something goes wrong.
 *
 *	int set_prop(char far *name, char far *val, int far *len, int bin)
 *
 *	    Sets the "name"d property of length "len" for the current node to
 *	    the indicated "val"ue.  If the current node does not have a
 *	    property with the the given name, one is created for it.  If the
 *	    "val"ue pointer is null, any property with the given name
 *	    is deleted.  "Bin" != 0 specifies the property is binary.
 *
 *	    Returns 0 if successful, PROP_FAIL if there is no active device
 *	    node.
 *
 *	int get_prop(char far *name, char far* far* val, int far *len)
 *
 *	    Stores a pointer to the "name"d property in the "val"ue buffer;
 *	    NULL if neither the current node nor the "options" node has a
 *	    property by that name.  Stores the length in "len".
 *	    Returns 0 if successful, PROP_FAIL if there is no active
 *	    device node.
 *
 *	int set_root_prop(char far *name, char far *val, int far *len, int bin)
 *
 *	    Sets the "name"d property of length "len" for the / node to
 *	    the indicated "val"ue.  If the / node does not have a
 *	    property with the the given name, one is created for it.  If the
 *	    "val"ue pointer is null, any property with the given name
 *	    is deleted.  "Bin" != 0 specifies the property is binary.
 *
 *	    Always returns PROP_OK
 *
 *	int get_root_prop(char far *name, char far* far* val, int far *len)
 *
 *	    Stores a pointer to the "name"d property in the "val"ue buffer;
 *	    NULL if neither the current node nor the "options" node has a
 *	    property by that name.  Stores the length in "len".
 *	    Always returns PROP_OK
 *
 *	int mem_adj(unsigned short mem_base, unsigned short mem_size)
 *
 *	    Adjusts the BEF's memory segment that starts at mem_base to
 *	    be of size mem_size.  Mem_base and mem_size are in 16 byte
 *	    paragraphs.
 */

extern int node_op(int);
extern int set_prop(char far *, char far * far *, int far *, int);
extern int get_prop(char far *, char far * far *, int far *);
extern int set_root_prop(char far *, char far * far *, int far *, int);
extern int get_root_prop(char far *, char far * far *, int far *);
extern int get_res(char far *, DWORD far *, DWORD far *);
extern int rel_res(char far *, DWORD far *, DWORD far *);
extern int set_res(char far *, DWORD far *, DWORD far *, int);
extern int mem_adj(unsigned short, unsigned short);

/* Length of {get|set}_res tuples (in DWORDs) for each resource type:	*/

#define	MaxTupleSize	3	/* .. Size of array with room for any tuple */
#define	SlotTupleSize	1	/* .. Slot number			*/
#define	NameTupleSize	2	/* .. Device name, bus type		*/
#define	PortTupleSize	3	/* .. Base address, length, flags	*/
#define	IrqTupleSize	2	/* .. Irq number, flags			*/
#define	DmaTupleSize	2	/* .. Dma number, flags			*/
#define	MemTupleSize	3	/* .. Base address, length (bytes), flags */
#define	AddrTupleSize	1	/* .. PCI addr				*/

/* Coding of PCI vendor/device ids in device name */
#define	RES_PCI_NAME_TO_VENDOR(x)	((ushort)((x) >> 16))
#define	RES_PCI_NAME_TO_DEVICE(x)	((ushort)(x))

/*
 * Interface routines, server (bootconf) side:
 *
 *	char *load_befext (char *file)
 *
 *	    Loads the indicated driver "file" into memory, validates its
 *	    magic number, and returns the driver's load point.  Returns
 *	    NULL (& prints an error msg) if the driver fails to load.
 *
 *	int call_befext (int entry, struct bef_interface *callback)
 *
 *	    Call the driver at the indicated "entry" point, passing the
 *	    address of the "callback" structure in ES:DI.  The "entry"
 *	    code must be PROBEINSTAL, LEGACYPROBE, or INSTALLONLY as
 *	    defined above.  Returns the value of AX deliverd by the driver
 *	    when it exits.
 *
 *	void free_befext (void)
 *
 *	    Unload the current driver.
 *
 *	char *path_befext (char *filename)
 *
 *	    Given the "filename" of a realmode driver, this routine will
 *	    search all of the driver directories for that file and return
 *	    its full path name.
 */

extern int CallBef_befext(int, struct bef_interface *);
extern char *GetBefPath_befext(char far *);
extern char *LoadBef_befext(char *);
extern void FreeBef_befext(void);
extern void save_biosprim_buf_befext();
extern void free_biosprim_buf_befext();
extern int mem_adjust(unsigned short base, unsigned short len);
#endif	/* _BEFEXT_H */
