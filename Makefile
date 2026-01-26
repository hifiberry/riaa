CC = gcc
CFLAGS = -Wall -O3 -fPIC
LDFLAGS = -shared

# Plugin names
PLUGINS = riaa
PRODUCTION = riaa

# Installation directory (standard LADSPA plugin directory)
INSTALL_DIR = /usr/local/lib/ladspa

all: $(addsuffix .so,$(PLUGINS))

riaa.so: riaa.o decibel.o counter.o ini.o configfile.o biquad.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Header dependencies
riaa.o: riaa.c biquad.h riaa.h samplerate.h decibel.h counter.h configfile.h

decibel.o: decibel.c decibel.h

counter.o: counter.c counter.h

biquad.o: biquad.c biquad.h

ini.o: ini.c ini.h

configfile.o: configfile.c configfile.h ini.h

clean:
	rm -f *.o *.so

install: riaa.so
	mkdir -p $(INSTALL_DIR)
	cp riaa.so $(INSTALL_DIR)/
	@echo "Installed riaa.so to $(INSTALL_DIR)/"

install-all: $(addsuffix .so,$(PLUGINS))
	mkdir -p $(INSTALL_DIR)
	cp $(addsuffix .so,$(PLUGINS)) $(INSTALL_DIR)/

uninstall:
	rm -f $(addprefix $(INSTALL_DIR)/,$(addsuffix .so,$(PLUGINS)))

.PHONY: all clean install install-all uninstall
