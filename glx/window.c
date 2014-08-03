#include "ce-aux.h"
#include "ce-log.h"
#include "ce-mod.h"
#include "ce-opt.h"

#include <assert.h>
#include <string.h>	/* memcpy */
#include <stdlib.h>	/* malloc */
#include <stdio.h>	/* sscanf */
#include <X11/Xlib.h>	/* Display */
#include <GL/glx.h>	/* glXChooseVisual */
#include <pthread.h>	/* pthread_mutex_lock */

static char *dpy_name = NULL;
static char *win_name = NULL;
Display *glx_dpy = NULL;
pthread_mutex_t win_mutex = PTHREAD_MUTEX_INITIALIZER;
int win_width = 320;
int win_height = 180;
Window glx_win;
GLXFBConfig glx_fbc;

void root_win_swapbuffers()
{
	glXSwapBuffers(glx_dpy, glx_win);
}

static void lprintFBConfig(Display *dpy, GLXFBConfig fbc, int id)
{
	char idbuf[32];
	if (id < 0)
		idbuf[0] = '\0';
	else
		snprintf(idbuf, 32, "FB %2i: ", id);

	int smp, bfrs, r, g, b, a, d, s, l, x;
	XVisualInfo *vis = glXGetVisualFromFBConfig(dpy, fbc);
	if (!vis)
		return;
	glXGetFBConfigAttrib(dpy, fbc, GLX_SAMPLES, &smp);
	glXGetFBConfigAttrib(dpy, fbc, GLX_SAMPLE_BUFFERS, &bfrs);
	glXGetFBConfigAttrib(dpy, fbc, GLX_RED_SIZE, &r);
	glXGetFBConfigAttrib(dpy, fbc, GLX_GREEN_SIZE, &g);
	glXGetFBConfigAttrib(dpy, fbc, GLX_BLUE_SIZE, &b);
	glXGetFBConfigAttrib(dpy, fbc, GLX_ALPHA_SIZE, &a);
	glXGetFBConfigAttrib(dpy, fbc, GLX_DEPTH_SIZE, &d);
	glXGetFBConfigAttrib(dpy, fbc, GLX_STENCIL_SIZE, &s);
	glXGetFBConfigAttrib(dpy, fbc, GLX_LEVEL, &l);
	glXGetFBConfigAttrib(dpy, fbc, GLX_X_VISUAL_TYPE, &x);

	lprintf(DBG "%sSmpl"lBLD_"%02i"_lBLD"sb"lBLD_"%02i"_lBLD", "
			"r"lF_RED"%02i"_lF"g"lF_GRE"%02i"_lF"b"lF_BLUE"%02i"_lF
			"a"lBLD_"%02i"_lBLD"d"lF_MAG"%02i"_lF"s"lF_YELW"%02i"_lF
			"l"lF_WHI"%02i"_lF"x"lULN_"%02x"_lULN"\n",
			idbuf, smp, bfrs, r, g, b, a, d, s, l, x);
	XFree(vis);
}
static int load()
{
	glx_dpy = XOpenDisplay(dpy_name);
	if (!glx_dpy) {
		lprintf(ERR "Failed to open display "lBLD_"%s"_lBLD"\n",
				dpy_name);
		return -1;
	}

	int rv = 0;

	/* Verify version */
	int maj, min;
	if (!glXQueryVersion(glx_dpy, &maj, &min)
			|| maj < 1 || (maj == 1 && min < 3)) {
		lprintf(ERR "Incompatible glX version "lBLD_"%i.%i "_lBLD
				"(return your software to a museum)!\n",
				maj, min);
		rv = -2;
		goto exitpt;
	}

	/* Requested visual attributes */
	int visattr[] = {
		GLX_RENDER_TYPE, GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_DEPTH_SIZE, 8,
		GLX_ALPHA_SIZE, 0,
		GLX_DOUBLEBUFFER, True,
		None
	};

	/* Searching through FBConfigs */
	int scr = DefaultScreen(glx_dpy);
	int fbc_cnt;
	GLXFBConfig* fbc = glXChooseFBConfig(glx_dpy, scr, visattr, &fbc_cnt);
	if (!fbc) {
		rv = -3;
		lprintf(ERR "No acceptable framebuffer configurations.\n");
		goto exitpt;
	}
	lprintf(DBG "Found "lF_CYA"%i"_lF" glX framebuffer configs.\n", fbc_cnt);
	for (int i = 0; i < fbc_cnt; i++) {
		lprintFBConfig(glx_dpy, fbc[i], i);
	}
	glx_fbc = *fbc;
	XFree(fbc);

	XVisualInfo *vis = glXGetVisualFromFBConfig(glx_dpy, glx_fbc);

	/* Create colourmap */
	Window root = RootWindow(glx_dpy, scr);
	XSetWindowAttributes wattr = {
		.background_pixel = 0,
		.background_pixmap = None,
		.border_pixel = 0,
		.colormap = XCreateColormap(glx_dpy, root, vis->visual,
				AllocNone),
		.event_mask = StructureNotifyMask | ExposureMask
			| KeyPressMask | KeyReleaseMask
			| ButtonPressMask | ButtonReleaseMask
			| PointerMotionMask,
	};

	int w, h;
	pthread_mutex_lock(&win_mutex);
	w = win_width;
	h = win_height;
	pthread_mutex_unlock(&win_mutex);
	/* Create window */
	glx_win = XCreateWindow(glx_dpy, root, 0, 0, w, h,
			0, vis->depth, InputOutput, vis->visual,
			CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
			&wattr);
	XFree(vis);

	/* Name window */
	XStoreName(glx_dpy, glx_win, win_name ? win_name : "A Window Name");

	/* Open window */
	XMapWindow(glx_dpy, glx_win);

exitpt: ;

	if (rv <= -2) {
		XDestroyWindow(glx_dpy, glx_win);
	}
	/*if (rv) {
		if (rv < -3)
			glXDestroyContext(glx_dpy, ctx);
		XDestroyWindow(glx_dpy, glx_win);
		XCloseDisplay(glx_dpy);
	}*/

	/* testing
#include <GL/gl.h>
#include <unistd.h>
	for (int i = 0; i < 5; i++) {
		glClearColor(0.f, 0.f, 1.f / 5.f * i, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
		glXSwapBuffers(glx_dpy, glx_win);
		sleep(1);
	}
	*/
	return rv;
}

static int unload()
{
	XDestroyWindow(glx_dpy, glx_win);
	XCloseDisplay(glx_dpy);
	return 0;
}

static int optcb(int index, const char *optarg)
{
	assert(index >= 0 && index <= 2);
	switch (index) {
	case 0:
		assert(optarg != NULL);
		if (dpy_name != NULL)
			free(dpy_name);
		int oa_len = strlen(optarg) + 1;
		dpy_name = memcpy(malloc(oa_len), optarg, oa_len);
		break;
	case 1:
		assert(optarg != NULL);
		int w, h;
		int r = sscanf(optarg, "%ix%i", &w, &h);
		if (r == 0) {
			lprintf(WRN "Unexpected argument for -r.\n");
			return -1;
		} else if (r == 1) {
			pthread_mutex_lock(&win_mutex);
			win_width = w;
			win_height = (int) (w / 16.f * 9.f);
			pthread_mutex_unlock(&win_mutex);
		} else if (r == 2) {
			pthread_mutex_lock(&win_mutex);
			win_width = w;
			win_height = h;
			pthread_mutex_unlock(&win_mutex);
		}
		break;
	case 2:
		if (win_name)
			free(win_name);
		if (optarg != NULL) {
			int oa_len = strlen(optarg) + 1;
			win_name = memcpy(malloc(oa_len), optarg, oa_len);
		} else {
			win_name = NULL;
		}
		break;
	}
	return 0;
}

static struct optsection opts = {
	.label = "Window managment:",
	.callback = optcb,
	.opt_a = {
		{ ARG_REQUIRED, 'd', "display", "DISPLAY\t"
			"Set the display to open the root window on." },
		{ ARG_REQUIRED, 'r', "resolution", "WxH\t"
			"Set the resolution of the root window." },
		{ ARG_OPTIONAL, 't', "title", "NAME\t"
			"Set the name for root window." },
		{ 0, '\0', NULL, NULL },
	},
};

static int glx_window_mod_id = -1;
static void __init mod_init()
{
	struct ce_mod m = {
		.comment = "Window functionality via glX and X display server.",
		.def = "glX | root-window 0:2.13; glx-visual",
		.use = "",
		.load = load,
		.unload = unload,
	};
	glx_window_mod_id = ce_mod_add(&m);
	int rv = opt_add(ce_options, &opts);
	assert(rv >= 0);
}

static void __exit mod_exit()
{
	free(dpy_name);
	free(win_name);
	assert(glx_window_mod_id != -1);
	int rv = opt_rm(ce_options, &opts);
	assert(rv >= 0);
	ce_mod_rm(glx_window_mod_id);
}

