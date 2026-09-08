# Orbs of the Overlord -- ZX Spectrum build.
#
#   make          build/orbs.tap
#   make shot     a screenshot from the headless emulator after 60 frames
#   make test     the emulator test-suite
#
# Needs: sdcc (z80), python3 with the `z80` and `pillow` packages.

CC      = sdcc
AS      = sdasz80
CFLAGS  = -mz80 --opt-code-speed --std-c11 --max-allocs-per-node 8000 \
          --disable-warning 85 --disable-warning 126 -Isrc
CODE    = 0x8000
DATA    = 0x5B00

SRC_C   = $(wildcard src/*.c)
SRC_S   = src/sys.s src/gfx.s
OBJ     = build/crt0.rel $(patsubst src/%.c,build/%.rel,$(SRC_C)) \
          $(patsubst src/%.s,build/%.rel,$(SRC_S))

all: build/orbs.tap

src/gfx.s src/gfx.h: assets/art.py tools/mkgfx.py
	python3 tools/mkgfx.py

build/crt0.rel: src/crt0.s
	@mkdir -p build
	$(AS) -o $@ $<

build/%.rel: src/%.s
	@mkdir -p build
	$(AS) -o $@ $<

build/%.rel: src/%.c src/gfx.h $(wildcard src/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

build/orbs.ihx: $(OBJ)
	$(CC) -mz80 --no-std-crt0 --code-loc $(CODE) --data-loc $(DATA) \
	      -Wl-m -o $@ $(OBJ)

build/orbs.bin: build/orbs.ihx
	makebin -p -s 65536 $< > build/orbs.full
	python3 -c "import sys; d=open('build/orbs.full','rb').read(); open('$@','wb').write(d[$(CODE):])"
	python3 tools/memmap.py build/orbs.map

build/orbs.tap: build/orbs.bin
	python3 tools/mktap.py $< $@ $(CODE)

# The rules, compiled for the host: dungeon.c and friends are the same files
# the Spectrum build uses, so the harness exercises the game rather than a
# model of it.
HOST_SRC = tests/host/harness.c src/dungeon.c src/actors.c src/items.c \
           src/rng.c src/vision.c src/data.c

build/host/harness: $(HOST_SRC) src/game.h src/gfx.h
	@mkdir -p build/host
	gcc -O1 -Wall -D__z88dk_fastcall= -Isrc -o $@ $(HOST_SRC)

host: build/host/harness

shot: build/orbs.bin
	python3 tools/emu.py build/orbs.bin build/orbs.map build/shot.png 60

play: build/orbs.bin
	python3 tools/play.py build/play.png 150

test: build/orbs.bin build/host/harness
	python3 -m pytest -q tests

clean:
	rm -rf build src/gfx.s src/gfx.h

.PHONY: all host shot play test clean
