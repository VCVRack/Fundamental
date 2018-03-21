SLUG = Fundamental
VERSION = 0.6.0

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += $(wildcard LICENSE*) res

FLAGS += -Idep/libsamplerate/src

libsamplerate = dep/libsamplerate/src/.libs/libsamplerate.a
$(libsamplerate):
	cd dep/libsamplerate && ./autogen.sh
	cd dep/libsamplerate && CFLAGS=-fPIC ./configure
	cd dep/libsamplerate && $(MAKE)

OBJECTS += $(libsamplerate)

RACK_DIR ?= ../..
include $(RACK_DIR)/plugin.mk
