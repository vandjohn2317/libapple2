#!/usr/bin/env python3
"""Play the game in the headless Spectrum and photograph it.

    python3 tools/play.py build/shot.png 200

The hero walks toward whatever is worth walking toward -- the nearest thing
on the floor, or the stairs once the floor is picked clean -- so the picture
is of a game in progress rather than of a hero standing in a corridor. The
route is worked out from the game's OWN grid, read out of the emulator's
memory, and it is driven with key presses like a person would.
"""
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import Spectrum  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# The player struct as sdcc packs it: x, y, facing, job, level, exp, hp, ...
P_HP, P_LEVEL, P_GOLD = 7, 4, 15
KEYS = {0: "W", 1: "D", 2: "S", 3: "A"}
DIRS = {0: (0, -1), 1: (1, 0), 2: (0, 1), 3: (-1, 0)}
WALKABLE = (1, 2, 4)


def grid(s):
    return s.peek("grid", 42 * 42)


def route(g, start, goals):
    """Breadth-first over walkable tiles and shut doors, which open on a bump."""
    goals = set(goals)
    prev = {start: None}
    q = collections.deque([start])
    while q:
        p = q.popleft()
        if p in goals:
            path = []
            while p != start:
                path.append(p)
                p = prev[p]
            return path[::-1]
        for dx, dy in DIRS.values():
            n = (p[0] + dx, p[1] + dy)
            if n in prev or not (0 <= n[0] < 42 and 0 <= n[1] < 42):
                continue
            t = g[n[1] * 42 + n[0]] & 15
            if t not in WALKABLE and t != 3:
                continue
            prev[n] = p
            q.append(n)
    return []


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "play.png")
    turns = int(sys.argv[2]) if len(sys.argv) > 2 else 150
    s = Spectrum(open(os.path.join(ROOT, "build", "orbs.bin"), "rb").read(),
                 mapfile=os.path.join(ROOT, "build", "orbs.map"))
    s.frame(60)
    s.tap("SPACE", 4, 6)
    for _ in range(200):
        s.frame(10)
        if s.peek8("exit_x"):
            break
    s.frame(60)

    P = s.addr("player")
    for turn in range(turns):
        g = grid(s)
        here = (s.m.memory[P], s.m.memory[P + 1])
        goals = []
        for i in range(40):
            b = s.peek(s.addr("items") + i * 5, 3)
            if b[0]:
                goals.append((b[1], b[2]))
        for i in range(24):
            b = s.peek(s.addr("enemies") + i * 23, 5)
            if b[0] != 0xFF and (b[3] | (b[4] << 8)):
                goals.append((b[1], b[2]))
        if not goals:
            goals = [(s.peek8("exit_x"), s.peek8("exit_y"))]
        path = route(g, here, goals)
        if not path:
            break
        step = path[0]
        face = next(f for f, (dx, dy) in DIRS.items()
                    if (here[0] + dx, here[1] + dy) == step)
        if s.m.memory[P + 2] != face:
            s.tap(KEYS[face], 3, 6)
            s.frame(12)
        s.tap(KEYS[face], 3, 6)
        s.frame(28)
        if s.m.memory[P + P_HP] | (s.m.memory[P + P_HP + 1] << 8):
            continue
        break                       # the hero is dead
    s.frame(40)
    s.screen(scale=3).save(out)
    hp = s.m.memory[P + P_HP] | (s.m.memory[P + P_HP + 1] << 8)
    print(f"{out}: floor {s.peek8('floor_no')}, level {s.m.memory[P + P_LEVEL]}, "
          f"hp {hp}, gold {s.m.memory[P + P_GOLD] | (s.m.memory[P + P_GOLD + 1] << 8)}, "
          f"log {bytes(s.peek('log_line', 33)).split(bytes([0]))[0].decode('ascii', 'replace')!r}")


if __name__ == "__main__":
    main()
