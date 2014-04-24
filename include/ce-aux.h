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
 * 	120	ce-opt.c ce_options
 *
 * 	130	ce-mod.c
 *
 * 	501	other components
 *
 *    65001	ce-mod.c root-mod exit
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

#endif /* ndef _CE_AUX_H */
