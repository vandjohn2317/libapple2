/* render.c -- the viewport, the status rows and the log.
 *
 * 16x10 tiles of 16x16 pixels over rows 0..19, then four text rows: who and
 * where, the two bars, what you did, and what the world did back.
 * A tile in view is drawn bright; one remembered from earlier is drawn in the
 * dim shade of the same ink, which is the fog of war for free.
 */
#include <string.h>
#include "game.h"

static uint8_t hue(void)
{
    return theme_futuristic ? CYAN : YELLOW;
}

static uint8_t tile_art(tidx i)
{
    uint8_t v = grid[i];
    switch (TILE(v)) {
    case TL_WALL:
    case TL_SECRET:
        return theme_futuristic ? T_WALL_F : T_WALL;
    case TL_EXIT:
        return T_STAIRS;
    case TL_DOOR:
    case TL_PUZZLE:
        return theme_futuristic ? T_DOOR_F : T_DOOR;
    case TL_DOOR_OPEN:
        return T_DOOR_OPEN;
    default:
        if (theme_futuristic)
            return T_FLOOR_F;
        return VARIANT(v) ? T_FLOOR2 : T_FLOOR;
    }
}

static uint8_t item_sprite(uint8_t kind)
{
    switch (kind) {
    case IT_GOLD: return S_GOLD;
    case IT_KEY: return S_KEY;
    case IT_CHEST: return S_CHEST;
    case IT_BOOK: return S_BOOK;
    default: return S_ORB;
    }
}

static uint8_t item_ink(uint8_t kind)
{
    switch (kind) {
    case IT_GOLD: return YELLOW | BRIGHT;
    case IT_KEY: return WHITE | BRIGHT;
    case IT_CHEST: return YELLOW | BRIGHT;
    case IT_BOOK: return MAGENTA | BRIGHT;
    default: return CYAN | BRIGHT;
    }
}

/* The viewport is drawn in three passes rather than one.
 *
 * Asking every tile what is standing on it means scanning the monster and
 * item tables a hundred and sixty times a turn -- ten thousand slot tests to
 * place at most a dozen things. The tiles go down first, then the tables are
 * walked ONCE each and whatever falls inside the window is stamped on top.
 * The order also settles who covers whom: floor, then items, then monsters,
 * then the hero.
 */
static uint8_t vp_ox, vp_oy;

static void viewport_origin(void)
{
    uint8_t ox = player.x > VW / 2 ? player.x - VW / 2 : 0;
    uint8_t oy = player.y > VH / 2 ? player.y - VH / 2 : 0;
    if (ox > GRID - VW)
        ox = GRID - VW;
    if (oy > GRID - VH)
        oy = GRID - VH;
    vp_ox = ox;
    vp_oy = oy;
}

/* 0xFFFF when the tile is off screen, otherwise its char cell. */
static uint16_t vp_cell(uint8_t x, uint8_t y)
{
    uint8_t tx = x - vp_ox, ty = y - vp_oy;
    if (x < vp_ox || y < vp_oy || tx >= VW || ty >= VH)
        return 0xFFFF;
    return COLROW(tx * 2, ty * 2);
}

void draw_viewport(void)
{
    uint8_t tx, ty, i;
    viewport_origin();

    for (ty = 0; ty < VH; ty++) {
        uint8_t y = vp_oy + ty;
        tidx row = IDX(vp_ox, y);
        for (tx = 0; tx < VW; tx++, row++) {
            uint16_t cr = COLROW(tx * 2, ty * 2);
            if (!BM_GET(revealed, row)) {
                blit_attr = 0;
                clear_tile(cr);
                continue;
            }
            if (BM_GET(seen, row)) {
                blit_src = TILE_PTR(tile_art(row));
                blit_attr = hue() | BRIGHT;
            } else {
                /* remembered: the dimmer pattern as well as the dimmer ink */
                blit_src = TILE_DIM_PTR(tile_art(row));
                blit_attr = hue();
            }
            blit_tile(cr);
        }
    }

    for (i = 0; i < MAX_ITEMS; i++) {
        Item *it = &items[i];
        uint16_t cr;
        tidx at;
        if (!it->kind)
            continue;
        cr = vp_cell(it->x, it->y);
        if (cr == 0xFFFF)
            continue;
        at = IDX(it->x, it->y);
        if (!BM_GET(revealed, at))
            continue;
        blit_src = SPRITE_PTR(item_sprite(it->kind));
        if (BM_GET(seen, at)) {
            blit_attr = item_ink(it->kind);
            blit_masked(cr);
        } else {
            /* remembered, not seen: keep the dim floor colour it sits on */
            blit_masked_noattr(cr);
        }
    }

    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        uint16_t cr;
        if (e->type == 0xFF || e->hp == 0)
            continue;
        if (!BM_GET(seen, IDX(e->x, e->y)))
            continue;
        cr = vp_cell(e->x, e->y);
        if (cr == 0xFFFF)
            continue;
        blit_src = SPRITE_PTR(enemy_types[e->type].sprite);
        blit_attr = enemy_types[e->type].ink;
        blit_masked(cr);
    }

    {
        uint16_t cr = vp_cell(player.x, player.y);
        if (cr != 0xFFFF) {
            blit_src = SPRITE_PTR(theme_futuristic ? S_COMMANDO : S_WARRIOR);
            blit_attr = WHITE | BRIGHT;
            blit_masked(cr);
        }
    }
}

/* ------------------------------------------------------------ text helpers */
void num_str(char *dst, uint16_t n)
{
    char tmp[6];
    uint8_t i = 0;
    if (n == 0) {
        dst[0] = '0';
        dst[1] = 0;
        return;
    }
    while (n) {
        tmp[i++] = '0' + n % 10;
        n /= 10;
    }
    while (i)
        *dst++ = tmp[--i];
    *dst = 0;
}

void log_set(char *dst, const char *s)
{
    uint8_t i = 0;
    while (*s && i < 32)
        dst[i++] = *s++;
    dst[i] = 0;
}

void log_cat(char *dst, const char *s)
{
    uint8_t i = strlen(dst);
    while (*s && i < 32)
        dst[i++] = *s++;
    dst[i] = 0;
}

void log_num(char *dst, uint16_t n)
{
    char buf[6];
    num_str(buf, n);
    log_cat(dst, buf);
}

static void print_padded(uint8_t col, uint8_t row, const char *s, uint8_t attr, uint8_t width)
{
    char buf[33];
    uint8_t i = 0;
    while (*s && i < width)
        buf[i++] = *s++;
    while (i < width)
        buf[i++] = ' ';
    buf[i] = 0;
    print_at(col, row, buf, attr);
}

static void draw_bar(uint8_t col, uint8_t row, uint8_t cells, uint16_t v, uint16_t vmax, uint8_t ink)
{
    uint16_t px = (uint16_t)cells * 8 - 2;
    uint8_t fill = vmax ? (uint8_t)((v * px) / vmax) : 0;
    if (v && fill == 0)
        fill = 1;
    blit_attr = ink | BRIGHT;
    blit_w = cells;
    blit_h = fill;
    bar(COLROW(col, row));
}

void draw_hud(void)
{
    char buf[33];
    uint8_t h = hue() | BRIGHT;
    /* row 20: WARRIOR L3      FLOOR 9  G 220 */
    log_set(buf, theme_futuristic ? "COMMANDO" : class_stats[player.job].name);
    log_cat(buf, " L");
    log_num(buf, player.level);
    print_padded(0, 20, buf, h, 15);
    log_set(buf, "FLOOR ");
    log_num(buf, floor_no);
    print_padded(16, 20, buf, h, 9);
    log_set(buf, "G");
    log_num(buf, player.gold);
    print_padded(25, 20, buf, YELLOW | BRIGHT, 7);
    /* row 21: HP [bar] 84   RG [bar] 70 K1 */
    print_at(0, 21, "HP", WHITE | BRIGHT);
    draw_bar(2, 21, 8, player.hp, player.hp_max, RED);
    num_str(buf, player.hp);
    print_padded(10, 21, buf, WHITE | BRIGHT, 4);
    print_at(15, 21, theme_futuristic ? "EN" : (player.job == JOB_WARRIOR ? "RG" : "MN"), WHITE | BRIGHT);
    draw_bar(17, 21, 8, player.resource, player.resource_max, theme_futuristic ? CYAN : YELLOW);
    num_str(buf, player.resource);
    print_padded(25, 21, buf, WHITE | BRIGHT, 4);
    log_set(buf, "K");
    log_num(buf, player.keys);
    print_padded(29, 21, buf, WHITE | BRIGHT, 3);
}

void draw_log(void)
{
    print_padded(0, 22, log_line, hue() | BRIGHT, 32);
    print_padded(0, 23, world_line, WHITE | BRIGHT, 32);
}

void draw_all(void)
{
    draw_viewport();
    draw_hud();
    draw_log();
}
