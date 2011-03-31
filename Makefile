CFLAGS=-Wall -Wextra -pedantic -Os -pipe -mno-cygwin
LDFLAGS=-s -mno-cygwin -Wl,--enable-stdcall-fixup

CC=$(MINGW32_CROSS_PREFIX)gcc
WINDRES=$(MINGW32_CROSS_PREFIX)windres
DLLWRAP=$(MINGW32_CROSS_PREFIX)dllwrap

SOURCES := $(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o) nsRestartExplorer-rc.o

all: nsRestartExplorer.dll

%-rc.o: %.rc
	$(WINDRES) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@rm -f $(CLEANUP) *.dll *.o
	@echo Object files removed

nsRestartExplorer.dll: $(OBJECTS)
	$(DLLWRAP) $(LDFLAGS) --def nsRestartExplorer.def -o $@ $(OBJECTS)
