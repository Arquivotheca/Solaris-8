/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_IB_H
#define	_SYS_PCI_IB_H

#pragma ident	"@(#)pci_ib.h	1.14	99/11/15 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/ddi_subrdefs.h>

typedef uint16_t ib_ign_t;
typedef uint8_t ib_ino_t;
typedef uint16_t ib_mondo_t;
typedef struct ib_ino_info ib_ino_info_t;
typedef uint8_t device_num_t;
typedef uint8_t interrupt_t;

/*
 * interrupt block soft state structure:
 *
 * Each pci node may share an interrupt block structure with its peer
 * node of have its own private interrupt block structure.
 */
typedef struct ib ib_t;
struct ib {

	pci_t *ib_pci_p;	/* link back to pci soft state */

	/*
	 * PCI slot and onboard I/O interrupt mapping register blocks addresses:
	 */
	uintptr_t ib_slot_intr_map_regs;
#define	ib_intr_map_regs	ib_slot_intr_map_regs
	uintptr_t ib_obio_intr_map_regs;

	/*
	 * PCI slot and onboard I/O clear interrupt register block addresses:
	 */
	uintptr_t ib_slot_clear_intr_regs;
	uintptr_t ib_obio_clear_intr_regs;

	/*
	 * UPA expansion slot interrupt mapping register addresses:
	 */
	uint64_t *ib_upa_exp_intr_map_reg[2];
	uint64_t ib_upa_exp_intr_map_reg_state[2];

	/*
	 * Interrupt retry register address:
	 */
	volatile uint64_t *ib_intr_retry_timer_reg;

	/*
	 * PCI slot and onboard I/O interrupt state diag register addresses:
	 */
	volatile uint64_t *ib_slot_intr_state_diag_reg;
	volatile uint64_t *ib_obio_intr_state_diag_reg;

	ib_ign_t ib_ign;			/* interrupt group number */
	uint_t ib_max_ino;			/* largest supported INO */
	ib_ino_info_t *ib_ino_lst;		/* ino link list */
	kmutex_t ib_ino_lst_mutex;		/* mutex to protect ino link */
						/* list */
	uint16_t ib_map_reg_counters[8];	/* counters for shared map */
						/* registers */

#ifdef _STARFIRE
	caddr_t ib_ittrans_cookie;	/* intr tgt translation */
#endif /* _STARFIRE */
};

#define	PCI_PULSE_INO	0x80000000
#define	PSYCHO_MAX_INO	0x3f
#define	SCHIZO_MAX_INO	0x37

/*
 * The following structure represents an interrupt entry for an INO.
 */
typedef struct ih {
	dev_info_t *ih_dip;		/* devinfo structure */
	uint32_t ih_inum;		/* interrupt number for this device */
	uint_t (*ih_handler)();		/* interrupt handler */
	caddr_t ih_handler_arg;		/* interrupt handler argument */
	ddi_acc_handle_t ih_config_handle; /* config space reg map handle */
	struct ih *ih_next;		/* next entry in list */
} ih_t;


/*
 * ino structure : one per each psycho slot ino with interrupt registered
 */
struct ib_ino_info {
	ib_ino_t ino_ino;		/* INO number - 8 bit */
	uint8_t ino_slot_no;		/* PCI slot number 0-8 */
	uint16_t ino_ih_size;		/* size of the pci intrspec list */
	struct ib_ino_info *ino_next;
	ih_t *ino_ih_head;		/* intr spec (part of ppd) list head */
	ih_t *ino_ih_tail;		/* intr spec (part of ppd) list tail */
	ih_t *ino_ih_start;		/* starting point in intr spec list  */
	ib_t *ino_ib_p;			/* link back to interrupt block state */
	uint64_t *ino_clr_reg;		/* ino interrupt clear register */
	uint64_t *ino_map_reg;		/* ino interrupt mapping register */
	uint64_t ino_map_reg_save;	/* = *ino_map_reg if saved */
	uint32_t pil;			/* PIL for this ino */
};

#define	IB_INTR_WAIT	1		/* wait for interrupt completion */
#define	IB_INTR_NOWAIT	0		/* already handling intr, no wait */

#define	IB_MONDO_TO_INO(mondo)		((ib_ino_t)((mondo) & 0x3f))
#define	IB_MAKE_MONDO(ib_p, ino)	((ib_mondo_t)((ib_p)->ib_ign | (ino)))
#define	IB_INO_INTR_ON(reg)	*(reg) |= COMMON_INTR_MAP_REG_VALID
#define	IB_INO_INTR_OFF(reg)	*(reg) &= ~COMMON_INTR_MAP_REG_VALID
#define	IB_INO_INTR_RESET(reg)	*(reg) = 0ull
#define	IB_INO_INTR_STATE_REG(ib_p, ino) ((ino) & 0x20 ? \
	ib_p->ib_obio_intr_state_diag_reg : ib_p->ib_slot_intr_state_diag_reg)
#define	IB_INO_INTR_PENDING(reg, ino) \
	(((*(reg) >> (((ino) & 0x1f) << 1)) & COMMON_CLEAR_INTR_REG_MASK) == \
	COMMON_CLEAR_INTR_REG_PENDING)
#define	IB_INO_INTR_CLEAR(reg)	*(reg) = COMMON_CLEAR_INTR_REG_IDLE

#define	IB_IS_OBIO_INO(ino) (ino & 0x20)

extern void ib_create(pci_t *pci_p);
extern void ib_destroy(pci_t *pci_p);
extern void ib_configure(ib_t *ib_p);
extern uint64_t ib_get_map_reg(ib_mondo_t mondo, uint32_t cpu_id);
extern uint32_t ib_map_reg_get_cpu(volatile uint64_t *reg);
extern void ib_intr_enable(ib_t *ib_p, dev_info_t *dip, ib_ino_t ino);
extern void ib_intr_disable(ib_t *ib_p, ib_ino_t ino, int wait);
extern void ib_intr_clear(ib_t *ib_p, ib_ino_t ino);
extern void ib_intr_dist(void *arg, int mondo, uint_t cpu_id);
extern void ib_save_intr_map_regs(ib_t *ib_p);
extern void ib_restore_intr_map_regs(ib_t *ib_p);

extern ib_ino_info_t *ib_locate_ino(ib_t *ib_p, ib_ino_t ino_num);
extern ib_ino_info_t *ib_new_ino(ib_t *ib_p, ib_ino_t ino_num, ih_t *ih_p);
extern void ib_delete_ino(ib_t *ib_p, ib_ino_info_t *ino_p);
extern void ib_free_ino_all(ib_t *ib_p);
extern void ib_ino_add_intr(ib_ino_info_t *ino_p, ih_t *ih_p);
extern void ib_ino_rem_intr(ib_ino_info_t *ino_p, ih_t *ih_p);
extern ih_t *ib_ino_locate_intr(ib_ino_info_t *ino_p, dev_info_t *dip,
    uint32_t inum);
extern ih_t *ib_alloc_ih(dev_info_t *dip, uint32_t inum,
    uint_t (*int_handler)(caddr_t int_handler_arg), caddr_t int_handler_arg);
extern void ib_free_ih(ih_t *ih_p);
extern void ib_ino_map_reg_share(ib_t *ib_p, ib_ino_t ino,
	ib_ino_info_t *ino_p);
extern int ib_ino_map_reg_still_shared(ib_t *ib_p, ib_ino_t ino,
	ib_ino_info_t *ino_p);
extern uint32_t ib_register_intr(ib_t *ib_p, ib_mondo_t mondo, uint_t pil,
	uint_t (*handler)(caddr_t arg), caddr_t arg);
extern void ib_unregister_intr(ib_mondo_t mondo);

extern int pci_pil[];

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_IB_H */
