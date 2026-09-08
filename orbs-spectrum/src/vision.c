/* vision.c -- what the hero can see: a small circle all round, and a cone
 * ahead that widens with distance. A wall stops the ray but is itself seen;
 * so does a shut door, which is what puts fog behind it until it opens. */
#include <string.h>
#include "game.h"

#define OMNI_RADIUS 2

uint8_t los_clear(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    int8_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int8_t dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int8_t sx = x0 < x1 ? 1 : -1;
    int8_t sy = y0 < y1 ? 1 : -1;
    int8_t err = dx - dy;
    uint8_t x = x0, y = y0;
    for (;;) {
        int8_t e2;
        uint8_t t;
        if (x == x1 && y == y1)
            return 1;
        e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
        if (x == x1 && y == y1)
            return 1;
        t = tile_at(x, y);
        if (is_solid(t) || t == TL_DOOR)
            return 0;
    }
}

static void see(uint8_t x, uint8_t y)
{
    if (x < GRID && y < GRID && los_clear(player.x, player.y, x, y))
        BM_SET(seen, IDX(x, y));
}

void vision_update(void)
{
    int8_t dx, dy, d, off;
    uint8_t maxd = class_stats[player.job].vision;
    int8_t fx = facing_dx[player.facing], fy = facing_dy[player.facing];
    int8_t px = fx ? 0 : 1, py = fx ? 1 : 0;   /* perpendicular */
    uint8_t i;

    memset(seen, 0, BM_BYTES);
    BM_SET(seen, IDX(player.x, player.y));
    for (dy = -OMNI_RADIUS; dy <= OMNI_RADIUS; dy++)
        for (dx = -OMNI_RADIUS; dx <= OMNI_RADIUS; dx++)
            see(player.x + dx, player.y + dy);
    for (d = 1; d <= (int8_t)maxd; d++) {
        int8_t half = d - 1;
        uint8_t bx = player.x + fx * d, by = player.y + fy * d;
        for (off = -half; off <= half; off++)
            see(bx + px * off, by + py * off);
    }
    for (i = 0; i < BM_BYTES; i++)
        revealed[i] |= seen[i];
}
