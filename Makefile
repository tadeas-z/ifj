# Implementation of the IFJ26 imperative language compiler
# Makefile
# Author(s): Tadeáš Zonyga (xzonygt00), Marek Zachensky (xzachem00)

CC = gcc
CPPFLAGS = -Iinc -Isrc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -g

TARGET = build/ifj26
SOURCES = $(wildcard src/*.c src/*/*.c)
OBJECTS = $(patsubst src/%.c,build/%.o,$(SOURCES))
DEPS = $(OBJECTS:.o=.d)

.PHONY: all clean check build-check pack
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

build/%.o: src/%.c
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(@D)' | Out-Null"
else
	mkdir -p $(@D)
endif
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -Command "if (Test-Path -LiteralPath 'build') { if ((Get-Item -LiteralPath 'build').Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Refusing to remove a linked build directory' }; Remove-Item -LiteralPath 'build' -Recurse -Force }"
else
	rm -rf build
endif

CHECK_FILES = $(shell find src inc tests -name '*.[ch]')

check:
	clang-format --dry-run --Werror $(CHECK_FILES)
	cppcheck --error-exitcode=1 -Iinc --inline-suppr $(CHECK_FILES)

build-check: check all

PACK_ZIP = xpopovd00.zip
PACK_DIR = build/pack
PACK_FILES = $(wildcard rozdeleni rozsireni dokumentace.pdf)

pack:
	rm -rf $(PACK_DIR) $(PACK_ZIP)
	mkdir -p $(PACK_DIR)/src
	cp $(shell find src inc -name '*.[ch]') $(PACK_DIR)/src/
	cp Makefile $(PACK_FILES) $(PACK_DIR)/
	cd $(PACK_DIR) && zip -r ../../$(PACK_ZIP) .

-include $(DEPS)
