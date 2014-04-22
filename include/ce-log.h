#ifndef _CE_AUX_LOG_H
#define _CE_AUX_LOG_H 0,02,06

/**
 * DOC: Log with graphics
 * The logging allows for using "\x1b[*m"-like escape sequences. It is
 * recommended to use them via the macros defined in xf-escg.h, for those
 * support is guaranteed.
 */
#include "xf-escg.h"

/* ce-log.c */
/**
 * log_stderr_threshold() - set level at which log goes to stderr
 * @lvlmcro:	level macro, one of %DBG, %TXT, %INF, %WRN, %ERR
 *
 * @lvlmacro and all lower levels(%ERR(1) < %INF(3)) will be written to %stderr.
 *
 * This defaults to %WRN.
 */
void log_stderr_threshold(const char *lvlmcro); 

/**
 * log_debug() - enable or disable DBG logs
 * @enable:	%true if messages prepended with %DBG will be logged, %false to
 * 		have them be ignored
 */
void log_debug(int enable);

/** 
 * log_txt_file() - open a file for logs
 * @file:	system path to open
 * @clear:	whether or not to clear the file prior to use
 * @copy:	should log history(since the execution) be copied over
 * @follow:	keep the file open and append log as they are printed?
 * @filter_esc:	should ansi escape codes like Set Graphics Rendering be 
 * 		filtered before writing logs to file
 *
 * Return:	negative on failure, positive integer handle on success or
 * 		0 when follow was false.
 */
int log_txt_file(const char *file, int clear, int copy, int follow, int filter_esc);

/**
 * log_txt_file_rm() - close a file following logs
 * @hndl:	handle returned by log_txt_file() with follow
 *
 * Return:	0 on success. 
 *
 * 		1 if such handle is already closed or was never obtained
 */
int log_txt_file_rm(int hndl);

#endif /* _CE_AUX_LOG_H */
