#ifndef _INPUT_H
#define _INPUT_H 0,2,13

/**
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
 */
enum {
	INPUT_KEY_NONE = 0,
	INPUT_KEY_MOUSE_LEFT = -1,
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
 * @INPUT_TYPE_PLANE:	trigger on two dimensional movement events like mouse
 *			movement (%INPUT_EVENT_PLANE)
 *
 * These values are OR-ed together at &struct input's %type field to indicate
 * acceptable event types for trigger.
 */
enum {
	INPUT_TYPE_KEY = 1 << 0,
	INPUT_TYPE_KEY_REPEAT = 1 << 2,
	INPUT_TYPE_FIRE = 1 << 3,
	INPUT_TYPE_PLANE = 1 << 4,
};

/**
 * enum - input event types
 * @INPUT_EVENT_PRESS:	key's state changing to down
 * @INPUT_EVENT_RELEASE:key's state changing to up
 * @INPUT_EVENT_FIRE:	an event of a stateless key fired
 * @INPUT_EVENT_PLANE:	two dimensional controller movement
 */
enum {
	INPUT_EVENT_PRESS = 1 << 0,
	INPUT_EVENT_RELEASE = 1 << 1,
	INPUT_EVENT_FIRE = 1 << 3,
	INPUT_EVENT_PLANE = 1 << 4,
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
