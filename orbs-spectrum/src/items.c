/* items.c -- gold, keys and chests lying on the floor.
 *
 * A chest can be opened or it can be broken, and those are not the same
 * thing: breaking one destroys what is inside 93 times out of 100. That
 * single number is what gives keys and Thievery their whole purpose.
 */
#include <string.h>
#include "game.h"

Item items[MAX_ITEMS];

/* Data.gd */
#define GOLD_PILES_PER_FLOOR 4
#define KEYS_MIN 2
#define KEYS_MAX 4
#define CHESTS_MIN 2
#define CHESTS_MAX 4
#define CHEST_BASE_LOCK 30
#define CHEST_LOCK_STEP 5
#define CHEST_MAX_LOCK 100
#define CHEST_BASE_HP 40
#define CHEST_HP_PER_LEVEL 12
#define CHEST_SMASH_TREASURE_PERCENT 7
#define THIEVERY_MAX_PERCENT 35

static const uint16_t gold_amounts[7] = {10, 100, 150, 200, 400, 500, 1000};

void items_clear(void)
{
    memset(items, 0, sizeof(items));
}

Item *item_at(uint8_t x, uint8_t y)
{
    uint8_t i;
    for (i = 0; i < MAX_ITEMS; i++)
        if (items[i].kind && items[i].x == x && items[i].y == y)
            return &items[i];
    return 0;
}

Item *item_add(uint8_t kind, uint8_t x, uint8_t y, uint8_t a, uint8_t b)
{
    uint8_t i;
    for (i = 0; i < MAX_ITEMS; i++)
        if (items[i].kind == IT_NONE) {
            items[i].kind = kind;
            items[i].x = x;
            items[i].y = y;
            items[i].a = a;
            items[i].b = b;
            return &items[i];
        }
    return 0;
}

static uint8_t chest_lock(void)
{
    uint16_t lock = CHEST_BASE_LOCK + (uint16_t)(floor_no - 1) * CHEST_LOCK_STEP;
    return lock > CHEST_MAX_LOCK ? CHEST_MAX_LOCK : (uint8_t)lock;
}

static uint8_t chest_hp(void)
{
    uint16_t hp = CHEST_BASE_HP + (uint16_t)(player.level - 1) * CHEST_HP_PER_LEVEL;
    return hp > 255 ? 255 : (uint8_t)hp;
}

void spawn_floor_items(void)
{
    uint8_t n, x, y;
    items_clear();
    for (n = GOLD_PILES_PER_FLOOR; n; n--)
        if (random_floor_tile(&x, &y, 2))
            item_add(IT_GOLD, x, y, rnd_below(7), 0);
    for (n = rnd_range(KEYS_MIN, KEYS_MAX); n; n--)
        if (random_floor_tile(&x, &y, 2))
            item_add(IT_KEY, x, y, 0, 0);
    for (n = rnd_range(CHESTS_MIN, CHESTS_MAX); n; n--)
        if (random_floor_tile(&x, &y, 3))
            item_add(IT_CHEST, x, y, chest_lock(), chest_hp());
}

static void take_gold(uint16_t amount)
{
    player.gold += amount;
    log_set(log_line, "+");
    log_num(log_line, amount);
    log_cat(log_line, " GOLD");
}

/* Whatever lies on the hero's tile is picked up. */
void try_take_items(void)
{
    Item *it = item_at(player.x, player.y);
    if (!it)
        return;
    switch (it->kind) {
    case IT_GOLD:
        take_gold(gold_amounts[it->a]);
        it->kind = IT_NONE;
        break;
    case IT_KEY:
        player.keys++;
        log_set(log_line, "A KEY  (");
        log_num(log_line, player.keys);
        log_cat(log_line, ")");
        it->kind = IT_NONE;
        break;
    default:
        break;
    }
}

/* What a chest pays out: a pile of gold next to it (gear comes later). */
static void spawn_treasure_near(Item *chest)
{
    int8_t dx, dy;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++) {
            uint8_t nx = chest->x + dx, ny = chest->y + dy;
            if ((dx || dy) && tile_at(nx, ny) == TL_FLOOR && !item_at(nx, ny) && !enemy_at(nx, ny)
                && !(nx == player.x && ny == player.y)) {
                item_add(IT_GOLD, nx, ny, 1 + rnd_below(6), 0);
                return;
            }
        }
}

/* Chests.gd: a key opens it; without one you try the lock, and Thievery
 * against the lock is never certain; failing that, you hit it. */
void bump_chest(Item *it)
{
    uint8_t chance;
    /* An opened chest is GONE, not a wreck standing in the doorway. It used
     * to become IT_CHEST_OPEN and stay where it was -- and since walking into
     * a chest bumps it rather than stepping onto it, a chest smashed in a
     * one-tile corridor walled the corridor off for the rest of the floor.
     * Reported with the stairs on the far side of one. */
    if (player.keys) {
        player.keys--;
        spawn_treasure_near(it);
        it->kind = IT_NONE;
        log_set(log_line, "UNLOCKED WITH A KEY");
        return;
    }
    chance = (uint8_t)(((uint16_t)player.thievery * 100) / it->a);
    if (chance > THIEVERY_MAX_PERCENT)
        chance = THIEVERY_MAX_PERCENT;
    if (rnd_chance(chance)) {
        spawn_treasure_near(it);
        it->kind = IT_NONE;
        log_set(log_line, "THE LOCK GIVES");
        return;
    }
    /* the blunt approach */
    {
        uint8_t blow = player_swing();
        if (blow >= it->b) {
            if (rnd_chance(CHEST_SMASH_TREASURE_PERCENT)) {
                spawn_treasure_near(it);
                log_set(log_line, "SMASHED -- SOMETHING SURVIVED");
            } else {
                log_set(log_line, "SMASHED -- NOTHING SURVIVED");
            }
            it->kind = IT_NONE;
        } else {
            it->b -= blow;
            log_set(log_line, "THE LOCK HOLDS. YOU HIT IT ");
            log_num(log_line, blow);
        }
    }
}
