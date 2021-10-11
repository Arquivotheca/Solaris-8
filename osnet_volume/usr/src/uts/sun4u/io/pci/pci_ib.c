/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_ib.c	1.13	99/11/15 SMI"

/*
 * PCI Interrupt Block (RISCx) implementation
 *	initialization
 *	interrupt enable/disable/clear and mapping register manipulation
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/async.h>
#include <sys/systm.h>		/* panicstr */
#include <sys/spl.h>
#include <sys/sunddi.h>
#include <sys/ivintr.h>
#include <sys/ddi_impldefs.h>
#include <sys/pci/pci_obj.h>
#include <sys/intreg.h>		/* UPAID_TO_IGN() */

#ifdef _STARFIRE
#include <sys/starfire.h>

void pc_ittrans_init(int, caddr_t *);
void pc_ittrans_uninit(caddr_t);
int pc_translate_tgtid(caddr_t, int, volatile uint64_t *);
void pc_ittrans_cleanup(caddr_t, volatile uint64_t *);
#endif /* _STARFIRE */

/*LINTLIBRARY*/

void
ib_create(pci_t *pci_p)
{
#ifdef DEBUG
	dev_info_t dip = pci_p->pci_dip;
#endif
	ib_t *ib_p;
	uintptr_t a;
	int i;

	/*
	 * Allocate interrupt block state structure and link it to
	 * the pci state structure.
	 */
	ib_p = (ib_t *)kmem_zalloc(sizeof (ib_t), KM_SLEEP);
	pci_p->pci_ib_p = ib_p;
	ib_p->ib_pci_p = pci_p;

	ib_p->ib_ign = (ib_ign_t)(UPAID_TO_IGN(pci_p->pci_id) << 6);
	a = pci_ib_setup(ib_p);

	/*
	 * Determine virtual addressess of interrupt mapping, clear and diag
	 * registers that have common offsets.
	 */
	ib_p->ib_slot_clear_intr_regs =
		a + COMMON_IB_SLOT_CLEAR_INTR_REG_OFFSET;
	ib_p->ib_upa_exp_intr_map_reg[0] =
		(uint64_t *)(a + COMMON_IB_UPA0_INTR_MAP_REG_OFFSET);
	ib_p->ib_upa_exp_intr_map_reg[1] =
		(uint64_t *)(a + COMMON_IB_UPA1_INTR_MAP_REG_OFFSET);
	ib_p->ib_intr_retry_timer_reg =
		(uint64_t *)(a + COMMON_IB_INTR_RETRY_TIMER_OFFSET);
	ib_p->ib_slot_intr_state_diag_reg =
		(uint64_t *)(a + COMMON_IB_SLOT_INTR_STATE_DIAG_REG);
	ib_p->ib_obio_intr_state_diag_reg =
		(uint64_t *)(a + COMMON_IB_OBIO_INTR_STATE_DIAG_REG);
	DEBUG1(DBG_ATTACH, dip, "ib_create: ign=%x\n", ib_p->ib_ign);
	DEBUG2(DBG_ATTACH, dip, "ib_create: slot_imr=%x, slot_cir=%x\n",
		ib_p->ib_slot_intr_map_regs, ib_p->ib_obio_intr_map_regs);
	DEBUG2(DBG_ATTACH, dip, "ib_create: obio_imr=%x, obio_cir=%x\n",
		ib_p->ib_slot_clear_intr_regs, ib_p->ib_obio_clear_intr_regs);
	DEBUG2(DBG_ATTACH, dip, "ib_create: upa0_imr=%x, upa1_imr=%x\n",
		ib_p->ib_upa_exp_intr_map_reg[0],
		ib_p->ib_upa_exp_intr_map_reg[1]);
	DEBUG3(DBG_ATTACH, dip,
		"ib_create: retry_timer=%x, obio_diag=%x slot_diag=%x\n",
		ib_p->ib_intr_retry_timer_reg,
		ib_p->ib_obio_intr_state_diag_reg,
		ib_p->ib_slot_intr_state_diag_reg);

	ib_p->ib_ino_lst = (ib_ino_info_t *)NULL;
	mutex_init(&ib_p->ib_ino_lst_mutex, NULL, MUTEX_DRIVER, NULL);

	DEBUG1(DBG_ATTACH, dip, "ib_create: numproxy=%x\n",
		pci_p->pci_numproxy);
	for (i = 1; i <= pci_p->pci_numproxy; i++) {
		set_intr_mapping_reg(pci_p->pci_id,
			ib_p->ib_upa_exp_intr_map_reg[i - 1], i);
	}

#ifdef _STARFIRE
	/* Setup Starfire interrupt target translation */
	pc_ittrans_init(pci_p->pci_id, &ib_p->ib_ittrans_cookie);
#endif _STARFIRE

	ib_configure(ib_p);
}

void
ib_destroy(pci_t *pci_p)
{
	ib_t *ib_p = pci_p->pci_ib_p;

	DEBUG0(DBG_IB, pci_p->pci_dip, "ib_destroy\n");
	mutex_destroy(&ib_p->ib_ino_lst_mutex);

	ib_free_ino_all(ib_p);

#ifdef _STARFIRE
	pc_ittrans_uninit(ib_p->ib_ittrans_cookie);
#endif /* _STARFIRE */

	kmem_free(ib_p, sizeof (ib_t));
	pci_p->pci_ib_p = NULL;
}

/* The nexus interrupt priority values */
int pci_pil[] = {14, 14, 14, 14, 14, 14};
void
ib_configure(ib_t *ib_p)
{
	dev_info_t *dip = ib_p->ib_pci_p->pci_dip;

	/* XXX could be different between psycho and schizo */
	*ib_p->ib_intr_retry_timer_reg = pci_intr_retry_intv;

	/*
	 * create the interrupt-priorities property if it doesn't
	 * already exist to provide a hint as to the PIL level for
	 * our interrupt.
	 */ {
		int len;

		if (ddi_getproplen(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, "interrupt-priorities",
		    &len) != DDI_PROP_SUCCESS) {
				/* Create the interrupt-priorities property. */
			(void) ddi_prop_create(DDI_DEV_T_NONE, dip,
			    DDI_PROP_CANSLEEP, "interrupt-priorities",
			    (caddr_t)pci_pil, sizeof (pci_pil));
		}
	}
}

/* can only used for psycho internal interrupts thermal, power, ue, ce, pbm */
void
ib_intr_enable(ib_t *ib_p, dev_info_t *dip, ib_ino_t ino)
{
	ib_mondo_t mondo = IB_MAKE_MONDO(ib_p, ino);
	uint64_t *map_reg = ib_intr_map_reg_addr(ib_p, ino);
	uint_t cpu_id;

	/*
	 * Determine the cpu for the interrupt.
	 */
	mutex_enter(&intr_dist_lock);
	cpu_id = intr_add_cpu(ib_intr_dist, ib_p, mondo, 0);
	mutex_exit(&intr_dist_lock);
	DEBUG2(DBG_IB, dip, "ib_intr_enable: ino=%x cpu_id=%x\n", ino, cpu_id);

#ifdef _STARFIRE
	cpu_id = pc_translate_tgtid(ib_p->ib_ittrans_cookie, cpu_id, map_reg);
#endif /* _STARFIRE */

	*map_reg = ib_get_map_reg(mondo, cpu_id);
}

/*
 * Disable the interrupt via it's interrupt mapping register.
 * Can only used for internal interrupts: thermal, power, ue, ce, pbm.
 * If called under interrupt context, wait should be set to 0
 */
void
ib_intr_disable(ib_t *ib_p, ib_ino_t ino, int wait)
{
	ib_mondo_t mondo = IB_MAKE_MONDO(ib_p, ino);
	uint64_t *map_reg = ib_intr_map_reg_addr(ib_p, ino);
	uint64_t junk;

	/* disable the interrupt */
	IB_INO_INTR_OFF(map_reg);
	junk = *map_reg;

	if (wait) {
		volatile uint64_t *state_reg = IB_INO_INTR_STATE_REG(ib_p, ino);
		/* busy wait if there is interrupt being processed */
		while (IB_INO_INTR_PENDING(state_reg, ino) && !panicstr)
			;
	}

	/*
	 * Update the interrupt to cpu mapping tables.
	 */
	mutex_enter(&intr_dist_lock);
	intr_rem_cpu(mondo);
#ifdef _STARFIRE
	pc_ittrans_cleanup(ib_p->ib_ittrans_cookie, map_reg);
#endif /* _STARFIRE */
	mutex_exit(&intr_dist_lock);
#ifdef	lint
	junk = junk;
#endif	/* lint */
}

/* can only used for psycho internal interrupts thermal, power, ue, ce, pbm */
void
ib_intr_clear(ib_t *ib_p, ib_ino_t ino)
{
	uint64_t *clr_reg = ib_clear_intr_reg_addr(ib_p, ino);
	IB_INO_INTR_CLEAR(clr_reg);
}

void
ib_intr_dist(void *arg, int mondo, uint_t cpu_id)
{
	ib_t *ib_p = (ib_t *)arg;
	ib_ino_t ino;
	volatile uint64_t junk, *map_reg, *state_reg;

	ASSERT(MUTEX_HELD(&intr_dist_lock));

	/*
	 * Get the INO and soft state for mondo.
	 */
	ino = IB_MONDO_TO_INO(mondo);
	map_reg = ib_intr_map_reg_addr(ib_p, ino);
	state_reg = IB_INO_INTR_STATE_REG(ib_p, ino);

	mutex_enter(&ib_p->ib_ino_lst_mutex);

	/* safety precaution. Verify ino registration for slot devices */
	if (!IB_IS_OBIO_INO(ino)) {
		ib_ino_info_t *ino_p = ib_locate_ino(ib_p, ino);
		if (!ino_p)
			cmn_err(CE_PANIC, "ib_intr_dist: bad ino %x\n", ino);
	}

#ifdef _STARFIRE
	/*
	 * For Starfire it is a pain to check the current target for
	 * the mondo since we have to read the PC asics ITTR slot
	 * assigned to this mondo. It will be much easier to assume
	 * the current target is always different and do the target
	 * reprogram all the time.
	 */
#else
	if (ib_map_reg_get_cpu(map_reg) == cpu_id) {
		/*
		 * target cpu is the same, don't reprogram.
		 */
		mutex_exit(&ib_p->ib_ino_lst_mutex);
		return;
	}
#endif /* _STARFIRE */

	/* disable interrupt, this could disrupt devices sharing our slot */
	IB_INO_INTR_OFF(map_reg);
	junk = *map_reg;

	/* busy wait if there is interrupt being processed */
	while (IB_INO_INTR_PENDING(state_reg, ino) && !panicstr)
		;

#ifdef _STARFIRE
	cpu_id = pc_translate_tgtid(ib_p->ib_ittrans_cookie, cpu_id, map_reg);
#endif /* _STARFIRE */

	*map_reg = ib_get_map_reg(mondo, cpu_id);
	junk = *map_reg;
	mutex_exit(&ib_p->ib_ino_lst_mutex);

#ifdef	lint
	junk = junk;
#endif	/* lint */
}

void
ib_save_intr_map_regs(ib_t *ib_p)
{
	ib_ino_info_t *ip;
	mutex_enter(&ib_p->ib_ino_lst_mutex);
	for (ip = ib_p->ib_ino_lst; ip; ip = ip->ino_next)
		ip->ino_map_reg_save = *ip->ino_map_reg;
	mutex_exit(&ib_p->ib_ino_lst_mutex);

	ib_p->ib_upa_exp_intr_map_reg_state[0] =
		*ib_p->ib_upa_exp_intr_map_reg[0];
	ib_p->ib_upa_exp_intr_map_reg_state[1] =
		*ib_p->ib_upa_exp_intr_map_reg[1];
}

void
ib_restore_intr_map_regs(ib_t *ib_p)
{
	ib_ino_info_t *ip;
	mutex_enter(&ib_p->ib_ino_lst_mutex);
	for (ip = ib_p->ib_ino_lst; ip; ip = ip->ino_next)
		*ip->ino_map_reg = ip->ino_map_reg_save;
	mutex_exit(&ib_p->ib_ino_lst_mutex);

	*ib_p->ib_upa_exp_intr_map_reg[0] =
		ib_p->ib_upa_exp_intr_map_reg_state[0];
	*ib_p->ib_upa_exp_intr_map_reg[1] =
		ib_p->ib_upa_exp_intr_map_reg_state[1];
}

/*
 * locate ino_info structure on ib_p->ib_ino_lst according to ino#
 * returns NULL if not found.
 */
ib_ino_info_t *
ib_locate_ino(ib_t *ib_p, ib_ino_t ino_num)
{
	ib_ino_info_t *ino_p = ib_p->ib_ino_lst;
	ASSERT(MUTEX_HELD(&ib_p->ib_ino_lst_mutex));

	for (; ino_p && ino_p->ino_ino != ino_num; ino_p = ino_p->ino_next);
	return (ino_p);
}

#define	IB_INO_TO_SLOT(ino) (IB_IS_OBIO_INO(ino) ? 0xff : ((ino) & 0x1f) >> 2)

ib_ino_info_t *
ib_new_ino(ib_t *ib_p, ib_ino_t ino_num, ih_t *ih_p)
{
	ib_ino_info_t *ino_p = kmem_alloc(sizeof (ib_ino_info_t), KM_SLEEP);
	ino_p->ino_ino = ino_num;
	ino_p->ino_slot_no = IB_INO_TO_SLOT(ino_num);
	ino_p->ino_ib_p = ib_p;
	ino_p->ino_clr_reg = ib_clear_intr_reg_addr(ib_p, ino_num);
	ino_p->ino_map_reg = ib_intr_map_reg_addr(ib_p, ino_num);

	/*
	 * cannot disable interrupt since we might share slot
	 * IB_INO_INTR_OFF(ino_p->ino_map_reg);
	 */

	ih_p->ih_next = ih_p;
	ino_p->ino_ih_head = ih_p;
	ino_p->ino_ih_tail = ih_p;
	ino_p->ino_ih_start = ih_p;
	ino_p->ino_ih_size = 1;

	ino_p->ino_next = ib_p->ib_ino_lst;
	ib_p->ib_ino_lst = ino_p;
	return (ino_p);
}

/* the ino_p is retrieved by previous call to ib_locate_ino() */
void
ib_delete_ino(ib_t *ib_p, ib_ino_info_t *ino_p)
{
	ib_ino_info_t *list = ib_p->ib_ino_lst;
	if (list == ino_p)
		ib_p->ib_ino_lst = list->ino_next;
	else {
		for (; list->ino_next != ino_p; list = list->ino_next);
		list->ino_next = ino_p->ino_next;
	}
}

/* free all ino when we are detaching */
void
ib_free_ino_all(ib_t *ib_p)
{
	ib_ino_info_t *tmp = ib_p->ib_ino_lst;
	ib_ino_info_t *next = NULL;
	while (tmp) {
		next = tmp->ino_next;
		kmem_free(tmp, sizeof (ib_ino_info_t));
		tmp = next;
	}
}

void
ib_ino_add_intr(ib_ino_info_t *ino_p, ih_t *ih_p)
{
	ib_ino_t ino = ino_p->ino_ino;
	uint64_t junk;
	volatile uint64_t *state_reg =
		IB_INO_INTR_STATE_REG(ino_p->ino_ib_p, ino);

	/* disable interrupt, this could disrupt devices sharing our slot */
	IB_INO_INTR_OFF(ino_p->ino_map_reg);
	junk = *ino_p->ino_map_reg;

	/* do NOT modify the link list until after the busy wait */

	/* busy wait if there is interrupt being processed */
	while (IB_INO_INTR_PENDING(state_reg, ino) && !panicstr)
		;

	/* link up pci_ispec_t portion of the ppd */
	ih_p->ih_next = ino_p->ino_ih_head;
	ino_p->ino_ih_tail->ih_next = ih_p;
	ino_p->ino_ih_tail = ih_p;

	ino_p->ino_ih_start = ino_p->ino_ih_head;
	ino_p->ino_ih_size++;

	/* re-enable interrupt */
	IB_INO_INTR_ON(ino_p->ino_map_reg);
	junk = *ino_p->ino_map_reg;
#ifdef	lint
	junk = junk;
#endif	/* lint */
}

/*
 * removes pci_ispec_t from the ino's link list.
 * uses hardware mutex to lock out interrupt threads.
 * Side effects: interrupt belongs to that ino is turned off on return.
 * if we are sharing PCI slot with other inos, the caller needs
 * to turn it back on.
 */
void
ib_ino_rem_intr(ib_ino_info_t *ino_p, ih_t *ih_p)
{
	int i;
	uint64_t junk;
	ib_ino_t ino = ino_p->ino_ino;
	ih_t *ih_lst = ino_p->ino_ih_head;
	volatile uint64_t *state_reg =
		IB_INO_INTR_STATE_REG(ino_p->ino_ib_p, ino);

	/* disable interrupt, this could disrupt devices sharing our slot */
	IB_INO_INTR_OFF(ino_p->ino_map_reg);
	junk = *ino_p->ino_map_reg;

	/* do NOT modify the link list until after the busy wait */

	/* busy wait if there is interrupt being processed */
	while (IB_INO_INTR_PENDING(state_reg, ino) && !panicstr)
		;

	if (ino_p->ino_ih_size == 1) {
		if (ih_lst != ih_p)
			goto not_found;
		/* no need to set head/tail as ino_p will be freed */
		goto reset;
	}

	/* search the link list for ih_p */
	for (i = 0;
		(i < ino_p->ino_ih_size) && (ih_lst->ih_next != ih_p);
		i++, ih_lst = ih_lst->ih_next);
	if (ih_lst->ih_next != ih_p)
		goto not_found;

	/* remove ih_p from the link list and maintain the head/tail */
	ih_lst->ih_next = ih_p->ih_next;
	if (ino_p->ino_ih_head == ih_p)
		ino_p->ino_ih_head = ih_p->ih_next;
	if (ino_p->ino_ih_tail == ih_p)
		ino_p->ino_ih_tail = ih_lst;
	ino_p->ino_ih_start = ino_p->ino_ih_head;
reset:
	if (ih_p->ih_config_handle)
		pci_config_teardown(&ih_p->ih_config_handle);
	kmem_free(ih_p, sizeof (ih_t));
	ino_p->ino_ih_size--;

	return;
not_found:
	DEBUG2(DBG_R_ISPEC, ino_p->ino_ib_p->ib_pci_p->pci_dip,
		"ino_p=%x does not have ih_p=%x\n", ino_p, ih_p);
#ifdef	lint
	junk = junk;
#endif	/* lint */
}

ih_t *
ib_ino_locate_intr(ib_ino_info_t *ino_p, dev_info_t *rdip, uint32_t inum)
{
	ih_t *ih_lst = ino_p->ino_ih_head;
	int i;
	for (i = 0; i < ino_p->ino_ih_size; i++, ih_lst = ih_lst->ih_next) {
		if (ih_lst->ih_dip == rdip &&
		    ih_lst->ih_inum == inum)
			return (ih_lst);
	}
	return ((ih_t *)NULL);
}

ih_t *
ib_alloc_ih(dev_info_t *rdip, uint32_t inum,
    uint_t (*int_handler)(caddr_t int_handler_arg), caddr_t int_handler_arg)
{
	ih_t *ih_p;

	ih_p = kmem_alloc(sizeof (ih_t), KM_SLEEP);
	ih_p->ih_dip = rdip;
	ih_p->ih_inum = inum;
	ih_p->ih_handler = int_handler;
	ih_p->ih_handler_arg = int_handler_arg;
	ih_p->ih_config_handle = NULL;

	return (ih_p);
}
