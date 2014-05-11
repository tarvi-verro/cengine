#ifndef _CE_MOD_H
#define _CE_MOD_H 0, 2, 8

/**
 * DOC: api for modules to handle changes in dependencies
 * DOC:	Example changes:
 *	scene is switched
 *
 *	scenebuilder is unloaded/loaded
 *
 *	streamer's files are dropped as scene is switched, though some might
 *	stay
 *
 *	input methods are switched
 * DOC:	Constants
 *	menu mod (ESC settings and what have you)
 *
 *	console
 * DOC: How should changes happen? 0.2.08
 * .def = "glfw-ce:2.7.8 | glfw-less:2.7.8; ge-window; ce-input; ce-loop"
 * .comment = "wrapper to provide cengine functionality"
 * .use = "&glfw-sth" // to include above mod to be initialized only after
 *		given mod
 *
 * .def = "glfw{2.7.8} | glfw:2.7.8; glfw-less:2.7.8"
 * .comment = "glfw functionality, this doesn't include "
 *
 * .def = "ce-main 0:2.8 | ce-main 0:2.8; ce-main-ctrl$; ce-main-reload$;"
 * .use = "ce-main-ctrl" * cant be '-' sep. when def missing *
 *
 * .def = "ce-frame-loop | ce-main-ctrl=frame-loop"
 *
 * .def = "glx-window | ce-window"
 * .use = "ce-input;"
 * .comment = "uses the glx api to provide a window"
 *
 * .def = "ge-effect 0:2 | OpenGL 3:3; ge-effect[] 0:2; ge-effect+sphere"
 *
 * .def = "ge-effect+particle-fire"
 * .use = "OpenGL 3:3; ge-effect 0:2"
 *
 * .def = "ce-scene;"
 * .use = "ge-effect+particle-{sparks,fire,smoke}; ce-main-ctrl=frame-loop"
 *
 * .def = "alsa-sound | ce-sound"
 *	ce_mod_rq()
 *	ce_mod_rm()
 *
 *	ce_mod_rq("ce-effect+{sparks,sun,smoke,airdistortion};ce-window")
 *
 * It is only required to specify the outermost extension.
 */
/**
 * struct ce_mod - a module providing functionality to scenes
 * @comment:	some words describing your module
 * @def:	following format "[mod name]|[implements 1],[implements 2]..."
 * @use:	modules that must be loaded before given mod's @load can be
 *		called; additionally prepend the implementation with: '!' -
 *		incompatible, '#' - load by the end of init process (by
 *		loads it after everything else) - you can use this to
 *		push time-consuming initialisations towards the end, so if one
 *		fails, the init fails in less time, '&' - load immediately
 *		after current mod
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
 * Use fcn multiple flags:
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
 * ce_main_ctrl function(provided by 'ce-main-ctrl').
 *
 * Each time this function is called with @mod_id of the 'ce-main'
 * functionality, all the previously required mods will be dropped.
 *
 *
 * Return:	negative value on failure
 */
int ce_mod_use(int mod_id, const char* use);

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
