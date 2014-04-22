#include <ce-aux.h>
#include <ce-log.h>
#include <ce-arg.h>
#include <ce-mod.h>
#include <stddef.h> /* NULL */
#include <stdlib.h> /* exit(), EXIT_SUCCESS */
#include <stdint.h>
#include <assert.h>

static int ce_main_mod_id = -1;
static void __init mod_init()
{
	struct ce_mod mod = {
		.comment = "Program start point was in ce-main.c.",
		.def = "ce-main-c 0:2.8 | ce-entry-pt 0:2.8; "
			"ce-exp+test; ce-exp[] 0:2.8",
		.use = "ce-window",
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
	assert(ce_main_mod_id != -1);
	ce_mod_rm(ce_main_mod_id);
}

extern size_t ce_mod_memcnt();
extern size_t ce_log_memcnt();
int main(int argc, const char **args)
{
	if (ce_main_mod_id == -1) {
		lputs(ERR "cengine-main mod not defined at main().");
		return EXIT_FAILURE;
	}
	int modid = ce_main_mod_id;

	lputs(INF "cengine-main reached.");
	log_stderr_threshold(DBG);
	lputs(INF "Setting default log file './log'.");
//log_txt_file("./log", 1, 1, 1, 0);



	arg_push_a(argc - 1, args + 1);

	int err = ce_mod_use(modid, "");
	if (err < 0)
		lprintf(ERR "Initializing ce-window failed: %s\n", 
				ce_mod_strerr(err));

#ifdef MEMCNT_ENABLED
	//memcnt_status(stderr);
#endif
	size_t mem_mod = ce_mod_memcnt();
	size_t mem_log = ce_log_memcnt();
	lprintf(INF "Memory usage report: ce-mod "lF_BLUE"%ti"_lF" "
			"+ ce-log "lF_BLUE"%ti"_lF" "
			"= "lF_BLUE"%ti"_lF".\n",
			mem_mod, mem_log, mem_mod + mem_log);
	//fprintf(stderr, "OKOK\n");
	/*lputs(DBG "wtfing");
	char *buf = malloc(72); // 57 to 72 ??? * /
	free(buf);
	lputs(DBG "wtf"); */
	return EXIT_SUCCESS;
}

