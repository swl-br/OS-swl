# swl-compiler Makefile
#
# Targets:
#   all      - build the swlc compiler (host tool, C only)
#   rt       - assemble the Assembly runtime (needs nasm)
#   test     - compile+assemble+link+run the SWL test programs (needs nasm)
#   clean    - remove build artifacts
#
# Compiler source layout:
#   src/swlc.h src/lexer.c src/parser.c src/sema.c src/codegen.c src/main.c

BUILD = build
CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2 -g
NASM ?= nasm
LD ?= ld

SRCS = src/main.c src/lexer.c src/parser.c src/sema.c src/codegen.c

.PHONY: all rt test clean

all: $(BUILD)/swlc

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/swlc: $(SRCS) src/swlc.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

rt: $(BUILD)/swlrt.o

$(BUILD)/swlrt.o: src/swlrt.asm | $(BUILD)
	$(NASM) -f elf32 -o $@ $<

test: all rt
	sh tests/runtests.sh $(abspath $(BUILD))

clean:
	rm -rf $(BUILD)
	# examples/*.out are checked-in test fixtures, never build artifacts
	rm -f examples/*.asm examples/*.o 2>/dev/null || true
