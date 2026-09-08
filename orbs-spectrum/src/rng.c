/* rng.c -- a 16-bit xorshift. Seeded per run, so a run is reproducible. */
#include "game.h"

uint16_t rng_state = 0x2A4B;

uint16_t rnd16(void)
{
    uint16_t x = rng_state;
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    if (x == 0)
        x = 0x2A4B;
    rng_state = x;
    return x;
}

uint8_t rnd8(void)
{
    return (uint8_t)rnd16();
}

uint8_t rnd_below(uint8_t n) __z88dk_fastcall
{
    if (n == 0)
        return 0;
    return (uint8_t)(((uint16_t)rnd8() * n) >> 8);
}

uint8_t rnd_range(uint8_t lo, uint8_t hi)
{
    return lo + rnd_below((uint8_t)(hi - lo + 1));
}

uint8_t rnd_chance(uint8_t pct) __z88dk_fastcall
{
    return rnd_below(100) < pct;
}

uint8_t d20(void)
{
    return 1 + rnd_below(20);
}
