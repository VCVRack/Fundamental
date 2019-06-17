RACK_DIR ?= ../..

FLAGS += -Idep/include
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += $(wildcard LICENSE*) res

# Static libs
libsamplerate := dep/lib/libsamplerate.a
OBJECTS += $(libsamplerate)

# Dependencies
DEPS += $(libsamplerate)

$(libsamplerate):
	cd dep && $(WGET) http://www.mega-nerd.com/SRC/libsamplerate-0.1.9.tar.gz
	cd dep && $(UNTAR) libsamplerate-0.1.9.tar.gz
	cd dep/libsamplerate-0.1.9 && $(CONFIGURE)
	cd dep/libsamplerate-0.1.9/src && $(MAKE)
	cd dep/libsamplerate-0.1.9/src && $(MAKE) install


include $(RACK_DIR)/plugin.mk
