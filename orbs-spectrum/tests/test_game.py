"""The game, tested by running it.

Everything here drives the real binary in a headless Spectrum: it presses
keys, reads the game's own variables out of memory, and looks at the screen
the ULA would have produced. There is no model of the game to fall out of
step with it.
"""
import os
import subprocess
import sys

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from emu import Spectrum  # noqa: E402

BIN = os.path.join(ROOT, "build", "orbs.bin")
MAP = os.path.join(ROOT, "build", "orbs.map")
HARNESS = os.path.join(ROOT, "build", "host", "harness")

ENEMY_SIZE = 23
ITEM_SIZE = 5

# The Player struct as sdcc packs it (no padding). Kept in one place so a
# field added to game.h is a one-line change here.
P = {}
_off = 0
for _name, _size in (("x", 1), ("y", 1), ("facing", 1), ("job", 1), ("level", 1),
                     ("exp", 2), ("hp", 2), ("hp_max", 2), ("strength", 1),
                     ("agility", 1), ("thievery", 1), ("base_defense", 1),
                     ("gold", 2), ("keys", 1), ("armor_points", 1), ("armor", 1),
                     ("weapon", 1), ("resource", 2), ("resource_max", 2)):
    P[_name] = (_off, _size)
    _off += _size


def boot(frames=60):
    s = Spectrum(open(BIN, "rb").read(), mapfile=MAP)
    s.frame(frames)
    return s


def start_run(s, timeout_frames=1200):
    """Press a key at the title and wait for the first floor to be standing."""
    s.tap("SPACE", 4, 6)
    for _ in range(timeout_frames // 10):
        s.frame(10)
        if s.peek8("exit_x") and s.peek8("player"):
            s.frame(60)          # let the first draw finish
            return True
    return False


def player_field(s, name):
    off, size = P[name]
    v = s.peek(s.addr("player") + off, size)
    return v[0] if size == 1 else v[0] | (v[1] << 8)


def px(s):
    return player_field(s, "x")


def py(s):
    return player_field(s, "y")


def hp(s):
    return player_field(s, "hp")


def text(s, sym):
    return bytes(s.peek(sym, 33)).split(b"\0")[0].decode("ascii", "replace")


def tile(s, x, y):
    return s.peek(s.addr("grid") + y * 42 + x)[0] & 0x0F


def live_enemies(s):
    base = s.addr("enemies")
    out = []
    for i in range(24):
        b = s.peek(base + i * ENEMY_SIZE, ENEMY_SIZE)
        if b[0] != 0xFF and (b[3] | (b[4] << 8)):
            out.append({"type": b[0], "x": b[1], "y": b[2], "hp": b[3] | (b[4] << 8)})
    return out


def items(s):
    base = s.addr("items")
    out = []
    for i in range(40):
        b = s.peek(base + i * ITEM_SIZE, ITEM_SIZE)
        if b[0]:
            out.append({"kind": b[0], "x": b[1], "y": b[2]})
    return out


# --------------------------------------------------------------- the machine
def test_it_boots_and_shows_the_title():
    s = boot(80)
    attrs = s.peek(0x5800, 768)
    assert any(a & 7 == 6 for a in attrs), "no yellow on the title screen"
    ink = sum(1 for b in s.peek(0x4000, 6144) if b)
    assert ink > 200, "the title screen is blank"


def test_the_stack_and_the_code_do_not_meet():
    """The areas the linker reports have to end below the stack, and the
    variables have to end below the code."""
    areas = {}
    for line in open(MAP):
        parts = line.split()
        if len(parts) >= 4 and parts[0].startswith("_") and parts[3] == "=":
            areas[parts[0]] = (int(parts[1], 16), int(parts[2], 16))
    assert areas, "no areas in the map file"
    code_end = max(a + n for k, (a, n) in areas.items() if a >= 0x8000)
    data_end = max(a + n for k, (a, n) in areas.items() if a < 0x8000)
    assert code_end < 0xFA00, f"code ends at 0x{code_end:04X}, in the stack"
    assert data_end <= 0x8000, f"data ends at 0x{data_end:04X}, in the code"


# --------------------------------------------------------------- a run
def test_a_run_starts_on_a_floor_that_hangs_together():
    s = boot()
    assert start_run(s), "the first floor never finished building"
    assert px(s) == s.peek8("start_x") and py(s) == s.peek8("start_y")
    assert tile(s, px(s), py(s)) in (1, 2, 4), "the hero stands in rock"
    assert tile(s, s.peek8("exit_x"), s.peek8("exit_y")) == 2, "no stairs down"
    assert hp(s) == 85, "the Warrior does not start with 85 health"
    assert live_enemies(s), "nothing on the floor to fight"
    kinds = {i["kind"] for i in items(s)}
    assert 1 in kinds and 2 in kinds and 3 in kinds, "no gold, keys and chests"


def test_walking_moves_the_hero_and_lifts_the_fog():
    s = boot()
    assert start_run(s)
    before = (px(s), py(s))
    revealed_before = sum(bin(b).count("1") for b in s.peek("revealed", 222))
    moved = False
    for key in ("S", "D", "W", "A", "S", "D"):
        s.tap(key, 3, 4)
        s.frame(30)
        if (px(s), py(s)) != before:
            moved = True
            break
    assert moved, "the hero would not walk"
    revealed_after = sum(bin(b).count("1") for b in s.peek("revealed", 222))
    assert revealed_after >= revealed_before, "the fog went back"


def test_a_wall_costs_nothing_and_says_so():
    s = boot()
    assert start_run(s)
    # face and walk into rock: whichever direction is blocked
    for key, dx, dy in (("W", 0, -1), ("S", 0, 1), ("A", -1, 0), ("D", 1, 0)):
        x, y = px(s), py(s)
        if tile(s, x + dx, y + dy) != 0:
            continue
        s.tap(key, 3, 4)
        s.frame(20)                     # the turn to face
        s.tap(key, 3, 4)
        s.frame(20)
        assert (px(s), py(s)) == (x, y), "walked into rock"
        assert "WALL" in text(s, "log_line")
        return
    pytest.skip("the hero started in the open")


def test_bumping_a_monster_fights_it():
    s = boot()
    assert start_run(s)
    # put a goblin where the hero is facing, and make the hero face it
    x, y = px(s), py(s)
    for dx, dy, facing in ((0, 1, 2), (0, -1, 0), (1, 0, 1), (-1, 0, 3)):
        if tile(s, x + dx, y + dy) in (1, 2, 4):
            break
    else:
        pytest.skip("the hero is walled in")
    base = s.addr("enemies")
    s.poke(base, bytes([0, x + dx, y + dy, 26, 0, 26, 0, 3, 3, 18, 0, 10, 2, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0]))
    s.poke(s.addr("player") + P["facing"][0], bytes([facing]))
    key = {0: "W", 1: "D", 2: "S", 3: "A"}[facing]
    s.tap(key, 3, 4)
    s.frame(40)
    line = text(s, "log_line")
    assert ("HIT" in line or "MISS" in line or "CRIT" in line), f"no exchange: {line!r}"
    assert (px(s), py(s)) == (x, y), "the hero walked through the monster"


def test_the_stairs_lead_down():
    s = boot()
    assert start_run(s)
    # stand the hero next to the stairs and walk on
    ex, ey = s.peek8("exit_x"), s.peek8("exit_y")
    for dx, dy, facing in ((0, -1, 2), (0, 1, 0), (-1, 0, 1), (1, 0, 3)):
        if tile(s, ex + dx, ey + dy) in (1, 4):
            break
    else:
        pytest.skip("the stairs are boxed in")
    s.poke(s.addr("player"), bytes([ex + dx, ey + dy, facing]))
    key = {0: "W", 1: "D", 2: "S", 3: "A"}[facing]
    s.tap(key, 3, 4)
    for _ in range(120):
        s.frame(10)
        if s.peek8("floor_no") == 2:
            break
    assert s.peek8("floor_no") == 2, "the stairs went nowhere"


def test_death_puts_a_new_hero_on_floor_one():
    s = boot()
    assert start_run(s)
    # walk down two floors' worth of hero, then take the health away
    s.poke(s.addr("player") + P["hp"][0], bytes([1, 0]))
    s.poke(s.addr("floor_no"), bytes([7]))
    base = s.addr("enemies")
    x, y = px(s), py(s)
    for dx, dy, facing in ((0, 1, 2), (0, -1, 0), (1, 0, 1), (-1, 0, 3)):
        if tile(s, x + dx, y + dy) in (1, 2, 4):
            break
    else:
        pytest.skip("the hero is walled in")
    # a monster that cannot miss and hits hard
    s.poke(base, bytes([11, x + dx, y + dy, 200, 0, 200, 0, 20, 90, 52, 0, 30, 40, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0]))
    s.poke(s.addr("player") + P["facing"][0], bytes([facing]))
    key = {0: "W", 1: "D", 2: "S", 3: "A"}[facing]
    for _ in range(8):
        s.tap(key, 3, 4)
        s.frame(40)
        if hp(s) == 0:
            break
    assert hp(s) == 0, "the hero would not die"
    s.frame(30)
    s.tap("SPACE", 4, 8)
    for _ in range(200):
        s.frame(10)
        if s.peek8("floor_no") == 1 and hp(s) == 85:
            break
    assert s.peek8("floor_no") == 1 and hp(s) == 85, "death did not start a new run"


def test_a_long_session_does_not_fall_over():
    """Two hundred presses of a hand mashing the keys."""
    s = boot()
    assert start_run(s)
    keys = ["W", "A", "S", "D", "S", "D", "W", "A", "X", "SPACE"]
    for i in range(200):
        s.tap(keys[i % len(keys)], 2, 3)
        s.frame(6)
        assert 0x8000 <= s.m.pc <= 0xFFFF, f"ran off into 0x{s.m.pc:04X}"
    assert s.m.sp > 0xFA00, f"the stack ran away to 0x{s.m.sp:04X}"
    assert hp(s) <= player_field(s, "hp_max"), "health above the maximum"


# --------------------------------------------------------------- the rules
@pytest.mark.skipif(not os.path.exists(HARNESS), reason="host harness not built")
def test_floors_are_all_connected_and_have_a_way_down():
    out = subprocess.run([HARNESS, "floors", "300"], capture_output=True, text=True)
    assert out.returncode == 0, out.stdout
    assert "no exit placed    0" in out.stdout
    assert "exit unreachable  0" in out.stdout
    assert "flood marks left  0" in out.stdout
    doors = float([l for l in out.stdout.splitlines() if "doors per floor" in l][0].split()[-1])
    assert doors >= 3.0, f"only {doors} doors a floor"


@pytest.mark.skipif(not os.path.exists(HARNESS), reason="host harness not built")
def test_you_arrive_at_floor_n_at_about_level_n():
    out = subprocess.run([HARNESS, "levels"], capture_output=True, text=True)
    rows = [l.split() for l in out.stdout.splitlines()[1:] if l.strip()]
    for floor, level, _hp, _swing, _ac in rows:
        floor, level = int(floor), int(level)
        assert level >= floor * 0.7, f"floor {floor} reached at level {level}"
        assert level <= floor * 1.6 + 2, f"floor {floor} reached at level {level}"
