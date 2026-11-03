/*
 * wm_stress.c - Multithreaded High-Concurrency Stress Test for Window Maker & X11
 *
 * Spawns multiple worker threads (default: CPU core count, e.g. 8 cores) to violently
 * stress window management operations on the active X11 display:
 *  - High-frequency window creation, mapping, unmapping and destruction
 *  - Property floods (_NET_WM_NAME, WM_NAME, dynamic titles, hints)
 *  - Rapid geometry updates (XMoveResizeWindow)
 *  - Random iconification / de-iconification
 *  - Rapid transient/dialog window creation and teardown
 *  - Random background colors and animated geometric shapes (rectangles, lines, arcs)
 *  - Graceful signal handling (Ctrl+C / SIGINT / SIGTERM) with clean resource freeing
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <stdatomic.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#define DEFAULT_THREADS 8
#define DEFAULT_WINDOWS_PER_THREAD 6
#define DEFAULT_DURATION_SEC 20

static volatile sig_atomic_t g_running = 1;

typedef struct {
	int thread_id;
	const char *display_name;
	int num_windows;
	int screen_width;
	int screen_height;
	_Atomic unsigned long created_count;
	_Atomic unsigned long destroyed_count;
	_Atomic unsigned long property_count;
	_Atomic unsigned long moveresize_count;
	_Atomic unsigned long draw_count;
} WorkerArg;

static void sigint_handler(int sig)
{
	(void)sig;
	g_running = 0;
}

static inline unsigned int fast_rand(unsigned int *seed)
{
	*seed = *seed * 1103515245 + 12345;
	return (*seed / 65536) % 32768;
}

typedef struct {
	Window win;
	int x, y, w, h;
	int dx, dy;
	unsigned long color;
	GC gc;
	int is_iconic;
	int is_transient;
	Window parent;
} StressWindow;

static void *worker_thread_func(void *param)
{
	WorkerArg *arg = (WorkerArg *)param;
	Display *dpy = XOpenDisplay(arg->display_name);
	if (!dpy) {
		fprintf(stderr, "[Thread %d] Failed to open display: %s\n", arg->thread_id,
		        arg->display_name ? arg->display_name : ":0");
		return NULL;
	}

	int screen = DefaultScreen(dpy);
	Window root = RootWindow(dpy, screen);
	arg->screen_width = DisplayWidth(dpy, screen);
	arg->screen_height = DisplayHeight(dpy, screen);

	Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
	Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
	Atom wm_change_state = XInternAtom(dpy, "WM_CHANGE_STATE", False);

	unsigned int seed = (unsigned int)(time(NULL) ^ (arg->thread_id << 16) ^ getpid());

	StressWindow *windows = calloc(arg->num_windows, sizeof(StressWindow));
	if (!windows) {
		XCloseDisplay(dpy);
		return NULL;
	}

	/* Random colors palette */
	unsigned long colors[] = {
		0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0x00FFFF, 0xFF00FF,
		0xFF8000, 0x800080, 0x008080, 0xC0C0C0, 0xFF69B4, 0x1E90FF,
		0x32CD32, 0xFFD700, 0x8A2BE2, 0x20B2AA
	};
	int num_colors = sizeof(colors) / sizeof(colors[0]);

	/* Main stress loop */
	while (g_running) {
		for (int i = 0; i < arg->num_windows && g_running; i++) {
			StressWindow *sw = &windows[i];

			/* If window doesn't exist, create it */
			if (sw->win == None) {
				sw->w = 120 + (fast_rand(&seed) % 300);
				sw->h = 80 + (fast_rand(&seed) % 240);
				int max_x = arg->screen_width - sw->w - 20;
				int max_y = arg->screen_height - sw->h - 40;
				sw->x = (max_x > 0) ? (fast_rand(&seed) % max_x) + 10 : 10;
				sw->y = (max_y > 0) ? (fast_rand(&seed) % max_y) + 30 : 30;
				sw->dx = (fast_rand(&seed) % 15) - 7;
				sw->dy = (fast_rand(&seed) % 15) - 7;
				if (sw->dx == 0) sw->dx = 3;
				if (sw->dy == 0) sw->dy = 3;

				sw->color = colors[fast_rand(&seed) % num_colors];
				sw->is_iconic = 0;

				XSetWindowAttributes xswa;
				xswa.background_pixel = sw->color;
				xswa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask;

				sw->win = XCreateWindow(dpy, root, sw->x, sw->y, sw->w, sw->h,
				                        1, CopyFromParent, InputOutput,
				                        CopyFromParent, CWBackPixel | CWEventMask, &xswa);

				if (sw->win != None) {
					/* Set WM_PROTOCOLS */
					XSetWMProtocols(dpy, sw->win, &wm_delete, 1);

					/* Random title */
					char title[128];
					snprintf(title, sizeof(title), "Stress T%d-W%d (#%lu)",
					         arg->thread_id, i, arg->created_count);
					XStoreName(dpy, sw->win, title);
					XChangeProperty(dpy, sw->win, net_wm_name, utf8_string, 8,
					                PropModeReplace, (unsigned char *)title, strlen(title));

					/* Decide if transient */
					if ((fast_rand(&seed) % 5) == 0 && i > 0 && windows[0].win != None) {
						sw->is_transient = 1;
						sw->parent = windows[0].win;
						XSetTransientForHint(dpy, sw->win, sw->parent);
					} else {
						sw->is_transient = 0;
						sw->parent = None;
					}

					/* Set standard size hints */
					XSizeHints hints;
					memset(&hints, 0, sizeof(hints));
					hints.flags = PPosition | PSize | PMinSize;
					hints.x = sw->x;
					hints.y = sw->y;
					hints.width = sw->w;
					hints.height = sw->h;
					hints.min_width = 80;
					hints.min_height = 60;
					XSetWMNormalHints(dpy, sw->win, &hints);

					/* Create GC for custom drawing */
					XGCValues gcv;
					gcv.foreground = WhitePixel(dpy, screen) ^ sw->color;
					gcv.line_width = 2;
					sw->gc = XCreateGC(dpy, sw->win, GCForeground | GCLineWidth, &gcv);

					/* Map Window */
					XMapWindow(dpy, sw->win);
					arg->created_count++;
				}
			} else {
				/* Existing window: perform random stress actions */
				int action = fast_rand(&seed) % 100;

				if (action < 20) {
					/* Action A: Kill / Destroy Window */
					if (sw->gc) {
						XFreeGC(dpy, sw->gc);
						sw->gc = None;
					}
					if ((fast_rand(&seed) % 2) == 0) {
						XDestroyWindow(dpy, sw->win);
					} else {
						XUnmapWindow(dpy, sw->win);
						XDestroyWindow(dpy, sw->win);
					}
					sw->win = None;
					arg->destroyed_count++;
				} else if (action < 45) {
					/* Action B: Move & Resize Window */
					sw->x += sw->dx;
					sw->y += sw->dy;
					if (sw->x < 0 || sw->x + sw->w >= arg->screen_width) sw->dx = -sw->dx;
					if (sw->y < 20 || sw->y + sw->h >= arg->screen_height) sw->dy = -sw->dy;

					/* Slightly perturb size */
					int dw = (fast_rand(&seed) % 11) - 5;
					int dh = (fast_rand(&seed) % 11) - 5;
					if (sw->w + dw > 80 && sw->w + dw < 600) sw->w += dw;
					if (sw->h + dh > 60 && sw->h + dh < 500) sw->h += dh;

					XMoveResizeWindow(dpy, sw->win, sw->x, sw->y, sw->w, sw->h);
					arg->moveresize_count++;
				} else if (action < 70) {
					/* Action C: Draw random animated shapes inside client */
					if (sw->gc) {
						int rx = fast_rand(&seed) % (sw->w > 20 ? sw->w - 20 : 1);
						int ry = fast_rand(&seed) % (sw->h > 20 ? sw->h - 20 : 1);
						int rw = 10 + (fast_rand(&seed) % 60);
						int rh = 10 + (fast_rand(&seed) % 60);

						XSetForeground(dpy, sw->gc, colors[fast_rand(&seed) % num_colors]);
						if ((fast_rand(&seed) % 3) == 0) {
							XFillRectangle(dpy, sw->win, sw->gc, rx, ry, rw, rh);
						} else if ((fast_rand(&seed) % 3) == 1) {
							XDrawLine(dpy, sw->win, sw->gc, 0, 0, sw->w, sw->h);
							XDrawLine(dpy, sw->win, sw->gc, sw->w, 0, 0, sw->h);
						} else {
							XDrawArc(dpy, sw->win, sw->gc, rx, ry, rw, rh, 0, 360 * 64);
						}
						arg->draw_count++;
					}
				} else if (action < 88) {
					/* Action D: Property Flood (Title / Icon name) */
					char dyn_title[128];
					snprintf(dyn_title, sizeof(dyn_title),
					         "STRESS [%05x] Th%d (iter %lu)",
					         fast_rand(&seed), arg->thread_id, arg->property_count);
					XStoreName(dpy, sw->win, dyn_title);
					XChangeProperty(dpy, sw->win, net_wm_name, utf8_string, 8,
					                PropModeReplace, (unsigned char *)dyn_title, strlen(dyn_title));
					arg->property_count++;
				} else {
					/* Action E: Iconify / De-iconify */
					if (!sw->is_iconic) {
						/* Request iconification */
						XEvent ev;
						memset(&ev, 0, sizeof(ev));
						ev.xclient.type = ClientMessage;
						ev.xclient.window = sw->win;
						ev.xclient.message_type = wm_change_state;
						ev.xclient.format = 32;
						ev.xclient.data.l[0] = IconicState;
						XSendEvent(dpy, root, False,
						           SubstructureRedirectMask | SubstructureNotifyMask, &ev);
						sw->is_iconic = 1;
					} else {
						/* De-iconify */
						XMapWindow(dpy, sw->win);
						sw->is_iconic = 0;
					}
				}
			}
		}

		/* Drain incoming events so queue doesn't blow up */
		while (XPending(dpy) > 0) {
			XEvent ev;
			XNextEvent(dpy, &ev);
		}

		XFlush(dpy);

		/* Short micro-sleep to prevent kernel socket starvation (1 to 8 ms) */
		struct timespec req;
		req.tv_sec = 0;
		req.tv_nsec = 1000000 + (fast_rand(&seed) % 4000000); /* 1 - 5 ms */
		nanosleep(&req, NULL);
	}

	/* Cleanup on termination */
	for (int i = 0; i < arg->num_windows; i++) {
		if (windows[i].win != None) {
			if (windows[i].gc) {
				XFreeGC(dpy, windows[i].gc);
			}
			XDestroyWindow(dpy, windows[i].win);
			arg->destroyed_count++;
		}
	}
	XFlush(dpy);
	XCloseDisplay(dpy);
	free(windows);
	return NULL;
}

int main(int argc, char **argv)
{
	int num_threads = get_nprocs();
	if (num_threads <= 0) num_threads = DEFAULT_THREADS;

	int duration = DEFAULT_DURATION_SEC;
	int windows_per_thread = DEFAULT_WINDOWS_PER_THREAD;
	const char *display_name = getenv("DISPLAY");
	if (!display_name) display_name = ":0";

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
			num_threads = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
			duration = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
			windows_per_thread = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-display") == 0 && i + 1 < argc) {
			display_name = argv[++i];
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("Usage: %s [options]\n"
			       "Options:\n"
			       "  -t <threads>   Number of parallel threads (default: detected CPU cores = %d)\n"
			       "  -w <windows>   Active windows per thread (default: %d, total concurrent: %d)\n"
			       "  -d <seconds>   Test duration in seconds (0 for infinite, default: %d)\n"
			       "  -display <str> X11 display (default: %s)\n"
			       "  -h, --help     Show this help\n",
			       argv[0], num_threads, windows_per_thread,
			       num_threads * windows_per_thread, duration, display_name);
			return 0;
		}
	}

	/* Initialize Xlib multithreading support */
	if (!XInitThreads()) {
		fprintf(stderr, "Warning: XInitThreads() returned 0, multithreading Xlib may have issues\n");
	}

	/* Verify display connection */
	Display *test_dpy = XOpenDisplay(display_name);
	if (!test_dpy) {
		fprintf(stderr, "Error: Unable to connect to X11 display %s\n", display_name);
		return 1;
	}
	XCloseDisplay(test_dpy);

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	printf("====================================================================\n");
	printf(" Window Maker Multithreaded High-Concurrency Stress Test (wm_stress)\n");
	printf("====================================================================\n");
	printf(" Display:             %s\n", display_name);
	printf(" Parallel Threads:    %d (saturating all available CPU cores)\n", num_threads);
	printf(" Windows per Thread:  %d (Total concurrent windows: %d)\n",
	       windows_per_thread, num_threads * windows_per_thread);
	printf(" Duration:            %d seconds %s\n",
	       duration, duration == 0 ? "(infinite, press Ctrl+C to stop)" : "");
	printf(" Actions:             CreateWindow, PropertyFlood, MoveResize,\n"
	       "                      DrawShapes, Iconify, Transient, Destroy\n");
	printf("====================================================================\n");
	printf("Starting stress workers...\n");

	pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
	WorkerArg *args = calloc(num_threads, sizeof(WorkerArg));

	for (int i = 0; i < num_threads; i++) {
		args[i].thread_id = i;
		args[i].display_name = display_name;
		args[i].num_windows = windows_per_thread;
		if (pthread_create(&threads[i], NULL, worker_thread_func, &args[i]) != 0) {
			fprintf(stderr, "Error creating worker thread %d\n", i);
			num_threads = i;
			break;
		}
	}

	struct timeval start_tv, cur_tv;
	gettimeofday(&start_tv, NULL);

	int elapsed = 0;
	while (g_running && (duration == 0 || elapsed < duration)) {
		sleep(1);
		elapsed++;
		gettimeofday(&cur_tv, NULL);
		double total_time = (cur_tv.tv_sec - start_tv.tv_sec) +
		                    (cur_tv.tv_usec - start_tv.tv_usec) / 1000000.0;

		unsigned long total_created = 0;
		unsigned long total_destroyed = 0;
		unsigned long total_props = 0;
		unsigned long total_moves = 0;
		unsigned long total_draws = 0;

		for (int i = 0; i < num_threads; i++) {
			total_created += args[i].created_count;
			total_destroyed += args[i].destroyed_count;
			total_props += args[i].property_count;
			total_moves += args[i].moveresize_count;
			total_draws += args[i].draw_count;
		}

		printf("\r[%3d s] Windows: +%-6lu -%-6lu | Props: %-7lu | Move/Resize: %-7lu | Draws: %-7lu (%.0f ops/s)",
		       elapsed, total_created, total_destroyed, total_props, total_moves, total_draws,
		       (total_created + total_destroyed + total_props + total_moves + total_draws) / (total_time > 0 ? total_time : 1.0));
		fflush(stdout);
	}

	printf("\nStopping workers and cleaning up...\n");
	g_running = 0;

	for (int i = 0; i < num_threads; i++) {
		pthread_join(threads[i], NULL);
	}

	gettimeofday(&cur_tv, NULL);
	double total_sec = (cur_tv.tv_sec - start_tv.tv_sec) +
	                   (cur_tv.tv_usec - start_tv.tv_usec) / 1000000.0;

	unsigned long total_created = 0;
	unsigned long total_destroyed = 0;
	unsigned long total_props = 0;
	unsigned long total_moves = 0;
	unsigned long total_draws = 0;

	for (int i = 0; i < num_threads; i++) {
		total_created += args[i].created_count;
		total_destroyed += args[i].destroyed_count;
		total_props += args[i].property_count;
		total_moves += args[i].moveresize_count;
		total_draws += args[i].draw_count;
	}

	unsigned long total_ops = total_created + total_destroyed + total_props + total_moves + total_draws;

	printf("====================================================================\n");
	printf(" Stress Test Completed Successfully in %.2f seconds!\n", total_sec);
	printf(" Total Windows Created:      %lu (%.1f/sec)\n", total_created, total_created / total_sec);
	printf(" Total Windows Destroyed:    %lu (%.1f/sec)\n", total_destroyed, total_destroyed / total_sec);
	printf(" Total Property Updates:     %lu (%.1f/sec)\n", total_props, total_props / total_sec);
	printf(" Total Move/Resize Actions:  %lu (%.1f/sec)\n", total_moves, total_moves / total_sec);
	printf(" Total Client Draws:         %lu (%.1f/sec)\n", total_draws, total_draws / total_sec);
	printf(" Total Operations:           %lu (%.1f/sec)\n", total_ops, total_ops / total_sec);
	printf("====================================================================\n");

	free(threads);
	free(args);
	return 0;
}
