#include "ce-aux.h"
#include "ce-log.h"
#include "ce-mod.h"
#include "xf-htable.h"
#include "xf-strb.h"
#define XF_MREGION_EXP_ALLOC(total,initsize,previous) \
	previous * 2
#include "xf-mregion.h"
#define XF_MREGION_EXP_ALLOC(total,initsize,previous) \
	previous * 2
#include <stdint.h> /* uint8_t */
#include <ctype.h> /* isspace */
#include <stdbool.h>
#define NAMEINF_UNSPECIF UINT8_MAX

/**
 * struct id_t - index and verification bits of identifiers
 * @index:	index in @mods_a
 * @iter:	verification that must match iter in &struct mod_inf %mods_a,
 *		used to verify that correct module is referenced after an
 *		index has been re-used
 * @iserr:	MSB of integer representation, when set, @index and @iter
 *		are invalid, see below for more details
 *
 * If @iserr is set, @index and @iter aren't represented in the value, it
 * should instead be handled as a signed negative integer containing an error
 * code returned by ce_mod_add().
 */
struct id_t {
	unsigned index : 23;
	unsigned iter : 8;
	unsigned iserr : 1;
};

/**
 * struct use_inf - information about a fcn to be initialized
 * @fcn_index:	index of the fcn to be initialized in fcns_a
 * @incompat:	if instead of initializing the fcn mustn't be loaded
 * @end:	if the fcn would be preferred to be initialized last (or
 *		somewhere among the last functionalities)
 * @after:	should be initialized after the given mod
 * @ver_len:	length of the version string
 * @ver_off:	offset of the version string in a separate array of
 *		characters
 */
struct use_inf {
	uint16_t fcn_index : 11; /* max2047 */
	uint16_t incompat : 1;
	uint16_t end : 1;
	uint16_t after : 1;
	uint16_t : 2; /* 16bits */
	uint16_t ver_len : 5;
	uint16_t ver_off : 11; /* padding */ /* 16 + 16 = 32bits */
};
/**
 * struct mod_inf_fcn - information about fcn provided in struct mod_inf
 * @index:	index of functionality in fcns_a
 * @ver_len:	length of provided functionality version string in
 *		&struct mod_inf.additional (after the &struct mod_inf_fcn's)
 */
struct mod_inf_fcn {
	uint16_t index : 11; /* max2047 */
	uint16_t ver_len : 5; /* max31 */
};

/**
 * struct mod_inf - extra information about a module
 * @name_off:	offset of name in bytes from @additional
 * @name_len:	length of the name string
 * @ver_len:	length of the version string or %0 if none given
 * @loaded:	%1 if the module is loaded, %0 if it's not loaded;
 * @loading:	%1 if the mod_load() is currently running for module
 * @iter:	used to verify that the index access was correct
 * @fcn_cnt:	count of &struct mod_inf_fcn entries defined by mod in
 *		@additional
 * @additional:	memory containing additional info, if this is %NULL, the
 *		instance is not used; direct access for defined functionality
 *		&struct mod_inf_fcn array length @fcn_cnt
 * @use_cnt:	amount of static &struct use_inf's in @additional - used
 *		functionality as reqested by &struct ce_mod.use
 * @use_live_cnt:
 *		how many additional &struct use_inf's in @additional after
 *		those counted in @use_cnt - used functionality as requested by
 *		ce_mod_use().
 * @use_live_size:
 *		how much space has been allocated for additional
 *		&struct use_inf's in @additional(mem allocated for
 *		@use_live_cnt entries), note however that no version info is
 *		allocated
 * @load:	the function to call for loading the module, returns
 *		%0 on success
 * @unload:	function to call for unloading the module, returns %0
 *		on success
 *
 * Use mod_inf_name_get() and mod_inf_vers_get() to retrieve the version and
 * name strings.
 *
 * Initialized state:
 * If @additional equals %NULL, the module described has been removed. If an
 * index led to this address, it is invalid.
 *
 * Memory layout of additional:
 * Fcn def	(%0 to @fcn_cnt * sizeof() &struct mod_inf_fcn)
 *
 * Fcn ver strs	(@fcn_cnt * sizeof() &struct mod_inf_fcn to @name_off)
 *
 * Mod name	(@name_off to @name_off + @name_len)
 *
 * Ver str	(@name_off + @name_len to @name_off + @name_len + @ver_len)
 *
 * Use infs	(@name_off + @name_len + @ver_len to @name_off + @name_len + @ver_len
 *			+ sizeof() &struct use_inf * (@use_cnt + @use_live_size))
 *
 * Use verstrs	(<end of use infs> to <end of use infs>
 *			+ <last useinf>->ver_off + <last useinf>->ver_len)
 */
struct mod_inf {
	uint16_t name_off;
	uint8_t name_len;
	uint8_t ver_len : 5;
	uint8_t loading : 1;
	uint8_t loaded : 1;
	uint8_t  : 1; /* 32bits */
	uint32_t iter : 4; /* 0xf(15) max */
	uint32_t fcn_cnt : 6; /* 63 fcns max */
	uint32_t use_cnt : 7; /* 127 required mods max */
	uint32_t use_live_cnt : 7; /* 127 additionally required mods */
	uint32_t use_live_size : 7;
	uint32_t : 1; /* 64 bits */
	/* name - (0 ... name_len-1), ver - (name_len ... name_len+ver_len) */
	struct mod_inf_fcn *additional; /* 32 + 16 + (16) + 64 = 128bits */
	int (*load)();
	int (*unload)();
};

/**
 * DOC: static int mods_length;
 * DOC: static int mods_size;
 * DOC: static struct mod_inf *mods_a;
 * List of ce_mod_add()'ed modules. If @mods_a[n].m == %NULL, then given mod is
 * not present.
 *
 * Adding and removing mods should only happen through ce_mod_add() and
 * ce_mod_rm().
 */
static int mods_length = 0; /* how much of mods_a has mods mapped to them */
static int mods_count = 0;  /* how many mods between 0 and mods_length */
static int mods_size = 10;
static struct mod_inf *mods_a;
/*
 * &struct use_blck_mod.indx holds 7 bits and value 127 is reserved for mods not present
 */
static const int mods_max = 2047;
/* invalid mod; not present */
static const int mods_unavail = 127;

/**
 * mods_expand() - ensure mods_a can hold given amount of members
 * @size:	how many members the mods_a should be able to hold
 */
static void mods_expand(int size)
{
	if (size <= mods_size) return;
	int newsize = mods_size * 2;
	if (newsize < size)
		newsize = size;
	mods_a = realloc(mods_a, sizeof(mods_a[0]) * newsize);

	for (int i = mods_size; i < newsize; i++) {
		/* initialized only the first time */
		mods_a[i].iter = 0;
	}

	mods_size = newsize;
}
/**
 * mod_inf_name_get() - get a modules name string
 * @minf:	pointer to the module, possibly in %mods_a
 * @len:	where to output the length of the name string
 * @name:	where to output the name string
 */
static inline void mod_inf_name_get(struct mod_inf *minf, int *len,
		const char **name)
{
	assert(minf != NULL);
	assert(minf->additional != NULL);
	*len = minf->name_len;
	*name = ((char *) minf->additional) + minf->name_off;
}

/**
 * mod_inf_fcn_vers_get() - get the provided fcn's version strings
 * @minf:	pointer to the module, possibly in %mods_a
 *
 * Return:	The non-terminated fcn versions.
 */
static inline const char *mod_inf_fcn_vers_get(struct mod_inf *minf)
{
	assert(minf != NULL);
	assert(minf->additional != NULL);
	return (const char *)(minf->additional + minf->fcn_cnt);
}

/**
 * mod_inf_vers_get() - get the version string of a mod
 * @minf:	pointer to the module, possibly in %mods_a
 * @len:	where to output the length of the version string
 * @vers:	where to output the version string
 */
static inline void mod_inf_vers_get(struct mod_inf *minf, int *len, const char **vers)
{
	assert(minf);
	assert(minf->additional != NULL);
	*len = minf->ver_len;
	*vers = ((char *)minf->additional) + minf->name_off
	       + minf->name_len;
}

/**
 * mod_inf_use_get() - get the compiled use info
 * @minf:	pointer to the module, possibly in %mods_a
 * @uinf_len:	where to output the length of @uinf &struct use_inf array
 * @uinf:	where to output the compiled use info &struct use_inf array
 * @uvers:	where to output the reference to an array of characters
 *		that are pointed to by &struct use_inf's
 */
static inline void mod_inf_use_get(struct mod_inf *minf, int* uinf_len,
		struct use_inf **uinf, char **uvers)
{
	assert(minf);
	if (uinf_len != NULL)
		*uinf_len = minf->use_cnt + minf->use_live_cnt;

	assert(uinf != NULL);
	*uinf = (struct use_inf *)(((uint8_t *)minf->additional)
		+ minf->name_off + minf->name_len + minf->ver_len);

	if (uvers != NULL)
		*uvers = ((char *) *uinf) + (minf->use_cnt
			+ minf->use_live_size) * sizeof(struct use_inf);
}


/**
 * DOC: struct refb *top_use;
 * Use buffer reference that was active when the latest module load function
 * (&struct mod_inf->load()) was called.
 *
 * This should be modified with care. Currently it is modified from two areas:
 *
 * 1. root level(ce_mod_use() from main() function) mod_use() when first run
 *
 * 2. mod_load() function which is reached via any mod_use()'s %use string.
 *
 */
static struct refb *top_use = NULL;

struct xf_mregion *fcn_names = NULL; /* functionality names */

/**
 * struct fcn_inf - information about functionality
 * @mod_index:	index of the mod for given fcn in mods_a or count of mods
 *		providing given functionality if @mod_count is set
 * @mod_count:	if set, @mod_index will specify amount of mods providing given
 *		functionality
 * @name_subreg: the subregion of fcn_names the name is in
 * @name_offset: offset of name from the start of select region
 * @name_len:	length of name in fcn_names subregion @name_subreg starting
 *		from @name_offset
 * @variable:	%0 if the fcn can not be expanded, %1 if there can be one
 *		child loaded at a given time, %2 if there can be indefinite
 *		children concurrently loaded
 * @expands:	whether or not given mod is expanding another fcn
 * @loaded:	%0 if it's not loaded, %1 if a module providing given fcn is
 *		loaded
 * @defined:	whether or not the full specification is known from .def or
 *		from a fcn's expansion string, this is so overriding a fcn
 *		previously from .use to a variable wouldn't yield an error
 * @parent:	if @expands is %1, this holds the index of parent fcn
 * @child_cnt:	if @variable is %2 or %1, this holds the number of fcn that
 *		can expand it, or %31 if it has %31+ children
 *
 * If @mod_index is %0 and @mod_count is %1, no modules provide given
 * functionality.
 */
struct fcn_inf {
	uint16_t mod_index : 11; /* index of single mod or count of mods providing */
	uint16_t : 4;
	uint16_t mod_count : 1; /* if set, mod_index is amount of mods instead */
	uint16_t name_subreg : 4;
	uint16_t name_offset : 12; /* max4095 */ /* 32bits */
	uint8_t name_len;
	uint8_t variable : 2;
	uint8_t expands : 1;
	uint8_t loaded : 1;
	uint8_t defined : 1;
	uint8_t : 3; /* padding */ /* 32+16bits */
	uint16_t parent : 11; /* max2047 */
	uint16_t child_cnt : 5; /* max31 */ /* 64bits */
};
static int fcns_length = 0;
static int fcns_size = 20;
static struct fcn_inf *fcns_a;
/* &struct fcn_inf.parent : 11
 */
static const int fcns_max = 2047;

static void *xf_mregion_allocv(struct xf_mregion *r, size_t size,
		int *subreg, int *offset)
{
	assert(r != NULL);
	assert(size > 0);
	struct xf_mregion_sub *s;
	int i = 0;
	for (s = &r->sub; s != NULL; s = s->next) {
		if (s->size - s->length < size) {
			i++;
			continue;
		}
		*subreg = i;
		*offset = s->length;
		void *rv = s->data + s->length;
		s->length += size;
		return rv;
	}
	/* Needs more memory */
	unsigned int total = 0;
	for (s = &r->sub; 0 == 0; s = s->next) {
		total += s->size;
		if (s->next == NULL) break;
	}
	unsigned int nsz = XF_MREGION_EXP_ALLOC(total, r->sub.size, s->size);
	if (nsz < size)
		nsz = size;

	s->next = malloc(sizeof(struct xf_mregion_sub) + nsz);
	assert(s->next != NULL);
	s = s->next;

	s->next = NULL;
	s->size = nsz;
	s->length = size;

	*subreg = i;
	*offset = 0;

	return s->data;
}

/**
 * fcn_inf_name() - returns non-terminated functionality name
 * @f:		functionality whose name is requested
 *
 * Return:	the functionality name, without a '\0' terminating it
 */
static const char *fcn_inf_name(struct fcn_inf *f)
{
	struct xf_mregion_sub *s = &fcn_names->sub;
	for (int i = 0, l = f->name_subreg; i < l; i++) {
		s = s->next;
		assert(s != NULL);
	}
	return s->data + f->name_offset;
}

/**
 * fcns_expand() - ensure fcns_a can hold given amount of members
 * @size:	how many members should fcns_a be able to hold
 *
 * Return:	%0 when fcns_a maximum capacity is reached,
 * 		%1 when no expansion was required,
 * 		%2 when fcns_a was expanded
 */
static int fcns_expand(int size)
{
	if (size <= fcns_size) return 1;
	if (size > fcns_max) {
		return 0;
	}
	fcns_size *= 2;
	if (fcns_size > fcns_max) {
		fcns_size = fcns_max;
		lprintf(WRN "fcns_a maximum capacity %i(requested for %i) now allocated\n",
				fcns_max, size);
	}

	fcns_a = realloc(fcns_a, sizeof(fcns_a[0]) * fcns_size);
	return 2;
}

/**
 * struct hashentry - an entry corresponding to a module name in fcn_l
 * @index:	index of the first entry by this name in fcns_a
 */
struct hashentry {
	uint16_t index /*: 11*/; /* 2047max */
	/*uint16_t  : 5;  31max - unused */
};

static struct xf_htable fcn_lookup = { .buckets = NULL, };
static struct xf_htable *fcn_l = &fcn_lookup; /* convinient use */

/**
 * fcn_parent_set() - iterate a parent to include a child
 * @fcn_child:	the child that's parent to set
 * @fcn_parent:	parent that's new child @fcn_child is
 *
 * Iterates the parent.
 *
 * While the @fcn_child value is currently not used at all, future
 * implementations might and consistency with fcn_parent_unset() requires.
 *
 * Return:	the parent index
 */
static int fcn_parent_set(int fcn_child, int fcn_parent)
{
	assert(fcn_child >= 0 && fcn_child < fcns_length);
	assert(fcn_parent >= 0 && fcn_parent < fcns_length);

	struct fcn_inf *f = fcns_a + fcn_parent;
	if (f->child_cnt != (1 << 5) - 1) {
		f->child_cnt++;
	}
	return fcn_parent;
}

/**
 * fcn_parent_unset() - unset a parent for a fcn
 * @fcn_child:	the child that's parent has been unset
 * @fcn_parent:	the parent to unset
 *
 * In effect, this merely decreases the child counter on parent fcn.
 *
 * Return:	negative on failure, positive on success
 */
static int fcn_parent_unset(int fcn_child, int fcn_parent)
{
	assert(fcn_parent >= 0 && fcn_parent < fcns_length);
	struct fcn_inf *f = fcns_a + fcn_parent;
	assert(f->child_cnt != 0);
	assert(f->variable);
	if (f->child_cnt != (1 << 5) - 1) {
		f->child_cnt--;
		return 1;
	}
	int i, cnt = 0;
	for (i = 0; i < fcns_length; i++) {
		if (i == fcn_child || !fcns_a[i].expands
				|| fcns_a[i].parent != fcn_parent)
			continue;
		cnt++;
	}
	assert(cnt >= (1 << 5) - 2);
	f->child_cnt = cnt > (1 << 5) - 1 ? (1 << 5) - 1 : cnt;
	return 1;
}

static int refb_fcn_cnt(struct refb *b, int fcn_index);
/**
 * fcn_get() - get fcn for given fcn name
 * @fcn_nl:	length of the name string in @fcn_n
 * @fcn_n:	fcn name string to get, this should contain '+'; if
 *		expandability is specified via @variable, it will not expect
 *		to find '$' or '[]' in the end(vals %1, %0, %-4)
 * @variable:	%2, %1, %0, or %-3 if it is determined by the presence of '$'
 *		or '[]' in the end of $fcn_n or %-4 if it remains unknown (use
 *		strings - use_compile())
 *
 * This function either finds the existing entry and verifies its properties
 * or if no existing entry is present, it inserts a new entry with specified
 * properties.
 *
 * Return:	negative on error, index of fcn in fcns_a otherwise
 */
static int fcn_get(int fcn_nl, const char *fcn_n, int variable)
{
	assert(fcn_nl > 0 && fcn_n != NULL && fcn_n[0]);
	assert(variable == 1 || variable == 2 || variable == 0
			|| variable == -3 || variable == -4);

	/* preprocess name - init vars */
	struct xf_strb b = { .a = NULL }; /* cannot be static as function calls itself */
	xf_strb_construct(&b, 32);

	int rvl; /* return value */
	xf_strb_setf(&b,"%.*s", fcn_nl, fcn_n);
	char* n = b.a;
	/* preprocess name */
	/* process variable/whether it is expandable */
	int i = 0, var = 0;
	for (i += 0; n[i] != '\0' && n[i] != '[' && n[i] != ']'
			&& n[i] != '$'; i++);
	if (n[i] == '[') {
		i++;
		if (n[i] != ']') {
			lprintf(ERR "Unexpected character '%.*s"
					lB_RED"%c"_lB"%s' expected ']'.\n",
					i, n, n[i], n + i + 1);
			rvl = -17;
			goto exitpt;
		}
		assert(variable == -3);
		var = 2;
	} else if (n[i] == '$') {
		assert(variable == -3);
		var = 1;
	} else if (n[i] == '\0') {
		if (variable >= 0) var = variable;
		else var = 0; /* either unknwn or determined by presence */
		i--;
	}
	i++;
	if (n[i] != '\0') {
		lprintf(ERR "Unexpected character '%.*s"
				lB_RED"%c"_lB"%s' expected '\\0'.\n",
				i, n, n[i], n + i + 1);
		rvl = -18;
		goto exitpt;
	}
	if (variable >= 0);
	else if (var == 2) xf_strb_clip(&b, b.length - 3);
	else if (var == 1) xf_strb_clip(&b, b.length - 2);

	/* add/verify all parents */
	int parent = -1; /* topmost parent */
	for (i = b.length - 2; i >= 0; i--) {
		if (n[i] != '+' && n[i] != '=')
			continue;

		if (parent != -1) {
			n[i] = '-';
			continue;
		}
		parent = fcn_get(i, b.a, 1 + (n[i] == '+'));
		fcns_a[parent].defined = 1;
		if (parent < 0) {
			rvl = parent;
			goto exitpt;
		}
		n[i] = '-';
	}
	struct hashentry def = {
		.index = fcns_length,
	};
	/* Allocate new name (will be removed later if already exists) */
	int name_add_subreg;
	int name_add_offset;
	void *name_add = xf_mregion_allocv(fcn_names, b.length - 1,
			&name_add_subreg, &name_add_offset);
	assert(name_add_subreg < 16); /* handle it when fails */
	assert(name_add_offset < 4096); /* --||-- */
	memcpy(name_add, b.a, b.length - 1);

	struct hashentry *e = xf_htable_see(fcn_l, name_add, b.length - 1, &def);
	struct fcn_inf *f;
	assert(e != NULL); /* if this actually fails at some point, handle it */
	if (e->index == fcns_length) {
		fcns_length++;
		if(!fcns_expand(fcns_length)) {
			static int wrnonce = 0;
			if (wrnonce) {
				rvl = -12;
				goto exitpt;
			}
			lprintf(ERR "fcns_a at maximum capacity, cannot set "
					"'"lF_RED"%.*s"_lF"'\n",
					fcn_nl, fcn_n);
			wrnonce = 1;
			rvl = -12;
			goto exitpt;
		}
		f = &fcns_a[e->index];
		f->mod_index = 0;
		f->mod_count = 1;
		f->name_offset = name_add_offset;
		f->name_subreg = name_add_subreg;
		f->name_len = b.length - 1;
		f->variable = var; /* def to 0 */
		f->expands = parent != -1;
		if (f->expands)
			fcn_parent_set(e->index, parent);
		f->loaded = 0;
		f->defined = 0;
		f->parent = parent != -1 ? parent : 0;
		f->child_cnt = 0;

		e = xf_htable_see(fcn_l, b.a, b.length - 1, &def);
	} else {
		xf_mregion_rewind(fcn_names, name_add);
		f = &fcns_a[e->index];
		int refc = top_use != NULL ? refb_fcn_cnt(top_use, e->index) : 0;
		if (variable != -4 && f->variable != var) {
			if (!(f->mod_count == 1 && f->mod_index == 0) || f->defined) {
				lprintf(ERR "FCN '"lF_RED"%.*s"_lF
						"' extend mismatch - expected %s.\n",
						fcn_nl, fcn_n, !f->variable?"no extension":
						(f->variable==2?"array of fcns":"single fcn"));
				rvl = -15;
				goto exitpt;
			} /* if no mods defining and using, overload */
			if (refc)
				lprintf(WRN "Overriding '"lF_YELW"%s"_lF
						"' to expand %s.\n",
						b.a, !var ? "nothing" :
						(var==1?"single":"multi"));
			f->variable = var;
			f->child_cnt = 0; /* reset not required, but why not */
		}
		if (f->expands != (parent != -1)) {
			if (!(f->mod_count == 1 && f->mod_index == 0) || refc) {
				rvl = -14;
				goto exitpt;
			}
			lprintf(WRN "Overriding '%.*s' expands to "lBLD_"%s"_lBLD"\n",
					fcn_nl, fcn_n, (parent != -1) ? "true" : "false");
			f->expands = (parent != -1);
			if (f->expands) {
				fcn_parent_set(e->index, parent);
				f->parent = parent;
			} else {
				fcn_parent_unset(f->parent, e->index);
			}
		}
	}
	rvl = e->index;
exitpt:
	xf_strb_destruct(&b);
	return rvl;
}
/**
 * fcn_provider_get() - get a loaded functionality's providing mod
 * @fcn_index:	the functionality which's provider to seek
 *
 * Returns:	provider module index or fails execution
 */
static int fcn_provider_get(int fcn_index)
{
	assert(fcn_index >= 0 && fcn_index < fcns_length);
	struct fcn_inf *f = fcns_a + fcn_index;
	assert(f->loaded);
	int i, l;
	int e, b;
	for (i = 0, l = mods_length; i < l; i++) {
		if (!mods_a[i].additional || !mods_a[i].loaded)
			continue;
		for (e = 0, b = mods_a[i].fcn_cnt; e < b; e++) {
			if (mods_a[i].additional[e].index != fcn_index)
				continue;
			return i;
		}
	}
	assert(1 == 3); /* broken loaded reference: functionaliy isn't
			   provided by any loaded module */
}

/**
 * mod_fcn_set() - add a provider mod to a fcn
 * @mod_index:	the provider that given fcn entry should specify
 * @fcn_nl:	length of the function name in @fcn_n
 * @fcn_n:	functionality name that should have an entry that points to
 *		@mod_index(or that the fcn entry's fcn's mod count includes it)
 *
 * This function parses the module name in def string given to ce_mod_add(),
 * makes sure such entry exists and either increments its providers count or
 * specifies its provider as @mod_index.
 *
 * Return:	A negative value on failure, index of the fcn in fcns_a
 *		otherwise.
 */
static int mod_fcn_set(int mod_index, int fcn_nl, const char *fcn_n)
{
	assert(mod_index >= 0 && mod_index < mods_length);
	assert(fcn_n != NULL && fcn_nl > 0);
	int index = fcn_get(fcn_nl, fcn_n, -3);
	if (index < 0) return index;
	struct fcn_inf *f = fcns_a + index;
	if (f->mod_count == 0) {
		f->mod_count = 1;
		f->mod_index = 2;
	} else if (f->mod_count && !f->mod_index) {
		f->mod_count = 0;
		f->mod_index = mod_index;
	} else {
		f->mod_index++;
	}
	f->defined = 1;

	return index;
}
static int mod_fcn_unset(int mod_index, int fcn_index)
{
	if (!fcns_a[fcn_index].mod_count) {
		fcns_a[fcn_index].mod_count = 1;
		fcns_a[fcn_index].mod_index = 0;
		return 1;
	}

	assert(fcns_a[fcn_index].mod_index > 1);
	fcns_a[fcn_index].mod_index--;

	if (fcns_a[fcn_index].mod_index != 1)
		return 1;

	/* convert to .mod_count=0, .mod_index=[singlemod'sindex] */
	int i, fnd = -1;
	for (i = 0; i < mods_length; i++) {
		if (!mods_a[i].additional || i == mod_index)
			continue;
		int z;
		for (z = 0; z < mods_a[i].fcn_cnt; z++) {
			if (mods_a[i].additional[z].index != fcn_index)
				continue;
			assert(fnd == -1);
			fcns_a[fcn_index].mod_count = 0;
			fcns_a[fcn_index].mod_index = i;
			fnd = i;
#ifndef NDEBUG
			return 1; /* continue loop to check for errors on debug */
#endif
		}
	}
	assert(fnd != -1);
	return 1;
}

/**
 * mod_inf_use_print() - print debug info about fcns module uses
 * @minf:	module, possibly in %mods_a
 */
static inline void mod_inf_use_print(struct mod_inf *minf)
{
	assert(minf != NULL);

	int uinf_l;
	struct use_inf *uinf;
	char *uvers;
	mod_inf_use_get(minf, &uinf_l, &uinf, &uvers);

	int mname_l;
	const char *mname;
	mod_inf_name_get(minf, &mname_l, &mname);
	if (!uinf_l) {
		lprintf(DBG "Mod %.*s requires no functionality.\n",
			mname_l, mname);
		return;
	}
	lprintf(DBG "Mod %.*s requires ", mname_l, mname);

	for (int i = 0; i < uinf_l; i++) {
		lprintf(""lF_YELW"%.*s%s%.*s"_lF"%s",
				fcns_a[uinf[i].fcn_index].name_len,
				fcn_inf_name(fcns_a + uinf[i].fcn_index),
				uinf[i].ver_len ? " " : "",
				uinf[i].ver_len, uvers + uinf[i].ver_off,
				i + 1 < uinf_l ? ", " : "");
	}
	lputs(".");
}

/**
 * struct refb - stores the usage reference counts for fcns and mods
 *
 * Don't access the contents directly from ce-mod.c, the functions that handle
 * this are defined in ce-mod-refb.c
 */
struct refb;
/**
 * refb_construct() - initializes use buffer's values and allocates associated mem
 * @b:		buffer instance to initialize
 *
 * Matching call to free the associated memory refb_destruct().
 */
static void refb_construct(struct refb *b);
/**
 * refb_destruct() - free memory associated with given buffer
 * @b:		buffer to destruct
 *
 * Use this to free memory allocated by refb_construct().
 */
static void refb_destruct(struct refb *b);
/*
 * refb_duplicate() - initializes dest as a copy of src (in its state)
 * @dest:	destination structure, that's not allocated
 * @src:	source buffer that's contents to duplicate
 */
/*static void refb_duplicate(struct refb *dest, struct refb *src);*/
/*
 * refb_assign() - copies structure contents from src to dest
 * @dest:	structure to copy to, that's not allocated
 * @src:	structure to copy from
 *
 * After calling this, it will be safe to only further use either the
 * @dest or the @src, but not both.
 *
 * If the @dest is previously allocated, it must be refb_destruct()'ed or
 * otherwise a memory leak will be introduced.
 */
/*static void refb_assign(struct refb *dest, struct refb *src);*/
/**
 * refb_expand() - expand a buffer to hold fcns_length and mods_length
 * @b:		buffer to expand
 */
static void refb_expand(struct refb *b);
/**
 * refb_mod_ref() - increase reference count by 1 for corresponding mod
 * @b:		&struct refb to modify
 * @mod_index:	module that's referenced
 *
 * Return:	the total refcount after +1
 */
static int refb_mod_ref(struct refb *b, int mod_index);
/**
 * refb_mod_unref() - decrease reference count by 1 for corresponding mod
 * @b:		&struct refb to modify
 * @mod_index:	module that's dereferenced
 *
 * Return:	the total refcount after the dereference
 */
static int refb_mod_unref(struct refb *b, int mod_index);
static int refb_mod_cnt(struct refb *b, int mod_index);
static int refb_fcn_ref(struct refb *b, int fcn_index);
static int refb_fcn_unref(struct refb *b, int fcn_index);
static int refb_fcn_cnt(struct refb *b, int fcn_index);

#include "mod-refb.c"

__attribute__((constructor(130))) static void ce_mod_init()
{
	mods_a = malloc(sizeof(mods_a[0]) * mods_size);
	for (int i = 0; i < mods_size; i++) {
		/* initialized only the first time */
		mods_a[i].iter = 0;
	}
	fcns_a = malloc(sizeof(fcns_a[0]) * fcns_size);
	xf_htable_construct(fcn_l, 4/*16 buckets*/, sizeof(struct hashentry),
			xf_hash_hsieh_superfast);
	fcn_names = xf_mregion_create(128);

	lputs(INF "Module handler initialized.");
	lprintf(DBG "Struct sizes in bytes: mod_inf: "lF_BLUE"%tu"_lF", "
			"fcn_inf: "lF_BLUE"%tu"_lF", "
			"use_inf: "lF_BLUE"%tu"_lF" \n",
			sizeof(struct mod_inf), sizeof(struct fcn_inf),
			sizeof(struct use_inf));
}

static struct xf_strb b1 = { .a = NULL };
static struct xf_strb b2 = { .a = NULL };
static struct mod_inf_fcn *b3 = NULL;
static int b3_size = 6;
static struct xf_strb b4 = { .a = NULL };
static struct use_inf *b5 = NULL;
static int b5_size = 6;
__attribute__((destructor(130))) static void ce_mod_exit()
{
	if (b1.a != NULL)
		xf_strb_destruct(&b1);
	if (b2.a != NULL)
		xf_strb_destruct(&b2);
	if (b3 != NULL)
		free(b3);
	if (b4.a != NULL)
		xf_strb_destruct(&b4);
	if (b5 != NULL)
		free(b5);
	if (top_use != NULL) {
		refb_destruct(top_use);
		free(top_use);
	}

	xf_mregion_destroy(fcn_names);
	xf_htable_destruct(fcn_l);
	int i;
	for (i = 0; i < mods_length; i++) {
		if (mods_a[i].additional == NULL)
			continue;
		free(mods_a[i].additional);
	}
	free(mods_a);
	free(fcns_a);

	lputs(INF "Module handler destructed.");
}
size_t ce_mod_memcnt()
{
	size_t cnt = 0;
	if (b1.a)
		cnt += b1.size;
	if (b2.a)
		cnt += b2.size;
	if (b3)
		cnt += b3_size * sizeof(b3[0]);
	if (b4.a)
		cnt += b4.size;
	if (b5)
		cnt += b5_size * sizeof(b5[0]);

	if (mods_a) {
		cnt += mods_size * sizeof(mods_a[0]);
		/* cnt .additional mems */
		int i;
		for (i = 0; i < mods_length; i++) {
			struct mod_inf *m = mods_a + i;
			if (!m->additional)
				continue;
			cnt += m->name_off + m->name_len + m->ver_len
				+ sizeof(struct use_inf)
					* (m->use_cnt + m->use_live_size);

			int l;
			struct use_inf *u;
			mod_inf_use_get(m, &l, &u, NULL);
			cnt += m->use_cnt == 0 ? 0 : u[m->use_cnt - 1].ver_off
				+ u[m->use_cnt - 1].ver_len;

		/*lprintf(INF "memcnt of %i|%i mods_a[%.*s].additional: "
				lF_BLUE"%i"_lF".\n",
				m->use_cnt, u[m->use_cnt - 1].ver_off,
				m->name_len,
				((char *)m->additional) + m->name_off,
				cnt - a);*/

		}
	}
	if (fcns_a)
		cnt += fcns_size * sizeof(fcns_a[0]);

	if (fcn_names)
		cnt += xf_mregion_memcnt(fcn_names);

	if (fcn_lookup.buckets)
		cnt += xf_htable_memcnt(fcn_l);
	return cnt;

}

const char *ce_mod_strerr(int err)
{
	assert(err < 0);

	switch (err)
	{
		/* ce_mod_add() err values */
		case -1: return "Modules array at maximum, cannot add.";
		case -2: return "Invalid functionality definition.";
		case -3: return "Invalid character in functionality definition.";
		case -4: return "Invalid definition for module, expected '|' or '\\0'.";
		/* fcn_get() err values */
		case -11: return "Functionality definition may only extend "
			  "one mod(multiple '+' found).";
		case -12: return "Maximum amount of functionalities defined.";
		case -13: return "Mismatched definitions - expandable.";
		case -14: return "Mismatched definitions - expands.";
		case -15: return "Functionality's extension mismatch.";
		case -16: return "Did not expect expandability flag for fcn.";
		case -17: return "Functionality extendable bracket mismatch.";
		case -18: return "Unexpected character after functionality variable.";
		/* use_compile */
		case -41: return "Invalid character in use string.";
		/* use_exec */
		case -61: return "Incompatible functionality already loaded.";
		case -62: return "No providing modules for required functionality.";
		case -63: return "Cannot find suitable provider for functionality.";
		/* use_exec_fcn_init */
		case -71: return "No compatible providers found for given fcn version.";
		/* mod_load */
		case -101:return "Module load function failed.";
		case -102:return "Failed to satisfy dependencies for module.";
		case -103:return "Cannot load module - provided fcn already used.";
		case -104:return "Cannot load module - conflicted fcn provider required.";
		case -105:return "Cannot load module - conflicted fcn provider unload failure.";
		/* mod_use */
		case -121:return "The ce-main mod is not supposed to be active during any init.";
		/* mod_unload */
		case -141:return "Cannot unload module - module is in use.";
		/* ce_mod_rm */
		case -201:return "Cannot remove module as it is still in use.";
		/* random */
		case -8999: return "Unfinished functionality :(";
	};
	return "Invalid error.";
}



static int use_exec(struct refb *refs, int mod_index,
		int in_len, const struct use_inf *in,
		const char *vers);

static int mod_unload(struct refb *refs, int mod_index);

/**
 * mod_load() - Attempts to load a module
 * @refs:	modules used buffer(&struct refb)
 * @mod_index:	index of the module to load
 *
 * Executes the module's use info(compiled &struct use_inf array) and calls
 * its load function.
 *
 * Return:	negative on failure
 */
static int mod_load(struct refb *refs, int mod_index)
{
	assert(top_use != NULL);

	struct mod_inf *minf = mods_a + mod_index;

	/* is mod already loaded? */
	if (minf->loaded)
		return 1;

	assert(!minf->loading);
	minf->loading = 1;

	int rval = 0;

	/* Get name/vers info */
	const char *name;
	int name_len;
	mod_inf_name_get(minf, &name_len, &name);
	const char *vers;
	int vers_len;
	mod_inf_vers_get(minf, &vers_len, &vers);

	/* Eliminate all functionality conflicts */
	int i, l;
	for (i = 0, l = minf->fcn_cnt; i < l; i++) {
		int fcn_index = minf->additional[i].index;
		//int fcn_used = *(ubuf->fcns_used + (fcn_index / 32))
		//	& (1 << (fcn_index % 32));
		/* check given fcn usage -- if it's used, fail instanty */
		int fcn_used = refb_fcn_cnt(refs, fcn_index);
		if (fcn_used) {
			lprintf(DBG "Cannot load module %.*s %.*s "
					"- fcn %.*s already referenced.\n",
					name_len, name, vers_len, vers,
					fcns_a[fcn_index].name_len,
					fcn_inf_name(fcns_a + fcn_index)
					);
			rval = -103;
			goto exitp;
		}
		if (!fcns_a[fcn_index].loaded)
			continue;

		int e = mod_unload(refs, fcn_provider_get(fcn_index));
		if (e >= 0) /* successful conflict avoid */
			continue;
		if (e == -141) {
			lprintf(DBG "Cannot load module %.*s %.*s "
					"- conflicting fcn %.*s provider required.\n",
					name_len, name, vers_len, vers,
					fcns_a[fcn_index].name_len,
					fcn_inf_name(fcns_a + fcn_index)
					);
			rval = -104;
			goto exitp;
		} else if (e < 0) {
			lprintf(DBG "Cannot load module %.*s %.*s "
					"- conflicting fcn %.*s provider unload "
					"failure.\n",
					name_len, name, vers_len, vers,
					fcns_a[fcn_index].name_len,
					fcn_inf_name(fcns_a + fcn_index));
			rval = -105;
			goto exitp;
		}
	}

	/* Execute dependencies(used mods) */
	int uinf_len;
	struct use_inf *uinf;
	char *uvers;
	mod_inf_use_get(minf, &uinf_len, &uinf, &uvers);
	i = use_exec(refs, mod_index, uinf_len, uinf, uvers);
	if (i < 0) {
		lprintf(WRN "Failed to satisfy dependencies for module %.*s %.*s.%i\n",
				name_len, name, vers_len, vers, i);
		rval = -102;
		goto exitp;
	}


	lprintf(INF "Loading module %.*s %.*s..\n",
			name_len, name, vers_len, vers);
	int fcnr = 0;
	if (minf->load != NULL)
		fcnr = minf->load();

	if (fcnr < 0) {
		lprintf(WRN "Failed to load module %.*s %.*s(returned %i).\n",
				name_len, name, vers_len, vers, fcnr);
		rval = -101;
		goto exitp;
	}
	if (minf->load != NULL)
		lprintf(INF "Module "lF_BLUE"%.*s %.*s"_lF
				"(returned "lF_BLUE"%i"_lF") loaded.\n",
				name_len, name, vers_len, vers, fcnr);
	else
		lprintf(INF "Module "lF_BLUE"%.*s %.*s"_lF
				" loaded.\n",
				name_len, name, vers_len, vers);

	/* Update ->loaded info for mod and provided fcns */
	minf->loaded = 1;
	struct mod_inf_fcn *mfcns = minf->additional;
	for (i = 0, l = minf->fcn_cnt; i < l; i++) {
		fcns_a[mfcns[i].index].loaded = 1;
	}

exitp:
	minf->loading = 0;
	if (rval == -101) { /* undo refs on minf->load() failure */
		for (i = 0; i < uinf_len; i++) {
			if (uinf[i].incompat)
				return -8999;
			int f = uinf[i].fcn_index;
			refb_fcn_unref(refs, f);
			refb_mod_unref(refs, fcn_provider_get(f));
		}
	}

	return rval;
}

static int mod_unload(struct refb *refs, int mod_index)
{
	assert(mod_index >= 0 && mod_index < mods_length);
	assert(refs != NULL);
	struct mod_inf *m = mods_a + mod_index;
	assert(m->loaded);

	const char *n;
	int n_l;
	const char *v;
	int v_l;
	mod_inf_name_get(m, &n_l, &n);
	mod_inf_vers_get(m, &v_l, &v);

	int x;
	/* check if module can be dropped */
	if ((x = refb_mod_cnt(refs, mod_index)) > 0) {
		lprintf(DBG "Cannot unload module %.*s %.*s "
				"- it is referenced %i times.\n",
				n_l, n, v_l, v, x);
		return -141;
	}

	/* Unload module */
	if (m->unload != NULL)
		x = m->unload();
	else
		x = 0;
	assert(x >= 0);
	lprintf(INF "Module "lF_BLUE"%.*s %.*s"_lF" unloaded.\n",
			n_l, n, v_l, v);

	m->loaded = 0;
	/* update provided fcn's.loaded */
	int i, l;
	struct mod_inf_fcn *mfcns = m->additional;
	for (i = 0, l = m->fcn_cnt; i < l; i++) {
		int f = mfcns[i].index;
		assert(fcns_a[f].loaded);
		assert(!refb_fcn_cnt(refs, f));
		fcns_a[f].loaded = 0;
	}

	/* dereference the deps */
	struct use_inf *mdeps;
	char *uvers;
	mod_inf_use_get(m, &l, &mdeps, &uvers);
	for (i = 0; i < l; i++) {
		if (mdeps[i].incompat)
			return -8999;
		int f = mdeps[i].fcn_index;
		refb_fcn_unref(refs, f);
		int p = fcn_provider_get(f);
		x = refb_mod_unref(refs, p);
		/* See if module is no longer used. */
		/* unload on not-use should happen at the end of init */
		/*if (!x) {
			mod_unload(refs, p);
		}*/
	}
	return 0;
}

/**
 * ver_mthan() - Answers the question is a more than b
 * @a_len:	length of @a
 * @a:		the first version string component
 * @b_len:	length of @b
 * @b:		the second version string component
 *
 * Return:	If a > b, returns 1; if a == b returns 0 and if a < b returns
 *		(-1).
 */
static int ver_mthan(int a_len, const char *a, int b_len, const char *b)
{
	int a_i = 0, b_i = 0;
	while (1) {
		if (a_i >= a_len && b_i >= b_len)
			return 0;
		else if (a_i >= a_len && b_i <= b_len)
			return -1; /* 'ba' is less than 'baa' */
		else if (b_i >= b_len)
			return 1;

		if (a[a_i] >= '1' && a[a_i] <= '9') { /* no leading zeroes */
			if (b[b_i] < '1' || b[b_i] > '9')
				return -1; /* numbers are less than letters */
			int a_s = a_i;
			int b_s = b_i;
			/* manual base 10 str-integer comparison yay */
			for (a_i += 1; a_i < a_len && a[a_i] >= '0' && a[a_i] <= '9'; a_i++);
			for (b_i += 1; b_i < b_len && b[b_i] >= '0' && b[b_i] <= '9'; b_i++);
			if ((a_i - a_s) > (b_i - b_s))
				return 1;
			else if ((a_i - a_s) < (b_i - b_s))
				return -1;

			int zl = (a_i - a_s);
			for (int z = 0; z < zl; z++) {
				char a_c = a[a_s + z];
				char b_c = b[b_s + z];
				if (a_c == b_c)
					continue;
				else if (a_c > b_c)
					return 1;
				else
					return -1;
			}
			/* number was equal.. */
			continue;
		} else if (b[b_i] >= '0' && b[b_i] <= '9') {
			return 1; /* a hadn't got a number at this pos */
		} else if (a[a_i] > b[b_i]) {
			return 1;
		} else if (a[a_i] < b[b_i]) {
			return -1;
		}
		a_i++;
		b_i++;
	}
	assert(1==3); /* yet another loop that exits the entire function */
	return -99;
}

/**
 * ver_compare() - compare two version strings for which is greater
 * @a_len:	length of first ver string @a
 * @a:		first version string
 * @b_len:	lenght of second ver string @b
 * @b:		second ver string
 *
 * Return:	Positive if @a is greater than @b, negative if @b is greater
 *		than @a, %0 if they're equal.
 */
static int ver_compare(int a_len, const char *a, int b_len, const char *b)
{
	int a_i = 0;
	int b_i = 0;

	/* mess with epochs */
	for (a_i = 0; a_i < a_len && a[a_i] != ':'; a_i++);
	for (b_i = 0; b_i < b_len && b[b_i] != ':'; b_i++);
	bool a_zero = a_i == a_len || (a_i == 1 && a[0] == '0');
	bool b_zero = b_i == b_len || (b_i == 1 && b[0] == '0');
	if (a_zero && !b_zero)
		return -1;
	else if (!a_zero && b_zero)
		return 1;

	if (a_i == a_len) a_i = 0;
	else a_i++;

	if (b_i == b_len) b_i = 0;
	else b_i++;

	while (1) {
		int a_s = a_i;
		int b_s = b_i;
		for (a_i = 0; a_i < a_len && a[a_i] != '.'; a_i++);
		for (b_i = 0; b_i < b_len && b[b_i] != '.'; b_i++);

		int j = ver_mthan(a_i - a_s, a + a_s, b_i - b_s, b + b_s);
		if (j)
			return j;

		if (a_i <= a_len && b_i <= b_len)
			continue;
		else if (a_i == a_len && b_i == b_len)
			return 0;
		else if (a_i == a_len)
			return -1; /* a < b, terminates faster */
		else
			return 1; /* a > b */
	}

	assert(1==2); /* loop exits function */
	return 1;
}

/**
 * ver_compatible() - test if a version string is compatible for a target
 * @t_len:	length of the target string @t
 * @t:		target (the required version "at least") string
 * @v_len:	length of the string to test
 * @v:		string to test
 *
 * Return:	Negative if they're incompatible, positive if they're
 *		compatible.
 */
static int ver_compatible(int t_len, const char *t, int v_len, const char *v)
{
	int t_i = 0;
	int v_i = 0;


	/* check epochs */
	for (t_i = 0; t_i < t_len && t[t_i] != ':'; t_i++);
	for (v_i = 0; v_i < v_len && v[v_i] != ':'; v_i++);

	/* 0-test */
	bool t_zero = t_i == t_len || (t_i == 1 && t[0] == '0');
	bool v_zero = v_i == v_len || (v_i == 1 && v[0] == '0');
	if (t_zero != v_zero)
		return -1;
	else if (t_zero);
	else if (t_i != v_i || memcmp(v, t, t_i - 1)) /* epochs differ? */
		return -1;

	if (t_i == t_len) t_i = 0;
	else t_i++;

	if (v_i == v_len) v_i = 0;
	else v_i++;

	/* check that the version is larger than or equal to target */
	while (1) {
		int t_s = t_i;
		int v_s = v_i;
		for (t_i += 0; t_i < t_len && t[t_i] != '.'; t_i++);
		for (v_i += 0; v_i < v_len && v[v_i] != '.'; v_i++);

		int c = ver_mthan(t_i - t_s, t + t_s, v_i - v_s, v + v_s);
		if (!c) {
			if (!((t_i - t_s) + (v_i - v_s))) /* end reached */
				return 1;
			t_i++;
			v_i++;
			continue;
		} else if (c < 0) {
			return 1; /* value more than target */
		} else {
			return -1; /* value is less than target */
		}
	}

	assert(1==3); /* loop exits function */
	return 1;
}

/**
 * mod_inf_fcn_get() - get the information about a mod providing given fcn
 * @mod_index:	index of the module that should provide given fcn
 * @fcn_index:	the target fcn, about which information is required
 * @prov_ver_l:	provided version string len output when given fcn is provided
 * @prov_ver:	provided version string output when given fcn is provided
 *
 * Return:	negative if given mod doesn't provide given fcn, positive if
 *		it does
 */
static int mod_inf_fcn_get(int mod_index, int fcn_index, int *prov_ver_l,
		const char **prov_ver)
{
	struct mod_inf *minf = mods_a + mod_index;
	int i, l, ver_off = 0;
	for (i = 0, l = minf->fcn_cnt; i < l; i++) {
		struct mod_inf_fcn *mf = minf->additional + i;
		if (mf->index != fcn_index) {
			ver_off += mf->ver_len;
			continue;
		}
		*prov_ver_l = mf->ver_len;
		*prov_ver = mod_inf_fcn_vers_get(minf) + ver_off;
		return 1;
	}
	return -1;
}

/**
 * use_exec_fcn_init() - finds and initializes the preferred mod for used fcn
 * @refs:	reference count buffer(&struct refb)
 * @fcn_index:	fcn that needs to be initialized
 * @req_ver_l:	req version @req_ver length
 * @req_ver:	version required by use
 *
 * This function doesn't increase the reference count.
 *
 * Return:	negative on failure, selected module index on success
 */
static int use_exec_fcn_init(struct refb *refs,
		int fcn_index, int req_ver_l, const char *req_ver)
{
	assert(refs != NULL);

	int rval = -8999;
	assert(fcn_index >= 0 && fcn_index < fcns_length);
	struct fcn_inf *f = fcns_a + fcn_index;

	/* Make a list of providers */
	struct provider {
		uint16_t mod_index : 11; /* max2047 */
		uint16_t : 4;
		uint16_t works : 1; /* whether or not given provider is suitable */
				    /* initialized to 1 */
		int prov_ver_l;
		const char *prov_ver;
	} *prov_a;
	int prov_length; /* no prov_a de-alloc if this is less than 2 */
	if (f->mod_count == 0) { /* make a list of 1 provider */
		struct provider sprv = {
			.mod_index = f->mod_index,
			.works = 1,
		};
		int test = mod_inf_fcn_get(f->mod_index, fcn_index,
				&sprv.prov_ver_l, &sprv.prov_ver);
		if (test < 0) {
			int n_l = 0;
			const char *n;
			int v_l = 0;
			const char *v;
			lputs(INF "AA");
			mod_inf_name_get(mods_a + f->mod_index, &n_l, &n);
			mod_inf_vers_get(mods_a + f->mod_index, &v_l, &v);

			assert(mods_a[f->mod_index].additional);
			lprintf(ERR "Mod %.*s %.*s specified to provide fcn"
					" %i:%.*s (%.*s) did not. %i\n", n_l, n, v_l, v,
					f->mod_index,f->name_len, fcn_inf_name(f),
					req_ver_l, req_ver, mods_a[f->mod_index]
					.fcn_cnt);
		}
		assert(test >= 0); /* fails on broken index */
		prov_a = &sprv;
		prov_length = 1;
	} else if (f->mod_index == 0) { /* no providers */
		prov_length = 0;
		prov_a = NULL;
	} else { /* put the 2+ providers into a list */
		assert(f->mod_index != 1); /* single mod should be referred to by id */
		prov_a = malloc(sizeof(prov_a[0]) * f->mod_index);

		int e;
		prov_length = 0;
		for (e = 0; e < mods_length; e++) {
			int prov_ver_l;
			const char *prov_ver;
			if (mod_inf_fcn_get(e, fcn_index, &prov_ver_l, &prov_ver) < 0)
				continue;
			prov_a[prov_length].mod_index = e;
			prov_a[prov_length].works = 1;
			prov_a[prov_length].prov_ver_l = prov_ver_l;
			prov_a[prov_length].prov_ver = prov_ver;

			prov_length++;
			if (prov_length == f->mod_index /* in mod_count mode */)
				break;
		}
		/* fail on miscounted provider count */
		assert(prov_length == f->mod_index);
	}

	/* See if a providing module is already loaded/ing */
	int i, prevprov = -1;
	for (i = 0; i < prov_length; i++) {
		struct mod_inf *minf = mods_a + prov_a[i].mod_index;
		if (!minf->loaded && !minf->loading)
			continue;
		/* test for multiple init'ed providers for a fcn */
		assert(prevprov == -1);
		if (refb_mod_cnt(refs, prov_a[i].mod_index) > 0) {
			rval = ver_compatible(req_ver_l, req_ver,
					prov_a[i].prov_ver_l,
					prov_a[i].prov_ver) >= 0
				? prov_a[i].mod_index : -1;
			goto exitpt;
		}
		prevprov = prov_a[i].mod_index;
#ifdef NDEBUG
		break;
#endif
	}

	/* Pick out the best provider */
	int lst, prov_valid = prov_length;
	while (prov_valid > 0) {
		lst = -1;
		for (i = 0; i < prov_length; i++) {
			if (!prov_a[i].works)
				continue;
			if (ver_compatible(req_ver_l, req_ver, prov_a[i].prov_ver_l,
						prov_a[i].prov_ver) < 0) {
				prov_a[i].works = 0;
				prov_valid--;
				continue;
			}
			if (lst == -1) {
				lst = i;
				continue;
			}
			if (ver_compare(prov_a[lst].prov_ver_l,
						prov_a[lst].prov_ver,
						prov_a[i].prov_ver_l,
						prov_a[i].prov_ver) > 0)
				continue;
			lst = i;
		}
		if (prov_valid <= 0)
			break;

		int nl;
		const char *n;
		int vl;
		const char *v;

		/* Attempt initialisation */
		if (prevprov == lst) {
			/* Previously loaded, but not referenced */
			rval = prov_a[lst].mod_index;
			goto exitpt;
		} else if (prevprov != -1) {
			/* Unload previous, less suitable mod */
			mod_unload(refs, prevprov);
			prevprov = -1;
		}

		int x = mod_load(refs, prov_a[lst].mod_index);
		if (x < 0) {
			mod_inf_name_get(mods_a + prov_a[lst].mod_index, &nl, &n);
			mod_inf_vers_get(mods_a + prov_a[lst].mod_index, &vl, &v);
			lprintf(WRN "Unsuccessful load of provider "
					lF_RED"%.*s %.*s"_lF
					" for fcn "lF_RED"%.*s %.*s"_lF".\n",
					nl, n, vl, v,
					f->name_len, fcn_inf_name(f),
					req_ver_l, req_ver);
			prov_a[lst].works = 0;
			prov_valid--;
			continue;
		}
		rval = prov_a[lst].mod_index;
		goto exitpt;
	}
	/* if (!prov_valid) { */
	lprintf(ERR "Failed to find a provider mod for fcn "
			lF_RED"%.*s %.*s"_lF".\n", f->name_len,
			fcn_inf_name(f), req_ver_l, req_ver);
	rval = -1;
	/* } */

exitpt:	;
	if (prov_length >= 2) {
		free(prov_a);
	}
	return rval;
}

/**
 * use_exec() - executes use information yielded by use_compile
 * @refs:	&struct refb that is used to keep the information on what
 *		functionality/mod is required and what can be uninitialized on
 *		demand
 * @mod_index:	the module currently being initialized
 * @in_len:	how many &struct use_inf's has been defined in @in
 * @in:		&struct use_inf's that specify the mods to initialize
 * @vers:	where the associated version strings are stored (as referenced
 *		to by &struct use_inf)
 *
 * Return:	negative on failure
 */
static int use_exec(struct refb *refs, int mod_index,
		int in_len, const struct use_inf *in,
		const char *vers)
{
	assert(mod_index >= 0 && mod_index < mods_length);
	assert(refs != NULL);

	int rval = 0;

	int i;
	for (i = 0; i < in_len; i++) {
		const struct use_inf *u = in + i;
		struct fcn_inf *f = fcns_a + u->fcn_index;
		if (u->incompat && f->loaded) {
			rval = -61;
			goto exitpt;
		}

		if (f->loaded) { /* Functionality already loaded, reference it */
			refb_mod_ref(refs, fcn_provider_get(u->fcn_index));
			refb_fcn_ref(refs, u->fcn_index);
			continue;
		}

		if (f->mod_count == 1 && f->mod_index == 0) {
			rval = -62;
			goto exitpt;
		}

		int tmod = use_exec_fcn_init(refs,
				u->fcn_index, u->ver_len, u->ver_off + vers);
		if (tmod < 0) {
			lprintf(WRN "Cannot find functionality provider for "
					"%.*s %.*s.\n",
					f->name_len, fcn_inf_name(f),
					u->ver_len, u->ver_off + vers);
			rval = -63;
			goto exitpt;
		}
		refb_mod_ref(refs, tmod);
		refb_fcn_ref(refs, u->fcn_index);
	}

exitpt:	;
	if (rval < 0) { /* undo previous refs */
		for (i--; i >= 0; i--) {
			const struct use_inf *u = in + i;
			/* safe usage of fcn_provider_get(), as previous loop
			 * initialized it */
			refb_mod_unref(refs, fcn_provider_get(u->fcn_index));
			refb_fcn_unref(refs, u->fcn_index);
		}
	}
	return rval;
}

/**
 * use_compile() - compiles a use string
 * @use:	input use string, see &struct ce_mod for details
 * @out_len:	where it stores the output &struct use_inf array @out's length
 * @out:	output array of &struct use_inf's, of length @out_l
 * @vers_len:	total length of version strings in @vers
 * @vers:	version string - note that individual versions are not null
 *		seperated from eachother, strlen(@vers) equals @vers_len,
 *		however the null terminator in the end of all the version
 *		strings is not required by use_exec().
 *
 * Note that both @out and @vers are temporary, in that when use_compile()
 * is called again, their contents will change.
 *
 * Return:	negative on error
 */
static int use_compile(const char *use,
		int *out_len, struct use_inf **out,
		int *vers_len, char **vers)
{
	if (!b4.a)
		xf_strb_construct(&b4, 64);
	else
		xf_strb_clear(&b4);
	if (!b5)
		/* struct use_inf */ b5 = malloc(b5_size * sizeof(b5[0]));
	int b5_length = 0;

	int i = 0;
	const char *d = use;
	for (i += 0; isspace(d[i]) || d[i] == ';'; i++);
	while (d[i]) {
		int e = b5_length;
		b5_length++;
		if (b5_length > b5_size) {
			b5_size *= 2;
			b5 = realloc(b5, sizeof(b5[0]) * b5_size);
			assert(b5 != NULL);
		}
		b5[e].incompat = 0;
		b5[e].end = 0;
		b5[e].after = 0;
		for (i += 0; d[i] == '!' || d[i] == '#' || d[i] == '&'; i++) {
			if (d[i] == '!') b5[e].incompat = 1;
			else if (d[i] == '#') b5[e].end = 1;
			else if (d[i] == '&') b5[e].after = 1;
		}
		int start = i;
		for (i += 0; !isspace(d[i]) && d[i] != '\0' && d[i] != ';'; i++);
		int end = i;
		b5[e].fcn_index = fcn_get(end - start, d + start, -4);
		for (i += 0; isspace(d[i]); i++);
		int verstart = i;
		for (i += 0; !isspace(d[i]) && d[i] != '\0' && d[i] != ';'; i++);
		b5[e].ver_off = b4.length - 1;
		b5[e].ver_len = i - verstart;
		if (i != verstart) { /* version was specified */
			xf_strb_appendf(&b4, "%.*s", i - verstart, d + verstart);
		}
		int fnd = 0;
		for (i += 0; isspace(d[i]) || d[i] == ';'; i++) {
			if (d[i] == ';') fnd = 1;
		}
		if (!fnd && d[i]) {
			lprintf(ERR "Invalid character in use string: "
					"'%.*s"lB_RED"%c"_lB"%s', expected ';' or '\\0'.",
					i, d, d[i], d + i + 1);
			return -41;
		}
	}
	/*int e; // what's the meaning of this?
	for (e = 0; e < b5_length; e++)
		fcns_a[b5[e].fcn_index].used = 1;*/
	*out_len = b5_length;
	*out = b5;
	*vers_len = b4.length - 1;
	*vers = b4.a;
	//lprintf(DBG "Compiled %.*s\n", *vers_len, *vers);
	return 0;
}

int ce_mod_add(const struct ce_mod *mod)
{
	assert(top_use == NULL);
	const char *d = mod->def;

	/* to determine whether to refb_expand() */
	int fcns_oldlen = fcns_length;

	/* add module entry */
	int n; /* index in mods_a */
	if (mods_count == mods_length) {
		if (mods_length + 1 > mods_max) {
			lprintf(ERR "mods_max overflow, cannot add module '%s'.\n",
					mod->def);
			return -1;
		}
		n = mods_length;
		mods_length++;
		mods_expand(mods_length);
		/* just in case free() is called for it before actual alloc */
		mods_a[n].additional = NULL;
		/* definitely expand refb */
		fcns_oldlen = -1;
	} else {
		for (n = 0; n < mods_length && mods_a[n].additional; n++);
		assert(n < mods_length);
	}
	mods_count++;
	int err = 0;
	struct mod_inf *minf = mods_a + n;
	minf->iter++;

	if (!b1.a)
		xf_strb_construct(&b1, 64);
	else
		xf_strb_clear(&b1);
	if (!b2.a)
		xf_strb_construct(&b2, 64);
	else
		xf_strb_clear(&b2);
	if (!b3)
		b3 = malloc(sizeof(b3[0]) * b3_size);
	int b3_length = 0;

	/* parse definition */
	int i;
	for (i = 0; isspace(d[i]); i++);
	int start = i;
	for (i += 0; !isspace(d[i]) && d[i] != '\0'; i++);
	int end = i;
	xf_strb_appendf(&b1, "%.*s", end - start, d + start);
	minf->name_len = end - start;

	for (i += 0; isspace(d[i]); i++);
	int verstart = i;
	for (i += 0; !isspace(d[i]) && d[i] != '\0' && d[i] != '|'; i++);
	if (verstart != i)
		xf_strb_appendf(&b1, "%.*s", i - verstart, d + verstart);
	minf->ver_len = i - verstart;

	for (i += 0; isspace(d[i]); i++);
	if (d[i] != '|' && d[i] != '\0') {
		lprintf(ERR "Invalid definition for module: "
				"'%.*s"lB_RED"%c"_lB"%s'\n",
				i, d, d[i], d + i + 1);
		err = -4;
		goto exitp;
	}
	if (d[i] == '|')
		i++;
	for (i += 0;  d[i] == ';' || isspace(d[i]); i++);
	/* get defined fcns */
	while (d[i]) {
		/* buf3 for entry */
		b3_length++;
		if (b3_size < b3_length) {
			b3_size *= 2;
			b3 = realloc(b3, sizeof(b3[0]) * b3_size);
		}
		/* get name */
		start = i;
		for (i += 0; d[i] != ';' && d[i] != '\0' && !isspace(d[i]); i++);
		end = i;
		int c = mod_fcn_set(n, end - start, d + start);
		if (c < 0) {
			lprintf(ERR "%s At: '%.*s"lB_RED"%c"_lB"%s'\n", ce_mod_strerr(c),
					start, d, d[start], d + start + 1);
			err = c;
			goto exitp;
		}
		assert(c < (1 << 11));
		/*lprintf(DBG "c: %i for %s \n", c, b1.a);*/
		b3[b3_length - 1].index = c;
		/* get ver */
		for (i += 0; isspace(d[i]); i++);
		verstart = i;
		for (i += 0; d[i] != ';' && d[i] != '\0' && !isspace(d[i]); i++);
		if (i != verstart) {
			xf_strb_appendf(&b2, "%.*s", i - verstart, d + verstart);
		}
		b3[b3_length - 1].ver_len = i - verstart;

		/*lprintf(DBG "FCN parsed - '"lF_CYA"%.*s"_lF"' "
				"v:'"lF_CYA"%.*s"_lF"'\n",
				fcns_a[c].name_len, fcn_inf_name(fcns_a + c),
				i - verstart, d + verstart);*/

		int sepfnd = 0;
		for (i += 0; (isspace(d[i]) || d[i] == ';') && d[i] != '\0'; i++) {
			if (d[i] == ';') sepfnd = 1;
		}
		if (!sepfnd && d[i]) {
			lprintf(ERR "Invalid character in module definition: "
					"'%.*s"lB_RED"%c"_lB"%s' expected ';' or '\\0'.\n",
					i, d, d[i], d + i + 1);
			err = -3;
			goto exitp;
		}
	}
	/* compile mod->use */
	int uinf_len = 0;
	struct use_inf *uinf;
	int uvers_len = 0;
	char *uvers;
	err = use_compile(mod->use, &uinf_len, &uinf, &uvers_len, &uvers);
	if (err < 0)
		goto exitp;

	/* alloc and fill minf->additional memory */
	minf->additional = malloc(
			b3_length * sizeof(b3[0]) /* mod_inf_fcn arr */
			+ (b2.length - 1) /* mod_inf_fcn's ver strs */
			+ (b1.length - 1) /* mod name & ver */
			+ uinf_len * sizeof(uinf[0]) /* use_inf arr */
			+ uvers_len /* use_inf's ver strs */);
	/* mod_inf_fcn arr */
	memcpy(minf->additional, b3, b3_length * sizeof(b3[0]));
	/* mod_inf_fcn's ver strs */
	memcpy(minf->additional + b3_length, b2.a, b2.length - 1);
	/* name offset, mod name & ver */
	minf->name_off = b3_length * sizeof(b3[0]) + (b2.length - 1);
	memcpy(((char *)minf->additional) + minf->name_off, b1.a, b1.length - 1);
	/* use_inf arr */
	int mnend = minf->name_off + minf->name_len + minf->ver_len;
	memcpy(((char *)minf->additional) + mnend, uinf, sizeof(uinf[0]) * uinf_len);
	/* use_inf's ver strs */
	memcpy(((char *)minf->additional) + mnend + sizeof(uinf[0]) * uinf_len,
			uvers, uvers_len);


	/* initialize the rest */
	minf->loaded = 0;
	minf->loading = 0;
	/*minf->used = 0; // What's the meaning of this? */

	assert(b3_length < (1 << 6) - 1);
	minf->fcn_cnt = b3_length;
	minf->load = mod->load;
	minf->unload = mod->unload;
	minf->use_cnt = uinf_len;
	minf->use_live_cnt = 0;
	minf->use_live_size = 0;

	lprintf(INF "Module "lF_GRE"%.*s"_lF" (id%2i) { ",
			minf->name_len, b1.a, n);

	for (i = 0; i < minf->fcn_cnt; i++) {
		struct fcn_inf *fin = fcns_a + minf->additional[i].index;
		if (fin->expands) {
			struct fcn_inf *pin = fcns_a + fin->parent;
			int plen = pin->name_len;
			lprintf(""lF_YELW"%.*s%c%.*s",
					plen, fcn_inf_name(pin),
					pin->variable == 1 ? '=' : '+',
					fin->name_len - plen - 1,
					fcn_inf_name(fin) + plen + 1);
		} else {
			lprintf(""lF_YELW"%.*s",
				fin->name_len, fcn_inf_name(fin));
		}
		lprintf("%s"_lF"; ",
				!fin->variable ? "" :
				(fin->variable == 1 ? "$": "[]"));
	};
	if (!minf->use_cnt) {
		lputs("} added.");
		goto exitp;
	}

	lprintf("} [ ");
	for (i = 0; i < minf->use_cnt; i++) {
		lprintf("%.*s%s%.*s""%s",
				fcns_a[uinf[i].fcn_index].name_len,
				fcn_inf_name(fcns_a + uinf[i].fcn_index),
				uinf[i].ver_len ? " " : "",
				uinf[i].ver_len, uvers + uinf[i].ver_off,
				i + 1 < minf->use_cnt ? ", " : "");
	}
	lputs(" ] added.");

exitp:
	if (err < 0) {
		for (i = 0; i < b3_length; i++)
			mod_fcn_unset(n, b3[i].index);

		free(mods_a[n].additional);
		mods_a[n].additional = NULL;
		mods_count--;
		return err;
	}

	if (top_use != NULL && fcns_oldlen != fcns_length)
		refb_expand(top_use);

	struct id_t id = {
		.index = n,
		.iter = minf->iter,
		.iserr = 0,
	};
	return *(int *)&id;
}

int ce_mod_rm(int mod_id)
{
	struct id_t *id = (struct id_t *) &mod_id;
	assert(!id->iserr);
	assert(id->index < mods_length);
	assert(mods_a[id->index].iter == id->iter);
	int n = id->index;

	if (mods_a[n].loaded) {
		int x = mod_unload(top_use, n);
		if (x < 0)
			return x;
	}

	int name_l;
	const char *name;
	mod_inf_name_get(mods_a + n, &name_l, &name);

	lprintf(INF "Module " lF_RED "%.*s" _lF " removed.\n",
			name_l, name);

	int i, l;
	for (i = 0, l = mods_a[n].fcn_cnt; i < l; i++)
		mod_fcn_unset(n, mods_a[n].additional[i].index);

	free(mods_a[n].additional);
	mods_a[n].additional = NULL;
	mods_count--;

	if (n != mods_length - 1)
		return 0;

	/* remove trailing NULL entries */
	for (i = mods_length - 1; i >= 0 && mods_a[i].additional == NULL; i--) {
		mods_length--;
	}
	return 0;
}

/**
 * DOC: static int root_mod;
 * This contains the module index that was first to call ce_mod_use().
 */
static int root_mod = -1;

static int mod_use(int mod_index, const char *use)
{
	static int use_level = 0;
	if (root_mod == mod_index && use_level != 0) {
		lputs(ERR "The ce-main module is not supposed to be active "
				"during any initialisation.");
		return -121;
	}
	int out_len;
	struct use_inf *out;
	int vers_len;
	char *vers;
	int err = use_compile(use, &out_len, &out, &vers_len, &vers);
	if (err < 0) {
		return err;
	}

	/* whether or not this has been called from a module's load() fnc  */
	int root = 0; /* don't make this static */
	if (top_use == NULL) {

		root_mod = mod_index;

		root = 1;
		int n_l;
		const char *n;
		int v_l;
		const char *v;
		mod_inf_name_get(mods_a + mod_index, &n_l, &n);
		mod_inf_vers_get(mods_a + mod_index, &v_l, &v);

		lprintf(INF "Root mod: "lF_GRE"%.*s %.*s"_lF".\n",
				n_l, n, v_l, v);
		/* malloc  */
		top_use = malloc(sizeof(struct refb));
		refb_construct(top_use);

		if ((err = mod_load(top_use, mod_index)) < 0) {
			lprintf(ERR "Root mod failed to be loaded, this is the end.\n");
			return err;
		}
		mods_a[mod_index].loading = 1; /* is it a good idea to manipulate this flag here? */
	}
	err = use_exec(top_use, mod_index, out_len, out, vers);
	if (root) {
		mods_a[mod_index].loading = 0; /* should this flag be constantly set root-mod? */
		if (err >= 0) {
			mods_a[mod_index].loaded = 1;
			int i, l;
			for (i = 0, l = mods_a[mod_index].fcn_cnt;
					i < l; i++) {
				int z = mods_a[mod_index].additional[i].index;
				fcns_a[z].loaded = 1;
			}
		}
		lprintf(INF "Root mod %sinitialized(err %i), should continue now..\n",
				err >= 0 ? "" : lF_RED"NOT "_lF, err);
	}
	if (err < 0)
		return err;
	struct mod_inf *minf = mods_a + mod_index;

	/* Expand minf->use_live_size ? */
	if (out_len > minf->use_live_size - minf->use_live_cnt) {
		/* count the length of static vers */
		int l;
		struct use_inf *u;
		mod_inf_use_get(minf, &l, &u, NULL);
		int vers_len = !(minf->use_cnt) ? 0 : u[minf->use_cnt - 1].ver_off
			+ u[minf->use_cnt - 1].ver_len;

		/* calc new size */
		int use_size_new =
			((minf->use_live_size == 0) + minf->use_live_size) * 2;
		if (use_size_new - minf->use_live_cnt < out_len)
			use_size_new = minf->use_live_cnt + out_len;

		/*lprintf(WRN "expanding minf->use_live_size; vers_len: %i, "
				"old sz: %lu, new: %lu, outln: %i\n", vers_len,
				minf->name_off + minf->name_len + minf->ver_len
				+ (minf->use_cnt + minf->use_live_size)
					* sizeof(struct use_inf)
				+ vers_len,
				minf->name_off + minf->name_len + minf->ver_len
				+ (minf->use_cnt + use_size_new)
					* sizeof(struct use_inf)
				+ vers_len,
				out_len
				);*/

		/*int offs = minf->name_off + minf->name_len + minf->ver_len
			+ sizeof(struct use_inf) * (minf->use_cnt - 1);
	lprintf(WRN "a %i usecnt%i off%i\n",
			((struct use_inf *)((char *) minf->additional) + offs)
			->ver_off, minf->use_cnt, offs);*/
		/* expand m->additional memory to hold more extra use slots */
		minf->additional = realloc(minf->additional, 0 /*
				+ sizeof(struct mod_inf_fcn) * minf->fcn_cnt
				+ fcn_vers_len*/ + minf->name_off
				+ minf->name_len + minf->ver_len
				+ (minf->use_cnt + use_size_new)
					* sizeof(struct use_inf)
				+ vers_len
				);
		assert(minf->additional != NULL);

		/* move version strings over */
		memmove(((char *)minf->additional) + minf->name_off
				+ minf->name_len + minf->ver_len
				+ (minf->use_cnt + use_size_new)
					* sizeof(struct use_inf),
				((char *)minf->additional) + minf->name_off
				+ minf->name_len + minf->ver_len
				+ (minf->use_cnt + minf->use_live_size)
					* sizeof(struct use_inf),
				vers_len);

		/* update infos */
		minf->use_live_size = use_size_new;
	}

	/* Add recently executed use info(without version strings) */
	int i;
	for (i = 0; i < out_len; i++) {
		out[i].ver_off = 0;
		out[i].ver_len = 0;
	}
	struct use_inf *u;
	mod_inf_use_get(minf, NULL, &u, NULL);

	memcpy(u + minf->use_cnt + minf->use_live_cnt,
			out, out_len * sizeof(struct use_inf));
	minf->use_live_cnt += out_len;

	/*lprintf(WRN "len now: %lu(w/o vers_len)\n",
			minf->name_off + minf->name_len + minf->ver_len
			+ (minf->use_cnt + minf->use_live_size)
			* sizeof(struct use_inf)
	       ); */
	return err;
}

int ce_mod_use(int mod_id, const char *use)
{
	struct id_t *id = (struct id_t *) &mod_id;
	assert(!id->iserr);
	assert(id->index < mods_length);
	assert(mods_a[id->index].iter == id->iter);

	int n = id->index;
	return mod_use(n, use);
}

__attribute__((destructor(65001))) static void root_mod_exit()
{
	if (root_mod >= 0 && mods_a[root_mod].loaded)
		mod_unload(top_use, root_mod);
}


