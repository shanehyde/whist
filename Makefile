MAKE=/usr/bin/make

.PHONY: all w0 wc test test-verbose test-run test-errors clean format metrics complexity

all: w0 wc

w0:
	$(MAKE) -C w0

wc: w0
	$(MAKE) -C wc

test:
	$(MAKE) -C w0 test

test-verbose:
	$(MAKE) -C w0 test-verbose

test-run:
	$(MAKE) -C w0 test-run

test-errors:
	$(MAKE) -C w0 test-errors

clean:
	$(MAKE) -C w0 clean
	$(MAKE) -C wc clean

format:
	$(MAKE) -C w0 format

metrics:
	$(MAKE) -C w0 metrics

complexity:
	$(MAKE) -C w0 complexity
