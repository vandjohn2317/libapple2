/* game.h -- Orbs of the Overlord on the Spectrum: the shared state.
 *
 * The rules are the Godot build's (Data.gd, Actors.gd, Dungeon.gd, Main.gd),
 * ported with the same numbers. Where a number is changed for this machine
 * it says so beside it.
 */
#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "gfx.h"
#include "sys.h"

/* ------------------------------------------------------------ the grid */
#define GRID 42
#define GRID_CELLS (GRID * GRID)
#define BM_BYTES ((GRID_CELLS + 7) / 8)
#define VW 16                       /* viewport, in tiles */
#define VH 10

typedef uint16_t tidx;              /* y * GRID + x */
#define IDX(x, y) ((tidx)((uint16_t)(y) * GRID + (x)))

/* tile codes, the same numbering as Data.gd */
#define TL_WALL 0
#define TL_FLOOR 1
#define TL_EXIT 2
#define TL_DOOR 3
#define TL_DOOR_OPEN 4
#define TL_SECRET 5
#define TL_PUZZLE 6
#define TILE(v) ((uint8_t)((v) & 0x0F))
#define VARIANT(v) ((uint8_t)((v) >> 4))

extern uint8_t grid[GRID_CELLS];
extern uint8_t dist[GRID_CELLS];     /* BFS steps from the start, 255 = unreached */
extern uint8_t revealed[BM_BYTES];   /* ever seen on this floor */
extern uint8_t seen[BM_BYTES];       /* seen this turn */
extern uint8_t start_x, start_y, exit_x, exit_y;

#define BM_GET(bm, i) ((bm)[(i) >> 3] & (1 << ((i) & 7)))
#define BM_SET(bm, i) ((bm)[(i) >> 3] |= (1 << ((i) & 7)))

static inline uint8_t tile_at(uint8_t x, uint8_t y)
{
    if (x >= GRID || y >= GRID)
        return TL_WALL;
    return TILE(grid[IDX(x, y)]);
}

static inline uint8_t is_solid(uint8_t t)
{
    return t == TL_WALL || t == TL_SECRET || t == TL_PUZZLE;
}

static inline uint8_t walkable_tile(uint8_t t)
{
    return t == TL_FLOOR || t == TL_EXIT || t == TL_DOOR_OPEN;
}

/* ------------------------------------------------------------ facing */
#define F_NORTH 0
#define F_EAST 1
#define F_SOUTH 2
#define F_WEST 3
extern const int8_t facing_dx[4];
extern const int8_t facing_dy[4];

/* ------------------------------------------------------------ the hero */
#define JOB_WARRIOR 0
#define JOB_BATTLEMAGE 1
#define JOB_PRIEST 2

typedef struct {
    uint8_t x, y, facing;
    uint8_t job;
    uint8_t level;
    uint16_t exp;
    uint16_t hp, hp_max;
    uint8_t strength, agility, thievery;
    uint8_t base_defense;
    uint16_t gold;
    uint8_t keys;
    uint8_t armor_points;
    uint8_t armor;                  /* index into armour table, 0xFF = none */
    uint8_t weapon;                 /* index into the class pool, 0xFF = bare hands */
    uint16_t resource, resource_max;
    uint8_t resource_locked;
    uint8_t defend_ac_bonus;
    uint8_t known;                  /* bitmask of the class pool */
    /* timed, in turns */
    uint8_t temp_strength, temp_defense, buff_turns;
    uint8_t speed_turns, ghost_turns, triple_shot_turns, clone_turns;
    uint8_t poison_turns, poison_value;
    uint8_t venom_turns, venom_value;
    uint8_t ghost_floors, rage_floors;
    /* class growth */
    uint8_t hp_gain, attr_gain, thievery_gain;
} Player;

extern Player player;

/* ------------------------------------------------------------ monsters */
#define TR_NONE 0
#define TR_FAST 1
#define TR_POISON 2
#define TR_THIEF 3
#define TR_BOMB 4
#define TR_MENDER 5
#define TR_ARMOURED 6

typedef struct {
    const char *name;
    uint8_t hp, defense, damage, exp;
    uint8_t ranged, view_range, ac, attack_bonus;
    uint8_t trait, trait_value;
    uint8_t sprite, ink;
} EnemyType;

#define ENEMY_TYPES 12
extern const EnemyType enemy_types[ENEMY_TYPES];

#define MAX_ENEMIES 24
typedef struct {
    uint8_t type;                   /* 0xFF = empty slot */
    uint8_t x, y;
    uint16_t hp, hp_max;
    uint8_t defense, damage;
    uint16_t exp;
    uint8_t ac, attack_bonus;
    uint8_t ranged, view_range;
    uint8_t frozen;
    uint8_t trait, trait_value;
    uint16_t carried_gold;
    uint8_t fleeing, slow_tick, flee_turns;
} Enemy;

extern Enemy enemies[MAX_ENEMIES];

/* ------------------------------------------------------------ things on the floor */
#define IT_NONE 0
#define IT_GOLD 1
#define IT_KEY 2
#define IT_CHEST 3
#define IT_CHEST_OPEN 4
#define IT_BOOK 5
#define IT_ORB 6

#define MAX_ITEMS 40
typedef struct {
    uint8_t kind, x, y;
    uint8_t a, b;                   /* gold: amount index; chest: lock, hp */
} Item;

extern Item items[MAX_ITEMS];

/* ------------------------------------------------------------ run state */
extern uint8_t floor_no;
extern uint8_t floor_kills;
extern uint8_t theme_futuristic;
extern char log_line[33];           /* what you did */
extern char world_line[33];         /* what the world did back */
extern uint16_t rng_state;

/* ------------------------------------------------------------ data.c */
typedef struct {
    const char *name;
    uint8_t hp, hp_gain, attr_gain, defense, thievery_gain, vision;
} ClassStats;
extern const ClassStats class_stats[3];
extern const char *const class_resource[3];

/* ------------------------------------------------------------ rng.c */
uint16_t rnd16(void);
uint8_t rnd8(void);
uint8_t rnd_below(uint8_t n) __z88dk_fastcall;          /* 0..n-1 */
uint8_t rnd_range(uint8_t lo, uint8_t hi);              /* lo..hi inclusive */
uint8_t rnd_chance(uint8_t pct) __z88dk_fastcall;       /* 1 with pct% odds */
uint8_t d20(void);

/* ------------------------------------------------------------ dungeon.c */
void dungeon_generate(void);
void bfs_from(uint8_t x, uint8_t y);
uint8_t random_floor_tile(uint8_t *px, uint8_t *py, uint8_t min_dist);
uint8_t walk_steps(void);
uint8_t monsters_on_floor(void);

/* ------------------------------------------------------------ vision.c */
uint8_t los_clear(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void vision_update(void);

/* ------------------------------------------------------------ actors.c */
void player_setup(uint8_t job);
uint8_t player_armor_class(void);
uint8_t player_attack_bonus(void);
uint8_t player_defense(void);
uint8_t player_effective_strength(void);
uint8_t player_swing(void);
uint8_t player_take_damage(uint8_t raw);
void player_tick_turn(void);
uint8_t player_gain_exp(uint16_t amount);
uint8_t depth_to_hit(void);
uint16_t depth_scale(uint16_t v);
uint16_t depth_damage_scale(uint16_t v);
void enemies_clear(void);
Enemy *enemy_at(uint8_t x, uint8_t y);
Enemy *spawn_enemy_at(uint8_t x, uint8_t y);
void spawn_enemies(void);
void enemy_turn(void);
void bump_attack(Enemy *e);
uint8_t combat_hit(uint8_t attack_bonus, uint8_t ac, uint8_t *crit);
uint8_t mitigate(uint8_t raw, uint8_t defense);

/* ------------------------------------------------------------ items.c */
void items_clear(void);
Item *item_at(uint8_t x, uint8_t y);
Item *item_add(uint8_t kind, uint8_t x, uint8_t y, uint8_t a, uint8_t b);
void spawn_floor_items(void);
void try_take_items(void);
void bump_chest(Item *it);

/* ------------------------------------------------------------ render.c */
void draw_viewport(void);
void draw_hud(void);
void draw_log(void);
void draw_all(void);
void log_set(char *dst, const char *s);
void log_cat(char *dst, const char *s);
void log_num(char *dst, uint16_t n);
void num_str(char *dst, uint16_t n);

/* ------------------------------------------------------------ main.c */
void new_floor(void);
uint8_t wait_key(void);

#endif
