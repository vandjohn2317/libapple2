/* sys.h -- the machine layer (sys.s) as C sees it. */
#ifndef SYS_H
#define SYS_H
#include <stdint.h>

extern const uint8_t *blit_src;
extern uint8_t blit_attr;
extern uint8_t blit_w, blit_h;
extern const char *print_ptr;
extern uint8_t print_attr;
extern uint8_t kbd[8];
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
void kbd_scan(void);
uint8_t kempston(void);

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

/* keyboard half-rows (kbd[row] bit) */
#define K_SHIFT (kbd[0] & 1)
#define K_Z (kbd[0] & 2)
#define K_X (kbd[0] & 4)
#define K_C (kbd[0] & 8)
#define K_V (kbd[0] & 16)
#define K_A (kbd[1] & 1)
#define K_S (kbd[1] & 2)
#define K_D (kbd[1] & 4)
#define K_F (kbd[1] & 8)
#define K_G (kbd[1] & 16)
#define K_Q (kbd[2] & 1)
#define K_W (kbd[2] & 2)
#define K_E (kbd[2] & 4)
#define K_R (kbd[2] & 8)
#define K_T (kbd[2] & 16)
#define K_1 (kbd[3] & 1)
#define K_2 (kbd[3] & 2)
#define K_3 (kbd[3] & 4)
#define K_4 (kbd[3] & 8)
#define K_5 (kbd[3] & 16)
#define K_0 (kbd[4] & 1)
#define K_9 (kbd[4] & 2)
#define K_8 (kbd[4] & 4)
#define K_7 (kbd[4] & 8)
#define K_6 (kbd[4] & 16)
#define K_P (kbd[5] & 1)
#define K_O (kbd[5] & 2)
#define K_I (kbd[5] & 4)
#define K_U (kbd[5] & 8)
#define K_Y (kbd[5] & 16)
#define K_ENTER (kbd[6] & 1)
#define K_L (kbd[6] & 2)
#define K_K (kbd[6] & 4)
#define K_J (kbd[6] & 8)
#define K_H (kbd[6] & 16)
#define K_SPACE (kbd[7] & 1)
#define K_SYM (kbd[7] & 2)
#define K_M (kbd[7] & 4)
#define K_N (kbd[7] & 8)
#define K_B (kbd[7] & 16)

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
