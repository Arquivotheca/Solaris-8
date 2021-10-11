/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved
 */

#ifndef	_AUDIO_IMPL_H
#define	_AUDIO_IMPL_H

#pragma ident	"@(#)audio_impl.h	1.3	99/10/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/devops.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/dditypes.h>
#include <sys/audio.h>

#define	AUDIO_MINOR_AUDIO		0	/* /dev/audio */
#define	AUDIO_MINOR_AUDIOCTL		1	/* /dev/audioctl */
#define	AUDIO_MINOR_WAVE_TABLE		2	/* reserved for future */
#define	AUDIO_MINOR_MIDI_PORT		3	/* reserved for future */
#define	AUDIO_MINOR_TIME		4	/* reserved for future */
#define	AUDIO_MINOR_USER1		7	/* reserved for future */
#define	AUDIO_MINOR_USER2		8	/* reserved for future */
#define	AUDIO_MINOR_USER3		9	/* reserved for future */

#define	AUDIO_MAX(a, b)			((a) > (b) ? (a) : (b))
#define	AUDIO_MAX4(w, x, y, z)		AUDIO_MAX(AUDIO_MAX(w, x), \
						AUDIO_MAX(y, z))

#define	AUDIO_PRECISION_SHIFT		3

#define	AUDIO_NO_DEV			0
#define	AUDIO_NO_CHANNEL		(-1)

#define	AUDIO_TOGGLE(X)			(X) ^= 1

/* USE ANYTHING BELOW HERE AT YOUR OWN RISK!!! */
#define	AUDIO_NUM_DEVS		10	/* /dev/audio, /dev/audioctl, etc. */
#define	AUDIO_MINOR_PER_INST	128	/* max # of channels to each instance */
#define	AUDIO_MIN_CLONE_CHS	1	/* minimum number of clone channels */
#define	AUDIO_CLONE_CHANLIM	(AUDIO_MINOR_PER_INST - AUDIO_NUM_DEVS)
					/* max # of clones to each instance */

/* audio support ioctl/iocdata commands */
#define	COPY_OUT_CH_NUMBER	(AIOC|1)	/* AUDIO_GET_CH_NUMBER */
#define	COPY_OUT_CH_TYPE	(AIOC|2)	/* AUDIO_GET_CH_TYPE */
#define	COPY_OUT_NUM_CHS	(AIOC|3)	/* AUDIO_GET_NUM_CHS */
#define	COPY_OUT_AD_DEV		(AIOC|4)	/* AUDIO_GET_AD_DEV */
#define	COPY_OUT_APM_DEV	(AIOC|5)	/* AUDIO_GET_APM_DEV */
#define	COPY_OUT_AS_DEV		(AIOC|6)	/* AUDIO_GET_AS_DEV */

/*
 * audio_msg_t		- structure used to store STREAMS messages
 */
struct audio_msg {
	mblk_t			*msg_orig;	/* the original message */
	void			*msg_optr;	/* ptr to data in orig. msg */
	void			*msg_proc;	/* the processed message */
	void			*msg_pptr;	/* ptr to data in proc. msg */
	void			*msg_eptr;	/* ptr to end of data in buf */
	size_t			msg_psize;	/* size of the processed msg */
	struct audio_msg	*msg_next;	/* pointer to the next msg */
};
typedef struct audio_msg audio_msg_t;

/*
 * audio_apm_info_t	- audio personality module state information
 */
struct audio_apm_info {
	kmutex_t		*apm_swlock;	/* APM structure state lock */
	kcondvar_t		apm_cv;		/* condition variable */
	int			(*apm_open)(queue_t *, dev_t *,
						int, int, cred_t *);
						/* APM open() routine */
	int			(*apm_close)(queue_t *, int, cred_t *);
						/* APM close() routine */
	audio_device_t		*apm_info;	/* audio_device_t structure */
	audio_device_type_e	apm_type;	/* the device type */
	void			*apm_private;	/* private APM data */
	void			*apm_ad_infop;	/* device capabilities */
	void			*apm_ad_state;	/* state of the device */
	int			*apm_max_chs;	/* max total chs for APM */
	int			*apm_max_in_chs; /* max in chs for APM */
	int			*apm_max_out_chs; /* max out chs for APM */
	int			*apm_chs;	/* total # of chs alloced */
	int			*apm_in_chs;	/* # input channels alloced */
	int			*apm_out_chs;	/* # output channels alloced */
	struct audio_apm_info	*apm_next;	/* pointer to the next struct */
	struct audio_apm_info	*apm_previous;	/* pointer to the prev struct */
};
typedef struct audio_apm_info audio_apm_info_t;

/*
 * audio_ch_t		- per channel state and operation data
 */
struct audio_ch {
	queue_t			*ch_qptr;	/* channel queue pointer */
	struct audio_state	*ch_statep;	/* channel instance state ptr */
	kmutex_t		ch_lock;	/* channel lock */
	kcondvar_t		ch_cv;		/* available for use by ch */
	audio_apm_info_t	*ch_apm_infop;	/* pointer to ch APM info */
	int			(*ch_wput)(queue_t *, mblk_t *);
						/* APM's write put rtn */
	int			(*ch_wsvc)(queue_t *);
						/* APM's write svc rtn */
	int			(*ch_rput)(queue_t *, mblk_t *);
						/* APM's read put routine */
	int			(*ch_rsvc)(queue_t *);
						/* APM's read svc routine */
	int			ch_dir;		/* I/O direction */
	uint_t			ch_flags;	/* channel state flags */
	dev_t			ch_dev;		/* channel device number */
	void			*ch_private;	/* channel private data */
	audio_channel_t		ch_info;	/* channel state info */
	audio_device_t		*ch_dev_info;	/* Audio Driver device info */
	kmutex_t		ch_msg_lock;	/* STREAMS message list lock */
	audio_msg_t		*ch_msgs;	/* list of messages */
	int			ch_msg_cnt;	/* number of queued messages */
};
typedef struct audio_ch audio_ch_t;

/* audio_ch.ch_flags defines */
#define	AUDIO_CHNL_ALLOCATED	0x0001u		/* the channel is allocated */
#define	AUDIO_CHNL_ACTIVE	0x0002u		/* the channel is active */

/*
 * audio_state_t	- per instance state and operation data
 */
struct audio_state {
	kmutex_t		as_lock;	/* instance state lock */
	kcondvar_t		as_cv;		/* cv for blocked ch alloc */
	int			as_max_chs;	/* max # of open channels */
	audio_ch_t		as_channels[AUDIO_CLONE_CHANLIM]; /* channels */
	int			as_dev_instance; /* Audio Driver dev inst. # */
	uint_t			as_ch_inuse;	/* # of channels in use */
	audio_apm_info_t	*as_apm_info_list;	/* APM info list */
};
typedef struct audio_state audio_state_t;

/*
 * Audio Support Driver Entry Point Routines
 */
int audio_sup_attach(dev_info_t *, ddi_attach_cmd_t);
int audio_sup_close(queue_t *, int, cred_t *p);
int audio_sup_detach(dev_info_t *, ddi_detach_cmd_t);
int audio_sup_open(queue_t *, dev_t *, int, int, cred_t *);
int audio_sup_rput(queue_t *, mblk_t *);
int audio_sup_rsvc(queue_t *);
int audio_sup_wput(queue_t *, mblk_t *);
int audio_sup_wsvc(queue_t *);

/*
 * Audio Support Driver Channel Routines
 */
int audio_sup_free_ch(audio_ch_t *);

/*
 * Audio Support Driver State Routines
 */
audio_state_t *audio_sup_get_state(dev_info_t *, dev_t);

/*
 * Audio Support Driver Instance Routines
 */
int audio_sup_get_dev_instance(dev_t, queue_t *);

/*
 * Audio Support Driver Minor Routines
 */
int audio_sup_ch_to_minor(audio_state_t *, int);
int audio_sup_get_max_chs(void);
int audio_sup_get_minors_per_inst(void);
int audio_sup_getminor(dev_t, audio_device_type_e);
int audio_sup_minor_to_ch(minor_t);

/*
 * Audio Support Driver Message Routines
 */
void audio_sup_flush_msgs(audio_ch_t *);
void audio_sup_free_msg(audio_msg_t *);
audio_msg_t *audio_sup_get_msg(audio_ch_t *);
int audio_sup_get_msg_cnt(audio_ch_t *);
int audio_sup_get_msg_size(audio_ch_t *);
void audio_sup_putback_msg(audio_ch_t *, audio_msg_t *);
int audio_sup_save_msg(audio_ch_t *, mblk_t *, void *, void *, size_t);

/*
 * Audio Support Driver Registration Routines
 */
audio_apm_info_t *audio_sup_register_apm(audio_state_t *,
	audio_device_type_e,
	int (*)(queue_t *, dev_t *, int, int, cred_t *),
	int (*)(queue_t *, int, cred_t *),
	kmutex_t *, void *, void *, void *, int *, int *,
	int *, int *, int *, int *, audio_device_t *);
int audio_sup_unregister_apm(audio_state_t *, audio_device_type_e);

/*
 * Audio Support Driver Miscellaneous Routines
 */
audio_device_type_e audio_sup_get_ch_type(dev_t, queue_t *);
int audio_sup_get_channel(queue_t *q);
audio_apm_info_t *audio_sup_get_apm_info(audio_state_t *, audio_device_type_e);
void *audio_sup_get_info(queue_t *);

/*
 * Trace declarations and defines.
 */

struct audio_trace_buf {
	uint_t		seq;		/* trace sequence number */
	char		*comment;	/* trace comment string */
	void		*data;		/* data to go with string */
};
typedef struct audio_trace_buf audio_trace_buf_t;

#ifdef DEBUG

#define	AUDIO_TRACE_BUFFER_SIZE		1024

extern audio_trace_buf_t audio_trace_buffer[AUDIO_TRACE_BUFFER_SIZE];
extern kmutex_t audio_tb_lock;	/* global trace buffer lock */
extern size_t audio_tb_siz;
extern int audio_tb_pos;
extern uint_t audio_tb_seq;

#define	ATRACE(M, D) {							\
	mutex_enter(&audio_tb_lock);					\
	audio_trace_buffer[audio_tb_pos].seq = audio_tb_seq++;		\
	audio_trace_buffer[audio_tb_pos].comment = (M);			\
	audio_trace_buffer[audio_tb_pos++].data = (void *)(D);		\
	if (audio_tb_pos >= audio_tb_siz)				\
		audio_tb_pos = 0;					\
	mutex_exit(&audio_tb_lock);					\
	}

#define	ATRACE_64(M, D)		ATRACE(M, D)

#define	ATRACE_32(M, D) {						\
	mutex_enter(&audio_tb_lock);					\
	audio_trace_buffer[audio_tb_pos].seq = audio_tb_seq++;		\
	audio_trace_buffer[audio_tb_pos].comment = (M);			\
	audio_trace_buffer[audio_tb_pos++].data = (void *)((D) & 0x0ffffffff);\
	if (audio_tb_pos >= audio_tb_siz)				\
		audio_tb_pos = 0;					\
	mutex_exit(&audio_tb_lock);					\
	}

#define	ATRACE_16(M, D) {						\
	mutex_enter(&audio_tb_lock);					\
	audio_trace_buffer[audio_tb_pos].seq = audio_tb_seq++;		\
	audio_trace_buffer[audio_tb_pos].comment = (M);			\
	audio_trace_buffer[audio_tb_pos++].data = (void *)((D) & 0x00000ffff);\
	if (audio_tb_pos >= audio_tb_siz)				\
		audio_tb_pos = 0;					\
	mutex_exit(&audio_tb_lock);					\
	}

#define	ATRACE_8(M, D) {						\
	mutex_enter(&audio_tb_lock);					\
	audio_trace_buffer[audio_tb_pos].seq = audio_tb_seq++;		\
	audio_trace_buffer[audio_tb_pos].comment = (M);			\
	audio_trace_buffer[audio_tb_pos++].data = (void *)((D) & 0x0000000ff);\
	if (audio_tb_pos >= audio_tb_siz)				\
		audio_tb_pos = 0;					\
	mutex_exit(&audio_tb_lock);					\
	}

#else	/* DEBUG */

#define	ATRACE(M, D)
#define	ATRACE_32(M, D)
#define	ATRACE_16(M, D)
#define	ATRACE_8(M, D)

#endif	/* DEBUG */

/*
 *	The following structures are private to the audio support driver.
 *	USET THEM AT YOUR OWN RISK.
 *
 * audio_i_state	- This structure is used to hold state information
 *			  between M_IOCTL and M_IOCDATA messages from the
 *			  STREAM head.
 */
struct audio_i_state {
	uint_t	flags;		/* flags, each APM defines this */
	long	command;	/* the M_IOCDATA command to execute next */
	caddr_t	address;	/* address to M_COPYOUT/M_COPYIN data from/to */
	caddr_t	address2;	/* address to M_COPYOUT/M_COPYIN data from/to */
};
typedef struct audio_i_state audio_i_state_t;

/*
 * audio_inst_list_t	- structure that describes the audio deivce instance
 */
struct audio_inst_info {
	dev_info_t		*dip;		/* known at attach time */
	dev_t			dev;		/* device name */
	audio_state_t		state;		/* state struct for this inst */
};
typedef struct audio_inst_info audio_inst_info_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _AUDIO_IMPL_H */
