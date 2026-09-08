/* sys.h -- the machine layer (sys.s) as C sees it. */
#ifndef SYS_H
#define SYS_H
#include <stdint.h>

extern const uint8_t *blit_src;
extern uint8_t blit_attr;
extern uint8_t blit_w, blit_h;
extern const char *print_ptr;
extern uint8_t print_attr;
/* Nine half-rows: the keyboard's eight, then a Kempston stick as the ninth.
 * kbd[] is what went DOWN since the last kbd_take(); kbd_down[] is what is
 * down right now. See the KB_* bits below. */
extern uint8_t kbd[9];
extern uint8_t kbd_down[9];
extern volatile uint8_t isr_frames;

void sys_cls(uint16_t attr) __z88dk_fastcall;
void sys_border(uint16_t colour) __z88dk_fastcall;
void sys_halt(void);
void blit_tile(uint16_t colrow) __z88dk_fastcall;
void blit_masked(uint16_t colrow) __z88dk_fastcall;
void blit_masked_noattr(uint16_t colrow) __z88dk_fastcall;
void clear_tile(uint16_t colrow) __z88dk_fastcall;
void set_attr4(uint16_t colrow) __z88dk_fastcall;
void print_str(uint16_t colrow) __z88dk_fastcall;
void fill_attr(uint16_t colrow) __z88dk_fastcall;
void bar(uint16_t colrow) __z88dk_fastcall;
void kbd_take(void);   /* the interrupt's edge latch, moved into kbd[] and emptied */

#define COLROW(c, r) ((uint16_t)(((uint16_t)(c) << 8) | (uint8_t)(r)))

/* attribute bytes */
#define BLACK 0
#define BLUE 1
#define RED 2
#define MAGENTA 3
#define GREEN 4
#define CYAN 5
#define YELLOW 6
#define WHITE 7
#define BRIGHT 0x40
#define FLASH 0x80
#define ATTR(ink, paper) ((uint8_t)((ink) | ((paper) << 3)))

/* Where each key sits in the matrix, as (row, bit). Bit 0 is the key nearest
 * the outside of the keyboard in each half-row. */
#define KB_ROW(r, b) ((uint8_t)(((r) << 4) | (b)))
#define KB_SHIFT KB_ROW(0, 0)
#define KB_Z KB_ROW(0, 1)
#define KB_X KB_ROW(0, 2)
#define KB_C KB_ROW(0, 3)
#define KB_V KB_ROW(0, 4)
#define KB_A KB_ROW(1, 0)
#define KB_S KB_ROW(1, 1)
#define KB_D KB_ROW(1, 2)
#define KB_F KB_ROW(1, 3)
#define KB_G KB_ROW(1, 4)
#define KB_Q KB_ROW(2, 0)
#define KB_W KB_ROW(2, 1)
#define KB_E KB_ROW(2, 2)
#define KB_R KB_ROW(2, 3)
#define KB_T KB_ROW(2, 4)
#define KB_1 KB_ROW(3, 0)
#define KB_5 KB_ROW(3, 4)
#define KB_0 KB_ROW(4, 0)
#define KB_8 KB_ROW(4, 2)
#define KB_7 KB_ROW(4, 3)
#define KB_6 KB_ROW(4, 4)
#define KB_P KB_ROW(5, 0)
#define KB_I KB_ROW(5, 2)
#define KB_ENTER KB_ROW(6, 0)
#define KB_H KB_ROW(6, 4)
#define KB_SPACE KB_ROW(7, 0)
#define KB_M KB_ROW(7, 2)
#define KB_N KB_ROW(7, 3)
#define KB_B KB_ROW(7, 4)
/* the stick, in the ninth row */
#define KB_JOY_RIGHT KB_ROW(8, 0)
#define KB_JOY_LEFT KB_ROW(8, 1)
#define KB_JOY_DOWN KB_ROW(8, 2)
#define KB_JOY_UP KB_ROW(8, 3)
#define KB_JOY_FIRE KB_ROW(8, 4)

static inline uint8_t key_in(const uint8_t *rows, uint8_t key)
{
    return rows[key >> 4] & (1 << (key & 15));
}

static inline void print_at(uint8_t col, uint8_t row, const char *s, uint8_t attr)
{
    print_ptr = s;
    print_attr = attr;
    print_str(COLROW(col, row));
}

static inline void attr_box(uint8_t col, uint8_t row, uint8_t w, uint8_t h, uint8_t attr)
{
    blit_attr = attr;
    blit_w = w;
    blit_h = h;
    fill_attr(COLROW(col, row));
}

#endif
