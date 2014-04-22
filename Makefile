
CC = gcc

INCL	= -IeXtFnc -Iinclude -include include/memcnt.h
LIB	= -ldl

FILES	 = ce-log.c
FILES	+= ce-arg.c
FILES	+= ce-mod.c
FILES	+= ce-main.c
FILES	+= glx-window.c
FILES	+= memcnt.c
#FILES	+= asdf.c

MACROS	+= -DCE_VERSION_MAJOR=0
MACROS	+= -DCE_VERSION_MINOR=2
MACROS	+= -DCE_VERSION_REVISION=6

default:
	$(CC) $(INCL) -Wall -std=c99 -pthread -rdynamic -fPIC $(MACROS) $(FILES) $(LIB) -o"cengine"
