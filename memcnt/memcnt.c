

#undef malloc
#undef free
#undef calloc
#undef realloc
#include <stdlib.h>

#include <stdint.h>
#include "xf-htable.h"
#include "xf-strb.h"



struct file_inf {
	uint16_t name_off;
	uint16_t name_len;
	uint32_t memcnt;
};
struct mem_inf {
	uint16_t file;
	uint16_t line;
	uint32_t size;
	void *memory;
};

static int file_length = 0;
static int file_size = 10;
static struct file_inf *file_a = NULL;

static int mem_length = 0;
static int mem_size = 32;
static struct mem_inf *mem_a = NULL;

static struct xf_htable file_lookup;
static struct xf_htable *file_l = &file_lookup; /* convinient use */

static struct xf_strb file_names;
static struct xf_strb *file_n = &file_names;

static int init = 0;
static int cnt_malloc = 0;
static int cnt_free = 0;
static int cnt_calloc = 0;
static int cnt_realloc = 0;


void memcnt_status(FILE *f)
{
	assert(file_a != NULL && mem_a != NULL);
	fprintf(f, "memcnt status:\n");
	int i, c = 0; 
	unsigned int tot = 0;
	struct mem_inf *minf;
	struct file_inf *finf;
	for (i = 0; i < mem_length; i++) {
		minf = mem_a + i;
		if (minf->memory == NULL)
			continue;
		c++;
		tot += minf->size;
		finf = file_a + minf->file;
		fprintf(f, "\t%24.*s+%-4i : %u bytes\n", finf->name_len,
				file_n->a + finf->name_off, minf->line, minf->size);
	}
	if (!c)
		fprintf(f, "\tlooks like all memory has been free()'d.\n");
	else
		fprintf(f, "%u bytes of memory has been allocated.\n", tot);
	fprintf(f, "Function counters:\n"
			"\tmalloc:  %4i\n"
			"\tcalloc:  %4i\n"
			"\trealloc: %4i\n"
			"\tfree:    %4i\n", cnt_malloc, cnt_calloc, cnt_realloc, 
			cnt_free);
}

__attribute__((constructor(101))) static void memcnt_init()
{
	file_a = malloc(sizeof(file_a[0]) * file_size);
	mem_a = malloc(sizeof(mem_a[0]) * mem_size);
	assert(file_a != NULL);
	assert(mem_a != NULL);
	int i;
	for (i = 0; i < mem_size; i++)
		mem_a[i].memory = NULL;
	

	xf_strb_construct(file_n, 64);
	xf_htable_construct(file_l, 4, sizeof(mem_a[0].file),
			xf_hash_hsieh_superfast);

	init = 1;
}
__attribute__((destructor(101))) static void memcnt_exit()
{
	assert(file_a != NULL && mem_a != NULL);
	memcnt_status(stderr);

	xf_htable_destruct(file_l);
	xf_strb_destruct(file_n);

	free(mem_a);
	free(file_a);
}

static uint16_t file_get(const char *n)
{
	int len = strlen(n);
	uint16_t newindx = file_length;
	uint16_t *tblval = (uint16_t *)
		xf_htable_see(file_l, n, len, &newindx);

	if (*tblval == newindx) {
		file_length++;
		if (file_length > file_size) {
			file_size *= 2;
			file_a = realloc(file_a, sizeof(file_a[0]) * file_size);
		}
		struct file_inf *f = file_a + (file_length - 1);
		f->name_off = file_n->length - 1;
		f->name_len = len;
		xf_strb_append(file_n, n);
	}

	return *tblval;
}

/**
 * mem_get - get corresponding mem_a entry to mem
 * @mem:	memory address to look by; %NULL for new slot
 *
 * Return:	%NULL if not found and @mem was not %NULL
 */
static struct mem_inf *mem_get(void *mem)
{
	int i;
	if (!mem) {
		for (i = 0; i < mem_length; i++) {
			if (mem_a[i].memory)
				continue;
			return mem_a + i;
		}
		mem_length++;
		if (mem_length > mem_size) {
			i = mem_size;
			mem_size *= 2;
			mem_a = realloc(mem_a, sizeof(mem_a[0]) * mem_size);
			for (i += 0; i < mem_size; i++)
				mem_a[i].memory = NULL;
		}
		return mem_a + (mem_length - 1);
	} else {
		for (i = 0; i < mem_length; i++) {
			if (mem_a[i].memory != mem) 
				continue;
			return mem_a + i;
		}
		return NULL;
	}
}


void *memcnt_malloc(const char *file, int line, size_t size)
{
	assert(init);
	assert(file_a != NULL && mem_a != NULL);
	assert(line >= 0 && line < UINT16_MAX);

	cnt_malloc++;
	void *m = malloc(size);
	struct mem_inf *minf = mem_get(NULL);
	minf->file = file_get(file);
	minf->line = (uint16_t) line;
	minf->size = (uint32_t) size;
	minf->memory = m;

	//fprintf(stdout, "void *_%p = malloc(%ti);\n", m, sz);
	return m;
}

void memcnt_free(const char *file, int line, void *mem)
{
	assert(init);
	assert(file_a != NULL && mem_a != NULL);
	assert(line >= 0 && line < UINT16_MAX);

	cnt_free++;

	//fprintf(stdout, "free(_%p);\n", mem);
	//
	

	if (mem == NULL) {
		fprintf(stderr, "memcnt free(NULL) received from %s+%i !\n",
				file, line);
		return;
	}
	struct mem_inf *minf = mem_get(mem);
	minf->memory = NULL;
	free(mem);

}

void *memcnt_realloc(const char *file, int line, void *mem, size_t size)
{
	assert(init);
	assert(file_a != NULL && mem_a != NULL);
	assert(line >= 0 && line < UINT16_MAX);

	cnt_realloc++;
	void *nm = realloc(mem, size);

	struct mem_inf *minf = mem_get(mem);

	if (nm != mem)
		minf->memory = nm;

	minf->file = file_get(file);
	minf->line = (uint16_t) line;
	/*int oldsize = minf->size;*/
	minf->size = (uint32_t) size;

	/*if (nm != mem)
		fprintf(stdout, "void *_%p = realloc(_%p, %ti); // from %u\n", 
				nm, mem, size, oldsize);
	else
		fprintf(stdout, "_%p = realloc(_%p, %ti); // from %u \n", 
				nm, mem, size, oldsize); */

	return nm;
}

void *memcnt_calloc(const char *file, int line, size_t a, size_t b)
{
	assert(init);
	assert(file_a != NULL && mem_a != NULL);
	assert(line >= 0 && line < UINT16_MAX);

	cnt_calloc++;
	void *m = calloc(a, b);
	
	//fprintf(stdout, "void *_%p = calloc(%ti, %ti);\n", m, a, b);

	struct mem_inf *minf = mem_get(NULL);
	assert(minf != NULL);

	minf->file = file_get(file);
	minf->line = (uint16_t) line;
	minf->size = (uint32_t) (a * b);
	minf->memory = m;
	return m;
}
