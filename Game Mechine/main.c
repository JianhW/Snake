#include <stdint.h>
#include "bsp.h"
#include "gpio.h"

#define FB_BASE 0xF8100000
#define FB_WIDTH 64
#define FB_HEIGHT 64

#define CELL_SIZE 2
#define GRID_W (FB_WIDTH / CELL_SIZE)
#define GRID_H (FB_HEIGHT / CELL_SIZE)

#define GPIO_BASE SYSTEM_GPIO_0_IO_CTRL
#define SWITCH_REG_ADDR (FB_BASE + 8192u)

#define BUTTON_UP_MASK    (1u << 0)
#define BUTTON_DOWN_MASK  (1u << 1)
#define BUTTON_LEFT_MASK  (1u << 2)
#define BUTTON_RIGHT_MASK (1u << 3)
#define BUTTON_MASK_ALL   (BUTTON_UP_MASK | BUTTON_DOWN_MASK | BUTTON_LEFT_MASK | BUTTON_RIGHT_MASK)

#define SWITCH_SPEED0_MASK (1u << 0)
#define SWITCH_SPEED1_MASK (1u << 1)
#define SWITCH_WRAP_MASK   (1u << 2)
#define SWITCH_PAUSE_MASK  (1u << 3)
#define SWITCH_THEME_MASK  (1u << 4)

#define BUZZER_MASK (1u << 3)
#define INPUT_ACTIVE_HIGH 1

#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_ORANGE  0xFC60
#define COLOR_GOLD    0xFEA0
#define COLOR_LIME    0x5FE0
#define COLOR_LEAF    0x2E26
#define COLOR_MINT    0x6671
#define COLOR_CYAN    0x07FF
#define COLOR_BLUE    0x041F
#define COLOR_PINK    0xF81F
#define COLOR_SILVER  0xC638
#define COLOR_SHADOW  0x18C3
#define COLOR_SKY     0x869F

#define MAX_SNAKE_LENGTH 160

#define TETRIS_W 10
#define TETRIS_H 14
#define TETRIS_X0 2
#define TETRIS_Y0 8
#define TETRIS_CELL 3

typedef enum
{
    APP_STATE_MENU = 0,
    APP_STATE_SNAKE = 1,
    APP_STATE_TETRIS = 2
} app_state_t;

typedef enum
{
    SUBSTATE_READY = 0,
    SUBSTATE_PLAYING = 1,
    SUBSTATE_OVER = 2,
    SUBSTATE_PAUSED = 3
} substate_t;

typedef enum
{
    MENU_SNAKE = 0,
    MENU_TETRIS = 1
} menu_item_t;

typedef struct
{
    int x;
    int y;
} point_t;

typedef struct
{
    int dx;
    int dy;
} direction_t;

static volatile uint16_t *const fb = (volatile uint16_t *)FB_BASE;
static volatile uint32_t *const switch_reg = (volatile uint32_t *)SWITCH_REG_ADDR;

static app_state_t app_state;
static substate_t snake_state;
static substate_t tetris_state;
static menu_item_t menu_item;

static uint32_t rng_state = 0x13579BDFu;
static uint32_t prev_buttons;
static uint32_t switch_state;
static uint32_t prev_switch_state;
static uint32_t move_delay_loops;
static uint32_t tetris_delay_loops;
static uint32_t buzzer_ticks;

static point_t snake[MAX_SNAKE_LENGTH];
static int snake_length;
static direction_t snake_dir;
static direction_t snake_next_dir;
static point_t snake_food;
static uint32_t snake_score;
static uint32_t snake_best_score;

static uint8_t tetris_board[TETRIS_H][TETRIS_W];
static uint8_t tetris_prev_cells[TETRIS_H][TETRIS_W];
static int tetris_piece;
static int tetris_next_piece;
static int tetris_rot;
static int tetris_x;
static int tetris_y;
static uint32_t tetris_score;
static uint32_t tetris_best_score;
static uint32_t tetris_drop_counter;
static uint32_t tetris_prev_score;
static int tetris_prev_next_piece;
static int tetris_ui_dirty;

static uint16_t tetris_color(uint8_t value);
static int tetris_cell_filled(int piece, int rot, int py, int px);
static void tetris_draw_preview(void);

static const uint8_t glyph_space[7] = {0, 0, 0, 0, 0, 0, 0};
static const uint8_t glyph_a[7] = {14, 17, 17, 31, 17, 17, 17};
static const uint8_t glyph_b[7] = {30, 17, 17, 30, 17, 17, 30};
static const uint8_t glyph_c[7] = {14, 17, 16, 16, 16, 17, 14};
static const uint8_t glyph_e[7] = {31, 16, 16, 30, 16, 16, 31};
static const uint8_t glyph_g[7] = {14, 17, 16, 23, 17, 17, 15};
static const uint8_t glyph_i[7] = {14, 4, 4, 4, 4, 4, 14};
static const uint8_t glyph_k[7] = {17, 18, 20, 24, 20, 18, 17};
static const uint8_t glyph_m[7] = {17, 27, 21, 17, 17, 17, 17};
static const uint8_t glyph_n[7] = {17, 25, 21, 19, 17, 17, 17};
static const uint8_t glyph_o[7] = {14, 17, 17, 17, 17, 17, 14};
static const uint8_t glyph_p[7] = {30, 17, 17, 30, 16, 16, 16};
static const uint8_t glyph_r[7] = {30, 17, 17, 30, 20, 18, 17};
static const uint8_t glyph_s[7] = {15, 16, 16, 14, 1, 1, 30};
static const uint8_t glyph_t[7] = {31, 4, 4, 4, 4, 4, 4};
static const uint8_t glyph_u[7] = {17, 17, 17, 17, 17, 17, 14};
static const uint8_t glyph_v[7] = {17, 17, 17, 17, 17, 10, 4};
static const uint8_t glyph_y[7] = {17, 17, 10, 4, 4, 4, 4};

static const uint8_t digit_0[7] = {14, 17, 19, 21, 25, 17, 14};
static const uint8_t digit_1[7] = {4, 12, 4, 4, 4, 4, 14};
static const uint8_t digit_2[7] = {14, 17, 1, 2, 4, 8, 31};
static const uint8_t digit_3[7] = {30, 1, 1, 14, 1, 1, 30};
static const uint8_t digit_4[7] = {2, 6, 10, 18, 31, 2, 2};
static const uint8_t digit_5[7] = {31, 16, 16, 30, 1, 1, 30};
static const uint8_t digit_6[7] = {14, 16, 16, 30, 17, 17, 14};
static const uint8_t digit_7[7] = {31, 1, 2, 4, 8, 8, 8};
static const uint8_t digit_8[7] = {14, 17, 17, 14, 17, 17, 14};
static const uint8_t digit_9[7] = {14, 17, 17, 15, 1, 1, 14};

static const uint8_t tiny_space[5] = {0, 0, 0, 0, 0};
static const uint8_t tiny_a[5] = {2, 5, 7, 5, 5};
static const uint8_t tiny_c[5] = {3, 4, 4, 4, 3};
static const uint8_t tiny_e[5] = {7, 4, 6, 4, 7};
static const uint8_t tiny_g[5] = {3, 4, 5, 5, 3};
static const uint8_t tiny_l[5] = {4, 4, 4, 4, 7};
static const uint8_t tiny_m[5] = {5, 7, 7, 5, 5};
static const uint8_t tiny_n[5] = {5, 7, 7, 7, 5};
static const uint8_t tiny_o[5] = {2, 5, 5, 5, 2};
static const uint8_t tiny_r[5] = {6, 5, 6, 5, 5};
static const uint8_t tiny_s[5] = {3, 4, 2, 1, 6};
static const uint8_t tiny_t[5] = {7, 2, 2, 2, 2};
static const uint8_t tiny_u[5] = {5, 5, 5, 5, 7};
static const uint8_t tiny_x[5] = {5, 5, 2, 5, 5};
static const uint8_t tiny_y[5] = {5, 5, 2, 2, 2};

static const uint8_t tetromino[7][4][4][4] =
{
    {
        {{0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}},
        {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}},
        {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}}
    },
    {
        {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}
    },
    {
        {{0, 1, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 0, 0}, {1, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}}
    },
    {
        {{0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}}
    },
    {
        {{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 1, 0}, {0, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
        {{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 1, 0}, {0, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}}
    },
    {
        {{1, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 1, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
        {{0, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 0, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}}
    },
    {
        {{0, 0, 1, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
        {{0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
        {{0, 0, 0, 0}, {1, 1, 1, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}},
        {{1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}}
    }
};

static void draw_pixel(int x, int y, uint16_t color)
{
    if(x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT)
        return;

    fb[y * FB_WIDTH + x] = color;
}

static void fill_rect(int x0, int y0, int w, int h, uint16_t color)
{
    int x;
    int y;

    for(y = y0; y < y0 + h; y++)
    {
        for(x = x0; x < x0 + w; x++)
        {
            draw_pixel(x, y, color);
        }
    }
}

static void draw_rect_outline(int x0, int y0, int w, int h, uint16_t color)
{
    fill_rect(x0, y0, w, 1, color);
    fill_rect(x0, y0 + h - 1, w, 1, color);
    fill_rect(x0, y0, 1, h, color);
    fill_rect(x0 + w - 1, y0, 1, h, color);
}

static void clear_screen(uint16_t color)
{
    fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, color);
}

static uint16_t read_buttons_raw(void)
{
    uint32_t value = gpio_getInput(GPIO_BASE);

#if INPUT_ACTIVE_HIGH
    return (uint16_t)(value & BUTTON_MASK_ALL);
#else
    return (uint16_t)((~value) & BUTTON_MASK_ALL);
#endif
}

static uint16_t read_buttons_pressed(void)
{
    uint16_t current = read_buttons_raw();
    uint16_t edge = (uint16_t)(current & (uint16_t)(~prev_buttons));

    prev_buttons = current;
    return edge;
}

static uint32_t read_switches(void)
{
    return (*switch_reg) & 0x1Fu;
}

static void buzzer_on(void)
{
    gpio_setOutputEnable(GPIO_BASE, BUZZER_MASK);
    gpio_setOutput(GPIO_BASE, BUZZER_MASK);
}

static void buzzer_off(void)
{
    gpio_setOutputEnable(GPIO_BASE, BUZZER_MASK);
    gpio_setOutput(GPIO_BASE, 0u);
}

static void trigger_buzzer(uint32_t ticks)
{
    buzzer_ticks = ticks;
    buzzer_on();
}

static void service_buzzer(void)
{
    if(buzzer_ticks > 0u)
    {
        buzzer_ticks--;
        if(buzzer_ticks == 0u)
            buzzer_off();
    }
}

static uint32_t next_random(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static uint16_t border_primary_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_CYAN : COLOR_GOLD;
}

static uint16_t border_secondary_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_BLUE : COLOR_ORANGE;
}

static uint16_t snake_head_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_PINK : COLOR_LIME;
}

static uint16_t snake_body_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_CYAN : COLOR_LEAF;
}

static uint16_t snake_shine_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_SILVER : COLOR_WHITE;
}

static uint16_t food_main_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_BLUE : COLOR_RED;
}

static uint16_t food_highlight_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_CYAN : COLOR_WHITE;
}

static uint16_t score_accent_color(void)
{
    uint32_t speed_sel = switch_state & (SWITCH_SPEED0_MASK | SWITCH_SPEED1_MASK);

    if(speed_sel == SWITCH_SPEED0_MASK)
        return COLOR_MINT;
    if(speed_sel == SWITCH_SPEED1_MASK)
        return COLOR_SKY;
    if(speed_sel == (SWITCH_SPEED0_MASK | SWITCH_SPEED1_MASK))
        return COLOR_RED;

    return (switch_state & SWITCH_THEME_MASK) ? COLOR_CYAN : COLOR_GOLD;
}

static const uint8_t *get_glyph(char c)
{
    switch(c)
    {
        case 'A': return glyph_a;
        case 'B': return glyph_b;
        case 'C': return glyph_c;
        case 'E': return glyph_e;
        case 'G': return glyph_g;
        case 'I': return glyph_i;
        case 'K': return glyph_k;
        case 'M': return glyph_m;
        case 'N': return glyph_n;
        case 'O': return glyph_o;
        case 'P': return glyph_p;
        case 'R': return glyph_r;
        case 'S': return glyph_s;
        case 'T': return glyph_t;
        case 'U': return glyph_u;
        case 'V': return glyph_v;
        case 'Y': return glyph_y;
        case '0': return digit_0;
        case '1': return digit_1;
        case '2': return digit_2;
        case '3': return digit_3;
        case '4': return digit_4;
        case '5': return digit_5;
        case '6': return digit_6;
        case '7': return digit_7;
        case '8': return digit_8;
        case '9': return digit_9;
        case ' ': return glyph_space;
        default:  return glyph_space;
    }
}

static const uint8_t *get_tiny_glyph(char c)
{
    switch(c)
    {
        case 'A': return tiny_a;
        case 'C': return tiny_c;
        case 'E': return tiny_e;
        case 'G': return tiny_g;
        case 'L': return tiny_l;
        case 'M': return tiny_m;
        case 'N': return tiny_n;
        case 'O': return tiny_o;
        case 'R': return tiny_r;
        case 'S': return tiny_s;
        case 'T': return tiny_t;
        case 'U': return tiny_u;
        case 'X': return tiny_x;
        case 'Y': return tiny_y;
        case ' ': return tiny_space;
        default:  return tiny_space;
    }
}

static void draw_glyph(int x, int y, char c, uint16_t color, int scale)
{
    int row;
    int col;
    const uint8_t *glyph = get_glyph(c);

    for(row = 0; row < 7; row++)
    {
        for(col = 0; col < 5; col++)
        {
            if(glyph[row] & (1u << (4 - col)))
                fill_rect(x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

static void draw_text(int x, int y, const char *text, uint16_t color, int scale)
{
    int cursor = 0;

    while(text[cursor] != '\0')
    {
        draw_glyph(x + cursor * (6 * scale), y, text[cursor], color, scale);
        cursor++;
    }
}

static void draw_tiny_text(int x, int y, const char *text, uint16_t color)
{
    int cursor = 0;

    while(text[cursor] != '\0')
    {
        int row;
        int col;
        const uint8_t *glyph = get_tiny_glyph(text[cursor]);

        for(row = 0; row < 5; row++)
        {
            for(col = 0; col < 3; col++)
            {
                if(glyph[row] & (1u << (2 - col)))
                    draw_pixel(x + cursor * 4 + col, y + row, color);
            }
        }
        cursor++;
    }
}

static int text_width(const char *text, int scale)
{
    int len = 0;

    while(text[len] != '\0')
        len++;

    if(len == 0)
        return 0;

    return len * 6 * scale - scale;
}

static void draw_center_text(int y, const char *text, uint16_t color, int scale)
{
    int width = text_width(text, scale);
    int x = (FB_WIDTH - width) / 2;

    draw_text(x, y, text, color, scale);
}

static uint16_t mix565(uint16_t a, uint16_t b)
{
    uint16_t r = (uint16_t)((((a >> 11) & 0x1F) + ((b >> 11) & 0x1F)) >> 1);
    uint16_t g = (uint16_t)((((a >> 5) & 0x3F) + ((b >> 5) & 0x3F)) >> 1);
    uint16_t bl = (uint16_t)((((a >> 0) & 0x1F) + ((b >> 0) & 0x1F)) >> 1);

    return (uint16_t)((r << 11) | (g << 5) | bl);
}

static void format_u32(uint32_t value, char *out)
{
    char buffer[10];
    int count = 0;
    int i;

    do
    {
        buffer[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while(value != 0u);

    for(i = 0; i < count; i++)
        out[i] = buffer[count - 1 - i];

    out[count] = '\0';
}

static uint16_t board_cell_color(int grid_x, int grid_y)
{
    if(grid_x == 0 || grid_y == 0 || grid_x == GRID_W - 1 || grid_y == GRID_H - 1)
        return (((grid_x + grid_y) & 1) == 0) ? border_primary_color() : border_secondary_color();

    return COLOR_BLACK;
}

static void draw_field_background(void)
{
    int x;
    int y;

    fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BLACK);

    for(y = 0; y < GRID_H; y++)
    {
        for(x = 0; x < GRID_W; x++)
        {
            fill_rect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE, board_cell_color(x, y));
        }
    }

    draw_rect_outline(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_WHITE);
}

static void redraw_board_cell(int grid_x, int grid_y)
{
    fill_rect(grid_x * CELL_SIZE, grid_y * CELL_SIZE, CELL_SIZE, CELL_SIZE, board_cell_color(grid_x, grid_y));
}

static int decode_button_direction(uint16_t pressed, direction_t *direction)
{
    if(pressed & BUTTON_RIGHT_MASK)
    {
        direction->dx = 1;
        direction->dy = 0;
        return 1;
    }
    if(pressed & BUTTON_LEFT_MASK)
    {
        direction->dx = -1;
        direction->dy = 0;
        return 1;
    }
    if(pressed & BUTTON_UP_MASK)
    {
        direction->dx = 0;
        direction->dy = -1;
        return 1;
    }
    if(pressed & BUTTON_DOWN_MASK)
    {
        direction->dx = 0;
        direction->dy = 1;
        return 1;
    }

    return 0;
}

static void update_mode_from_switches(void)
{
    uint32_t speed_sel;

    switch_state = read_switches();
    speed_sel = switch_state & (SWITCH_SPEED0_MASK | SWITCH_SPEED1_MASK);

    switch(speed_sel)
    {
        case 0u:
            move_delay_loops = 240000u;
            tetris_delay_loops = 260000u;
            break;
        case SWITCH_SPEED0_MASK:
            move_delay_loops = 180000u;
            tetris_delay_loops = 210000u;
            break;
        case SWITCH_SPEED1_MASK:
            move_delay_loops = 130000u;
            tetris_delay_loops = 160000u;
            break;
        default:
            move_delay_loops = 90000u;
            tetris_delay_loops = 110000u;
            break;
    }
}

static void game_delay(uint32_t loops)
{
    volatile uint32_t i;

    for(i = 0; i < loops; i++)
    {
        if((i & 2047u) == 0u)
            service_buzzer();
    }
}

static void draw_score_box(uint32_t value, char label)
{
    char text[10];

    format_u32(value, text);
    fill_rect(44, 2, 18, 8, COLOR_SHADOW);
    draw_glyph(46, 3, label, COLOR_WHITE, 1);
    draw_text(52, 3, text, score_accent_color(), 1);
}

static void show_menu(void)
{
    clear_screen(COLOR_BLACK);
    draw_center_text(6, "GAME", border_primary_color(), 1);
    draw_center_text(16, "MENU", border_secondary_color(), 1);

    if(menu_item == MENU_SNAKE)
        fill_rect(10, 28, 3, 6, score_accent_color());
    else
        fill_rect(10, 38, 3, 6, score_accent_color());

    draw_text(18, 27, "SNAKE", snake_head_color(), 1);
    draw_text(18, 37, "TETRIS", food_main_color(), 1);
    draw_tiny_text(1, 56, "SELECT YOUR GAME", COLOR_WHITE);
}

static void snake_draw_food(void)
{
    int px = snake_food.x * CELL_SIZE;
    int py = snake_food.y * CELL_SIZE;

    fill_rect(px, py, CELL_SIZE, CELL_SIZE, food_main_color());
    fill_rect(px, py, 1, 1, food_highlight_color());
}

static void snake_draw_segment(point_t segment, int is_head)
{
    int px = segment.x * CELL_SIZE;
    int py = segment.y * CELL_SIZE;
    uint16_t color = is_head ? snake_head_color() : snake_body_color();

    fill_rect(px, py, CELL_SIZE, CELL_SIZE, color);
    draw_pixel(px, py, is_head ? snake_shine_color() : mix565(color, snake_shine_color()));
}

static void snake_render(void)
{
    int i;

    draw_field_background();
    snake_draw_food();

    for(i = snake_length - 1; i >= 0; i--)
        snake_draw_segment(snake[i], i == 0);

    draw_score_box(snake_score, 'S');
}

static int snake_on_body(int x, int y)
{
    int i;

    for(i = 0; i < snake_length; i++)
    {
        if(snake[i].x == x && snake[i].y == y)
            return 1;
    }

    return 0;
}

static void snake_spawn_food(void)
{
    int x;
    int y;

    do
    {
        x = 1 + (int)(next_random() % (uint32_t)(GRID_W - 2));
        y = 1 + (int)(next_random() % (uint32_t)(GRID_H - 2));
    }
    while(snake_on_body(x, y));

    snake_food.x = x;
    snake_food.y = y;
}

static void snake_reset(direction_t dir)
{
    int i;

    snake_length = 4;
    snake[0].x = GRID_W / 2;
    snake[0].y = GRID_H / 2;

    for(i = 1; i < snake_length; i++)
    {
        snake[i].x = snake[0].x - dir.dx * i;
        snake[i].y = snake[0].y - dir.dy * i;
    }

    snake_dir = dir;
    snake_next_dir = dir;
    snake_score = 0;
}

static void snake_start(direction_t dir)
{
    snake_reset(dir);
    snake_spawn_food();
    snake_state = SUBSTATE_PLAYING;
    buzzer_off();
    snake_render();
}

static int snake_is_opposite(direction_t a, direction_t b)
{
    return (a.dx == -b.dx) && (a.dy == -b.dy);
}

static void snake_update_direction(uint16_t pressed)
{
    direction_t requested;

    if(decode_button_direction(pressed, &requested) && !snake_is_opposite(requested, snake_dir))
        snake_next_dir = requested;
}

static int snake_hits_body(int x, int y, int ignore_tail)
{
    int i;
    int limit = snake_length - (ignore_tail ? 1 : 0);

    for(i = 0; i < limit; i++)
    {
        if(snake[i].x == x && snake[i].y == y)
            return 1;
    }

    return 0;
}

static void show_snake_ready(void)
{
    clear_screen(COLOR_BLACK);
    draw_center_text(14, "SNAKE", snake_head_color(), 2);
    draw_center_text(30, "PRESS", COLOR_WHITE, 1);
    draw_center_text(38, "ANY", score_accent_color(), 1);
    draw_center_text(46, "KEY", score_accent_color(), 1);
}

static void show_snake_over(void)
{
    char best_text[10];

    clear_screen(COLOR_BLACK);
    draw_center_text(16, "GAME", COLOR_RED, 2);
    draw_center_text(32, "OVER", COLOR_WHITE, 2);
    draw_center_text(48, "BEST", border_primary_color(), 1);
    format_u32(snake_best_score, best_text);
    draw_center_text(56, best_text, COLOR_SKY, 1);
}

static void show_snake_pause(void)
{
    clear_screen(COLOR_BLACK);
    draw_center_text(16, "PAUSE", border_primary_color(), 2);
    draw_center_text(40, "SW9", COLOR_WHITE, 1);
    draw_center_text(48, "RUN", score_accent_color(), 1);
}

static void snake_finish(void)
{
    if(snake_score > snake_best_score)
        snake_best_score = snake_score;

    snake_state = SUBSTATE_OVER;
    show_snake_over();
}

static void snake_step(void)
{
    point_t new_head;
    point_t old_head;
    point_t old_tail;
    int grow = 0;
    int i;

    snake_dir = snake_next_dir;
    old_head = snake[0];
    old_tail = snake[snake_length - 1];

    new_head.x = snake[0].x + snake_dir.dx;
    new_head.y = snake[0].y + snake_dir.dy;

    if((switch_state & SWITCH_WRAP_MASK) == 0u)
    {
        if(new_head.x <= 0 || new_head.x >= GRID_W - 1 || new_head.y <= 0 || new_head.y >= GRID_H - 1)
        {
            snake_finish();
            return;
        }
    }
    else
    {
        if(new_head.x <= 0) new_head.x = GRID_W - 2;
        else if(new_head.x >= GRID_W - 1) new_head.x = 1;
        if(new_head.y <= 0) new_head.y = GRID_H - 2;
        else if(new_head.y >= GRID_H - 1) new_head.y = 1;
    }

    if(new_head.x == snake_food.x && new_head.y == snake_food.y)
        grow = 1;

    if(snake_hits_body(new_head.x, new_head.y, !grow))
    {
        snake_finish();
        return;
    }

    if(grow && snake_length < MAX_SNAKE_LENGTH)
        snake_length++;

    for(i = snake_length - 1; i > 0; i--)
        snake[i] = snake[i - 1];

    snake[0] = new_head;

    if(grow)
    {
        snake_score++;
        snake_spawn_food();
        snake_draw_food();
        draw_score_box(snake_score, 'S');
        trigger_buzzer(200u);
    }

    if(!grow)
        redraw_board_cell(old_tail.x, old_tail.y);

    snake_draw_segment(old_head, 0);
    snake_draw_segment(new_head, 1);
}

static void draw_tetris_cell(int x, int y, uint16_t color)
{
    fill_rect(TETRIS_X0 + x * TETRIS_CELL, TETRIS_Y0 + y * TETRIS_CELL, TETRIS_CELL, TETRIS_CELL, color);
}

static void tetris_clear_dynamic_area(void)
{
    fill_rect(TETRIS_X0, TETRIS_Y0, TETRIS_W * TETRIS_CELL, TETRIS_H * TETRIS_CELL, COLOR_BLACK);
}

static void tetris_compose_cells(uint8_t out[TETRIS_H][TETRIS_W])
{
    int x;
    int y;
    int px;
    int py;

    for(y = 0; y < TETRIS_H; y++)
    {
        for(x = 0; x < TETRIS_W; x++)
            out[y][x] = tetris_board[y][x];
    }

    if(tetris_state != SUBSTATE_PLAYING)
        return;

    for(py = 0; py < 4; py++)
    {
        for(px = 0; px < 4; px++)
        {
            if(tetris_cell_filled(tetris_piece, tetris_rot, py, px))
            {
                int gx = tetris_x + px;
                int gy = tetris_y + py;

                if(gx >= 0 && gx < TETRIS_W && gy >= 0 && gy < TETRIS_H)
                    out[gy][gx] = (uint8_t)(tetris_piece + 1);
            }
        }
    }
}

static void tetris_draw_changed_cells(void)
{
    uint8_t current[TETRIS_H][TETRIS_W];
    int x;
    int y;

    tetris_compose_cells(current);

    for(y = 0; y < TETRIS_H; y++)
    {
        for(x = 0; x < TETRIS_W; x++)
        {
            if(tetris_ui_dirty || current[y][x] != tetris_prev_cells[y][x])
            {
                if(current[y][x] != 0)
                    draw_tetris_cell(x, y, tetris_color(current[y][x]));
                else
                    draw_tetris_cell(x, y, COLOR_BLACK);

                tetris_prev_cells[y][x] = current[y][x];
            }
        }
    }
}

static uint16_t tetris_color(uint8_t value)
{
    if(switch_state & SWITCH_THEME_MASK)
    {
        static const uint16_t theme_b[7] = {COLOR_CYAN, COLOR_BLUE, COLOR_PINK, COLOR_SILVER, COLOR_MINT, COLOR_SKY, COLOR_WHITE};
        return theme_b[(value - 1u) % 7u];
    }

    static const uint16_t theme_a[7] = {COLOR_RED, COLOR_GOLD, COLOR_LIME, COLOR_ORANGE, COLOR_SKY, COLOR_LEAF, COLOR_WHITE};
    return theme_a[(value - 1u) % 7u];
}

static void tetris_draw_board(void)
{
    fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BLACK);
    draw_rect_outline(TETRIS_X0 - 1, TETRIS_Y0 - 1, TETRIS_W * TETRIS_CELL + 2, TETRIS_H * TETRIS_CELL + 2, border_primary_color());
    tetris_clear_dynamic_area();
}

static int tetris_cell_filled(int piece, int rot, int py, int px)
{
    return tetromino[piece][rot & 3][py][px] != 0;
}

static void tetris_draw_preview(void)
{
    int px;
    int py;
    int preview_x0 = 48;
    int preview_y0 = 18;

    draw_rect_outline(preview_x0 - 2, preview_y0 - 2, 12, 12, border_secondary_color());
    draw_text(44, 8, "NEXT", score_accent_color(), 1);

    fill_rect(preview_x0, preview_y0, 8, 8, COLOR_BLACK);

    for(py = 0; py < 4; py++)
    {
        for(px = 0; px < 4; px++)
        {
            if(tetris_cell_filled(tetris_next_piece, 0, py, px))
            {
                fill_rect(preview_x0 + px * 2, preview_y0 + py * 2, 2, 2, tetris_color((uint8_t)(tetris_next_piece + 1)));
            }
        }
    }
}

static void tetris_draw_piece(int piece, int rot, int base_x, int base_y, uint16_t color)
{
    int px;
    int py;

    for(py = 0; py < 4; py++)
    {
        for(px = 0; px < 4; px++)
        {
            if(tetris_cell_filled(piece, rot, py, px))
            {
                int gx = base_x + px;
                int gy = base_y + py;

                if(gx >= 0 && gx < TETRIS_W && gy >= 0 && gy < TETRIS_H)
                    draw_tetris_cell(gx, gy, color);
            }
        }
    }
}

static int tetris_collides(int piece, int rot, int base_x, int base_y)
{
    int px;
    int py;

    for(py = 0; py < 4; py++)
    {
        for(px = 0; px < 4; px++)
        {
            if(tetris_cell_filled(piece, rot, py, px))
            {
                int gx = base_x + px;
                int gy = base_y + py;

                if(gx < 0 || gx >= TETRIS_W || gy >= TETRIS_H)
                    return 1;

                if(gy >= 0 && tetris_board[gy][gx] != 0)
                    return 1;
            }
        }
    }

    return 0;
}

static void tetris_spawn_piece(void)
{
    tetris_piece = tetris_next_piece;
    tetris_next_piece = (int)(next_random() % 7u);
    tetris_rot = 0;
    tetris_x = 3;
    tetris_y = 0;
    tetris_drop_counter = 0u;
    tetris_ui_dirty = 1u;

    if(tetris_collides(tetris_piece, tetris_rot, tetris_x, tetris_y))
    {
        if(tetris_score > tetris_best_score)
            tetris_best_score = tetris_score;

        tetris_state = SUBSTATE_OVER;
        clear_screen(COLOR_BLACK);
        draw_center_text(16, "GAME", COLOR_RED, 2);
        draw_center_text(32, "OVER", COLOR_WHITE, 2);
        draw_center_text(48, "BEST", border_primary_color(), 1);
        {
            char best_text[10];
            format_u32(tetris_best_score, best_text);
            draw_center_text(56, best_text, COLOR_SKY, 1);
        }
    }
}

static void tetris_lock_piece(void)
{
    int px;
    int py;
    int y;

    for(py = 0; py < 4; py++)
    {
        for(px = 0; px < 4; px++)
        {
            if(tetris_cell_filled(tetris_piece, tetris_rot, py, px))
            {
                int gx = tetris_x + px;
                int gy = tetris_y + py;

                if(gx >= 0 && gx < TETRIS_W && gy >= 0 && gy < TETRIS_H)
                    tetris_board[gy][gx] = (uint8_t)(tetris_piece + 1);
            }
        }
    }

    for(y = TETRIS_H - 1; y >= 0; y--)
    {
        int x;
        int full = 1;

        for(x = 0; x < TETRIS_W; x++)
        {
            if(tetris_board[y][x] == 0)
            {
                full = 0;
                break;
            }
        }

        if(full)
        {
            int yy;
            int xx;

            for(yy = y; yy > 0; yy--)
            {
                for(xx = 0; xx < TETRIS_W; xx++)
                    tetris_board[yy][xx] = tetris_board[yy - 1][xx];
            }

            for(xx = 0; xx < TETRIS_W; xx++)
                tetris_board[0][xx] = 0;

            tetris_score += 10u;
            trigger_buzzer(300u);
            tetris_ui_dirty = 1u;
            y++;
        }
    }

    tetris_spawn_piece();
}

static void tetris_render(void)
{
    if(tetris_ui_dirty || tetris_prev_score != tetris_score)
    {
        draw_score_box(tetris_score, 'T');
        tetris_prev_score = tetris_score;
    }

    if(tetris_ui_dirty || tetris_prev_next_piece != tetris_next_piece)
    {
        tetris_draw_preview();
        tetris_prev_next_piece = tetris_next_piece;
    }

    tetris_draw_changed_cells();
    tetris_ui_dirty = 0u;
}

static void tetris_reset(void)
{
    int x;
    int y;

    for(y = 0; y < TETRIS_H; y++)
    {
        for(x = 0; x < TETRIS_W; x++)
        {
            tetris_board[y][x] = 0;
            tetris_prev_cells[y][x] = 0xFFu;
        }
    }

    tetris_score = 0;
    tetris_prev_score = 0xFFFFFFFFu;
    tetris_prev_next_piece = -1;
    tetris_ui_dirty = 1u;
    tetris_state = SUBSTATE_PLAYING;
    tetris_next_piece = (int)(next_random() % 7u);
    tetris_spawn_piece();
    buzzer_off();
    tetris_draw_board();
    tetris_render();
}

static void show_tetris_ready(void)
{
    clear_screen(COLOR_BLACK);
    draw_center_text(10, "TETRIS", food_main_color(), 2);
    draw_center_text(30, "PRESS", COLOR_WHITE, 1);
    draw_center_text(38, "ANY", score_accent_color(), 1);
    draw_center_text(46, "KEY", score_accent_color(), 1);
}

static void show_tetris_pause(void)
{
    clear_screen(COLOR_BLACK);
    draw_center_text(16, "PAUSE", border_primary_color(), 2);
    draw_center_text(40, "SW9", COLOR_WHITE, 1);
    draw_center_text(48, "RUN", score_accent_color(), 1);
}

static void tetris_step(uint16_t pressed)
{
    int force_drop = 0;

    if(pressed & BUTTON_LEFT_MASK)
    {
        if(!tetris_collides(tetris_piece, tetris_rot, tetris_x - 1, tetris_y))
            tetris_x--;
    }
    if(pressed & BUTTON_RIGHT_MASK)
    {
        if(!tetris_collides(tetris_piece, tetris_rot, tetris_x + 1, tetris_y))
            tetris_x++;
    }
    if(pressed & BUTTON_UP_MASK)
    {
        if(!tetris_collides(tetris_piece, tetris_rot + 1, tetris_x, tetris_y))
            tetris_rot = (tetris_rot + 1) & 3;
    }
    if(pressed & BUTTON_DOWN_MASK)
    {
        if(!tetris_collides(tetris_piece, tetris_rot, tetris_x, tetris_y + 1))
            tetris_y++;
        else
            tetris_lock_piece();

        force_drop = 1;
    }

    if(tetris_state != SUBSTATE_PLAYING)
        return;

    tetris_drop_counter++;

    if(force_drop || tetris_drop_counter >= 3u)
    {
        tetris_drop_counter = 0u;

        if(!tetris_collides(tetris_piece, tetris_rot, tetris_x, tetris_y + 1))
            tetris_y++;
        else
            tetris_lock_piece();
    }

    if(tetris_state == SUBSTATE_PLAYING)
        tetris_render();
}

void trap(void)
{
    while(1)
    {
    }
}

void main(void)
{
    direction_t dir;

    bsp_init();
    gpio_setOutputEnable(GPIO_BASE, BUZZER_MASK);
    buzzer_off();
    update_mode_from_switches();
    prev_switch_state = switch_state;
    prev_buttons = read_buttons_raw();

    app_state = APP_STATE_MENU;
    menu_item = MENU_SNAKE;
    snake_state = SUBSTATE_READY;
    tetris_state = SUBSTATE_READY;
    show_menu();

    while(1)
    {
        uint16_t pressed = read_buttons_pressed();

        update_mode_from_switches();
        service_buzzer();

        if(app_state == APP_STATE_MENU)
        {
            if(pressed & BUTTON_UP_MASK)
            {
                menu_item = MENU_SNAKE;
                show_menu();
            }
            else if(pressed & BUTTON_DOWN_MASK)
            {
                menu_item = MENU_TETRIS;
                show_menu();
            }
            else if(pressed & BUTTON_RIGHT_MASK)
            {
                if(menu_item == MENU_SNAKE)
                {
                    app_state = APP_STATE_SNAKE;
                    snake_state = SUBSTATE_READY;
                    show_snake_ready();
                }
                else
                {
                    app_state = APP_STATE_TETRIS;
                    tetris_state = SUBSTATE_READY;
                    show_tetris_ready();
                }
            }

            game_delay(90000u);
            continue;
        }

        if(app_state == APP_STATE_SNAKE)
        {
            if(snake_state == SUBSTATE_PLAYING && switch_state != prev_switch_state)
            {
                if(switch_state & SWITCH_PAUSE_MASK)
                {
                    snake_state = SUBSTATE_PAUSED;
                    show_snake_pause();
                }
                else
                {
                    snake_render();
                }
            }
            prev_switch_state = switch_state;

            if(snake_state == SUBSTATE_READY)
            {
                if(decode_button_direction(pressed, &dir))
                    snake_start(dir);

                game_delay(90000u);
                continue;
            }

            if(snake_state == SUBSTATE_OVER)
            {
                if(pressed & BUTTON_LEFT_MASK)
                {
                    app_state = APP_STATE_MENU;
                    show_menu();
                    continue;
                }

                if(decode_button_direction(pressed, &dir))
                    snake_start(dir);

                game_delay(90000u);
                continue;
            }

            if(snake_state == SUBSTATE_PAUSED)
            {
                if((switch_state & SWITCH_PAUSE_MASK) == 0u)
                {
                    snake_state = SUBSTATE_PLAYING;
                    snake_render();
                }

                if(pressed & BUTTON_LEFT_MASK)
                {
                    app_state = APP_STATE_MENU;
                    show_menu();
                }

                game_delay(90000u);
                continue;
            }

            if(switch_state & SWITCH_PAUSE_MASK)
            {
                snake_state = SUBSTATE_PAUSED;
                show_snake_pause();
                game_delay(90000u);
                continue;
            }

            snake_update_direction(pressed);
            snake_step();
            game_delay(move_delay_loops);
            continue;
        }

        if(app_state == APP_STATE_TETRIS)
        {
            if(tetris_state == SUBSTATE_PLAYING && switch_state != prev_switch_state)
            {
                if(switch_state & SWITCH_PAUSE_MASK)
                {
                    tetris_state = SUBSTATE_PAUSED;
                    show_tetris_pause();
                }
                else
                {
                    tetris_ui_dirty = 1u;
                    tetris_render();
                }
            }
            prev_switch_state = switch_state;

            if(tetris_state == SUBSTATE_READY)
            {
                if(pressed != 0u)
                    tetris_reset();

                game_delay(90000u);
                continue;
            }

            if(tetris_state == SUBSTATE_OVER)
            {
                if(pressed & BUTTON_LEFT_MASK)
                {
                    app_state = APP_STATE_MENU;
                    show_menu();
                    continue;
                }

                if(pressed != 0u)
                    tetris_reset();

                game_delay(90000u);
                continue;
            }

            if(tetris_state == SUBSTATE_PAUSED)
            {
                if((switch_state & SWITCH_PAUSE_MASK) == 0u)
                {
                    tetris_state = SUBSTATE_PLAYING;
                    tetris_ui_dirty = 1u;
                    tetris_draw_board();
                    tetris_render();
                }

                if(pressed & BUTTON_LEFT_MASK)
                {
                    app_state = APP_STATE_MENU;
                    show_menu();
                }

                game_delay(90000u);
                continue;
            }

            if(switch_state & SWITCH_PAUSE_MASK)
            {
                tetris_state = SUBSTATE_PAUSED;
                show_tetris_pause();
                game_delay(90000u);
                continue;
            }

            tetris_step(pressed);
            game_delay(tetris_delay_loops);
        }
    }
}
