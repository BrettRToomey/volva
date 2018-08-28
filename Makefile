CC=clang
TARGET=volv
INCLUDE_DIR=/usr/local/include/volva/

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
	cp $(TARGET) /usr/local/bin

clean:
	- rm -f $(TARGET)

.PHONY: all debug release clean
