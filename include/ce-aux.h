#ifndef _CE_AUX_H
#define _CE_AUX_H 0,02,06
/* some basic functionality */

/**
 * DOC: __init & __exit
 * These two function attributes can be used to run setup functions for listing
 * your module components for use in program.
 *
 * DOC: constructor & destructor priority table
 * 	000-100	system implementation functionality
 *
 * 	101-500	auxiliary components 
 *
 *	101	memcnt.c
 *
 * 	110	ce-log.c lprintf(), lputs() and log_* fncs
 *
 * 	120	ce-arg.c arg_* fncs
 *
 * 	130	ce-mod.c
 *
 * 	501+	other components
 */
#define __init __attribute__ ((constructor(501)))
#define __exit __attribute__ ((destructor(501)))


/* ce-log.c */
/** 
 * DOC: Logging introduction 
 * Each line given to log should have a log level prepended to it. Don't use
 * cryptic numbers for this, but rather given constants.
 *
 * If extending to a new line will considerably improve readability of given 
 * log(think formatted data below eachother), prepend each extended line with 
 * %EXT.
 *
 * DOC: Logging levels
 * %DBG - information only useful to the programmer
 * 
 * %TXT - user input, messages to user
 *
 * %INF - operational info - module (un)loaded and whatnot
 *
 * %WRN - system might not work as user expects
 *
 * %ERR - bug imminent, probable unstability
 *
 */
#define QUOT(sth) #sth
#define STR(sth) QUOT(sth)
#define _FI __FILE__ "+" STR(__LINE__) ":"
#define DBG _FI "5:"
#define TXT _FI "4:"
#define INF _FI "3:"
#define WRN _FI "2:"
#define ERR _FI "1:"

/**
 * lprintf() - print a formatted message to log
 * @format:	printf-styled format string
 * @...:	arguments pointed to by @format
 *
 * Return:	characters written to log
 */
int lprintf(const char *format, ...)
	__attribute__((format(printf,1,2)));

/**
 * lputs() - print a string message to log
 * @s:		string to log(newline carriage will be appended to this)
 *
 * Return:	characters written to log
 */
int lputs(const char *s);


/* ce-arg.c */
/**
 * DOC: Argument introduction
 * To share arguments between multiple modules, each module that would expect
 * input arguments, can register a callback function for receiving arguments. 
 *
 * Any main() functions are expected to call arg_push_a() to push the arguments
 * (defined in ce-arg.h).
 *
 * Unlike conventional arguments passed to main(), this system allows for new
 * arguments to be added at run-time.
 *
 * To list a callback, use arg_callb_add() and to remove it once your module is
 * being unloaded, arg_callb_rm(). These two functions should be called from
 * functions marked with the attributes %__init and %__exit respectively. This 
 * ensures that your module will work as a plug-in and won't horribly crash the
 * entire program when an argument is passed to a registered function that's 
 * been since unloaded.
 */

/**
 * arg_callb_add() - add a callback to receive the main-like arguments
 * @cb:		callback that will be called when an argument is received, 
 * 		this function should return the amount of arguments
 * 		captured(0 if none so the current will also be checked by other
 * 		callbacks, 1 if only the given argument was captured, and 2 or 
 * 		more if you expected extra arguments and verified them using
 * 		arg_peek())
 */
void arg_callb_add(int (*cb)(const char *arg, int argsize));

/**
 * arg_callb_rm() - remove a previously added callback function
 * @cb:		callback to remove
 *
 * Return:	0 on success, any non-zero integer on failure
 *
 * 		1 when such callback is not listed
 */
int arg_callb_rm(int (*cb)(const char *, int));

/**
 * arg_peek() - peek forward next arguments(for arg callbacks only)
 * @forw:	how many iterations from current to get the argument from
 *
 * Return:	%NULL if there's no argument at current + @forw,
 * 		argument otherwise
 */
const char *arg_peek(int forw);

/**
 * arg_bool() - check given string against a set of acceptable boolean values
 * @arg:	string to check
 *
 * Return:	1 if @arg is "true"-like, 0 if "false"-like or -1 if it its 
 * 		yellow or -2 if nothing's passed.
 */
int arg_bool(const char *arg);

#endif /* ndef _CE_AUX_H */
