/*
 * Window Maker Runtime Telemetry & Performance Profiling System
 *
 * Captures real-time statistics, event latency metrics, bypass counters,
 * and memory utilization, dumping metrics to /tmp/wmaker_runtime_logs.
 */

#include "wconfig.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <time.h>
#include <inttypes.h>
#include <locale.h>

#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
#include <malloc.h>
#define HAVE_MALLINFO2 1
#endif

#include <WINGs/WUtil.h>
#include <WINGs/WINGs.h>

#include "WindowMaker.h"
#include "window.h"
#include "screen.h"
#include "framewin.h"
#include "menu.h"
#include "dock.h"
#include "appicon.h"

#include "telemetry.h"

WTelemetryStats wTelemetry;

static WMHandlerID telemetry_timer_id = NULL;
static FILE *spike_file = NULL;

static void telemetryEventWaitHook(unsigned long long wait_ns);
static FILE *get_spike_file(void);

/* The dump carries window titles and command lines, so it must not be
 * readable by other users: create it 0600 and tighten a file left by an
 * older build (CodeQL: file created without restricting permissions). */
static FILE *open_private(const char *path, int flags)
{
	int fd;
	FILE *f;

	/* O_NOFOLLOW: if the runtime directory was pre-created by another user,
	 * a symlink inside it must not redirect the dump (which contains window
	 * titles and command lines) onto a file of our own. */
	fd = open(path, flags | O_NOFOLLOW, 0600);
	if (fd < 0)
		return NULL;

	/* the mode above only applies when the file is created */
	(void)fchmod(fd, 0600);

	f = fdopen(fd, (flags & O_APPEND) ? "a" : "w");
	if (!f)
		close(fd);

	return f;
}

static const char *get_event_name(int type)
{
	switch (type) {
	case KeyPress: return "KeyPress";
	case KeyRelease: return "KeyRelease";
	case ButtonPress: return "ButtonPress";
	case ButtonRelease: return "ButtonRelease";
	case MotionNotify: return "MotionNotify";
	case EnterNotify: return "EnterNotify";
	case LeaveNotify: return "LeaveNotify";
	case FocusIn: return "FocusIn";
	case FocusOut: return "FocusOut";
	case KeymapNotify: return "KeymapNotify";
	case Expose: return "Expose";
	case GraphicsExpose: return "GraphicsExpose";
	case NoExpose: return "NoExpose";
	case VisibilityNotify: return "VisibilityNotify";
	case CreateNotify: return "CreateNotify";
	case DestroyNotify: return "DestroyNotify";
	case UnmapNotify: return "UnmapNotify";
	case MapNotify: return "MapNotify";
	case MapRequest: return "MapRequest";
	case ReparentNotify: return "ReparentNotify";
	case ConfigureNotify: return "ConfigureNotify";
	case ConfigureRequest: return "ConfigureRequest";
	case GravityNotify: return "GravityNotify";
	case ResizeRequest: return "ResizeRequest";
	case CirculateNotify: return "CirculateNotify";
	case CirculateRequest: return "CirculateRequest";
	case PropertyNotify: return "PropertyNotify";
	case SelectionClear: return "SelectionClear";
	case SelectionRequest: return "SelectionRequest";
	case SelectionNotify: return "SelectionNotify";
	case ColormapNotify: return "ColormapNotify";
	case ClientMessage: return "ClientMessage";
	case MappingNotify: return "MappingNotify";
	case GenericEvent: return "GenericEvent";
	default: return "ExtensionOrOther";
	}
}

static void sanitize_log_string(const char *src, char *dst, size_t dst_len)
{
	size_t i = 0;

	if (!dst || dst_len == 0)
		return;

	if (!src) {
		dst[0] = '\0';
		return;
	}

	while (src[i] != '\0' && i + 1 < dst_len) {
		unsigned char c = (unsigned char)src[i];
		if (c < 32 || c == 127 || c == '"')
			dst[i] = ' ';
		else
			dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}

static void describe_window(Window win, char *buf, size_t len)
{
	WObjDescriptor *desc = NULL;
	int s;

	if (!buf || len == 0)
		return;

	buf[0] = '\0';
	if (win == None) {
		snprintf(buf, len, "role=none");
		return;
	}

	for (s = 0; s < w_global.screen_count; s++) {
		const WScreen *scr = wScreenWithNumber(s);
		if (scr && scr->root_win == win) {
			snprintf(buf, len, "role=root screen=%d", s);
			return;
		}
	}

	if (dpy && XFindContext(dpy, win, w_global.context.client_win, (XPointer *)&desc) != XCNOENT && desc) {
		char clean_title[64];
		char clean_class[64];

		switch (desc->parent_type) {
		case WCLASS_WINDOW: {
			const WWindow *wwin = (const WWindow *)desc->parent;
			if (wwin) {
				const char *title = (wwin->frame && wwin->frame->title) ? wwin->frame->title : NULL;
				sanitize_log_string(wwin->wm_class, clean_class, sizeof(clean_class));
				sanitize_log_string(title, clean_title, sizeof(clean_title));
				snprintf(buf, len, "role=client class=\"%s\" title=\"%s\"",
				         clean_class[0] ? clean_class : "?",
				         clean_title[0] ? clean_title : "?");
			} else {
				snprintf(buf, len, "role=client");
			}
			return;
		}
		case WCLASS_FRAME: {
			const WFrameWindow *fwin = (const WFrameWindow *)desc->parent;
			if (fwin && fwin->flags.is_client_window_frame && fwin->child) {
				const WWindow *wwin = fwin->child;
				const char *title = fwin->title ? fwin->title : NULL;
				sanitize_log_string(wwin->wm_class, clean_class, sizeof(clean_class));
				sanitize_log_string(title, clean_title, sizeof(clean_title));
				snprintf(buf, len, "role=frame class=\"%s\" title=\"%s\"",
				         clean_class[0] ? clean_class : "?",
				         clean_title[0] ? clean_title : "?");
			} else if (fwin && fwin->title) {
				sanitize_log_string(fwin->title, clean_title, sizeof(clean_title));
				snprintf(buf, len, "role=frame title=\"%s\"", clean_title);
			} else {
				snprintf(buf, len, "role=frame");
			}
			return;
		}
		case WCLASS_MENU: {
			const WMenu *menu = (const WMenu *)desc->parent;
			if (menu && menu->frame && menu->frame->title) {
				sanitize_log_string(menu->frame->title, clean_title, sizeof(clean_title));
				snprintf(buf, len, "role=menu title=\"%s\"", clean_title);
			} else {
				snprintf(buf, len, "role=menu");
			}
			return;
		}
		case WCLASS_DOCK_ICON: {
			const WAppIcon *aicon = (const WAppIcon *)desc->parent;
			if (aicon && aicon->dock) {
				const char *dtype = (aicon->dock->type == WM_CLIP) ? "clip"
				                  : (aicon->dock->type == WM_DRAWER) ? "drawer" : "dock";
				const char *cmd = aicon->command ? aicon->command : (aicon->wm_instance ? aicon->wm_instance : "?");
				sanitize_log_string(cmd, clean_title, sizeof(clean_title));
				snprintf(buf, len, "role=%s app=\"%s\"", dtype, clean_title);
			} else {
				snprintf(buf, len, "role=dock_icon");
			}
			return;
		}
		case WCLASS_APPICON: {
			const WAppIcon *aicon = (const WAppIcon *)desc->parent;
			const char *cmd = (aicon && aicon->command) ? aicon->command
			                : (aicon && aicon->wm_instance ? aicon->wm_instance : "?");
			sanitize_log_string(cmd, clean_title, sizeof(clean_title));
			snprintf(buf, len, "role=appicon app=\"%s\"", clean_title);
			return;
		}
		case WCLASS_MINIWINDOW:
			snprintf(buf, len, "role=miniwindow");
			return;
		case WCLASS_DUMMYWINDOW:
			snprintf(buf, len, "role=dummy_window");
			return;
		case WCLASS_PAGER:
			snprintf(buf, len, "role=pager");
			return;
		case WCLASS_TEXT_INPUT:
			snprintf(buf, len, "role=text_input");
			return;
		default:
			snprintf(buf, len, "role=desc_%d", desc->parent_type);
			return;
		}
	}

	snprintf(buf, len, "role=unmanaged");
}

static void format_window_role(const XEvent *event, Window win, char *buf, size_t len)
{
	char main_desc[160];

	if (!buf || len == 0)
		return;

	describe_window(win, main_desc, sizeof(main_desc));

	if (event && (event->type == ButtonPress || event->type == ButtonRelease) &&
	    event->xbutton.subwindow != None && event->xbutton.subwindow != win) {
		char sub_desc[160];
		describe_window(event->xbutton.subwindow, sub_desc, sizeof(sub_desc));
		snprintf(buf, len, " %s (subwin 0x%lx %s)", main_desc,
		         (unsigned long)event->xbutton.subwindow, sub_desc);
	} else {
		snprintf(buf, len, " %s", main_desc);
	}
}

/* Human-readable context for a latency spike: which property, which key,
 * which button, which geometry.  Without it a spike line such as
 * "PropertyNotify on 0x... took 378 ms" cannot be acted upon. */
static void format_event_context(const XEvent *event, char *buf, size_t len)
{
	Display *dpy = event->xany.display;

	buf[0] = '\0';
	switch (event->type) {
	case PropertyNotify: {
		char *name = XGetAtomName(dpy, event->xproperty.atom);
		snprintf(buf, len, " atom=%s state=%s",
			 name ? name : "?",
			 event->xproperty.state == PropertyNewValue ? "new" : "deleted");
		if (name)
			XFree(name);
		break;
	}
	case KeyPress:
	case KeyRelease:
		snprintf(buf, len, " keycode=%u state=0x%x",
			 event->xkey.keycode, event->xkey.state);
		break;
	case ButtonPress:
	case ButtonRelease:
		snprintf(buf, len, " button=%u state=0x%x subwindow=0x%lx",
			 event->xbutton.button, event->xbutton.state,
			 (unsigned long)event->xbutton.subwindow);
		break;
	case MotionNotify:
		snprintf(buf, len, " x=%d y=%d x_root=%d y_root=%d state=0x%x",
			 event->xmotion.x, event->xmotion.y,
			 event->xmotion.x_root, event->xmotion.y_root,
			 event->xmotion.state);
		break;
	case MapRequest:
		snprintf(buf, len, " parent=0x%lx",
			 (unsigned long)event->xmaprequest.parent);
		break;
	case ConfigureRequest:
		snprintf(buf, len, " %dx%d+%d+%d",
			 event->xconfigurerequest.width, event->xconfigurerequest.height,
			 event->xconfigurerequest.x, event->xconfigurerequest.y);
		break;
	case EnterNotify:
	case LeaveNotify:
		snprintf(buf, len, " mode=%d detail=%d focus=%s",
			 event->xcrossing.mode, event->xcrossing.detail,
			 event->xcrossing.focus ? "yes" : "no");
		break;
	case ClientMessage: {
		char *name = XGetAtomName(dpy, event->xclient.message_type);
		snprintf(buf, len, " type=%s", name ? name : "?");
		if (name)
			XFree(name);
		break;
	}
	case Expose:
		snprintf(buf, len, " %dx%d+%d+%d count=%d",
			 event->xexpose.width, event->xexpose.height,
			 event->xexpose.x, event->xexpose.y, event->xexpose.count);
		break;
	default:
		break;
	}
}

static void get_process_memory(uint64_t *vm_size_kb, uint64_t *vm_rss_kb)
{
	FILE *f;
	unsigned long size = 0, resident = 0, share = 0;
	long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;

	if (page_size_kb <= 0)
		page_size_kb = 4;

	*vm_size_kb = 0;
	*vm_rss_kb = 0;

	f = fopen("/proc/self/statm", "r");
	if (f) {
		if (fscanf(f, "%lu %lu %lu", &size, &resident, &share) == 3) {
			*vm_size_kb = (uint64_t)size * page_size_kb;
			*vm_rss_kb = (uint64_t)resident * page_size_kb;
		}
		fclose(f);
	}
}

static void timer_callback(void *data)
{
	(void)data;
	wTelemetryDump();
}

/* Cheap, always-on resource snapshot: CPU time, page faults, context
 * switches, heap vs mmap split and the X event backlog.  All of it is a few
 * syscalls (or nothing at all) once per dump, and it is what tells us whether
 * an optimisation actually reduced cycles, faults, wakeups or memory. */
struct WmResourceUsage {
	unsigned long long utime_ms;
	unsigned long long stime_ms;
	unsigned long minflt;
	unsigned long majflt;
	unsigned long nvcsw;
	unsigned long nivcsw;
	unsigned long long arena_kb;
	unsigned long long inuse_kb;
	unsigned long long mmap_kb;
	int x_pending;
};

static void get_process_resources(struct WmResourceUsage *r)
{
	struct rusage ru;

	memset(r, 0, sizeof(*r));

	if (getrusage(RUSAGE_SELF, &ru) == 0) {
		r->utime_ms = (unsigned long long)ru.ru_utime.tv_sec * 1000ULL +
			(unsigned long long)ru.ru_utime.tv_usec / 1000ULL;
		r->stime_ms = (unsigned long long)ru.ru_stime.tv_sec * 1000ULL +
			(unsigned long long)ru.ru_stime.tv_usec / 1000ULL;
		r->minflt = ru.ru_minflt;
		r->majflt = ru.ru_majflt;
		r->nvcsw = ru.ru_nvcsw;
		r->nivcsw = ru.ru_nivcsw;
	}
#ifdef HAVE_MALLINFO2
	{
		struct mallinfo2 mi = mallinfo2();

		r->arena_kb = (unsigned long long)mi.arena / 1024ULL;
		r->inuse_kb = (unsigned long long)mi.uordblks / 1024ULL;
		r->mmap_kb = (unsigned long long)mi.hblkhd / 1024ULL;
	}
#endif
	/* XPending() flushes and reads whatever is already queued: at dump time
	 * that is exactly the backlog we want to report. */
	if (dpy)
		r->x_pending = XPending(dpy);
}

static const char *get_telemetry_dir(void)
{
	static char runtime_dir[PATH_MAX];
	const char *env = getenv("WM_RUNTIME_LOG_DIR");
	const char *runtime;

	if (env && env[0] != '\0')
		return env;

	/* The dumps carry window titles and command lines: prefer the private
	 * per-user runtime directory over the world-writable /tmp, where the
	 * predictable name could be pre-created by another local user. */
	runtime = getenv("XDG_RUNTIME_DIR");
	if (runtime && runtime[0] != '\0') {
		snprintf(runtime_dir, sizeof(runtime_dir), "%s/wmaker", runtime);
		return runtime_dir;
	}

	return TELEMETRY_DIR;
}

/* Keep the spike log open: reopening it for every slow event cost one
 * fopen/fclose pair plus a 4 KB stdio buffer each time (5,084 opens and
 * ~18.6 MB of churn in the 12.5 h DHAT session).  Lines are flushed
 * individually because helper processes fork from this one.
 *
 * The log is rotated at 4 MB (keeping one .1 generation) so a pathological
 * session cannot fill the runtime directory. */
#define TELEMETRY_SPIKES_MAX_BYTES (4ULL * 1024ULL * 1024ULL)

static unsigned long long spike_bytes;

static FILE *get_spike_file(void)
{
	if (!spike_file) {
		char path[512];
		struct stat st;

		snprintf(path, sizeof(path), "%s/latency_spikes.log", get_telemetry_dir());
		spike_file = open_private(path, O_WRONLY | O_CREAT | O_APPEND);
		if (spike_file) {
			if (fstat(fileno(spike_file), &st) == 0)
				spike_bytes = (unsigned long long)st.st_size;
			else
				spike_bytes = 0;
		}
	}
	return spike_file;
}

static void rotate_spike_file(void)
{
	char path[512];
	char old[520];

	if (!spike_file)
		return;

	snprintf(path, sizeof(path), "%s/latency_spikes.log", get_telemetry_dir());
	snprintf(old, sizeof(old), "%s.1", path);

	fclose(spike_file);
	spike_file = NULL;
	spike_bytes = 0;
	if (rename(path, old) != 0)
		/* Rotation is best effort: the log keeps growing if this fails
		 * (Coverity CID 1686093). */
		wwarning("could not rotate %s: %s", path, strerror(errno));
}

static void close_spike_file(void)
{
	if (spike_file) {
		fclose(spike_file);
		spike_file = NULL;
	}
}

void wTelemetryInit(void)
{
	memset(&wTelemetry, 0, sizeof(wTelemetry));
	clock_gettime(CLOCK_MONOTONIC, &wTelemetry.start_time);
	wTelemetry.last_dump_time = wTelemetry.start_time;

	/* Let WINGs report blocking waits so that modal (menu/dialog) time can
	 * be excluded from event handler latency. */
	WMEventWaitHook = telemetryEventWaitHook;

	/* Telemetry directory creation is best effort: if it fails the dump
	 * below is the one that reports the missing file (Coverity CID
	 * 1686099).  0700 because the dumps carry window titles and command
	 * lines; if the directory already exists and belongs to somebody else,
	 * do not write into it at all. */
	{
		const char *dir = get_telemetry_dir();
		struct stat st;

		if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
			wwarning("could not create %s: %s", dir, strerror(errno));
		} else if (lstat(dir, &st) == 0 && (!S_ISDIR(st.st_mode) || st.st_uid != getuid())) {
			wwarning("telemetry directory %s is not a directory owned by us: "
				 "disabling telemetry dumps", dir);
			return;
		}
	}

	/* Dump stats every 60 seconds (60,000 ms) */
	telemetry_timer_id = WMAddPersistentTimerHandler(60000, timer_callback, NULL);

	/* Create initial files */
	wTelemetryDump();
}

void wTelemetryRecordEventStart(struct timespec *tstart)
{
	if (!tstart)
		return;
	clock_gettime(CLOCK_MONOTONIC, tstart);
}

uint64_t wTelemetryElapsedNs(const struct timespec *tstart)
{
	struct timespec now;

	if (!tstart)
		return 0;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint64_t)(now.tv_sec - tstart->tv_sec) * 1000000000ULL +
		(uint64_t)(now.tv_nsec - tstart->tv_nsec);
}

/* Called by WINGs after every blocking wait.  A wait that happens while a
 * handler is running belongs to a modal loop (menu, dialog, move/resize):
 * it is interaction time, not handler latency. */
static void telemetryEventWaitHook(unsigned long long wait_ns)
{
	if (wTelemetry.dispatch_depth > 0)
		wTelemetry.modal_ns += wait_ns;
}

/* The window a report should name.  For MapRequest/ConfigureRequest the
 * generic xany.window overlays the parent field of the union, which would
 * print the root window instead of the client. */
static Window event_window(const XEvent *event)
{
	switch (event->type) {
	case MapRequest:
		return event->xmaprequest.window;
	case ConfigureRequest:
		return event->xconfigurerequest.window;
	case CreateNotify:
		return event->xcreatewindow.window;
	default:
		return event->xany.window;
	}
}

void wTelemetryRecordEventEnd(const XEvent *event, const struct timespec *tstart,
			      uint64_t modal_ns)
{
	uint64_t elapsed_ns;
	uint64_t total_ns;
	int event_type;
	Window win;
	char ctx[256];

	if (!event || !tstart || tstart->tv_sec == 0)
		return;

	event_type = event->type;
	win = event_window(event);

	total_ns = wTelemetryElapsedNs(tstart);
	elapsed_ns = total_ns > modal_ns ? total_ns - modal_ns : 0;

	wTelemetry.total_events++;

	if (event_type >= 0 && event_type < TELEMETRY_MAX_EVENTS) {
		wTelemetry.event_counts[event_type]++;
		wTelemetry.event_time_ns[event_type] += elapsed_ns;
		if (elapsed_ns > wTelemetry.event_max_time_ns[event_type])
			wTelemetry.event_max_time_ns[event_type] = elapsed_ns;
	}

	/* Latency spike detection: alert if an event takes longer than 16.6 ms (one 60fps frame) */
	if (elapsed_ns > 16666667ULL) {
		FILE *fspike = get_spike_file();
		if (fspike) {
			time_t now = time(NULL);
			struct tm tm_now;
			char timestr[64];
			char role[512];
			char line[1024];
			int len;

			localtime_r(&now, &tm_now);
			strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm_now);
			format_window_role(event, win, role, sizeof(role));
			format_event_context(event, ctx, sizeof(ctx));
			len = snprintf(line, sizeof(line),
			               "[%s] Spike: event %s (%d) on window 0x%lx%s took %.2f ms%s",
			               timestr, get_event_name(event_type), event_type,
			               (unsigned long)win, role, (double)elapsed_ns / 1000000.0, ctx);
			if (modal_ns > 0 && len > 0 && (size_t)len < sizeof(line))
				len += snprintf(line + len, sizeof(line) - (size_t)len,
				                " (%llu ms in modal menus/dialogs excluded)",
				                (unsigned long long)(modal_ns / 1000000ULL));
			if (len > 0) {
				if ((size_t)len >= sizeof(line))
					len = (int)sizeof(line) - 1;
				fwrite(line, 1, (size_t)len, fspike);
				fputc('\n', fspike);
				/* Keep the stream unbuffered across fork()s (dock apps and
				 * helpers inherit it): one line, one write. */
				fflush(fspike);
				spike_bytes += (unsigned long long)len + 1;
				if (spike_bytes > TELEMETRY_SPIKES_MAX_BYTES)
					rotate_spike_file();
			}
		}
	}
}

void wTelemetryDump(void)
{
	FILE *fjson, *flog;
	struct timespec now;
	double uptime_sec;
	uint64_t vm_size_kb = 0, vm_rss_kb = 0;
	struct WmResourceUsage res;
	int i, first = 1;

	clock_gettime(CLOCK_MONOTONIC, &now);
	uptime_sec = (double)(now.tv_sec - wTelemetry.start_time.tv_sec) +
	             (double)(now.tv_nsec - wTelemetry.start_time.tv_nsec) / 1e9;
	wTelemetry.last_dump_time = now;

	get_process_memory(&vm_size_kb, &vm_rss_kb);
	get_process_resources(&res);

	/* 1. Write JSON telemetry dump (enforcing standard C locale for JSON compliance) */
	char *saved_locale = setlocale(LC_NUMERIC, NULL);
	char *dup_locale = saved_locale ? wstrdup(saved_locale) : NULL;
	setlocale(LC_NUMERIC, "C");

	char path_json[512], path_log[512];
	snprintf(path_json, sizeof(path_json), "%s/runtime_stats.json", get_telemetry_dir());
	snprintf(path_log, sizeof(path_log), "%s/runtime_stats.log", get_telemetry_dir());

	fjson = open_private(path_json, O_WRONLY | O_CREAT | O_TRUNC);
	if (fjson) {
		fprintf(fjson, "{\n");
		fprintf(fjson, "  \"uptime_seconds\": %.2f,\n", uptime_sec);
		fprintf(fjson, "  \"modal_time_ms\": %.2f,\n", (double)wTelemetry.modal_ns / 1e6);
		fprintf(fjson, "  \"memory_vmsize_kb\": %" PRIu64 ",\n", vm_size_kb);
		fprintf(fjson, "  \"memory_vmrss_kb\": %" PRIu64 ",\n", vm_rss_kb);
		fprintf(fjson, "  \"total_events\": %" PRIu64 ",\n", wTelemetry.total_events);
		fprintf(fjson, "  \"bypasses\": {\n");
		fprintf(fjson, "    \"xquery_pointer_bypassed\": %" PRIu64 ",\n", wTelemetry.xquery_pointer_bypassed);
		fprintf(fjson, "    \"colormap_queries_bypassed\": %" PRIu64 ",\n", wTelemetry.colormap_queries_bypassed);
		fprintf(fjson, "    \"key_grabs_set\": %" PRIu64 ",\n", wTelemetry.key_grabs_set);
		fprintf(fjson, "    \"key_grabs_deduplicated\": %" PRIu64 ",\n", wTelemetry.key_grabs_deduplicated);
		fprintf(fjson, "    \"xsync_eliminated\": %" PRIu64 ",\n", wTelemetry.xsync_eliminated);
		fprintf(fjson, "    \"property_xgetgeometry_bypassed\": %" PRIu64 ",\n", wTelemetry.property_xgetgeometry_bypassed);
		fprintf(fjson, "    \"motion_events_compressed\": %" PRIu64 ",\n", wTelemetry.motion_events_compressed);
		fprintf(fjson, "    \"mini_preview_bypassed\": %" PRIu64 ",\n", wTelemetry.mini_preview_bypassed);
		fprintf(fjson, "    \"wwindow_cache_hits\": %" PRIu64 ",\n", wTelemetry.wwindow_cache_hits);
		fprintf(fjson, "    \"wwindow_cache_misses\": %" PRIu64 "\n", wTelemetry.wwindow_cache_misses);
		fprintf(fjson, "  },\n");
		{
			unsigned long wh = 0, wm = 0, lh = 0, lm = 0, ah = 0;

			WMGetFontCacheStats(&wh, &wm, &lh, &lm, &ah);
			fprintf(fjson, "  \"font_caches\": {\n");
			fprintf(fjson, "    \"ascii_width_hits\": %lu,\n", ah);
			fprintf(fjson, "    \"width_hits\": %lu,\n", wh);
			fprintf(fjson, "    \"width_misses\": %lu,\n", wm);
			fprintf(fjson, "    \"layout_hits\": %lu,\n", lh);
			fprintf(fjson, "    \"layout_misses\": %lu\n", lm);
			fprintf(fjson, "  },\n");
		}
		fprintf(fjson, "  \"resources\": {\n");
		fprintf(fjson, "    \"utime_ms\": %llu,\n", res.utime_ms);
		fprintf(fjson, "    \"stime_ms\": %llu,\n", res.stime_ms);
		fprintf(fjson, "    \"minor_faults\": %lu,\n", res.minflt);
		fprintf(fjson, "    \"major_faults\": %lu,\n", res.majflt);
		fprintf(fjson, "    \"voluntary_cs\": %lu,\n", res.nvcsw);
		fprintf(fjson, "    \"involuntary_cs\": %lu,\n", res.nivcsw);
		fprintf(fjson, "    \"heap_arena_kb\": %llu,\n", res.arena_kb);
		fprintf(fjson, "    \"heap_inuse_kb\": %llu,\n", res.inuse_kb);
		fprintf(fjson, "    \"malloc_mmap_kb\": %llu,\n", res.mmap_kb);
		fprintf(fjson, "    \"x_pending_events\": %d\n", res.x_pending);
		fprintf(fjson, "  },\n");
		fprintf(fjson, "  \"diagnostics\": {\n");
		fprintf(fjson, "    \"map_request_already_managed\": %" PRIu64 ",\n", wTelemetry.map_request_already_managed);
		fprintf(fjson, "    \"map_request_failed\": %" PRIu64 ",\n", wTelemetry.map_request_failed);
		fprintf(fjson, "    \"map_request_no_screen\": %" PRIu64 ",\n", wTelemetry.map_request_no_screen);
		fprintf(fjson, "    \"map_request_override_redirect\": %" PRIu64 "\n", wTelemetry.map_request_override_redirect);
		fprintf(fjson, "  },\n");
		fprintf(fjson, "  \"events\": {\n");
		for (i = 0; i < TELEMETRY_MAX_EVENTS; i++) {
			if (wTelemetry.event_counts[i] > 0) {
				double avg_ms = (double)wTelemetry.event_time_ns[i] / (double)wTelemetry.event_counts[i] / 1e6;
				double max_ms = (double)wTelemetry.event_max_time_ns[i] / 1e6;
				double total_ms = (double)wTelemetry.event_time_ns[i] / 1e6;

				char ev_name_buf[64];
				if (i <= 35)
					snprintf(ev_name_buf, sizeof(ev_name_buf), "%s", get_event_name(i));
				else
					snprintf(ev_name_buf, sizeof(ev_name_buf), "ExtensionOrOther_%d", i);

				if (!first)
					fprintf(fjson, ",\n");
				first = 0;
				fprintf(fjson, "    \"%s\": {\"type\": %d, \"count\": %" PRIu64 ", \"total_ms\": %.2f, \"avg_ms\": %.4f, \"max_ms\": %.2f}",
				        ev_name_buf, i, wTelemetry.event_counts[i], total_ms, avg_ms, max_ms);
			}
		}
		fprintf(fjson, "\n  }\n");
		fprintf(fjson, "}\n");
		fclose(fjson);
	}

	if (dup_locale) {
		setlocale(LC_NUMERIC, dup_locale);
		wfree(dup_locale);
	}

	/* 2. Write Human-Readable Log */
	flog = open_private(path_log, O_WRONLY | O_CREAT | O_TRUNC);
	if (flog) {
		time_t cal_now = time(NULL);
		struct tm tm_now;
		char timestr[64];

		localtime_r(&cal_now, &tm_now);
		strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm_now);

		fprintf(flog, "================================================================================\n");
		fprintf(flog, "Window Maker Runtime Telemetry Report (%s)\n", timestr);
		fprintf(flog, "Uptime: %.2f s | Memory VmRSS: %" PRIu64 " KB | VmSize: %" PRIu64 " KB\n",
		        uptime_sec, vm_rss_kb, vm_size_kb);
		fprintf(flog, "Total X11 Events Handled: %" PRIu64 "\n", wTelemetry.total_events);
		fprintf(flog, "Modal (menu/dialog) time excluded from latency: %.2f ms\n",
		        (double)wTelemetry.modal_ns / 1e6);
		fprintf(flog, "Resources: user %.2f ms | system %.2f ms | faults %lu minor / %lu major | cs %lu vol / %lu invol\n",
		        (double)res.utime_ms, (double)res.stime_ms,
		        res.minflt, res.majflt, res.nvcsw, res.nivcsw);
		fprintf(flog, "Heap: arena %llu KB | in use %llu KB | malloc mmap %llu KB | X events pending: %d\n",
		        res.arena_kb, res.inuse_kb, res.mmap_kb, res.x_pending);
		fprintf(flog, "MapRequest: already managed %" PRIu64 " | manage failed %" PRIu64 " | screen not found %" PRIu64 " | override-redirect (benign) %" PRIu64 "\n",
		        wTelemetry.map_request_already_managed, wTelemetry.map_request_failed,
		        wTelemetry.map_request_no_screen, wTelemetry.map_request_override_redirect);
		fprintf(flog, "--------------------------------------------------------------------------------\n");
		fprintf(flog, "Optimization & Bypass Metrics:\n");
		fprintf(flog, "  - XQueryPointer Bypassed (Single Head / Cache): %" PRIu64 "\n", wTelemetry.xquery_pointer_bypassed);
		fprintf(flog, "  - Colormap Queries / XSync Bypassed:           %" PRIu64 "\n", wTelemetry.colormap_queries_bypassed);
		fprintf(flog, "  - Key Grabs Installed / Precompiled:            %" PRIu64 "\n", wTelemetry.key_grabs_set);
		fprintf(flog, "  - Key Grabs Deduplicated:                       %" PRIu64 "\n", wTelemetry.key_grabs_deduplicated);
		fprintf(flog, "  - Event Loop XSync Round-Trips Eliminated:      %" PRIu64 "\n", wTelemetry.xsync_eliminated);
		fprintf(flog, "  - PropertyNotify XGetGeometry Bypassed:         %" PRIu64 "\n", wTelemetry.property_xgetgeometry_bypassed);
		fprintf(flog, "  - Motion Events Compressed:                     %" PRIu64 "\n", wTelemetry.motion_events_compressed);
		fprintf(flog, "  - Mini-Preview Screenshot Captures Bypassed:    %" PRIu64 "\n", wTelemetry.mini_preview_bypassed);
		uint64_t total_cache = wTelemetry.wwindow_cache_hits + wTelemetry.wwindow_cache_misses;
		double hit_rate = total_cache > 0 ? (double)wTelemetry.wwindow_cache_hits / (double)total_cache * 100.0 : 0.0;
		fprintf(flog, "  - WWindow L1 Cache: %" PRIu64 " hits, %" PRIu64 " misses (%.1f%% hit rate)\n",
		        wTelemetry.wwindow_cache_hits, wTelemetry.wwindow_cache_misses, hit_rate);
		fprintf(flog, "--------------------------------------------------------------------------------\n");
		fprintf(flog, "%-22s %10s %14s %12s %12s\n", "Event Type", "Count", "Total (ms)", "Avg (ms)", "Max (ms)");
		fprintf(flog, "--------------------------------------------------------------------------------\n");
		for (i = 0; i < TELEMETRY_MAX_EVENTS; i++) {
			if (wTelemetry.event_counts[i] > 0) {
				double avg_ms = (double)wTelemetry.event_time_ns[i] / (double)wTelemetry.event_counts[i] / 1e6;
				double max_ms = (double)wTelemetry.event_max_time_ns[i] / 1e6;
				double total_ms = (double)wTelemetry.event_time_ns[i] / 1e6;

				char ev_name_buf[64];
				if (i <= 35)
					snprintf(ev_name_buf, sizeof(ev_name_buf), "%s", get_event_name(i));
				else
					snprintf(ev_name_buf, sizeof(ev_name_buf), "ExtensionOrOther_%d", i);

				fprintf(flog, "%-22s %10" PRIu64 " %14.2f %12.4f %12.2f\n",
				        ev_name_buf, wTelemetry.event_counts[i], total_ms, avg_ms, max_ms);
			}
		}
		fprintf(flog, "================================================================================\n");
		fclose(flog);
	}
}

void wTelemetryShutdown(void)
{
	WMEventWaitHook = NULL;
	close_spike_file();
	if (telemetry_timer_id) {
		WMDeleteTimerHandler(telemetry_timer_id);
		telemetry_timer_id = NULL;
	}
	wTelemetryDump();
}
