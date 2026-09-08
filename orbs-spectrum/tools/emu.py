#!/usr/bin/env python3
"""A headless ZX Spectrum 48K around the `z80` package: enough machine to run
the game, press its keys, read its variables and photograph its screen.

    python3 tools/emu.py build/orbs.bin build/orbs.map shot.png [frames] [keys...]

Keys are given as names like W, A, SPACE, ENTER, 1 ... each held for a few
frames in turn. In tests, use the Spectrum class directly.
"""
import os, re, sys
import z80
from PIL import Image

FRAME_TICKS = 69888
ROM = "/usr/share/spectrum-roms/48.rom"

# Half-row, bit for every key on the matrix.
KEYS = {}
for row, names in enumerate([
        ["SHIFT", "Z", "X", "C", "V"], ["A", "S", "D", "F", "G"],
        ["Q", "W", "E", "R", "T"], ["1", "2", "3", "4", "5"],
        ["0", "9", "8", "7", "6"], ["P", "O", "I", "U", "Y"],
        ["ENTER", "L", "K", "J", "H"], ["SPACE", "SYM", "M", "N", "B"]]):
    for bit, name in enumerate(names):
        KEYS[name] = (row, bit)

PALETTE = [(0, 0, 0), (0, 0, 0xD8), (0xD8, 0, 0), (0xD8, 0, 0xD8),
           (0, 0xD8, 0), (0, 0xD8, 0xD8), (0xD8, 0xD8, 0), (0xD8, 0xD8, 0xD8),
           (0, 0, 0), (0, 0, 0xFF), (0xFF, 0, 0), (0xFF, 0, 0xFF),
           (0, 0xFF, 0), (0, 0xFF, 0xFF), (0xFF, 0xFF, 0), (0xFF, 0xFF, 0xFF)]


def read_map(path):
    """Symbol table from the sdld .map file: name -> address."""
    syms = {}
    if not path or not os.path.exists(path):
        return syms
    for line in open(path):
        m = re.match(r"\s+([0-9A-F]{8})\s+(\S+)", line)
        if m:
            syms[m.group(2)] = int(m.group(1), 16)
    return syms


class Spectrum:
    def __init__(self, binary, org=0x8000, entry=0x8000, mapfile=None):
        self.m = z80.Z80Machine()
        if os.path.exists(ROM):
            self.m.set_memory_block(0, open(ROM, "rb").read())
        self.m.set_memory_block(org, binary)
        self.m.pc = entry
        self.m.sp = 0xFD00
        self.pressed = set()
        self.border = 0
        self.m.set_input_callback(self._input)
        self.m.set_output_callback(self._output)
        self.syms = read_map(mapfile)
        self.frames = 0

    # --- ports -----------------------------------------------------------
    def _input(self, addr):
        if (addr & 0xFF) == 0xFE:
            hi = addr >> 8
            v = 0x1F
            for (row, bit) in self.pressed:
                if not (hi & (1 << row)):
                    v &= ~(1 << bit)
            return v | 0xA0
        if (addr & 0xFF) == 0x1F:
            return 0xFF          # no Kempston fitted
        return 0xFF

    def _output(self, addr, value):
        if (addr & 0xFF) == 0xFE:
            self.border = value & 7

    # --- running ---------------------------------------------------------
    def frame(self, n=1):
        for _ in range(n):
            self.m.ticks_to_stop = FRAME_TICKS
            while True:
                ev = self.m.run()
                if ev & self.m._TICKS_LIMIT_HIT or self.m.ticks_to_stop == 0:
                    break
                if ev & self.m._BREAKPOINT_HIT:
                    self.m.step_over_breakpoint()
            self.m.on_handle_active_int()
            self.frames += 1

    def press(self, *names):
        for n in names:
            self.pressed.add(KEYS[n.upper()])

    def release(self, *names):
        for n in names:
            self.pressed.discard(KEYS[n.upper()])

    def tap(self, name, hold=3, gap=4):
        """Press a key for `hold` frames, then let go for `gap` frames."""
        self.press(name)
        self.frame(hold)
        self.release(name)
        self.frame(gap)

    # --- memory ----------------------------------------------------------
    def addr(self, sym):
        return self.syms["_" + sym] if "_" + sym in self.syms else self.syms[sym]

    def peek(self, sym, n=1):
        a = self.addr(sym) if isinstance(sym, str) else sym
        return bytes(self.m.memory[a:a + n])

    def peek8(self, sym):
        return self.peek(sym)[0]

    def peek16(self, sym):
        b = self.peek(sym, 2)
        return b[0] | (b[1] << 8)

    def poke(self, sym, data):
        a = self.addr(sym) if isinstance(sym, str) else sym
        self.m.set_memory_block(a, bytes(data))

    # --- screen ----------------------------------------------------------
    def screen(self, scale=2, border=16):
        mem = self.m.memory
        img = Image.new("RGB", (256 + 2 * border, 192 + 2 * border), PALETTE[self.border])
        px = img.load()
        for y in range(192):
            line = 0x4000 + ((y & 0xC0) << 5) + ((y & 7) << 8) + ((y & 0x38) << 2)
            arow = 0x5800 + (y >> 3) * 32
            for cx in range(32):
                b = mem[line + cx]
                attr = mem[arow + cx]
                ink = (attr & 7) + (8 if attr & 0x40 else 0)
                paper = ((attr >> 3) & 7) + (8 if attr & 0x40 else 0)
                for bit in range(8):
                    on = b & (0x80 >> bit)
                    px[border + cx * 8 + bit, border + y] = PALETTE[ink if on else paper]
        if scale != 1:
            img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
        return img

    def text_rows(self):
        """A rough read of the attribute map, for tests that ask what is where."""
        return [bytes(self.m.memory[0x5800 + r * 32:0x5800 + r * 32 + 32]) for r in range(24)]


def main():
    binary = open(sys.argv[1], "rb").read()
    mapfile = sys.argv[2]
    out = sys.argv[3]
    frames = int(sys.argv[4]) if len(sys.argv) > 4 else 50
    spec = Spectrum(binary, mapfile=mapfile)
    spec.frame(frames)
    for key in sys.argv[5:]:
        spec.tap(key)
    spec.frame(5)
    spec.screen(scale=3).save(out)
    print(f"{out}: {spec.frames} frames, pc=0x{spec.m.pc:04X}")


if __name__ == "__main__":
    main()
