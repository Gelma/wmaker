/*
 * Window Maker Runtime Telemetry & Performance Profiling System
 *
 * Provides ultra-lightweight, in-memory latency tracking, event counts,
 * cache hit/miss statistics, memory footprint monitoring, and periodic
 * dumps into /tmp/wmaker_runtime_logs.
 */

#ifndef _WMAKER_TELEMETRY_H
#define _WMAKER_TELEMETRY_H

#include <X11/Xlib.h>
#include <stdint.h>
#include <time.h>

#define TELEMETRY_DIR "/tmp/wmaker_runtime_logs"
#define TELEMETRY_STATS_FILE TELEMETRY_DIR "/runtime_stats.json"
#define TELEMETRY_LOG_FILE   TELEMETRY_DIR "/runtime_stats.log"
#define TELEMETRY_SPIKES_FILE TELEMETRY_DIR "/latency_spikes.log"

#define TELEMETRY_MAX_EVENTS 128

typedef struct WTelemetryStats {
	uint64_t total_events;
	uint64_t event_counts[TELEMETRY_MAX_EVENTS];
	uint64_t event_time_ns[TELEMETRY_MAX_EVENTS];
	uint64_t event_max_time_ns[TELEMETRY_MAX_EVENTS];

	/* Optimizations and bypass counters */
	uint64_t xquery_pointer_bypassed;
	uint64_t colormap_queries_bypassed;
	uint64_t key_grabs_set;
	uint64_t key_grabs_deduplicated;
	uint64_t xsync_eliminated;
	uint64_t property_xgetgeometry_bypassed;
	uint64_t motion_events_compressed;
	uint64_t mini_preview_bypassed;
	uint64_t wwindow_cache_hits;
	uint64_t wwindow_cache_misses;

	/* Event loop iterations */
	uint64_t poll_iterations;
	uint64_t idle_executions;

	/* Modal (menu/dialog) accounting: wall time spent inside nested event
	 * loops while a handler is running, plus the current dispatch nesting. */
	uint64_t modal_ns;
	int dispatch_depth;

	/* Timestamps */
	struct timespec start_time;
	struct timespec last_dump_time;
} WTelemetryStats;

extern WTelemetryStats wTelemetry;

void wTelemetryInit(void);
void wTelemetryRecordEventStart(struct timespec *tstart);
void wTelemetryRecordEventEnd(const XEvent *event, const struct timespec *tstart,
			      uint64_t modal_ns);
uint64_t wTelemetryElapsedNs(const struct timespec *tstart);
void wTelemetryDump(void);
void wTelemetryShutdown(void);

#endif /* _WMAKER_TELEMETRY_H */
