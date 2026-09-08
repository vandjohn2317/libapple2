/* dungeon.c -- floor generation, a port of Dungeon.gd.
 *
 * Two ideas make the floors readable and both are kept:
 *   1. the random walk carries momentum, so it digs corridors, not caverns;
 *   2. doors go only on articulation points -- tiles whose removal cuts the
 *      floor in two -- so a door always means something.
 *
 * The articulation test is not Tarjan (no room for its arrays here). A
 * straight corridor tile p has two floor neighbours; on a grid their BFS
 * distances differ from p's by exactly one. If both are nearer the start,
 * p sits on a loop and is not a door. If one is further, flood from it with
 * p blocked: reaching any tile nearer than p means a way round exists; running
 * dry means p gates everything the flood touched -- which is then exactly
 * the region a door has to have something worth the walk behind it.
 */
#include <string.h>
#include "game.h"

uint8_t grid[GRID_CELLS];
uint8_t dist[GRID_CELLS];
uint8_t revealed[BM_BYTES];
uint8_t seen[BM_BYTES];
static uint16_t queue[1024];
uint8_t start_x, start_y, exit_x, exit_y;

#define MAX_DOORS 6
static uint8_t door_count;
static tidx door_at[MAX_DOORS];
static uint8_t door_x[MAX_DOORS], door_y[MAX_DOORS];
static uint16_t floor_tiles;        /* how much open ground this floor has */
/* One tile behind each door, chosen while its region is still in hand. */
uint8_t door_reward_count;
uint8_t door_reward_x[MAX_DOORS], door_reward_y[MAX_DOORS];

/* Data.gd: floors grow from 260 steps / 6 monsters to 846 / 24 by floor 10. */
#define WALK_STEPS_FIRST 260
#define WALK_STEPS_FULL 846
#define MONSTERS_FIRST 6
#define MONSTERS_FULL 24
#define FLOOR_RAMP_TO 10
#define CORRIDOR_STRAIGHTNESS 78    /* percent */
#define MIN_DOORS 3
#define MAX_DOORS_WANT 6
#define EXIT_MIN_WALK 8
#define EXIT_FAR_PERCENT 55

static uint8_t ramp_num(void)
{
    uint8_t f = floor_no > FLOOR_RAMP_TO ? FLOOR_RAMP_TO : floor_no;
    return f - 1;                   /* 0..9 over 9 */
}

uint8_t walk_steps(void)
{
    /* returned in units of 4 so it fits a byte: 65..211 */
    uint16_t steps = WALK_STEPS_FIRST + ((uint16_t)(WALK_STEPS_FULL - WALK_STEPS_FIRST) * ramp_num()) / 9;
    return (uint8_t)(steps >> 2);
}

uint8_t monsters_on_floor(void)
{
    uint16_t n = MONSTERS_FIRST * 9 + (uint16_t)(MONSTERS_FULL - MONSTERS_FIRST) * ramp_num();
    return (uint8_t)((n + 4) / 9);
}

static uint8_t passable_eventually(tidx i)
{
    uint8_t t = TILE(grid[i]);
    return t >= TL_FLOOR && t <= TL_DOOR_OPEN;
}

/* Breadth-first from (x, y) over everything a door could open onto. Fills
 * dist[]. `blocked` is treated as rock. */
static void bfs(tidx origin, tidx blocked)
{
    uint16_t head = 0, tail = 0;
    memset(dist, 255, GRID_CELLS);
    dist[origin] = 0;
    queue[tail++] = origin;
    while (head != tail) {
        tidx p = queue[head++];
        uint8_t d = dist[p];
        tidx n;
        if (d < 254)
            d++;
        n = p - GRID;
        if (n != blocked && dist[n] == 255 && passable_eventually(n)) { dist[n] = d; queue[tail++] = n; }
        n = p + GRID;
        if (n != blocked && dist[n] == 255 && passable_eventually(n)) { dist[n] = d; queue[tail++] = n; }
        n = p - 1;
        if (n != blocked && dist[n] == 255 && passable_eventually(n)) { dist[n] = d; queue[tail++] = n; }
        n = p + 1;
        if (n != blocked && dist[n] == 255 && passable_eventually(n)) { dist[n] = d; queue[tail++] = n; }
    }
}

void bfs_from(uint8_t x, uint8_t y)
{
    bfs(IDX(x, y), 0xFFFF);
}

/* Flood from `from` with `blocked` as rock. Returns 1 the moment the region
 * proves it is NOT sealed: it touches a tile nearer the start than `blocked`,
 * which means a way round exists. Returns 0 when it runs dry, and then
 * queue[0..region_len-1] IS the sealed region. The tile count is a safety
 * bound on the queue, never a design rule -- capping it at half the floor
 * threw away exactly the doors with the most behind them.
 *
 * Visited is marked in bit 7 of the tile itself and cleared from the queue
 * afterwards. A separate bitmap cost a shift, a mask and an indexed load per
 * neighbour, and this test is the whole price of building a floor: with the
 * bitmap a floor took two seconds of flood alone.
 */
static uint16_t region_len;

static uint8_t flood_escapes(tidx from, tidx blocked, uint8_t limit)
{
    uint16_t head = 0, tail = 0, cap = floor_tiles;
    uint8_t escaped = 0;
    grid[from] |= 0x80;
    queue[tail++] = from;
    while (head != tail) {
        tidx p = queue[head++];
        uint8_t k;
        if (tail > cap) {
            escaped = 1;
            break;
        }
        for (k = 0; k < 4; k++) {
            tidx n = k == 0 ? p - GRID : k == 1 ? p + GRID : k == 2 ? p - 1 : p + 1;
            uint8_t g, t;
            if (n == blocked)
                continue;
            g = grid[n];
            if (g & 0x80)
                continue;
            t = g & 0x0F;
            if (t < TL_FLOOR || t > TL_DOOR_OPEN)
                continue;
            if (dist[n] < limit) {
                escaped = 1;
                goto done;
            }
            grid[n] = g | 0x80;
            queue[tail++] = n;
        }
    }
done:
    region_len = tail;
    for (head = 0; head < tail; head++)
        grid[queue[head]] &= 0x7F;
    return escaped;
}

/* Open on exactly two opposite sides: the only shape a door frame fits. */
static uint8_t is_straight(tidx p)
{
    uint8_t w = passable_eventually(p - 1), e = passable_eventually(p + 1);
    uint8_t n = passable_eventually(p - GRID), s = passable_eventually(p + GRID);
    if (w && e && !n && !s)
        return 1;
    return n && s && !w && !e;
}

/* A random plain-floor tile of the region the last flood left in the queue,
 * or 0xFFFF. */
static tidx random_in_region(void)
{
    uint16_t count = 0, i, pick;
    for (i = 0; i < region_len; i++)
        if (TILE(grid[queue[i]]) == TL_FLOOR)
            count++;
    if (count == 0)
        return 0xFFFF;
    pick = rnd16() % count;
    for (i = 0; i < region_len; i++)
        if (TILE(grid[queue[i]]) == TL_FLOOR) {
            if (pick == 0)
                return queue[i];
            pick--;
        }
    return 0xFFFF;
}

#define MAX_CAND 128

/* How many candidates may be flood-tested before the generator settles for
 * the doors it has. Each test is the expensive part of building a floor. */
#define FLOOD_BUDGET 96

static void place_doors(void)
{
    static tidx cand[MAX_CAND];
    static uint8_t cand_x[MAX_CAND], cand_y[MAX_CAND];
    uint8_t ncand = 0, want, pass, i, j;
    tidx p;
    door_count = 0;
    door_reward_count = 0;

    /* Candidates: straight floor tiles past the safe zone.
     *
     * A big floor has more of these than the table holds, and taking the
     * first hundred and twenty-eight in scan order would put every door in
     * the north of the map. They are reservoir-sampled instead, so the ones
     * kept are an even draw from the whole floor. */
    {
        uint8_t x, y;
        uint16_t nseen = 0;
        for (y = 1; y < GRID - 1; y++) {
            p = IDX(1, y);
            for (x = 1; x < GRID - 1; x++, p++) {
                uint8_t slot;
                if (TILE(grid[p]) != TL_FLOOR || dist[p] == 255 || dist[p] <= 2)
                    continue;
                if (!is_straight(p))
                    continue;
                nseen++;
                if (ncand < MAX_CAND) {
                    slot = ncand++;
                } else {
                    uint16_t r = rnd16() % nseen;
                    if (r >= MAX_CAND)
                        continue;
                    slot = (uint8_t)r;
                }
                cand_x[slot] = x;
                cand_y[slot] = y;
                cand[slot] = p;
            }
        }
    }
    /* nearest first */
    for (i = 1; i < ncand; i++) {
        tidx v = cand[i];
        uint8_t vx = cand_x[i], vy = cand_y[i];
        j = i;
        while (j > 0 && dist[cand[j - 1]] > dist[v]) {
            cand[j] = cand[j - 1];
            cand_x[j] = cand_x[j - 1];
            cand_y[j] = cand_y[j - 1];
            j--;
        }
        cand[j] = v;
        cand_x[j] = vx;
        cand_y[j] = vy;
    }

    want = rnd_range(MIN_DOORS, MAX_DOORS_WANT);
    for (pass = 0; pass < 3; pass++) {
        uint8_t gap = pass == 0 ? 5 : pass == 1 ? 2 : 1;
        uint8_t near = pass == 0 ? 12 : pass == 1 ? 6 : 2;
        uint8_t budget = FLOOD_BUDGET;
        if (pass > 0 && door_count >= MIN_DOORS)
            break;
        for (i = 0; i < ncand && door_count < want && budget; i++) {
            uint8_t d, too_close = 0, px, py;
            tidx far;
            p = cand[i];
            d = dist[p];
            if (d <= near || TILE(grid[p]) == TL_DOOR)
                continue;
            px = cand_x[i];
            py = cand_y[i];
            for (j = 0; j < door_count; j++) {
                uint8_t qx = door_x[j], qy = door_y[j];
                uint8_t dx = px > qx ? px - qx : qx - px;
                uint8_t dy = py > qy ? py - qy : qy - py;
                if (dx + dy < gap) {
                    too_close = 1;
                    break;
                }
            }
            if (too_close)
                continue;
            /* the far neighbour, if there is one */
            far = 0xFFFF;
            if (passable_eventually(p - 1) && dist[p - 1] > d) far = p - 1;
            else if (passable_eventually(p + 1) && dist[p + 1] > d) far = p + 1;
            else if (passable_eventually(p - GRID) && dist[p - GRID] > d) far = p - GRID;
            else if (passable_eventually(p + GRID) && dist[p + GRID] > d) far = p + GRID;
            if (far == 0xFFFF)
                continue;           /* both sides nearer: a loop */
            budget--;
            if (flood_escapes(far, p, d))
                continue;           /* a way round: not a door */
            door_x[door_count] = px;
            door_y[door_count] = py;
            door_at[door_count++] = p;
            grid[p] = TL_DOOR | (rnd_below(3) << 4);
            {
                tidx r = random_in_region();
                if (r != 0xFFFF) {
                    uint8_t ry = 0;
                    while (r >= GRID) {
                        r -= GRID;
                        ry++;
                    }
                    door_reward_x[door_reward_count] = (uint8_t)r;
                    door_reward_y[door_reward_count] = ry;
                    door_reward_count++;
                }
            }
        }
    }
}

static void place_exit(void)
{
    uint8_t furthest = 0, far;
    uint16_t count = 0, pick;
    tidx p, fallback = IDX(start_x, start_y);
    for (p = 0; p < GRID_CELLS; p++) {
        if (dist[p] != 255 && dist[p] > furthest && TILE(grid[p]) == TL_FLOOR) {
            furthest = dist[p];
            fallback = p;
        }
    }
    far = (uint8_t)(((uint16_t)furthest * EXIT_FAR_PERCENT) / 100);
    if (far < EXIT_MIN_WALK)
        far = EXIT_MIN_WALK;
    for (p = 0; p < GRID_CELLS; p++)
        if (dist[p] != 255 && dist[p] >= far && TILE(grid[p]) == TL_FLOOR)
            count++;
    if (count == 0) {
        p = fallback;
    } else {
        pick = rnd16() % count;
        for (p = 0; p < GRID_CELLS; p++)
            if (dist[p] != 255 && dist[p] >= far && TILE(grid[p]) == TL_FLOOR) {
                if (pick == 0)
                    break;
                pick--;
            }
    }
    grid[p] = TL_EXIT;
    exit_y = 0;
    while (p >= GRID) {
        p -= GRID;
        exit_y++;
    }
    exit_x = (uint8_t)p;
}

void dungeon_generate(void)
{
    uint8_t steps4 = walk_steps();
    uint8_t x = GRID / 2, y = GRID / 2, k;
    uint8_t last_dx = 0, last_dy = 0, have_last = 0;

    memset(grid, TL_WALL, GRID_CELLS);
    memset(revealed, 0, BM_BYTES);
    memset(seen, 0, BM_BYTES);

    /* the walk, with momentum */
    while (steps4--) {
        for (k = 0; k < 4; k++) {
            int8_t mx = 0, my = 0;
            uint8_t picked = 0;
            /* the floor's stone varies by 6x6 patch */
            uint8_t patch = ((x / 6) + (y / 6) * 3 + (uint8_t)rng_state) & 1;
            grid[IDX(x, y)] = TL_FLOOR | (patch << 4);
            if (have_last && rnd_chance(CORRIDOR_STRAIGHTNESS)) {
                uint8_t nx = x + last_dx, ny = y + last_dy;
                if (nx >= 1 && nx <= GRID - 2 && ny >= 1 && ny <= GRID - 2) {
                    mx = last_dx;
                    my = last_dy;
                    picked = 1;
                }
            }
            if (!picked) {
                uint8_t r = rnd_below(4);
                mx = facing_dx[r];
                my = facing_dy[r];
            }
            last_dx = mx;
            last_dy = my;
            have_last = 1;
            x += mx;
            y += my;
            if (x < 1) x = 1;
            if (x > GRID - 2) x = GRID - 2;
            if (y < 1) y = 1;
            if (y > GRID - 2) y = GRID - 2;
        }
    }
    start_x = GRID / 2;
    start_y = GRID / 2;
    floor_tiles = 0;
    {
        tidx i;
        for (i = 0; i < GRID_CELLS; i++)
            if (TILE(grid[i]) == TL_FLOOR)
                floor_tiles++;
    }

    bfs(IDX(start_x, start_y), 0xFFFF);
    place_doors();
    place_exit();
    /* dist[] is left as the walk from the start, which the spawners use */
}

/* A random plain floor tile at least min_dist steps from the start, with
 * nothing standing on it. 0 if none could be found. */
uint8_t random_floor_tile(uint8_t *px, uint8_t *py, uint8_t min_dist)
{
    uint8_t tries;
    for (tries = 0; tries < 200; tries++) {
        uint8_t x = 1 + rnd_below(GRID - 2), y = 1 + rnd_below(GRID - 2);
        tidx i = IDX(x, y);
        if (TILE(grid[i]) != TL_FLOOR || dist[i] == 255 || dist[i] < min_dist)
            continue;
        if (enemy_at(x, y) || item_at(x, y))
            continue;
        if (x == player.x && y == player.y)
            continue;
        *px = x;
        *py = y;
        return 1;
    }
    return 0;
}
