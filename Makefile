SLUG = Fundamental
VERSION = 0.5.1

SOURCES = $(wildcard src/*.cpp)

DISTRIBUTABLES += $(wildcard LICENSE*) res


include ../../plugin.mk
