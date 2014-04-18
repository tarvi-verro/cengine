#include "ce-aux.h"
#include "ce-mod.h"
#include <assert.h>

static int load()
{
	lputs(INF "glx-window load called");
	return 0;
}
static int unload()
{
	lputs(INF "glx-window unload called");
	return 0;
}

static int glx_window_mod_id = -1;
static void __init mod_init()
{
	struct ce_mod m = {
		.comment = "Window functionality via glX and POSIX.",
		.def = "glX | ce-window 0:2.6",
		.use = "",
		.load = load,
		.unload = unload,
	};
	glx_window_mod_id = ce_mod_add(&m);
}
static void __exit mod_exit()
{
	assert(glx_window_mod_id != -1);
	ce_mod_rm(glx_window_mod_id);
}

