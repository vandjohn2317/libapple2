/* harness.c -- runs the game's own rules on the host and reports on them.
 *
 * The point is that these are THE rules, not a model of them: dungeon.c,
 * actors.c, items.c, rng.c and vision.c are compiled straight from src/.
 * Only the screen and the keyboard are stubbed out.
 *
 *   ./harness floors 400      generate 400 floors, report the shape of them
 *   ./harness combat 20000    swing at the roster, report hit rates
 *   ./harness levels          walk the experience curve
 *   ./harness vision          the cone, measured
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

/* the bits of main.c the rules reach for */
uint8_t floor_no = 1;
uint8_t floor_kills;
uint8_t theme_futuristic;
char log_line[33];
char world_line[33];

void log_set(char *dst, const char *s) { strncpy(dst, s, 32); dst[32] = 0; }
void log_cat(char *dst, const char *s)
{
    size_t n = strlen(dst);
    strncpy(dst + n, s, 32 - n);
    dst[32] = 0;
}
void log_num(char *dst, uint16_t n)
{
    char b[8];
    sprintf(b, "%u", n);
    log_cat(dst, b);
}

static uint16_t reachable_from_start(void)
{
    uint16_t n = 0, i;
    bfs_from(start_x, start_y);
    for (i = 0; i < GRID_CELLS; i++)
        if (dist[i] != 255)
            n++;
    return n;
}

static int cmd_floors(int count)
{
    int f, bad = 0;
    long doors_total = 0, tiles_total = 0, exit_total = 0;
    unsigned min_tiles = 65535, max_tiles = 0;
    unsigned no_exit = 0, unreachable_exit = 0, short_walk = 0, marks = 0;
    int door_hist[8] = {0};
    for (f = 0; f < count; f++) {
        uint16_t i, tiles = 0, doors = 0;
        floor_no = 1 + (f % 30);
        rng_state = 1 + f * 7919;
        player_setup(JOB_WARRIOR);
        enemies_clear();
        items_clear();
        dungeon_generate();
        player.x = start_x;
        player.y = start_y;
        for (i = 0; i < GRID_CELLS; i++) {
            uint8_t t = TILE(grid[i]);
            if (grid[i] & 0x80)
                marks++;
            if (t == TL_FLOOR)
                tiles++;
            else if (t == TL_DOOR)
                doors++;
        }
        reachable_from_start();
        if (TILE(grid[IDX(exit_x, exit_y)]) != TL_EXIT)
            no_exit++;
        else if (dist[IDX(exit_x, exit_y)] == 255)
            unreachable_exit++;
        else if (dist[IDX(exit_x, exit_y)] < 8)
            short_walk++;
        door_hist[doors > 7 ? 7 : doors]++;
        doors_total += doors;
        tiles_total += tiles;
        exit_total += dist[IDX(exit_x, exit_y)] == 255 ? 0 : dist[IDX(exit_x, exit_y)];
        if (tiles < min_tiles) min_tiles = tiles;
        if (tiles > max_tiles) max_tiles = tiles;
    }
    for (f = 0; f < 8; f++)
        printf("floors with %d door%s  %d\n", f, f == 1 ? " " : "s", door_hist[f]);
    printf("floors            %d\n", count);
    printf("open tiles        %ld avg, %u min, %u max\n", tiles_total / count, min_tiles, max_tiles);
    printf("doors per floor   %.2f\n", (double)doors_total / count);
    printf("walk to the exit  %ld steps avg\n", exit_total / count);
    printf("no exit placed    %u\n", no_exit);
    printf("exit unreachable  %u\n", unreachable_exit);
    printf("exit too close    %u\n", short_walk);
    printf("flood marks left  %u\n", marks);
    if (no_exit || unreachable_exit || marks) {
        printf("FAIL\n");
        bad = 1;
    }
    return bad;
}

static int cmd_combat(int rounds)
{
    int i, t;
    printf("%-16s %6s %6s %6s %6s\n", "monster", "floor", "hit%", "swings", "itshit%");
    for (t = 0; t < ENEMY_TYPES; t += 3) {
        int fl;
        for (fl = 1; fl <= 25; fl += 12) {
            long hits = 0, tries = 0, swings = 0, kills = 0, theirs = 0, their_tries = 0;
            floor_no = fl;
            player_setup(JOB_WARRIOR);
            player.level = fl;
            player.strength = 10 + 3 * (fl - 1);
            player.agility = 10 + 3 * (fl - 1);
            for (i = 0; i < rounds / 10; i++) {
                Enemy e;
                uint8_t crit;
                memset(&e, 0, sizeof(e));
                e.type = t;
                e.hp = depth_scale(enemy_types[t].hp);
                e.hp_max = e.hp;
                e.defense = enemy_types[t].defense;
                e.ac = enemy_types[t].ac;
                e.attack_bonus = enemy_types[t].attack_bonus + depth_to_hit();
                e.damage = depth_damage_scale(enemy_types[t].damage);
                while (e.hp) {
                    tries++;
                    if (combat_hit(player_attack_bonus(), e.ac, &crit)) {
                        uint8_t raw = player_swing();
                        uint8_t dealt;
                        if (crit)
                            raw = raw > 127 ? 255 : raw * 2;
                        dealt = mitigate(raw, e.defense);
                        hits++;
                        if (dealt >= e.hp)
                            e.hp = 0;
                        else
                            e.hp -= dealt;
                    }
                    swings++;
                    if (swings > 200000)
                        break;
                }
                kills++;
                their_tries += 20;
                {
                    int k;
                    for (k = 0; k < 20; k++)
                        if (combat_hit(e.attack_bonus, player_armor_class(), &crit))
                            theirs++;
                }
            }
            printf("%-16s %6d %5ld%% %6.1f %6ld%%\n", enemy_types[t].name, fl,
                   tries ? hits * 100 / tries : 0,
                   kills ? (double)swings / kills : 0.0,
                   their_tries ? theirs * 100 / their_tries : 0);
        }
    }
    return 0;
}

static int cmd_levels(void)
{
    int f;
    printf("%6s %6s %6s %6s %6s\n", "floor", "level", "hp", "swing", "ac");
    player_setup(JOB_WARRIOR);
    for (f = 1; f <= 40; f++) {
        int i;
        floor_no = f;
        /* one floor's worth of kills: the monsters a floor of this depth holds */
        for (i = 0; i < monsters_on_floor(); i++) {
            uint8_t t = rnd_below(2 + f > ENEMY_TYPES ? ENEMY_TYPES : 2 + f);
            player_gain_exp(depth_scale(enemy_types[t].exp));
        }
        if (f % 5 == 0 || f == 1) {
            long sum = 0;
            for (i = 0; i < 1000; i++)
                sum += player_swing();
            printf("%6d %6d %6d %6.1f %6d\n", f, player.level, player.hp_max,
                   sum / 1000.0, player_armor_class());
        }
    }
    return 0;
}

static int cmd_vision(void)
{
    int f, seen_count = 0;
    floor_no = 5;
    rng_state = 12345;
    player_setup(JOB_WARRIOR);
    enemies_clear();
    items_clear();
    dungeon_generate();
    player.x = start_x;
    player.y = start_y;
    for (f = 0; f < 4; f++) {
        uint16_t i, n = 0;
        player.facing = f;
        vision_update();
        for (i = 0; i < GRID_CELLS; i++)
            if (BM_GET(seen, i))
                n++;
        printf("facing %d: %u tiles in view\n", f, n);
        seen_count += n;
    }
    printf("revealed after four looks: %u\n", (unsigned)({
        uint16_t i, n = 0;
        for (i = 0; i < GRID_CELLS; i++) if (BM_GET(revealed, i)) n++;
        n; }));
    return seen_count == 0;
}

int main(int argc, char **argv)
{
    const char *cmd = argc > 1 ? argv[1] : "floors";
    int n = argc > 2 ? atoi(argv[2]) : 200;
    if (!strcmp(cmd, "floors"))
        return cmd_floors(n);
    if (!strcmp(cmd, "combat"))
        return cmd_combat(n ? n : 20000);
    if (!strcmp(cmd, "levels"))
        return cmd_levels();
    if (!strcmp(cmd, "vision"))
        return cmd_vision();
    fprintf(stderr, "unknown command %s\n", cmd);
    return 2;
}
