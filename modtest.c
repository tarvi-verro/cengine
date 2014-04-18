/* gcc -Wall -fPIC -shared modtest.c -o ./modules/modtest.so */
#include <stdio.h>
#include "ce-mod.h"
int dynmod_version()
{
	return 1043;
}
static int dynmod_init()
{
	int a;
	puts("Hello there world, modtest here!");
	return 0;
}
static void dynmod_exit()
{
	puts("Goodbye world, its been guud");
}
extern struct ce_module dynmod;
__attribute__ ((visibility ("default"))) struct ce_module dynmodule = {
	.inf = "2:4:3 dynamic mod speaking to world",
	.init = dynmod_init,
	.exit = dynmod_exit,

};

