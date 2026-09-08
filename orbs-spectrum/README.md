# Orbs of the Overlord — ZX Spectrum

A demake of *Orbs of the Overlord* (7 Towers Soft) for the 48K ZX Spectrum.

The Godot build draws a 3D dungeon, quantises it to four tones and dithers it
to chunky pixels. Underneath that it is a turn-based roguelite on a 42×42 grid
where every number is an integer and every monster takes one greedy step
towards you. That is the game this port is: the same rules, on the machine the
look was already imitating.

**The interface is English only.** The Godot build has a Greek table beside the
English one; this one does not, and the font has no Greek glyphs. That is a
deliberate choice for the 48K, not an omission — see *What is not here*.

## Building

Needs `sdcc` (with the z80 target), `python3`, and the Python packages `z80`
and `pillow`.

```
apt-get install sdcc
pip install z80 pillow pytest
make            # build/orbs.tap
make test       # the emulator suite and the host harness
make play       # play a scripted session and photograph it
make shot       # one screenshot after boot
```

`build/orbs.tap` loads on a real 48K, on a +2/+3, or in any emulator. The tape
carries a BASIC loader that does `CLEAR 32767: LOAD "" CODE: RANDOMIZE USR
32768`.

## What runs

* **The floors.** The generator is `Dungeon.gd` ported whole: a random walk
  with 78% momentum, so it digs winding corridors rather than caverns; floors
  that grow from 260 steps and six monsters on floor one to the full 42×42 and
  two dozen by floor ten; doors only on tiles whose removal cuts the floor in
  two, so a door always gates something; and the stairs down in the far half of
  the walk, measured as a walk rather than as a distance across the map.
* **Sight.** A circle of two all round, plus a cone ahead that is 2d−1 wide at
  distance d and reaches as far as the class can see (a Warrior five). Line of
  sight is a Bresenham walk; a wall stops the ray but is itself seen, and so is
  a shut door, which is what puts fog behind it until it opens.
* **Fighting.** Every blow is d20 + attack bonus against d20 + (armour class −
  10), a natural 20 always lands and a natural 1 always misses. Armour takes a
  quarter of the defence pool off a blow but never more than 60% of the blow
  itself. Depth is expressed in hit points and damage, never in armour class,
  which is what stops the dice walking off the end of their own scale.
* **The roster.** Twelve monsters in the order they are first met, with the
  six traits that make them different things rather than different numbers:
  fast, venomous, thieving, bursting, mending, armoured.
* **Levels.** 100 + 100×(level−1) to the next one; a level gives health,
  attributes, defence and thievery, and heals to full. Measured over forty
  floors, you arrive at floor N at about level N.
* **The floor's furniture.** Gold, keys, and chests that can be picked with
  Thievery (capped at 35%, however high it climbs), opened with a key, or
  beaten open — which destroys what is inside 93 times in 100.

## The screen

Sixteen by ten tiles of 16×16 pixels, then four rows of text.

Every tile and every sprite is authored in **four tones** in `assets/art.py`
and collapsed to the Spectrum's one bit by ordered dither at build time —
tone 3 solid, tone 2 a checker, tone 1 a quarter dot, and any isolated pixel
kept lit so a speck on a floor does not vanish. It is the same reduction the
Godot build's shader performs, done once at build time instead of every frame.

Colour is one ink per 8×8 cell, so **everything is aligned to the cell grid**
and attribute clash never happens: a monster occupies exactly the four cells of
its tile. A tile in view is drawn `BRIGHT` and one only remembered is drawn in
the same ink without it, which is the fog of war for nothing.

Sprites carry a mask that is the sprite dilated by one pixel, so a figure
always stands in a one-pixel halo of paper and never merges with the floor
pattern behind it.

## The machine

```
0x4000-0x5AFF  screen
0x5B00-0x7FFF  variables          (contended memory, data only)
0x8000-0xC8xx  code, graphics, text
0xFA00-0xFCFF  stack
0xFDFD         IM2 handler stub
0xFE00-0xFF00  IM2 vector table
```

`src/crt0.s` sets up IM2 and counts frames; `src/sys.s` is the only code that
knows the ULA exists — screen addresses, the four blitters, the 5×7 font, the
bars, and the keyboard matrix. Everything above that is C, and the same C
compiles on the host (see below).

`make` prints where the areas landed and how much room is left, and fails the
build if the code ever reaches the stack.

## Testing

Two harnesses, and neither of them models the game.

**`tests/test_game.py`** drives the real binary in a headless Spectrum built
on the `z80` package (`tools/emu.py`): it presses keys through the keyboard
matrix, reads the game's own variables out of memory by symbol name, and looks
at the screen the ULA would have produced. It checks that a floor hangs
together, that walking lifts the fog, that a wall costs nothing, that bumping a
monster starts an exchange, that the stairs go down, that death starts a new
run, and that two hundred presses of a hand mashing the keys leaves the stack
where it was.

**`tests/host/harness.c`** compiles `dungeon.c`, `actors.c`, `items.c`,
`vision.c` and `rng.c` — the rules themselves, not a copy of them — with gcc,
and runs them thousands of times: four hundred floors checked for a reachable
exit, the level curve walked to floor forty, the whole roster fought at three
depths.

```
./build/host/harness floors 400
./build/host/harness combat 20000
./build/host/harness levels
```

## What is not here

This is the first playable slice. Everything below is in the Godot build and
not yet in this one:

* the other two classes, and the screen that asks which one you are;
* weapons, armour and the fifteen-weapon ladder (the hero fights bare-handed);
* spells and rages, and the resource bar that pays for them (the bar is drawn
  and full, and nothing spends it yet);
* the fifteen bosses, their arenas and their telegraphs;
* the duel screen, with its dice, its footing and its flavoured ground;
* the rift, and the futuristic side of it — the tiles, the sprites and the
  palette switch are built and wired to `theme_futuristic`, but nothing sets
  it yet;
* the shop, saving, achievements and the record;
* sound.

**And the Greek.** The Godot build's interface is in two languages. This one is
in one, on purpose: a Greek font costs about sixty 8×8 glyphs and a second copy
of every line of interface text, which is a few kilobytes on a machine that has
about eleven left. If the port grows onto a 128K it can have both.
