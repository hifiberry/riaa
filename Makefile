CC = gcc
CFLAGS = -Wall -O3 -fPIC -Idsp -Iutils
LDFLAGS = -shared

# Plugin names
PLUGINS = riaa
PRODUCTION = riaa

# Installation directory (standard LADSPA plugin directory)
INSTALL_DIR = /usr/local/lib/ladspa

all: $(addsuffix .so,$(PLUGINS)) riaa_process

riaa.so: riaa_ladspa.o dsp/decibel.o utils/counter.o utils/ini.o utils/configfile.o dsp/biquad.o dsp/declick.o dsp/riaa_calc.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

riaa_process: riaa_process.o
	$(CC) -o $@ $^ -lsndfile -ldl

riaa_process.o: riaa_process.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

dsp/%.o: dsp/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

utils/%.o: utils/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Header dependencies
riaa_ladspa.o: riaa_ladspa.c riaa_ladspa.h dsp/biquad.h dsp/riaa_coeffs.h dsp/riaa_calc.h dsp/samplerate.h dsp/decibel.h utils/counter.h utils/configfile.h dsp/declick.h utils/controls.h

dsp/decibel.o: dsp/decibel.c dsp/decibel.h

utils/counter.o: utils/counter.c utils/counter.h

dsp/biquad.o: dsp/biquad.c dsp/biquad.h

dsp/declick.o: dsp/declick.c dsp/declick.h

dsp/riaa_calc.o: dsp/riaa_calc.c dsp/riaa_calc.h dsp/biquad.h dsp/riaa_coeffs.h

utils/ini.o: utils/ini.c utils/ini.h

utils/configfile.o: utils/configfile.c utils/configfile.h utils/ini.h

clean:
	rm -f *.o dsp/*.o utils/*.o *.so

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
