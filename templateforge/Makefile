CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -Iinclude
TARGET  := templateforge

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,build/%.o,$(SRCS))

.PHONY: all clean run

all: $(TARGET)

# Create build directory before compiling objects
$(OBJS): | build_dir

build_dir:
	mkdir -p build

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -rf build $(TARGET)
