CC=clang
TARGET=volv
PREFIX=/usr/local
INCLUDE_DIR=$(PREFIX)/include/volva/
INSTALL_BINARY=yes

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
ifeq ($(INSTALL_BINARY), yes)
	cp $(TARGET) $(PREFIX)/bin
endif

clean:
	- rm -f $(TARGET)

.PHONY: all debug release clean
