#include <stdarg.h> /* va_list */
#include <stdio.h>
#include "xf-strb.h"
#include "ce-aux.h"
#include "ce-log.h"
#include <time.h>

/**
 * DOC: log-pipeline
 * lputs()/lprintf() -> log_raw_process() -> log_raw_push() 
 * -> log_argcb() -> log_txt_push()
 */

/* raw data */
static struct xf_strb log_raw = { .a = NULL, };
static struct xf_strb *rawb = &log_raw; /* convinience */

/* log_raw_push() calls these */
static void (**raw_callb_a)(const char *str);
static int raw_callb_length = 0;
static int raw_callb_size = 1;
void log_raw_listen_add(void (*callb)(const char *str));
int log_raw_listen_rm(void (*callb)(const char *str));

/* misc */
static time_t logstart;

/* txt callb's */
static int txt_callb_length = 0;
static int txt_callb_size = 3;
static struct {
	void *inf; 
	void (*call)(const char *str, int len, int lvl, void* inf);
} *txt_callb_a;
void log_txt_pull(const char *in);
void log_txt_push(const char *out, int len, int lvl, void *unused);
static void log_txt_out(const char *out, int len, int lvl, void *d);
static int log_txt_listen_rm(void (*call)(const char *, int, int, void*), void *inf);
static void log_txt_file_rmall();
static void log_txt_listen_add(void (*call)(const char *, int, int, void*), void *inf);

size_t ce_log_memcnt()
{
	size_t cnt = 0;
	if (rawb->a)
		cnt += rawb->size;
	if (txt_callb_a)
		cnt += txt_callb_size * sizeof(txt_callb_a[0]);
	if (raw_callb_a)
		cnt += raw_callb_size * sizeof(raw_callb_a[0]);
	return cnt;
}

__attribute__((constructor(110))) static void log_init()
{
	/* raw */
	raw_callb_a = malloc(sizeof(raw_callb_a[0]) * raw_callb_size);
	xf_strb_construct(rawb, 128);
	xf_strb_set(rawb, "\n");
	//logstart = 0;//time(NULL);
	logstart = time(NULL);
	/* txt */
	txt_callb_a = malloc(sizeof(txt_callb_a[0]) * txt_callb_size);
	log_raw_listen_add(log_txt_pull);
	log_txt_listen_add(log_txt_out, NULL);
	lputs(INF "Logging initialized.");
}

__attribute__((destructor(110))) static void log_exit()
{
	lputs(INF "Logging end reached.");
	/*FILE *f = fopen("logcpy", "w+");
	fwrite(rawb->a, 1, rawb->length - 1, f);
	fclose(f);*/
	/* txt */
	log_txt_file_rmall();
	log_txt_listen_rm(log_txt_out, NULL);
	log_raw_listen_rm(log_txt_pull);
	free(txt_callb_a);
	/* raw */
	xf_strb_destruct(rawb);
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
void log_raw_listen_add(void (*callb)(const char *str))
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
 * 		1 when such callback is not listed
 */
int log_raw_listen_rm(void (*callb)(const char *str))
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
 * @s:		where the new logs start
 */
static void log_raw_push(int s)
{
	const char *strt = rawb->a + s;
	for (int i = 0; i < raw_callb_length; i++)
		raw_callb_a[i](strt);
}
/**
 * log_raw_process() - processes the newly added lines
 * @s:		starting position of newly appended lines
 *
 * Adds timestamps to the lines and ensures line header consistency.
 */
static void log_raw_process(int s)
{
	int ll = rawb->length - 2;
	char *b = rawb->a;
	for (int i = rawb->length - 3; i >= s - 1; i--) {
		int cs;
		if (b[i] == '\n' && i != rawb->length - 2)
			cs = i + 1;
		else 
			continue;

//fprintf(stdout, "XXXXX\n");
		int fnd = 0;
		for (int z = cs; z < ll; z++) {
			if (b[z] != ':') continue;
			fnd++;
			if (fnd == 2) {
				break; /* found all necessary properties */
			}
		}
		time_t ct = time(NULL) - logstart;
		if (fnd != 2) {
			xf_strb_insertf(rawb, cs,
					"%tX:" DBG "((missing log line properties))\n"
					"%tX:unknown:3:", ct, ct); /* inf lvl  */
		} else {
//fprintf(stdout, "YYYYY\n");
			xf_strb_insertf(rawb, cs, "%tX:", ct);
		}
//fprintf(stdout, "ZZZZZ\n");

		ll = i;
	}
	log_raw_push(s);
}

int lprintf(const char *format, ...)
{
	va_list l;
	va_start(l, format);
	/* "1: err" : len 7, strlen 6, lstindx 5*/
	/* "1: "    : len 4, strlen 3, lstindx 2 */
	int s = rawb->length - 1;
	int r = xf_strb_vappendf(rawb, format, l);
	//printf("r%i, >>>\n%s\n<<<\n", r, rawb->a + s);
	va_end(l);
	log_raw_process(s);

	return r;
}
int lputs(const char *str)
{
	int s = rawb->length - 1;
	int c = xf_strb_append(rawb, str);
	c += xf_strb_append(rawb, "\n");

	log_raw_process(s);

	return c;
}


/* handle some output methods */
static int err_thres = 
#ifdef NDEBUG
'2'; /* default WRN */
#else
'5'; /* debug DBG */
#endif
static int out_use = 0;

/**
 * log_txt_conv() - convert raw logs into more readable format
 * @in:		raw log input string to convert
 * @lend:	variable holding whether @in starts with a new line, initialize 
 * 		it to 1; alternatively, if the string is expected not to expand
 * 		you can use NULL to disregard keeping this variable between 
 * 		calls
 * @llvl:	variable holding current line's level('1'-'5'), if the string
 * 		is not expected to expand you can use %NULL to disregard keeping
 * 		this variable between calls
 * @out:	function called for writing the converted text
 * @pass:	pass a pointer to @out
 */
static void log_txt_conv(const char* in, int *lend, int *llvl,
		void (*out)(const char *str, int len, int lvl, void*), void *pass)
{
	static const char chrlvl[5][sizeof(LFG_RED "ERR" LFG_DEF ": ")] = { 
		LFG_RED "ERR" LFG_DEF ": ", 
		LFG_YELLOW "WRN" LFG_DEF ": ", 
		LFG_BLUE "INF" LFG_DEF ": ", 
		LFG_WHITE "TXT" LFG_DEF ": ", 
		LFG_CYAN "DBG" LFG_DEF ": "
	};
	int _lend = 1;
	if (lend == NULL)
		lend = &_lend;
	int _llvl = '3';
	if (llvl == NULL)
		llvl = &_llvl;

	const char *p = in;
	const char *w = p;
	char bufr[81];
	char hdr[128];
	for (; *p != '\0'; p++) {
		if (*p == '\n') {
			out(w, p - w + 1, *llvl, pass);
			w = p + 1;
			*lend = 1;
			continue;
		}
		if (!*lend) 
			continue;

		unsigned int tstmp;
		
		sscanf(p, "%x:%80[^:]", &tstmp, bufr);
		for (; *p != ':'; p++); // Skip time
		for (p++; *p != ':'; p++); // Skip origin 
		p++; 
		w = p;
		*lend = 0;
		const char *prep;
	//	out(w, p - w, *llvl, pass);

		prep = chrlvl[*p - '1'];
		*llvl = *p;

		int hdl = snprintf(hdr, sizeof(hdr), "[%3u] " 
					LFG_WHITE "%16s" LFG_DEF " ", tstmp, bufr);

		out(hdr, hdl, *llvl, pass);
		out(prep, sizeof(chrlvl[0]), *llvl, pass);
		w = p + 2;
		p++;
	}
	out(w, p - w, *llvl, pass);
}
void log_stderr_threshold(const char *lvlmcro)
{
	
	assert(lvlmcro != NULL);
	int offs = sizeof(DBG) - 3;
	/*lprintf(DBG "offs %i\n", offs);*/
	lvlmcro += offs;
	assert(*lvlmcro >= '1' && '5' >= *lvlmcro);
	err_thres = *lvlmcro;
	
}
void log_debug(int enable)
{
}


static void log_txt_listen_add(void (*call)(const char *s, int l, int lvl, void* inf), 
		void *inf) 
{
	if (txt_callb_length + 1 > txt_callb_size) {
		txt_callb_size *= 2;
		txt_callb_a = realloc(txt_callb_a, sizeof(txt_callb_a[0])
				* txt_callb_size);
	}
	txt_callb_a[txt_callb_length].call = call;
	txt_callb_a[txt_callb_length].inf = inf;
	txt_callb_length++;
}
static int log_txt_listen_rm(void (*call)(const char *, int, int, void*), void *inf)
{
	for (int i = 0; i < txt_callb_length; i++) {
		if (txt_callb_a[i].call != call || txt_callb_a[i].inf != inf) continue;
		memmove(txt_callb_a + i, txt_callb_a + i + 1, 
				sizeof(txt_callb_a[0]) 
				* (txt_callb_length - i - 1));
		txt_callb_length--;
		return 0;
	}
	return 1;
}
void log_txt_push(const char *out, int len, int lvl, void *unused)
{
	for (int i = 0; i < txt_callb_length; i++)
		txt_callb_a[i].call(out, len, lvl, txt_callb_a[i].inf);
}
void log_txt_pull(const char *in)
{
	static int lend = 1;
	static int llvl = '3';
	log_txt_conv(in, &lend, &llvl, log_txt_push, NULL);
}
static void log_txt_out(const char *out, int len, int lvl, void *d)
{
	if (lvl > err_thres) return;
	fwrite(out, 1, len, stderr);
}
struct logfile {
	int id;
	int filter_sgr;
	FILE *f;
};
static void log_txt_file_out(const char *out, int len, int lvl, void *pass)
{
	struct logfile *f = (struct logfile *) pass;
	if (!f->filter_sgr) {
		fwrite(out, 1, len, f->f);
	} else {
		int s = 0;
		int i;
		for (i = 0; i < len; i++) {
			if (out[i] != 0x1b) continue;
			fwrite(out + s, 1, i - s, f->f);
			for (i++; i < len && out[i] != 'm'; i++);
			s = i + 1;
		}
		if (s < len)
			fwrite(out + s, 1, i - s, f->f);
	}
}

int log_txt_file(const char *file, int clear, int copy, int follow, int filter_sgr)
{
	static int idcounter = 1;
	FILE *f;

	if (clear)
		f = fopen(file, "w");
	else
		f = fopen(file, "a");

	if (f == NULL) {
		lprintf(WRN "Could not open file '%s' for appending.\n",
				file);
		return -1;
	}

	if (copy) {
		struct logfile tmpf = { .id = 0, .f = f, .filter_sgr = filter_sgr };
		lprintf(INF "Writing current log to '%s'.\n", file);
		log_txt_conv(rawb->a, NULL, NULL, log_txt_file_out, &tmpf);
	}
	if (!follow) {
		fclose(f);
		return 0;
	}
	struct logfile *lf = malloc(sizeof(struct logfile));
	lf->id = idcounter++;
	lf->f = f;
	lf->filter_sgr = filter_sgr;
	log_txt_listen_add(log_txt_file_out, lf);
	lprintf(INF "Now following '%s' for logging, hndl %i.\n", file, lf->id);
	
	return lf->id;
}
int log_txt_file_rm(int hndl)
{
	for (int i = 0; i < txt_callb_length; i++) {
		if (txt_callb_a[i].call != log_txt_file_out 
				|| ((struct logfile *)txt_callb_a[i].inf)->id != hndl) 
			continue;
		lprintf(INF "Closing log file with handle %i.\n", 
				((struct logfile *)txt_callb_a[i].inf)->id);
		fclose(((struct logfile *)txt_callb_a[i].inf)->f);
		free(txt_callb_a[i].inf);
		log_txt_listen_rm(log_txt_file_out, txt_callb_a[i].inf);
		return 0;
	}
	return 1;
}
static void log_txt_file_rmall() 
{
	for (int i = txt_callb_length - 1; i >= 0; i--) {
		if (txt_callb_a[i].call != log_txt_file_out)
			continue;
		lprintf(INF "Closing log file with handle %i.\n", 
				((struct logfile *)txt_callb_a[i].inf)->id);
		fclose(((struct logfile *)txt_callb_a[i].inf)->f);
		free(txt_callb_a[i].inf);
		log_txt_listen_rm(log_txt_file_out, txt_callb_a[i].inf);
	}
}

/* argument handling */
static int log_argcb(const char *arg, int size)
{
	if (size == 7 && !memcmp(arg, "--help", 6)) {
		lprintf(TXT "  %-27s %s\n", "-o, --log-stdout[=t/false]", 
				"Use the stdout stream for loglvls not going to stderr.");
		lprintf(TXT "  %-27s %s\n", "--log-stderr-thres=LOGLVL",
				"Set the threshold at which messages are printed to stderr.");
		lprintf(TXT "  %-27s %s\n", "",
				"Valid LOGLVL values are: DBG, TXT, INF, WRN, ERR");
		lprintf(TXT "  %-27s %s\n", "--log-file-wfce=FILE",
				"Open log FILE and clear(c) it, paste history(w) and "
				"follow(f).");
		lprintf(TXT "  %-27s %s\n", "", "Additionally filter esc sequences(e).");
	} else if (size >= 13 && !memcmp(arg, "--log-file-", 11)) {
		int follow = 0;
		int write = 0;
		int clear = 0;
		int no_sgt = 0;
		int i;
		for (i = 11; arg[i] != '\0' && arg[i] != '='; i++) {
			if (arg[i] == 'w')	write = 1;
			else if (arg[i] == 'f') follow = 1;
			else if (arg[i] == 'c') clear  = 1;
			else if (arg[i] == 'e') no_sgt = 1;
			else return 0;
		}

		const char *prm = NULL;
		if (arg[i] == '=') prm = arg + i + 1;
		else if (arg[i] != '\0' || !(prm = arg_peek(1))) 
			return 0;

		log_txt_file(prm, clear, write, follow, no_sgt);
		return arg[i] == '=' ? 1 : 2;

	} else if (size >= 13 && !memcmp(arg, "--log-stdout", 12)) {
		const char *prm = NULL;
		if (arg[12] == '=') prm = arg + 13;
		else if (arg[12] != '\0' || !(prm = arg_peek(1))) 
			return 0;
		
		int b = arg_bool(prm);
		if (b == -1 && arg[12] == '=')
			lprintf( WRN "Invalid boolean parameter '%s'\n", prm);
		
		if (prm == NULL || b < 0) {
			out_use = 1;
			lputs(INF "Logging to stdout enabled.");
			return 1;
		}
		out_use = b;
		lprintf(INF "Logging to stdout %s.\n", 
				out_use ? "enabled" : "disabled");
		return arg[12] == '=' ? 1 : 2;
	} else if (size == 3 && arg[0] == '-' && arg[1] == 'o') {
		if (out_use == 1)
			return 1;
		out_use = 1;
		lputs(INF "Logging to stdout enabled.");
		return 1;
	} else if (size >= 19 && !memcmp(arg, "--log-stderr-thres", 18)) {
		const char *prm = NULL;
		if (arg[18] == '=') prm = arg + 19;
		else if (arg[18] != '\0' || !(prm = arg_peek(1))) 
			return 0;

		if (prm == NULL) {
			lputs(WRN "Missing LOGLVL.");
			return 1;
		}

		if (strlen(prm) != 3) {
			lprintf(WRN "Invalid LOGLVL: '%s'\n", prm);
			return arg[18] == '=' ? 1 : 2;
		}
		else if (!memcmp(prm,"DBG",3)) log_stderr_threshold(DBG);
		else if (!memcmp(prm,"TXT",3)) log_stderr_threshold(TXT);
		else if (!memcmp(prm,"INF",3)) log_stderr_threshold(INF);
		else if (!memcmp(prm,"WRN",3)) log_stderr_threshold(WRN);
		else if (!memcmp(prm,"ERR",3)) log_stderr_threshold(ERR);
		else {
			lprintf(WRN "Invalid LOGLVL: '%s'\n", prm);
			return arg[18] == '=' ? 1 : 2;
		}
		lprintf(INF "Log stderr level is now %s.\n", prm);
		return arg[18] == '=' ? 1 : 2;

	}
	return 0;
}
static void __init log_init_argcb()
{
	arg_callb_add(log_argcb);
	lputs(DBG "Logging arguments hooked.");
}
static void __exit log_exit_argcb()
{
	arg_callb_rm(log_argcb);
	lputs(DBG "Logging arguments removed.");
}
