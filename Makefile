# Info about building this file can be obtained from:
#  http://make.paulandlesley.org/autodep.html
#  http://miller.emu.id.au/pmiller/books/rmch/
#  http://mad-scientist.net/make/rules.html


O?=build

PRINT_PRETTY?=1

#VPATH = extfnc:include

CC?=gcc
LD?=ld
# applied via implicit rule
CFLAGS+=-Wall -std=c99 -Iextfnc -Iinclude -fPIC # -pthread
# An alternative of finding gcc libs was to use "$(CC) -print-file-name=", but
# that doesn't work with clang.
LIBGCC?=$(dir $(shell $(CC) -print-libgcc-file-name))
LDFLAGS:=-lpthread -lc -L$(LIBGCC) -lgcc
L?=/usr/lib
SRC :=
OBJ :=


# Subfiles should append .c files to SRC
include core/sub.mk
include glx/sub.mk
include scn/sub.mk

OBJ += $(patsubst %.c, $O/%.o, $(filter %.c, $(SRC)))


# Prettier build rules
# Okay, invoking linker manually is crazy, but meh
$O/cengine: $(OBJ)
ifeq ($(PRINT_PRETTY), 1)
	@printf "  LD\t$@\n"
	@$(LD) -o $@ --dynamic-linker /lib64/ld-linux-x86-64.so.2 \
		$L/crt1.o $L/crti.o $(LIBGCC)crtbegin.o $(OBJ) $(LDFLAGS) \
		$(LIBGCC)crtend.o $L/crtn.o
else
	$(LD) -o $@ --dynamic-linker /lib64/ld-linux-x86-64.so.2 \
		$L/crt1.o $L/crti.o $(LIBGCC)crtbegin.o $(OBJ) $(LDFLAGS) \
		$(LIBGCC)crtend.o $L/crtn.o
endif

$O/%.o: %.c | $O
ifeq ($(PRINT_PRETTY), 1)
	@printf "  CC\t$@\n"
	@$(CC) $(CFLAGS) -c $< -o $@
else
	$(CC) $(CFLAGS) -c $< -o $@
endif

$O:
	@mkdir $O

# Makes the .d header dependency connections
$O/%.d: %.c | $O
	@mkdir -p $(@D)
	@$(CC) -MM -MG $(CFLAGS) "$<" -MT "$O/$*.o $@" -o $@


ifneq ($(MAKECMDGOALS),clean)
-include $(SRC:%.c=$O/%.d)
endif

clean:
	rm -f $O/cengine $(OBJ) \
		$(patsubst %.o, %.d, $(OBJ))

# Make sure extfnc is checked out.
extfnc/readme:
	git submodule init
	git submodule update

.PHONY: clean
