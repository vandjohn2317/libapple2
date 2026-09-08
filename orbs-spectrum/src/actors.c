/* actors.c -- the hero, the monsters, and the d20 between them.
 *
 * Every attack is d20 + bonus against d20 + (armour class - 10); a natural
 * 20 always lands and a natural 1 always misses. Armour takes a quarter of
 * the defence pool off a blow, but never more than 60% of the blow.
 * Depth is expressed in hit points and damage, never in armour class.
 */
#include <string.h>
#include "game.h"

Player player;
Enemy enemies[MAX_ENEMIES];

/* Data.gd */
#define PLAYER_BASE_AC 10
#define PLAYER_AC_AGILITY_DIVISOR 4
#define PLAYER_AC_AGILITY_MAX 8
#define PLAYER_MIN_AC 12
#define PLAYER_BASE_ATTACK_BONUS 2
#define PLAYER_ATTACK_BONUS_STR_DIVISOR 5
#define PLAYER_ATTACK_BONUS_LEVEL_DIVISOR 2
#define PLAYER_ATTACK_BONUS_MAX 10
#define MITIGATION_MAX_PERCENT 60
#define STRENGTH_TO_DAMAGE 2
#define BARE_HANDS_MIN 2
#define BARE_HANDS_MAX 5
#define XP_BASE 100
#define XP_PER_LEVEL 100
#define LEVEL_DEFENSE_GAIN 2
#define RESOURCE_BAR_GROWTH 3
#define BASE_MAX_RESOURCE 100
#define BASE_THIEVERY 10
#define DEPTH_HP_PER_FLOOR 9        /* percent */
#define DEPTH_DAMAGE_PER_FLOOR 11
#define DEPTH_TO_HIT_EVERY_FLOORS 2
#define DEPTH_TO_HIT_MAX 6
#define GOLD_PER_KILL_DIVISOR 2
#define ENEMY_RANGED_HIT_CHANCE 55
#define MAX_RANGED_ATTACKERS_PER_TURN 1
#define THIEF_ESCAPE_RANGE 9
#define THIEF_ESCAPE_TURNS 8
#define MEND_RANGE 2
#define BLAST_RADIUS 1
#define RESOURCE_PER_KILL_RATIO 55  /* percent of the kill's intrinsic exp */
#define RESOURCE_PER_KILL_MIN 4
#define RESOURCE_PER_KILL_MAX_PERCENT 20

/* ------------------------------------------------------------ dice */
uint8_t combat_hit(uint8_t attack_bonus, uint8_t ac, uint8_t *crit)
{
    uint8_t a = d20(), d = d20();
    uint8_t hit = (a + attack_bonus) >= (d + ac - 10);
    *crit = 0;
    if (a == 20) {
        hit = 1;
        *crit = 1;
    } else if (a == 1) {
        hit = 0;
    }
    return hit;
}

uint8_t mitigate(uint8_t raw, uint8_t defense)
{
    uint8_t off = defense / 4;
    uint8_t cap = (uint8_t)(((uint16_t)raw * MITIGATION_MAX_PERCENT) / 100);
    if (off > cap)
        off = cap;
    if (raw - off < 1)
        return 1;
    return raw - off;
}

/* ------------------------------------------------------------ depth */
uint8_t depth_to_hit(void)
{
    uint8_t d = (floor_no - 1) / DEPTH_TO_HIT_EVERY_FLOORS;
    return d > DEPTH_TO_HIT_MAX ? DEPTH_TO_HIT_MAX : d;
}

uint16_t depth_scale(uint16_t v)
{
    uint16_t pct = 100 + (uint16_t)(floor_no - 1) * DEPTH_HP_PER_FLOOR;
    uint16_t r = (v * pct + 50) / 100;
    return r < 1 ? 1 : r;
}

uint16_t depth_damage_scale(uint16_t v)
{
    uint16_t pct = 100 + (uint16_t)(floor_no - 1) * DEPTH_DAMAGE_PER_FLOOR;
    uint16_t r = (v * pct + 50) / 100;
    return r < 1 ? 1 : r;
}

/* ------------------------------------------------------------ the hero */
void player_setup(uint8_t job)
{
    const ClassStats *st = &class_stats[job];
    memset(&player, 0, sizeof(player));
    player.job = job;
    player.level = 1;
    player.hp_max = st->hp;
    player.hp = st->hp;
    player.strength = 10;
    player.agility = 10;
    player.thievery = BASE_THIEVERY;
    player.base_defense = st->defense;
    player.hp_gain = st->hp_gain;
    player.attr_gain = st->attr_gain;
    player.thievery_gain = st->thievery_gain;
    player.resource_max = BASE_MAX_RESOURCE;
    player.resource = BASE_MAX_RESOURCE;
    player.armor = 0xFF;
    player.weapon = 0xFF;
    player.facing = F_SOUTH;
}

uint8_t player_effective_strength(void)
{
    uint8_t s = player.strength + player.temp_strength;
    if (player.rage_floors)
        s += 30;
    return s;
}

uint8_t player_armor_class(void)
{
    uint8_t agi = player.agility / PLAYER_AC_AGILITY_DIVISOR;
    uint8_t ac;
    if (agi > PLAYER_AC_AGILITY_MAX)
        agi = PLAYER_AC_AGILITY_MAX;
    ac = PLAYER_BASE_AC + agi;
    /* armour adds its ac once there is any of it left -- items.c later */
    if (ac < PLAYER_MIN_AC)
        ac = PLAYER_MIN_AC;
    return ac + player.defend_ac_bonus;
}

uint8_t player_attack_bonus(void)
{
    uint8_t b = PLAYER_BASE_ATTACK_BONUS + player_effective_strength() / PLAYER_ATTACK_BONUS_STR_DIVISOR
                + player.level / PLAYER_ATTACK_BONUS_LEVEL_DIVISOR;
    return b > PLAYER_ATTACK_BONUS_MAX ? PLAYER_ATTACK_BONUS_MAX : b;
}

uint8_t player_defense(void)
{
    return player.base_defense + player.temp_defense;
}

/* One blow, before crits and everything else the caller stacks on. */
uint8_t player_swing(void)
{
    uint8_t dice = rnd_range(BARE_HANDS_MIN, BARE_HANDS_MAX);
    uint8_t v = dice + player_effective_strength() / STRENGTH_TO_DAMAGE;
    return v < 1 ? 1 : v;
}

uint8_t player_take_damage(uint8_t raw)
{
    uint8_t dmg = mitigate(raw, player_defense());
    if (player.armor_points > 0) {
        uint8_t soak = player.armor_points < dmg ? player.armor_points : dmg;
        player.armor_points -= soak;
        dmg -= soak;
    }
    if (dmg >= player.hp)
        player.hp = 0;
    else
        player.hp -= dmg;
    return dmg;
}

void player_tick_turn(void)
{
    if (player.buff_turns) {
        player.buff_turns--;
        if (player.buff_turns == 0) {
            player.temp_strength = 0;
            player.temp_defense = 0;
        }
    }
    if (player.speed_turns) player.speed_turns--;
    if (player.ghost_turns) player.ghost_turns--;
    if (player.triple_shot_turns) player.triple_shot_turns--;
    if (player.clone_turns) player.clone_turns--;
    if (player.poison_turns) player.poison_turns--;
    if (player.venom_turns) {
        player.venom_turns--;
        if (player.venom_turns == 0)
            player.venom_value = 0;
    }
}

static uint16_t xp_to_next_level(void)
{
    return XP_BASE + (uint16_t)(player.level - 1) * XP_PER_LEVEL;
}

/* Several levels can land at once and the remainder is kept. Returns the
 * number gained; a level heals to full. */
uint8_t player_gain_exp(uint16_t amount)
{
    uint8_t gained = 0;
    player.exp += amount;
    while (player.exp >= xp_to_next_level()) {
        player.exp -= xp_to_next_level();
        player.level++;
        gained++;
        player.hp_max += player.hp_gain;
        player.strength += player.attr_gain;
        player.agility += player.attr_gain;
        player.base_defense += LEVEL_DEFENSE_GAIN;
        player.thievery += player.thievery_gain;
        if (!player.resource_locked)
            player.resource_max += RESOURCE_BAR_GROWTH;
    }
    if (gained) {
        player.hp = player.hp_max;
        if (!player.resource_locked)
            player.resource = player.resource_max;
    }
    return gained;
}

/* ------------------------------------------------------------ monsters */
void enemies_clear(void)
{
    uint8_t i;
    for (i = 0; i < MAX_ENEMIES; i++)
        enemies[i].type = 0xFF;
}

Enemy *enemy_at(uint8_t x, uint8_t y)
{
    uint8_t i;
    for (i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].type != 0xFF && enemies[i].hp && enemies[i].x == x && enemies[i].y == y)
            return &enemies[i];
    return 0;
}

Enemy *spawn_enemy_at(uint8_t x, uint8_t y)
{
    uint8_t i, span;
    const EnemyType *t;
    Enemy *e = 0;
    for (i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].type == 0xFF || enemies[i].hp == 0) {
            e = &enemies[i];
            break;
        }
    if (!e)
        return 0;
    /* deeper floors weight the roster toward the nastier end */
    span = 2 + floor_no;
    if (span > ENEMY_TYPES)
        span = ENEMY_TYPES;
    i = rnd_below(span);
    t = &enemy_types[i];
    memset(e, 0, sizeof(*e));
    e->type = i;
    e->x = x;
    e->y = y;
    e->hp = depth_scale(t->hp);
    e->hp_max = e->hp;
    e->defense = t->defense;
    e->damage = (uint8_t)depth_damage_scale(t->damage);
    e->exp = depth_scale(t->exp);
    e->ac = t->ac;
    e->attack_bonus = t->attack_bonus + depth_to_hit();
    e->ranged = t->ranged;
    e->view_range = t->view_range;
    e->trait = t->trait;
    e->trait_value = t->trait_value;
    return e;
}

void spawn_enemies(void)
{
    uint8_t n = monsters_on_floor(), x, y;
    enemies_clear();
    while (n--) {
        if (!random_floor_tile(&x, &y, 7))
            break;
        spawn_enemy_at(x, y);
    }
}

static const char *enemy_name(const Enemy *e)
{
    return enemy_types[e->type].name;
}

/* ------------------------------------------------------------ rewards */
static uint16_t resource_for_kill(uint16_t enemy_exp)
{
    /* the monster's intrinsic worth, with the depth taken back out */
    uint16_t pct = 100 + (uint16_t)(floor_no - 1) * DEPTH_HP_PER_FLOOR;
    uint16_t intrinsic = (enemy_exp * 100) / pct;
    uint16_t back = (intrinsic * RESOURCE_PER_KILL_RATIO + 50) / 100;
    uint16_t cap = (player.resource_max * RESOURCE_PER_KILL_MAX_PERCENT) / 100;
    if (cap < RESOURCE_PER_KILL_MIN)
        cap = RESOURCE_PER_KILL_MIN;
    if (back < RESOURCE_PER_KILL_MIN)
        back = RESOURCE_PER_KILL_MIN;
    return back > cap ? cap : back;
}

static void detonate(Enemy *e);

static void reward_kill(Enemy *e)
{
    uint16_t coin = e->exp / GOLD_PER_KILL_DIVISOR;
    uint8_t levels;
    if (e->hp)
        return;
    if (e->carried_gold) {
        coin += e->carried_gold;
        e->carried_gold = 0;
    }
    levels = player_gain_exp(e->exp);
    player.gold += coin;
    if (!player.resource_locked && player.resource < player.resource_max) {
        uint16_t back = resource_for_kill(e->exp);
        player.resource += back;
        if (player.resource > player.resource_max)
            player.resource = player.resource_max;
    }
    floor_kills++;
    log_cat(log_line, " ");
    log_cat(log_line, enemy_name(e));
    log_cat(log_line, " DOWN +");
    log_num(log_line, e->exp);
    if (levels) {
        log_set(world_line, "LEVEL ");
        log_num(world_line, player.level);
        log_cat(world_line, " -- HEALED TO FULL");
    }
    /* killing one does not defuse it */
    if (e->trait == TR_BOMB)
        detonate(e);
}

/* ------------------------------------------------------------ the hero swings */
void bump_attack(Enemy *e)
{
    uint8_t crit, raw, dealt;
    if (!combat_hit(player_attack_bonus(), e->ac, &crit)) {
        log_set(log_line, "MISS");
        return;
    }
    raw = player_swing();
    if (crit)
        raw = raw > 127 ? 255 : raw * 2;
    if (player.triple_shot_turns)
        raw = raw > 85 ? 255 : raw * 3;
    dealt = mitigate(raw, e->defense);
    if (dealt >= e->hp)
        e->hp = 0;
    else
        e->hp -= dealt;
    log_set(log_line, crit ? "CRIT " : "HIT ");
    log_num(log_line, dealt);
    reward_kill(e);
}

/* ------------------------------------------------------------ the monsters' turn */
static void enemy_attack(Enemy *e, uint8_t from_afar)
{
    uint8_t crit, dealt;
    (void)from_afar;
    if (!combat_hit(e->attack_bonus, player_armor_class(), &crit))
        return;
    dealt = player_take_damage(e->damage + rnd_range(1, 4));
    log_set(world_line, enemy_name(e));
    log_cat(world_line, " HITS YOU FOR ");
    log_num(world_line, dealt);
    /* what the blow leaves behind */
    if (e->trait == TR_POISON) {
        uint8_t v = e->damage / 2;
        if (player.venom_turns < e->trait_value)
            player.venom_turns = e->trait_value;
        if (v < 1)
            v = 1;
        if (player.venom_value < v)
            player.venom_value = v;
        log_cat(world_line, " VENOM");
    } else if (e->trait == TR_THIEF) {
        uint16_t taken = player.gold < e->trait_value ? player.gold : e->trait_value;
        if (taken) {
            player.gold -= taken;
            e->carried_gold += taken;
            log_set(world_line, "IT TAKES ");
            log_num(world_line, taken);
            log_cat(world_line, " GOLD AND RUNS");
        } else {
            log_set(world_line, "YOUR PURSE IS EMPTY");
        }
        e->fleeing = 1;
    }
}

static uint8_t try_step(Enemy *e, int8_t dx, int8_t dy)
{
    uint8_t nx = e->x + dx, ny = e->y + dy;
    if (dx == 0 && dy == 0)
        return 0;
    if (!walkable_tile(tile_at(nx, ny)))
        return 0;
    if (nx == player.x && ny == player.y)
        return 0;
    if (enemy_at(nx, ny))
        return 0;
    e->x = nx;
    e->y = ny;
    return 1;
}

static int8_t sgn(int8_t v)
{
    return v > 0 ? 1 : v < 0 ? -1 : 0;
}

static uint8_t absi(int8_t v)
{
    return v < 0 ? -v : v;
}

/* Greedy on the longer axis, then the other -- enough to feel purposeful
 * without a pathfinder. */
static void enemy_advance(Enemy *e, int8_t tx, int8_t ty)
{
    if (absi(tx) > absi(ty)) {
        if (!try_step(e, sgn(tx), 0))
            try_step(e, 0, sgn(ty));
    } else {
        if (!try_step(e, 0, sgn(ty)))
            try_step(e, sgn(tx), 0);
    }
}

static void enemy_flee(Enemy *e, int8_t tx, int8_t ty)
{
    uint8_t gap;
    enemy_advance(e, -tx, -ty);
    e->flee_turns++;
    tx = player.x - e->x;
    ty = player.y - e->y;
    gap = absi(tx) > absi(ty) ? absi(tx) : absi(ty);
    if (gap >= THIEF_ESCAPE_RANGE || e->flee_turns >= THIEF_ESCAPE_TURNS) {
        if (e->carried_gold) {
            log_set(world_line, enemy_name(e));
            log_cat(world_line, " IS GONE, AND SO IS ");
            log_num(world_line, e->carried_gold);
            log_cat(world_line, " GOLD");
        }
        e->hp = 0;
        e->carried_gold = 0;
    }
}

static uint8_t mend_a_neighbour(Enemy *e)
{
    Enemy *best = 0;
    uint16_t worst_num = 0, worst_den = 1;
    uint8_t i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *o = &enemies[i];
        uint8_t dx, dy;
        if (o == e || o->type == 0xFF || o->hp == 0 || o->hp >= o->hp_max)
            continue;
        dx = absi(o->x - e->x);
        dy = absi(o->y - e->y);
        if ((dx > dy ? dx : dy) > MEND_RANGE)
            continue;
        /* the most wounded, as a fraction: hp/max < worst */
        if (!best || (uint32_t)o->hp * worst_den < (uint32_t)worst_num * o->hp_max) {
            best = o;
            worst_num = o->hp;
            worst_den = o->hp_max;
        }
    }
    if (!best)
        return 0;
    best->hp += e->trait_value;
    if (best->hp > best->hp_max)
        best->hp = best->hp_max;
    log_set(world_line, enemy_name(e));
    log_cat(world_line, " MENDS ");
    log_cat(world_line, enemy_name(best));
    return 1;
}

static void detonate(Enemy *e)
{
    uint8_t i, caught = 0, hit_you = 0;
    int8_t dx, dy;
    if (e->trait != TR_BOMB)
        return;
    e->hp = 0;
    e->trait = TR_NONE;             /* spent: one burst, not a chain of its own */
    dx = player.x - e->x;
    dy = player.y - e->y;
    if (absi(dx) <= BLAST_RADIUS && absi(dy) <= BLAST_RADIUS)
        hit_you = player_take_damage(e->trait_value);
    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *o = &enemies[i];
        if (o == e || o->type == 0xFF || o->hp == 0)
            continue;
        if (absi(o->x - e->x) > BLAST_RADIUS || absi(o->y - e->y) > BLAST_RADIUS)
            continue;
        {
            uint8_t d = mitigate(e->trait_value, o->defense);
            if (d >= o->hp) {
                o->hp = 0;
                reward_kill(o);
            } else {
                o->hp -= d;
            }
        }
        caught++;
    }
    log_set(world_line, "IT BURSTS -- ");
    log_num(world_line, hit_you);
    log_cat(world_line, " TO YOU, ");
    log_num(world_line, caught);
    log_cat(world_line, " OTHERS");
    reward_kill(e);
}

static uint8_t enemy_act(Enemy *e, uint8_t ranged_used)
{
    int8_t tx = player.x - e->x, ty = player.y - e->y;
    uint8_t cheb = absi(tx) > absi(ty) ? absi(tx) : absi(ty);
    uint8_t manhattan = absi(tx) + absi(ty);

    if (e->fleeing) {
        enemy_flee(e, tx, ty);
        return ranged_used;
    }
    if (e->trait == TR_MENDER && mend_a_neighbour(e))
        return ranged_used;
    /* orthogonally adjacent, as in the original */
    if (manhattan == 1) {
        if (e->trait == TR_BOMB)
            detonate(e);
        else
            enemy_attack(e, 0);
        return ranged_used;
    }
    if (e->ranged) {
        if (player.ghost_turns || player.ghost_floors)
            return ranged_used;
        if (cheb <= e->view_range && ranged_used < MAX_RANGED_ATTACKERS_PER_TURN
            && los_clear(e->x, e->y, player.x, player.y)) {
            if (rnd_chance(ENEMY_RANGED_HIT_CHANCE))
                enemy_attack(e, 1);
            else
                log_set(world_line, "A SHOT MISSES YOU");
            return ranged_used + 1;
        }
        if (e->trait == TR_FAST && cheb <= 7 && los_clear(e->x, e->y, player.x, player.y))
            enemy_advance(e, tx, ty);
        return ranged_used;
    }
    if (cheb <= 7 && los_clear(e->x, e->y, player.x, player.y))
        enemy_advance(e, tx, ty);
    return ranged_used;
}

void enemy_turn(void)
{
    uint8_t i, ranged_used = 0;
    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (e->type == 0xFF || e->hp == 0)
            continue;
        if (e->frozen) {
            e->frozen--;
            continue;
        }
        if (e->trait == TR_ARMOURED) {
            e->slow_tick++;
            if ((e->slow_tick & 1) == 0)
                continue;
        }
        ranged_used = enemy_act(e, ranged_used);
        if (e->trait == TR_FAST && e->hp && player.hp)
            ranged_used = enemy_act(e, ranged_used);
        if (player.hp == 0)
            return;
    }
}
