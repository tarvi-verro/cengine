#include "ce-aux.h"
#include "xf-escg.h"
#include "ce-opt.h"

#include <dlfcn.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

int dlib_load(const char *path);

static int optcb(int index, const char *optarg)
{
	assert(index == 0 && optarg);

	return dlib_load(optarg);
}

static struct optsection dlib_opts = {
	.label = "Dynamic libraries:",
	.callback = optcb,
	.opt_a = {
		{ ARG_REQUIRED, 'y', "dynamic-lib",
			"PATH\tLoad a dynamic library." },
		{ ARG_NONE, '\0', NULL, NULL }
	},
};

struct lib_inf {
	void *hnd;
#ifndef NDEBUG
	char *path;
#endif
};

static struct lib_inf *libs_a = NULL;
static int libs_length = 0;
static int libs_size = 2;

static void __attribute__((constructor(140))) ce_dlib_init()
{
	libs_a = malloc(libs_size * sizeof(libs_a[0]));
	opt_add(ce_options, &dlib_opts);
}

static void __attribute__((destructor(140))) ce_dlib_exit()
{
	free(libs_a);
	libs_a = NULL;
	opt_rm(ce_options, &dlib_opts);
}

static inline void verify_load(struct lib_inf *l, const char *path)
{
#ifndef NDEBUG
	int len = strlen(path) + 1;
	l->path = memcpy(malloc(len), path, len);
#endif
}

static inline void verify_unload(struct lib_inf *l)
{
#ifndef NDEBUG
	assert(l->hnd);
	void *hnd = dlopen(l->path, RTLD_NOLOAD);
	assert(hnd == NULL || hnd == l->hnd);
	int count = 0;
	while (hnd != NULL) {
		count++;
		dlclose(l->hnd);
		hnd = dlopen(l->path, RTLD_NOLOAD);
	}
	if (count) {
		lprintf(ERR "Dynamic library "lF_RED"%s"_lF
				" was additionally referenced "
				lF_RED"%i"_lF" times.\n", l->path, count);
	}
	free(l->path);
#endif
}

static void __attribute__((destructor(50001))) dlibs_unload()
{
	assert(libs_a != NULL);
	int j = 0;

	for (int i = libs_length - 1; i >= 0; i--) {
		if (!libs_a[i].hnd) {
			continue;
		}
		int rv = dlclose(libs_a[i].hnd);
		assert(rv == 0);
		verify_unload(libs_a + i);
		libs_a[i].hnd = NULL;
		j++;
	}
	libs_length = 0;

	if (j == 1)
		lputs(INF "Unloaded a dynamic library.");
	else if (j)
		lprintf(INF "Unloaded "lF_BLUE"%i"_lF" dynamic libraries.\n", j);
}

int dlib_load(const char *path)
{
	assert(libs_a != NULL);
	dlerror();
	void *dl = dlopen(path, RTLD_LAZY);
	if (dl == NULL) {
		lprintf(ERR "Failed to open "lF_RED"%s"_lF": %s\n",
				path, dlerror());
		return -1;
	}
	lprintf(INF "Dynamic library "lF_BLUE"%s"_lF" loaded.\n",
			path);

	for (int i = 0; i < libs_length; i++) {
		if (libs_a[i].hnd != NULL)
			continue;
		libs_a[i].hnd = dl;
		return i;
	}
	if (libs_length + 1 > libs_size) {
		libs_size *= 2;
		libs_a = realloc(libs_a, libs_size * sizeof(libs_a[0]));
		assert(libs_a);
	}
	libs_a[libs_length].hnd = dl;
	verify_load(libs_a + libs_length, path);
	libs_length++;

	return libs_length - 1;
}

int dlib_unload(int indx)
{
	assert(libs_a != NULL);
	assert(indx >= 0 && indx < libs_length);
	assert(libs_a[indx].hnd != NULL);
	lprintf(INF "Unloading dynamic library indx "lF_BLUE"%i"_lF".\n",
			indx);
	dlclose(libs_a[indx].hnd);
	verify_unload(libs_a + indx);
	libs_a[indx].hnd = NULL;
	if (indx == libs_length - 1) {
		libs_length--;
		int i;
		for (i = libs_length - 1; i >= 0 && !libs_a[i].hnd; i--);
		libs_length = i + 1;
	}
	return 0;
}
