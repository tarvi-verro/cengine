
SRC += glx/window.c
SRC += glx/context.c
SRC += glx/input.c
# -lX11	XCreateWindow, XOpenDisplay, XMapWindow
# -lGL	glXCreateContext, glXMakeCurrent
LDFLAGS += -lX11 -lGL -lxcb -pthread -lX11-xcb -lxcb-keysyms -lxcb-xinput
CFLAGS += -pthread


