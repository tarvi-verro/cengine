#ifndef _INPUT_H
#define _INPUT_H 0,2,13

/**
 * TODO: rebindable keys, multiple default keys
 * DOC: input.h
 * Functionality for capturing keyboard, gamepad, mouse or other input
 * events.
 *
 * Note that this API is thread safe, and the callback (&struct
 * inputsection.%callback) ideally executes in a separate thread
 * (although some implementations might execute it during GL swapbuffer
 * function).
 */

/**
 * struct inputset - a set of inputs
 */
struct inputset;

/**
 * enum - input keys and triggers
 * @INPUT_KEY_NONE:	no key
 * @INPUT_KEY_MOUSE_OFF:mouse buttons' offset, where INPUT_KEY_MOUSE_OFF+0 is
 *			left mouse button, INPUT_KEY_MOUSE_OFF+1 middle mouse
 *			button, _OFF+3 scroll-up, _OFF+4 scroll-down etc
 * @INPUT_KEY_CLOSE:	triggered by the window manager (for example after the
 *			window "X" close button) (%INPUT_EVENT_FIRE)
 * @INPUT_KEY_POINTER:	input of a cursor pointer (%INPUT_EVENT_POINTER);
 * 			can't be combined with @INPUT_TYPE_MOTION, see below
 * @INPUT_KEY_RESIZE:	fired when the window is resized (%INPUT_EVENT_FIRE)
 * @INPUT_KEY_EXPOSE:	triggered when the window (re)appears
 *			(%INPUT_EVENT_FIRE)
 *
 */
enum {
	INPUT_KEY_NONE = 0,
	INPUT_KEY_MOUSE_OFF = -32,
	INPUT_KEY_MOTION = -51,
	INPUT_KEY_CLOSE = -61,
	INPUT_KEY_RESIZE = -62,
	INPUT_KEY_EXPOSE = -63,
};

/**
 * enum - input types
 * @INPUT_TYPE_KEY:	trigger type key, one that has two states (up & down),
 *			and is triggered when its state changes
 *			(%INPUT_EVENT_PRESS, %INPUT_EVENT_RELEASE)
 * @INPUT_TYPE_KEY_REPEAT:modifier for @INPUT_TYPE_KEY to also trigger on the
 *			automatic key repeat event (ignored otherwise)
 * @INPUT_TYPE_FIRE:	trigger on a fire-only event (like mouse scroll), that
 *			has no state (%INPUT_EVENT_FIRE)
 * @INPUT_TYPE_MOTION:	trigger on two dimensional movement events like mouse
 *			movement (%INPUT_EVENT_MOTION)
 * @INPUT_TYPE_POINTER:	input of a cursor pointer (%INPUT_EVENT_POINTER);
 *			can't be combined with @INPUT_TYPE_MOTION, see below
 *
 * These values are OR-ed together at &struct input's %type field to indicate
 * acceptable event types for trigger.
 *
 * Plane and pointer inputs:
 * Mouse events cannot at the same time provide the grabbed mouse inputs
 * (@INPUT_TYPE_MOTION) and visual pointer controlled events
 * (@INPUT_TYPE_POINTER) so a &struct inputset can only listen for one at a
 * time.
 *
 */
enum {
	INPUT_TYPE_KEY = 1 << 0,
	INPUT_TYPE_KEY_REPEAT = 1 << 2,
	INPUT_TYPE_FIRE = 1 << 3,
	INPUT_TYPE_MOTION = 1 << 4,
	INPUT_TYPE_POINTER = (1 << 4) | (1 << 5),
};

/**
 * enum - input event types
 * @INPUT_EVENT_PRESS:	key's state changing to down
 * @INPUT_EVENT_RELEASE:key's state changing to up
 * @INPUT_EVENT_FIRE:	an event of a stateless key fired
 * @INPUT_EVENT_MOTION:	two dimensional controller movement; accompanied by
 * 			x and y coordinates specifying the amount of movement
 * @INPUT_EVENT_POINTER:pointer location change; accompanied by x and y
 *			coordinates relative to the window top-left
 */
enum {
	INPUT_EVENT_PRESS = 1 << 0,
	INPUT_EVENT_RELEASE = 1 << 1,
	INPUT_EVENT_FIRE = 1 << 3,
	INPUT_EVENT_MOTION = 1 << 4,
	INPUT_EVENT_POINTER = (1 << 4) | (1 << 5),
};

/**
 * struct input - a single input descriptor
 * @name:	name and description for the input in the format:
 *		"[name]:[desc]"
 * @defkey:	default trigger key, either unicode value that represents the key
 *		(while L'õ' is possible, most people probably will not find a
 *		key for this, also multiple values may map to the same key), or
 *		one of INPUT_KEY_*
 * @types:	accepted input methods INPUT_TYPE_* joined by bitwise-OR
 */
struct input {
	const char *name;
	int defkey;
	unsigned char types;
};

/**
 * struct in_trig - describes an input hook
 * @callback:	the function to be called when one of the @inputs_a is
 *		fired; the param %n corresponds to the index of @inputs_a;
 *		note that this is called immediately on input, on a
 *		thread dedicated to listening input - so don't waste its
 *		time more than you absolutely have to
 * @inputs_a:	array of &struct input, terminated by
 *		{ .name = %NULL, .defkey = %0 }
 *
 */
struct inputsection {
	int (*callback)(int n, int event, int x, int y);
	struct input inputs_a[];
};

/**
 * input_add() - adds input trigger to given set of inputs
 * @set:	set to add the inputs to
 * @trig:	the trigger that would handle given events
 *
 * Return:	Negative on error, on success returns an identifier that
 *		is used for input_rm() later.
 */
int input_add(struct inputset *set, const struct inputsection *trig);

/**
 * input_rm() - removes previously added input trigger from set
 * @set:	set to remove the trigger from
 * @in_id:	the identifier as returned by input_add()
 *
 * Return:	Negative on error, %0 on success.
 */
int input_rm(struct inputset *set, int in_id);

/**
 * input_set_handler() - sets active inputset
 * @set:	the set of inputs that will capture all the input events, or
 *		%NULL to reset to none
 *
 * Return:	The function returns previous active inputset, which
 *		should be reset once the inputset no longer needs to
 *		capture the keys.
 */
struct inputset *input_set_active(struct inputset *set);

/**
 * input_set_create() - allocates an input set
 *
 * Return:	Allocated input set that must be free'd with
 *		input_set_destroy().
 */
struct inputset *input_set_create();

/**
 * input_set_destroy() - free's memory associated with a set
 * @set:	set to free
 *
 * When the set is destroyed, it must not be active, nor be set active
 * after. Otherwise undefined consequences will follow.
 *
 * Return:	Negative on failure, %0 on success.
 */
int input_set_destroy(struct inputset *set);

#endif
