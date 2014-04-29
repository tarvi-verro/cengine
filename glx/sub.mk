
SRC += glx/window.c
# -lX11	XCreateWindow, XOpenDisplay, XMapWindow
# -lGL	glXCreateContext, glXMakeCurrent
LDFLAGS += -lX11 -lGL

