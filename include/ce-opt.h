#ifndef _CE_OPT_H
#define _CE_OPT_H 0,2,11
/**
 * DOC: ce-opt.h
 * A command-line argument based modular option mechanism.
 *
 * The input syntax of argc/v is that of getopt() with %POSIXLY_CORRECT.
 *
 * Triggering options happens via opt_parse().
 */

/**
 * struct optset - a set of options
 */
struct optset;

/* ce-opt.c */
/**
 * DOC: ce_options
 * The global options set.
 */
extern struct optset *ce_options;

enum {
	ARG_NONE = 0,
	ARG_REQUIRED = 1,
	ARG_OPTIONAL = 2,
};

/**
 * struct opt - representation of an option
 * @has_arg:	%ARG_NONE, %ARG_REQUIRED or %ARG_OPTIONAL
 * @name_short:	character representation of the option, matched when the
 * 		command line argument started with '-'
 * @name_long:	string representation of the option, matched when the command
 * 		line argument started with '--'
 * @help:	a short description of the option; if @has_arg, this must be
 * 		prepended with 'ARGNAME\t'
 *
 * One of @name_short and @name_long may be %NULL (unset).
 */
struct opt {
	unsigned char has_arg;
	char name_short;
	const char *name_long;
	const char *help;
};

/**
 * struct optsection - a group of options with a label
 * @label:	the label unifying these options
 * @callback:	function to call when one of these options has been parsed;
 * 		return any non-null value to print out the help string for
 * 		given index
 * @opt_a:	array of options, terminated by { %0, %0, %NULL, %NULL }
 */
struct optsection {
	const char *label;
	int (*callback)(int index, const char *optarg);

	struct opt opt_a[];
};

/**
 * opt_add() - add options to a set
 * @set:	the option target 'domain'
 * @sect:	the &struct optsection to add
 *
 * Note that the lifespan of memory pointed to by @sect must last at least
 * until opt_rm() removes it.
 *
 * Return:	Negative on failure: %-1 on name conflict.
 */
int opt_add(struct optset *set, struct optsection *sect);

/**
 * opt_rm() - remove a section of options
 * @set:	the option set to remove from
 * @sect:	section to remove
 *
 * Return:	Negative on failure.
 */
int opt_rm(struct optset *set, struct optsection *sect);

/**
 * opt_parse() - parse cmd line arguments for options
 * @set:	the collection of target options
 * @argc:	argument count in @argv
 * @argv:	argument vector
 * @offset:	the first @argv to parse, skipping all the previous; for
 * 		main() arguments, this would be set to %1 to skip the program
 * 		name
 *
 * The @argc and @argv are typically found passed to program main() function.
 *
 * Return:	Negative on failure, index of @argv containing the first
 * 		non-option or @argc if only options were found.
 */
int opt_parse(struct optset *set, int argc, char * const argv[], int offset);

/**
 * optarg_bool() - check given string against acceptable boolean values
 * @optarg:	string to check
 *
 * The purpose is to have uniform argument values.
 *
 * Return:	%1 if @optarg is a %true value, %0 if %false value,
 * 		%-1 if it is yellow and %-2 if @optarg is an empty string
 * 		or %NULL.
 */
int optarg_bool(const char *optarg);

#endif
