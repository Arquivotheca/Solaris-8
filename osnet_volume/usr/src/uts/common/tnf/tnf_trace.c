/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma ident	"@(#)tnf_trace.c	1.20	97/10/28 SMI"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ddi.h>		/* strchr */
#include <sys/sunddi.h>		/* strchr */
#include <sys/tnf_com.h>
#include <sys/tnf_writer.h>
#include <sys/tnf_probe.h>
#include <sys/debug.h>

#include "tnf_buf.h"
#include "tnf_types.h"
#include "tnf_trace.h"

/*
 * New derived types for probes
 */

TNF_STD_DERIVED_TAG(tnf_probe_event, tnf_tag,
		tnf_derived_properties, TNF_OPAQUE);

TNF_STD_DERIVED_TAG(tnf_time_base, tnf_int64,
		tnf_derived_properties, TNF_INT64);

TNF_STD_DERIVED_TAG(tnf_time_delta, tnf_uint32,
		tnf_derived_properties, TNF_UINT32);

TNF_STD_DERIVED_TAG(tnf_pid, tnf_int32,
		tnf_derived_properties, TNF_INT32);

TNF_STD_DERIVED_TAG(tnf_lwpid, tnf_uint32,
		tnf_derived_properties, TNF_UINT32);

TNF_STD_DERIVED_TAG(tnf_kthread_id, tnf_opaque,
		tnf_derived_properties, TNF_OPAQUE);

TNF_STD_DERIVED_TAG(tnf_cpuid, tnf_int32,
		tnf_derived_properties, TNF_INT32);

TNF_STD_DERIVED_TAG(tnf_device, tnf_uint32,
		tnf_derived_properties, TNF_UINT32);

TNF_STD_DERIVED_TAG(tnf_symbol, tnf_opaque,
		tnf_derived_properties, TNF_OPAQUE);

TNF_STD_ARRAY_TAG(tnf_symbols, tnf_symbol, TNF_ARRAY);

TNF_STD_DERIVED_TAG(tnf_sysnum, tnf_int32,
		tnf_derived_properties, TNF_INT32);

TNF_STD_DERIVED_TAG(tnf_microstate, tnf_uint8,
		tnf_derived_properties, TNF_UINT32);

TNF_STD_DERIVED_TAG(tnf_offset, tnf_int64,
		tnf_derived_properties, TNF_INT64);

TNF_STD_DERIVED_TAG(tnf_fault_type, tnf_uint8,
		tnf_derived_properties, TNF_UINT32);

TNF_STD_DERIVED_TAG(tnf_seg_access, tnf_uint8,
		tnf_derived_properties, TNF_UINT32);

TNF_STD_DERIVED_TAG(tnf_bioflags, tnf_int32,
		tnf_derived_properties, TNF_INT32);

TNF_STD_DERIVED_TAG(tnf_diskaddr, tnf_int64,
		tnf_derived_properties, TNF_INT64);

static char	*kernel_schedule_slot_names[] = {
	TNF_N_TAG,
	TNF_N_TID,
	TNF_N_LWPID,
	TNF_N_PID,
	TNF_N_TIME_BASE,
	"cpuid",		/* XXX */
	0};

static tnf_tag_data_t	**kernel_schedule_slots[] = {
	&TAG_DATA(tnf_tag),
	&TAG_DATA(tnf_kthread_id),
	&TAG_DATA(tnf_lwpid),
	&TAG_DATA(tnf_pid),
	&TAG_DATA(tnf_time_base),
	&TAG_DATA(tnf_cpuid),
	0};

TNF_STD_STRUCT_TAG(tnf_kernel_schedule,
		kernel_schedule_slots,
		kernel_schedule_slot_names,
		sizeof (tnf_schedule_prototype_t));

/*
 * Probe type record (metatag)
 */

static tnf_tag_data_t	**probe_type_slots[] = {
	&TAG_DATA(tnf_tag),
	&TAG_DATA(tnf_name),
	&TAG_DATA(tnf_properties),
	&TAG_DATA(tnf_slot_types),
	&TAG_DATA(tnf_type_size),
	&TAG_DATA(tnf_slot_names),
	&TAG_DATA(tnf_string),  	/* detail */
	0};

TNF_METATAG(tnf_probe_type, tnf_type_properties,
    probe_type_slots, tnf_struct_tag_1);

/*
 * Write a kernel schedule record
 * Can only be written in reusable data space.
 */

tnf_record_p
tnf_kernel_schedule(tnf_ops_t *ops, tnf_schedule_t *sched)
{
	tnf_tag_data_t *metatag_data;
	tnf_record_p metatag_index;
	tnf_schedule_prototype_t *buffer;
	kthread_t *t;

	t = curthread;

	/* Cannot be called when writing into tag space */
	ASSERT(ops->mode == TNF_ALLOC_REUSABLE);

	ALLOC(ops, sizeof (*buffer), buffer, sched->record_p,
	    TNF_ALLOC_REUSABLE); /* XXX see comment above */

	metatag_data = TAG_DATA(tnf_kernel_schedule);
	metatag_index = metatag_data->tag_index ?
		metatag_data->tag_index :
		metatag_data->tag_desc(ops, metatag_data);

	ASSIGN(buffer,	tag, 		metatag_index);
	ASSIGN2(buffer, tid, 		t,		kthread_id);
	ASSIGN(buffer,	lwpid, 		t->t_tid);
	ASSIGN(buffer,	pid, 		ttoproc(t)->p_pid);
	ASSIGN(buffer,	time_base, 	sched->time_base);
	ASSIGN(buffer,	cpuid, 		sched->cpuid);

	/*
	 * Remember schedule record generation number so the distance
	 * in virtual space can be calculated from an event record
	 */
	sched->record_gen = ((tnf_block_header_t *)
	    ((uintptr_t)buffer & TNF_BLOCK_MASK))->generation;
	/* Cannot have been written into tag space */
	ASSERT(sched->record_gen != TNF_TAG_GENERATION_NUM);

	return ((tnf_record_p)buffer);
}

/*
 * Array of addresses and derivatives
 */

tnf_reference_t
tnf_opaque_array_1(tnf_ops_t *ops, void **opaques,
	tnf_record_p reference, tnf_tag_data_t *tag_data)
{
	tnf_record_p 	tag_index;
	size_t		record_size;
	void		**tmp;
	uintptr_t	*ref_p;
	tnf_array_header_t 	*bufhdr;

	tag_index = tag_data->tag_index ? tag_data->tag_index :
		tag_data->tag_desc(ops, tag_data);

	if (!opaques)
		return (TNF_NULL);

	record_size = sizeof (*bufhdr);
	tmp = opaques;
	while (*tmp++)
		record_size += sizeof (tnf_opaque_t);

	ALLOC2(ops, record_size, bufhdr, ops->mode);

	ASSIGN(bufhdr, tag, 		tag_index);
	/* LINTED assignment of 64-bit integer to 32-bit integer */
	ASSIGN(bufhdr, self_size, 	record_size);

	tmp = opaques;
	/* LINTED pointer cast may result in improper alignment */
	ref_p = (uintptr_t *)((char *)bufhdr + sizeof (*bufhdr));
	while (*tmp) {
		*ref_p = tnf_opaque(ops, *tmp, (tnf_reference_t *)ref_p);
		tmp++;
		ref_p++;
	}

	return (tnf_ref32(ops, (tnf_record_p) bufhdr, reference));
}

/*
 * Tag initializer
 */

void
tnf_tag_trace_init(void)
{

	TAG_SNAP(tnf_probe_event);
	TAG_SNAP(tnf_time_base);
	TAG_SNAP(tnf_time_delta);
	TAG_SNAP(tnf_pid);
	TAG_SNAP(tnf_lwpid);

	TAG_SNAP(tnf_kthread_id);
	TAG_SNAP(tnf_cpuid);
	TAG_SNAP(tnf_device);
	TAG_SNAP(tnf_symbol);
	TAG_SNAP(tnf_symbols);
	TAG_SNAP(tnf_sysnum);
	TAG_SNAP(tnf_microstate);
	TAG_SNAP(tnf_offset);
	TAG_SNAP(tnf_fault_type);
	TAG_SNAP(tnf_seg_access);
	TAG_SNAP(tnf_bioflags);
	TAG_SNAP(tnf_diskaddr);
	TAG_SNAP(tnf_kernel_schedule);

	TAG_SNAP(tnf_probe_type);

}
