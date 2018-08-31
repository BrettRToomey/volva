CC=clang
TARGET=volv
PREFIX=/usr/local/
INCLUDE_DIR=$(PREFIX)/include/volva/

local_CFLAGS = -dynamic-list,plugins.list

debug: local_CFLAGS := $(local_CFLAGS) $(CFLAGS)

local_LFLAGS = -lcurl $(LFLAGS)

all: debug

debug: $(TARGET)
release: $(TARGET)

$(TARGET):
	$(CC) -o $(TARGET) src/main.c $(local_LFLAGS) $(local_CFLAGS)

install:
	mkdir -p $(INCLUDE_DIR)
	cp src/plugins.h $(INCLUDE_DIR)
	cp $(TARGET) $(PREFIX)/bin

clean:
	- rm -f $(TARGET)

.PHONY: all debug release clean
