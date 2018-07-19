
/*
 * This file defines pagecache limit specific tracepoints and should only be
 * included through include/trace/events/vmscan.h, never directly.
 */

TRACE_EVENT(mm_shrink_page_cache_start,

	TP_PROTO(gfp_t mask),

	TP_ARGS(mask),

	TP_STRUCT__entry(
		__field(gfp_t, mask)
	),

	TP_fast_assign(
		__entry->mask = mask;
	),

	TP_printk("mask=%s",
		show_gfp_flags(__entry->mask))
);

TRACE_EVENT(mm_shrink_page_cache_end,

	TP_PROTO(unsigned long nr_reclaimed),

	TP_ARGS(nr_reclaimed),

	TP_STRUCT__entry(
		__field(unsigned long, nr_reclaimed)
	),

	TP_fast_assign(
		__entry->nr_reclaimed = nr_reclaimed;
	),

	TP_printk("nr_reclaimed=%lu",
		__entry->nr_reclaimed)
);

TRACE_EVENT(mm_pagecache_reclaim_start,

	TP_PROTO(unsigned long nr_pages, int pass, int prio, gfp_t mask,
							bool may_write),

	TP_ARGS(nr_pages, pass, prio, mask, may_write),

	TP_STRUCT__entry(
		__field(unsigned long,	nr_pages	)
		__field(int,		pass		)
		__field(int,		prio		)
		__field(gfp_t,		mask		)
		__field(bool,		may_write	)
	),

	TP_fast_assign(
		__entry->nr_pages = nr_pages;
		__entry->pass = pass;
		__entry->prio = prio;
		__entry->mask = mask;
		__entry->may_write = may_write;
	),

	TP_printk("nr_pages=%lu pass=%d prio=%d mask=%s may_write=%d",
		__entry->nr_pages,
		__entry->pass,
		__entry->prio,
		show_gfp_flags(__entry->mask),
		(int) __entry->may_write)
);

TRACE_EVENT(mm_pagecache_reclaim_end,

	TP_PROTO(unsigned long nr_scanned, unsigned long nr_reclaimed,
						unsigned int nr_zones),

	TP_ARGS(nr_scanned, nr_reclaimed, nr_zones),

	TP_STRUCT__entry(
		__field(unsigned long,	nr_scanned	)
		__field(unsigned long,	nr_reclaimed	)
		__field(unsigned int,	nr_zones	)
	),

	TP_fast_assign(
		__entry->nr_scanned = nr_scanned;
		__entry->nr_reclaimed = nr_reclaimed;
		__entry->nr_zones = nr_zones;
	),

	TP_printk("nr_scanned=%lu nr_reclaimed=%lu nr_scanned_zones=%u",
		__entry->nr_scanned,
		__entry->nr_reclaimed,
		__entry->nr_zones)
);


