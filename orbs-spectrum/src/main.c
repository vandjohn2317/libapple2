/* main.c -- the front door and the turn loop.
 *
 * One key, one turn: turning to face a way is free, a step or a swing costs
 * the turn, and then everything on the floor gets its move.
 */
#include <string.h>
#include "game.h"

uint8_t floor_no;
uint8_t floor_kills;
uint8_t theme_futuristic;
char log_line[33];
char world_line[33];

/* keys, as the game sees them */
#define IN_UP 1
#define IN_DOWN 2
#define IN_LEFT 4
#define IN_RIGHT 8
#define IN_FIRE 16
#define IN_SEARCH 32
#define IN_MAP 64
#define IN_ANY 128

/* THE KEYS.
 *
 * `kbd` is what went down since the game last asked and `kbd_down` is what is
 * down right now, both filled by the frame interrupt. A press is acted on
 * once, when it goes down -- so a tap that happened while the screen was
 * being drawn still counts, and a key held across a turn is not a second
 * press. Holding one walks: after REPEAT_DELAY frames of it being down the
 * turn loop takes it again, every REPEAT_RATE frames after that.
 */
#define REPEAT_DELAY 12
#define REPEAT_RATE 5

static uint8_t decode(const uint8_t *rows)
{
    uint8_t v = 0;
    if (key_in(rows, KB_W) || key_in(rows, KB_7) || key_in(rows, KB_JOY_UP))
        v |= IN_UP;
    if (key_in(rows, KB_S) || key_in(rows, KB_6) || key_in(rows, KB_JOY_DOWN))
        v |= IN_DOWN;
    if (key_in(rows, KB_A) || key_in(rows, KB_5) || key_in(rows, KB_JOY_LEFT))
        v |= IN_LEFT;
    if (key_in(rows, KB_D) || key_in(rows, KB_8) || key_in(rows, KB_JOY_RIGHT))
        v |= IN_RIGHT;
    if (key_in(rows, KB_SPACE) || key_in(rows, KB_ENTER) || key_in(rows, KB_F)
        || key_in(rows, KB_JOY_FIRE))
        v |= IN_FIRE;
    if (key_in(rows, KB_X))
        v |= IN_SEARCH;
    if (key_in(rows, KB_M))
        v |= IN_MAP;
    if (v || rows[0] || rows[1] || rows[2] || rows[3] || rows[4] || rows[5]
        || rows[6] || rows[7] || rows[8])
        v |= IN_ANY;
    return v;
}

static uint8_t held_frames;

uint8_t wait_key(void)
{
    for (;;) {
        uint8_t fresh, held;
        sys_halt();
        kbd_take();
        fresh = decode(kbd) & ~IN_ANY;
        held = decode(kbd_down) & ~IN_ANY;
        if (fresh) {
            held_frames = 0;
            return fresh;
        }
        if (held) {
            if (++held_frames >= REPEAT_DELAY) {
                held_frames = REPEAT_DELAY - REPEAT_RATE;
                return held;
            }
        } else {
            held_frames = 0;
        }
    }
}

/* Wait for a key that is pressed AFTER this is called, so a key still down
 * from the last screen does not answer this one. */
static void wait_any_key(void)
{
    kbd_take();
    for (;;) {
        sys_halt();
        kbd_take();
        if (decode(kbd) & IN_ANY)
            return;
    }
}

/* Building a floor takes a second or two of Z80: the walk, the search for
 * articulation points, and the spawns. Say so, or the machine looks hung. */
static void say_working(void)
{
    char buf[33];
    log_set(buf, "DESCENDING TO FLOOR ");
    log_num(buf, floor_no);
    log_cat(buf, " ...");
    print_at(0, 22, "                                ", WHITE);
    print_at(0, 23, "                                ", WHITE);
    print_at(0, 22, buf, YELLOW | BRIGHT);
    sys_border(BLUE);
}

void new_floor(void)
{
    uint8_t i;
    say_working();
    dungeon_generate();
    player.x = start_x;
    player.y = start_y;
    player.facing = F_SOUTH;
    player.defend_ac_bonus = 0;
    floor_kills = 0;
    spawn_enemies();
    spawn_floor_items();
    /* every door gates a region, and every region gets something worth the walk */
    {
        extern uint8_t door_reward_count, door_reward_x[], door_reward_y[];
        for (i = 0; i < door_reward_count; i++) {
            uint8_t x = door_reward_x[i], y = door_reward_y[i];
            if (!enemy_at(x, y) && !item_at(x, y))
                spawn_enemy_at(x, y);
        }
    }
    vision_update();
    sys_border(BLACK);
    log_set(log_line, "FLOOR ");
    log_num(log_line, floor_no);
    world_line[0] = 0;
}

static void begin_run(void)
{
    player_setup(JOB_WARRIOR);
    floor_no = 1;
    theme_futuristic = 0;
    new_floor();
    log_set(log_line, "A NEW LIFE BEGINS ON FLOOR 1");
}

static void descend(void)
{
    floor_no++;
    log_set(log_line, "");
    world_line[0] = 0;
    if (player.ghost_floors) player.ghost_floors--;
    if (player.rage_floors) player.rage_floors--;
    new_floor();
}

static void player_died(void)
{
    draw_all();
    attr_box(8, 9, 16, 3, ATTR(WHITE | BRIGHT, RED));
    print_at(8, 9, "                ", ATTR(WHITE | BRIGHT, RED));
    print_at(8, 10, "    YOU DIED    ", ATTR(WHITE | BRIGHT, RED));
    print_at(8, 11, "                ", ATTR(WHITE | BRIGHT, RED));
    wait_any_key();
    begin_run();
}

/* Everything that happens after the hero has spent the turn. */
static void after_player_action(void)
{
    player_tick_turn();
    if (player.venom_turns && player.venom_value) {
        if (player.venom_value >= player.hp)
            player.hp = 0;
        else
            player.hp -= player.venom_value;
        log_set(world_line, "VENOM BITES FOR ");
        log_num(world_line, player.venom_value);
        if (player.hp == 0) {
            player_died();
            return;
        }
    }
    vision_update();
    enemy_turn();
    if (player.hp == 0)
        player_died();
}

static void open_door(uint8_t x, uint8_t y)
{
    tidx i = IDX(x, y);
    grid[i] = TL_DOOR_OPEN | (grid[i] & 0xF0);
    log_set(log_line, "THE DOOR SWINGS OPEN");
}

/* ONE PRESS TURNS AND GOES.
 *
 * The Godot build spends a press on turning and the next one on walking,
 * because there the camera swings round with you and the turn is a thing you
 * do on purpose. Here it only ever read as a dropped key: press left, nothing
 * moves, press left again, now it does. So a direction sets the facing AND
 * takes the step in the same turn. Walking into rock still only turns you --
 * and that is what looking round a corner is now, and it still costs nothing.
 */
static void try_move(uint8_t face)
{
    uint8_t nx, ny, t;
    Enemy *e;
    Item *it;
    world_line[0] = 0;
    player.facing = face;
    nx = player.x + facing_dx[face];
    ny = player.y + facing_dy[face];
    t = tile_at(nx, ny);
    if (t == TL_DOOR) {
        open_door(nx, ny);
        after_player_action();
        return;
    }
    it = item_at(nx, ny);
    if (it && it->kind == IT_CHEST) {
        bump_chest(it);
        after_player_action();
        return;
    }
    e = enemy_at(nx, ny);
    if (e) {
        bump_attack(e);
        after_player_action();
        return;
    }
    if (!walkable_tile(t)) {
        vision_update();            /* you turned, so you can see that way now */
        log_set(log_line, "A WALL");
        return;
    }
    player.x = nx;
    player.y = ny;
    log_line[0] = 0;
    /* Mad Speed: a second free step in the same direction, if it is open */
    if (player.speed_turns) {
        uint8_t mx = nx + facing_dx[face], my = ny + facing_dy[face];
        if (walkable_tile(tile_at(mx, my)) && !enemy_at(mx, my) && !item_at(mx, my)) {
            player.x = mx;
            player.y = my;
        }
    }
    try_take_items();
    if (tile_at(player.x, player.y) == TL_EXIT) {
        descend();
        return;
    }
    after_player_action();
}

static void title_screen(void)
{
    sys_cls(ATTR(WHITE, BLACK));
    print_at(4, 6, "ORBS OF THE OVERLORD", YELLOW | BRIGHT);
    print_at(9, 8, "7 TOWERS SOFT", YELLOW);
    print_at(3, 12, "NINETY-NINE FLOORS, FIFTEEN", WHITE);
    print_at(3, 13, "BOSSES, AND A TEAR IN THE", WHITE);
    print_at(3, 14, "MIDDLE OF THEM.", WHITE);
    print_at(6, 18, "PRESS ANY KEY TO DESCEND", WHITE | BRIGHT);
    print_at(1, 21, "WASD OR 5678 TO TURN AND WALK", WHITE);
    print_at(1, 22, "WALK INTO THINGS TO USE THEM", WHITE);
    wait_any_key();
    /* the seed is the moment somebody pressed a key */
    rng_state ^= ((uint16_t)isr_frames << 8) | (isr_frames ^ 0x5A);
    if (rng_state == 0)
        rng_state = 1;
}

void main(void)
{
    sys_border(BLACK);
    title_screen();
    sys_cls(ATTR(WHITE, BLACK));
    begin_run();
    draw_all();
    for (;;) {
        uint8_t k = wait_key();
        if (k & IN_UP) try_move(F_NORTH);
        else if (k & IN_DOWN) try_move(F_SOUTH);
        else if (k & IN_LEFT) try_move(F_WEST);
        else if (k & IN_RIGHT) try_move(F_EAST);
        else continue;
        draw_all();
    }
}
