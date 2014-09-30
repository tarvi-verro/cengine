#ifndef _CE_MOD_H
#define _CE_MOD_H 0,2,13

/**
 * DOC: ce-mod.h
 * The functions in this header are defined to give modules a safe way to
 * avoid functionality conflicts, initialize and manage the lifetime of
 * modules or resources and handle dynamic loading of modules.
 *
 * For an indepth introduction, see Documentation/ce-mod-intro.txt.
 *
 */

/**
 * struct ce_mod - a module providing functionality to scenes
 * @comment:	some words describing your module
 * @def:	following format "[mod name]|[implements 1],[implements 2]..."
 * @use:	specify interfaces that the module needs, so associated
 *		module will be loaded; sadditionally prepend the functionality
 *		name with: '!' - incompatible, '#' - not immediately needed,
 *		use this to leave time-consuming initialisations towards the
 *		end, so if one fails, the init fails in less time, '&' - load
 *		only (immediately) after current mod
 * @load:	function that initializes the module or %NULL if given module
 *		doesn't require initialisation; the function should return
 *		negative if the module initialisation failed
 * @unload:	the module is no longer required, free up associated resources
 *
 * Calling @load after @unload must be valid.
 *
 * If multiple modules implement the same functionality, only one of them
 * will be allowed to be loaded at a given time.
 *
 * Use Interface Flag '#&' Combination:
 * Using a functionality with flags '#&' will allow the fcn to only load
 * after the current mod, and also pushes it to the end.
 */
struct ce_mod {
	const char *comment;
	const char *def;
	const char *use;
	int (*load)();
	int (*unload)();
};

/**
 * ce_mod_use() - initializes specified functionalities
 * @mod_id:	the module that requires these to be initialized, as returned
 *		by ce_mod_add()
 * @use:	modules that must be loaded before given function returns
 *		successfully, see &struct ce_mod for more accurate
 *		specification
 *
 * Main-level initialisation:
 * The program main() function is expected to have a mod with 'ce-main'
 * functionality, and call ce_mod_use() in that main() function to start
 * initialisation process. After the initialisation for all involved mods is
 * finished, this function returns and the main should call the specified
 * ce_main_ctrl function (provided by 'ce-main-ctrl').
 *
 * Return:	negative value on failure
 */
int ce_mod_use(int mod_id, const char* use);

/**
 * ce_mod_unuse() - specify functionality no longer needed
 * @mod_id:	the module that no longer requires given functionality
 * @unuse:	list of semi-colon separated functionality names
 *
 * Return:	negative on failure
 */
int ce_mod_unuse(int mod_id, const char* unuse);

/**
 * ce_mod_cleanup() - checks and unloads unnecessary modules
 */
void ce_mod_cleanup();

/**
 * ce_mod_add() - registers a module
 * @mod:	module to add
 *
 * Return:	added module's unique identifier on success, negative value on
 *		failure
 */
int ce_mod_add(const struct ce_mod *mod);

/**
 * ce_mod_rm() - remove a module
 * @mod_id:	identifier of the module to remove as returned by ce_mod_add()
 *
 * Return:	0 on success, negative on error (current implementation
 *		asserts all failure cases)
 */
int ce_mod_rm(int mod_id);
const char *ce_mod_strerr(int err);

#endif /* _CE_MOD_H */
