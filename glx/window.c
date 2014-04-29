#include "ce-aux.h"
#include "ce-log.h"
#include "ce-mod.h"
#include "ce-opt.h"

#include <assert.h>
#include <string.h>	/* memcpy */
#include <stdlib.h>	/* malloc */
#include <X11/Xlib.h>	/* Display */
#include <GL/glx.h>	/* glXCreateContext */

static char *dpy_name = NULL;
static char *win_name = NULL;
static Display *dpy = NULL;
static GLXContext ctx;
static int width = 320;
static int height = 180;
static Window win;

static int load()
{
	lputs(INF "glx-window load called");
	dpy = XOpenDisplay(dpy_name);
	if (!dpy) {
		lprintf(ERR "Failed to open display "lBLD_"%s"_lBLD"\n",
				dpy_name);
		return -1;
	}

	/* Creating a glx window */
	int scr = DefaultScreen(dpy);
	Window root = RootWindow(dpy, scr);

	int visattr[] = {
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DEPTH_SIZE, 1,
		GLX_DOUBLEBUFFER,
		None
	};
	XVisualInfo *visinfo = glXChooseVisual(dpy, scr, visattr);
	if (!visinfo) {
		lprintf(ERR "Failed to choose visuals.\n");
		return -2;
	}

	XSetWindowAttributes xwattr = {
		.background_pixel = 0,
		.border_pixel = 0,
		.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone),
		.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask,
	};
	win = XCreateWindow(dpy, root, 0, 0, width, height,
			0, visinfo->depth, InputOutput, visinfo->visual,
			CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
			&xwattr);
	XSizeHints hints = {
		.x = 0,
		.y = 0,
		.width = width,
		.height = height,
		.flags = USSize | USPosition,
	};
	XSetNormalHints(dpy, win, &hints);
	XSetStandardProperties(dpy, win, win_name ? win_name : "A Window Name",
			"A Window Name Two", None, NULL, 0, &hints);

	int rv = 0;
	ctx = glXCreateContext(dpy, visinfo, NULL, 1);
	if (!ctx) {
		lprintf(ERR "glXCreateContex failed\n");
		rv = -3;
		goto exitpt;
	}

	/* Open window */
	XMapWindow(dpy, win);
	glXMakeCurrent(dpy, win, ctx);


exitpt: ;
	XFree(visinfo);
	if (rv) {
		if (rv < -3)
			glXDestroyContext(dpy, ctx);
		XDestroyWindow(dpy, win);
		XCloseDisplay(dpy);
	}

	/* testing
#include <GL/gl.h>
#include <unistd.h>
	for (int i = 0; i < 5; i++) {
		glClearColor(0.f, 0.f, 1.f / 5.f * i, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
		glXSwapBuffers(dpy, win);
		sleep(1);
	}
	*/
	return rv;
}
static int unload()
{
	lputs(INF "glx-window unload called");
	glXDestroyContext(dpy, ctx);
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
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
			width = w;
			height = (int) (w / 16.f * 9.f);
		} else if (r == 2) {
			width = w;
			height = h;
		}
	case 2:
		if (win_name)
			free(win_name);
		if (optarg != NULL) {
			int oa_len = strlen(optarg) + 1;
			win_name = memcpy(malloc(oa_len), optarg, oa_len);
		} else {
			win_name = NULL;
		}
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
		.comment = "Window functionality via glX and POSIX.",
		.def = "glX | root-window 0:2.13",
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
	assert(glx_window_mod_id != -1);
	int rv = opt_rm(ce_options, &opts);
	assert(rv >= 0);
	ce_mod_rm(glx_window_mod_id);
}

