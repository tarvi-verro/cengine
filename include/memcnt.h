
#include <stdio.h>
#include <stdlib.h>


void *memcnt_malloc(const char *file, int line, size_t size);
#define malloc(mem) \
	memcnt_malloc(__FILE__, __LINE__, mem)

void memcnt_free(const char *file, int line, void *mem);
#define free(mem) \
	memcnt_free(__FILE__, __LINE__, mem)

void *memcnt_realloc(const char *file, int line, void *mem, size_t size);
#define realloc(mem,nz) \
	memcnt_realloc(__FILE__, __LINE__, mem, nz)

void *memcnt_calloc(const char *file, int line, size_t a, size_t b);
#define calloc(a,b) \
	memcnt_calloc(__FILE__, __LINE__,a , b)

void memcnt_status(FILE *f);
#define MEMCNT_ENABLED 1
