CC = gcc
CFLAGS = -Wall -O3 -fPIC -Idsp -Iutils
LDFLAGS = -shared

# Plugin names
PLUGINS = riaa
PRODUCTION = riaa

# Installation directories
LADSPA_INSTALL_DIR = /usr/local/lib/ladspa
LV2_INSTALL_DIR = /usr/local/lib/lv2

# LV2 bundle directory
LV2_BUNDLE = lv2/riaa.lv2

.PHONY: all ladspa lv2 clean install install-ladspa install-lv2 uninstall

all: ladspa lv2 riaa_process

ladspa: riaa.so

lv2: $(LV2_BUNDLE)/riaa.so

riaa.so: riaa_ladspa.o dsp/decibel.o utils/counter.o utils/ini.o utils/configfile.o dsp/biquad.o dsp/declick.o dsp/riaa_calc.o dsp/notch.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

$(LV2_BUNDLE)/riaa.so: riaa_lv2.o dsp/decibel.o utils/counter.o dsp/biquad.o dsp/declick.o dsp/riaa_calc.o dsp/notch.o
	mkdir -p $(LV2_BUNDLE)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

riaa_lv2.o: riaa_lv2.c
	$(CC) $(CFLAGS) -c -o $@ $<

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

riaa_lv2.o: riaa_lv2.c dsp/biquad.h dsp/riaa_coeffs.h dsp/riaa_calc.h dsp/samplerate.h dsp/decibel.h utils/counter.h dsp/declick.h

dsp/decibel.o: dsp/decibel.c dsp/decibel.h

utils/counter.o: utils/counter.c utils/counter.h

dsp/biquad.o: dsp/biquad.c dsp/biquad.h

dsp/declick.o: dsp/declick.c dsp/declick.h

dsp/riaa_calc.o: dsp/riaa_calc.c dsp/riaa_calc.h dsp/biquad.h dsp/riaa_coeffs.h

dsp/notch.o: dsp/notch.c dsp/notch.h

utils/ini.o: utils/ini.c utils/ini.h

utils/configfile.o: utils/configfile.c utils/configfile.h utils/ini.h

clean:
	rm -f *.o dsp/*.o utils/*.o *.so riaa_process
	rm -rf $(LV2_BUNDLE)/*.so

install: install-ladspa install-lv2

install-ladspa: riaa.so
	mkdir -p $(LADSPA_INSTALL_DIR)
	cp riaa.so $(LADSPA_INSTALL_DIR)/
	@echo "Installed riaa.so to $(LADSPA_INSTALL_DIR)/"

install-lv2: $(LV2_BUNDLE)/riaa.so
	mkdir -p $(LV2_INSTALL_DIR)/riaa.lv2
	cp $(LV2_BUNDLE)/riaa.so $(LV2_INSTALL_DIR)/riaa.lv2/
	cp $(LV2_BUNDLE)/manifest.ttl $(LV2_INSTALL_DIR)/riaa.lv2/
	cp $(LV2_BUNDLE)/riaa.ttl $(LV2_INSTALL_DIR)/riaa.lv2/
	@echo "Installed LV2 bundle to $(LV2_INSTALL_DIR)/riaa.lv2/"

uninstall:
	rm -f $(LADSPA_INSTALL_DIR)/riaa.so
	rm -rf $(LV2_INSTALL_DIR)/riaa.lv2
	@echo "Uninstalled RIAA plugins"
