/*
 * TODO: give input.h rebindable keys and use those for toggling motion/pointer
 */

/* required for clock_gettime with stdc99*/
#define _POSIX_C_SOURCE >= 199309L
#define _GNU_SOURCE

#include "xf-escg.h"	/* lF_WHI lBLD_ _lBLD _lF */
#include "ce-aux.h"	/* __init lprintf */
#include "ce-mod.h"	/* ce_mod_add */
#include "input.h"	/* input_add */
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>	/* malloc */
#include <string.h>	/* memcpy */

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

/* provided by root-window */
extern void root_win_swapbuffers();

/* control function called from main() after init */
extern int (*control)();

static pthread_mutex_t scn_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t scn_cond = PTHREAD_COND_INITIALIZER;
static int progress = 0;

static int input_cb(int n, int type, int x, int y);
static struct inputset *input_set = NULL;
static int inp_sect_ptr;
static int inp_sect_mot;

static struct inputsection input_section_ptr = {
	.callback = input_cb,
	.inputs_a = {
		{ "colour:Progress the colour.", L'ö',
			INPUT_TYPE_KEY },
		{ "bye:Exit the program.", 'q',
			INPUT_TYPE_KEY },
		{ "m-track:Prints some mouse movements.", INPUT_KEY_MOTION,
			INPUT_TYPE_POINTER },
		{ "m-up:Prints out mouse-up events.", INPUT_KEY_MOUSE_OFF + 3,
			INPUT_TYPE_FIRE },
		{ "m-left:Waits for left mouse button.", INPUT_KEY_MOUSE_OFF + 0,
			INPUT_TYPE_KEY },
		{ "mousectl-toggle:Toggles between pointer and motion for mouse inputs.", 't',
			INPUT_TYPE_KEY },
		{ "close:Exit the program.", INPUT_KEY_CLOSE,
			INPUT_TYPE_FIRE },
		{ NULL, '\0', 0 }
	},
};

static struct inputsection *input_section_mot;

static int input_section_active_mot;

static int scn_colour_loop()
{
	lprintf(INF "Reached colour scene loop, woo!\n");
	struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = 10*1000*1000, /* 10ms */
	};
	unsigned cnt = 0;
	while (1) {
		pthread_mutex_lock(&scn_mutex);
		if (progress == -1) {
			pthread_mutex_unlock(&scn_mutex);
			break;
		} else if (!progress) {
			pthread_cond_wait(&scn_cond, &scn_mutex);
			if (progress == -1) {
				pthread_mutex_unlock(&scn_mutex);
				break;
			}
		}
		pthread_mutex_unlock(&scn_mutex);

		cnt++;
		glClearColor( .5f, 0.f,
				(sinf(cnt/30.f) + 1.f) * .5f
				+ (cosf(cnt/6.f)) * .08f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
		root_win_swapbuffers();
		nanosleep(&ts, NULL);
	}
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	root_win_swapbuffers();
	ts.tv_nsec = 300*1000*1000;
	nanosleep(&ts, NULL);
	return 0;
}

static int input_cb(int n, int type, int x, int y)
{
	if (n == 0) {
		pthread_mutex_lock(&scn_mutex);
		if (progress != -1)
			progress = INPUT_EVENT_PRESS == type ? 1 : 0;
		pthread_cond_signal(&scn_cond);
		pthread_mutex_unlock(&scn_mutex);
	} else if (n == 1 || n == 6) {
		int first = 0;
		pthread_mutex_lock(&scn_mutex);
		if (progress != -1) {
			progress = -1;
			first = 1;
		}
		pthread_cond_signal(&scn_cond);
		pthread_mutex_unlock(&scn_mutex);
		if (first)
			lprintf(INF "Bye-bye.\n");
	} else if (n == 2) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		static struct timespec ts_last = {
			.tv_sec = 0,
			.tv_nsec = 0,
		};
		if (ts_last.tv_sec < ts.tv_sec || (ts_last.tv_sec == ts.tv_sec
					&& ts_last.tv_nsec < ts.tv_nsec)) {
			ts_last.tv_sec = ts.tv_sec;
			ts_last.tv_nsec = ts.tv_nsec
				+ 200 * 1000 * 1000; /* 200ms */
			if (ts_last.tv_nsec >= 1000 * 1000 * 1000) {
				ts_last.tv_sec += 1;
				ts_last.tv_nsec -= 1000 * 1000 * 1000;
			}
			lprintf(TXT "Mouse movement (%s): %2i %2i\n",
					(type == INPUT_EVENT_MOTION) ? "motion"
						: "pointer", x, y);
		}
	} else if (n == 3) {
		assert(type == INPUT_EVENT_FIRE);
		lprintf(TXT "Mouse wheel up triggered! (pt %2i %2i)\n",
				x, y);
	} else if (n == 4) {
		assert(type == INPUT_EVENT_PRESS
				|| type == INPUT_EVENT_RELEASE);
		lprintf(TXT "Mouse left button %s! (pt %2i %2i)\n",
				type==INPUT_EVENT_PRESS ? "pressed"
					: "released",
				x, y);
	} else if (n == 5) {
		if (type == INPUT_EVENT_PRESS)
			return 0;
		if (input_section_active_mot) {
			input_rm(input_set, inp_sect_mot);
			inp_sect_ptr = input_add(input_set, &input_section_ptr);
		} else {
			input_rm(input_set, inp_sect_ptr);
			inp_sect_mot = input_add(input_set, input_section_mot);
		}
		input_section_active_mot = !input_section_active_mot;
		input_set_active(input_set);
	} else if (n == 6) {
		/* Handled above, dead section */
	}
	return 0;
}

static int load()
{
	control = scn_colour_loop;
	lprintf(INF lF_WHI lBLD_"scn~colour selected."_lBLD _lF"\n");

	input_set = input_set_create();
	inp_sect_ptr = input_add(input_set, &input_section_ptr);
	input_set_active(input_set);
	input_section_active_mot = 0;

	/* Create section where mouse input is motion */
	int i;
	for (i = 0; input_section_ptr.inputs_a[i].name; i++);
	i++;
	input_section_mot = malloc(sizeof(struct inputsection)
			+ sizeof(struct input) * i);
	memcpy(input_section_mot, &input_section_ptr, sizeof(struct inputsection)
			+ sizeof(struct input) * i);
	input_section_mot->inputs_a[2].types = INPUT_TYPE_MOTION;
	return 0;
}

static int unload()
{
	input_set_active(NULL);
	input_set_destroy(input_set);
	return 0;
}


static int colour_mod_id = -1;

static void __init code_load()
{
	struct ce_mod m = {
		.comment = "An example scene containing input determined background colour shuffling.",
		.def = "scn-colour | scn~colour; control=colour-loop",
		.use = "gl-context; root-window; input;",
		.load = load,
		.unload = unload,
	};
	colour_mod_id = ce_mod_add(&m);
	assert(colour_mod_id >= 0);
}

static void __exit code_unload()
{
	ce_mod_rm(colour_mod_id);
}
