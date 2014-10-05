#include "ce-aux.h"
#include "ce-log.h"
#include "ce-opt.h"
#include "ce-mod.h"
#include "xf-strb.h"
#include <stddef.h> /* NULL */
#include <stdlib.h> /* exit(), EXIT_SUCCESS */
#include <stdint.h>
#include <assert.h>


int (*control)() = NULL;

static char *load = NULL;

static int optcb(int index, const char *optarg)
{
	assert(index == 0 && optarg != NULL);
	if (load) {
		lprintf(WRN "Only one --load string acceptable. "
				"Overwriting previous.\n");
		free(load);
	}
	size_t l = strlen(optarg) + 1;
	load = memcpy(malloc(l), optarg, l);
	return 0;
}
static struct optsection opts = {
	.label = "Main:",
	.callback = optcb,
	.opt_a = {
		{ ARG_REQUIRED, 'l', "load", "FCN\t"
			"Load functionality before giving control over." },
		{ 0, '\0', NULL, NULL },
	},
};

static int ce_main_mod_id = -1;
static void __init mod_init()
{
	int rv = opt_add(ce_options, &opts);
	assert(rv >= 0);
	struct ce_mod mod = {
		.comment = "Program start point was in ce-main.c.",
		.def = "ce-main-c 0:2.13 | ce-entry-pt 0:2.8; "
			"ce-exp+test; ce-exp[] 0:2.8; control$",
		.use = "",
		.load = NULL,
		.unload = NULL,
	};
	int modid = ce_mod_add(&mod);
	if (modid < 0) {
		lprintf(ERR "Failed to add cengine-main module: %s\n",
				ce_mod_strerr(modid));
		lputs(INF "Exiting.");
	}
	ce_main_mod_id = modid;
}

static void __exit mod_exit()
{
	free(load);
	assert(ce_main_mod_id != -1);
	ce_mod_rm(ce_main_mod_id);
	opt_rm(ce_options, &opts);
}

extern size_t ce_mod_memcnt();
extern size_t ce_log_memcnt();
int main(int argc, char * const *args)
{
	if (ce_main_mod_id == -1) {
		lputs(ERR "cengine-main mod not defined at main().");
		return EXIT_FAILURE;
	}
	int modid = ce_main_mod_id;

	lputs(INF "cengine-main reached.");

	opt_parse(ce_options, argc, args, 1);

	if (load) {
		int err = ce_mod_use(modid, load);
		if (err < 0)
			lprintf(ERR "Initializing --load string failed: %s\n",
					ce_mod_strerr(err));
	}

	if (control) {
		control();
	} else {
		lprintf(ERR "Nothing to give control over to.\n");
	}

	if (load) {
		ce_mod_unuse(modid, load);
		ce_mod_cleanup();
	}

#ifdef MEMCNT_ENABLED
	//memcnt_status(stderr);
#endif
	size_t mem_mod = ce_mod_memcnt();
	size_t mem_log = ce_log_memcnt();
	lprintf(INF "Memory usage report: ce-mod "lF_BLUE"%ti"_lF" "
			"+ ce-log "lF_BLUE"%ti"_lF" "
			"= "lF_BLUE"%ti"_lF".\n",
			mem_mod, mem_log, mem_mod + mem_log);
	return EXIT_SUCCESS;
}

