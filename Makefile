CC      := clang
PKGS	:= libspotify allegro-5.0 allegro_audio-5.0
CFLAGS  := -g -Wall -Iinclude `pkg-config --cflags $(PKGS)`
LIBS    := -lpthread -lm `pkg-config --libs $(PKGS)`

TARGET	:= liballegro_spotify-5.0.so
SOURCES := $(shell find src/ -type f -name *.c)
OBJECTS := $(patsubst src/%,build/%,$(SOURCES:.c=.o))
DEPS	:= $(OBJECTS:.o=.deps)

$(TARGET): $(OBJECTS)
	@echo "  Linking '$(TARGET)'..."; $(CC) -shared -Wl,-soname,liballegro_spotify-5.0.so $^ -o $(TARGET) $(LIBS)

build/%.o: src/%.c
	@mkdir -p build/
	@echo "  CC $<"; $(CC) $(CFLAGS) -MD -MF $(@:.o=.deps) -c -fPIC -o $@ $<

clean:
	@echo "  Cleaning..."; $(RM) -r build/ $(TARGET)

-include $(DEPS)

.PHONY: clean
