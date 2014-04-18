#ifndef _CE_AUX_ARG_H
#define _CE_AUX_ARG_H 0,02,06

/* ce-arg.c */

/**
 * arg_push_a() - passes an array of arguments to callbacks
 * @argc:	amount of strings @args contains
 * @args:	array of arguments
 *
 * Return:	Number of arguments not recognized.
 */
int arg_push_a(int argc, const char **args);

/**
 * arg_push_str() - parses arguments and passes them to callbacks
 * @argstr:	string of arguments like they would've been written after the 
 * 		program name in a terminal
 *
 * This function works on top of arg_push_a().
 *
 * Return:	Number of arguments not recognized.
 */
int arg_push_str(const char *argstr);


#endif /* _CE_AUX_ARG_H */
