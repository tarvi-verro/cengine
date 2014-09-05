#ifndef _CE_AUX_LOG_H
#define _CE_AUX_LOG_H 0,2,13

/**
 * DOC: Log with graphics
 * The logging allows for using "\x1b[*m"-like escape sequences. It is
 * recommended to use them via the macros defined in xf-escg.h, for those
 * support is guaranteed.
 */
#include "xf-escg.h"
#include <stdio.h>

/**
 * enum log_file_flags - flags to use with log_file_add()
 * @LOGFILE_FILTER_SGR:	filters the escape sequences
 * @LOGFILE_AUTOCLOSE:	automatically close the file on log_file_rm()
 */
enum log_file_flags {
	LOGFILE_FILTER_SGR = 1 << 0,
	LOGFILE_AUTOCLOSE = 1 << 1,
};

/* ce-log.c */

/**
 * log_txt_file() - open a file for logs
 * @f:		an open file handle to start writing the logs to
 * @flags:	bitwise flags to modify how the file is handled and written to,
 * 		see &enum log_file_flags
 *
 * Return:	negative on failure, positive integer handle on success
 */
int log_txt_file_add(FILE *f, int flags);

/**
 * log_txt_file_rm() - close a file following logs
 * @hndl:	handle returned by log_txt_file() with follow
 *
 * Return:	negative on failure, the handle id on success
 */
int log_txt_file_rm(int hndl);

#endif /* _CE_AUX_LOG_H */
