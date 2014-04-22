
/**
 * DOC: ce-mod-refb using sequental memory
 * This implementation of the refb_ functions is optimized for constant
 * update/access times for refc of up to 14, fast buffer duplication,
 * replacement(refb_assign() -- rollback to duplicated state) and
 * destructing(free()'ing either a duplicated or constructed instance's mem).
 *
 * It aims to keep malloc()'ed memory to the minimum by using the structure to
 * store any values that are not of dynamic size.
 *
 * Also it allows for a max reference count of ((1 << 20) - 1) + 15 = 1048590.
 */

/**
 * struct refb - stores the usage reference counts for fcns and mods
 * @fcns_len:	how many functionalities does &bug hold
 * @mods_len:	how many mods does @mods_used hold
 * @overflow_len:
 * 		how many overflowed
 * @overflow_size:
 * 		how many &struct overflow_inf's is there memory for
 *
 * Use functions refb_construct(), refb_destruct(), refb_cpy(),
 * refb_expand() to handle.
 *
 * The @fcns_used and @mods_used 4-bit value 15 means that there might be
 * an entry for more references at @overflow.
 */
struct refb {
	uint16_t fcns_len;
	uint16_t mods_len;
	uint16_t overflow_len;
	uint16_t overflow_size;
	uint8_t *buf;
	//uint8_t *fcns_refc; /* each byte is further divided into 4-bits */
	//uint8_t *mods_refc; /* each byte is divided into 4-bits */
};
struct overflow_inf {
	uint32_t isfcn : 1;
	uint32_t index : 11;
	uint32_t add : 20; /* 15+this = total refc */
} *overflow;

// helper functions
static inline uint8_t *_refb_fcns(struct refb *b)
{
	return b->buf;
}
static inline uint8_t *_refb_mods(struct refb *b)
{
	return b->buf + b->fcns_len / 2 + (b->fcns_len % 2); /* rnd up */
}
static inline struct overflow_inf *_refb_overflow(struct refb *b)
{
	return (struct overflow_inf *) (_refb_mods(b) +
			b->mods_len / 2 + (b->mods_len % 2)); /* rnd up */
}


static void refb_construct(struct refb *b)
{
	b->fcns_len = fcns_length;
	b->mods_len = mods_length;
	b->overflow_len = 0;
	b->overflow_size = 0; /* default overflow size */
	b->buf = calloc(fcns_length / 2 + (fcns_length % 2)
			+ mods_length / 2 + (mods_length % 2)
			+ b->overflow_size * sizeof(struct overflow_inf),
			1);
	assert(b->buf != NULL);
}

static void refb_destruct(struct refb *b)
{
	free(b->buf);
}

static void refb_duplicate(struct refb *dest, struct refb *from)
{
	assert(from != NULL && dest != NULL && from->buf != NULL);
	dest->fcns_len = from->fcns_len;
	dest->mods_len = from->mods_len;
	dest->overflow_len = from->overflow_len;
	dest->overflow_size = from->overflow_size;
	size_t memlen = dest->fcns_len / 2 + (dest->fcns_len % 2)
			+ dest->mods_len / 2 + (dest->mods_len % 2)
			+ dest->overflow_size * sizeof(struct overflow_inf);
	dest->buf = malloc(memlen);
	assert(dest->buf != NULL);
	memcpy(dest->buf, from->buf, memlen);
}

static void refb_assign(struct refb *dest, struct refb *from)
{
	assert(from != NULL && dest != NULL);
	dest->fcns_len = from->fcns_len;
	dest->mods_len = from->mods_len;
	dest->overflow_len = from->overflow_len;
	dest->overflow_size = from->overflow_size;
	dest->buf = from->buf;
}

static void refb_expand(struct refb *b)
{
	assert(b != NULL);
	assert(b->fcns_len <= fcns_length);
	assert(b->mods_len <= mods_length);

	struct refb l;

	l.fcns_len = fcns_length;
	l.mods_len = mods_length;
	l.overflow_len = b->overflow_len;
	l.overflow_size = b->overflow_size;

	l.buf = calloc(l.fcns_len / 2 + (l.fcns_len % 2)
			+ l.mods_len / 2 + (l.mods_len % 2)
			+ l.overflow_size * sizeof(struct overflow_inf),
			1);
	assert(l.buf != NULL);
	memcpy(_refb_fcns(&l), _refb_fcns(b), b->fcns_len / 2 + (b->fcns_len % 2));
	memcpy(_refb_mods(&l), _refb_mods(b), b->mods_len / 2 + (b->mods_len % 2));
	memcpy(_refb_overflow(&l), _refb_overflow(b), l.overflow_len);

	refb_destruct(b);
}

// refb_mod_* //

static int refb_mod_ref(struct refb *b, int mod_index)
{
	assert(b != NULL);
	assert(mod_index >= 0 && mod_index < mods_length);
	assert(b->mods_len > mod_index); /* possible expand instead of this */

	int p = _refb_mods(b)[mod_index / 2];
	int cc = (mod_index % 2) ?
		(p & 0xf0) >> 4
		: p & 0x0f;

	if (cc < 0xf) { /* within the 15 limit */
		cc++;
		p = (mod_index % 2) ? (p & 0x0f) | (cc << 4)
			: (p & 0xf0) | cc;
		_refb_mods(b)[mod_index / 2] = p;
		return cc;
	}
	/* 4-bits wasn't enough to specify */

	int i, l, trgt = -1;
	struct overflow_inf *o = _refb_overflow(b);
	for (i = 0, l = b->overflow_len; i < l; i++) {
		if (o[i].isfcn || o[i].index != mod_index)
			continue;
		trgt = i;
		break;
	}
	if (trgt == -1) {
		if (b->overflow_len + 1 > b->overflow_size) {
			b->overflow_size = (b->overflow_size
					+ (b->overflow_size == 0)) * 2;
			b->buf = realloc(b->buf, b->fcns_len / 2 + (b->fcns_len % 2)
					+ b->mods_len / 2 + (b->mods_len % 2)
					+ b->overflow_size
						* sizeof(struct overflow_inf));
			assert(b->buf != NULL);
		}
		o = _refb_overflow(b) + b->overflow_len;
		o->isfcn = 0;
		o->index = mod_index;
		o->add = 0;
		b->overflow_len++;
	} else {
		o = o + trgt;
	}
	assert(o->add < ((1 << 20) - 1));
	o->add++;
	return o->add + 0xf;
}

static int refb_mod_unref(struct refb *b, int mod_index)
{
	assert(b != NULL);
	assert(mod_index >= 0 && mod_index < mods_length);
	assert(b->mods_len > mod_index);

	int p = _refb_mods(b)[mod_index / 2];
	int cc = (mod_index % 2) ?
		(p & 0xf0) >> 4
		: p & 0x0f;

	if (cc < 0xf) { /* default case */
		assert(cc > 0);
		cc--;
		p = (mod_index % 2) ? (p & 0x0f) | (cc << 4)
			: (p & 0xf0) | cc;
		_refb_mods(b)[mod_index / 2] = p;
		return cc;
	}
	int i, l, trgt = -1;
	struct overflow_inf *o = _refb_overflow(b);
	for (i = 0, l = b->overflow_len; i < l; i++) {
		if (o[i].isfcn || o[i].index != mod_index)
			continue;
		trgt = i;
		break;
	}
	if (trgt == -1 || o[trgt].add == 0) { /* 15 but no addition */
		cc--;
		p = (mod_index % 2) ? (p & 0x0f) | (cc << 4)
			: (p & 0xf0) | cc;
		_refb_mods(b)[mod_index / 2] = p;
		return cc;
	} else {
		o = o + trgt;
	}
	o->add--; /* decrement the addition */
	return o->add + 0xf;
}

static int refb_mod_cnt(struct refb *b, int mod_index)
{
	assert(b != NULL);
	assert(mod_index >= 0 && mod_index < mods_length);
	assert(b->mods_len > mod_index);

	int p = _refb_mods(b)[mod_index / 2];
	int cc = (mod_index % 2) ?
		(p & 0xf0) >> 4
		: p & 0x0f;

	if (cc < 15) /* default case */
		return cc;
	/* possible addition */
	int i, l;
	struct overflow_inf *o = _refb_overflow(b);
	for (i = 0, l = b->overflow_len; i < l; i++) {
		if (o[i].isfcn || o[i].index != mod_index)
			continue;
		return 15 + o[i].add;
	}
	return 15; /* no addition */
}

// refb_fcn_* //

static int refb_fcn_ref(struct refb *b, int fcn_index)
{
	assert(b != NULL);
	assert(fcn_index >= 0 && fcn_index < fcns_length);
	assert(b->fcns_len > fcn_index); /* possible expand instead of this */

	int p = _refb_fcns(b)[fcn_index / 2];
	int cc = (fcn_index % 2) ?
		(p & 0xf0) >> 4
		: p & 0x0f;

	if (cc < 0xf) { /* within the 15 limit */
		cc++;
		p = (fcn_index % 2) ? (p & 0x0f) | (cc << 4)
			: (p & 0xf0) | cc;
		_refb_fcns(b)[fcn_index / 2] = p;
		return cc;
	}
	/* 4-bits wasn't enough to specify */

	int i, l, trgt = -1;
	struct overflow_inf *o = _refb_overflow(b);
	for (i = 0, l = b->overflow_len; i < l; i++) {
		if (!o[i].isfcn || o[i].index != fcn_index)
			continue;
		trgt = i;
		break;
	}
	if (trgt == -1) {
		if (b->overflow_len + 1 > b->overflow_size) {
			b->overflow_size = (b->overflow_size
					+ (b->overflow_size == 0)) * 2;
			b->buf = realloc(b->buf, b->fcns_len / 2 + (b->fcns_len % 2)
					+ b->mods_len / 2 + (b->mods_len % 2)
					+ b->overflow_size
						* sizeof(struct overflow_inf));
			assert(b->buf != NULL);
		}
		o = _refb_overflow(b) + b->overflow_len;
		o->isfcn = 0;
		o->index = fcn_index;
		o->add = 0;
		b->overflow_len++;
	} else {
		o = o + trgt;
	}
	assert(o->add < ((1 << 20) - 1));
	o->add++;
	return o->add + 0xf;
}


static int refb_fcn_unref(struct refb *b, int fcn_index)
{
	assert(b != NULL);
	assert(fcn_index >= 0 && fcn_index < fcns_length);
	assert(b->fcns_len > fcn_index);

	int p = _refb_fcns(b)[fcn_index / 2];
	int cc = (fcn_index % 2) ?
		(p & 0xf0) >> 4
		: p & 0x0f;

	if (cc < 0xf) { /* default case */
#ifndef NDEBUG
		if (!cc) {
			lprintf(ERR "Cannot unreference "
					lF_RED"%.*s"_lF" from 0!!\n",
					fcns_a[fcn_index].name_len,
					fcn_name.a + fcns_a[fcn_index].name_off);
		}
#endif
		assert(cc > 0);
		cc--;
		p = (fcn_index % 2) ? (p & 0x0f) | (cc << 4)
			: (p & 0xf0) | cc;
		_refb_fcns(b)[fcn_index / 2] = p;
		return cc;
	}
	int i, l, trgt = -1;
	struct overflow_inf *o = _refb_overflow(b);
	for (i = 0, l = b->overflow_len; i < l; i++) {
		if (!o[i].isfcn || o[i].index != fcn_index)
			continue;
		trgt = i;
		break;
	}
	if (trgt == -1 || o[trgt].add == 0) { /* 15 but no addition */
		cc--;
		p = (fcn_index % 2) ? (p & 0x0f) | (cc << 4)
			: (p & 0xf0) | cc;
		_refb_fcns(b)[fcn_index / 2] = p;
		return cc;
	} else {
		o = o + trgt;
	}
	o->add--; /* decrement the addition */
	return o->add + 0xf;
}

static int refb_fcn_cnt(struct refb *b, int fcn_index)
{
	assert(b != NULL);
	assert(fcn_index >= 0 && fcn_index < fcns_length);
	assert(b->fcns_len > fcn_index);

	int p = _refb_fcns(b)[fcn_index / 2];
	int cc = (fcn_index % 2) ?
		(p & 0xf0) >> 4
		: p & 0x0f;

	if (cc < 15) /* default case */
		return cc;
	/* possible addition */
	int i, l;
	struct overflow_inf *o = _refb_overflow(b);
	for (i = 0, l = b->overflow_len; i < l; i++) {
		if (!o[i].isfcn || o[i].index != fcn_index)
			continue;
		return 15 + o[i].add;
	}
	return 15; /* no addition */
}


