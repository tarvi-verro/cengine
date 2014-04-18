#ifndef _CE_AUX_LOG_H
#define _CE_AUX_LOG_H 0,02,06

/**
 * DOC: Style sequences
 * The logging allows for using "\x1b[*m"-like escape sequences. It is 
 * recommended to use the following, for which support is gauranteed:
 *
 * %LRESET - resets styles
 *
 * %LBOLD, %LBOLD_OFF - bold style
 *
 * %LUNDRL, %LUNDRL_OFF - underline text
 *
 * Foreground colours: %LFG_BLACK, %LFG_RED, %LFG_GREEN, %LFG_YELLOW, 
 * %LFG_BLUE, %LFG_MAGNETA, %LFG_CYAN, %LFG_WHITE and %LFG_DEF for default
 * colour.
 *
 * Background colours: %LBG_BLACK, %LBG_RED, %LBG_GREEN, %LBG_YELLOW, 
 * %LBG_BLUE, %LBG_MAGNETA, %LBG_CYAN, %LBG_WHITE and %LBG_DEF for default
 * colour.
 *
 * Don't forget to end all started ranges or they might colour next logs.
 */
#define LRESET		"\x1b[0m"
#define LBOLD		"\x1b[1m"
#define LBOLD_OFF	"\x1b[21m"
#define LUNDRL		"\x1b[4m"
#define LUNDRL_OFF	"\x1b[24m"
/* colours */
#define LFG_BLACK	"\x1b[30m"
#define LFG_RED		"\x1b[31m"
#define LFG_GREEN	"\x1b[32m"
#define LFG_YELLOW	"\x1b[33m"
#define LFG_BLUE	"\x1b[34m"
#define LFG_MAGNETA	"\x1b[35m"
#define LFG_CYAN	"\x1b[36m"
#define LFG_WHITE	"\x1b[37m"
#define LFG_DEF		"\x1b[39m"

#define LBG_BLACK	"\x1b[40m"
#define LBG_RED		"\x1b[41m"
#define LBG_GREEN	"\x1b[42m"
#define LBG_YELLOW	"\x1b[43m"
#define LBG_BLUE	"\x1b[44m"
#define LBG_MAGNETA	"\x1b[45m"
#define LBG_CYAN	"\x1b[46m"
#define LBG_WHITE	"\x1b[47m"
#define LBG_DEF		"\x1b[49m"

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
