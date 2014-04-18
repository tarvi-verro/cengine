#include "ce-aux.h"
#include "ce-arg.h"
#include <stdlib.h> /* alloc realloc free */
#include <string.h> /* memcmp */
#include <assert.h>

/* length of used portion of the array */
static int cb_length = 0;
static int cb_size = 8;
static int (**cb_a)(const char *, int);
__attribute__((constructor(120))) static void arg_init()
{
	cb_a = malloc(sizeof(cb_a[0]) * cb_size);
	lputs(INF "Argument functions initialized.");
}
__attribute__((destructor(120))) static void arg_exit()
{
	free(cb_a);
	lputs(INF "Argument functions destructed.");
}
void arg_callb_add(int (*cb)(const char *, int))
{
	int i;
	for (i = 0; i < cb_length; i++) {
		if (cb_a[i] != NULL)
			continue;
		cb_length--;
		break;
	}
	cb_length++;
	if (cb_length > cb_size) {
		cb_size *= 2;
		cb_a = realloc(cb_a, sizeof(cb_a[0]) * cb_size);
	}
	cb_a[i] = cb;
}
int arg_callb_rm(int (*cb)(const char *, int))
{
	for (int i = 0; i < cb_length; i++) {
		if (cb_a[i] != cb) 
			continue;

		cb_a[i] = NULL;
		if (i != cb_length - 1) 
			return 0;

		/* cut off any extra NULL's from the end */
		for (; i >= 0; i--) {
			if (cb_a[i] != NULL)
				break;
			cb_length--;
		}
		return 0;

	}
	return 1;
}
static int push_argc;
static const char **push_args;
static int push_iter = -1;
int arg_push_a(int argc, const char **args)
{
	int unkn = 0;
	push_argc = argc;
	push_args = args;
	for (int i = 0; i < argc; i++) {
		push_iter = i;
		int argsize = strlen(args[i]) + 1;
		int ishlp = argsize == 7 && !memcmp(args[i], "--help", 7);
		int fnd = ishlp ? 1 : 0;
		for (int c = 0; c < cb_length; c++) {
			if (cb_a[c] == NULL)
				continue;
			if ((fnd = cb_a[c](args[i], argsize)))
				break;
		}
		/* did a callback mess up the return value? */
		assert(fnd >= 0 && fnd + i <= argc); 
		if(fnd) {
			i += fnd - 1; /* iterate the amount of args captured */
		} else if (!ishlp) {
			unkn++;
			lprintf(WRN "Unknown argument `%s`. Try -o --help\n", args[i]);
		}
	}
	push_iter = -1;
	return unkn;
}
const char *arg_peek(int forw)
{
	assert(push_iter != -1);
	if (push_iter + forw >= push_argc)
		return NULL;
	return push_args[push_iter + forw];
}
int arg_push_str(const char *argstr)
{
	assert(1 == 3); /* implement when actually used */
}
int arg_bool(const char *a)
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
	b[7] |= 32;
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
