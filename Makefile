RACK_DIR ?= ../..
SLUG = Fundamental
VERSION = 0.6.0

# Dependencies

include $(RACK_DIR)/dep.mk

libsamplerate = dep/libsamplerate-0.1.9/src/.libs/libsamplerate.a
$(libsamplerate):
	cd dep && $(WGET) http://www.mega-nerd.com/SRC/libsamplerate-0.1.9.tar.gz
	cd dep && $(UNTAR) libsamplerate-0.1.9.tar.gz
	cd dep/libsamplerate-0.1.9 && $(CONFIGURE)
	cd dep/libsamplerate-0.1.9 && $(MAKE)

# Plugin build

FLAGS += -Idep/libsamplerate-0.1.9/src

SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += $(wildcard LICENSE*) res
OBJECTS += $(libsamplerate)

include $(RACK_DIR)/plugin.mk
