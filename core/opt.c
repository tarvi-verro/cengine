#include "ce-aux.h"
#include "ce-opt.h"
#include <stdlib.h>	/* alloc realloc free */
#include <stdint.h>
#include <string.h>	/* memcmp, strlen */
#include <ctype.h>	/* isspace */
#include <stdio.h>	/* snprintf */
#include <assert.h>
#include "xf-htable.h"
#include "xf-escg.h"

int optarg_bool(const char *a)
{
	if (a == NULL) return -2;
	int l = strlen(a);
	if (l > 7) return -1;
	if (l == 1) {
		if (*a == '0' || *a == '-')	return 0;
		if (*a == '1' || *a == '+')	return 1;
	}
	char b[7];
	memcpy(b, a, l);
	/* lowercase any uppercase letters */
	b[0] |= 32;
	b[1] |= 32;
	b[2] |= 32;
	b[3] |= 32;
	b[4] |= 32;
	b[5] |= 32;
	b[6] |= 32;
	if (l == 1) {
		if (*b == 't' || *b == 'y')	return 1;
		if (*b == 'f' || *b == 'n')	return 0;
	} else if (l == 2) { /* enable disable */
		if (!memcmp("no", b, 2))	return 0;
	} else if (l == 3) {
		if (!memcmp("yes", b, 3))	return 1;
	} else if (l == 4) {
		if (!memcmp("true", b, 4))	return 1;
	} else if (l == 5) {
		if (!memcmp("false", b, 5))	return 0;
	} else if (l == 6) {
		if (!memcmp("enable", b, 6))	return 1;
	} else if (l == 7) {
		if (!memcmp("disable", b, 7))	return 0;
	}
	return -1;
}

/* Define optset and functions */
static const int section_max = UINT8_MAX;
static const int option_max = UINT8_MAX; /* max per section */
static const int nlong_max = 40; /* disclusive of '\0' */
static const int arghelp_max = 20; /* disclusive of '\0' */
struct optid {
	/* section_a[sectid]->opt_a[optid] */
	uint8_t sectid;
	uint8_t optid;
};
struct optset {
	int section_size;
	int section_length;
	struct optsection **section_a;

	struct xf_htable tbl_long;

	int shrt_size;
	int shrt_length;
	struct {
		char nshrt;
		struct optid opti;
	} *shrt_a;
};
struct optset *optset_construct(struct optset *set, int sect_size)
{
	assert(set != NULL);
	assert(sect_size > 0);

	set->section_size = sect_size;
	set->section_length = 0;
	set->section_a = malloc(sect_size * sizeof(struct optsection *));

	set->shrt_size = 6;
	set->shrt_length = 0;
	set->shrt_a = malloc(sizeof(set->shrt_a[0]) * set->shrt_size);

	xf_htable_construct(&set->tbl_long, 4 /* 16bck */,
			sizeof(struct optid), xf_hash_hsieh_superfast);
	return set;
}

void optset_destruct(struct optset *set)
{
	assert(set != NULL && set->section_a != NULL && set->shrt_a != NULL);
	free(set->section_a);
	xf_htable_destruct(&set->tbl_long);
	free(set->shrt_a);
#ifndef NDEBUG
	set->section_a = NULL;
	set->shrt_a = NULL;
#endif
}

static int opt_nshrt_add(struct optset *set, char nshrt, struct optid opti)
{
	int i;
	for (i = 0; i < set->shrt_length && set->shrt_a[i].nshrt < nshrt; i++);
	if (i < set->shrt_length && set->shrt_a[i].nshrt == nshrt)
		return 1;

	if (set->shrt_length >= set->shrt_size) {
		set->shrt_size *= 2;
		set->shrt_a = realloc(set->shrt_a,
				sizeof(set->shrt_a[0]) * set->shrt_size);
		assert(set->shrt_a != NULL);
	}

	memmove(set->shrt_a + i + 1, set->shrt_a + i,
			(set->shrt_length - i) * sizeof(set->shrt_a[0]));
	set->shrt_length++;
	set->shrt_a[i].nshrt = nshrt;
	set->shrt_a[i].opti = opti;
	return 0;
}
static void opt_nshrt_rm_sectid(struct optset *set, int sectid)
{
	int i;
	for (i = set->shrt_length - 1; i >= 0; i--) {
		if (set->shrt_a[i].opti.sectid != sectid)
			continue;
		memmove(set->shrt_a + i, set->shrt_a + i + 1,
				sizeof(set->shrt_a[0]) * (set->shrt_length - i - 1));
		set->shrt_length--;
	}
}
static struct optid opt_nshrt_find(struct optset *set, char nshrt)
{
	/* binary search */
	int imin = 0, imax = set->shrt_length - 1;
	while (imin < imax) {
		int imid = (imin + imax) / 2;
		assert(imin < imax);
		if (set->shrt_a[imid].nshrt < nshrt)
			imin = imid + 1;
		else
			imax = imid;
	}
	if (imin == imax && set->shrt_a[imin].nshrt == nshrt)
		return set->shrt_a[imin].opti;

	/* linear search
	int i;
	for (i = 0; i < set->shrt_length; i++) {
		if (set->shrt_a[i].nshrt != nshrt)
			continue;
		return set->shrt_a[i].opti;
	};
	*/
	return (struct optid) { .sectid = section_max, .optid = option_max };
}
static int opt_nlong_add(struct optset *set, const char *nlong, int nlong_len,
		struct optid opti)
{
	if (nlong_len > nlong_max) {
		lprintf(ERR "opt nlong_max(%i) reached for set %p opt '"lBLD_"%s"_lBLD"\n",
				nlong_max, set, nlong);
		return 2;
	}
	int a = xf_htable_add(&set->tbl_long, nlong, nlong_len, &opti);
	if (a == XF_HTABLE_ESET)
		lprintf(ERR "opt '"lBLD_"%s"_lBLD"' duplicate entry for set %p\n",
				nlong, set);
	else if (a == XF_HTABLE_EFULL)
		lprintf(ERR "opt htable bucket full for set %p\n", set);

	return !(XF_HTABLE_ESUCCESS == a);
}
static int opt_nlong_rm(struct optset *set, const char *nlong, int nlong_len)
{
	int x =  !(XF_HTABLE_ESUCCESS == xf_htable_remove(&set->tbl_long, nlong, nlong_len));
	/*void *p = xf_htable_find(&set->tbl_long, nlong, nlong_len);
	lprintf(DBG "rm %i :: find"lF_BLUE"%p"_lF"\n", x, p);*/
	return x;
}
static struct optid opt_nlong_find(struct optset *set, const char *nlong, int nlong_len)
{
	struct optid *fnd = (struct optid *)
		xf_htable_find(&set->tbl_long, nlong, nlong_len);
	if (fnd == NULL)
		return (struct optid) { .sectid = section_max, .optid = option_max };
	return *fnd;
}

static int opt_rm_lim(struct optset *set, struct optsection *sect, int lim);
int opt_add(struct optset *set, struct optsection *sect)
{
	assert(set != NULL && set->section_a != NULL);
	assert(sect != NULL);
	assert(sect->callback != NULL);

	int i;
#ifndef NDEBUG
	for (i = 0; i < set->section_length; i++)
		assert(set->section_a[i] != sect);
#endif
	if (set->section_length >= section_max) {
		lprintf(ERR "opt section_max(%i) reached for set %p\n",
				section_max, set);
		return -2;
	}


	if (set->section_length >= set->section_size) {
		set->section_size *= 2;
		set->section_a = realloc(set->section_a,
				sizeof(struct optsection *) * set->section_size);
		assert(set->section_a != NULL);
	}
	set->section_a[set->section_length] = sect;
	set->section_length++;

	struct optid opti;
	opti.sectid = set->section_length - 1;

	for (i = 0; sect->opt_a[i].name_short || sect->opt_a[i].name_long; i++) {
		if (i >= option_max) {
			lprintf(ERR "opt option_max(%i) reached for set %p "
					"section '"lBLD_"%s"_lBLD"(%p)'\n",
					option_max, set, sect->label, sect);
			break;
		}
		struct opt* o = sect->opt_a + i;

		/* Verify argument name in .help */
		if (o->has_arg) {
			int y;
			for (y = 0; o->help[y] != '\0' && o->help[y] != '\t'; y++);
			if (o->help[y] != '\t') {
				lprintf(ERR "Option with argument must name "
						"argument in struct opt.help: "
						"\""lBLD_"ARG\\t"_lBLD"...\"\n");
				opt_rm_lim(set, sect, i - 1);
				return -1;
			}
			if (y > arghelp_max) {
				lprintf(ERR "opt arghelp_max(%i) reached for \"%.*s\"\n",
						arghelp_max, y, o->help);
			}
		}
		opti.optid = i;
		if (o->name_short != '\0' && opt_nshrt_add(set, o->name_short, opti)) {
			opt_rm_lim(set, sect, i - 1);
			return -1;
		}
		if (o->name_long != NULL && opt_nlong_add(set,
					o->name_long, strlen(o->name_long), opti)) {
			opt_rm_lim(set, sect, i - 1);
			return -1;
		}
	}
	return 0;
}
static int opt_rm_lim(struct optset *set, struct optsection *sect, int lim)
{
	struct optsection *s = NULL;
	int i;
	for (i = 0; i < set->section_length; i++) {
		if (set->section_a[i] != sect)
			continue;
		s = set->section_a[i];
		break;
	}
	assert(s != NULL); /* no such section registered */

	int sectid = i;
	opt_nshrt_rm_sectid(set, sectid);

	if (lim < 0)
		lim = option_max + 1;

	for (i = 0; (sect->opt_a[i].name_short || sect->opt_a[i].name_long)
			&& i < lim; i++) {
		if (sect->opt_a[i].name_long == NULL)
			continue;
		opt_nlong_rm(set, sect->opt_a[i].name_long, strlen(sect->opt_a[i].name_long));
	}
	return 0;
}
int opt_rm(struct optset *set, struct optsection *sect)
{
	assert(set != NULL && set->section_a != NULL);
	assert(sect != NULL);

	return opt_rm_lim(set, sect, -1);
}

static int opt_help_i(struct optset *set, struct optid opti, int colspacing);
int opt_display(struct optset *set)
{
	/*   --log-stderr-thres  LOGLVL */
	int i, e;
	for (i = 0; i < set->section_length; i++) {
		if (set->section_a[i]->label)
			continue;
		struct opt *oa = set->section_a[i]->opt_a;
		for (e = 0; oa[e].name_short || oa[e].name_long; e++)
			opt_help_i(set, (struct optid) { .sectid = i, .optid = e, }, 28);
	}
	for (i = 0; i < set->section_length; i++) {
		if (set->section_a[i]->label == NULL)
			continue;
		lprintf(TXT """%s""\n", set->section_a[i]->label);
		struct opt *oa = set->section_a[i]->opt_a;
		for (e = 0; oa[e].name_short || oa[e].name_long; e++)
			opt_help_i(set, (struct optid) { .sectid = i, .optid = e, }, 28);
	}
	return 0;
}
static int opt_help_i(struct optset *set, struct optid opti, int colspacing)
{
	struct opt *o = set->section_a[opti.sectid]->opt_a + opti.optid;

	const size_t blen = nlong_max + arghelp_max + sizeof("-n, --  [] ")
		+ sizeof(lBLD_ _lBLD lBLD_ _lBLD lBLD_ _lBLD) - 1;
	/*lprintf(DBG "%ti\n", blen);*/
	char b[blen];
	int n = 0; /*snprintf(b, blen, ""*/
	int off = 0;
	int helpoff = 0;
	if (o->name_short != '\0' && o->name_long != NULL) {
		n += snprintf(b + n, blen - n, ""lBLD_"-%c"_lBLD", "lBLD_"--%s"_lBLD"",
				o->name_short, o->name_long);
		off += sizeof(lBLD_ _lBLD lBLD_ _lBLD) - 1;
	} else if (o->name_short != '\0') {
		n += snprintf(b + n, blen - n, ""lBLD_"-%c"_lBLD, o->name_short);
		off += sizeof(lBLD_ _lBLD) - 1;
	} else {
		n += snprintf(b + n, blen - n, ""lBLD_"--%s"_lBLD, o->name_long);
		off += sizeof(lBLD_ _lBLD) - 1;
	}

	if (o->has_arg) {
		int y;
		for (y = 0; o->help[y] != '\t' && o->help[y] != '\0'; y++);
		assert(o->help[y] != '\0');
		if (o->has_arg == ARG_OPTIONAL)
			n += snprintf(b + n, blen - n, "  [""%.*s""]", y, o->help);
		else
			n += snprintf(b + n, blen - n, "  ""%.*s""", y, o->help);
		/*off += sizeof(lBLD_ _lBLD) - 1;*/
		helpoff = y + 1;
	}
	assert(n + 1 < blen); /* otherwise memoryleak */


	lprintf(TXT "  "lBLD_"%-*s"_lBLD"  %s\n", colspacing + off, b, o->help + helpoff);
	return 0;
}
int opt_parse(struct optset *set, int argc, char * const argv[], int offset)
{
	assert(argv != NULL);

	int i;
	for (i = offset; i < argc; i++) {
		const char *a = argv[i];
		assert(!isspace(*a));

		if (*a != '-' || a[1] == '\0' || (a[1] == '-' && a[2] == '\0'))
			break;

		if (a[1] == '-') {
			int e;
			for (e = 2; a[e] != '\0' && a[e] != '='; e++);
			struct optid opti = opt_nlong_find(set, a + 2, e - 2);
			if (opti.sectid == section_max && opti.optid == option_max) {
				lprintf(WRN "Unrecognized option \""lF_RED"%s"_lF"\".\n", a);
				continue;
			}
			struct opt *o = set->section_a[opti.sectid]->opt_a + opti.optid;
			const char *z = NULL;
			if (a[e] == '=') {
				if (!o->has_arg) {
					lprintf(ERR "Option \""lF_RED"%*s"_lF"\" does not specify "
							"any arguments.\n", e, a);
					opt_help_i(set, opti, 0);
					continue;
				}
				z = a + e + 1;
				int u = set->section_a[opti.sectid]->callback(opti.optid, z);
				if (u)	opt_help_i(set, opti, 0);
				continue;
			}
			assert(a[e] == '\0');
			if (o->has_arg == ARG_REQUIRED) {
				if (i + 1 >= argc) {
					lprintf(ERR "Option \""lF_RED"%s"_lF"\" "
							"requires an argument\n", a);
					opt_help_i(set, opti, 0);
					continue;
				}
				int u = set->section_a[opti.sectid]->callback(opti.optid,
						argv[i + 1]);
				if (u)	opt_help_i(set, opti, 0);
				i++;
				continue;
			} else if (o->has_arg == ARG_NONE) {
				int u = set->section_a[opti.sectid]->callback(opti.optid,
						NULL);
				if (u)	opt_help_i(set, opti, 0);
				continue;
			}
			assert(o->has_arg == ARG_OPTIONAL);
			z = NULL;
			if (i + 1 < argc && argv[i + 1][0] != '-')
				z = argv[i + 1];

			int u = set->section_a[opti.sectid]->callback(opti.optid, z);
			if (u)	opt_help_i(set, opti, 0);
			continue;
		}
		assert(a[1] != '-' && a[1] != '\0');
		int e;
		for (e = 1; a[e] != '\0'; e++) {
			struct optid opti = opt_nshrt_find(set, a[1]);
			if (opti.sectid == section_max && opti.optid == option_max) {
				lprintf(WRN "Unrecognized short option \"-"lF_RED"%c"_lF"\"\n", a[1]);
				break;
			}
			struct opt *o = set->section_a[opti.sectid]->opt_a + opti.optid;
			if (!o->has_arg) {
				int u = set->section_a[opti.sectid]->callback(opti.optid,
						NULL);
				if (u)	opt_help_i(set, opti, 0);
				continue;
			}
			if (a[e + 1] != '\0') {
				int u = set->section_a[opti.sectid]->callback(opti.optid,
						a + e + 1);
				if (u)	opt_help_i(set, opti, 0);
				break;
			}

			if (i + 1 >= argc) {
				if (o->has_arg == ARG_OPTIONAL) {
					int u = set->section_a[opti.sectid]->callback(opti.optid,
							NULL);
					if (u)	opt_help_i(set, opti, 0);
					break;
				} else {
					lprintf(ERR "Short option \"-"lF_RED"%c"_lF"\" requires "
							"an argument.\n", a[e]);
					opt_help_i(set, opti, 0);
					break;
				}
			}

			if (o->has_arg == ARG_OPTIONAL && argv[i + 1][0] == '-') {
				int u = set->section_a[opti.sectid]->callback(opti.optid,
						NULL);
				if (u)	opt_help_i(set, opti, 0);
				break;
			}
			int u = set->section_a[opti.sectid]->callback(opti.optid,
					argv[i + 1]);
			if (u)	opt_help_i(set, opti, 0);
			i++;
			break;
		}
	}

	return i;
}

/* ce_options */
struct optset *ce_options = &(struct optset) {  };
static int ce_options_helpcb(int index, const char *optarg)
{
	opt_display(ce_options);
	return 0;
}
static struct optsection ce_options_help = {
	.label = NULL,
	.callback = ce_options_helpcb,
	.opt_a = {
		{ ARG_NONE, 'h', "help", "Display help." },
		{ 0, 0, NULL, NULL },
	},
};

__attribute__((constructor(120))) static void ce_opt_init()
{
	struct optset *os = optset_construct(ce_options, 3);
	assert(os != NULL && os == ce_options);
	opt_add(os, &ce_options_help);
}

__attribute__((destructor(120))) static void ce_opt_exit()
{
	opt_rm(ce_options, &ce_options_help);
	optset_destruct(ce_options);
}


