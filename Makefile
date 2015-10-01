# Info about building this file can be obtained from:
#  http://make.paulandlesley.org/autodep.html
#  http://miller.emu.id.au/pmiller/books/rmch/
#  http://mad-scientist.net/make/rules.html

# Makes the .d header dependency connections
MKDEP = gcc -MM -MG $(CFLAGS) "$*.c" \
	| sed -e "s@^\(.*\)\:@$*.d $*.o\:@" > $@

#VPATH = extfnc:include

CC ?= gcc
# applied via implicit rule
CFLAGS += -Wall -std=c99 -Iextfnc -Iinclude # -pthread
LDFLAGS := # -pthread -rdynamic -fPIC
SRC :=
OBJ :=

# Subfiles should append .c files to SRC
include core/sub.mk
include glx/sub.mk
include scn/sub.mk


OBJ += $(patsubst %.c, %.o, $(filter %.c, $(SRC)))


all:	$(OBJ)
	$(CC) -o "cengine" $(CFLAGS)$(OBJ)$(LDFLAGS)

%.d: %.c
	@$(MKDEP)

-include $(SRC:%.c=%.d)

clean:
	rm -f cengine $(OBJ) \
		$(patsubst %.o, %.d, $(OBJ))

xf-strb.h:
	git submodule init
	git submodule update

.PHONY: all clean xf-strb.h
