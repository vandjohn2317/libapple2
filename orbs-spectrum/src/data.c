/* data.c -- the balance tables, copied from Data.gd. Any change here is a
 * design decision, not a port bug. */
#include "game.h"

const int8_t facing_dx[4] = {0, 1, 0, -1};
const int8_t facing_dy[4] = {-1, 0, 1, 0};

/* (hp, hp_per_level, attr gain, base defence, thievery gain, vision) */
const ClassStats class_stats[3] = {
    {"WARRIOR", 85, 12, 3, 12, 2, 5},
    {"BATTLEMAGE", 60, 8, 4, 9, 3, 4},
    {"PRIEST", 45, 5, 5, 6, 4, 2},
};
const char *const class_resource[3] = {"RAGE", "MANA", "MANA"};

/* ORDER IS THE DIFFICULTY RAMP: a floor draws from the first 2 + floor rows.
 * name, hp, defence, damage, exp, ranged, view, ac, attack bonus, trait, value, sprite, ink */
const EnemyType enemy_types[ENEMY_TYPES] = {
    {"GOBLIN",          26,  3, 3, 18, 0, 6, 10, 2, TR_NONE,     0,  S_GOBLIN,   GREEN | BRIGHT},
    {"GIANT RAT",       16,  2, 2, 14, 0, 6, 11, 2, TR_FAST,     0,  S_RAT,      RED},
    {"SKELETON",        33,  8, 4, 24, 0, 6, 12, 3, TR_NONE,     0,  S_SKELETON, WHITE | BRIGHT},
    {"GIANT SPIDER",    30,  5, 3, 27, 0, 6, 12, 3, TR_POISON,   4,  S_SPIDER,   BLUE | BRIGHT},
    {"HUGE JELLY",      52,  5, 5, 28, 0, 6, 11, 3, TR_NONE,     0,  S_JELLY,    MAGENTA | BRIGHT},
    {"GREMLIN THIEF",   24,  4, 2, 22, 0, 6, 13, 3, TR_THIEF,    30, S_GOBLIN,   YELLOW},
    {"SINISTER WIZARD", 46, 12, 4, 35, 1, 4, 13, 3, TR_NONE,     0,  S_WIZARD,   CYAN | BRIGHT},
    {"ROT BLOAT",       44,  6, 4, 33, 0, 6,  9, 3, TR_BOMB,     16, S_JELLY,    GREEN},
    {"BEHOLDER",        59, 16, 5, 40, 1, 3, 14, 4, TR_NONE,     0,  S_BEHOLDER, RED | BRIGHT},
    {"CULT ACOLYTE",    40, 10, 3, 38, 0, 6, 12, 2, TR_MENDER,   9,  S_WIZARD,   MAGENTA},
    {"STONE GOLEM",     90, 30, 6, 48, 0, 6,  9, 4, TR_ARMOURED, 0,  S_JELLY,    WHITE},
    {"LICH KING",       78, 20, 7, 52, 0, 6, 15, 5, TR_NONE,     0,  S_WIZARD,   YELLOW | BRIGHT},
};
