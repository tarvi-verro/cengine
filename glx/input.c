
/* TODO:
 *	Window resize
 *	Window exposure
 *	Error handling when xcb_con should fail
 *	Window close [X] event
 *	Regrab master pointer when focus regained
 *	Hide the pointer when INPUT_TYPE_MOTION active
 */

/* required for clock_gettime and pthread_timedjoin_np with stdc99*/
#define _POSIX_C_SOURCE >= 199309L
#define _GNU_SOURCE

#include "ce-aux.h"
#include "ce-log.h"
#include "ce-mod.h"
#include "ce-opt.h"
#include "input.h"

#include <X11/Xlib-xcb.h> /* XGetXCBConnection */
#include <X11/Xlibint.h> /* XESetWireToEvent */
#include <pthread.h>	/* pthread_create */
#include <stdio.h>	/* fprintf */
#include <stdint.h>	/* uint8_t */
#include <stdlib.h>	/* free */
#include <unistd.h>	/* sleep */
#include <stdbool.h>
#include <assert.h>
#include <string.h>	/* memset */
#include <errno.h>
#include <time.h>	/* clock_gettime */
#include <xcb/xcb_keysyms.h> /* (-lxcb-keysyms) */
#include <xcb/xinput.h>	/* xcb_input_list_input_devices (-lxcb-xinput)*/

extern Display *glx_dpy;
extern Window glx_win;

static xcb_connection_t *xcb_con = NULL;

static int u32_to_utf8(uint32_t input, char *out, int out_size)
{
#define pbits(n) printf("%i%i%i%i %i%i%i%i\n",!!(n&0x80),!!(n&0x40),!!(n&0x20),!!(n&0x10),\
		!!(n&8),!!(n&4),!!(n&2),!!(n&1))
	unsigned c = input;
	if (c < 0x80) {
		assert(out_size >= 2);
		out[0] = c;
		out[1] = '\0';
		return 1;
	}
	if (c < 0x800) {
		assert(out_size >= 3);
		out[0] = 0xc0 | ((input >> 6) & 0x7f);
		out[1] = 0x80 | (input & 0x3f);
		out[2] = '\0';
		return 2;
	}
	if (c < 0x10000) {
		assert(out_size >= 4);
		out[0] = 0xe0 | ((input >> 12) & 0xf);
		out[1] = 0x80 | ((input >> 6) & 0x3f);
		out[2] = 0x80 | (input & 0x3f);
		out[3] = '\0';
		return 3;
	}
	if (c < 0x200000) {
		assert(out_size >= 5);
		out[0] = 0xf0 | ((input >> 18) & 0x7);
		out[1] = 0x80 | ((input >> 12) & 0x3f);
		out[2] = 0x80 | ((input >> 6) & 0x3f);
		out[3] = 0x80 | (input & 0x3f);
		out[4] = '\0';
		return 4;
	}
	if (c < 0x4000000) {
		assert(out_size >= 6);
		out[0] = 0xf8 | ((input >> 24) & 0x3);
		out[1] = 0x80 | ((input >> 18) & 0x3f);
		out[2] = 0x80 | ((input >> 12) & 0x3f);
		out[3] = 0x80 | ((input >> 6) & 0x3f);
		out[4] = 0x80 | (input & 0x3f);
		out[5] = '\0';
		return 5;
	}
	if (c <= 0x7FFFFFFF) {
		assert(out_size >= 7);
		out[0] = 0xfc | ((input >> 30) & 0x1);
		out[1] = 0x80 | ((input >> 24) & 0x3f);
		out[2] = 0x80 | ((input >> 18) & 0x3f);
		out[3] = 0x80 | ((input >> 12) & 0x3f);
		out[4] = 0x80 | ((input >> 6) & 0x3f);
		out[5] = 0x80 | (input & 0x3f);
		out[6] = '\0';
		return 6;
	}
	assert(1==3); /* RFC 3629 ended UTF-8 already at U+10FFFF */
	out[0] = '\0';
	return 0;
}

struct event_triggers_pt {
	uint8_t cb_index;
	uint8_t input_index : 7;
	uint8_t modifier : 1; /* INPUT_TYPE_KEY_REPEAT for INPUT_TYPE_KEY */
};

struct event_triggers {
	int (**cbs_a)(int n, int event, int x, int y);

	struct event_triggers_pt *trigs_a;

	uint8_t motion_trig;
	bool motion_curs;

	int key_trig_off;
	uint8_t *key_trig;

	/*int button_trig_off; // const 1*/
	uint8_t *button_trig;
};

static pthread_mutex_t loop_control;
static pthread_cond_t loop_control_cond;
static struct event_triggers *loop_control_next = NULL;

static xcb_key_symbols_t *symbol_table = NULL;

static void event_handle_raw(xcb_ge_generic_event_t *ge,
		struct event_triggers *tri)
{
	assert(!tri->motion_curs);
	int i;
	if (ge->event_type == XCB_INPUT_RAW_BUTTON_PRESS) {
		xcb_input_raw_button_press_event_t *bp
			= (xcb_input_raw_button_press_event_t *) ge;
		struct event_triggers_pt *pt = tri->trigs_a
			+ tri->button_trig[bp->detail - 1];
		switch (bp->detail) {
		case 4: /* mwhl-up */
			i = tri->cbs_a[pt->cb_index](pt->input_index,
					INPUT_EVENT_FIRE,
					0, 0); /* raw event: rel coords */
			assert(!i);
			break;
		case 5: /* mwhl-down */
			i = tri->cbs_a[pt->cb_index](pt->input_index,
					INPUT_EVENT_FIRE,
					0, 0); /* raw event: rel coords */
			assert(!i);
			break;
		default: /* actual buttons */
			i = tri->cbs_a[pt->cb_index](pt->input_index,
					INPUT_EVENT_PRESS,
					0, 0); /* raw event: rel coords */
			assert(!i);
		}
	} else if (ge->event_type == XCB_INPUT_RAW_BUTTON_RELEASE) {
		xcb_input_raw_button_release_event_t *bp
			= (xcb_input_raw_button_release_event_t *) ge;
		switch (bp->detail) {
		case 4: break; /* mwhl-up */
		case 5: break; /* mwhl-down*/
		default:; /* actual buttons */
			struct event_triggers_pt *pt = tri->trigs_a
				+ tri->button_trig[bp->detail - 1];
			i = tri->cbs_a[pt->cb_index](pt->input_index,
					INPUT_EVENT_RELEASE,
					0, 0); /* raw event: rel coords */
			assert(!i);
		}
	} else if (ge->event_type == XCB_INPUT_RAW_MOTION) {
		/* TODO: Doubt valuators_len will ever be any other value,
		 * verification needed.
		 * Assumption is made that Rel-X and Rel-Y mouse master
		 * pointer coordinates are represented by bits 0x1 and 0x2
		 * respectively.
		 * Subpixel values are dropped.
		 */
		xcb_input_raw_motion_event_t *rp =
			(xcb_input_raw_motion_event_t *) ge;

		assert(rp->valuators_len == 2);
		int32_t *a = (int32_t *) (rp + 1);
		uint32_t v = (*(uint64_t *) a) & 0x3;
		if (!v) /* must be one of the scroll movements */
			return;
		a += 2;
		int32_t x = 0;
		int32_t y = 0;
		if (v & 0x1) {
			x = *a; /* a[1] - uint32_t subpixel */
			a += 2;
		}
		if (v & 0x2) {
			y = *a;
			a += 2;
		}
		struct event_triggers_pt *pt = tri->trigs_a
			+ tri->motion_trig;
		i = tri->cbs_a[pt->cb_index](pt->input_index,
				INPUT_EVENT_MOTION, x, y);
		assert(!i);
	} else {
		lprintf(WRN "Unhandled raw event!\n");
	}
}

extern pthread_mutex_t win_mutex;
extern int win_width;
extern int win_height;

/**
 * event_feed_xlib() - feed a given event to the xlib wire listeners
 * @event:	event to feed them
 *
 * This code snippet was found at the following site:
 * https://bugs.freedesktop.org/show_bug.cgi?id=35945#c4
 *
 * TODO: Is it OK to call this wire function from another thread?
 */
static void event_feed_xlib(xcb_generic_event_t *event)
{
	/* Returns the previous function, sets new one to NULL. We now possess
	 * the potential function to call. */
	Bool (*proc)(Display*, XEvent*, xEvent*) =
		XESetWireToEvent(glx_dpy, event->response_type, 0);

	if (proc) {
		/* Resets the previous function to handle the events. */
		XESetWireToEvent(glx_dpy, event->response_type, proc);
		XEvent dummy;
		event->sequence = LastKnownRequestProcessed(glx_dpy);
		proc(glx_dpy, &dummy, (xEvent *) event);
	}
}

static void event_handle(int *looping, xcb_generic_event_t *event,
		struct event_triggers *tri)
{
	int i;
	int typ = event->response_type & ~0x80;
	if (typ == XCB_KEY_RELEASE) {
		xcb_key_release_event_t *kp =
			(xcb_key_release_event_t *) event;
		/* handle fake controlevents */
		if (kp->detail == 0) {
			assert(kp->root_x != 0);
			*looping = kp->root_x;
			return;
		}

		/* exclude auto-repeat if necessary */
		struct event_triggers_pt *pt = tri->trigs_a
			+ tri->key_trig[kp->detail - tri->key_trig_off];
		if (pt->modifier) {
			/* autorepeat enabled */
			i = tri->cbs_a[pt->cb_index](pt->input_index,
					INPUT_EVENT_RELEASE,
					kp->event_x, kp->event_y);
			assert(!i);
			return;
		}
		xcb_generic_event_t *qevent;
		qevent = xcb_poll_for_queued_event(xcb_con);
		if (qevent == NULL) {
			/* none queued, but perhaps will be in a ms */
			struct timespec ts = {
				.tv_sec = 0,
				.tv_nsec = 1*1000*1000
			};
			nanosleep(&ts, NULL);
			qevent = xcb_poll_for_event(xcb_con);
		}
		if (qevent != NULL){
			int qtyp = qevent->response_type & ~0x80;
			xcb_key_press_event_t *qkp =
				(xcb_key_press_event_t *) qevent;
			if (qtyp == XCB_KEY_PRESS
					&& qkp->time == kp->time
					&& qkp->detail == kp->detail
					&& qkp->state == kp->state) {
				/* duplicate; cancel */
				free(qevent);
				return;
			}
		}
		/* either not a duplicate or Xserver is lagging */
		i = tri->cbs_a[pt->cb_index](pt->input_index,
				INPUT_EVENT_RELEASE,
				kp->event_x, kp->event_y);
		assert(!i);
		if (qevent != NULL) {
			event_handle(looping, qevent, tri);
			free(qevent);
		}
		return;
	} else if (typ == XCB_KEY_PRESS) {
		xcb_key_press_event_t *kp =
			(xcb_key_press_event_t *) event;
		struct event_triggers_pt *pt = tri->trigs_a
			+ tri->key_trig[kp->detail - tri->key_trig_off];
		i = tri->cbs_a[pt->cb_index](pt->input_index,
				INPUT_EVENT_PRESS,
				kp->event_x, kp->event_y);
		assert(!i);
		return;
	} else if (typ == XCB_BUTTON_PRESS) {
		if (!tri->motion_curs) /* Raw events enabled, avoid duplicates */
			return;
		xcb_button_press_event_t *bp =
			(xcb_button_press_event_t *) event;
		struct event_triggers_pt *pt = tri->trigs_a
			+ tri->button_trig[bp->detail - 1];
		switch (bp->detail) {
		case 4: /* mwhl-up */
			i = tri->cbs_a[pt->cb_index](pt->input_index,
					INPUT_EVENT_FIRE,
					bp->event_x, bp->event_y);
			assert(!i);
			break;
		case 5: /* mwhl-down */
			i = tri->cbs_a[pt->cb_index](pt->input_index,
					INPUT_EVENT_FIRE,
					bp->event_x, bp->event_y);
			assert(!i);
			break;
		default: /* actual buttons */
			i = tri->cbs_a[pt->cb_index](pt->input_index,
					INPUT_EVENT_PRESS,
					bp->event_x, bp->event_y);
			assert(!i);
		}
	} else if (typ == XCB_BUTTON_RELEASE) {
		if (!tri->motion_curs) /* Raw events enabled, avoid duplicates */
			return;
		xcb_button_release_event_t *bp =
			(xcb_button_release_event_t *) event;
		switch (bp->detail) {
		case 4: break; /* mwhl-up */
		case 5: break; /* mwhl-down */
		default:;
			struct event_triggers_pt *pt = tri->trigs_a
				+ tri->button_trig[bp->detail - 1];
			i = tri->cbs_a[pt->cb_index](pt->input_index,
					INPUT_EVENT_RELEASE,
					bp->event_x, bp->event_y);
			assert(!i);
		}
	} else if (typ == XCB_MOTION_NOTIFY) {
		if (!tri->motion_curs) /* Raw events enabled, avoid duplicates */
			return;
		xcb_motion_notify_event_t *np =
			(xcb_motion_notify_event_t *) event;
		struct event_triggers_pt *pt = tri->trigs_a
			+ tri->motion_trig;
		i = tri->cbs_a[pt->cb_index](pt->input_index,
				INPUT_EVENT_MOTION,
				np->event_x, np->event_y);
		assert(!i);
	} else if (typ == XCB_GE_GENERIC) {
		xcb_ge_generic_event_t *ge =
			(xcb_ge_generic_event_t *) event;
		event_handle_raw(ge, tri);
	} else if (typ == XCB_EXPOSE) {
		xcb_expose_event_t *ex =
		       (xcb_expose_event_t *) event;
		/*lprintf(DBG "XCB_EXPOSE %ix%i\n", ex->width, ex->height);*/
		pthread_mutex_lock(&win_mutex);
		if (win_width != ex->width || win_height == ex->height) {
			win_width = ex->width;
			win_height = ex->height;
		}
		pthread_mutex_unlock(&win_mutex);
		event_feed_xlib(event);
	} else if (typ == XCB_MAP_NOTIFY) {
		event_feed_xlib(event);
	} else if (typ == XCB_UNMAP_NOTIFY) {
		event_feed_xlib(event);
	} else if (typ == XCB_CONFIGURE_NOTIFY) {
		/*xcb_configure_notify_event_t *ce =
			(xcb_configure_notify_event_t *) event;
		lprintf(WRN "XCB_CONFIGURE_NOTIFY %ix%i\n", ce->width,
				ce->height);*/
		/* An event that receives news of window resize before
		 * XCB_EXPOSE, but also sometimes multiple times for the same
		 * dimensions. Also receives window position changes. */
		event_feed_xlib(event);
	} else if (typ == XCB_CHANGE_KEYBOARD_CONTROL) {
		/* An event that is triggered after every buffer swap. */
	} else if (typ == XCB_GET_KEYBOARD_CONTROL) {
		/* An event that is triggered every time buffers are swapped. */
		event_feed_xlib(event);
	} else if (typ == 0) { /* error event */
		xcb_generic_error_t *ep =
			(xcb_generic_error_t *) event;
		lprintf(ERR "Something went wrong with xcb/X11: "
				"ec"lF_RED"%3i"_lF"\n", ep->error_code);
	} else {
		lprintf(WRN "Unrecognized event type"lF_YELW"%3i"_lF".\n", typ);
		event_feed_xlib(event);
	}
}

static xcb_input_device_id_t input_master_pointer;
static void event_input_raw_set(bool toraw)
{
	if (!toraw) {
		xcb_input_xi_ungrab_device(xcb_con,
				XCB_CURRENT_TIME, input_master_pointer);
		return;
	}

	/* Grab the master pointer */
	xcb_generic_error_t *e = NULL;

	uint32_t mask = XCB_INPUT_XI_EVENT_MASK_RAW_MOTION
		| XCB_INPUT_XI_EVENT_MASK_RAW_BUTTON_PRESS
		| XCB_INPUT_XI_EVENT_MASK_RAW_BUTTON_RELEASE;

	xcb_input_xi_grab_device_cookie_t gc
		= xcb_input_xi_grab_device(xcb_con, glx_win, XCB_CURRENT_TIME,
				XCB_NONE, input_master_pointer,
				XCB_GRAB_MODE_ASYNC,
				XCB_GRAB_MODE_ASYNC,
				true, 1, &mask);
	xcb_input_xi_grab_device_reply_t *gr
		= xcb_input_xi_grab_device_reply(xcb_con, gc, &e);
	assert(e == NULL);
	assert(!gr->status);
	free(gr);
}

static void *event_loop(void *nothing)
{
	assert(nothing == NULL); /* As passed from pthread_create */

	struct event_triggers *tri = NULL;
	int looping = 0;
	int w;
reload_tri:
	{
		bool curs_current = tri == NULL || tri->motion_curs;
		w = pthread_mutex_lock(&loop_control);
		assert(!w);
		tri = loop_control_next;
		pthread_cond_signal(&loop_control_cond);
		w = pthread_mutex_unlock(&loop_control);
		assert(!w);
		if (tri->motion_curs != curs_current) {
			event_input_raw_set(!tri->motion_curs);
		}
	}

	while (!looping) {
		xcb_generic_event_t *event = xcb_wait_for_event(xcb_con);
		if (!event) {
			printf("No event today :( \n");
			break;
		}
		event_handle(&looping, event, tri);
		free(event);
	}

	if (looping == 1) { /* close loop */
	} else if (looping == 2) { /* reload tri */
		looping = 0;
		goto reload_tri;
	}

	return NULL;
}

/**
 * event_signal_send() - send special crafted signal event to event loop
 * @root_x:	signal id; %0 to terminate, %1 to reload loop_control_next
 *
 * Crafts and sends a special event that event_loop() should catch and
 * respond to.
 */
static void event_signal_send(int root_x)
{
#if 0 /* Sending events with Xlib */
	XKeyEvent ev = {
		.type = KeyPress,
		.serial = 0x0,
		.send_event = 0,
		.display = glx_dpy,
		.window = glx_win,
		.root = XDefaultRootWindow(glx_dpy),
		.subwindow = 0,
		.time = CurrentTime,
		.x = 0,
		.y = 0,
		.x_root = root_x,
		.y_root = 0,
		.state = 0,
		.keycode = 0,
		.same_screen = 1,
	};
	XSendEvent(glx_dpy, glx_win, False, KeyPressMask, (XEvent *) &ev);
	XFlush(glx_dpy);
#else /* Sending events with xcb */
	struct xcb_key_press_event_t *ev = calloc(32, 1);
	ev->response_type = XCB_KEY_RELEASE;
	ev->detail = 0;
	ev->sequence = 0;
	ev->time = XCB_CURRENT_TIME;
	ev->root = XDefaultRootWindow(glx_dpy);
	ev->event = 0;
	ev->child = 0;
	ev->root_x = root_x;
	ev->root_y = 0;
	ev->event_x = 0;
	ev->event_y = 0;
	ev->state = 0;
	ev->same_screen = 1;
	ev->pad0 = 0;
	xcb_send_event(xcb_con, 0, glx_win, XCB_EVENT_MASK_KEY_PRESS, (char *) ev);
	xcb_flush(xcb_con);
#endif
}

struct inputset {
	bool dirty;
	uint8_t sections_length;
	uint8_t sections_count;
	uint8_t sections_size;
	const struct inputsection **sections_a;

	uint16_t handleinfo_length;
	struct event_triggers *handleinfo;
};

static int handleinfo_unused_length = 0;
static struct event_triggers *handleinfo_unused = NULL;

static struct inputset *active = NULL;
static int inf_keycode_min = -1;
static int inf_keycode_max = -1;

int input_add(struct inputset *set, const struct inputsection *trig)
{
	assert(set != NULL);
	assert(trig != NULL);

	int index = -1;
	if (set->sections_length > set->sections_count) {
		for (int i = 0; i < set->sections_length; i++) {
			if (set->sections_a[i])
				continue;
			index = i;
			set->sections_count++;
		}
	} else if (set->sections_length + 1 < set->sections_size) {
		int n = set->sections_size * 2;
		if (n >= UINT8_MAX) {
			lprintf(ERR "Input set's section buffer exhausted.\n");
			return -1;
		}
		set->sections_size *= 2;
		set->sections_a = realloc(set->sections_a,
				sizeof(void *) * set->sections_size);
		assert(set->sections_a != NULL);
		index = set->sections_length;
		set->sections_length++;
		set->sections_count++;
	} else {
		index = set->sections_length;
		set->sections_length++;
		set->sections_count++;
	}
	set->dirty = true;
	set->sections_a[index] = trig;

	return index;
}

int input_rm(struct inputset *set, int in_id)
{
	assert(in_id >= 0);
	assert(set != NULL);

	assert(in_id < set->sections_length && set->sections_a[in_id] != NULL);

	set->sections_a[in_id] = NULL;
	set->sections_count--;
	if (in_id == set->sections_length - 1)
		set->sections_length--;

	set->dirty = true;

	return 0;
}

static void trig_targets_process(struct event_triggers *ev,
		const struct input *inp, int ev_trigs_id)
{
	if (inp->defkey >= 0) {
		assert(inp->types & INPUT_TYPE_KEY);
		xcb_keycode_t *kc = xcb_key_symbols_get_keycode(symbol_table,
				inp->defkey);
		ev->key_trig[(*kc) - ev->key_trig_off] = ev_trigs_id;
		free(kc);
	} else if (inp->defkey >= INPUT_KEY_MOUSE_OFF) {
		if (inp->defkey == INPUT_KEY_MOUSE_OFF + 3
				|| inp->defkey == INPUT_KEY_MOUSE_OFF + 4) {
			assert(inp->types & INPUT_TYPE_FIRE);
		} else {
			assert(inp->types & INPUT_TYPE_KEY);
		}
		ev->button_trig[inp->defkey - INPUT_KEY_MOUSE_OFF]
			= ev_trigs_id;
	} else if (inp->defkey == INPUT_KEY_MOTION) {
		assert((inp->types & INPUT_TYPE_MOTION));
		assert(!ev->motion_trig);
		ev->motion_curs =
			(INPUT_TYPE_POINTER & inp->types) == INPUT_TYPE_POINTER;
		ev->motion_trig = ev_trigs_id;
	} else {
		assert(1==2);
	}
}

static int dummy_callback(int n, int event, int x, int y)
{
	printf("Unbound key\n");
	return 0;
}

static struct event_triggers *ev_null = NULL;
static int ev_null_length = 0;

void dummy_construct(struct event_triggers **ev, int *ev_length)
{
	assert(ev != NULL && ev_length != NULL);
	int trigs_len = 1;
	int ev_length_req = sizeof(struct event_triggers)
		+ sizeof(int (**)(int, int, int, int)) * 1
		+ sizeof(struct event_triggers_pt) * trigs_len
		+ sizeof(uint8_t) * (inf_keycode_max - inf_keycode_min)
		+ sizeof(uint8_t) * (INPUT_KEY_MOUSE_OFF * -1);

	if ((*ev_length) < ev_length_req) {
		/* align to 32bytes */
		*ev_length = ev_length_req + (32 - (ev_length_req % 32));
		*ev = realloc((*ev), *ev_length);
	}

	/* point pointers */
	(*ev)->trigs_a = (struct event_triggers_pt *) ((*ev) + 1);

	(*ev)->cbs_a = (int (**)(int, int, int, int))
		((*ev)->trigs_a + trigs_len);

	(*ev)->key_trig = (uint8_t *) ((*ev)->cbs_a + 1);
	/* 0 will point to dummy trigs_a entry */
	memset((*ev)->key_trig, 0, inf_keycode_max - inf_keycode_min);

	(*ev)->button_trig = (uint8_t *) ((*ev)->key_trig
			+ (inf_keycode_max - inf_keycode_min));
	memset((*ev)->button_trig, 0, INPUT_KEY_MOUSE_OFF * -1);

	/* other */
	(*ev)->key_trig_off = inf_keycode_min;
	(*ev)->motion_trig = 0;
	(*ev)->motion_curs = true;

	/* dummy info */
	(*ev)->trigs_a[0].cb_index = 0;
	(*ev)->trigs_a[0].input_index = 0;
	(*ev)->trigs_a[0].modifier = 0;
	(*ev)->cbs_a[0] = dummy_callback;
}

struct inputset *input_set_active(struct inputset *set)
{
	struct event_triggers *next;

	if (set == NULL) {
		next = ev_null;
	} else if (!set->dirty && set == active) {
		return set;
	} else if (set->dirty) {
		int ev_length;
		struct event_triggers *ev;
		if (active == set) {
			ev_length = handleinfo_unused_length;
			ev = handleinfo_unused;
		} else {
			ev_length = set->handleinfo_length;
			ev = set->handleinfo;
		}

		int trigs_len = 1; /* one 'no-trigger' entry */
		for (int i = 0; i < set->sections_length; i++) {
			const struct inputsection *t = set->sections_a[i];

			int j;
			for (j = 0; t->inputs_a[j].name != NULL
					|| t->inputs_a[j].defkey != '\0'; j++);
			trigs_len += j;
		}
		/* make sure there's enough memory */
		int ev_length_req = sizeof(struct event_triggers)
			+ sizeof(int (**)(int n, int event, int x, int y))
				/* one dummy 'no-trigger' function */
				* (set->sections_length + 1)
			+ sizeof(struct event_triggers_pt) * trigs_len
			+ sizeof(uint8_t) * (inf_keycode_max - inf_keycode_min)
			+ sizeof(uint8_t) * (INPUT_KEY_MOUSE_OFF * -1);

		if (ev_length < ev_length_req) {
			/* align to 32bytes */
			ev_length = ev_length_req + (32 - (ev_length_req % 32));
			ev = realloc(ev, ev_length);
		}

		/* point pointers */
		ev->trigs_a = (struct event_triggers_pt *) (ev + 1);

		ev->cbs_a = (int (**)(int, int, int, int))
			(ev->trigs_a + trigs_len);

		ev->key_trig = (uint8_t *) (ev->cbs_a + set->sections_length);
		/* 0 will point to dummy trigs_a entry */
		memset(ev->key_trig, 0, inf_keycode_max - inf_keycode_min);

		ev->button_trig = (uint8_t *) (ev->key_trig
				+ (inf_keycode_max - inf_keycode_min));
		memset(ev->button_trig, 0, INPUT_KEY_MOUSE_OFF * -1);

		/* other */
		ev->key_trig_off = inf_keycode_min;
		ev->motion_trig = 0;
		ev->motion_curs = 1;

		/* fill in info */
		int trig_id = 1;
		for (int i = 0; i < set->sections_length; i++) {
			/* callbacks */
			ev->cbs_a[i] = set->sections_a[i]->callback;
			const struct inputsection *t = set->sections_a[i];
			for (int j = 0; t->inputs_a[j].name != NULL
					|| t->inputs_a[j].defkey != '\0'; j++) {
				ev->trigs_a[trig_id].cb_index = i;
				ev->trigs_a[trig_id].input_index = j;
				ev->trigs_a[trig_id].modifier =
					!!(t->inputs_a[j].types
							& INPUT_TYPE_KEY_REPEAT);
				/* finds the configured target key/button/events
				 * and maps them or default if not found */
				trig_targets_process(ev, t->inputs_a + j, trig_id);
				trig_id++;
			}
		}

		/* dummy info */
		ev->trigs_a[0].cb_index = set->sections_length;
		ev->trigs_a[0].input_index = 0;
		ev->cbs_a[set->sections_length] = dummy_callback;

		/* set the unused (even if active != set, set's one is now
		 * unused, active's will remain untouched) */
		if (active == set) {
			handleinfo_unused_length = set->handleinfo_length;
			handleinfo_unused = set->handleinfo;
		}
		set->handleinfo = ev;
		set->handleinfo_length = ev_length;

		next = set->handleinfo;
		set->dirty = false;
	}

	/* update eventthread */
	pthread_mutex_lock(&loop_control);
	loop_control_next = next;
	event_signal_send(2);
	struct timespec timeout;
	int i = clock_gettime(CLOCK_REALTIME, &timeout);
	assert(i != -1);
	timeout.tv_sec += 2;
	if (pthread_cond_timedwait(&loop_control_cond, &loop_control, &timeout)) {
		lprintf(ERR "Input thread jammed: did not respond to event-triggers change!\n");
		exit(1);
	}
	pthread_mutex_unlock(&loop_control);

	struct inputset *active_old = active;
	active = set;
	return active_old;
}

struct inputset *input_set_create()
{
	struct inputset *set = malloc(sizeof(struct inputset));
	set->dirty = true;
	set->sections_length = 0;
	set->sections_count = 0;
	set->sections_size = 4;
	set->sections_a = malloc(set->sections_size * sizeof(void *));
	set->handleinfo_length = 0;
	set->handleinfo = NULL;

	return set;
}

int input_set_destroy(struct inputset *set)
{
	assert(set != NULL);
	assert(set != active);

	free(set->handleinfo);
	free(set->sections_a);
	free(set);
	return 0;
}


static pthread_t loopthread;

static void input_lsdevs();
#include "input-lsdevs.c"

static bool lsdevs = false;
static int input_opts_cb(int index, const char *optarg)
{
	assert(index == 0);
	lsdevs = true;
	return 0;
}

struct optsection input_opts = {
	.label = "Input:",
	.callback = input_opts_cb,
	.opt_a = {
		{ ARG_NONE, '\0', "lsdevs", "List available devices on load." },
		{ ARG_NONE, '\0', NULL, NULL },
	},
};


static int load()
{
	xcb_con = XGetXCBConnection(glx_dpy);
	XSetEventQueueOwner(glx_dpy, XCBOwnsEventQueue);

	const xcb_setup_t *s = xcb_get_setup(xcb_con);
	inf_keycode_max = s->max_keycode;
	inf_keycode_min = s->min_keycode;

	symbol_table = xcb_key_symbols_alloc(xcb_con);

	/* find input_master_pointer */
	xcb_generic_error_t *e = NULL;
	bool found_mptr = false;
	xcb_input_xi_query_device_cookie_t dc
		= xcb_input_xi_query_device(xcb_con,
				XCB_INPUT_DEVICE_TYPE_MASTER_POINTER);
	xcb_input_xi_query_device_reply_t *dr
		= xcb_input_xi_query_device_reply(xcb_con, dc, &e);
	assert(e == NULL); /* TODO: handle the error */
	xcb_input_xi_device_info_iterator_t iter
		= xcb_input_xi_query_device_infos_iterator(dr);
	while (iter.rem) {
		xcb_input_xi_device_info_t *d = iter.data;
		if (d->type != XCB_INPUT_DEVICE_TYPE_MASTER_POINTER) {
			xcb_input_xi_device_info_next(&iter);
			continue;
		}
		input_master_pointer = d->deviceid;
		found_mptr = true;
		break;
	}
	free(dr);
	assert(found_mptr); /* TODO: handle it */


	int w = pthread_mutex_init(&loop_control, NULL);
	assert(!w);
	w = pthread_cond_init(&loop_control_cond, NULL);
	assert(!w);

	dummy_construct(&ev_null, &ev_null_length);
	assert(ev_null != NULL);

	pthread_mutex_lock(&loop_control);
	loop_control_next = ev_null;

	int i = pthread_create(&loopthread, NULL, event_loop, NULL);
	if (i) {
		errno = i;
		perror("pthread_create");
	}
	pthread_cond_wait(&loop_control_cond, &loop_control);
	pthread_mutex_unlock(&loop_control);

	if (lsdevs) {
		input_lsdevs();
		lsdevs = false;
	}
	return 0;
}

static int unload()
{
	event_signal_send(1);
	int i;
	void *res;

	struct timespec timeout;
	i = clock_gettime(CLOCK_REALTIME, &timeout);
	assert(i != -1);
	timeout.tv_sec += 2;

	i = pthread_timedjoin_np(loopthread, &res, &timeout);
	if (i != 0) {
		lprintf(ERR "Input thread did not exit in time!\n");
		/*i = pthread_cancel(loopthread);
		  assert(!i);*/
	}

	i = clock_gettime(CLOCK_REALTIME, &timeout);
	assert(i != -1);
	timeout.tv_sec += 2;

	if (pthread_mutex_timedlock(&loop_control, &timeout)) {
		lprintf(ERR "Input thread control mutex couldn't be released!\n");
	} else {
		pthread_mutex_unlock(&loop_control);
		pthread_mutex_destroy(&loop_control);
	}
	pthread_cond_destroy(&loop_control_cond);

	XSetEventQueueOwner(glx_dpy, XlibOwnsEventQueue);

	free(handleinfo_unused);
	handleinfo_unused = NULL;
	handleinfo_unused_length = 0;

	inf_keycode_min = -1;
	inf_keycode_max = -1;

	free(ev_null);
	ev_null = NULL;
	ev_null_length = 0;

	xcb_key_symbols_free(symbol_table);

	/* free'd as XCloseDisplay(glx_dpy) by window.c */
	xcb_con = NULL;
	return 0;
}

static int glx_input_mod_id = -1;
static void __init code_load()
{
	struct ce_mod m =
	{
		.comment = "X window input events via xcb.",
		.def = "xcb-input | input",
		.use = "glx-visual",
		.load = load,
		.unload = unload,
	};
	glx_input_mod_id = ce_mod_add(&m);
	assert(glx_input_mod_id >= 0);
	int rv = opt_add(ce_options, &input_opts);
	assert(rv >= 0);
}

static void __exit code_unload()
{
	assert(glx_input_mod_id >= 0);
	ce_mod_rm(glx_input_mod_id);
	int rv = opt_rm(ce_options, &input_opts);
	assert(rv >= 0);
}
