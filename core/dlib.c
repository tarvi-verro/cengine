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

static void **libs_a = NULL;
static int libs_length = 0;
static int libs_size = 2;

static void __attribute__((constructor(140))) ce_dlib_init()
{
	libs_a = malloc(libs_size * sizeof(void *));
	opt_add(ce_options, &dlib_opts);
}

static void __attribute__((destructor(140))) ce_dlib_exit()
{
	free(libs_a);
	libs_a = NULL;
	opt_rm(ce_options, &dlib_opts);
}


static void __attribute__((destructor(50001))) dlibs_unload()
{
	assert(libs_a != NULL);
	int j = 0;

	for (int i = libs_length - 1; i >= 0; i--) {
		if (!libs_a[i]) {
			continue;
		}
		int rv = dlclose(libs_a[i]);
		assert(rv == 0);
		libs_a[i] = NULL;
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
	void *dl = dlopen(path, RTLD_NOW);
	if (dl == NULL) {
		lprintf(ERR "Failed to open "lF_RED"%s"_lF": %s\n",
				path, dlerror());
		return -1;
	}
	lprintf(INF "Dynamic library "lF_BLUE"%s"_lF" loaded.\n",
			path);

	for (int i = 0; i < libs_length; i++) {
		if (libs_a[i] != NULL)
			continue;
		libs_a[i] = dl;
		return i;
	}
	if (libs_length + 1 > libs_size) {
		libs_size *= 2;
		libs_a = realloc(libs_a, libs_size * sizeof(void *));
		assert(libs_a);
	}
	libs_a[libs_length] = dl;
	libs_length++;

	return libs_length - 1;
}

int dlib_unload(int indx)
{
	assert(libs_a != NULL);
	assert(indx >= 0 && indx < libs_length);
	assert(libs_a[indx] != NULL);
	lprintf(INF "Unloading dynamic library indx "lF_BLUE"%i"_lF".\n",
			indx);
	dlclose(libs_a[indx]);
	libs_a[indx] = NULL;
	if (indx == libs_length - 1) {
		libs_length--;
		int i;
		for (i = libs_length - 1; i >= 0 && !libs_a[i]; i--);
		libs_length = i + 1;
	}
	return 0;
}
