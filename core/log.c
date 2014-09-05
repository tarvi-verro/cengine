#include <stdarg.h> /* va_list */
#include <stdio.h>
#include "xf-strb.h"
#include "ce-aux.h"
#include "ce-log.h"
#include "xf-escg.h"
#include <stdbool.h>
#include <time.h>
#define __USE_UNIX98 1
#include <pthread.h>

/**
 * DOC: log-pipeline
 * lputs()/lprintf() -> log_raw_process() -> log_raw_push()
 * -> log_argcb() -> log_txt_push()
 */

/* log_raw_push() calls these */
static void (**raw_callb_a)(const char *str, int length);
static int raw_callb_length = 0;
static int raw_callb_size = 1;
void log_raw_listen_add(void (*callb)(const char *str, int length));
int log_raw_listen_rm(void (*callb)(const char *str, int length));

static pthread_key_t lraw_bufs;
static void lraw_bufs_cleanup(void *arg);

/* misc */
static time_t logstart;

static void logfile_rmall();
static int logfile_add(FILE *f, int flags);
static int logfile_rm(int id);
static void logfile_callback(const char *str, int length);

struct logfile {
	unsigned int flags;
	FILE *f;
	pthread_mutex_t wrlock;
};

static struct logfile *lfile_a = NULL;
static int lfile_length = 0;
static int lfile_size = 3;
static int lfile_sgr_filter_users = 0;
static pthread_rwlock_t lfile_rwlock = PTHREAD_RWLOCK_INITIALIZER;

/**
 * A buffer for processing raw log messages before they will be written to
 * stdout/stderr and specified files.
 */
static __thread struct xf_strb *thbuf = NULL;
/**
 * A buffer for processing logfile messages before they'll be written to
 * respective files. Constructed and destructed at the same place as thbuf.
 */
static __thread struct xf_strb *lfbuf = NULL;

/*
 * Standard output select.
 */
static int logstd_id = -1;
static bool logstderr = true;

size_t ce_log_memcnt()
{
	size_t cnt = 0;
	if (raw_callb_a)
		cnt += raw_callb_size * sizeof(raw_callb_a[0]);
	return cnt;
}

__attribute__((constructor(110))) static void log_init()
{
	/* raw */
	raw_callb_a = malloc(sizeof(raw_callb_a[0]) * raw_callb_size);
	logstart = time(NULL);
	pthread_key_create(&lraw_bufs, lraw_bufs_cleanup);
	/* txt */
	lfile_a = malloc(sizeof(lfile_a[0]) * lfile_size);
	log_raw_listen_add(logfile_callback);
	logstd_id = logfile_add(logstderr ? stderr : stdout, 0);
	lputs(INF "Logging initialized.");
}

__attribute__((destructor(110))) static void log_exit()
{
	lputs(INF "Logging end reached.");
	/* txt */
	logfile_rmall();
	log_raw_listen_rm(logfile_callback);
	pthread_rwlock_wrlock(&lfile_rwlock);
	free(lfile_a);
	lfile_length = 0;
	lfile_a = NULL;
	pthread_rwlock_unlock(&lfile_rwlock);
	/* raw */
	pthread_key_delete(lraw_bufs);
	free(raw_callb_a);
}

/**
 * log_raw_listen_add() - listen in on logs
 * @callb:	callback to call when new logs are appended
 *
 * A line in the log follows the format - "3f:origin.c~3:5:Hello world!\n".
 * The "3f" is hexadecimal encoded seconds since the beginning of the program,
 * "ce-origin.c~3" is the origin of the log, "5" stands for error level(%DBG)
 * where a smaller number is of more importance or a more important message,
 * these follow the format specified in ce-aux.h.
 *
 * Note that the origin part may be overridden to anything that doesn't contain
 * '\0' and ':' characters.
 */
void log_raw_listen_add(void (*callb)(const char *str, int length))
{
	if (raw_callb_length + 1 > raw_callb_size) {
		raw_callb_size *= 2;
		raw_callb_a = realloc(raw_callb_a, sizeof(raw_callb_a[0])
				* raw_callb_size);
	}
	raw_callb_a[raw_callb_length] = callb;
	raw_callb_length++;
}

/**
 * log_raw_listen_rm() - remove a listening callback
 * @callb:	callback to remove
 *
 * Return:	0 on success
 *
 *		1 when such callback is not listed
 */
int log_raw_listen_rm(void (*callb)(const char *str, int length))
{
	for (int i = 0; i < raw_callb_length; i++) {
		if (raw_callb_a[i] != callb) continue;
		/* 010 */
		memmove(raw_callb_a + i, raw_callb_a + i + 1,
				sizeof(raw_callb_a[0])
				* (raw_callb_length - i - 1));
		raw_callb_length--;
		return 0;
	}
	return 1;
}

/**
 * log_raw_push() - passes the new logs to log_raw_readcb fncs
 * @s:		the lines to push, the each line beginning in the string must
 *		start with a header ('time:file+line:lvl:') and the string must
 *		end with a newline character
 * @length:	the length of @s to push, ending with a '\n' rather than a
 *		%NULL char
 */
static void log_raw_push(const char *s, int length)
{
	for (int i = 0; i < raw_callb_length; i++)
		raw_callb_a[i](s, length);
}

/**
 * log_raw_process() - processes the newly added lines
 * @inp_buf:	line(s) to append to the log, note that the contents of the
 *		strbuf may change
 *
 * Adds timestamps to the lines and ensures line header consistency and then
 * pushes the resulting lines to the log.
 */
static void log_raw_process(struct xf_strb *msg)
{
	int i = 0, line = 0;
	int e;
	for (i += 0; i < msg->length - 1; i++) {
		if (msg->a[i] != '\n')
			continue;
		/* Check the line header */
		for (e = line; e < i && msg->a[e] != ':'; e++); /* file/line */
		time_t ct = time(NULL) - logstart;
		if (msg->a[e] == ':' && msg->a[e + 1] >= '1'
				&& msg->a[e + 1] <= '5'
				&& msg->a[e + 2] == ':') {
			i += xf_strb_insertf(msg, line, "%llx:",
					(long long unsigned) ct);
		} else {
			i += xf_strb_insertf(msg, line,
					"%llx:"DBG"((missing log line header))\n"
					"%llx:unknown+33:2:",
					(long long unsigned) ct,
					(long long unsigned) ct);
		}
		line = i + 1;
		assert(msg->a[i] == '\n'); /**/
	}
	if (!line)
		return;
	log_raw_push(msg->a, line);
	memmove(msg->a, msg->a + line, msg->length - line);
	msg->length = msg->length - line;
}

static void lraw_bufs_cleanup(void *arg)
{
	assert(arg == thbuf);
	struct xf_strb *bufs = arg;
	xf_strb_destruct(bufs);
	xf_strb_destruct(bufs + 1);
	free(bufs);
}

int lprintf(const char *format, ...)
{
	if (!thbuf) {
		void *val = pthread_getspecific(lraw_bufs);
		assert(!val);
		struct xf_strb *bufs = malloc(sizeof(struct xf_strb) * 2);
		thbuf = bufs;
		lfbuf = bufs + 1;
		xf_strb_construct(thbuf, 24);
		xf_strb_construct(lfbuf, 24);
		assert(thbuf->a);
		pthread_setspecific(lraw_bufs, bufs);
	}
	va_list l;
	va_start(l, format);
	int r = xf_strb_vappendf(thbuf, format, l);
	va_end(l);

	log_raw_process(thbuf);
	return r;
}

int lputs(const char *str)
{
	if (!thbuf) {
		void *val = pthread_getspecific(lraw_bufs);
		assert(!val);
		struct xf_strb *bufs = malloc(sizeof(struct xf_strb) * 2);
		thbuf = bufs;
		lfbuf = bufs + 1;
		xf_strb_construct(thbuf, 24);
		xf_strb_construct(lfbuf, 24);
		assert(thbuf->a);
		pthread_setspecific(lraw_bufs, bufs);
	}
	int c = xf_strb_append(thbuf, str);
	c += xf_strb_append(thbuf, "\n");

	log_raw_process(thbuf);

	return c;
}


/* handle some output methods */
static int err_thres =
#ifdef NDEBUG
'2'; /* default WRN */
#else
'5'; /* debug DBG */
#endif

void log_stderr_threshold(const char *lvlmcro)
{
	assert(lvlmcro != NULL);
	int offs = sizeof(DBG) - 3;
	lvlmcro += offs;
	assert(*lvlmcro >= '1' && '5' >= *lvlmcro);
	err_thres = *lvlmcro;

}

static void logfile_callback(const char *str, int length)
{
	static const char chrlvl[5][sizeof(lF_RED "ERR" _lF ": ")] = {
		lF_RED "ERR" _lF ": ",
		lF_YELW "WRN" _lF ": ",
		lF_BLUE "INF" _lF ": ",
		lF_WHI "TXT" _lF ": ",
		lF_CYA "DBG" _lF ": "
	};
	int i, y;
	unsigned int tstmp;
	char bufr[81];
	for (i = 0; i < length; i++) {
		sscanf(str + i, "%x:%80[^:]", &tstmp, bufr);
		for (i += 0; str[i] != ':'; i++); /* skip time */
		for (i += 1; str[i] != ':'; i++); /* skip name~line */

		i++; /* jump over the ':' onto lvl */
		y = i;

		for (i++; str[i] != '\n' && i < length; i++);

		xf_strb_appendf(lfbuf, "[%3u] "lF_WHI"%16s"_lF" %s%.*s",
				tstmp, bufr, chrlvl[str[y]-'1'],
				(i + 1) - (y + 2), str + y + 2);
	}

	pthread_rwlock_rdlock(&lfile_rwlock);
	for (i = 0; i < lfile_length; i++) {
		struct logfile *lf = lfile_a + i;
		if (!lf->f || (lf->flags & LOGFILE_FILTER_SGR))
			continue;
		pthread_mutex_lock(&lf->wrlock);
		fwrite(lfbuf->a, 1, lfbuf->length - 1, lf->f);
		pthread_mutex_unlock(&lf->wrlock);
	}
	y = lfile_sgr_filter_users;
	pthread_rwlock_unlock(&lfile_rwlock);

	if (!y) {
		xf_strb_clear(lfbuf);
		return;
	}

	for (i = 0; i < length; i++) {
		if (str[i] != '\x1b' || str[i + 1] != '[')
			continue;
		y = i;
		for (i += 2; str[i] != 'm'; i++);
		xf_strb_delete(lfbuf, y, i - y);
	}

	pthread_rwlock_rdlock(&lfile_rwlock);
	for (i = 0; i < lfile_length; i++) {
		struct logfile *lf = lfile_a + i;
		if (!lf->f || !(lf->flags & LOGFILE_FILTER_SGR))
			continue;
		pthread_mutex_lock(&lf->wrlock);
		fwrite(lfbuf->a, 1, lfbuf->length - 1, lf->f);
		pthread_mutex_unlock(&lf->wrlock);
	}
	pthread_rwlock_unlock(&lfile_rwlock);
}

/**
 * logfile_add() - add a file to log to
 * @f:		an open file handle to log to
 * @flags:	LOGFILE_* flags joined by bitwise OR("|")
 *
 * Return:	Negative on error, an id that can be used for logfile_rm()
 *		otherwise.
 */
static int logfile_add(FILE *f, int flags)
{
	assert(f);
	assert(!(~((~flags) | LOGFILE_FILTER_SGR | LOGFILE_AUTOCLOSE)));

	pthread_rwlock_wrlock(&lfile_rwlock);
	int i;
	for (i = 0; i < lfile_length; i++) {
		if (!lfile_a[i].f)
			break;
	}
	if (i == lfile_length) {
		if (lfile_length >= lfile_size) {
			lfile_size *= 2;
			lfile_a = realloc(lfile_a,
					lfile_size * sizeof(lfile_a[0]));
			assert(lfile_a);
		}
		lfile_length++;
	}
	struct logfile *lf = lfile_a + i;
	lf->flags = flags;
	lf->f = f;
	pthread_mutex_init(&lf->wrlock, NULL);

	if ((flags & LOGFILE_FILTER_SGR))
		lfile_sgr_filter_users++;
	pthread_rwlock_unlock(&lfile_rwlock);
	return i;
}

/**
 * logfile_rm() - Remove a logfile
 * @id:		id of the logfile to remove
 *
 * Return:	Negative on error.
 */
static int logfile_rm(int id)
{
	pthread_rwlock_wrlock(&lfile_rwlock);
	if (id < 0 || id >= lfile_length)
		return -1;
	assert(id >= 0 && id < lfile_length);
	struct logfile *lf = lfile_a + id;
	pthread_mutex_lock(&lf->wrlock);

	if ((lf->flags & LOGFILE_AUTOCLOSE))
		fclose(lf->f);

	if ((lf->flags & LOGFILE_FILTER_SGR))
		lfile_sgr_filter_users--;

	lf->f = NULL;
	pthread_mutex_unlock(&lf->wrlock);
	pthread_mutex_destroy(&lf->wrlock);
	if (id == lfile_length - 1)
		lfile_length--;

	pthread_rwlock_unlock(&lfile_rwlock);
	return id;
}

/**
 * logfile_rmall() - remove all logfiles
 */
static void logfile_rmall()
{
	pthread_rwlock_wrlock(&lfile_rwlock);
	for (int i = 0; i < lfile_length; i++) {
		struct logfile *lf = lfile_a + i;
		pthread_mutex_lock(&lf->wrlock);
		if ((lf->flags & LOGFILE_AUTOCLOSE)) {
			fclose(lf->f);
		}
		if ((lf->flags & LOGFILE_FILTER_SGR))
			lfile_sgr_filter_users--;
		lf->f = NULL;
		pthread_mutex_unlock(&lf->wrlock);
		pthread_mutex_destroy(&lf->wrlock);
	}
	lfile_length = 0;
	pthread_rwlock_unlock(&lfile_rwlock);
}

int log_txt_file_add(FILE *f, int flags)
{
	return logfile_add(f, flags);
}

int log_txt_file_rm(int hndl)
{
	return logfile_rm(hndl);
}


/* options */
#include "ce-opt.h"

static inline int log_optcb_stdout(const char *arg)
{ /* -o, --log-stdout [true/f] */
	int b = optarg_bool(arg);
	if (b == -1) {
		lprintf(WRN "Invalid boolean parameter '"lBLD_"%s"_lBLD"'\n", arg);
		return -1;
	} else if (b == -2) { /* empty/NULL */
		if (!logstderr)
			return 0;
		logstderr = false;
		logfile_rm(logstd_id);
		logstd_id = logfile_add(stdout, 0);
		lputs(INF "Logging to stdout enabled.");
		return 0;
	}
	if (!b == logstderr)
		lprintf(WRN "Logging output already set to "lF_YELW"%s"_lF".\n",
				logstderr ? "stderr" : "stdout");
	logstderr = !b;
	logfile_rm(logstd_id);
	logstd_id = logfile_add(logstderr ? stderr : stdout, 0);
	lprintf(INF "Logging to "lF_BLUE"%s"_lF".\n",
			logstderr ? "stderr" : "stdout");
	return 0;
}

static int log_optcb(int index, const char *optarg)
{
	switch (index) {
		case 0: return log_optcb_stdout(optarg);
	};
	assert(1 == 3); /* this should not be reached */
}

static struct optsection log_optsection = {
	.label = "Logging:",
	.callback = log_optcb,
	.opt_a = {
		{ ARG_OPTIONAL, 'o', "log-stdout", "f/true\t"
			"Log to stdout instead of stderr." },
		{ 0, '\0', NULL, NULL },
	},
};

/* Initialized separately later to allow opt's constructors to be called. */
static int log_opt_added = 0;
static void __init log_init_argcb()
{
	log_opt_added = opt_add(ce_options, &log_optsection) >= 0;
}

static void __exit log_exit_argcb()
{
	if (log_opt_added)
		opt_rm(ce_options, &log_optsection);
}

