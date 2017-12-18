SLUG = Fundamental
VERSION = 0.5.0

SOURCES = $(wildcard src/*.cpp)


include ../../plugin.mk


dist: all
	mkdir -p dist/$(SLUG)
	cp LICENSE* dist/$(SLUG)/
	cp $(TARGET) dist/$(SLUG)/
	cp -R res dist/$(SLUG)/
	cd dist && zip -5 -r $(SLUG)-$(VERSION)-$(ARCH).zip $(SLUG)
