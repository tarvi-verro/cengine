#include "ce-aux.h"
#include "ce-log.h"
#include "ce-mod.h"
#include "ce-opt.h"

#include <assert.h>
#include <string.h>	/* memcmp, strlen */
#include <stdio.h>	/* printf */
#include <X11/Xlib.h>	/* Display */
#include <GL/glx.h>	/* glXCreateContext */

/* glX-visual mod, window.c */
extern Display *glx_dpy;
extern Window glx_win;
extern XVisualInfo *glx_vis;
extern GLXFBConfig glx_fbc;

static GLXContext ctx;


/*
 *
 */
typedef GLXContext (*glXCreateContextAttribsARBProc)
	(Display*, GLXFBConfig, GLXContext, Bool, const int*);

static int attribs_err = 0;
static int attribs_err_cb(Display *dpy, XErrorEvent *e)
{
	attribs_err = 1;
	return 0;
}

/**
 * ext_find() - is an extension in the extension string list
 * @ext_str:	extension list string
 * @target:	extension to look for
 *
 * Return:	0 if @target is not found in @ext_str, 1 if it is
 */
static int ext_find(const char *ext_str, const char *target)
{
	int target_len = strlen(target);
	int i, b;
	for (i = 1, b = 0; ext_str[i] != '\0'; i++) {
		if (ext_str[i] != ' ')
			continue;
		if (target_len == i - b
				&& !memcmp(ext_str + b, target, target_len))
			return 1;

		b = i + 1;
	}
	if (target_len == i - b
			&& !memcmp(ext_str + b, target, target_len))
		return 1;
	return 0;
}

static int load_ctx(int gl_major, int gl_minor)
{
	int scr = DefaultScreen(glx_dpy);
	const char *ext_str = glXQueryExtensionsString(glx_dpy, scr);
	assert(ext_str);

	if (!ext_find(ext_str, "GLX_ARB_create_context"))
		return -2;


	glXCreateContextAttribsARBProc glXCreateContextAttribsARB = NULL;
	glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
		glXGetProcAddressARB((const GLubyte *) "glXCreateContextAttribsARB");

	if (!glXCreateContextAttribsARB)
		return -3;

	/* Put in place error handle CreateContextAttribs failure */
	int (*oldhndl)(Display*, XErrorEvent*) =
		      XSetErrorHandler(attribs_err_cb);
	attribs_err = 0;

	int ctxattr[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, gl_major,
		GLX_CONTEXT_MINOR_VERSION_ARB, gl_minor,
		None,
	};
	ctx = glXCreateContextAttribsARB(glx_dpy, glx_fbc, 0, 1, ctxattr);

	oldhndl = XSetErrorHandler(oldhndl);
	assert(oldhndl == attribs_err_cb);

	/*ctx = glXCreateContext(glx_dpy, glx_vis, NULL, 1);
	if (!ctx) {
		lprintf(ERR "glXCreateContex failed\n");
		return -3;
	}*/
	if (attribs_err) {
		lprintf(ERR "GLX "lF_RED"%i"_lF"."lF_RED"%i"_lF" "
				"context initialisation failed.\n",
				gl_major, gl_minor);
		return -4;
	}
	glXMakeCurrent(glx_dpy, glx_win, ctx);
	return 0;
}

static int load_33()
{
	return load_ctx(3, 3);
}

static int load_21()
{
	return load_ctx(2, 1);
}

static int unload()
{
	glXDestroyContext(glx_dpy, ctx);
	return 0;
}

static int glx_ctx_21_mod_id = -1;
static int glx_ctx_33_mod_id = -1;
static void __init link()
{
	/* temporarily 'gl-context 3', soon will be updated to include
	 * the fact that 1.4 pipeline and 3.0 shader stuff exist
	 * alongside eachother for the mostpart */
	struct ce_mod m = {
		.comment = "GL Drawing context via glX.",
		.def = "glX-context | gl-context 2.1",
		.use = "glx-visual",
		.load = load_21,
		.unload = unload,
	};
	glx_ctx_21_mod_id = ce_mod_add(&m);

	m.def = "glX-context-modern | gl-context 3.3";
	m.load = load_33;
	glx_ctx_33_mod_id = ce_mod_add(&m);
}

static void __exit unlink()
{
	assert(glx_ctx_21_mod_id >= 0);
	assert(glx_ctx_33_mod_id >= 0);
	ce_mod_rm(glx_ctx_21_mod_id);
	ce_mod_rm(glx_ctx_33_mod_id);
}

