/* @(#)rquota.x 1.4 88/02/08 Copyr 1987 Sun Micro */

/*
 * Remote quota protocol
 * Requires unix authentication
 */

const RQ_PATHLEN = 1024;

struct getquota_args {
	string gqa_pathp<RQ_PATHLEN>;  	/* path to filesystem of interest */
	int32_t gqa_uid;        	/* inquire about quota for uid */
};

/*
 * remote quota structure
 */
struct rquota {
	int32_t rq_bsize;		/* block size for block counts */
	bool rq_active;  		/* indicates whether quota is active */
	uint32_t rq_bhardlimit;		/* absolute limit on disk blks alloc */
	uint32_t rq_bsoftlimit;		/* preferred limit on disk blks */
	uint32_t rq_curblocks;		/* current block count */
	uint32_t rq_fhardlimit;		/* absolute limit on allocated files */
	uint32_t rq_fsoftlimit;		/* preferred file limit */
	uint32_t rq_curfiles;		/* current # allocated files */
	uint32_t rq_btimeleft;		/* time left for excessive disk use */
	uint32_t rq_ftimeleft;		/* time left for excessive files */
};	

enum gqr_status {
	Q_OK = 1,		/* quota returned */
	Q_NOQUOTA = 2,  	/* noquota for uid */
	Q_EPERM = 3		/* no permission to access quota */
};

union getquota_rslt switch (gqr_status status) {
case Q_OK:
	rquota gqr_rquota;	/* valid if status == Q_OK */
case Q_NOQUOTA:
	void;
case Q_EPERM:
	void;
};

program RQUOTAPROG {
	version RQUOTAVERS {
		/*
		 * Get all quotas
		 */
		getquota_rslt
		RQUOTAPROC_GETQUOTA(getquota_args) = 1;

		/*
	 	 * Get active quotas only
		 */
		getquota_rslt
		RQUOTAPROC_GETACTIVEQUOTA(getquota_args) = 2;
	} = 1;
} = 100011;
