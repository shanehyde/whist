MAKE=/usr/bin/make

.PHONY: all w0 wc test test-verbose test-run test-errors compare clean format metrics complexity

all: w0 wc

w0:
	$(MAKE) -C w0

wc: w0
	$(MAKE) -C wc

test:
	$(MAKE) -C w0 test FILTER=$(FILTER)

test-verbose:
	$(MAKE) -C w0 test-verbose FILTER=$(FILTER)

test-run:
	$(MAKE) -C w0 test-run FILTER=$(FILTER)

test-errors:
	$(MAKE) -C w0 test-errors FILTER=$(FILTER)

compare: w0 wc
	tools/compare_output.sh $(if $(FILTER),$(FILTER))

clean:
	$(MAKE) -C w0 clean
	$(MAKE) -C wc clean

format:
	$(MAKE) -C w0 format

metrics:
	$(MAKE) -C w0 metrics

complexity:
	$(MAKE) -C w0 complexity
