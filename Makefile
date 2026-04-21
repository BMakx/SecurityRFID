# Root Makefile
#
# Delegates to the per-device Makefiles.
# Run `make` to build both, `make master` or `make slave` for one device.

.PHONY: all master slave clean

all: master slave

master:
	$(MAKE) -C master

slave:
	$(MAKE) -C slave

clean:
	$(MAKE) -C master clean
	$(MAKE) -C slave  clean
