
SOURCES = $(wildcard src/*.cpp)

include ../../plugin.mk


dist:
	mkdir -p dist/Fundamental
	cp LICENSE* dist/Fundamental/
	cp plugin.* dist/Fundamental/
	cp -R res dist/Fundamental/
