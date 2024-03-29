BUILD_TYPE := $(shell echo $(MAKECMDGOALS) | cut -d '-' -f2)
BUILD_TYPE_UPPER := $(shell echo $(BUILD_TYPE) | tr a-z A-Z)

.DEFAULT_GOAL := all
SHELL := /bin/bash

.PHONY : all
all: debug


# setup target creates the build directory and links it in

default_flags = -G"Unix Makefiles" -DENABLE_TESTING=TRUE -DENABLE_TOOLS=TRUE -DENABLE_COVERAGE=FALSE

.PHONY : setup
setup: setup-debug

.PHONY : submodule-update
submodule-update :
	git submodule sync --recursive
	git submodule update --init --recursive
	git submodule foreach --recursive git lfs pull

ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

.PHONY : setup-debug
setup-debug: submodule-update
	mkdir -p build/debug
	pushd build/debug && cmake \
		-DCMAKE_BUILD_TYPE=DEBUG \
		$(default_flags) \
		$(CURDIR)/src

.PHONY : clean
clean: clean-debug

.PHONY : clean-debug
clean-debug:
	rm -rf build


.PHONY : debug
debug:
	$(MAKE) --no-print-directory -C build/debug

.PHONY : format
format:
	./.format.sh