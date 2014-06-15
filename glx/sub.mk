
SRC += glx/window.c
SRC += glx/context.c
# -lX11	XCreateWindow, XOpenDisplay, XMapWindow
# -lGL	glXCreateContext, glXMakeCurrent
LDFLAGS += -lX11 -lGL

