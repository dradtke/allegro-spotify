CC      := clang
PKGS	:= libspotify allegro-5.0 allegro_audio-5.0
CFLAGS  := -g -Wall -I../include `pkg-config --cflags $(PKGS)`
LIBS    := -lpthread -lm -lallegro_spotify-5.0 -L.. `pkg-config --libs $(PKGS)`

TARGET	:= main
SOURCES := $(shell find src/ -type f -name *.c)
OBJECTS := $(patsubst src/%,build/%,$(SOURCES:.c=.o))
DEPS	:= $(OBJECTS:.o=.deps)

$(TARGET): $(OBJECTS)
	@echo "  Linking '$(TARGET)'..."; $(CC) $^ -o $(TARGET) $(LIBS)

build/%.o: src/%.c
	@mkdir -p build/
	@echo "  CC $<"; $(CC) $(CFLAGS) -MD -MF $(@:.o=.deps) -c -o $@ $<

run: $(TARGET)
	@LD_LIBRARY_PATH=.. ./$(TARGET) "$(artist)" "$(track)"

clean:
	@echo "  Cleaning..."; $(RM) -r build/ $(TARGET)

-include $(DEPS)

.PHONY: run clean
